#include "xl_lal.h"

#include "xl_com.h"
#include "xl_base.h"
#include "xl_types.h"
#include "xl_var.h"
#include "xl_func.h"
#include "../ximg/xi_resources.h"

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
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw LException(this); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
		};
		class XTypeOf : public XInternal
		{
		public:
			XTypeOf(void) {}
			virtual ~XTypeOf(void) override {}
			virtual LObject * GetType(void) override { throw ObjectHasNoTypeException(this); }
			virtual LObject * Invoke(int argc, LObject ** argv) override
			{
				if (argc != 1) throw ObjectHasNoSuchOverloadException(this, argc, argv);
				return argv[0]->GetType();
			}
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual string ToString(void) const override { return L"typeof"; }
		};
		class XSizeOf : public XInternal
		{
			LContext & _ctx;
		public:
			XSizeOf(LContext & ctx) : _ctx(ctx) {}
			virtual ~XSizeOf(void) override {}
			virtual LObject * GetType(void) override { throw ObjectHasNoTypeException(this); }
			virtual LObject * Invoke(int argc, LObject ** argv) override
			{
				if (argc != 1) throw ObjectHasNoSuchOverloadException(this, argc, argv);
				SafePointer<LObject> type;
				if (argv[0]->GetClass() == Class::Type) type.SetRetain(argv[0]);
				else type = argv[0]->GetType();
				auto size = _ctx.GetClassInstanceSize(type);

				// TODO: IMPLEMENT RETURN COMPUTABLE
				
				throw ObjectHasNoSuchOverloadException(this, argc, argv);
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

		LContext::LContext(const string & module) : _module_name(module), _import_list(0x10)
		{
			_root_ns = XL::CreateNamespace(L"", L"", *this);
			_subsystem = uint(XI::Module::ExecutionSubsystem::ConsoleUI);
		}
		LContext::~LContext(void) {}
		void LContext::MakeSubsystemConsole(void) { _subsystem = uint(XI::Module::ExecutionSubsystem::ConsoleUI); }
		void LContext::MakeSubsystemGUI(void) { _subsystem = uint(XI::Module::ExecutionSubsystem::GUI); }
		void LContext::MakeSubsystemNone(void) { _subsystem = uint(XI::Module::ExecutionSubsystem::NoUI); }
		void LContext::MakeSubsystemLibrary(void) { _subsystem = uint(XI::Module::ExecutionSubsystem::Library); }
		bool LContext::IncludeModule(const string & module_name, IModuleLoadCallback * callback)
		{
			if (module_name == _module_name) return true;
			for (auto & imp : _import_list) if (imp == module_name) return true;
			_import_list << module_name;
			SafePointer<Streaming::Stream> input = callback->GetModuleStream(module_name);
			if (!input) return false;
			XI::Module module(input, XI::Module::ModuleLoadFlags::LoadLink);
			Volumes::Dictionary<XClass *, XI::Module::Class *> cpp;
			for (auto & d : module.modules_depends_on) if (!IncludeModule(d, callback)) return false;
			for (auto & c : module.classes) {
				auto obj = ProvidePath(*this, c.key);
				SafePointer<XClass> cls = XL::CreateClass(GetName(c.key), c.key, false, *this);
				obj->AddMember(GetName(c.key), cls);
				cls->OverrideLanguageSemantics(c.value.class_nature);
				cls->OverrideArgumentSpecification(c.value.instance_spec);
				cpp.Append(cls, &c.value);
			}
			for (auto & c : module.aliases) {
				auto obj = ProvidePath(*this, c.key);
				SafePointer<XAlias> als = CreateAliasRaw(GetName(c.key), c.key, c.value, false);
				obj->AddMember(GetName(c.key), als);
			}
			for (auto & c : module.functions) {
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
				fd->AddOverload(retval, input_refs.Length(), input_refs.GetBuffer(), flags, false);
			}
			for (auto & c : module.variables) {
				// TODO: IMPLEMENT
			}
			for (auto & c : module.literals) {
				auto obj = ProvidePath(*this, c.key);
				SafePointer<XLiteral> lit = CreateLiteral(*this, c.value);
				lit->Attach(GetName(c.key), c.key, false);
				obj->AddMember(GetName(c.key), lit);
			}
			for (auto & c : cpp) {
				// c.value.fields;
				// c.value.interfaces_implements;
				// c.value.parent_class;
				// c.value.properties;

				// TODO: PROCESS CLASS

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
					fd->AddOverload(retval, input_refs.Length(), input_refs.GetBuffer(), flags, false, m.value.vft_index);
				}
			}
			return true;
		}
		LObject * LContext::GetRootNamespace(void) { return _root_ns; }
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
		LObject * LContext::QueryTypeOfOperator(void) { return new XTypeOf; }
		LObject * LContext::QuerySizeOfOperator(void) { return new XSizeOf(*this); }
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
		LObject * LContext::QueryComputable(LObject * of_type, const XA::ExpressionTree & with_tree)
		{
			if (of_type->GetClass() != Class::Type) throw InvalidArgumentException();
			return CreateComputable(*this, static_cast<XType *>(of_type), with_tree);
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
		Volumes::ObjectDictionary<string, DataBlock> & LContext::QueryResources(void) { return _rsrc; }
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
			module.resources = _rsrc;
			static_cast<XObject *>(_root_ns.Inner())->EncodeSymbols(module, Class::Internal);

			// TODO: IMPLEMENT
			// Volumes::Dictionary<string, Variable> variables;		// FQN
			// SafePointer<DataBlock> data;

			module.Save(dest);
		}
	}
}