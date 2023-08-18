#include "xv_compiler.h"

#include "xv_meta.h"
#include "xv_lexical.h"
#include "xv_proto.h"
#include "xv_oapi.h"
#include "../xlang/xl_code.h"
#include "../xasm/xa_compiler.h"
#include "../ximg/xi_resources.h"

namespace Engine
{
	namespace XV
	{
		constexpr int InitPriorityVFT	= 1;
		constexpr int InitPriorityVar	= 2;
		constexpr int InitPriorityUser	= 3;

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
		
		struct VEnumerableDesc
		{
			string iterator_name;
			bool iterator_enforce_ref;
			SafePointer<XL::LObject> iterator;
			SafePointer<XL::LObject> init, step, cond;
		};
		struct VInitServiceDesc
		{
			int priority;
			SafePointer<XL::LObject> init_expr;
			SafePointer<XL::LObject> shwn_expr;
		};
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
			ObjectArray<XL::LObject> objects_retain = ObjectArray<XL::LObject>(0x10);
			Array<string> input_names = Array<string>(0x10);
			uint flags;
			SafePointer<ITokenStream> code_source;
			bool constructor, destructor;
		};
		struct VContext : public XL::IModuleLoadCallback, public ICompilationContext
		{
			bool module_is_library;
			XL::LContext & ctx;
			SafePointer<TokenStream> input;
			SafePointer<ITokenStream> input_override;
			ICompilerCallback * callback;
			CompilerStatusDesc & status;
			Token current_token;
			Volumes::List<VInitServiceDesc> init_list;
			Volumes::List<VPostCompileDesc> post_compile;
			Volumes::Dictionary<string, string> metadata;

			VContext(XL::LContext & _lal, ICompilerCallback * _callback, CompilerStatusDesc & _status, TokenStream * stream) :
				module_is_library(false), ctx(_lal), callback(_callback), status(_status) { input.SetRetain(stream); }
			virtual Streaming::Stream * GetModuleStream(const string & name) override
			{
				if (callback) return callback->QueryModuleFileStream(name);
				else throw IO::FileAccessException(IO::Error::FileNotFound);
			}
			virtual XL::LContext * GetLanguageContext(void) override { return &ctx; }
			virtual XL::LObject * ProcessLanguageExpression(ITokenStream * input, Token & input_current_token, XL::LObject ** vns, int num_vns) override
			{
				SafePointer<XL::LObject> result;
				auto _io = input_override;
				auto _s = status;
				auto _t = current_token;
				try {
					VObservationDesc desc;
					desc.current_namespace = 0;
					for (int i = 0; i < num_vns; i++) desc.namespace_search_list << vns[i];
					input_override.SetRetain(input);
					current_token = input_current_token;
					result = ProcessExpression(desc);
					input_current_token = current_token;
				} catch (...) { result.SetReference(0); }
				input_override = _io;
				status = _s;
				current_token = _t;
				if (result) result->Retain();
				return result;
			}
			virtual bool ProcessLanguageDefinitions(ITokenStream * input, XL::LObject * dest_ns, XL::LObject ** vns, int num_vns) override
			{
				bool result = true;
				auto _io = input_override;
				auto _s = status;
				auto _t = current_token;
				try {
					VObservationDesc desc;
					desc.current_namespace = dest_ns;
					for (int i = 0; i < num_vns; i++) desc.namespace_search_list << vns[i];
					OverrideInput(input);
					ProcessNamespace(desc);
				} catch (...) { result = false; }
				input_override = _io;
				status = _s;
				current_token = _t;
				return result;
			}
			ITokenStream * ExposeInput(void) { return input_override ? input_override.Inner() : input.Inner(); }
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
			void OverrideInput(ITokenStream * new_input) { input_override.SetRetain(new_input); ReadNextToken(); }
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
			void RegisterInitHandler(int priority, XL::LObject * init, XL::LObject * sdwn)
			{
				VInitServiceDesc desc;
				desc.priority = priority;
				desc.init_expr.SetRetain(init);
				desc.shwn_expr.SetRetain(sdwn);
				auto element = init_list.GetFirst();
				while (element && element->GetValue().priority <= priority) element = element->GetNext();
				if (element) init_list.InsertBefore(element, desc);
				else init_list.InsertLast(desc);
			}
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
			XL::LObject * ProcessExpressionSubject(XL::LObject ** ssl, int ssc)
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
				} else if (IsKeyword(Lexic::KeywordSizeOfMX)) {
					ReadNextToken();
					object = ctx.QuerySizeOfOperator(true);
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
				} else if (IsKeyword(Lexic::KeywordNull)) {
					ReadNextToken(); object = ctx.QueryNullLiteral();
				} else if (IsKeyword(Lexic::KeywordNew)) {
					ReadNextToken(); object = CreateOperatorNew(ctx);
				} else if (IsKeyword(Lexic::KeywordConstruct)) {
					ReadNextToken(); object = CreateOperatorConstruct(ctx);
				} else Abort(CompilerStatus::AnotherTokenExpected);
				object->Retain();
				return object;
			}
			XL::LObject * ProcessExpressionPostfix(XL::LObject ** ssl, int ssc)
			{
				SafePointer<XL::LObject> object = ProcessExpressionSubject(ssl, ssc);
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
						if (object->GetClass() == XL::Class::InstancedProperty && (op.contents == XL::OperatorIncrement || op.contents == XL::OperatorDecrement)) {
							SafePointer<XL::LObject> method, inter;
							try { method = object->GetMember(op.contents); }
							catch (...) { Abort(CompilerStatus::NoSuchSymbol, op); }
							try { inter = method->Invoke(0, 0); }
							catch (...) { Abort(CompilerStatus::NoSuchOverload, op); }
							try { object = ctx.SetPropertyValue(object, inter); } catch (...) { Abort(CompilerStatus::NoSuchOverload, op); }
						} else {
							SafePointer<XL::LObject> method;
							try { method = object->GetMember(op.contents); }
							catch (...) { Abort(CompilerStatus::NoSuchSymbol, op); }
							try { object = method->Invoke(0, 0); }
							catch (...) { Abort(CompilerStatus::NoSuchOverload, op); }
						}
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
						try {
							SafePointer<XL::LObject> take_addr = ctx.QueryAddressOfOperator();
							return take_addr->Invoke(1, object.InnerRef());
						} catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, definition); }
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
					ReadNextToken();
					if (IsPunct(L"[")) {
						Array<int> dim_list(0x10);
						ReadNextToken(); auto d1e = current_token;
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
					} else {
						auto definition = current_token;
						SafePointer<XL::LObject> type = ProcessExpressionUnary(ssl, ssc);
						try {
							SafePointer<XL::LObject> dynamic_array = ctx.QueryObject(L"dordo");
							SafePointer<XL::LObject> operator_instantiate = dynamic_array->GetMember(XL::OperatorSubscript);
							type = operator_instantiate->Invoke(1, type.InnerRef());
						} catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, definition); }
						type->Retain();
						return type;
					}
				} else return ProcessExpressionPostfix(ssl, ssc);
			}
			XL::LObject * ProcessExpressionMultiplicative(XL::LObject ** ssl, int ssc)
			{
				SafePointer<XL::LObject> object = ProcessExpressionUnary(ssl, ssc);
				while (IsPunct(XL::OperatorAnd) || IsPunct(XL::OperatorMultiply) || IsPunct(XL::OperatorDivide) ||
					IsPunct(XL::OperatorResidual) || IsPunct(XL::OperatorShiftLeft) || IsPunct(XL::OperatorShiftRight)) {
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
					IsPunct(XL::OperatorSubtract) || IsPunct(XL::OperatorCompare)) {
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
				SafePointer<XL::LObject> object = ProcessExpressionComparative(ssl, ssc);
				ObjectArray<XL::LObject> args(0x10);
				Array<XL::LObject *> argv(0x10);
				args.Append(object); argv.Append(object);
				auto definition = current_token;
				while (IsPunct(L"||")) {
					auto op = current_token; ReadNextToken();
					SafePointer<XL::LObject> arg = ProcessExpressionComparative(ssl, ssc);
					args.Append(arg); argv.Append(arg);
				}
				if (argv.Length() > 1) {
					try {
						SafePointer<XL::LObject> op = ctx.QueryLogicalOrOperator();
						object = op->Invoke(argv.Length(), argv.GetBuffer());
					} catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, definition); }
				}
				object->Retain();
				return object;
			}
			XL::LObject * ProcessExpressionLogicalAnd(XL::LObject ** ssl, int ssc)
			{
				SafePointer<XL::LObject> object = ProcessExpressionLogicalOr(ssl, ssc);
				ObjectArray<XL::LObject> args(0x10);
				Array<XL::LObject *> argv(0x10);
				args.Append(object); argv.Append(object);
				auto definition = current_token;
				while (IsPunct(L"&&")) {
					auto op = current_token; ReadNextToken();
					SafePointer<XL::LObject> arg = ProcessExpressionLogicalOr(ssl, ssc);
					args.Append(arg); argv.Append(arg);
				}
				if (argv.Length() > 1) {
					try {
						SafePointer<XL::LObject> op = ctx.QueryLogicalAndOperator();
						object = op->Invoke(argv.Length(), argv.GetBuffer());
					} catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, definition); }
				}
				object->Retain();
				return object;
			}
			XL::LObject * ProcessExpressionTernary(XL::LObject ** ssl, int ssc)
			{
				SafePointer<XL::LObject> primary = ProcessExpressionLogicalAnd(ssl, ssc);
				if (IsPunct(L"?")) {
					auto definition = current_token; ReadNextToken();
					SafePointer<XL::LObject> if_true = ProcessExpressionTernary(ssl, ssc);
					AssertPunct(L":"); ReadNextToken();
					SafePointer<XL::LObject> if_false = ProcessExpressionTernary(ssl, ssc);
					try { return ctx.QueryTernaryResult(primary, if_true, if_false); }
					catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, definition); }
				} else { primary->Retain(); return primary; }
			}
			XL::LObject * ProcessExpressionAssignation(XL::LObject ** ssl, int ssc)
			{
				SafePointer<XL::LObject> object = ProcessExpressionTernary(ssl, ssc);
				if (IsPunct(XL::OperatorAssign) || IsPunct(XL::OperatorAOr) || IsPunct(XL::OperatorAXor) ||
					IsPunct(XL::OperatorAAnd) || IsPunct(XL::OperatorAAdd) || IsPunct(XL::OperatorASubtract) ||
					IsPunct(XL::OperatorAMultiply) || IsPunct(XL::OperatorADivide) || IsPunct(XL::OperatorAResidual) ||
					IsPunct(XL::OperatorAShiftLeft) || IsPunct(XL::OperatorAShiftRight)) {
					auto op = current_token;
					ReadNextToken();
					SafePointer<XL::LObject> rs = ProcessExpressionAssignation(ssl, ssc);
					if (object->GetClass() == XL::Class::InstancedProperty) {
						if (op.contents == XL::OperatorAssign) {
							try { object = ctx.SetPropertyValue(object, rs); } catch (...) { Abort(CompilerStatus::NoSuchOverload, op); }
						} else {
							auto sop = op.contents.Fragment(0, op.contents.Length() - 1);
							SafePointer<XL::LObject> op_obj, inter;
							try { op_obj = ctx.GetRootNamespace()->GetMember(sop); } catch (...) { Abort(CompilerStatus::NoSuchSymbol, op); }
							try {
								XL::LObject * argv[2] = { object, rs };
								inter = op_obj->Invoke(2, argv);
							} catch (...) { Abort(CompilerStatus::NoSuchOverload, op); }
							try { object = ctx.SetPropertyValue(object, inter); } catch (...) { Abort(CompilerStatus::NoSuchOverload, op); }
						}
					} else {
						SafePointer<XL::LObject> op_obj;
						try { op_obj = object->GetMember(op.contents); } catch (...) { Abort(CompilerStatus::NoSuchSymbol, op); }
						try { object = op_obj->Invoke(1, rs.InnerRef()); } catch (...) { Abort(CompilerStatus::NoSuchOverload, op); }
					}
				}
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
				ReadNextToken();
				SafePointer<XL::LObject> type = ProcessTypeExpression(desc);
				while (true) {
					auto definition = current_token; AssertIdent(); auto name = current_token.contents; ReadNextToken();
					XL::LObject * var;
					try { var = ctx.CreateVariable(desc.current_namespace, name, type); }
					catch (...) { Abort(CompilerStatus::SymbolRedefinition, definition); }
					for (auto & attr : attributes) {
						if (attr.key[0] == L'[') Abort(CompilerStatus::InapproptiateAttribute, definition);
						else var->AddAttribute(attr.key, attr.value);
					}
					attributes.Clear();
					SafePointer<XL::LObject> init, sdwn;
					if (IsPunct(L"(")) {
						ObjectArray<XL::LObject> args(0x10);
						Array<XL::LObject *> argv(0x10);
						ReadNextToken();
						if (IsPunct(L")")) ReadNextToken(); else while (true) {
							SafePointer<XL::LObject> arg = ProcessExpression(desc);
							args.Append(arg); argv.Append(arg);
							if (IsPunct(L")")) { ReadNextToken(); break; }
							AssertPunct(L","); ReadNextToken(); 
						}
						try { init = ctx.InitInstance(var, argv.Length(), argv.GetBuffer()); }
						catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, definition); }
					} else if (IsPunct(L"=")) {
						ReadNextToken();
						SafePointer<XL::LObject> expr = ProcessExpression(desc);
						try { init = ctx.InitInstance(var, expr); }
						catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, definition); }
					} else {
						try { init = ctx.InitInstance(var, 0, 0); }
						catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, definition); }
					}
					sdwn = ctx.DestroyInstance(var);
					if (init) RegisterInitHandler(InitPriorityVar, init, sdwn);
					else if (sdwn) RegisterInitHandler(InitPriorityVar, 0, sdwn);
					if (IsPunct(L";")) { ReadNextToken(); break; }
					AssertPunct(L","); ReadNextToken();
				}
			}
			void ProcessPrototypeDefinition(Volumes::Dictionary<string, string> & attributes, VObservationDesc & desc)
			{
				bool function;
				string name;
				Array<string> arg_list(0x10);
				ReadNextToken();
				if (IsKeyword(Lexic::KeywordClass)) {
					ReadNextToken(); function = false;
				} else {
					AssertKeyword(Lexic::KeywordFunction);
					ReadNextToken(); function = true;
				}
				auto def = current_token;
				AssertIdent(); name = current_token.contents; ReadNextToken();
				AssertPunct(L"("); ReadNextToken();
				while (true) {
					AssertIdent(); string arg = current_token.contents;
					for (auto & a : arg_list) if (a == arg) Abort(CompilerStatus::SymbolRedefinition, current_token);
					arg_list.Append(arg);
					ReadNextToken(); if (IsPunct(L",")) ReadNextToken(); else break;
				}
				AssertPunct(L")"); ReadNextToken();
				XL::LObject * prot;
				try {
					if (function) prot = CreateFunctionPrototype(this, desc.current_namespace, name, arg_list.Length(), arg_list.GetBuffer());
					else prot = CreateClassPrototype(this, desc.current_namespace, name, arg_list.Length(), arg_list.GetBuffer());
				} catch (...) {Abort( CompilerStatus::SymbolRedefinition, def); }
				for (auto & attr : attributes) {
					if (attr.key[0] == L'[') Abort(CompilerStatus::InapproptiateAttribute, def);
					else prot->AddAttribute(attr.key, attr.value);
				}
				attributes.Clear();
				SetPrototypeVisibility(prot, desc.namespace_search_list.GetBuffer(), desc.namespace_search_list.Length());
				AssertPunct(L"{");
				SafePointer<ITokenStream> stream;
				if (!ExposeInput()->ReadBlock(stream.InnerRef())) Abort(CompilerStatus::InvalidTokenInput, ExposeInput()->GetCurrentPosition());
				ReadNextToken();
				SupplyPrototypeImplementation(prot, stream);
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
				if (is_conv) { AssertPunct(L"("); ReadNextToken(); }
				if (is_ctor || is_dtor) retval = ctx.QueryObject(XL::NameVoid);
				else retval = ProcessTypeExpression(desc);
				if (is_conv) { AssertPunct(L")"); ReadNextToken(); }
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
				while (IsKeyword(Lexic::KeywordEntry) || IsKeyword(Lexic::KeywordThrows) || IsKeyword(Lexic::KeywordVirtual) ||
					IsKeyword(Lexic::KeywordPure) || IsKeyword(Lexic::KeywordOverride)) {
					if (IsKeyword(Lexic::KeywordEntry)) {
						if (!is_static) Abort(CompilerStatus::InvalidFunctionTrats, current_token);
						flags |= XL::FunctionMain;
					} else if (IsKeyword(Lexic::KeywordThrows)) flags |= XL::FunctionThrows;
					else if (IsKeyword(Lexic::KeywordVirtual)) {
						if (is_static || is_ctor) Abort(CompilerStatus::InvalidFunctionTrats, current_token);
						flags |= XL::FunctionVirtual;
					} else if (IsKeyword(Lexic::KeywordPure)) {
						if (!(flags & XL::FunctionVirtual)) Abort(CompilerStatus::InvalidFunctionTrats, current_token);
						if (flags & XL::FunctionOverride) Abort(CompilerStatus::InvalidFunctionTrats, current_token);
						if (is_static || is_ctor || is_dtor) Abort(CompilerStatus::InvalidFunctionTrats, current_token);
						org = -1; flags |= XL::FunctionPureCall;
					} else if (IsKeyword(Lexic::KeywordOverride)) {
						if (flags & XL::FunctionPureCall) Abort(CompilerStatus::InvalidFunctionTrats, current_token);
						if (is_static || is_ctor) Abort(CompilerStatus::InvalidFunctionTrats, current_token);
						flags |= XL::FunctionOverride;
					}
					ReadNextToken();
				}
				for (auto & attr : attributes) {
					if (attr.key == Lexic::AttributeNoTC) flags &= ~XL::FunctionThisCall;
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
							try {
								SafePointer<XL::LObject> inv = func->Invoke(0, 0);
								RegisterInitHandler(InitPriorityUser, inv, 0);
							} catch (...) { Abort(CompilerStatus::InapproptiateAttribute, definition); }
						} else if (attr.key == Lexic::AttributeFinal) {
							if (!is_static || attr.value.Length()) Abort(CompilerStatus::InapproptiateAttribute, definition);
							try {
								SafePointer<XL::LObject> inv = func->Invoke(0, 0);
								RegisterInitHandler(InitPriorityUser, 0, inv);
							} catch (...) { Abort(CompilerStatus::InapproptiateAttribute, definition); }
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
					SafePointer<ITokenStream> stream;
					if (!ExposeInput()->ReadBlock(stream.InnerRef())) Abort(CompilerStatus::InvalidTokenInput, ExposeInput()->GetCurrentPosition());
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
					for (auto & v : desc.namespace_search_list) if (v->GetClass() != XL::Class::Namespace) pc.objects_retain.Append(v);
					post_compile.InsertLast(pc);
				} else if (org == 1) {
					auto start = current_token;
					AssertPunct(L"{");
					SafePointer<ITokenStream> asm_stream;
					if (!ExposeInput()->ReadBlock(asm_stream.InnerRef())) Abort(CompilerStatus::InvalidTokenInput, ExposeInput()->GetCurrentPosition());
					ReadNextToken();
					auto asm_code = asm_stream->ExtractContents();
					XA::CompilerStatusDesc asm_desc;
					XA::Function asm_func;
					XA::CompileFunction(asm_code, asm_func, asm_desc);
					if (asm_desc.status != XA::CompilerStatus::Success) Abort(CompilerStatus::AssemblyError, start);
					ctx.SupplyFunctionImplementation(func, asm_func);
				} else {
					AssertPunct(L";"); ReadNextToken();
					ctx.SupplyFunctionImplementation(func, import_name, import_lib);
				}
			}
			void ProcessClassDefinition(Volumes::Dictionary<string, string> & attributes, VObservationDesc & desc)
			{
				bool is_interface = IsKeyword(Lexic::KeywordInterface);
				bool is_structure = IsKeyword(Lexic::KeywordStructure);
				ReadNextToken();
				bool is_abstract = IsKeyword(Lexic::KeywordVirtual) || is_interface;
				if (IsKeyword(Lexic::KeywordVirtual)) ReadNextToken();
				AssertIdent();
				auto definition = current_token;
				XL::LObject * type;
				try { type = ctx.CreateClass(desc.current_namespace, current_token.contents); }
				catch (...) { Abort(CompilerStatus::SymbolRedefinition, current_token); }
				ctx.LockClass(type, true);
				if (is_interface) ctx.MarkClassAsInterface(type);
				ReadNextToken();
				if (IsKeyword(Lexic::KeywordInherit)) {
					ReadNextToken();
					auto def = current_token;
					SafePointer<XL::LObject> parent = ProcessTypeExpression(desc);
					try { ctx.AdoptParentClass(type, parent); }
					catch (...) { Abort(CompilerStatus::InvalidParentClass, def); }
				} else if (is_abstract) ctx.CreateClassVFT(type);
				if (IsPunct(L":") && !is_interface) {
					ReadNextToken();
					while (true) {
						auto def = current_token;
						SafePointer<XL::LObject> interface = ProcessTypeExpression(desc);
						try { ctx.AdoptInterface(type, interface); }
						catch (...) { Abort(CompilerStatus::InvalidInterfaceClass, def); }
						if (!IsPunct(L",")) break;
						ReadNextToken();
					}
				}
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
				AssertPunct(L"{"); ReadNextToken();
				VObservationDesc subdesc;
				subdesc.current_namespace = type;
				subdesc.namespace_search_list << type;
				subdesc.namespace_search_list << desc.namespace_search_list;
				ProcessClass(type, is_interface, false, subdesc);
				AssertPunct(L"}"); ReadNextToken();
				ObjectArray<XL::LObject> vft_init_seq(0x40);
				ctx.CreateClassDefaultMethods(type, XL::CreateMethodDestructor, vft_init_seq);
				if (is_structure) ctx.CreateClassDefaultMethods(type, XL::CreateMethodConstructorInit |
					XL::CreateMethodConstructorCopy | XL::CreateMethodConstructorMove | XL::CreateMethodConstructorZero |
					XL::CreateMethodAssign, vft_init_seq);

				// TODO: ADD AUTO METHODS

				ctx.LockClass(type, false);
				for (auto & s : vft_init_seq) RegisterInitHandler(InitPriorityVFT, &s, 0);
			}
			void ProcessAliasDefinition(Volumes::Dictionary<string, string> & attributes, VObservationDesc & desc)
			{
				if (!attributes.IsEmpty()) Abort(CompilerStatus::InapproptiateAttribute, current_token);
				ReadNextToken(); AssertIdent();
				auto definition = current_token;
				ReadNextToken(); AssertPunct(L"=");
				ReadNextToken();
				auto expr = current_token;
				VObservationDesc vspec;
				vspec.current_namespace = desc.current_namespace;
				vspec.namespace_search_list << desc.current_namespace;
				SafePointer<XL::LObject> dest = ProcessExpression(desc.current_namespace->GetClass() == XL::Class::Type ? vspec : desc);
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
			void ProcessContinue(VObservationDesc & desc)
			{
				ReadNextToken();
				auto definition = current_token;
				SafePointer<XL::LObject> type = ProcessTypeExpression(desc);
				if (!type->IsDefinedLocally()) Abort(CompilerStatus::MustBeLocalClass, definition);
				AssertPunct(L"{"); ReadNextToken();
				VObservationDesc subdesc;
				subdesc.current_namespace = type;
				subdesc.namespace_search_list << type;
				subdesc.namespace_search_list << desc.namespace_search_list;
				ProcessClass(type, ctx.IsInterface(type), true, subdesc);
				AssertPunct(L"}"); ReadNextToken();
			}
			void ProcessProperty(XL::LObject * prop, XL::LObject * prop_type, VObservationDesc & desc)
			{
				Volumes::Dictionary<string, string> attributes;
				while (!IsPunct(L"}")) {
					if (IsPunct(L"[")) {
						ProcessAttributeDefinition(attributes);
					} else {
						AssertIdent();
						bool setter;
						if (current_token.contents == Lexic::IdentifierSet) setter = true;
						else if (current_token.contents == Lexic::IdentifierGet) setter = false;
						else Abort(CompilerStatus::AnotherTokenExpected, current_token);
						auto definition = current_token;
						ReadNextToken();
						string import_name, import_lib;
						uint flags = XL::FunctionMethod | XL::FunctionThisCall;
						uint org = 0; // 0 - V, 1 - A, 2 - import, 3 - import from library, -1 - pure
						while (IsKeyword(Lexic::KeywordThrows) || IsKeyword(Lexic::KeywordVirtual) ||
							IsKeyword(Lexic::KeywordPure) || IsKeyword(Lexic::KeywordOverride)) {
							if (IsKeyword(Lexic::KeywordThrows)) flags |= XL::FunctionThrows;
							else if (IsKeyword(Lexic::KeywordVirtual)) {
								flags |= XL::FunctionVirtual;
							} else if (IsKeyword(Lexic::KeywordPure)) {
								if (!(flags & XL::FunctionVirtual)) Abort(CompilerStatus::InvalidFunctionTrats, current_token);
								if (flags & XL::FunctionOverride) Abort(CompilerStatus::InvalidFunctionTrats, current_token);
								org = -1; flags |= XL::FunctionPureCall;
							} else if (IsKeyword(Lexic::KeywordOverride)) {
								if (flags & XL::FunctionPureCall) Abort(CompilerStatus::InvalidFunctionTrats, current_token);
								flags |= XL::FunctionOverride;
							}
							ReadNextToken();
						}
						for (auto & attr : attributes) {
							if (attr.key == Lexic::AttributeNoTC) flags &= ~XL::FunctionThisCall;
						}
						XL::LObject * func;
						try {
							if (setter) func = ctx.CreatePropertySetter(prop, flags);
							else func = ctx.CreatePropertyGetter(prop, flags);
						} catch (...) { Abort(CompilerStatus::SymbolRedefinition, definition); }
						for (auto & attr : attributes) {
							if (attr.key[0] == L'[') {
								if (attr.key == Lexic::AttributeAsm) {
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
							SafePointer<ITokenStream> stream;
							if (!ExposeInput()->ReadBlock(stream.InnerRef())) Abort(CompilerStatus::InvalidTokenInput, ExposeInput()->GetCurrentPosition());
							ReadNextToken();
							VPostCompileDesc pc;
							pc.visibility = desc;
							pc.function = func;
							pc.instance.SetRetain(desc.current_namespace);
							if (setter) {
								SafePointer<XL::LObject> ref_type = ctx.QueryTypeReference(prop_type);
								SafePointer<XL::LObject> void_type = ctx.QueryObject(XL::NameVoid);
								pc.retval = void_type;
								pc.input_types.Append(ref_type);
								pc.input_names.Append(Lexic::IdentifierValue);
							} else pc.retval.SetRetain(prop_type);
							pc.flags = flags;
							pc.code_source = stream;
							pc.constructor = false;
							pc.destructor = false;
							for (auto & v : desc.namespace_search_list) if (v->GetClass() != XL::Class::Namespace) pc.objects_retain.Append(v);
							post_compile.InsertLast(pc);
						} else if (org == 1) {
							auto start = current_token;
							AssertPunct(L"{");
							SafePointer<ITokenStream> asm_stream;
							if (!ExposeInput()->ReadBlock(asm_stream.InnerRef())) Abort(CompilerStatus::InvalidTokenInput, ExposeInput()->GetCurrentPosition());
							ReadNextToken();
							auto asm_code = asm_stream->ExtractContents();
							XA::CompilerStatusDesc asm_desc;
							XA::Function asm_func;
							XA::CompileFunction(asm_code, asm_func, asm_desc);
							if (asm_desc.status != XA::CompilerStatus::Success) Abort(CompilerStatus::AssemblyError, start);
							ctx.SupplyFunctionImplementation(func, asm_func);
						} else {
							AssertPunct(L";"); ReadNextToken();
							ctx.SupplyFunctionImplementation(func, import_name, import_lib);
						}

					}
				}
			}
			void ProcessClass(XL::LObject * cls, bool is_interface, bool is_continue, VObservationDesc & desc)
			{
				Volumes::Dictionary<string, string> attributes;
				while (!IsPunct(L"}")) {
					if (IsPunct(L"[")) {
						ProcessAttributeDefinition(attributes);
					} else if (IsKeyword(Lexic::KeywordClass) || IsKeyword(Lexic::KeywordInterface) || IsKeyword(Lexic::KeywordStructure)) {
						ProcessClassDefinition(attributes, desc);
					} else if (IsKeyword(Lexic::KeywordContinue)) {
						ProcessContinue(desc);
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
					} else if (IsKeyword(Lexic::KeywordPrototype)) {
						ProcessPrototypeDefinition(attributes, desc);
					} else {
						auto type_def = current_token;
						SafePointer<XL::LObject> type = ProcessTypeExpression(desc);
						AssertIdent(); auto name = current_token.contents; auto def = current_token; ReadNextToken();
						if (IsPunct(L";") && !is_interface && !is_continue) {
							ReadNextToken();
							int align_mode = 0; // 0 - align, 1 - don't align, 2 - explicit offset
							XA::ObjectSize offset;
							for (auto & a : attributes) {
								if (a.key == Lexic::AttributeOffset) {
									if (align_mode) Abort(CompilerStatus::InapproptiateAttribute, def);
									align_mode = 2;
									offset.num_bytes = offset.num_words = 0;
									auto sep = a.value.Split(L':');
									if (sep.Length() > 2) Abort(CompilerStatus::InapproptiateAttribute, def);
									try {
										offset.num_bytes = sep[0].ToUInt32();
										if (sep.Length() == 2) offset.num_words = sep[1].ToUInt32();
									} catch (...) { Abort(CompilerStatus::InapproptiateAttribute, def); }
								} else if (a.key == Lexic::AttributeUnalign) {
									if (align_mode || a.value.Length()) Abort(CompilerStatus::InapproptiateAttribute, def);
									align_mode = 1;
								}
							}
							XL::LObject * field;
							try {
								if (align_mode == 2) field = ctx.CreateField(cls, name, type, offset);
								else if (align_mode == 1) field = ctx.CreateField(cls, name, type, false);
								else field = ctx.CreateField(cls, name, type, true);
							} catch (...) { Abort(CompilerStatus::SymbolRedefinition, def); }
							for (auto & a : attributes) {
								if (a.key[0] == L'[') {
									if (a.key == Lexic::AttributeOffset) {
									} else if (a.key == Lexic::AttributeUnalign) {
									} else Abort(CompilerStatus::InapproptiateAttribute, def);
								} else field->AddAttribute(a.key, a.value);
							}
						} else {
							AssertPunct(L"{"); ReadNextToken();
							XL::LObject * prop;
							try { prop = ctx.CreateProperty(cls, name, type); }
							catch (...) { Abort(CompilerStatus::SymbolRedefinition, def); }
							for (auto & a : attributes) {
								if (a.key[0] == L'[') Abort(CompilerStatus::InapproptiateAttribute, def);
								else prop->AddAttribute(a.key, a.value);
							}
							ProcessProperty(prop, type, desc);
							AssertPunct(L"}"); ReadNextToken();
						}
						attributes.Clear();
					}
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
					} else if (IsKeyword(Lexic::KeywordClass) || IsKeyword(Lexic::KeywordInterface) || IsKeyword(Lexic::KeywordStructure)) {
						ProcessClassDefinition(attributes, desc);
					} else if (IsKeyword(Lexic::KeywordContinue)) {
						ProcessContinue(desc);
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
					} else if (IsKeyword(Lexic::KeywordPrototype)) {
						ProcessPrototypeDefinition(attributes, desc);
					} else if (IsKeyword(Lexic::KeywordImport)) {
						ReadNextToken();
						AssertGenericIdent();
						try { if (!ctx.IncludeModule(current_token.contents, this)) throw Exception(); }
						catch (...) { Abort(CompilerStatus::ReferenceAccessFailure, current_token); }
						ReadNextToken(); AssertPunct(L";"); ReadNextToken();
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
			void ProcessLocalVariable(XL::LFunctionContext & fctx, VObservationDesc & desc, XL::LObject * type, bool auto_ref)
			{
				while (true) {
					auto definition = current_token;
					AssertIdent(); auto name = current_token.contents; ReadNextToken();
					SafePointer<XL::LObject> var;
					if (IsPunct(L"(") && type) {
						ObjectArray<XL::LObject> args(0x10);
						Array<XL::LObject *> argv(0x10);
						ReadNextToken();
						if (IsPunct(L")")) ReadNextToken(); else while (true) {
							SafePointer<XL::LObject> arg = ProcessExpression(desc);
							args.Append(arg); argv.Append(arg);
							if (IsPunct(L")")) { ReadNextToken(); break; }
							AssertPunct(L","); ReadNextToken(); 
						}
						try { var = fctx.EncodeCreateVariable(type, argv.Length(), argv.GetBuffer()); }
						catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, definition); }
					} else if (IsPunct(L"=")) {
						ReadNextToken();
						SafePointer<XL::LObject> expr = ProcessExpression(desc);
						try {
							SafePointer<XL::LObject> act_type;
							if (type) act_type.SetRetain(type); else {
								act_type = expr->GetType();
								if (auto_ref) act_type = ctx.QueryTypeReference(act_type);
							}
							var = fctx.EncodeCreateVariable(act_type, expr);
						} catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, definition); }
					} else if (type) {
						try { var = fctx.EncodeCreateVariable(type); }
						catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, definition); }
					} else Abort(CompilerStatus::InvalidAutoVariable, definition);
					try { desc.current_namespace->AddMember(name, var); }
					catch (...) { Abort(CompilerStatus::SymbolRedefinition, definition); }
					if (IsPunct(L";")) { ReadNextToken(); break; }
					AssertPunct(L","); ReadNextToken(); 
				}
			}
			void ProcessForRange(XL::LFunctionContext & fctx, VObservationDesc & desc, VEnumerableDesc & edesc)
			{
				bool invert = false;
				if (IsPunct(L"~")) { ReadNextToken(); invert = true; }
				if (IsPunct(L"[")) {
					ReadNextToken();
					edesc.init = ProcessExpression(desc);
					AssertPunct(L","); ReadNextToken();
					edesc.cond = ProcessExpression(desc);
					if (invert) swap(edesc.init, edesc.cond);
					if (edesc.iterator_name.Length()) {
						if (!edesc.iterator) {
							edesc.iterator = edesc.init->GetType();
							if (edesc.iterator_enforce_ref) edesc.iterator = ctx.QueryTypeReference(edesc.iterator);
						}
						edesc.iterator = fctx.EncodeCreateVariable(edesc.iterator, edesc.init);
						desc.current_namespace->AddMember(edesc.iterator_name, edesc.iterator);
					} else {
						SafePointer<XL::LObject> asgn = edesc.iterator->GetMember(XL::OperatorAssign);
						edesc.init = asgn->Invoke(1, edesc.init.InnerRef());
					}
					SafePointer<XL::LObject> cmp;
					if (invert) cmp = ctx.GetRootNamespace()->GetMember(XL::OperatorGreaterEqual);
					else cmp = ctx.GetRootNamespace()->GetMember(XL::OperatorLesserEqual);
					XL::LObject * argv[2] = { edesc.iterator.Inner(), edesc.cond.Inner() };
					edesc.cond = cmp->Invoke(2, argv);
					if (IsPunct(L";")) {
						ReadNextToken();
						edesc.step = ProcessExpression(desc);
						SafePointer<XL::LObject> it;
						if (invert) it = edesc.iterator->GetMember(XL::OperatorASubtract);
						else it = edesc.iterator->GetMember(XL::OperatorAAdd);
						edesc.step = it->Invoke(1, edesc.step.InnerRef());
					} else {
						SafePointer<XL::LObject> it;
						if (invert) it = edesc.iterator->GetMember(XL::OperatorDecrement);
						else it = edesc.iterator->GetMember(XL::OperatorIncrement);
						edesc.step = it->Invoke(0, 0);
					}
					AssertPunct(L"]"); ReadNextToken();
				} else {
					string step;
					SafePointer<XL::LObject> range = ProcessExpression(desc);
					SafePointer<XL::LObject> begin, end;
					if (invert) {
						begin = range->GetMember(XL::IteratorEnd);
						begin = begin->Invoke(0, 0);
						end = range->GetMember(XL::IteratorPreBegin);
						end = end->Invoke(0, 0);
						step = XL::OperatorDecrement;
					} else {
						begin = range->GetMember(XL::IteratorBegin);
						begin = begin->Invoke(0, 0);
						end = range->GetMember(XL::IteratorPostEnd);
						end = end->Invoke(0, 0);
						step = XL::OperatorIncrement;
					}
					if (edesc.iterator_name.Length()) {
						if (!edesc.iterator) {
							edesc.iterator = begin->GetType();
							if (edesc.iterator_enforce_ref) edesc.iterator = ctx.QueryTypeReference(edesc.iterator);
						}
						edesc.init = edesc.iterator = fctx.EncodeCreateVariable(edesc.iterator, begin);
						desc.current_namespace->AddMember(edesc.iterator_name, edesc.iterator);
					} else {
						SafePointer<XL::LObject> asgn = edesc.iterator->GetMember(XL::OperatorAssign);
						edesc.init = asgn->Invoke(1, begin.InnerRef());
					}
					SafePointer<XL::LObject> step_method = edesc.iterator->GetMember(step);
					edesc.step = step_method->Invoke(0, 0);
					SafePointer<XL::LObject> neq = ctx.GetRootNamespace()->GetMember(XL::OperatorNotEqual);
					XL::LObject * argv[2] = { edesc.iterator.Inner(), end.Inner() };
					edesc.cond = neq->Invoke(2, argv);
				}
			}
			void ProcessStatement(XL::LFunctionContext & fctx, VObservationDesc & desc, bool allow_new_regular_scope)
			{
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
				} else if (IsKeyword(Lexic::KeywordIf)) {
					ReadNextToken(); AssertPunct(L"("); ReadNextToken();
					auto expr = current_token;
					XL::LObject * scope;
					SafePointer<XL::LObject> cond = ProcessExpression(desc);
					try { fctx.OpenIfBlock(cond, &scope); }
					catch (XL::ObjectIsNotEvaluatableException &) { Abort(CompilerStatus::ExpressionMustBeValue, expr); }
					catch (XL::ObjectMayThrow &) { Abort(CompilerStatus::InvalidThrowPlace, expr); }
					catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, expr); }
					AssertPunct(L")"); ReadNextToken();
					VObservationDesc subdesc;
					subdesc.current_namespace = scope;
					subdesc.namespace_search_list << scope;
					subdesc.namespace_search_list << desc.namespace_search_list;
					ProcessStatement(fctx, subdesc, false);
					if (IsKeyword(Lexic::KeywordElse)) {
						ReadNextToken();
						fctx.OpenElseBlock(&scope);
						subdesc.current_namespace = scope;
						subdesc.namespace_search_list.Clear();
						subdesc.namespace_search_list << scope;
						subdesc.namespace_search_list << desc.namespace_search_list;
						ProcessStatement(fctx, subdesc, false);
						fctx.CloseIfElseBlock();
					} else fctx.CloseIfElseBlock();
				} else if (IsKeyword(Lexic::KeywordFor)) {
					VObservationDesc inter;
					inter.current_namespace = desc.current_namespace;
					inter.namespace_search_list = desc.namespace_search_list;
					auto def = current_token;
					ReadNextToken(); AssertPunct(L"("); ReadNextToken();
					string create_local_name;
					SafePointer<XL::LObject> create_local_type;
					SafePointer<XL::LObject> init_statement, step_statement, cond_statement;
					if (IsKeyword(Lexic::KeywordVariable)) {
						if (allow_new_regular_scope) {
							XL::LObject * inter_scope;
							fctx.OpenRegularBlock(&inter_scope);
							inter.current_namespace = inter_scope;
							inter.namespace_search_list.Insert(inter_scope, 0);
						}
						ReadNextToken();
						bool create_local_enf_ref = false;
						if (IsPunct(XL::OperatorReferInvert)) { ReadNextToken(); create_local_enf_ref = true; }
						AssertIdent(); create_local_name = current_token.contents; ReadNextToken();
						if (IsPunct(L":")) {
							ReadNextToken();
							VEnumerableDesc vdesc;
							vdesc.iterator_name = create_local_name;
							vdesc.iterator_enforce_ref = create_local_enf_ref;
							ProcessForRange(fctx, inter, vdesc);
							init_statement = vdesc.init;
							step_statement = vdesc.step;
							cond_statement = vdesc.cond;
							create_local_type = init_statement;
						} else {
							AssertPunct(L"="); ReadNextToken();
							auto init_statement = ProcessExpression(inter);
							create_local_type = init_statement->GetType();
							if (create_local_enf_ref) create_local_type = ctx.QueryTypeReference(create_local_type);
							SafePointer<XL::LObject> var = fctx.EncodeCreateVariable(create_local_type, init_statement);
							inter.current_namespace->AddMember(create_local_name, var);
						}
					} else {
						SafePointer<XL::LObject> expr = ProcessExpression(desc);
						if (expr->GetClass() == XL::Class::Type) {
							if (allow_new_regular_scope) {
								XL::LObject * inter_scope;
								fctx.OpenRegularBlock(&inter_scope);
								inter.current_namespace = inter_scope;
								inter.namespace_search_list.Insert(inter_scope, 0);
							}
							create_local_type = expr;
							AssertIdent(); create_local_name = current_token.contents; ReadNextToken();
							if (IsPunct(L":")) {
								ReadNextToken();
								VEnumerableDesc vdesc;
								vdesc.iterator_name = create_local_name;
								vdesc.iterator_enforce_ref = false;
								vdesc.iterator = create_local_type;
								ProcessForRange(fctx, inter, vdesc);
								init_statement = vdesc.init;
								step_statement = vdesc.step;
								cond_statement = vdesc.cond;
							} else if (IsPunct(L"=")) {
								ReadNextToken();
								init_statement = ProcessExpression(inter);
								SafePointer<XL::LObject> var = fctx.EncodeCreateVariable(create_local_type, init_statement);
								inter.current_namespace->AddMember(create_local_name, var);
							} else {
								SafePointer<XL::LObject> var = fctx.EncodeCreateVariable(create_local_type);
								inter.current_namespace->AddMember(create_local_name, var);
							}
						} else {
							init_statement = expr;
							if (IsPunct(L":")) {
								ReadNextToken();
								VEnumerableDesc vdesc;
								vdesc.iterator_enforce_ref = false;
								vdesc.iterator = init_statement;
								ProcessForRange(fctx, inter, vdesc);
								init_statement = vdesc.init;
								step_statement = vdesc.step;
								cond_statement = vdesc.cond;
							}
						}
					}
					if (!cond_statement) {
						AssertPunct(L";"); ReadNextToken();
						cond_statement = ProcessExpression(inter);
						AssertPunct(L";"); ReadNextToken();
						step_statement = ProcessExpression(inter);
					}
					AssertPunct(L")"); ReadNextToken();
					XL::LObject * scope;
					try {
						if (!create_local_type) fctx.EncodeExpression(init_statement);
						fctx.OpenForBlock(cond_statement, step_statement, &scope);
					} catch (XL::ObjectIsNotEvaluatableException &) { Abort(CompilerStatus::ExpressionMustBeValue, def); }
					catch (XL::ObjectMayThrow &) { Abort(CompilerStatus::InvalidThrowPlace, def); }
					catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, def); }
					VObservationDesc subdesc;
					subdesc.current_namespace = scope;
					subdesc.namespace_search_list << scope;
					subdesc.namespace_search_list << inter.namespace_search_list;
					ProcessStatement(fctx, subdesc, false);
					fctx.CloseForBlock();
					if (create_local_type && allow_new_regular_scope) fctx.CloseRegularBlock();
				} else if (IsKeyword(Lexic::KeywordWhile)) {
					ReadNextToken(); AssertPunct(L"("); ReadNextToken();
					auto expr = current_token;
					XL::LObject * scope;
					SafePointer<XL::LObject> cond = ProcessExpression(desc);
					try { fctx.OpenWhileBlock(cond, &scope); }
					catch (XL::ObjectIsNotEvaluatableException &) { Abort(CompilerStatus::ExpressionMustBeValue, expr); }
					catch (XL::ObjectMayThrow &) { Abort(CompilerStatus::InvalidThrowPlace, expr); }
					catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, expr); }
					AssertPunct(L")"); ReadNextToken();
					VObservationDesc subdesc;
					subdesc.current_namespace = scope;
					subdesc.namespace_search_list << scope;
					subdesc.namespace_search_list << desc.namespace_search_list;
					ProcessStatement(fctx, subdesc, false);
					fctx.CloseWhileBlock();
				} else if (IsKeyword(Lexic::KeywordDo)) {
					ReadNextToken();
					XL::LObject * scope;
					fctx.OpenDoWhileBlock(&scope);
					VObservationDesc subdesc;
					subdesc.current_namespace = scope;
					subdesc.namespace_search_list << scope;
					subdesc.namespace_search_list << desc.namespace_search_list;
					ProcessStatement(fctx, subdesc, false);
					AssertKeyword(Lexic::KeywordWhile); ReadNextToken(); AssertPunct(L"("); ReadNextToken();
					auto expr = current_token;
					SafePointer<XL::LObject> cond = ProcessExpression(desc);
					try { fctx.CloseDoWhileBlock(cond); }
					catch (XL::ObjectIsNotEvaluatableException &) { Abort(CompilerStatus::ExpressionMustBeValue, expr); }
					catch (XL::ObjectMayThrow &) { Abort(CompilerStatus::InvalidThrowPlace, expr); }
					catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, expr); }
					AssertPunct(L")"); ReadNextToken(); AssertPunct(L";"); ReadNextToken();
				} else if (IsKeyword(Lexic::KeywordBreak)) {
					auto definition = current_token; ReadNextToken();
					SafePointer<XL::LObject> dist;
					if (!IsPunct(L";")) {
						auto expr = current_token;
						dist = ProcessExpression(desc);
						if (dist->GetClass() != XL::Class::Literal) Abort(CompilerStatus::ExpressionMustBeConst, expr);
						try {
							int index = ctx.QueryLiteralValue(dist);
							if (index < 0) throw Exception();
						} catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, expr); }
					}
					AssertPunct(L";"); ReadNextToken();
					try { if (dist) fctx.EncodeBreak(dist.Inner()); else fctx.EncodeBreak(); }
					catch (...) { Abort(CompilerStatus::InvalidLoopCtrlPlace, definition); }
				} else if (IsKeyword(Lexic::KeywordContinue)) {
					auto definition = current_token; ReadNextToken();
					SafePointer<XL::LObject> dist;
					if (!IsPunct(L";")) {
						auto expr = current_token;
						dist = ProcessExpression(desc);
						if (dist->GetClass() != XL::Class::Literal) Abort(CompilerStatus::ExpressionMustBeConst, expr);
						try {
							int index = ctx.QueryLiteralValue(dist);
							if (index < 0) throw Exception();
						} catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, expr); }
					}
					AssertPunct(L";"); ReadNextToken();
					try { if (dist) fctx.EncodeContinue(dist.Inner()); else fctx.EncodeContinue(); }
					catch (...) { Abort(CompilerStatus::InvalidLoopCtrlPlace, definition); }
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
				} else if (IsKeyword(Lexic::KeywordVariable)) {
					ReadNextToken();
					bool ref = false;
					if (IsPunct(XL::OperatorReferInvert)) { ref = true; ReadNextToken(); }
					ProcessLocalVariable(fctx, desc, 0, ref);
				} else if (IsKeyword(Lexic::KeywordReturn)) {
					ReadNextToken();
					if (fctx.IsZeroReturn()) {
						AssertPunct(L";"); ReadNextToken();
						fctx.EncodeReturn(0);
					} else {
						auto definition = current_token;
						SafePointer<XL::LObject> expr = ProcessExpression(desc);
						try { fctx.EncodeReturn(expr); }
						catch (XL::ObjectHasNoSuchOverloadException &) { Abort(CompilerStatus::ObjectTypeMismatch, definition); }
						catch (XL::ObjectHasNoSuchMemberException &) { Abort(CompilerStatus::ObjectTypeMismatch, definition); }
						catch (XL::ObjectIsNotEvaluatableException &) { Abort(CompilerStatus::ExpressionMustBeValue, definition); }
						catch (XL::ObjectMayThrow &) { Abort(CompilerStatus::InvalidThrowPlace, definition); }
						catch (InvalidArgumentException &) { Abort(CompilerStatus::ObjectTypeMismatch, definition); }
						catch (InvalidStateException &) { Abort(CompilerStatus::InvalidThrowPlace, definition); }
						catch (...) { Abort(CompilerStatus::InternalError, definition); }
						AssertPunct(L";"); ReadNextToken();
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
				} else if (IsKeyword(Lexic::KeywordDelete)) {
					ReadNextToken();
					auto definition = current_token;
					SafePointer<XL::LObject> subj = ProcessExpression(desc);
					AssertPunct(L";"); ReadNextToken();
					try {
						SafePointer<XL::LObject> eval = CreateDelete(subj);
						fctx.EncodeExpression(eval);
						eval = CreateFree(subj);
						fctx.EncodeExpression(eval);
					} catch (XL::ObjectIsNotEvaluatableException &) { Abort(CompilerStatus::ExpressionMustBeValue, definition); }
					catch (XL::ObjectMayThrow &) { Abort(CompilerStatus::InvalidThrowPlace, definition); }
					catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, definition); }
				} else if (IsKeyword(Lexic::KeywordDestruct)) {
					ReadNextToken();
					auto definition = current_token;
					SafePointer<XL::LObject> subj = ProcessExpression(desc);
					AssertPunct(L";"); ReadNextToken();
					try {
						SafePointer<XL::LObject> eval = CreateDestruct(subj);
						fctx.EncodeExpression(eval);
					} catch (XL::ObjectIsNotEvaluatableException &) { Abort(CompilerStatus::ExpressionMustBeValue, definition); }
					catch (XL::ObjectMayThrow &) { Abort(CompilerStatus::InvalidThrowPlace, definition); }
					catch (...) { Abort(CompilerStatus::ObjectTypeMismatch, definition); }
				} else if (IsKeyword(Lexic::KeywordTrap)) {
					ReadNextToken(); AssertPunct(L";"); ReadNextToken();
					fctx.EncodeTrap();
				} else {
					auto definition = current_token;
					SafePointer<XL::LObject> expr = ProcessExpression(desc);
					if (expr->GetClass() == XL::Class::Type) {
						ProcessLocalVariable(fctx, desc, expr, false);
					} else {
						AssertPunct(L";"); ReadNextToken();
						try { fctx.EncodeExpression(expr); }
						catch (XL::ObjectIsNotEvaluatableException &) { Abort(CompilerStatus::ExpressionMustBeValue, definition); }
						catch (XL::ObjectMayThrow &) { Abort(CompilerStatus::InvalidThrowPlace, definition); }
						catch (...) { Abort(CompilerStatus::InternalError, definition); }
					}
				}
			}
			void ProcessBlock(XL::LFunctionContext & fctx, VObservationDesc & desc) { while (!IsPunct(L"}") && !IsEOF()) ProcessStatement(fctx, desc, true); }
			void ProcessCode(VPostCompileDesc & pc)
			{
				class FunctionInitCallback : public XL::IFunctionInitCallback
				{
					ObjectArray<XL::LObject> retain_list;
					VContext & _vctx;
					VPostCompileDesc & _pc;
				public:
					FunctionInitCallback(VContext & vctx, VPostCompileDesc & pc) : retain_list(0x80), _vctx(vctx), _pc(pc) {}
					virtual void GetNextInit(XL::LObject * arguments_scope, XL::FunctionInitDesc & desc) override
					{
						desc.init.Clear();
						if (_vctx.IsKeyword(Lexic::KeywordInit)) {
							VObservationDesc vdesc;
							vdesc.current_namespace = arguments_scope;
							vdesc.namespace_search_list << arguments_scope;
							vdesc.namespace_search_list << _pc.visibility.namespace_search_list;
							_vctx.ReadNextToken();
							if (_vctx.IsPunct(L"(") || _vctx.IsPunct(L"=")) {
								desc.subject = _pc.instance;
							} else {
								_vctx.AssertIdent();
								SafePointer<XL::LObject> field = _pc.instance->GetMember(_vctx.current_token.contents);
								if (field->GetClass() != XL::Class::Field) throw InvalidArgumentException();
								retain_list.Append(field);
								desc.subject = field;
								_vctx.ReadNextToken();
							}
							if (_vctx.IsPunct(L"(")) {
								_vctx.ReadNextToken();
								if (!_vctx.IsPunct(L")")) {
									while (true) {
										SafePointer<XL::LObject> expr = _vctx.ProcessExpression(vdesc);
										retain_list.Append(expr);
										desc.init.InsertLast(expr);
										if (_vctx.IsPunct(L")")) { _vctx.ReadNextToken(); break; }
										_vctx.AssertPunct(L","); _vctx.ReadNextToken();
									}
								} else _vctx.ReadNextToken();
							} else {
								_vctx.AssertPunct(L"="); _vctx.ReadNextToken();
								SafePointer<XL::LObject> expr = _vctx.ProcessExpression(vdesc);
								retain_list.Append(expr);
								desc.init.InsertLast(expr);
							}
							_vctx.AssertPunct(L";"); _vctx.ReadNextToken();
						} else desc.subject = 0;
					}
				};
				FunctionInitCallback init_callback(*this, pc);
				OverrideInput(pc.code_source);
				ObjectArray<XL::LObject> vft_init_seq(0x40);
				Array<XL::LObject *> argv(0x10);
				for (auto & a : pc.input_types) argv << &a;
				XL::FunctionContextDesc fc_desc;
				fc_desc.retval = pc.retval;
				fc_desc.instance = pc.instance;
				fc_desc.argc = argv.Length();
				fc_desc.argvt = argv.GetBuffer();
				fc_desc.argvn = pc.input_names.GetBuffer();
				fc_desc.this_name = Lexic::IdentifierThis;
				fc_desc.flags = pc.flags;
				fc_desc.vft_init = 0;
				fc_desc.vft_init_seq = 0;
				fc_desc.create_init_sequence = pc.constructor;
				fc_desc.create_shutdown_sequence = pc.destructor;
				fc_desc.init_callback = 0;
				if (pc.constructor) {
					fc_desc.vft_init = pc.instance;
					fc_desc.vft_init_seq = &vft_init_seq;
					fc_desc.init_callback = &init_callback;
				}
				SafePointer<XL::LFunctionContext> fctx = new XL::LFunctionContext(ctx, pc.function, fc_desc);
				VObservationDesc desc;
				desc.current_namespace = fctx->GetRootScope();
				desc.namespace_search_list << fctx->GetRootScope();
				if (fc_desc.instance) desc.namespace_search_list << fctx->GetInstance();
				desc.namespace_search_list << pc.visibility.namespace_search_list;
				ProcessBlock(*fctx, desc);
				fctx->EndEncoding();
				if (pc.constructor) for (auto & s : vft_init_seq) RegisterInitHandler(InitPriorityVFT, &s, 0);
			}
			void Process(void)
			{
				try {
					EnablePrototypes(this);
					ReadNextToken();
					VObservationDesc desc;
					desc.current_namespace = ctx.GetRootNamespace();
					desc.namespace_search_list << desc.current_namespace;
					ProcessNamespace(desc);
					if (current_token.type != TokenType::EOF) Abort(CompilerStatus::AnotherTokenExpected, current_token);
					for (auto & pc : post_compile) ProcessCode(pc);
					post_compile.Clear();
					bool needs_init = false;
					bool needs_final = false;
					for (auto & d : init_list) {
						if (d.init_expr) needs_init = true;
						if (d.shwn_expr) needs_final = true;
					}
					if (needs_init) {
						auto func = ctx.CreatePrivateFunction(XL::FunctionInitializer | XL::FunctionThrows);
						Array<XL::LObject *> perform(0x100), revert(0x100);
						for (auto & d : init_list) if (d.init_expr) {
							perform << d.init_expr.Inner();
							revert << d.shwn_expr.Inner();
						}
						XL::LFunctionContext fctx(ctx, func, XL::FunctionInitializer | XL::FunctionThrows, perform, revert);
					}
					if (needs_final) {
						auto func = ctx.CreatePrivateFunction(XL::FunctionFinalizer);
						Array<XL::LObject *> perform(0x100), revert(0x100);
						for (auto & d : init_list.InversedElements()) if (d.shwn_expr) {
							perform << d.shwn_expr.Inner();
							revert << 0;
						}
						XL::LFunctionContext fctx(ctx, func, XL::FunctionFinalizer, perform, revert);
					}
					init_list.Clear();
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
				if (res_pc) _res.Append(res_pv, res_pc);
				if (mdl_pc) _mdl.Append(mdl_pv, mdl_pc);
			}
			virtual ~ListCompilerCallback(void) override {}
			virtual Streaming::Stream * QueryModuleFileStream(const string & module_name) override
			{
				for (auto & path : _mdl) try {
					return new Streaming::FileStream(path + L"/" + module_name + L"." + XI::FileExtensionLibrary, Streaming::AccessRead, Streaming::OpenExisting);
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
					if (vctx.module_is_library) *output = new OutputModule(module_name, XI::FileExtensionLibrary, data);
					else *output = new OutputModule(module_name, XI::FileExtensionExecutable, data);
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