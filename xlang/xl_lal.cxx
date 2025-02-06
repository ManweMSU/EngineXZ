#include "xl_lal.h"

#include "xl_com.h"
#include "xl_base.h"
#include "xl_types.h"
#include "xl_var.h"
#include "xl_func.h"
#include "xl_prop.h"
#include "xl_synth.h"
#include "../ximg/xi_resources.h"
#include "../ximg/xi_function.h"

namespace Engine
{
	namespace XL
	{
		LException::LException(LObject * reason) { source.SetRetain(reason); }
		ObjectHasNoTypeException::ObjectHasNoTypeException(LObject * reason) : LException(reason) {}
		string ObjectHasNoTypeException::ToString(void) const { return L"Object " + source->ToString() + L" has no type property."; }
		ObjectHasNoSuchMemberException::ObjectHasNoSuchMemberException(LObject * reason, const string & member_name) : LException(reason), member(member_name) {}
		string ObjectHasNoSuchMemberException::ToString(void) const { return L"Object " + source->ToString() + L" has no member named \"" + member + L"\"."; }
		ObjectMemberRedefinitionException::ObjectMemberRedefinitionException(LObject * reason, const string & member_name) : LException(reason), member(member_name) {}
		string ObjectMemberRedefinitionException::ToString(void) const { return L"Object " + source->ToString() + L" already has a member named \"" + member + L"\"."; }
		ObjectHasNoSuchOverloadException::ObjectHasNoSuchOverloadException(LObject * reason, int argc, LObject ** argv) : LException(reason), arguments(argc) { for (int i = 0; i < argc; i++) arguments.Append(argv[i]); }
		string ObjectHasNoSuchOverloadException::ToString(void) const { return L"Object " + source->ToString() + L" can not be invoked with arguments " + arguments.ToString() + L"."; }
		ObjectIsNotEvaluatableException::ObjectIsNotEvaluatableException(LObject * reason) : LException(reason) {}
		string ObjectIsNotEvaluatableException::ToString(void) const { return L"Object " + source->ToString() + L" can not be evaluated."; }
		ObjectHasNoAttributesException::ObjectHasNoAttributesException(LObject * reason) : LException(reason) {}
		string ObjectHasNoAttributesException::ToString(void) const { return L"Object " + source->ToString() + L" can not have attributes."; }
		ObjectMayThrow::ObjectMayThrow(LObject * reason) : LException(reason) {}
		string ObjectMayThrow::ToString(void) const { return L"Object " + source->ToString() + L" can throw exceptions."; }
		ObjectHasNoSuitableCast::ObjectHasNoSuitableCast(LObject * reason, LObject * from, LObject * to) : LException(reason) { type_from.SetRetain(from); type_to.SetRetain(to); }
		string ObjectHasNoSuitableCast::ToString(void) const { return L"Object " + source->ToString() + L" cannot be casted from " + type_from->ToString() + L" to " + type_to->ToString() + L"."; }

		class XInternal : public XObject
		{
		public:
			virtual Class GetClass(void) override { return Class::Internal; }
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
			virtual void ListMembers(Volumes::Dictionary<string, Class> & list) override {}
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw LException(this); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
		};
		class XAddressOf : public XInternal
		{
			LContext & _ctx;
			class _computable : public Object, public IComputableProvider
			{
			public:
				XA::ObjectSize _spec;
				SafePointer<XType> _type;
				SafePointer<LObject> _subject;
			public:
				_computable(void) {}
				virtual ~_computable(void) override {}
				virtual Object * ComputableProviderQueryObject(void) override { return this; }
				virtual XType * ComputableGetType(void) override { _type->Retain(); return _type; }
				virtual XA::ExpressionTree ComputableEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
				{
					auto tree = _subject->Evaluate(func, error_ctx);
					return MakeAddressOf(tree, _spec);
				}
			};
		public:
			XAddressOf(LContext & ctx) : _ctx(ctx) {}
			virtual ~XAddressOf(void) override {}
			virtual LObject * GetType(void) override { throw ObjectHasNoTypeException(this); }
			virtual LObject * Invoke(int argc, LObject ** argv, LObject ** actual) override
			{
				if (argc != 1) throw ObjectHasNoSuchOverloadException(this, argc, argv);
				if (actual) { *actual = this; Retain(); }
				SafePointer<_computable> com = new _computable;
				SafePointer<LObject> type = argv[0]->GetType();
				SafePointer<LObject> ptr = _ctx.QueryTypePointer(type);
				if (ptr->GetClass() != Class::Type) throw ObjectHasNoSuchOverloadException(this, argc, argv);
				com->_spec = static_cast<XType *>(type.Inner())->GetArgumentSpecification().size;
				com->_type.SetRetain(static_cast<XType *>(ptr.Inner()));
				com->_subject.SetRetain(argv[0]);
				return CreateComputable(_ctx, com);
			}
			virtual void ListInvokations(LObject * first, Volumes::List<InvokationDesc> & list) override {}
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual string ToString(void) const override { return L"address of"; }
		};
		class XLogicalOr : public XInternal
		{
			LContext & _ctx;
			class _computable : public Object, public IComputableProvider
			{
			public:
				ObjectArray<Object> _retain;
				SafePointer<XType> _boolean;
				Array<LObject *> _argv;
			public:
				_computable(void) : _retain(0x20), _argv(0x20) {}
				virtual ~_computable(void) override {}
				virtual Object * ComputableProviderQueryObject(void) override { return this; }
				virtual XType * ComputableGetType(void) override { _boolean->Retain(); return _boolean; }
				virtual XA::ExpressionTree ComputableEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
				{
					auto tree = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformLogicalOr, XA::ReferenceFlagInvoke));
					for (auto & v : _argv) XA::TH::AddTreeInput(tree, v->Evaluate(func, error_ctx), _boolean->GetArgumentSpecification());
					XA::TH::AddTreeOutput(tree, _boolean->GetArgumentSpecification());
					return tree;
				}
			};
		public:
			XLogicalOr(LContext & ctx) : _ctx(ctx) {}
			virtual ~XLogicalOr(void) override {}
			virtual LObject * GetType(void) override { throw ObjectHasNoTypeException(this); }
			virtual LObject * Invoke(int argc, LObject ** argv, LObject ** actual) override
			{
				if (argc < 2) throw ObjectHasNoSuchOverloadException(this, argc, argv);
				if (actual) { *actual = this; Retain(); }
				SafePointer<_computable> com = new _computable;
				com->_boolean = CreateType(XI::Module::TypeReference::MakeClassReference(NameBoolean), _ctx);
				for (int i = 0; i < argc; i++) {
					SafePointer<LObject> conv = PerformTypeCast(com->_boolean, argv[i], CastPriorityConverter);
					com->_retain.Append(conv); com->_argv.Append(conv);
				}
				return CreateComputable(_ctx, com);
			}
			virtual void ListInvokations(LObject * first, Volumes::List<InvokationDesc> & list) override {}
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual string ToString(void) const override { return L"logical or"; }
		};
		class XLogicalAnd : public XInternal
		{
			LContext & _ctx;
			class _computable : public Object, public IComputableProvider
			{
			public:
				ObjectArray<Object> _retain;
				SafePointer<XType> _boolean;
				Array<LObject *> _argv;
			public:
				_computable(void) : _retain(0x20), _argv(0x20) {}
				virtual ~_computable(void) override {}
				virtual Object * ComputableProviderQueryObject(void) override { return this; }
				virtual XType * ComputableGetType(void) override { _boolean->Retain(); return _boolean; }
				virtual XA::ExpressionTree ComputableEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
				{
					auto tree = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformLogicalAnd, XA::ReferenceFlagInvoke));
					for (auto & v : _argv) XA::TH::AddTreeInput(tree, v->Evaluate(func, error_ctx), _boolean->GetArgumentSpecification());
					XA::TH::AddTreeOutput(tree, _boolean->GetArgumentSpecification());
					return tree;
				}
			};
		public:
			XLogicalAnd(LContext & ctx) : _ctx(ctx) {}
			virtual ~XLogicalAnd(void) override {}
			virtual LObject * GetType(void) override { throw ObjectHasNoTypeException(this); }
			virtual LObject * Invoke(int argc, LObject ** argv, LObject ** actual) override
			{
				if (argc < 2) throw ObjectHasNoSuchOverloadException(this, argc, argv);
				if (actual) { *actual = this; Retain(); }
				SafePointer<_computable> com = new _computable;
				com->_boolean = CreateType(XI::Module::TypeReference::MakeClassReference(NameBoolean), _ctx);
				for (int i = 0; i < argc; i++) {
					SafePointer<LObject> conv = PerformTypeCast(com->_boolean, argv[i], CastPriorityConverter);
					com->_retain.Append(conv); com->_argv.Append(conv);
				}
				return CreateComputable(_ctx, com);
			}
			virtual void ListInvokations(LObject * first, Volumes::List<InvokationDesc> & list) override {}
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual string ToString(void) const override { return L"logical and"; }
		};
		class XTypeOf : public XInternal
		{
		public:
			XTypeOf(void) {}
			virtual ~XTypeOf(void) override {}
			virtual LObject * GetType(void) override { throw ObjectHasNoTypeException(this); }
			virtual LObject * Invoke(int argc, LObject ** argv, LObject ** actual) override
			{
				if (argc != 1) throw ObjectHasNoSuchOverloadException(this, argc, argv);
				if (actual) { *actual = this; Retain(); }
				return argv[0]->GetType();
			}
			virtual void ListInvokations(LObject * first, Volumes::List<InvokationDesc> & list) override
			{
				InvokationDesc result;
				Volumes::KeyValuePair< SafePointer<LObject>, Class > rv(0, Class::Type);
				Volumes::KeyValuePair< SafePointer<LObject>, Class > in(0, Class::Null);
				result.arglist.InsertLast(rv);
				result.arglist.InsertLast(in);
				list.InsertLast(result);
			}
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual string ToString(void) const override { return L"typeof"; }
		};
		class XSizeOf : public XInternal
		{
			LContext & _ctx;
			bool _mx;
			class _computable : public Object, public IComputableProvider
			{
			public:
				SafePointer<XType> _retval;
				XA::ObjectSize _size;
			public:
				_computable(void) {}
				virtual ~_computable(void) override {}
				virtual Object * ComputableProviderQueryObject(void) override { return this; }
				virtual XType * ComputableGetType(void) override { _retval->Retain(); return _retval; }
				virtual XA::ExpressionTree ComputableEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
				{
					auto intptr = XA::TH::MakeSpec(XA::TH::MakeSize(0, 1));
					auto offset = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformAddressOffset, XA::ReferenceFlagInvoke));
					XA::TH::AddTreeInput(offset, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceNull)), intptr);
					XA::TH::AddTreeInput(offset, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLiteral)), XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, _size));
					XA::TH::AddTreeOutput(offset, intptr);
					return MakeAddressOf(offset, intptr.size);
				}
			};
		public:
			XSizeOf(LContext & ctx, bool max_size) : _ctx(ctx), _mx(max_size) {}
			virtual ~XSizeOf(void) override {}
			virtual LObject * GetType(void) override { throw ObjectHasNoTypeException(this); }
			virtual LObject * Invoke(int argc, LObject ** argv, LObject ** actual) override
			{
				if (argc != 1) throw ObjectHasNoSuchOverloadException(this, argc, argv);
				if (actual) { *actual = this; Retain(); }
				SafePointer<LObject> type;
				if (argv[0]->GetClass() == Class::Type) type.SetRetain(argv[0]);
				else type = argv[0]->GetType();
				if (_mx) {
					auto size = _ctx.GetClassInstanceSize(type);
					return _ctx.QueryLiteral(uint64(size.num_bytes + 8 * size.num_words));
				} else {
					SafePointer<_computable> com = new _computable;
					com->_size = _ctx.GetClassInstanceSize(type);
					com->_retval = CreateType(XI::Module::TypeReference::MakeClassReference(NameUIntPtr), _ctx);
					return CreateComputable(_ctx, com);
				}
			}
			virtual void ListInvokations(LObject * first, Volumes::List<InvokationDesc> & list) override
			{
				InvokationDesc result;
				if (_mx) {
					SafePointer<LObject> type = _ctx.QueryObject(NameInt32);
					Volumes::KeyValuePair< SafePointer<LObject>, Class > rv(type, Class::Null);
					result.arglist.InsertLast(rv);
				} else {
					SafePointer<LObject> type = _ctx.QueryObject(NameUIntPtr);
					Volumes::KeyValuePair< SafePointer<LObject>, Class > rv(type, Class::Null);
					result.arglist.InsertLast(rv);
				}
				Volumes::KeyValuePair< SafePointer<LObject>, Class > in(0, Class::Null);
				result.arglist.InsertLast(in);
				list.InsertLast(result);
			}
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual string ToString(void) const override { return L"sizeof"; }
		};
		class ModuleProvider : public Object, public IComputableProvider
		{
			LContext & _ctx;
			string _name;
		public:
			ModuleProvider(LContext & ctx, const string & name) : _ctx(ctx), _name(name) {}
			virtual ~ModuleProvider(void) override {}
			virtual Object * ComputableProviderQueryObject(void) override { return this; }
			virtual XType * ComputableGetType(void) override
			{
				SafePointer<LObject> void_type = _ctx.QueryObject(NameVoid);
				return static_cast<XType *>(_ctx.QueryTypePointer(void_type));
			}
			virtual XA::ExpressionTree ComputableEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
			{
				if (_name.Length()) return MakeAddressOf(MakeReference(func, L"M:" + _name), XA::TH::MakeSize(0, 1));
				else return MakeAddressOf(MakeReference(func, L"M"), XA::TH::MakeSize(0, 1));
			}
		};
		class InterfaceProvider : public Object, public IComputableProvider
		{
			LContext & _ctx;
			string _name;
		public:
			InterfaceProvider(LContext & ctx, const string & name) : _ctx(ctx), _name(name) {}
			virtual ~InterfaceProvider(void) override {}
			virtual Object * ComputableProviderQueryObject(void) override { return this; }
			virtual XType * ComputableGetType(void) override
			{
				SafePointer<LObject> void_type = _ctx.QueryObject(NameVoid);
				return static_cast<XType *>(_ctx.QueryTypePointer(void_type));
			}
			virtual XA::ExpressionTree ComputableEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
			{
				return MakeAddressOf(MakeReference(func, L"I:" + _name), XA::TH::MakeSize(0, 1));
			}
		};
		class TernaryProvider : public Object, public IComputableProvider
		{
		public:
			SafePointer<XType> _retval, _boolean;
			SafePointer<LObject> _cond, _on_true, _on_false;
		public:
			TernaryProvider(void) {}
			virtual ~TernaryProvider(void) override {}
			virtual Object * ComputableProviderQueryObject(void) override { return this; }
			virtual XType * ComputableGetType(void) override { _retval->Retain(); return _retval; }
			virtual XA::ExpressionTree ComputableEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
			{
				auto init_tree = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceInit));
				SafePointer<LObject> init = CreateComputable(_retval->GetContext(), _retval, init_tree);
				SafePointer<LObject> on_true_init = InitInstance(init, _on_true);
				SafePointer<LObject> on_false_init = InitInstance(init, _on_false);
				auto fork = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformLogicalFork, XA::ReferenceFlagInvoke));
				XA::TH::AddTreeInput(fork, _cond->Evaluate(func, error_ctx), _boolean->GetArgumentSpecification());
				XA::TH::AddTreeInput(fork, on_true_init->Evaluate(func, error_ctx), XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 0));
				XA::TH::AddTreeInput(fork, on_false_init->Evaluate(func, error_ctx), XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 0));
				XA::TH::AddTreeOutput(fork, XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 0));
				auto create = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformTemporary, XA::ReferenceFlagInvoke));
				SafePointer<LObject> dtor = _retval->GetDestructor();
				XA::TH::AddTreeInput(create, fork, XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 0));
				XA::TH::AddTreeOutput(create, _retval->GetArgumentSpecification(), XA::TH::MakeFinal(dtor->GetClass() == Class::Null ?
					XA::TH::MakeRef(XA::ReferenceNull) : MakeSymbolReferenceL(func, dtor->GetFullName())));
				return create;
			}
		};
		class FunctionLoader : XI::IFunctionLoader
		{
			XFunctionOverload * _dest;
			FunctionLoader(XFunctionOverload * dest) : _dest(dest) {}
			virtual Platform GetArchitecture(void) noexcept override { return Platform::Unknown; }
			virtual XA::Environment GetEnvironment(void) noexcept override { return XA::Environment::Unknown; }
			virtual void HandleAbstractFunction(const string & symbol, const XI::Module::Function & fin, Streaming::Stream * fout) noexcept override
			{
				try {
					auto & impl = _dest->GetImplementationDesc();
					impl._xa.Load(fout);
				} catch (...) {}
			}
			virtual void HandlePlatformFunction(const string & symbol, const XI::Module::Function & fin, Streaming::Stream * fout) noexcept override {}
			virtual void HandleNearImport(const string & symbol, const XI::Module::Function & fin, const string & func_name) noexcept override {}
			virtual void HandleFarImport(const string & symbol, const XI::Module::Function & fin, const string & func_name, const string & lib_name) noexcept override {}
			virtual void HandleLoadError(const string & symbol, const XI::Module::Function & fin, XI::LoadFunctionError error) noexcept override {}
		public:
			static void Load(XFunctionOverload * dest, XI::Module::Function & src) { FunctionLoader ldr(dest); XI::LoadFunction(L"", src, &ldr); }
		};

		string GetPath(const string & symbol)
		{
			int index = symbol.FindLast(L'.');
			if (index >= 0) return symbol.Fragment(0, index); else return L"";
		}
		string GetName(const string & symbol)
		{
			int index = symbol.FindLast(L'.');
			if (index >= 0) return symbol.Fragment(index + 1, -1); else return symbol;
		}
		string GetFuncPath(const string & symbol)
		{
			int index = symbol.FindFirst(L':');
			if (index >= 0) return symbol.Fragment(0, index); else return L"";
		}
		string GetFuncSign(const string & symbol)
		{
			int index = symbol.FindFirst(L':');
			if (index >= 0) return symbol.Fragment(index + 1, -1); else return symbol;
		}
		LObject * ProvidePath(LContext & ctx, const string & symbol)
		{
			auto path = GetPath(symbol);
			if (!path.Length()) return ctx.GetRootNamespace();
			try {
				SafePointer<LObject> loc = ctx.QueryObject(path);
				return loc;
			} catch (...) {}
			auto parent = ProvidePath(ctx, path);
			SafePointer<LObject> ns = CreateNamespace(GetName(path), path, ctx);
			parent->AddMember(ns->GetName(), ns);
			return ns;
		}

		LContext::LContext(const string & module) : _idle_mode(false), _module_name(module), _import_list(0x10), _private_counter(0), _perform_version_control(false), _write_version_info(false)
		{
			_root_ns = XL::CreateNamespace(L"", L"", *this);
			_subsystem = uint(XI::Module::ExecutionSubsystem::ConsoleUI);
			_data = new DataBlock(0x1000);
			_verinfo.ThisModuleVersion = 0xFFFFFFFF;
		}
		LContext::~LContext(void) {}
		bool LContext::IsIdle(void) { return _idle_mode; }
		void LContext::SetIdleMode(bool set) { _idle_mode = set; }
		bool LContext::IsVersionControlOn(void) { return _perform_version_control; }
		bool LContext::IsVersionInfoOn(void) { return _write_version_info; }
		void LContext::SetVersionInfoMode(bool control, bool write) { _perform_version_control = control; _write_version_info = write; }
		void LContext::SetVersionInfoVersion(uint32 version) { _verinfo.ThisModuleVersion = version; }
		void LContext::SetVersionInfoSubstitute(uint32 version, uint32 with_mask) { XI::AssemblyVersionReplacement vs; vs.MustBe = version; vs.Mask = with_mask; _verinfo.ReplacesVersions << vs; }
		void LContext::MakeSubsystemConsole(void) { _subsystem = uint(XI::Module::ExecutionSubsystem::ConsoleUI); }
		void LContext::MakeSubsystemGUI(void) { _subsystem = uint(XI::Module::ExecutionSubsystem::GUI); }
		void LContext::MakeSubsystemNone(void) { _subsystem = uint(XI::Module::ExecutionSubsystem::NoUI); }
		void LContext::MakeSubsystemLibrary(void) { _subsystem = uint(XI::Module::ExecutionSubsystem::Library); }
		bool LContext::IncludeModule(const string & module_name, IModuleLoadCallback * callback, bool embed)
		{
			if (module_name == _module_name) return true;
			for (auto & imp : _import_list) if (imp == module_name) return true;
			_import_list << module_name;
			SafePointer<Streaming::Stream> input = callback->GetModuleStream(module_name);
			if (!input) return false;
			if (_perform_version_control && !_idle_mode) {
				XI::Module module(input, XI::Module::ModuleLoadFlags::LoadResources);
				input->Seek(0, Streaming::Begin);
				XI::AssemblyVersionInformation vi;
				if (XI::LoadModuleVersionInformation(module.resources, vi)) {
					if (vi.ThisModuleVersion != 0xFFFFFFFF) _verinfo.ModuleVersionsNeeded.Append(module_name, vi.ThisModuleVersion);
				}
			}
			if (embed && !_idle_mode) {
				SafePointer<DataBlock> data = input->ReadAll();
				input->Seek(0, Streaming::Begin);
				int index = 1;
				while (true) {
					auto rsrc = XI::MakeResourceID(L"XDL", index);
					if (_rsrc.ElementExists(rsrc)) { index++; continue; }
					_rsrc.Append(rsrc, data);
					break;
				}
			}
			XI::Module module(input, XI::Module::ModuleLoadFlags::LoadLink);
			Volumes::Dictionary<XClass *, XI::Module::Class *> cpp;
			for (auto & d : module.modules_depends_on) if (!IncludeModule(d, callback, false)) return false;
			for (auto & c : module.classes) try {
				auto obj = ProvidePath(*this, c.key);
				SafePointer<XClass> cls = XL::CreateClass(GetName(c.key), c.key, false, *this);
				obj->AddMember(GetName(c.key), cls);
				cls->OverrideLanguageSemantics(c.value.class_nature);
				cls->OverrideArgumentSpecification(c.value.instance_spec);
				cpp.Append(cls, &c.value);
			} catch (...) {}
			for (auto & c : module.aliases) try {
				auto obj = ProvidePath(*this, c.key);
				SafePointer<XAlias> als = CreateAliasRaw(GetName(c.key), c.key, c.value, false);
				obj->AddMember(GetName(c.key), als);
			} catch (...) {}
			for (auto & c : module.functions) try {
				auto full_path = GetFuncPath(c.key);
				auto sign = GetFuncSign(c.key);
				auto name = GetName(full_path);
				auto obj = ProvidePath(*this, full_path);
				XFunction * fd = 0;
				try {
					SafePointer<LObject> fdp = obj->GetMember(name);
					if (fdp->GetClass() == Class::Function) fd = static_cast<XFunction *>(fdp.Inner());
				} catch (...) {}
				if (!fd) {
					SafePointer<XFunction> fdc = XL::CreateFunction(*this, name, full_path, 0);
					obj->AddMember(name, fdc);
					fd = fdc;
				}
				SafePointer< Array<XI::Module::TypeReference> > args = XI::Module::TypeReference(sign).GetFunctionSignature();
				SafePointer<XType> retval = CreateType(args->ElementAt(0).QueryCanonicalName(), *this);
				ObjectArray<XType> inputs(0x10);
				Array<XType *> input_refs(0x10);
				for (int i = 1; i < args->Length(); i++) {
					SafePointer<XType> input = CreateType(args->ElementAt(i).QueryCanonicalName(), *this);
					inputs.Append(input);
					input_refs.Append(input.Inner());
				}
				uint flags = 0;
				if (c.value.code_flags & XI::Module::Function::FunctionThrows) flags |= FunctionThrows;
				if (c.value.code_flags & XI::Module::Function::FunctionInline) flags |= FunctionInline;
				if (c.value.code_flags & XI::Module::Function::ConvertorExpanding) flags |= FunctionCExpanding;
				if (c.value.code_flags & XI::Module::Function::ConvertorNarrowing) flags |= FunctionCNarrowing;
				if (c.value.code_flags & XI::Module::Function::ConvertorExpensive) flags |= FunctionCExpensive;
				if (c.value.code_flags & XI::Module::Function::ConvertorSimilar) flags |= FunctionCSimilar;
				auto fver = fd->AddOverload(retval, input_refs.Length(), input_refs.GetBuffer(), flags, false);
				if (!_idle_mode) FunctionLoader::Load(fver, c.value);
			} catch (...) {}
			for (auto & c : module.variables) try {
				auto obj = ProvidePath(*this, c.key);
				SafePointer<XType> type = XL::CreateType(c.value.type_canonical_name, *this);
				SafePointer<XComputable> var = XL::CreateVariable(*this, GetName(c.key), c.key, false, type, c.value.offset);
				obj->AddMember(GetName(c.key), var);
			} catch (...) {}
			for (auto & c : module.literals) try {
				auto obj = ProvidePath(*this, c.key);
				SafePointer<XLiteral> lit = CreateLiteral(*this, c.value);
				lit->Attach(GetName(c.key), c.key, false);
				obj->AddMember(GetName(c.key), lit);
			} catch (...) {}
			for (auto & c : cpp) {
				if (c.value->parent_class.interface_name.Length()) {
					SafePointer<LObject> parent = QueryObject(c.value->parent_class.interface_name);
					if (parent->GetClass() != Class::Type) return false;
					if (static_cast<XType *>(parent.Inner())->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) return false;
					c.key->AdoptParentClass(static_cast<XClass *>(parent.Inner()), false);
				} else c.key->SetPrimaryVFT(c.value->parent_class.vft_pointer_offset);
				for (auto & i : c.value->interfaces_implements) {
					SafePointer<LObject> interface = QueryObject(i.interface_name);
					if (interface->GetClass() != Class::Type) return false;
					if (static_cast<XType *>(interface.Inner())->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) return false;
					c.key->AdoptInterface(static_cast<XClass *>(interface.Inner()), -1, i.vft_pointer_offset);
				}
				for (auto & f : c.value->fields) {
					SafePointer<XType> type = CreateType(f.value.type_canonical_name, *this);
					XL::CreateField(c.key, type, f.key, f.value.offset);
				}
				for (auto & m : c.value->methods) {
					auto name = GetFuncPath(m.key);
					auto sign = GetFuncSign(m.key);
					XFunction * fd = 0;
					try {
						SafePointer<LObject> fdp = c.key->GetMember(name);
						if (fdp->GetClass() == Class::Function) fd = static_cast<XFunction *>(fdp.Inner());
					} catch (...) {}
					if (!fd) {
						SafePointer<XFunction> fdc = XL::CreateFunction(*this, name, c.key->GetFullName() + L"." + name, c.key);
						c.key->AddMember(name, fdc);
						fd = fdc;
					}
					SafePointer< Array<XI::Module::TypeReference> > args = XI::Module::TypeReference(sign).GetFunctionSignature();
					SafePointer<XType> retval = CreateType(args->ElementAt(0).QueryCanonicalName(), *this);
					ObjectArray<XType> inputs(0x10);
					Array<XType *> input_refs(0x10);
					for (int i = 1; i < args->Length(); i++) {
						SafePointer<XType> input = CreateType(args->ElementAt(i).QueryCanonicalName(), *this);
						inputs.Append(input);
						input_refs.Append(input.Inner());
					}
					uint flags = 0;
					if (m.value.code_flags & XI::Module::Function::FunctionInstance) flags |= FunctionMethod;
					if (m.value.code_flags & XI::Module::Function::FunctionThisCall) flags |= FunctionThisCall;
					if (m.value.code_flags & XI::Module::Function::FunctionThrows) flags |= FunctionThrows;
					if (m.value.code_flags & XI::Module::Function::FunctionInline) flags |= FunctionInline;
					if (m.value.code_flags & XI::Module::Function::ConvertorExpanding) flags |= FunctionCExpanding;
					if (m.value.code_flags & XI::Module::Function::ConvertorNarrowing) flags |= FunctionCNarrowing;
					if (m.value.code_flags & XI::Module::Function::ConvertorExpensive) flags |= FunctionCExpensive;
					if (m.value.code_flags & XI::Module::Function::ConvertorSimilar) flags |= FunctionCSimilar;
					auto fver = fd->AddOverload(retval, input_refs.Length(), input_refs.GetBuffer(), flags, false, m.value.vft_index);
					if (!_idle_mode) FunctionLoader::Load(fver, m.value);
				}
				for (auto & p : c.value->properties) {
					SafePointer<XType> type = CreateType(p.value.type_canonical_name, *this);
					auto prop = XL::CreateProperty(c.key, type, p.key);
					prop->SetSetter(p.value.setter_name);
					prop->SetGetter(p.value.getter_name);
				}
			}
			for (auto & c : cpp) c.key->UpdateInternals();
			if (_prot_hdlr) for (auto & p : module.prototypes) if (p.value.data) try { _prot_hdlr->HandlePrototype(p.key, p.value.target_language, *p.value.data, p.value.attributes); } catch (...) {}
			return true;
		}
		void LContext::SetPrototypeHandler(LPrototypeHandler * hdlr) { _prot_hdlr.SetRetain(hdlr); }
		LPrototypeHandler * LContext::GetPrototypeHandler(void) { return _prot_hdlr; }
		LObject * LContext::GetRootNamespace(void) { return _root_ns; }
		LObject * LContext::GetPrivateNamespace(void)
		{
			if (!_private_ns) {
				_private_ns = XL::CreateNamespace(L"_@" + _module_name, L"_@" + _module_name, *this);
				_root_ns->AddMember(_private_ns->GetName(), _private_ns);
			}
			return _private_ns;
		}
		LObject * LContext::CreateNamespace(LObject * create_under, const string & name)
		{
			if (!create_under || create_under->GetClass() != Class::Namespace) throw InvalidArgumentException();
			try {
				SafePointer<LObject> ns = create_under->GetMember(name);
				if (ns->GetClass() == Class::Namespace) return ns;
			} catch (...) {}
			auto prefix = create_under->GetFullName();
			if (prefix.Length()) prefix += L".";
			SafePointer<LObject> ns = XL::CreateNamespace(name, prefix + name, *this);
			create_under->AddMember(name, ns);
			return ns;
		}
		LObject * LContext::CreateAlias(LObject * create_under, const string & name, LObject * destination)
		{
			if (!create_under || !destination) throw InvalidArgumentException();
			if (create_under->GetClass() != Class::Namespace && create_under->GetClass() != Class::Type) throw InvalidArgumentException();
			auto prefix = create_under->GetFullName();
			if (prefix.Length()) prefix += L".";
			if (destination->GetClass() == Class::Type && !destination->GetFullName().Length()) {
				SafePointer<LObject> alias = XL::CreateAlias(name, prefix + name, static_cast<XType *>(destination)->GetCanonicalType(), true, true);
				create_under->AddMember(name, alias);
				return alias;
			} else if (destination->GetFullName().Length()) {
				SafePointer<LObject> alias = XL::CreateAlias(name, prefix + name, destination->GetFullName(), false, true);
				create_under->AddMember(name, alias);
				return alias;
			} else throw InvalidArgumentException();
		}
		LObject * LContext::CreateClass(LObject * create_under, const string & name)
		{
			if (!create_under) throw InvalidArgumentException();
			if (create_under->GetClass() != Class::Namespace && create_under->GetClass() != Class::Type) throw InvalidArgumentException();
			auto prefix = create_under->GetFullName();
			if (prefix.Length()) prefix += L".";
			SafePointer<LObject> cls = XL::CreateClass(name, prefix + name, true, *this);
			create_under->AddMember(name, cls);
			return cls;
		}
		LObject * LContext::CreateFunction(LObject * create_under, const string & name)
		{
			if (!create_under) throw InvalidArgumentException();
			if (create_under->GetClass() != Class::Namespace && create_under->GetClass() != Class::Type) throw InvalidArgumentException();
			try {
				SafePointer<LObject> func = create_under->GetMember(name);
				if (func->GetClass() == Class::Function) return func;
			} catch (...) {}
			auto prefix = create_under->GetFullName();
			if (prefix.Length()) prefix += L".";
			XClass * type = 0;
			if (create_under && create_under->GetClass() == Class::Type) {
				auto gt = static_cast<XType *>(create_under);
				if (gt->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Class) {
					type = static_cast<XClass *>(gt);
				} else throw InvalidArgumentException();
			}
			SafePointer<LObject> func = XL::CreateFunction(*this, name, prefix + name, type);
			create_under->AddMember(name, func);
			return func;
		}
		LObject * LContext::CreateFunctionOverload(LObject * create_under, LObject * retval, int argc, LObject ** argv, uint flags)
		{
			if (!create_under || create_under->GetClass() != Class::Function) throw InvalidArgumentException();
			return static_cast<XFunction *>(create_under)->AddOverload(static_cast<XType *>(retval), argc, reinterpret_cast<XType **>(argv), flags, true);
		}
		LObject * LContext::CreateVariable(LObject * create_under, const string & name, LObject * type)
		{
			if (!create_under) throw InvalidArgumentException();
			if (create_under->GetClass() != Class::Namespace && create_under->GetClass() != Class::Type) throw InvalidArgumentException();
			if (!type || type->GetClass() != Class::Type) throw InvalidArgumentException();
			auto xtype = static_cast<XType *>(type);
			if (xtype->IsLocked()) throw InvalidStateException();
			auto size = xtype->GetArgumentSpecification().size;
			int align = 1;
			int max_size = size.num_bytes + 8 * size.num_words;
			if (size.num_bytes > 1) align = 2;
			if (size.num_bytes > 3) align = 4;
			if (size.num_bytes > 7) align = 8;
			if (size.num_words) align = 8;
			while (_data->Length() % align) _data->Append(0);
			auto offset = _data->Length();
			for (int i = 0; i < max_size; i++) _data->Append(0);
			auto prefix = create_under->GetFullName();
			if (prefix.Length()) prefix += L".";
			SafePointer<LObject> var = XL::CreateVariable(*this, name, prefix + name, true, xtype, XA::TH::MakeSize(offset, 0));
			create_under->AddMember(name, var);
			return var;
		}
		LObject * LContext::CreateVariable(LObject * create_under, const string & name, LObject * type, XA::ObjectSize size)
		{
			if (!create_under) throw InvalidArgumentException();
			if (create_under->GetClass() != Class::Namespace && create_under->GetClass() != Class::Type) throw InvalidArgumentException();
			if (!type || type->GetClass() != Class::Type) throw InvalidArgumentException();
			auto xtype = static_cast<XType *>(type);
			int align = 1;
			int max_size = size.num_bytes + 8 * size.num_words;
			if (size.num_bytes > 1) align = 2;
			if (size.num_bytes > 3) align = 4;
			if (size.num_bytes > 7) align = 8;
			if (size.num_words) align = 8;
			while (_data->Length() % align) _data->Append(0);
			auto offset = _data->Length();
			for (int i = 0; i < max_size; i++) _data->Append(0);
			auto prefix = create_under->GetFullName();
			if (prefix.Length()) prefix += L".";
			SafePointer<LObject> var = XL::CreateVariable(*this, name, prefix + name, true, xtype, XA::TH::MakeSize(offset, 0));
			create_under->AddMember(name, var);
			return var;
		}
		LObject * LContext::CreateField(LObject * create_under, const string & name, LObject * type, XA::ObjectSize offs_override)
		{
			if (!create_under || create_under->GetClass() != Class::Type) throw InvalidArgumentException();
			if (!type || type->GetClass() != Class::Type) throw InvalidArgumentException();
			auto type_create_under = static_cast<XType *>(create_under);
			auto type_type = static_cast<XType *>(type);
			if (type_type->IsLocked()) throw InvalidStateException();
			if (type_create_under->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) throw InvalidArgumentException();
			auto class_create_under = static_cast<XClass *>(create_under);
			return XL::CreateField(class_create_under, type_type, name, offs_override);
		}
		LObject * LContext::CreateField(LObject * create_under, const string & name, LObject * type, bool align_mode)
		{
			if (!create_under || create_under->GetClass() != Class::Type) throw InvalidArgumentException();
			if (!type || type->GetClass() != Class::Type) throw InvalidArgumentException();
			auto type_create_under = static_cast<XType *>(create_under);
			auto type_type = static_cast<XType *>(type);
			if (type_type->IsLocked()) throw InvalidStateException();
			if (type_create_under->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) throw InvalidArgumentException();
			auto class_create_under = static_cast<XClass *>(create_under);
			XA::ObjectSize offset;
			if (align_mode) {
				auto spec_type = type_type->GetArgumentSpecification();
				auto spec_class = class_create_under->GetArgumentSpecification();
				offset = spec_class.size;
				if (spec_type.size.num_words) while (offset.num_bytes & 0x07) offset.num_bytes++;
				if ((spec_type.size.num_bytes & 0x07) == 0 && spec_type.size.num_bytes) {
					while (offset.num_bytes & 0x07) offset.num_bytes++;
					while (offset.num_words & 0x01) offset.num_words++;
				}
				if ((spec_type.size.num_bytes & 0x03) == 0) while (offset.num_bytes & 0x03) offset.num_bytes++;
				if ((spec_type.size.num_bytes & 0x01) == 0) while (offset.num_bytes & 0x01) offset.num_bytes++;
				XA::ObjectSize resize;
				resize.num_bytes = offset.num_bytes + spec_type.size.num_bytes;
				resize.num_words = offset.num_words + spec_type.size.num_words;
				class_create_under->OverrideArgumentSpecification(XA::TH::MakeSpec(spec_class.semantics, resize));
			} else {
				offset = class_create_under->GetArgumentSpecification().size;
				XA::ObjectSize resize;
				resize.num_bytes = offset.num_bytes + type_type->GetArgumentSpecification().size.num_bytes;
				resize.num_words = offset.num_words + type_type->GetArgumentSpecification().size.num_words;
				class_create_under->OverrideArgumentSpecification(XA::TH::MakeSpec(class_create_under->GetArgumentSpecification().semantics, resize));
			}
			return XL::CreateField(class_create_under, type_type, name, offset);
		}
		LObject * LContext::CreateProperty(LObject * create_under, const string & name, LObject * type)
		{
			if (!create_under || create_under->GetClass() != Class::Type) throw InvalidArgumentException();
			if (!type || type->GetClass() != Class::Type) throw InvalidArgumentException();
			auto type_create_under = static_cast<XType *>(create_under);
			auto type_type = static_cast<XType *>(type);
			if (type_create_under->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) throw InvalidArgumentException();
			auto class_create_under = static_cast<XClass *>(create_under);
			return XL::CreateProperty(class_create_under, type_type, name);
		}
		LObject * LContext::CreatePropertySetter(LObject * prop, uint flags)
		{
			if (!prop || prop->GetClass() != Class::Property) throw InvalidArgumentException();
			auto xprop = static_cast<XProperty *>(prop);
			auto fd = CreateFunction(xprop->GetInstanceType(), xprop->GetName() + NameSetter);
			SafePointer<LObject> type_prop = prop->GetType();
			SafePointer<LObject> type_ref = QueryTypeReference(type_prop);
			SafePointer<XType> type_void = CreateType(XI::Module::TypeReference::MakeClassReference(NameVoid), *this);
			auto over = CreateFunctionOverload(fd, type_void, 1, type_ref.InnerRef(), flags);
			xprop->SetSetter(over->GetName());
			return over;
		}
		LObject * LContext::CreatePropertyGetter(LObject * prop, uint flags)
		{
			if (!prop || prop->GetClass() != Class::Property) throw InvalidArgumentException();
			auto xprop = static_cast<XProperty *>(prop);
			auto fd = CreateFunction(xprop->GetInstanceType(), xprop->GetName() + NameGetter);
			SafePointer<LObject> type_prop = prop->GetType();
			auto over = CreateFunctionOverload(fd, type_prop, 0, 0, flags);
			xprop->SetGetter(over->GetName());
			return over;
		}
		LObject * LContext::CreatePrivateFunction(uint flags)
		{
			GetPrivateNamespace();
			auto name = string(_private_counter, HexadecimalBaseLowerCase, 8);
			_private_counter++;
			SafePointer<XFunction> fd = XL::CreateFunction(*this, name, _private_ns->GetName() + L"." + name, 0);
			_private_ns->AddMember(fd->GetName(), fd);
			SafePointer<XType> retval = XL::CreateType(XI::Module::TypeReference::MakeClassReference(NameVoid), *this);
			return fd->AddOverload(retval, 0, 0, flags, true);
		}
		LObject * LContext::CreatePrivateClass(void)
		{
			GetPrivateNamespace();
			auto name = string(_private_counter, HexadecimalBaseLowerCase, 8);
			_private_counter++;
			return CreateClass(_private_ns, name);
		}
		void LContext::InstallObject(LObject * object, const string & path) { auto obj = ProvidePath(*this, path); obj->AddMember(GetName(path), object); }
		bool LContext::IsInterface(LObject * cls)
		{
			if (!cls || cls->GetClass() != Class::Type) return false;
			auto type = static_cast<XType *>(cls);
			if (type->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) return false;
			auto xcls = static_cast<XClass *>(cls);
			return xcls->GetLanguageSemantics() == XI::Module::Class::Nature::Interface;
		}
		XA::ArgumentSemantics LContext::GetClassSemantics(LObject * cls)
		{
			if (!cls || cls->GetClass() != Class::Type) throw InvalidArgumentException();
			return static_cast<XType *>(cls)->GetArgumentSpecification().semantics;
		}
		XA::ObjectSize LContext::GetClassInstanceSize(LObject * cls)
		{
			if (!cls || cls->GetClass() != Class::Type) throw InvalidArgumentException();
			return static_cast<XType *>(cls)->GetArgumentSpecification().size;
		}
		void LContext::SetClassSemantics(LObject * cls, XA::ArgumentSemantics value)
		{
			if (!cls || cls->GetClass() != Class::Type) throw InvalidArgumentException();
			if (static_cast<XType *>(cls)->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) throw InvalidArgumentException();
			auto xtype = static_cast<XClass *>(cls);
			auto spec = xtype->GetArgumentSpecification();
			xtype->OverrideArgumentSpecification(XA::TH::MakeSpec(value, spec.size));
		}
		void LContext::SetClassInstanceSize(LObject * cls, XA::ObjectSize value)
		{
			if (!cls || cls->GetClass() != Class::Type) throw InvalidArgumentException();
			if (static_cast<XType *>(cls)->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) throw InvalidArgumentException();
			auto xtype = static_cast<XClass *>(cls);
			auto spec = xtype->GetArgumentSpecification();
			xtype->OverrideArgumentSpecification(XA::TH::MakeSpec(spec.semantics, value));
		}
		void LContext::MarkClassAsCore(LObject * cls)
		{
			if (!cls || cls->GetClass() != Class::Type) throw InvalidArgumentException();
			if (static_cast<XType *>(cls)->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) throw InvalidArgumentException();
			auto xtype = static_cast<XClass *>(cls);
			xtype->OverrideLanguageSemantics(XI::Module::Class::Nature::Core);
		}
		void LContext::MarkClassAsStandard(LObject * cls)
		{
			if (!cls || cls->GetClass() != Class::Type) throw InvalidArgumentException();
			if (static_cast<XType *>(cls)->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) throw InvalidArgumentException();
			auto xtype = static_cast<XClass *>(cls);
			xtype->OverrideLanguageSemantics(XI::Module::Class::Nature::Standard);
		}
		void LContext::MarkClassAsInterface(LObject * cls)
		{
			if (!cls || cls->GetClass() != Class::Type) throw InvalidArgumentException();
			if (static_cast<XType *>(cls)->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) throw InvalidArgumentException();
			auto xtype = static_cast<XClass *>(cls);
			xtype->OverrideLanguageSemantics(XI::Module::Class::Nature::Interface);
		}
		void LContext::QueryFunctionImplementation(LObject * func, XA::Function & code)
		{
			if (!func || func->GetClass() != Class::FunctionOverload) throw InvalidArgumentException();
			code = static_cast<XFunctionOverload *>(func)->GetImplementationDesc()._xa;
		}
		void LContext::SupplyFunctionImplementation(LObject * func, const XA::Function & code)
		{
			if (!func || func->GetClass() != Class::FunctionOverload) throw InvalidArgumentException();
			static_cast<XFunctionOverload *>(func)->GetImplementationDesc()._xa = code;
		}
		void LContext::SupplyFunctionImplementation(LObject * func, const string & name, const string & lib)
		{
			if (!func || func->GetClass() != Class::FunctionOverload) throw InvalidArgumentException();
			auto & desc = static_cast<XFunctionOverload *>(func)->GetImplementationDesc();
			desc._import_func = name;
			desc._import_library = lib;
		}
		void LContext::CreateClassDefaultMethods(LObject * cls, uint methods, ObjectArray<LObject> & vft_init)
		{
			if (!cls || cls->GetClass() != Class::Type) throw InvalidArgumentException();
			if (static_cast<XType *>(cls)->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) throw InvalidArgumentException();
			CreateDefaultImplementations(static_cast<XClass *>(cls), methods, vft_init);
		}
		void LContext::CreateClassVFT(LObject * cls)
		{
			if (!cls || cls->GetClass() != Class::Type) throw InvalidArgumentException();
			if (static_cast<XType *>(cls)->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) throw InvalidArgumentException();
			auto xcls = static_cast<XClass *>(cls);
			auto current = xcls->GetPrimaryVFT();
			auto spec = xcls->GetArgumentSpecification();
			auto size = spec.size;
			if (current.num_bytes != 0xFFFFFFFF || current.num_words != 0xFFFFFFFF) throw InvalidStateException();
			if (size.num_bytes || size.num_words) throw InvalidStateException();
			xcls->SetPrimaryVFT(XA::TH::MakeSize(0, 0));
			xcls->OverrideArgumentSpecification(XA::TH::MakeSpec(spec.semantics, 0, 1));
		}
		void LContext::AdoptParentClass(LObject * cls, LObject * parent)
		{
			if (!cls || cls->GetClass() != Class::Type) throw InvalidArgumentException();
			if (static_cast<XType *>(cls)->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) throw InvalidArgumentException();
			if (!parent || parent->GetClass() != Class::Type) throw InvalidArgumentException();
			if (static_cast<XType *>(parent)->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) throw InvalidArgumentException();
			if (static_cast<XType *>(parent)->IsLocked()) throw InvalidStateException();
			static_cast<XClass *>(cls)->AdoptParentClass(static_cast<XClass *>(parent));
		}
		void LContext::AdoptInterface(LObject * cls, LObject * interface)
		{
			if (!cls || cls->GetClass() != Class::Type) throw InvalidArgumentException();
			if (static_cast<XType *>(cls)->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) throw InvalidArgumentException();
			if (!interface || interface->GetClass() != Class::Type) throw InvalidArgumentException();
			if (static_cast<XType *>(interface)->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) throw InvalidArgumentException();
			if (static_cast<XType *>(interface)->IsLocked()) throw InvalidStateException();
			static_cast<XClass *>(cls)->AdoptInterface(static_cast<XClass *>(interface));
		}
		void LContext::LockClass(LObject * cls, bool lock)
		{
			if (!cls || cls->GetClass() != Class::Type) throw InvalidArgumentException();
			if (static_cast<XType *>(cls)->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) throw InvalidArgumentException();
			static_cast<XClass *>(cls)->Lock(lock);
		}
		LObject * LContext::QueryObject(const string & path)
		{
			auto way = path.Split(L'.');
			SafePointer<LObject> current = _root_ns;
			for (auto & w : way) {
				auto index = w.FindFirst(L':');
				auto name = w.Fragment(0, index);
				current = current->GetMember(name);
			}
			current->Retain();
			return current;
		}
		LObject * LContext::QueryScope(void) { return XL::CreateScope(); }
		LObject * LContext::QueryStaticArray(LObject * type, int volume)
		{
			if (!type || type->GetClass() != Class::Type) throw InvalidArgumentException();
			if (volume < 0) throw InvalidArgumentException();
			auto cn = static_cast<XType *>(type)->GetCanonicalType();
			if (XI::Module::TypeReference(cn).GetReferenceClass() == XI::Module::TypeReference::Class::Reference) throw InvalidArgumentException();
			return CreateType(XI::Module::TypeReference::MakeArray(cn, volume), *this);
		}
		LObject * LContext::QueryTypePointer(LObject * type)
		{
			if (!type || type->GetClass() != Class::Type) throw InvalidArgumentException();
			auto cn = static_cast<XType *>(type)->GetCanonicalType();
			if (XI::Module::TypeReference(cn).GetReferenceClass() == XI::Module::TypeReference::Class::Reference) throw InvalidArgumentException();
			return CreateType(XI::Module::TypeReference::MakePointer(cn), *this);
		}
		LObject * LContext::QueryTypeReference(LObject * type)
		{
			if (!type || type->GetClass() != Class::Type) throw InvalidArgumentException();
			auto cn = static_cast<XType *>(type)->GetCanonicalType();
			if (XI::Module::TypeReference(cn).GetReferenceClass() == XI::Module::TypeReference::Class::Reference) throw InvalidArgumentException();
			return CreateType(XI::Module::TypeReference::MakeReference(cn), *this);
		}
		LObject * LContext::QueryFunctionPointer(LObject * retval, int argc, LObject ** argv)
		{
			if (argc && !argv) throw InvalidArgumentException();
			if (!retval || retval->GetClass() != Class::Type) throw InvalidArgumentException();
			for (int i = 0; i < argc; i++) if (!argv[i] || argv[i]->GetClass() != Class::Type) throw InvalidArgumentException();
			string rv = static_cast<XType *>(retval)->GetCanonicalType();
			Array<string> args(argc);
			for (int i = 0; i < argc; i++) args << static_cast<XType *>(argv[i])->GetCanonicalType();
			return CreateType(XI::Module::TypeReference::MakePointer(XI::Module::TypeReference::MakeFunction(rv, &args)), *this);
		}
		LObject * LContext::QueryTernaryResult(LObject * cond, LObject * if_true, LObject * if_false)
		{
			SafePointer<TernaryProvider> com = new TernaryProvider;
			SafePointer<LObject> primary_type = if_true->GetType();
			if (primary_type->GetClass() != Class::Type) throw InvalidStateException();
			com->_boolean = CreateType(XI::Module::TypeReference::MakeClassReference(NameBoolean), *this);
			com->_retval.SetRetain(static_cast<XType *>(primary_type.Inner()));
			com->_cond = PerformTypeCast(com->_boolean, cond, CastPriorityConverter);
			com->_on_true = PerformTypeCast(com->_retval, if_true, CastPriorityConverter);
			com->_on_false = PerformTypeCast(com->_retval, if_false, CastPriorityConverter);
			return CreateComputable(*this, com);
		}
		LObject * LContext::QueryTypeOfOperator(void) { return new XTypeOf; }
		LObject * LContext::QuerySizeOfOperator(bool max_size) { return new XSizeOf(*this, max_size); }
		LObject * LContext::QueryModuleOperator(void)
		{
			SafePointer<ModuleProvider> provider = new ModuleProvider(*this, L"");
			return CreateComputable(*this, provider);
		}
		LObject * LContext::QueryModuleOperator(const string & name)
		{
			SafePointer<ModuleProvider> provider = new ModuleProvider(*this, name);
			return CreateComputable(*this, provider);
		}
		LObject * LContext::QueryInterfaceOperator(const string & name)
		{
			SafePointer<InterfaceProvider> provider = new InterfaceProvider(*this, name);
			return CreateComputable(*this, provider);
		}
		LObject * LContext::QueryAddressOfOperator(void) { return new XAddressOf(*this); }
		LObject * LContext::QueryLogicalAndOperator(void) { return new XLogicalAnd(*this); }
		LObject * LContext::QueryLogicalOrOperator(void) { return new XLogicalOr(*this); }
		LObject * LContext::QueryLiteral(bool value) { return CreateLiteral(*this, value); }
		LObject * LContext::QueryLiteral(uint64 value) { return CreateLiteral(*this, value); }
		LObject * LContext::QueryLiteral(double value) { return CreateLiteral(*this, value); }
		LObject * LContext::QueryLiteral(const string & value) { return CreateLiteral(*this, value); }
		LObject * LContext::QueryDetachedLiteral(LObject * base)
		{
			if (!base || base->GetClass() != Class::Literal) throw InvalidArgumentException();
			if (!base->GetFullName().Length()) { base->Retain(); return base; }
			else return static_cast<XLiteral *>(base)->Clone();
		}
		LObject * LContext::QueryNullLiteral(void) { return XL::CreateNullLiteral(*this); }
		LObject * LContext::QueryComputable(LObject * of_type, const XA::ExpressionTree & with_tree)
		{
			if (of_type->GetClass() != Class::Type) throw InvalidArgumentException();
			return CreateComputable(*this, static_cast<XType *>(of_type), with_tree);
		}
		LObject * LContext::InitInstance(LObject * instance, LObject * expression) { return XL::InitInstance(instance, expression); }
		LObject * LContext::InitInstance(LObject * instance, int argc, LObject ** argv) { return XL::InitInstance(instance, argc, argv); }
		LObject * LContext::DestroyInstance(LObject * instance) { return XL::DestroyInstance(instance); }
		LObject * LContext::SetPropertyValue(LObject * prop, LObject * value)
		{
			if (!prop || prop->GetClass() != Class::InstancedProperty) throw InvalidArgumentException();
			SafePointer<LObject> setter = static_cast<XInstancedProperty *>(prop)->GetSetter();
			return setter->Invoke(1, &value);
		}
		void LContext::AttachLiteral(LObject * literal, LObject * attach_under, const string & name)
		{
			if (!attach_under) throw InvalidArgumentException();
			if (attach_under->GetClass() != Class::Namespace && attach_under->GetClass() != Class::Type) throw InvalidArgumentException();
			auto prefix = attach_under->GetFullName();
			if (prefix.Length()) prefix += L".";
			static_cast<XLiteral *>(literal)->Attach(name, prefix + name, true);
			attach_under->AddMember(name, literal);
		}
		int LContext::QueryLiteralValue(LObject * literal)
		{
			if (!literal || literal->GetClass() != Class::Literal) throw InvalidArgumentException();
			return static_cast<XLiteral *>(literal)->QueryValueAsInteger();
		}
		string LContext::QueryLiteralString(LObject * literal)
		{
			if (!literal || literal->GetClass() != Class::Literal) throw InvalidArgumentException();
			return static_cast<XLiteral *>(literal)->QueryValueAsString();
		}
		Volumes::ObjectDictionary<uint64, DataBlock> & LContext::QueryResources(void) { return _rsrc; }
		void LContext::ProduceModule(const string & asm_name, int v1, int v2, int v3, int v4, Streaming::Stream * dest)
		{
			XI::Module module;
			module.module_import_name = _module_name;
			module.assembler_name = asm_name;
			module.assembler_version.major = v1;
			module.assembler_version.minor = v2;
			module.assembler_version.subver = v3;
			module.assembler_version.build = v4;
			module.subsystem = static_cast<XI::Module::ExecutionSubsystem>(_subsystem);
			module.modules_depends_on = _import_list;
			module.data = _data;
			module.resources = _rsrc;
			if (_write_version_info) XI::AddModuleVersionInformation(module.resources, _verinfo);
			static_cast<XObject *>(_root_ns.Inner())->EncodeSymbols(module, Class::Internal);
			module.Save(dest);
		}
	}
}