#include "xv_proto.h"

#include "xv_meta.h"
#include "../xlang/xl_base.h"
#include "../xlang/xl_com.h"
#include "../xlang/xl_types.h"
#include "../xlang/xl_var.h"
#include "../xlang/xl_func.h"

namespace Engine
{
	namespace XV
	{
		constexpr const widechar * TraitSameTypes			= L"idem";
		constexpr const widechar * TraitIsClass				= L"est_cls";
		constexpr const widechar * TraitIsReference			= L"est_com";
		constexpr const widechar * TraitIsPointer			= L"est_adl";
		constexpr const widechar * TraitIsFunctionPointer	= L"est_fadl";
		constexpr const widechar * TraitIsStaticArray		= L"est_ordo";
		constexpr const widechar * TraitVolumeOfType		= L"destinatio";
		constexpr const widechar * TraitElementOfType		= L"res";
		constexpr const widechar * TraitNameOfClass			= L"nomen";
		constexpr const widechar * TraitParentOfClass		= L"parens";
		constexpr const widechar * TraitTypeConformsTo		= L"congruo";
		constexpr const widechar * TraitHasElement			= L"habet";
		constexpr const widechar * TraitIsType				= L"est_genus";
		constexpr const widechar * TraitIsPrototype			= L"est_praeforma";
		constexpr const widechar * TraitIsField				= L"est_valor";
		constexpr const widechar * TraitIsProperty			= L"est_possessio";
		constexpr const widechar * TraitIsFunction			= L"est_functio";
		constexpr const widechar * TraitIsLiteral			= L"est_const";
		constexpr const widechar * TraitIsConvertible		= L"convertitur";
		constexpr const widechar * TraitHaveConstructor		= L"habet_structor";
		constexpr const widechar * TraitHaveOverload		= L"habet_vers";
		constexpr const widechar * TraitExtractOverload		= L"para_vers";
		constexpr const widechar * TraitIsClassFunction		= L"functio_est_classis";
		constexpr const widechar * TraitFunctionThrows		= L"functio_iacit";
		constexpr const widechar * TraitConstructorThrows	= L"structor_iacit";

		XL::LObject * CreatePartialInstantiation(XL::LObject * proto, int argc, XL::LObject ** argv);

		enum class PrototypeClass { Type, Function };
		class VPrototype : public XL::XObject
		{
		public:
			virtual PrototypeClass GetPrototypeClass(void) = 0;
			virtual int GetPrototypeArgumentCount(void) = 0;
			virtual string GetPrototypeArgument(int index) = 0;
			virtual XL::LObject * Instantiate(int argc, XL::LObject ** argv) = 0;
			virtual void SetArgumentList(int argc, const string * argv) = 0;
			virtual void SetVisibilityList(int argc, XL::LObject ** argv) = 0;
			virtual void SetImplementation(ITokenStream * impl) = 0;
		};
		class OperatorInstantiate : public XL::XObject
		{
			SafePointer<VPrototype> _proto;
		public:
			OperatorInstantiate(VPrototype * proto) { _proto.SetRetain(proto); }
			virtual ~OperatorInstantiate(void) override {}
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual XL::Class GetClass(void) override { return XL::Class::Internal; }
			virtual LObject * GetType(void) override { throw XL::ObjectHasNoTypeException(this); }
			virtual LObject * GetMember(const string & name) override { throw XL::ObjectHasNoSuchMemberException(this, name); }
			virtual LObject * Invoke(int argc, LObject ** argv) override { return _proto->Instantiate(argc, argv); }
			virtual void AddMember(const string & name, LObject * child) override { throw XL::LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw XL::ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw XL::ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, XL::Class parent) override {}
		};
		class BasePrototype : public VPrototype
		{
		protected:
			ICompilationContext & _ctx;
			string _name, _path;
			bool _local;
			SafePointer<DataBlock> _impl;
			Array<string> _args, _vl;
			Volumes::Dictionary<string, string> _attributes;

			struct _processor_context
			{
				XL::LObject ** vns;
				int num_vns;
				Volumes::Dictionary<string, Token> subst;
			};

			static void _stream_regular(DataBlock * into, ITokenStream * from, Token & current, bool no_eof)
			{
				while (current.type != TokenType::PrototypeCommand) {
					if (into && (!no_eof || current.type != TokenType::EOF)) SerializeToken(*into, current);
					if (current.type == TokenType::EOF) return;
					if (!from->ReadToken(current)) throw Exception();
				}
			}
			static void _stream_block(DataBlock * into, ITokenStream * from, Token & current, ICompilationContext & ctx, const _processor_context & pc, bool no_eof)
			{
				while (true) {
					if (current.type == TokenType::PrototypeCommand) {
						if (!_handle_command(into, from, current, ctx, pc)) break;
					} else _stream_regular(into, from, current, no_eof);
					if (current.type == TokenType::EOF) break;
				}
			}
			static void _skip_command(DataBlock * into, ITokenStream * from, Token & current)
			{
				while (true) {
					if (current.type == TokenType::EOF) throw Exception();
					auto exit = current.type == TokenType::PrototypeCommand;
					if (!from->ReadToken(current)) throw Exception();
					if (exit) break;
				}
			}
			static bool _handle_command(DataBlock * into, ITokenStream * from, Token & current, ICompilationContext & ctx, const _processor_context & pc)
			{
				if (!from->ReadToken(current)) throw Exception();
				if (current.contents == L"cense") {
					_skip_command(into, from, current);
				} else if (current.contents == L"si") {
					bool skip;
					if (into) {
						if (!from->ReadToken(current)) throw Exception();
						SafePointer<XL::LObject> cond = ctx.ProcessLanguageExpression(from, current, pc.vns, pc.num_vns);
						if (!cond) {
							XI::Module::Literal lit;
							lit.contents = XI::Module::Literal::Class::Boolean;
							lit.length = 1;
							lit.data_uint64 = 0;
							cond = XL::CreateLiteral(*ctx.GetLanguageContext(), lit);
							_skip_command(into, from, current);
						} else {
							if (current.type != TokenType::PrototypeCommand) throw Exception();
							if (!from->ReadToken(current)) throw Exception();
						}
						if (cond->GetClass() != XL::Class::Literal) throw Exception();
						auto & lit = static_cast<XL::XLiteral *>(cond.Inner())->Expose();
						if (lit.contents != XI::Module::Literal::Class::Boolean) throw Exception();
						if (lit.length != 1) throw Exception();
						skip = !lit.data_boolean;
					} else {
						_skip_command(into, from, current);
						skip = true;
					}
					_stream_block(skip ? 0 : into, from, current, ctx, pc, false);
				} else if (current.contents == L"replica") {
					if (into) {
						if (!from->ReadToken(current)) throw Exception();
						string ident = current.contents;
						if (!from->ReadToken(current)) throw Exception();
						SafePointer<XL::LObject> enm = ctx.ProcessLanguageExpression(from, current, pc.vns, pc.num_vns);
						if (current.type != TokenType::PrototypeCommand) throw Exception();
						if (!from->ReadToken(current)) throw Exception();
						SafePointer<DataBlock> defer = new DataBlock(0x1000);
						_stream_block(defer, from, current, ctx, pc, false); defer->Append(0);
						if (enm->GetClass() != XL::Class::Type) throw Exception();
						if (static_cast<XL::XType *>(enm.Inner())->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Class) {
							auto xcls = static_cast<XL::XClass *>(enm.Inner());
							ObjectArray<XL::LObject> fields(0x10);
							xcls->ListFields(fields);
							for (auto & f : fields) {
								SafePointer<ITokenStream> defer_stream = new RestorationStream(defer);
								Token defer_token;
								if (!defer_stream->ReadToken(defer_token)) throw Exception();
								_processor_context spc = pc;
								Token et;
								et.range_from = et.range_length = et.ex_data = 0;
								et.contents = f.GetName();
								et.type = TokenType::Identifier;
								et.contents_i = 0;
								et.contents_f = 0.0;
								spc.subst.Append(ident, et);
								_stream_block(into, defer_stream, defer_token, ctx, spc, true);
							}
						} else if (static_cast<XL::XType *>(enm.Inner())->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Array) {
							auto xarr = static_cast<XL::XArray *>(enm.Inner());
							for (int i = 0; i < xarr->GetVolume(); i++) {
								SafePointer<ITokenStream> defer_stream = new RestorationStream(defer);
								Token defer_token;
								if (!defer_stream->ReadToken(defer_token)) throw Exception();
								_processor_context spc = pc;
								Token et;
								et.range_from = et.range_length = 0;
								et.ex_data = TokenLiteralInteger;
								et.type = TokenType::Literal;
								et.contents_i = i;
								et.contents_f = 0.0;
								spc.subst.Append(ident, et);
								_stream_block(into, defer_stream, defer_token, ctx, spc, true);
							}
						} else throw Exception();
					} else {
						_skip_command(into, from, current);
						_stream_block(0, from, current, ctx, pc, false);
					}
				} else if (current.contents == L"falle") {
					_skip_command(into, from, current);
					if (into) throw Exception();
				} else if (current.contents == L"fini") {
					_skip_command(into, from, current);
					return false;
				} else if (current.contents == L"loca") {
					if (into) {
						Token tc = current;
						if (!from->ReadToken(current)) throw Exception();
						auto token = pc.subst[current.contents];
						if (token) {
							if (!from->ReadToken(current)) throw Exception();
							if (current.type != TokenType::PrototypeCommand) throw Exception();
							if (!from->ReadToken(current)) throw Exception();
							SerializeToken(*into, *token);
						} else {
							Token t;
							t.range_length = t.range_from = t.range_length = 0;
							t.type = TokenType::PrototypeCommand;
							t.contents_i = 0;
							t.contents_f = 0.0;
							SerializeToken(*into, t); SerializeToken(*into, tc); SerializeToken(*into, current);
							while (true) {
								if (!from->ReadToken(current)) throw Exception();
								SerializeToken(*into, current);
								if (current.type == TokenType::EOF || current.type == TokenType::PrototypeCommand) break;
							}
							if (!from->ReadToken(current)) throw Exception();
						}
					} else _skip_command(into, from, current);
				} else throw Exception();
				return true;
			}
			static ITokenStream * _perform_substitution(ITokenStream * input, const Volumes::Dictionary<string, string> & subst_table)
			{
				SafePointer<DataBlock> conv = new DataBlock(0x1000);
				while (true) {
					Token token;
					if (!input->ReadToken(token)) throw InvalidFormatException();
					if (token.type == TokenType::PrototypeSymbol) {
						if (!input->ReadToken(token)) throw InvalidFormatException();
						Token subst;
						subst.range_from = subst.range_length = subst.ex_data = 0;
						subst.contents_i = 0;
						subst.contents_f = 0.0;
						subst.type = TokenType::Identifier;
						if (token.type == TokenType::Keyword || token.type == TokenType::Identifier) {
							subst.contents = token.contents;
						} else if (token.type == TokenType::Literal && token.ex_data == TokenLiteralLogicalY) {
							subst.contents = Lexic::LiteralYes;
						} else if (token.type == TokenType::Literal && token.ex_data == TokenLiteralLogicalN) {
							subst.contents = Lexic::LiteralNo;
						} else throw InvalidFormatException();
						auto sv = subst_table[subst.contents];
						if (!sv) throw InvalidFormatException();
						subst.contents = *sv;
						SerializeToken(*conv, subst);
					} else if (token.type == TokenType::EOF) {
						SerializeToken(*conv, token);
						break;
					} else SerializeToken(*conv, token);
				}
				return new RestorationStream(conv);
			}
			static ITokenStream * _perform_processing(ITokenStream * input, ICompilationContext & ctx, XL::LObject ** vns, int num_vns)
			{
				SafePointer<DataBlock> conv = new DataBlock(0x1000);
				Token token;
				if (!input->ReadToken(token)) throw Exception();
				while (true) {
					if (token.type == TokenType::PrototypeCommand) {
						_processor_context pc;
						pc.vns = vns;
						pc.num_vns = num_vns;
						if (!_handle_command(conv, input, token, ctx, pc)) break;
					} else _stream_regular(conv, input, token, false);
					if (token.type == TokenType::EOF) break;
				}
				return new RestorationStream(conv);
			}
			XL::LObject * _deduce_argument_type(int index, int argc, LObject ** argv)
			{
				SafePointer<XL::LObject> result;
				SafePointer<XL::LObject> subst_scope = _ctx.GetLanguageContext()->QueryScope();
				ObjectArray<Object> retain(0x10);
				for (int i = 0; i < argc; i++) subst_scope->AddMember(L"_" + string(i), argv[i]);
				Array<XL::LObject *> view_list(0x20);
				view_list.Append(subst_scope);
				for (auto & v : _vl) {
					if (v.Length()) {
						SafePointer<XL::LObject> object = _ctx.GetLanguageContext()->QueryObject(v);
						retain.Append(object);
						view_list.Append(object.Inner());
					} else view_list << _ctx.GetLanguageContext()->GetRootNamespace();
				}
				Token token;
				SafePointer<ITokenStream> input = new RestorationStream(_impl);
				if (!input->ReadToken(token)) throw Exception();
				while (true) {
					if (token.type == TokenType::PrototypeCommand) {
						if (!input->ReadToken(token)) throw Exception();
						if (token.contents == L"cense") {
							if (!input->ReadToken(token)) throw Exception();
							bool found = false;
							for (int i = 0; i < _args.Length(); i++) if (_args[i] == token.contents) {
								found = i == index;
								break;
							}
							if (found) {
								if (!input->ReadToken(token)) throw Exception();
								SafePointer<XL::LObject> type = _ctx.ProcessLanguageExpression(input, token, view_list.GetBuffer(), view_list.Length());
								if (type->GetClass() != XL::Class::Type) throw Exception();
								result = type;
								if (token.type != TokenType::PrototypeCommand) throw Exception();
								break;
							} else _skip_command(0, input, token);
						} else _skip_command(0, input, token);
					} else _stream_regular(0, input, token, false);
					if (token.type == TokenType::EOF) break;
				}
				if (!result) throw Exception();
				result->Retain();
				return result;
			}
		public:
			BasePrototype(ICompilationContext & ctx, const string & name, const string & path, bool local) : _ctx(ctx), _name(name), _path(path), _local(local), _args(0x10), _vl(0x10) {}
			BasePrototype(ICompilationContext & ctx, const string & name, const string & path, bool local, const DataBlock & data) : _ctx(ctx), _name(name), _path(path), _local(local), _args(0x10), _vl(0x10)
			{
				_impl = new DataBlock(data.Length());
				SafePointer<DataBlock> data_copy = new DataBlock(data);
				SafePointer<ITokenStream> stream = new RestorationStream(data_copy, 1);
				while (true) {
					Token token;
					if (!stream->ReadToken(token)) throw InvalidFormatException();
					if (token.type == TokenType::Identifier) _args << token.contents;
					else if (token.type == TokenType::EOF) break;
					else throw InvalidFormatException();
				}
				while (true) {
					Token token;
					if (!stream->ReadToken(token)) throw InvalidFormatException();
					if (token.type == TokenType::Identifier) _vl << token.contents;
					else if (token.type == TokenType::EOF) break;
					else throw InvalidFormatException();
				}
				while (true) {
					Token token;
					if (!stream->ReadToken(token)) throw InvalidFormatException();
					SerializeToken(*_impl, token);
					if (token.type == TokenType::EOF) break;
				}
			}
			virtual ~BasePrototype(void) override {}
			// Common implementations
			virtual string GetName(void) override { return _name; }
			virtual string GetFullName(void) override { return _path; }
			virtual bool IsDefinedLocally(void) override { return _local; }
			virtual XL::Class GetClass(void) override { return XL::Class::Prototype; }
			virtual LObject * GetType(void) override { throw XL::ObjectHasNoTypeException(this); }
			virtual LObject * GetMember(const string & name) override
			{
				if (name == XL::OperatorSubscript) return new OperatorInstantiate(this);
				else throw XL::ObjectHasNoSuchMemberException(this, name);
			}
			virtual LObject * Invoke(int argc, LObject ** argv) override { throw XL::ObjectHasNoSuchOverloadException(this, argc, argv); }
			virtual void AddMember(const string & name, LObject * child) override { throw XL::LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { if (!_attributes.Append(key, value)) throw XL::ObjectMemberRedefinitionException(this, key); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw XL::ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, XL::Class parent) override
			{
				if (!_local) return;
				XI::Module::Prototype proto;
				proto.target_language = Meta::Stamp;
				proto.attributes = _attributes;
				proto.data = new DataBlock(0x1000);
				if (GetPrototypeClass() == PrototypeClass::Type) proto.data->Append(0);
				else if (GetPrototypeClass() == PrototypeClass::Function) proto.data->Append(1);
				else return;
				for (auto & a : _args) {
					Token token;
					token.type = TokenType::Identifier;
					token.contents = a;
					SerializeToken(*proto.data, token);
				}
				proto.data->Append(0);
				for (auto & v : _vl) {
					Token token;
					token.type = TokenType::Identifier;
					token.contents = v;
					SerializeToken(*proto.data, token);
				}
				proto.data->Append(0);
				if (_impl) proto.data->Append(*_impl); else proto.data->Append(0);
				dest.prototypes.Append(GetFullName(), proto);
			}
			// Extended interface
			virtual PrototypeClass GetPrototypeClass(void) override { return PrototypeClass::Type; }
			virtual int GetPrototypeArgumentCount(void) override { return _args.Length(); }
			virtual string GetPrototypeArgument(int index) override { return _args[index]; }
			virtual XL::LObject * Instantiate(int argc, XL::LObject ** argv) override
			{
				try {
					if (argc < _args.Length() && GetPrototypeClass() == PrototypeClass::Function) return CreatePartialInstantiation(this, argc, argv);
					if (argc != _args.Length()) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					string inst_name = L"@praeformae." + _path + L"@cum";
					for (int i = 0; i < argc; i++) {
						if (argv[i]->GetClass() == XL::Class::Type) {
							inst_name += L"@" + static_cast<XL::XType *>(argv[i])->GetCanonicalType();
						} else throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					}
					XL::LObject * ns = _ctx.GetLanguageContext()->GetRootNamespace();
					while (true) {
						auto del = inst_name.FindFirst(L'.');
						if (del < 0) break;
						ns = _ctx.GetLanguageContext()->CreateNamespace(ns, inst_name.Fragment(0, del));
						inst_name = inst_name.Fragment(del + 1, -1);
					}
					try { return ns->GetMember(inst_name); } catch (...) {}
					ObjectArray<Object> retain(0x20);
					SafePointer<XL::LObject> subst_scope = _ctx.GetLanguageContext()->QueryScope();
					Array<XL::LObject *> view_list(0x20);
					view_list.Append(subst_scope);
					uint _subst_serial = 0;
					Volumes::Dictionary<string, string> subst;
					for (auto & v : _vl) {
						if (v.Length()) {
							SafePointer<XL::LObject> object = _ctx.GetLanguageContext()->QueryObject(v);
							retain.Append(object);
							view_list.Append(object.Inner());
						} else view_list << _ctx.GetLanguageContext()->GetRootNamespace();
					}
					for (int i = 0; i < _args.Length(); i++) {
						auto ident = L"__@" + string(_subst_serial, HexadecimalBase, 8);
						_subst_serial++;
						subst.Append(_args[i], ident);
						subst_scope->AddMember(ident, argv[i]);
					}
					subst.Append(L"_", inst_name);
					SafePointer<ITokenStream> stream = new RestorationStream(_impl);
					stream = _perform_substitution(stream, subst);
					stream = _perform_processing(stream, _ctx, view_list.GetBuffer(), view_list.Length());
					if (!_ctx.ProcessLanguageDefinitions(stream, ns, view_list.GetBuffer(), view_list.Length())) throw InvalidFormatException();
					return ns->GetMember(inst_name);
				} catch (...) { throw XL::ObjectHasNoSuchOverloadException(this, argc, argv); }
			}
			virtual void SetArgumentList(int argc, const string * argv) override { _args.Clear(); _args.Append(argv, argc); }
			virtual void SetVisibilityList(int argc, XL::LObject ** argv) override { _vl.Clear(); for (int i = 0; i < argc; i++) _vl << argv[i]->GetFullName(); }
			virtual void SetImplementation(ITokenStream * impl) override { _impl = impl->Serialize(); }
		};
		class FunctionPrototype : public BasePrototype
		{
		public:
			FunctionPrototype(ICompilationContext & ctx, const string & name, const string & path, bool local) : BasePrototype(ctx, name, path, local) {}
			FunctionPrototype(ICompilationContext & ctx, const string & name, const string & path, bool local, const DataBlock & data) : BasePrototype(ctx, name, path, local, data) {}
			virtual ~FunctionPrototype(void) override {}
			virtual LObject * Invoke(int argc, LObject ** argv) override { return PartialInvoke(0, 0, argc, argv); }
			virtual PrototypeClass GetPrototypeClass(void) { return PrototypeClass::Function; }
			virtual LObject * PartialInvoke(int inst_argc, LObject ** inst_argv, int argc, LObject ** argv)
			{
				ObjectArray<Object> retain(_args.Length());
				Array<XL::LObject *> inst_args(_args.Length());
				for (int i = 0; i < inst_argc; i++) inst_args << inst_argv[i];
				while (inst_args.Length() < _args.Length()) {
					int index = inst_args.Length();
					SafePointer<XL::LObject> arg = _deduce_argument_type(index, argc, argv);
					retain.Append(arg);
					inst_args << arg;
				}
				SafePointer<XL::LObject> instance = Instantiate(inst_args.Length(), inst_args.GetBuffer());
				return instance->Invoke(argc, argv);
			}
		};
		class PartialPrototype : public XL::XObject
		{
			ObjectArray<Object> _retain;
			SafePointer<FunctionPrototype> _proto;
			int _argc;
			XL::LObject ** _argv;
		public:
			PartialPrototype(FunctionPrototype * proto, int argc, XL::LObject ** argv) : _retain(argc), _argc(argc), _argv(argv)
			{
				for (int i = 0; i < argc; i++) _retain.Append(argv[i]);
				_proto.SetRetain(proto);
			}
			virtual ~PartialPrototype(void) override {}
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual XL::Class GetClass(void) override { return XL::Class::Internal; }
			virtual LObject * GetType(void) override { throw XL::ObjectHasNoTypeException(this); }
			virtual LObject * GetMember(const string & name) override { throw XL::ObjectHasNoSuchMemberException(this, name); }
			virtual LObject * Invoke(int argc, LObject ** argv) override { return _proto->PartialInvoke(_argc, _argv, argc, argv); }
			virtual void AddMember(const string & name, LObject * child) override { throw XL::LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw XL::ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw XL::ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, XL::Class parent) override {}
		};
		class PrototypeHandler : public XL::LPrototypeHandler
		{
			ICompilationContext & _ctx;
		public:
			PrototypeHandler(ICompilationContext * ctx) : _ctx(*ctx) {}
			virtual ~PrototypeHandler(void) override {}
			virtual void HandlePrototype(const string & ident, const string & lang_spec, const DataBlock & data, const Volumes::Dictionary<string, string> & attributes) override
			{
				if (lang_spec != Meta::Stamp || !data.Length()) return;
				SafePointer<BasePrototype> proto;
				auto del = ident.FindLast(L'.');
				if (data[0] == 0) proto = new BasePrototype(_ctx, ident.Fragment(del + 1, -1), ident, false, data);
				else if (data[0] == 1) proto = new FunctionPrototype(_ctx, ident.Fragment(del + 1, -1), ident, false, data);
				_ctx.GetLanguageContext()->InstallObject(proto, ident);
			}
		};
		class BuiltInOperator : public XL::XObject
		{
			XL::LContext & _ctx;
			string _name, _path;

			XL::LObject * _create_literal(XL::LContext & ctx, bool value)
			{
				XI::Module::Literal lit;
				lit.contents = XI::Module::Literal::Class::Boolean;
				lit.length = 1;
				lit.data_uint64 = value;
				return XL::CreateLiteral(ctx, lit);
			}
			XL::LObject * _create_literal(XL::LContext & ctx, int value)
			{
				XI::Module::Literal lit;
				lit.contents = XI::Module::Literal::Class::SignedInteger;
				lit.length = 4;
				lit.data_sint64 = value;
				return XL::CreateLiteral(ctx, lit);
			}
			XL::LObject * _create_literal(XL::LContext & ctx, const string & value)
			{
				XI::Module::Literal lit;
				lit.contents = XI::Module::Literal::Class::String;
				lit.length = 0;
				lit.data_string = value;
				return XL::CreateLiteral(ctx, lit);
			}
		public:
			BuiltInOperator(XL::LContext & ctx, const string & name, const string & path) : _ctx(ctx), _name(name), _path(path) {}
			virtual ~BuiltInOperator(void) override {}
			virtual string GetName(void) override { return _name; }
			virtual string GetFullName(void) override { return _path; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual XL::Class GetClass(void) override { return XL::Class::Internal; }
			virtual LObject * GetType(void) override { throw XL::ObjectHasNoTypeException(this); }
			virtual LObject * GetMember(const string & name) override { throw XL::ObjectHasNoSuchMemberException(this, name); }
			virtual LObject * Invoke(int argc, LObject ** argv) override
			{
				if (_name == TraitSameTypes) {
					if (argc != 2) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					if (argv[0]->GetClass() != XL::Class::Type || argv[1]->GetClass() != XL::Class::Type) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					return _create_literal(static_cast<XL::XType *>(argv[0])->GetContext(),
						static_cast<XL::XType *>(argv[0])->GetCanonicalType() == static_cast<XL::XType *>(argv[1])->GetCanonicalType());
				} else if (_name == TraitIsClass || _name == TraitIsReference || _name == TraitIsPointer || _name == TraitIsFunctionPointer || _name == TraitIsStaticArray) {
					if (argc != 1) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					if (argv[0]->GetClass() != XL::Class::Type) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					bool result = false;
					if (_name == TraitIsClass) result = static_cast<XL::XType *>(argv[0])->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Class;
					else if (_name == TraitIsReference) result = static_cast<XL::XType *>(argv[0])->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Reference;
					else if (_name == TraitIsStaticArray) result = static_cast<XL::XType *>(argv[0])->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Array;
					else if (_name == TraitIsPointer) {
						if (static_cast<XL::XType *>(argv[0])->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Pointer) {
							SafePointer<XL::XType> element = static_cast<XL::XPointer *>(argv[0])->GetElementType();
							result = element->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Function;
						}
					} else if (_name == TraitIsFunctionPointer) {
						if (static_cast<XL::XType *>(argv[0])->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Pointer) {
							SafePointer<XL::XType> element = static_cast<XL::XPointer *>(argv[0])->GetElementType();
							result = element->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Function;
						}
					}
					return _create_literal(static_cast<XL::XType *>(argv[0])->GetContext(), result);
				} else if (_name == TraitVolumeOfType) {
					if (argc != 1) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					if (argv[0]->GetClass() != XL::Class::Type) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					if (static_cast<XL::XType *>(argv[0])->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Array) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					auto xarr = static_cast<XL::XArray *>(argv[0]);
					return _create_literal(xarr->GetContext(), xarr->GetVolume());
				} else if (_name == TraitElementOfType) {
					if (argc != 1) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					if (argv[0]->GetClass() != XL::Class::Type) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					if (static_cast<XL::XType *>(argv[0])->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Array) {
						return static_cast<XL::XArray *>(argv[0])->GetElementType();
					} else if (static_cast<XL::XType *>(argv[0])->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Reference) {
						return static_cast<XL::XReference *>(argv[0])->GetElementType();
					} else if (static_cast<XL::XType *>(argv[0])->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Pointer) {
						SafePointer<XL::XType> element = static_cast<XL::XPointer *>(argv[0])->GetElementType();
						if (element->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Function) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
						element->Retain();
						return element;
					} else throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
				} else if (_name == TraitNameOfClass) {
					if (argc != 1) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					if (argv[0]->GetClass() == XL::Class::Type) {
						if (static_cast<XL::XType *>(argv[0])->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
						auto cls = static_cast<XL::XClass *>(argv[0]);
						return _create_literal(cls->GetContext(), cls->GetFullName());
					} else if (argv[0]->GetClass() == XL::Class::Field) {
						return _create_literal(_ctx, argv[0]->GetName());
					} else throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
				} else if (_name == TraitParentOfClass) {
					if (argc != 1) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					if (argv[0]->GetClass() != XL::Class::Type) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					if (static_cast<XL::XType *>(argv[0])->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					auto cls = static_cast<XL::XClass *>(argv[0]);
					auto parent = cls->GetParentClass();
					if (parent) { parent->Retain(); return parent; }
					throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
				} else if (_name == TraitTypeConformsTo) {
					if (argc != 2) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					if (argv[0]->GetClass() != XL::Class::Type || argv[1]->GetClass() != XL::Class::Type) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					ObjectArray<XL::XType> conf(0x10);
					static_cast<XL::XType *>(argv[0])->GetTypesConformsTo(conf);
					for (auto & c : conf) if (c.GetCanonicalType() == static_cast<XL::XType *>(argv[1])->GetCanonicalType()) {
						return _create_literal(static_cast<XL::XType *>(argv[0])->GetContext(), true);
					}
					return _create_literal(static_cast<XL::XType *>(argv[0])->GetContext(), false);
				} else if (_name == TraitHasElement) {
					if (argc != 2) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					if (argv[1]->GetClass() != XL::Class::Literal) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					auto & lit = static_cast<XL::XLiteral *>(argv[1])->Expose();
					if (lit.contents != XI::Module::Literal::Class::String) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					try {
						SafePointer<XL::LObject> obj = argv[0]->GetMember(lit.data_string);
						return _create_literal(_ctx, true);
					} catch (...) {}
					return _create_literal(_ctx, false);
				} else if (_name == TraitIsType || _name == TraitIsPrototype || _name == TraitIsField || _name == TraitIsProperty || _name == TraitIsFunction || _name == TraitIsLiteral) {
					if (argc != 1) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					if (_name == TraitIsType) return _create_literal(_ctx, argv[0]->GetClass() == XL::Class::Type);
					else if (_name == TraitIsPrototype) return _create_literal(_ctx, argv[0]->GetClass() == XL::Class::Prototype);
					else if (_name == TraitIsField) return _create_literal(_ctx, argv[0]->GetClass() == XL::Class::Field);
					else if (_name == TraitIsProperty) return _create_literal(_ctx, argv[0]->GetClass() == XL::Class::Property);
					else if (_name == TraitIsFunction) return _create_literal(_ctx, argv[0]->GetClass() == XL::Class::Function || argv[0]->GetClass() == XL::Class::FunctionOverload);
					else if (_name == TraitIsLiteral) return _create_literal(_ctx, argv[0]->GetClass() == XL::Class::Literal);
					else return _create_literal(_ctx, false);
				} else if (_name == TraitIsConvertible) {
					if (argc != 2) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					if (argv[0]->GetClass() != XL::Class::Type || argv[1]->GetClass() != XL::Class::Type) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					auto type_from = static_cast<XL::XType *>(argv[0]);
					auto type_to = static_cast<XL::XType *>(argv[1]);
					auto cast_level = XL::CheckCastPossibility(type_to, type_from, XL::CastPriorityConverter);
					return _create_literal(static_cast<XL::XType *>(argv[0])->GetContext(), cast_level >= 0);
				} else if (_name == TraitHaveConstructor || _name == TraitConstructorThrows) {
					if (argc < 1) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					for (int i = 0; i < argc; i++) if (argv[i]->GetClass() != XL::Class::Type) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					auto type = static_cast<XL::XType *>(argv[0]);
					SafePointer<XL::LObject> ctor;
					try {
						if (argc == 1) {
							ctor = type->GetConstructorInit();
						} else if (argc == 2) {
							auto in = static_cast<XL::XType *>(argv[1]);
							if (in->GetCanonicalType() == type->GetCanonicalType()) ctor = type->GetConstructorCopy();
							else if (in->GetCanonicalType() == XI::Module::TypeReference::MakeReference(type->GetCanonicalType())) ctor = type->GetConstructorCopy(); else {
								SafePointer<XL::LObject> ctor_fd = type->GetMember(XL::NameConstructor);
								if (ctor_fd->GetClass() == XL::Class::Function) {
									Array<string> names(0x10);
									for (int i = 1; i < argc; i++) names << static_cast<XL::XType *>(argv[i])->GetCanonicalType();
									auto ctor_ver = static_cast<XL::XFunction *>(ctor_fd.Inner())->GetOverloadT(argc - 1, names, true);
									ctor.SetRetain(ctor_ver);
								}
							}
						} else {
							SafePointer<XL::LObject> ctor_fd = type->GetMember(XL::NameConstructor);
							if (ctor_fd->GetClass() == XL::Class::Function) {
								Array<string> names(0x10);
								for (int i = 1; i < argc; i++) names << static_cast<XL::XType *>(argv[i])->GetCanonicalType();
								auto ctor_ver = static_cast<XL::XFunction *>(ctor_fd.Inner())->GetOverloadT(argc - 1, names, true);
								ctor.SetRetain(ctor_ver);
							}
						}
					} catch (...) { ctor.SetReference(0); }
					if (_name == TraitHaveConstructor) {
						return _create_literal(_ctx, bool(ctor));
					} else if (_name == TraitConstructorThrows) {
						if (!ctor) return _create_literal(_ctx, false);
						return _create_literal(_ctx, static_cast<XL::XFunctionOverload *>(ctor.Inner())->Throws());
					}
				} else if (_name == TraitHaveOverload || _name == TraitExtractOverload) {
					if (argc < 1) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					for (int i = 1; i < argc; i++) if (argv[i]->GetClass() != XL::Class::Type) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					if (argv[0]->GetClass() != XL::Class::Function) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					Array<string> names(0x10);
					for (int i = 1; i < argc; i++) names << static_cast<XL::XType *>(argv[i])->GetCanonicalType();
					auto func_ver = static_cast<XL::XFunction *>(argv[0])->GetOverloadT(argc - 1, names, true);
					if (_name == TraitHaveOverload) {
						return _create_literal(_ctx, func_ver != 0);
					} else if (_name == TraitExtractOverload) {
						if (!func_ver) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
						func_ver->Retain(); return func_ver;
					}
				} else if (_name == TraitIsClassFunction) {
					if (argc != 1) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					if (argv[0]->GetClass() != XL::Class::FunctionOverload) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					return _create_literal(_ctx, !static_cast<XL::XFunctionOverload *>(argv[0])->NeedsInstance());
				} else if (_name == TraitFunctionThrows) {
					if (argc != 1) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					if (argv[0]->GetClass() != XL::Class::FunctionOverload) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					return _create_literal(_ctx, static_cast<XL::XFunctionOverload *>(argv[0])->Throws());
				} else throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
			}
			virtual void AddMember(const string & name, LObject * child) override { throw XL::LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw XL::ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw XL::ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, XL::Class parent) override {}
			static void Create(XL::LContext & ctx, XL::LObject * ns, const string & name)
			{
				SafePointer<XL::LObject> self = new BuiltInOperator(ctx, name, ns->GetName() + L"." + name);
				ns->AddMember(name, self);
			}
		};

		XL::LObject * CreatePartialInstantiation(XL::LObject * proto, int argc, XL::LObject ** argv) { return new PartialPrototype(static_cast<FunctionPrototype *>(proto), argc, argv); }

		void EnablePrototypes(ICompilationContext * on_context)
		{
			SafePointer<PrototypeHandler> hdlr = new PrototypeHandler(on_context);
			on_context->GetLanguageContext()->SetPrototypeHandler(hdlr);
			auto trait_ns = on_context->GetLanguageContext()->CreateNamespace(on_context->GetLanguageContext()->GetRootNamespace(), L"tractus");
			auto & ctx = *on_context->GetLanguageContext();
			BuiltInOperator::Create(ctx, trait_ns, TraitSameTypes);
			BuiltInOperator::Create(ctx, trait_ns, TraitIsClass);
			BuiltInOperator::Create(ctx, trait_ns, TraitIsReference);
			BuiltInOperator::Create(ctx, trait_ns, TraitIsPointer);
			BuiltInOperator::Create(ctx, trait_ns, TraitIsFunctionPointer);
			BuiltInOperator::Create(ctx, trait_ns, TraitIsStaticArray);
			BuiltInOperator::Create(ctx, trait_ns, TraitVolumeOfType);
			BuiltInOperator::Create(ctx, trait_ns, TraitElementOfType);
			BuiltInOperator::Create(ctx, trait_ns, TraitNameOfClass);
			BuiltInOperator::Create(ctx, trait_ns, TraitParentOfClass);
			BuiltInOperator::Create(ctx, trait_ns, TraitTypeConformsTo);
			BuiltInOperator::Create(ctx, trait_ns, TraitHasElement);
			BuiltInOperator::Create(ctx, trait_ns, TraitIsType);
			BuiltInOperator::Create(ctx, trait_ns, TraitIsPrototype);
			BuiltInOperator::Create(ctx, trait_ns, TraitIsField);
			BuiltInOperator::Create(ctx, trait_ns, TraitIsFunction);
			BuiltInOperator::Create(ctx, trait_ns, TraitIsProperty);
			BuiltInOperator::Create(ctx, trait_ns, TraitIsLiteral);
			BuiltInOperator::Create(ctx, trait_ns, TraitIsConvertible);
			BuiltInOperator::Create(ctx, trait_ns, TraitHaveConstructor);
			BuiltInOperator::Create(ctx, trait_ns, TraitHaveOverload);
			BuiltInOperator::Create(ctx, trait_ns, TraitExtractOverload);
			BuiltInOperator::Create(ctx, trait_ns, TraitIsClassFunction);
			BuiltInOperator::Create(ctx, trait_ns, TraitFunctionThrows);
			BuiltInOperator::Create(ctx, trait_ns, TraitConstructorThrows);
		}
		void SupplyPrototypeImplementation(XL::LObject * proto, ITokenStream * impl)
		{
			if (!proto || proto->GetClass() != XL::Class::Prototype) throw InvalidArgumentException();
			static_cast<BasePrototype *>(proto)->SetImplementation(impl);
		}
		void SetPrototypeVisibility(XL::LObject * proto, XL::LObject ** vns, int num_vns)
		{
			if (!proto || proto->GetClass() != XL::Class::Prototype) throw InvalidArgumentException();
			static_cast<BasePrototype *>(proto)->SetVisibilityList(num_vns, vns);
		}
		XL::LObject * CreateClassPrototype(ICompilationContext * context, XL::LObject * at, const string & name, int argc, const string * argv)
		{
			if (!at) throw InvalidArgumentException();
			if (at->GetClass() != XL::Class::Namespace && at->GetClass() != XL::Class::Type) throw InvalidArgumentException();
			auto prefix = at->GetFullName();
			if (prefix.Length()) prefix += L".";
			SafePointer<BasePrototype> proto = new BasePrototype(*context, name, prefix + name, true);
			proto->SetArgumentList(argc, argv);
			at->AddMember(name, proto);
			return proto;
		}
		XL::LObject * CreateFunctionPrototype(ICompilationContext * context, XL::LObject * at, const string & name, int argc, const string * argv)
		{
			if (!at) throw InvalidArgumentException();
			if (at->GetClass() != XL::Class::Namespace && at->GetClass() != XL::Class::Type) throw InvalidArgumentException();
			auto prefix = at->GetFullName();
			if (prefix.Length()) prefix += L".";
			SafePointer<BasePrototype> proto = new FunctionPrototype(*context, name, prefix + name, true);
			proto->SetArgumentList(argc, argv);
			at->AddMember(name, proto);
			return proto;
		}
	}
}