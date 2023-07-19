#include "xv_compiler.h"

#include "xv_meta.h"
#include "xv_lexical.h"
#include "../xlang/xl_code.h"
#include "../xexec/xx_ext.h"
#include "../xasm/xa_compiler.h"
#include "../ximg/xi_resources.h"

namespace Engine
{
	namespace XV
	{
		void SetStatusError(CompilerStatusDesc & status, CompilerStatus error)
		{
			status.status = error;
			status.error_line = L"";
			status.error_line_pos = status.error_line_no = status.error_line_len = -1;
		}
		void SetStatusError(CompilerStatusDesc & status, CompilerStatus error, const string & source)
		{
			status.status = error;
			status.error_line = source;
			status.error_line_pos = status.error_line_no = status.error_line_len = -1;
		}
		
		struct VObservationDesc
		{
			XL::LObject * current_namespace;
			Array<XL::LObject *> namespace_search_list = Array<XL::LObject *>(0x10);
		};
		struct VPostCompileDesc
		{
			VObservationDesc visibility;
			XL::LObject * function;
			SafePointer<XL::LObject> instance;
			SafePointer<XL::LObject> retval;
			ObjectArray<XL::LObject> input_types = ObjectArray<XL::LObject>(0x10);
			Array<string> input_names = Array<string>(0x10);
			uint flags;
			SafePointer<TokenStream> code_source;
			bool constructor, destructor;
		};
		struct VContext
		{
			bool module_is_library;
			XL::LContext & ctx;
			SafePointer<TokenStream> input;
			SafePointer<TokenStream> input_override;
			ICompilerCallback * callback;
			CompilerStatusDesc & status;
			Token current_token;
			Volumes::List<VPostCompileDesc> post_compile;
			Volumes::Dictionary<string, string> metadata;

			VContext(XL::LContext & _lal, ICompilerCallback * _callback, CompilerStatusDesc & _status, TokenStream * stream) :
				module_is_library(false), ctx(_lal), callback(_callback), status(_status) { input.SetRetain(stream); }
			void Abort(CompilerStatus error)
			{
				status.status = error;
				status.error_line = L"";
				status.error_line_no = status.error_line_pos = status.error_line_len = -1;
				throw Exception();
			}
			void Abort(CompilerStatus error, const string & source)
			{
				status.status = error;
				status.error_line = source;
				status.error_line_no = status.error_line_pos = status.error_line_len = -1;
				throw Exception();
			}
			void Abort(CompilerStatus error, int from, int length)
			{
				status.status = error;
				input->ExtractRange(from, length, status.error_line, status.error_line_no, status.error_line_pos, status.error_line_len);
				throw Exception();
			}
			void Abort(CompilerStatus error, const Token & where) { Abort(error, where.range_from, where.range_length); }
			void ReadNextToken(void)
			{
				if (input_override) {
					if (!input_override->ReadToken(current_token)) Abort(CompilerStatus::InvalidTokenInput, input_override->GetCurrentPosition(), 1);
				} else {
					if (!input->ReadToken(current_token)) Abort(CompilerStatus::InvalidTokenInput, input->GetCurrentPosition(), 1);
				}
			}
			void OverrideInput(TokenStream * new_input) { input_override.SetRetain(new_input); ReadNextToken(); }
			bool IsPunct(const widechar * seq) { return current_token.type == TokenType::Punctuation && current_token.contents == seq; }
			bool IsKeyword(const widechar * seq) { return current_token.type == TokenType::Keyword && current_token.contents == seq; }
			bool IsIdent(void) { return current_token.type == TokenType::Identifier; }
			bool IsLiteral(void) { return current_token.type == TokenType::Literal; }
			bool IsEOF(void) { return current_token.type == TokenType::EOF; }
			bool IsGenericIdent(void)
			{
				if (current_token.type == TokenType::Identifier) return true;
				if (current_token.type == TokenType::Keyword) return true;
				if (current_token.type == TokenType::Literal) {
					if (current_token.ex_data == TokenLiteralString) return true;
					if (current_token.ex_data == TokenLiteralLogicalY) return true;
					if (current_token.ex_data == TokenLiteralLogicalN) return true;
				}
				return false;
			}
			void AssertPunct(const widechar * seq) { if (!IsPunct(seq)) Abort(CompilerStatus::AnotherTokenExpected, current_token); }
			void AssertKeyword(const widechar * seq) { if (!IsKeyword(seq)) Abort(CompilerStatus::AnotherTokenExpected, current_token); }
			void AssertIdent(void) { if (!IsIdent()) Abort(CompilerStatus::AnotherTokenExpected, current_token); }
			void AssertGenericIdent(void) { if (!IsGenericIdent()) Abort(CompilerStatus::AnotherTokenExpected, current_token); }
			string ReadOperatorName(bool instance)
			{
				if (current_token.type != TokenType::Punctuation) Abort(CompilerStatus::AnotherTokenExpected, current_token);
				auto definition = current_token;
				auto name = current_token.contents;
				ReadNextToken();
				if (name == L"(") { AssertPunct(L")"); ReadNextToken(); name = L"()"; }
				else if (name == L"[") { AssertPunct(L"]"); ReadNextToken(); name = L"[]"; }
				if (instance) {
					if (name != XL::OperatorInvoke && name != XL::OperatorSubscript && name != XL::OperatorReferInvert &&
						name != XL::OperatorFollow && name != XL::OperatorNot && name != XL::OperatorNegative &&
						name != XL::OperatorAssign && name != XL::OperatorAOr && name != XL::OperatorAXor &&
						name != XL::OperatorAAnd && name != XL::OperatorAAdd && name != XL::OperatorASubtract &&
						name != XL::OperatorAMultiply && name != XL::OperatorADivide && name != XL::OperatorAResidual &&
						name != XL::OperatorAShiftLeft && name != XL::OperatorAShiftRight && name != XL::OperatorIncrement &&
						name != XL::OperatorDecrement) Abort(CompilerStatus::InvalidOperatorName, definition);
				} else {
					if (name != XL::OperatorOr && name != XL::OperatorXor && name != XL::OperatorAnd &&
						name != XL::OperatorAdd && name != XL::OperatorSubtract && name != XL::OperatorMultiply &&
						name != XL::OperatorDivide && name != XL::OperatorResidual && name != XL::OperatorLesser &&
						name != XL::OperatorGreater && name != XL::OperatorEqual && name != XL::OperatorNotEqual &&
						name != XL::OperatorLesserEqual && name != XL::OperatorGreaterEqual && name != XL::OperatorCompare &&
						name != XL::OperatorShiftLeft && name != XL::OperatorShiftRight) Abort(CompilerStatus::InvalidOperatorName, definition);
				}
				return name;
			}
			XL::LObject * ProcessExpressionPostfix(XL::LObject ** ssl, int ssc)
			{
				SafePointer<XL::LObject> object;
				if (IsIdent()) {
					for (int i = 0; i < ssc; i++) try {
						object = ssl[i]->GetMember(current_token.contents);
						break;
					} catch (...) {}
					if (!object) Abort(CompilerStatus::NoSuchSymbol, current_token);
					ReadNextToken();
				} else if (IsPunct(L".")) {
					object.SetRetain(ctx.GetRootNamespace());
				} else if (IsPunct(L"(")) {
					ReadNextToken();
					object = ProcessExpressionAssignation(ssl, ssc);
					AssertPunct(L")"); ReadNextToken();
				} else if (IsLiteral()) {
					if (current_token.ex_data == TokenLiteralLogicalY) {
						object = ctx.QueryLiteral(true);
					} else if (current_token.ex_data == TokenLiteralLogicalN) {
						object = ctx.QueryLiteral(false);
					} else if (current_token.ex_data == TokenLiteralInteger) {
						object = ctx.QueryLiteral(current_token.contents_i);
					} else if (current_token.ex_data == TokenLiteralFloat) {
						object = ctx.QueryLiteral(current_token.contents_f);
					} else if (current_token.ex_data == TokenLiteralString) {
						object = ctx.QueryLiteral(current_token.contents);
					} else Abort(CompilerStatus::InternalError, current_token);
					ReadNextToken();
				} else if (IsKeyword(Lexic::KeywordClass)) {
					ReadNextToken();
					object = ctx.QueryTypeOfOperator();
				} else if (IsKeyword(Lexic::KeywordSizeOf)) {
					ReadNextToken();
					object = ctx.QuerySizeOfOperator();
				} else if (IsKeyword(Lexic::KeywordModule)) {
					ReadNextToken(); AssertPunct(L"("); ReadNextToken();
					if (IsPunct(L")")) {
						ReadNextToken();
						object = ctx.QueryModuleOperator();
					} else {
						AssertGenericIdent();
						auto name = current_token.contents;
						ReadNextToken();
						while (IsPunct(L".")) {
							ReadNextToken(); AssertGenericIdent();
							name += L"." + current_token.contents;
							ReadNextToken();
						}
						AssertPunct(L")"); ReadNextToken();
						object = ctx.QueryModuleOperator(name);
					}
				} else if (IsKeyword(Lexic::KeywordInterface)) {
					ReadNextToken(); AssertPunct(L"("); ReadNextToken(); AssertGenericIdent();
					auto name = current_token.contents;
					ReadNextToken();
					while (IsPunct(L".")) {
						ReadNextToken(); AssertGenericIdent();
						name += L"." + current_token.contents;
						ReadNextToken();
					}
					AssertPunct(L")"); ReadNextToken();
					object = ctx.QueryInterfaceOperator(name);
				} else if (IsKeyword(Lexic::KeywordFunction)) {
					auto definition = current_token;
					ReadNextToken(); AssertPunct(L"("); ReadNextToken();
					SafePointer<XL::LObject> retval = ProcessTypeExpression(ssl, ssc);
					AssertPunct(L")"); ReadNextToken();
					ObjectArray<XL::LObject> args(0x10);
					AssertPunct(L"("); ReadNextToken();
					if (!IsPunct(L")")) {
						while (true) {
							SafePointer<XL::LObject> arg = ProcessTypeExpression(ssl, ssc);
							args.Append(arg);
							if (!IsPunct(L",")) break;
							ReadNextToken();
						}
						AssertPunct(L")"); ReadNextToken();
					} else { ReadNextToken(); }
					Array<XL::LObject *> input(0x20);
					for (auto & arg : args) input << &arg;
					try { return ctx.QueryFunctionPointer(retval, input.Length(), input.GetBuffer()); }
					catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, definition); }
				} else Abort(CompilerStatus::AnotherTokenExpected);
				while (IsPunct(L"(") || IsPunct(L".") || IsPunct(L"[") || IsPunct(XL::OperatorFollow) ||
					IsPunct(XL::OperatorIncrement) || IsPunct(XL::OperatorDecrement)) {
					auto op = current_token;
					ReadNextToken();
					if (op.contents == L"(") {
						ObjectArray<XL::LObject> args(0x20);
						if (!IsPunct(L")")) {
							while (true) {
								SafePointer<XL::LObject> arg = ProcessExpression(ssl, ssc);
								args.Append(arg);
								if (!IsPunct(L",")) break;
								ReadNextToken();
							}
							AssertPunct(L")"); ReadNextToken();
						} else { ReadNextToken(); }
						Array<XL::LObject *> input(0x20);
						for (auto & arg : args) input << &arg;
						try { object = object->Invoke(input.Length(), input.GetBuffer()); }
						catch (...) { Abort(CompilerStatus::NoSuchOverload, op); }
					} else if (op.contents == L".") {
						AssertIdent();
						try { object = object->GetMember(current_token.contents); }
						catch (...) { Abort(CompilerStatus::NoSuchSymbol, current_token); }
						ReadNextToken();
					} else if (op.contents == L"[") {
						SafePointer<XL::LObject> subscript;
						try { subscript = object->GetMember(XL::OperatorSubscript); }
						catch (...) { Abort(CompilerStatus::NoSuchSymbol, op); }
						ObjectArray<XL::LObject> args(0x20);
						if (!IsPunct(L"]")) {
							while (true) {
								SafePointer<XL::LObject> arg = ProcessExpression(ssl, ssc);
								args.Append(arg);
								if (!IsPunct(L",")) break;
								ReadNextToken();
							}
							AssertPunct(L"]"); ReadNextToken();
						} else { ReadNextToken(); }
						Array<XL::LObject *> input(0x20);
						for (auto & arg : args) input << &arg;
						try { object = subscript->Invoke(input.Length(), input.GetBuffer()); }
						catch (...) { Abort(CompilerStatus::NoSuchOverload, op); }
					} else {
						SafePointer<XL::LObject> method;
						try { method = object->GetMember(op.contents); }
						catch (...) { Abort(CompilerStatus::NoSuchSymbol, op); }
						try { object = method->Invoke(0, 0); }
						catch (...) { Abort(CompilerStatus::NoSuchOverload, op); }
					}
				}
				object->Retain();
				return object;
			}
			XL::LObject * ProcessExpressionUnary(XL::LObject ** ssl, int ssc)
			{
				if (IsPunct(XL::OperatorTakeAddress)) {
					auto op = current_token;
					ReadNextToken();
					auto definition = current_token;
					SafePointer<XL::LObject> object = ProcessExpressionUnary(ssl, ssc);
					if (object->GetClass() == XL::Class::Type) {
						try { return ctx.QueryTypePointer(object); }
						catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, definition); }
					} else {
						// TODO: IMPLEMENT TAKE ADDRESS '@'
						// SafePointer<XL::LObject> method;
						// try { method = object->GetMember(op.contents); }
						// catch (...) { Abort(CompilerStatus::NoSuchSymbol, op); }
						// try { return method->Invoke(0, 0); }
						// catch (...) { Abort(CompilerStatus::NoSuchOverload, op); }
					}
				} else if (IsPunct(XL::OperatorReferInvert)) {
					auto op = current_token;
					ReadNextToken();
					auto definition = current_token;
					SafePointer<XL::LObject> object = ProcessExpressionUnary(ssl, ssc);
					if (object->GetClass() == XL::Class::Type) {
						try { return ctx.QueryTypeReference(object); }
						catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, definition); }
					} else {
						SafePointer<XL::LObject> method;
						try { method = object->GetMember(op.contents); }
						catch (...) { Abort(CompilerStatus::NoSuchSymbol, op); }
						try { return method->Invoke(0, 0); }
						catch (...) { Abort(CompilerStatus::NoSuchOverload, op); }
					}
				} else if (IsPunct(XL::OperatorNot)) {
					auto op = current_token;
					ReadNextToken();
					auto definition = current_token;
					SafePointer<XL::LObject> object = ProcessExpressionUnary(ssl, ssc), method;
					try { method = object->GetMember(op.contents); }
					catch (...) { Abort(CompilerStatus::NoSuchSymbol, op); }
					try { return method->Invoke(0, 0); }
					catch (...) { Abort(CompilerStatus::NoSuchOverload, op); }
				} else if (IsPunct(XL::OperatorNegative)) {
					auto op = current_token;
					ReadNextToken();
					auto definition = current_token;
					SafePointer<XL::LObject> object = ProcessExpressionUnary(ssl, ssc), method;
					try { method = object->GetMember(op.contents); }
					catch (...) { Abort(CompilerStatus::NoSuchSymbol, op); }
					try { return method->Invoke(0, 0); }
					catch (...) { Abort(CompilerStatus::NoSuchOverload, op); }
				} else if (IsPunct(XL::OperatorAdd)) {
					ReadNextToken();
					return ProcessExpressionUnary(ssl, ssc);
				} else if (IsKeyword(Lexic::KeywordArray)) {
					Array<int> dim_list(0x10);
					ReadNextToken(); AssertPunct(L"["); ReadNextToken();
					auto d1e = current_token;
					SafePointer<XL::LObject> d1 = ProcessExpression(ssl, ssc);
					if (d1->GetClass() != XL::Class::Literal) Abort(CompilerStatus::ExpressionMustBeConst, d1e);
					try { dim_list << ctx.QueryLiteralValue(d1); }
					catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, d1e); }
					while (IsPunct(L",")) {
						ReadNextToken();
						auto dne = current_token;
						SafePointer<XL::LObject> dn = ProcessExpression(ssl, ssc);
						if (dn->GetClass() != XL::Class::Literal) Abort(CompilerStatus::ExpressionMustBeConst, dne);
						try { dim_list << ctx.QueryLiteralValue(dn); }
						catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, dne); }
					}
					AssertPunct(L"]"); ReadNextToken();
					auto definition = current_token;
					SafePointer<XL::LObject> type = ProcessExpressionUnary(ssl, ssc);
					try { for (auto & d : dim_list.InversedElements()) { type = ctx.QueryStaticArray(type, d); } }
					catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, definition); }
					type->Retain();
					return type;
				} else return ProcessExpressionPostfix(ssl, ssc);
			}
			XL::LObject * ProcessExpressionMultiplicative(XL::LObject ** ssl, int ssc)
			{
				SafePointer<XL::LObject> object = ProcessExpressionUnary(ssl, ssc);
				while (IsPunct(XL::OperatorAnd) || IsPunct(XL::OperatorMultiply) || IsPunct(XL::OperatorDivide) ||
					IsPunct(XL::OperatorResidual)) {
					auto op = current_token;
					ReadNextToken();
					SafePointer<XL::LObject> rs = ProcessExpressionUnary(ssl, ssc), op_obj;
					try { op_obj = ctx.QueryObject(op.contents); } catch (...) { Abort(CompilerStatus::NoSuchSymbol, op); }
					try {
						XL::LObject * opv[2] = { object.Inner(), rs.Inner() };
						object = op_obj->Invoke(2, opv);
					} catch (...) { Abort(CompilerStatus::NoSuchOverload, op); }
				}
				object->Retain();
				return object;
			}
			XL::LObject * ProcessExpressionAdditive(XL::LObject ** ssl, int ssc)
			{
				SafePointer<XL::LObject> object = ProcessExpressionMultiplicative(ssl, ssc);
				while (IsPunct(XL::OperatorOr) || IsPunct(XL::OperatorXor) || IsPunct(XL::OperatorAdd) ||
					IsPunct(XL::OperatorSubtract) || IsPunct(XL::OperatorShiftLeft) || IsPunct(XL::OperatorShiftRight) ||
					IsPunct(XL::OperatorCompare)) {
					auto op = current_token;
					ReadNextToken();
					SafePointer<XL::LObject> rs = ProcessExpressionMultiplicative(ssl, ssc), op_obj;
					try { op_obj = ctx.QueryObject(op.contents); } catch (...) { Abort(CompilerStatus::NoSuchSymbol, op); }
					try {
						XL::LObject * opv[2] = { object.Inner(), rs.Inner() };
						object = op_obj->Invoke(2, opv);
					} catch (...) { Abort(CompilerStatus::NoSuchOverload, op); }
				}
				object->Retain();
				return object;
			}
			XL::LObject * ProcessExpressionComparative(XL::LObject ** ssl, int ssc)
			{
				SafePointer<XL::LObject> object = ProcessExpressionAdditive(ssl, ssc);
				while (IsPunct(XL::OperatorEqual) || IsPunct(XL::OperatorNotEqual) || IsPunct(XL::OperatorLesser) ||
					IsPunct(XL::OperatorLesserEqual) || IsPunct(XL::OperatorGreater) || IsPunct(XL::OperatorGreaterEqual)) {
					auto op = current_token;
					ReadNextToken();
					SafePointer<XL::LObject> rs = ProcessExpressionAdditive(ssl, ssc), op_obj;
					try { op_obj = ctx.QueryObject(op.contents); } catch (...) { Abort(CompilerStatus::NoSuchSymbol, op); }
					try {
						XL::LObject * opv[2] = { object.Inner(), rs.Inner() };
						object = op_obj->Invoke(2, opv);
					} catch (...) { Abort(CompilerStatus::NoSuchOverload, op); }
				}
				object->Retain();
				return object;
			}
			XL::LObject * ProcessExpressionLogicalOr(XL::LObject ** ssl, int ssc)
			{
				// TODO: IMPLEMENT
				// ||
				return ProcessExpressionComparative(ssl, ssc);
			}
			XL::LObject * ProcessExpressionLogicalAnd(XL::LObject ** ssl, int ssc)
			{
				// TODO: IMPLEMENT
				// &&
				return ProcessExpressionLogicalOr(ssl, ssc);
			}
			XL::LObject * ProcessExpressionTernary(XL::LObject ** ssl, int ssc)
			{
				// TODO: IMPLEMENT
				// ?:
				return ProcessExpressionLogicalAnd(ssl, ssc);
			}
			XL::LObject * ProcessExpressionAssignation(XL::LObject ** ssl, int ssc)
			{
				SafePointer<XL::LObject> object = ProcessExpressionTernary(ssl, ssc);

				// TODO: IMPLEMENT
				// if (IsPunct(XL::OperatorAssign) || IsPunct(XL::OperatorAOr) || IsPunct(XL::OperatorAXor) ||
				// 	IsPunct(XL::OperatorAAnd) || IsPunct(XL::OperatorAAdd) || IsPunct(XL::OperatorASubtract) ||
				// 	IsPunct(XL::OperatorAMultiply) || IsPunct(XL::OperatorADivide) || IsPunct(XL::OperatorAResidual) ||
				// 	IsPunct(XL::OperatorAShiftLeft) || IsPunct(XL::OperatorAShiftRight)) {
				// 	auto op = current_token;
				// 	ReadNextToken();
				// 	SafePointer<XL::LObject> rs = ProcessExpressionAssignation(ssl, ssc), op_obj;
				// 	try { op_obj = ctx.QueryObject(op.contents); } catch (...) { Abort(CompilerStatus::NoSuchSymbol, op); }
				// 	try {
				// 		XL::LObject * opv[2] = { object.Inner(), rs.Inner() };
				// 		object = op_obj->Invoke(2, opv);
				// 	} catch (...) { Abort(CompilerStatus::NoSuchOverload, op); }
				// }

				object->Retain();
				return object;
			}
			XL::LObject * ProcessExpression(XL::LObject ** ssl, int ssc) { return ProcessExpressionAssignation(ssl, ssc); }
			XL::LObject * ProcessExpression(VObservationDesc & desc) { return ProcessExpression(desc.namespace_search_list.GetBuffer(), desc.namespace_search_list.Length()); }
			XL::LObject * ProcessTypeExpression(XL::LObject ** ssl, int ssc)
			{
				auto expr = current_token;
				auto result = ProcessExpression(ssl, ssc);
				if (result->GetClass() != XL::Class::Type) { result->Release(); Abort(CompilerStatus::ObjectTypeMismatch, expr); }
				return result;
			}
			XL::LObject * ProcessTypeExpression(VObservationDesc & desc)
			{
				auto expr = current_token;
				auto result = ProcessExpression(desc);
				if (result->GetClass() != XL::Class::Type) { result->Release(); Abort(CompilerStatus::ObjectTypeMismatch, expr); }
				return result;
			}
			void ProcessVariableDefinition(Volumes::Dictionary<string, string> & attributes, VObservationDesc & desc)
			{
				// TODO: IMPLEMENT VARIABLE
			}
			void ProcessFunctionDefinition(Volumes::Dictionary<string, string> & attributes, VObservationDesc & desc)
			{
				bool is_class_function = desc.current_namespace->GetClass() == XL::Class::Type;
				bool is_operator, is_static, is_ctor, is_dtor, is_conv;
				if (is_class_function) {
					if (IsKeyword(Lexic::KeywordClassFunc)) {
						ReadNextToken(); is_static = true;
					} else is_static = false;
					is_operator = false;
					is_ctor = IsKeyword(Lexic::KeywordCtor);
					is_dtor = IsKeyword(Lexic::KeywordDtor);
					is_conv = IsKeyword(Lexic::KeywordConvertor);
				} else {
					is_static = true;
					is_operator = is_ctor = is_dtor = is_conv = false;
				}
				if (is_static && (is_ctor || is_dtor || is_conv)) Abort(CompilerStatus::FunctionMustBeInstance, current_token);
				auto definition = current_token;
				if (!is_ctor && !is_dtor && !is_conv) AssertKeyword(Lexic::KeywordFunction);
				ReadNextToken();
				SafePointer<XL::LObject> retval;
				ObjectArray<XL::LObject> argv_object(0x10);
				Array<XL::LObject *> argv(0x10);
				Array<string> argv_names(0x10);
				string name, import_name, import_lib;
				uint flags = 0;
				uint org = 0; // 0 - V, 1 - A, 2 - import, 3 - import from library, -1 - pure
				if (is_ctor || is_dtor) retval = ctx.QueryObject(XL::NameVoid);
				else retval = ProcessTypeExpression(desc);
				if (is_ctor) {
					if (IsIdent() && current_token.contents == Lexic::ConstructorZero) {
						ReadNextToken(); name = XL::NameConstructorZero;
					} else if (IsIdent() && current_token.contents == Lexic::ConstructorMove) {
						ReadNextToken(); name = XL::NameConstructorMove;
					} else {
						name = XL::NameConstructor;
					}
				} else if (is_dtor) {
					name = XL::NameDestructor;
				} else if (is_conv) {
					name = XL::NameConverter;
				} else {
					if (IsKeyword(Lexic::KeywordOperator)) {
						ReadNextToken(); definition = current_token;
						name = ReadOperatorName(!is_static);
						is_operator = true;
					} else {
						definition = current_token;
						AssertIdent(); name = current_token.contents; ReadNextToken();
					}
				}
				AssertPunct(L"("); ReadNextToken();
				while (true) {
					if (IsPunct(L")") && !argv.Length()) {
						break;
					} else {
						SafePointer<XL::LObject> type = ProcessTypeExpression(desc);
						string name;
						if (IsIdent()) { name = current_token.contents; ReadNextToken(); }
						argv.Append(type);
						argv_names.Append(name);
						argv_object.Append(type);
						if (IsPunct(L",")) ReadNextToken(); else break;
					}
				}
				AssertPunct(L")"); ReadNextToken();
				if (!is_static) flags |= XL::FunctionMethod | XL::FunctionThisCall;
				while (IsKeyword(Lexic::KeywordEntry) || IsKeyword(Lexic::KeywordThrows) || IsKeyword(Lexic::KeywordVirtual) || IsKeyword(Lexic::KeywordPure)) {
					if (IsKeyword(Lexic::KeywordEntry)) {
						if (!is_static) Abort(CompilerStatus::InvalidFunctionTrats, current_token);
						flags |= XL::FunctionMain;
					} else if (IsKeyword(Lexic::KeywordThrows)) flags |= XL::FunctionThrows;
					else if (IsKeyword(Lexic::KeywordVirtual)) {
						if (is_static) Abort(CompilerStatus::InvalidFunctionTrats, current_token);
						flags |= XL::FunctionVirtual;
					} else if (IsKeyword(Lexic::KeywordPure)) {
						if (!(flags & XL::FunctionVirtual)) Abort(CompilerStatus::InvalidFunctionTrats, current_token);
						org = -1; flags |= XL::FunctionPureCall;
					}
					ReadNextToken();
				}
				for (auto & attr : attributes) {
					if (attr.key == Lexic::AttributeInit) flags |= XL::FunctionInitializer;
					else if (attr.key == Lexic::AttributeFinal) flags |= XL::FunctionFinalizer;
					else if (attr.key == Lexic::AttributeNoTC) flags &= ~XL::FunctionThisCall;
				}
				XL::LObject * func;
				try {
					auto dir = ctx.CreateFunction((is_operator && is_static) ? ctx.GetRootNamespace() : desc.current_namespace, name);
					func = ctx.CreateFunctionOverload(dir, retval, argv.Length(), argv.GetBuffer(), flags);
				} catch (...) { Abort(CompilerStatus::SymbolRedefinition, definition); }
				for (auto & attr : attributes) {
					if (attr.key[0] == L'[') {
						if (attr.key == Lexic::AttributeInit) {
							if (!is_static || attr.value.Length()) Abort(CompilerStatus::InapproptiateAttribute, definition);
						} else if (attr.key == Lexic::AttributeFinal) {
							if (!is_static || attr.value.Length()) Abort(CompilerStatus::InapproptiateAttribute, definition);
						} else if (attr.key == Lexic::AttributeAsm) {
							if (org || attr.value.Length()) Abort(CompilerStatus::InapproptiateAttribute, definition);
							org = 1;
						} else if (attr.key == Lexic::AttributeImport) {
							if (!attr.value.Length()) Abort(CompilerStatus::InapproptiateAttribute, definition);
							if (org == -1 || org == 1 || org == 2) Abort(CompilerStatus::InapproptiateAttribute, definition);
							if (org == 0) org = 2;
							import_name = attr.value;
						} else if (attr.key == Lexic::AttributeImpLib) {
							if (!attr.value.Length()) Abort(CompilerStatus::InapproptiateAttribute, definition);
							if (org == -1 || org == 1 || org == 3) Abort(CompilerStatus::InapproptiateAttribute, definition);
							org = 3;
							import_lib = attr.value;
						} else if (attr.key == Lexic::AttributeNoTC) {
							if (attr.value.Length()) Abort(CompilerStatus::InapproptiateAttribute, definition);
						} else Abort(CompilerStatus::InapproptiateAttribute, definition);
					} else func->AddAttribute(attr.key, attr.value);
				}
				if (org == 3 && !import_name.Length()) Abort(CompilerStatus::InapproptiateAttribute, definition);
				attributes.Clear();
				if (org == -1) {
					AssertPunct(L";"); ReadNextToken();
				} else if (org == 0) {
					AssertPunct(L"{");
					SafePointer<TokenStream> stream;
					if (!input->ReadBlock(stream.InnerRef())) Abort(CompilerStatus::InvalidTokenInput, input->GetCurrentPosition());
					ReadNextToken();
					VPostCompileDesc pc;
					pc.visibility = desc;
					pc.function = func;
					pc.instance.SetRetain(is_static ? 0 : desc.current_namespace);
					pc.retval = retval;
					pc.input_types = argv_object;
					pc.input_names = argv_names;
					pc.flags = flags;
					pc.code_source = stream;
					pc.constructor = is_ctor;
					pc.destructor = is_dtor;
					post_compile.InsertLast(pc);
				} else if (org == 1) {
					auto start = current_token;
					AssertPunct(L"{");
					SafePointer<TokenStream> asm_stream;
					if (!input->ReadBlock(asm_stream.InnerRef())) Abort(CompilerStatus::InvalidTokenInput, input->GetCurrentPosition());
					ReadNextToken();
					auto asm_code = asm_stream->ExtractContents();
					XA::CompilerStatusDesc asm_desc;
					XA::Function asm_func;
					XA::CompileFunction(asm_code, asm_func, asm_desc);
					if (asm_desc.status != XA::CompilerStatus::Success) Abort(CompilerStatus::AssemblyError, start);
					ctx.SupplyFunctionImplementation(func, asm_func);
				} else ctx.SupplyFunctionImplementation(func, import_name, import_lib);
			}
			void ProcessClassDefinition(Volumes::Dictionary<string, string> & attributes, VObservationDesc & desc)
			{
				bool is_interface = IsKeyword(Lexic::KeywordInterface);
				ReadNextToken(); AssertIdent();
				auto definition = current_token;
				XL::LObject * type;
				try { type = ctx.CreateClass(desc.current_namespace, current_token.contents); }
				catch (...) { Abort(CompilerStatus::SymbolRedefinition, current_token); }
				if (is_interface) ctx.MarkClassAsInterface(type);
				ReadNextToken();
				for (auto & attr : attributes) {
					if (attr.key[0] == L'[') {
						if (attr.key == Lexic::AttributeSize) {
							auto sep = attr.value.Split(L':');
							if (sep.Length() > 2) Abort(CompilerStatus::InapproptiateAttribute, definition);
							XA::ObjectSize size;
							try {
								size.num_bytes = sep[0].ToUInt32();
								if (sep.Length() == 2) size.num_words = sep[1].ToUInt32(); else size.num_words = 0;
							} catch (...) { Abort(CompilerStatus::InapproptiateAttribute, definition); }
							ctx.SetClassInstanceSize(type, size);
						} else if (attr.key == Lexic::AttributeCore) {
							if (attr.value.Length() || is_interface) Abort(CompilerStatus::InapproptiateAttribute, definition);
							ctx.MarkClassAsCore(type);
						} else if (attr.key == Lexic::AttributeSemant) {
							if (attr.value == Lexic::AttributeSUnk) ctx.SetClassSemantics(type, XA::ArgumentSemantics::Unknown);
							else if (attr.value == Lexic::AttributeSNone) ctx.SetClassSemantics(type, XA::ArgumentSemantics::Unclassified);
							else if (attr.value == Lexic::AttributeSUInt) ctx.SetClassSemantics(type, XA::ArgumentSemantics::Integer);
							else if (attr.value == Lexic::AttributeSSInt) ctx.SetClassSemantics(type, XA::ArgumentSemantics::SignedInteger);
							else if (attr.value == Lexic::AttributeSFloat) ctx.SetClassSemantics(type, XA::ArgumentSemantics::FloatingPoint);
							else if (attr.value == Lexic::AttributeSObject) ctx.SetClassSemantics(type, XA::ArgumentSemantics::Object);
							else if (attr.value == Lexic::AttributeSThis) ctx.SetClassSemantics(type, XA::ArgumentSemantics::This);
							else if (attr.value == Lexic::AttributeSRTTI) ctx.SetClassSemantics(type, XA::ArgumentSemantics::RTTI);
							else if (attr.value == Lexic::AttributeSError) ctx.SetClassSemantics(type, XA::ArgumentSemantics::ErrorData);
							else Abort(CompilerStatus::InapproptiateAttribute, definition);
						} else Abort(CompilerStatus::InapproptiateAttribute, definition);
					} else type->AddAttribute(attr.key, attr.value);
				}
				attributes.Clear();

				// TODO: IMPLEMENT INTERFACES AND PARENT CLASS

				AssertPunct(L"{"); ReadNextToken();
				VObservationDesc subdesc;
				subdesc.current_namespace = type;
				subdesc.namespace_search_list << type;
				subdesc.namespace_search_list << desc.namespace_search_list;
				ProcessClass(type, is_interface, subdesc);
				AssertPunct(L"}"); ReadNextToken();
			}
			void ProcessAliasDefinition(Volumes::Dictionary<string, string> & attributes, VObservationDesc & desc)
			{
				if (!attributes.IsEmpty()) Abort(CompilerStatus::InapproptiateAttribute, current_token);
				ReadNextToken(); AssertIdent();
				auto definition = current_token;
				ReadNextToken(); AssertPunct(L"=");
				ReadNextToken();
				auto expr = current_token;
				SafePointer<XL::LObject> dest = ProcessExpression(desc);
				AssertPunct(L";"); ReadNextToken();
				try { ctx.CreateAlias(desc.current_namespace, definition.contents, dest); }
				catch (XL::ObjectMemberRedefinitionException &) { Abort(CompilerStatus::SymbolRedefinition, definition); }
				catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, expr); }
			}
			void ProcessConstantDefinition(Volumes::Dictionary<string, string> & attributes, VObservationDesc & desc)
			{
				ReadNextToken(); AssertIdent();
				auto definition = current_token;
				ReadNextToken(); AssertPunct(L"=");
				ReadNextToken();
				auto expr = current_token;
				SafePointer<XL::LObject> value = ProcessExpression(desc);
				AssertPunct(L";"); ReadNextToken();
				if (value->GetClass() != XL::Class::Literal) Abort(CompilerStatus::ExpressionMustBeConst, expr);
				value = ctx.QueryDetachedLiteral(value);
				for (auto & attr : attributes) {
					if (attr.key[0] == L'[') Abort(CompilerStatus::InapproptiateAttribute, definition);
					else value->AddAttribute(attr.key, attr.value);
				}
				attributes.Clear();
				try { ctx.AttachLiteral(value, desc.current_namespace, definition.contents); }
				catch (...) { Abort(CompilerStatus::SymbolRedefinition, definition); }
			}
			void ProcessUsingDefinition(VObservationDesc & desc)
			{
				ReadNextToken();
				auto expr = current_token;
				SafePointer<XL::LObject> ns = ProcessExpression(desc);
				if (ns->GetClass() != XL::Class::Namespace) Abort(CompilerStatus::ObjectTypeMismatch, expr);
				desc.namespace_search_list.Append(ns);
				AssertPunct(L";"); ReadNextToken();
			}
			void ProcessAttributeDefinition(Volumes::Dictionary<string, string> & attributes)
			{
				Token definition;
				string key, value;
				ReadNextToken();
				if (IsPunct(L"[")) {
					ReadNextToken(); AssertGenericIdent();
					definition = current_token;
					key = L"[" + current_token.contents + L"]";
					ReadNextToken(); AssertPunct(L"]"); ReadNextToken();
				} else if (IsGenericIdent()) {
					definition = current_token;
					key = current_token.contents;
					ReadNextToken();
					while (IsPunct(L".")) {
						ReadNextToken(); AssertGenericIdent();
						key += L"." + current_token.contents;
						ReadNextToken();
					}
				} else Abort(CompilerStatus::AnotherTokenExpected, current_token);
				if (attributes.ElementExists(key)) Abort(CompilerStatus::SymbolRedefinition, definition);
				if (IsGenericIdent()) {
					value = current_token.contents;
					ReadNextToken();
				}
				AssertPunct(L"]"); ReadNextToken();
				if (key == Lexic::AttributeSystem) {
					if (value == Lexic::AttributeConsole) {
						ctx.MakeSubsystemConsole();
						module_is_library = false;
					} else if (value == Lexic::AttributeGUI) {
						ctx.MakeSubsystemGUI();
						module_is_library = false;
					} else if (value == Lexic::AttributeNoUI) {
						ctx.MakeSubsystemNone();
						module_is_library = false;
					} else if (value == Lexic::AttributeLibrary) {
						ctx.MakeSubsystemLibrary();
						module_is_library = true;
					} else Abort(CompilerStatus::InvalidSubsystem, definition);
				} else attributes.Append(key, value);
			}
			void ProcessClass(XL::LObject * cls, int is_interface, VObservationDesc & desc)
			{
				Volumes::Dictionary<string, string> attributes;
				while (!IsPunct(L"}")) {
					if (IsPunct(L"[")) {
						ProcessAttributeDefinition(attributes);
					} else if (IsKeyword(Lexic::KeywordClass) || IsKeyword(Lexic::KeywordInterface)) {
						ProcessClassDefinition(attributes, desc);
					} else if (IsKeyword(Lexic::KeywordAlias)) {
						ProcessAliasDefinition(attributes, desc);
					} else if (IsKeyword(Lexic::KeywordFunction) ||
						IsKeyword(Lexic::KeywordClassFunc) || IsKeyword(Lexic::KeywordCtor) ||
						IsKeyword(Lexic::KeywordDtor) || IsKeyword(Lexic::KeywordConvertor)) {
						ProcessFunctionDefinition(attributes, desc);
					} else if (IsKeyword(Lexic::KeywordConst)) {
						ProcessConstantDefinition(attributes, desc);
					} else if (IsKeyword(Lexic::KeywordUse)) {
						ProcessUsingDefinition(desc);
					} else if (IsKeyword(Lexic::KeywordVariable)) {
						ProcessVariableDefinition(attributes, desc);
					} else Abort(CompilerStatus::AnotherTokenExpected, current_token);

					// TODO: ADD FIELDS
					// TODO: ADD PROPERTIES
				}
			}
			void ProcessNamespace(VObservationDesc & desc)
			{
				Volumes::Dictionary<string, string> attributes;
				while (!IsEOF() && !IsPunct(L"}")) {
					if (IsPunct(L"[")) {
						ProcessAttributeDefinition(attributes);
					} else if (IsKeyword(Lexic::KeywordNamespace)) {
						if (!attributes.IsEmpty()) Abort(CompilerStatus::InapproptiateAttribute, current_token);
						ReadNextToken(); AssertIdent();
						XL::LObject * ns;
						try { ns = ctx.CreateNamespace(desc.current_namespace, current_token.contents); }
						catch (...) { Abort(CompilerStatus::SymbolRedefinition, current_token); }
						ReadNextToken(); AssertPunct(L"{");
						ReadNextToken();
						VObservationDesc subdesc;
						subdesc.current_namespace = ns;
						subdesc.namespace_search_list << ns;
						subdesc.namespace_search_list << desc.namespace_search_list;
						ProcessNamespace(subdesc);
						AssertPunct(L"}"); ReadNextToken();
					} else if (IsKeyword(Lexic::KeywordClass) || IsKeyword(Lexic::KeywordInterface)) {
						ProcessClassDefinition(attributes, desc);
					} else if (IsKeyword(Lexic::KeywordAlias)) {
						ProcessAliasDefinition(attributes, desc);
					} else if (IsKeyword(Lexic::KeywordFunction)) {
						ProcessFunctionDefinition(attributes, desc);
					} else if (IsKeyword(Lexic::KeywordConst)) {
						ProcessConstantDefinition(attributes, desc);
					} else if (IsKeyword(Lexic::KeywordUse)) {
						ProcessUsingDefinition(desc);
					} else if (IsKeyword(Lexic::KeywordVariable)) {
						ProcessVariableDefinition(attributes, desc);
					} else if (IsKeyword(Lexic::KeywordImport)) {
						// TODO: ADD IMPORT
					} else if (IsKeyword(Lexic::KeywordResource)) {
						ReadNextToken();
						if (IsIdent()) {
							if (current_token.contents == Lexic::ResourceData) {
								ReadNextToken(); AssertPunct(L"("); ReadNextToken();
								auto type_expr = current_token;
								SafePointer<XL::LObject> rsrc_type = ProcessExpression(desc);
								AssertPunct(L","); ReadNextToken();
								auto id_expr = current_token;
								SafePointer<XL::LObject> rsrc_id = ProcessExpression(desc);
								AssertPunct(L")"); ReadNextToken(); AssertPunct(L"="); ReadNextToken();
								auto file_expr = current_token; ReadNextToken();
								AssertPunct(L";"); ReadNextToken();
								string type, file;
								int id;
								SafePointer<DataBlock> data;
								if (file_expr.type != TokenType::Literal || file_expr.ex_data != TokenLiteralString) Abort(CompilerStatus::AnotherTokenExpected, file_expr);
								if (rsrc_type->GetClass() != XL::Class::Literal) Abort(CompilerStatus::ExpressionMustBeConst, type_expr);
								if (rsrc_id->GetClass() != XL::Class::Literal) Abort(CompilerStatus::ExpressionMustBeConst, id_expr);
								try { type = ctx.QueryLiteralString(rsrc_type); }
								catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, type_expr); }
								try { id = ctx.QueryLiteralValue(rsrc_id); }
								catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, id_expr); }
								file = file_expr.contents;
								try {
									if (!callback) throw Exception();
									SafePointer<Streaming::Stream> stream = callback->QueryResourceFileStream(file);
									if (!stream) throw Exception();
									if (stream->Length() > 0x1000000) throw Exception();
									data = stream->ReadAll();
								} catch (...) { Abort(CompilerStatus::ResourceFileNotFound, file_expr); }
								if (type.Length() > 4) Abort(CompilerStatus::InvalidResourceType, type_expr);
								for (int i = 0; i < type.Length(); i++) if (type[i] < 32 || type[i] > 127) Abort(CompilerStatus::InvalidResourceType, type_expr);
								if (!ctx.QueryResources().Append(type + L":" + string(id), data)) Abort(CompilerStatus::SymbolRedefinition, type_expr);
							} else if (current_token.contents == Lexic::ResourceIcon) {
								ReadNextToken(); AssertPunct(L"("); ReadNextToken();
								auto id_expr = current_token;
								SafePointer<XL::LObject> rsrc_id = ProcessExpression(desc);
								AssertPunct(L")"); ReadNextToken(); AssertPunct(L"="); ReadNextToken();
								auto file_expr = current_token; ReadNextToken();
								AssertPunct(L";"); ReadNextToken();
								string file;
								int id;
								SafePointer<Streaming::Stream> stream;
								SafePointer<Codec::Image> picture;
								if (file_expr.type != TokenType::Literal || file_expr.ex_data != TokenLiteralString) Abort(CompilerStatus::AnotherTokenExpected, file_expr);
								if (rsrc_id->GetClass() != XL::Class::Literal) Abort(CompilerStatus::ExpressionMustBeConst, id_expr);
								try { id = ctx.QueryLiteralValue(rsrc_id); }
								catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, id_expr); }
								file = file_expr.contents;
								try {
									if (!callback) throw Exception();
									stream = callback->QueryResourceFileStream(file);
									if (!stream) throw Exception();
								} catch (...) { Abort(CompilerStatus::ResourceFileNotFound, file_expr); }
								try {
									picture = Codec::DecodeImage(stream);
									if (!picture) throw Exception();
									stream.SetReference(0);
								} catch (...) { Abort(CompilerStatus::InvalidPictureFormat, file_expr); }
								XI::AddModuleIcon(ctx.QueryResources(), id, picture);
							} else if (current_token.contents == Lexic::ResourceMeta) {
								ReadNextToken(); AssertPunct(L"("); ReadNextToken();
								auto key_expr = current_token;
								SafePointer<XL::LObject> meta_key = ProcessExpression(desc);
								AssertPunct(L")"); ReadNextToken(); AssertPunct(L"="); ReadNextToken();
								auto value_expr = current_token;
								SafePointer<XL::LObject> meta_value = ProcessExpression(desc);
								AssertPunct(L";"); ReadNextToken();
								string key, value;
								if (meta_key->GetClass() != XL::Class::Literal) Abort(CompilerStatus::ExpressionMustBeConst, key_expr);
								if (meta_value->GetClass() != XL::Class::Literal) Abort(CompilerStatus::ExpressionMustBeConst, value_expr);
								try { key = ctx.QueryLiteralString(meta_key); }
								catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, key_expr); }
								try { value = ctx.QueryLiteralString(meta_value); }
								catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, value_expr); }
								if (!metadata.Append(key, value)) Abort(CompilerStatus::SymbolRedefinition, key_expr);
							} else Abort(CompilerStatus::AnotherTokenExpected);
						} else Abort(CompilerStatus::AnotherTokenExpected);
					} else Abort(CompilerStatus::AnotherTokenExpected, current_token);
				}
			}
			void ProcessStatement(XL::LFunctionContext & fctx, VObservationDesc & desc, bool allow_new_regular_scope)
			{
				// TODO: IMPLEMENT

				if (IsPunct(L"{")) {
					ReadNextToken();
					if (allow_new_regular_scope) {
						XL::LObject * scope;
						fctx.OpenRegularBlock(&scope);
						VObservationDesc subdesc;
						subdesc.current_namespace = scope;
						subdesc.namespace_search_list << scope;
						subdesc.namespace_search_list << desc.namespace_search_list;
						ProcessBlock(fctx, subdesc);
						fctx.CloseRegularBlock();
					} else ProcessBlock(fctx, desc);
					AssertPunct(L"}"); ReadNextToken();
				} else if (IsPunct(L";")) {
					ReadNextToken();
				} else if (IsKeyword(Lexic::KeywordTry)) {
					ReadNextToken();
					XL::LObject * scope;
					fctx.OpenTryBlock(&scope);
					VObservationDesc subdesc;
					subdesc.current_namespace = scope;
					subdesc.namespace_search_list << scope;
					subdesc.namespace_search_list << desc.namespace_search_list;
					ProcessStatement(fctx, subdesc, false);
					if (IsKeyword(Lexic::KeywordCatch)) {
						ReadNextToken();
						if (IsPunct(L"(")) {
							ReadNextToken();
							if (!IsPunct(L")")) {
								SafePointer<XL::LObject> e1t = ProcessTypeExpression(desc);
								AssertIdent(); auto e1n = current_token.contents; ReadNextToken();
								if (IsPunct(L",")) {
									ReadNextToken();
									SafePointer<XL::LObject> e2t = ProcessTypeExpression(desc);
									AssertIdent(); auto e2n = current_token.contents; ReadNextToken();
									AssertPunct(L")"); ReadNextToken();
									fctx.OpenCatchBlock(&scope, e1n, e2n, e1t, e2t);
								} else {
									AssertPunct(L")"); ReadNextToken();
									fctx.OpenCatchBlock(&scope, e1n, L"", e1t, 0);
								}
							} else {
								ReadNextToken();
								fctx.OpenCatchBlock(&scope, L"", L"", 0, 0);
							}
						} else fctx.OpenCatchBlock(&scope, L"", L"", 0, 0);
						VObservationDesc subdesc;
						subdesc.current_namespace = scope;
						subdesc.namespace_search_list.Clear();
						subdesc.namespace_search_list << scope;
						subdesc.namespace_search_list << desc.namespace_search_list;
						ProcessStatement(fctx, subdesc, false);
						fctx.CloseCatchBlock();
					} else {
						fctx.OpenCatchBlock(&scope, L"", L"", 0, 0);
						fctx.CloseCatchBlock();
					}
				} else if (IsKeyword(Lexic::KeywordThrow)) {
					auto definition = current_token;
					ReadNextToken();
					SafePointer<XL::LObject> error_expr = ProcessExpression(desc);
					SafePointer<XL::LObject> suberror_expr;
					if (IsPunct(L",")) {
						ReadNextToken();
						suberror_expr = ProcessExpression(desc);
					}
					AssertPunct(L";"); ReadNextToken();
					try { if (suberror_expr) fctx.EncodeThrow(error_expr, suberror_expr); else fctx.EncodeThrow(error_expr); }
					catch (XL::ObjectHasNoSuchOverloadException &) { Abort(CompilerStatus::NoSuchOverload, definition); }
					catch (XL::ObjectIsNotEvaluatableException &) { Abort(CompilerStatus::ExpressionMustBeValue, definition); }
					catch (XL::ObjectMayThrow &) { Abort(CompilerStatus::InvalidThrowPlace, definition); }
					catch (InvalidArgumentException &) { Abort(CompilerStatus::NoSuchOverload, definition); }
					catch (InvalidStateException &) { Abort(CompilerStatus::InvalidThrowPlace, definition); }
					catch (...) { Abort(CompilerStatus::InternalError, definition); }
				} else {
					auto definition = current_token;
					SafePointer<XL::LObject> expr = ProcessExpression(desc);
					AssertPunct(L";"); ReadNextToken();
					try { fctx.EncodeExpression(expr); }
					catch (XL::ObjectIsNotEvaluatableException &) { Abort(CompilerStatus::ExpressionMustBeValue, definition); }
					catch (XL::ObjectMayThrow &) { Abort(CompilerStatus::InvalidThrowPlace, definition); }
					catch (...) { Abort(CompilerStatus::InternalError, definition); }
				}
			}
			void ProcessBlock(XL::LFunctionContext & fctx, VObservationDesc & desc) { while (!IsPunct(L"}") && !IsEOF()) ProcessStatement(fctx, desc, true); }
			void ProcessCode(VPostCompileDesc & pc)
			{
				OverrideInput(pc.code_source);
				Array<XL::LObject *> argv(0x10);
				for (auto & a : pc.input_types) argv << &a;
				SafePointer<XL::LFunctionContext> fctx = new XL::LFunctionContext(ctx, pc.function, pc.flags, pc.retval, argv.Length(), argv.GetBuffer(), pc.input_names.GetBuffer());
				VObservationDesc desc;
				desc.current_namespace = fctx->GetRootScope();
				desc.namespace_search_list << fctx->GetRootScope();
				desc.namespace_search_list << pc.visibility.namespace_search_list;
				ProcessBlock(*fctx, desc);
				fctx->EndEncoding();
			}
			void Process(void)
			{
				try {
					ReadNextToken();
					VObservationDesc desc;
					desc.current_namespace = ctx.GetRootNamespace();
					desc.namespace_search_list << desc.current_namespace;
					ProcessNamespace(desc);
					if (current_token.type != TokenType::EOF) Abort(CompilerStatus::AnotherTokenExpected, current_token);
					for (auto & pc : post_compile) ProcessCode(pc);
					if (!metadata.IsEmpty()) XI::AddModuleMetadata(ctx.QueryResources(), metadata);
				} catch (...) { if (status.status == CompilerStatus::Success) SetStatusError(status, CompilerStatus::InternalError); }
			}
		};

		class ListCompilerCallback : public ICompilerCallback
		{
			Array<string> _res, _mdl;
			SafePointer<ICompilerCallback> _dropback;
		public:
			ListCompilerCallback(const string * res_pv, int res_pc, const string * mdl_pv, int mdl_pc, ICompilerCallback * dropback) :
				_res(res_pc), _mdl(mdl_pc)
			{
				_dropback.SetRetain(dropback);
				_res.Append(res_pv, res_pc);
				_mdl.Append(mdl_pv, mdl_pc);
			}
			virtual ~ListCompilerCallback(void) override {}
			virtual Streaming::Stream * QueryModuleFileStream(const string & module_name) override
			{
				for (auto & path : _mdl) try {
					return new Streaming::FileStream(path + L"/" + module_name + L"." + XX::FileExtensionLibrary, Streaming::AccessRead, Streaming::OpenExisting);
				} catch (...) {}
				if (_dropback) return _dropback->QueryModuleFileStream(module_name);
				throw IO::FileAccessException(IO::Error::FileNotFound);
			}
			virtual Streaming::Stream * QueryResourceFileStream(const string & resource_file_name) override
			{
				for (auto & path : _res) try {
					return new Streaming::FileStream(path + L"/" + resource_file_name, Streaming::AccessRead, Streaming::OpenExisting);
				} catch (...) {}
				if (_dropback) return _dropback->QueryResourceFileStream(resource_file_name);
				throw IO::FileAccessException(IO::Error::FileNotFound);
			}
			virtual string ToString(void) const override { return L"ListCompilerCallback"; }
		};
		class OutputModule : public IOutputModule
		{
			string _name, _ext;
			SafePointer<Streaming::Stream> _stream;
		public:
			OutputModule(const string & name, const string & ext, Streaming::Stream * data) : _name(name), _ext(ext) { _stream.SetRetain(data); }
			virtual ~OutputModule(void) override {}
			virtual string GetOutputModuleName(void) const override { return _name; }
			virtual string GetOutputModuleExtension(void) const override { return _ext; }
			virtual Streaming::Stream * GetOutputModuleData(void) const override { return _stream; }
			virtual string ToString(void) const override { return L"OutputModule"; }
		};
		ICompilerCallback * CreateCompilerCallback(const string * res_pv, int res_pc, const string * mdl_pv, int mdl_pc, ICompilerCallback * dropback) { return new ListCompilerCallback(res_pv, res_pc, mdl_pv, mdl_pc, dropback); }
		void CompileModule(const string & module_name, const Array<uint32> & input, IOutputModule ** output, ICompilerCallback * callback, CompilerStatusDesc & status)
		{
			try {
				XL::LContext lctx(module_name);
				lctx.MakeSubsystemConsole();
				SafePointer<TokenStream> input_stream = new TokenStream(input.GetBuffer(), input.Length());
				VContext vctx(lctx, callback, status, input_stream);
				status.status = CompilerStatus::Success;
				vctx.Process();
				if (status.status == CompilerStatus::Success) {
					SafePointer<Streaming::MemoryStream> data = new Streaming::MemoryStream(0x10000);
					lctx.ProduceModule(Meta::Stamp, Meta::VersionMajor, Meta::VersionMinor, Meta::Subversion, Meta::BuildNumber, data);
					data->Seek(0, Streaming::Begin);
					if (vctx.module_is_library) *output = new OutputModule(module_name, XX::FileExtensionLibrary, data);
					else *output = new OutputModule(module_name, XX::FileExtensionExecutable, data);
					SetStatusError(status, CompilerStatus::Success);
				}
			} catch (...) { SetStatusError(status, CompilerStatus::InternalError); }
		}
		void CompileModule(const string & input, string & output_path, ICompilerCallback * callback, CompilerStatusDesc & status)
		{
			string input_path = IO::ExpandPath(input);
			Array<uint32> input_string(0x1000);
			SafePointer<IOutputModule> output;
			SafePointer<ICompilerCallback> internal_callback;
			try {
				SafePointer<Streaming::Stream> stream = new Streaming::FileStream(input_path, Streaming::AccessRead, Streaming::OpenExisting);
				SafePointer<Streaming::TextReader> reader = new Streaming::TextReader(stream);
				while (!reader->EofReached()) {
					auto code = reader->ReadChar();
					if (code != 0xFFFFFFFF) input_string << code;
				}
				string src_dir = IO::Path::GetDirectory(input_path);
				internal_callback = CreateCompilerCallback(&src_dir, 1, &src_dir, 1, callback);
			} catch (...) {
				SetStatusError(status, CompilerStatus::FileAccessFailure, input_path);
				return;
			}
			CompileModule(IO::Path::GetFileNameWithoutExtension(input_path), input_string, output.InnerRef(), internal_callback, status);
			if (status.status != CompilerStatus::Success) return;
			output_path = IO::ExpandPath(output_path + L"/" + output->GetOutputModuleName() + L"." + output->GetOutputModuleExtension());
			try {
				SafePointer<Streaming::Stream> stream = new Streaming::FileStream(output_path, Streaming::AccessWrite, Streaming::CreateAlways);
				auto data = output->GetOutputModuleData();
				data->CopyTo(stream);
			} catch (...) {
				SetStatusError(status, CompilerStatus::FileAccessFailure, output_path);
				return;
			}
		}
	}
}