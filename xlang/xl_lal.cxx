#include "xl_lal.h"

#include "../xasm/xa_type_helper.h"
#include "../ximg/xi_module.h"
#include "../ximg/xi_function.h"
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

		XA::ExpressionTree MakeReference(XA::Function & func_at, const string & symbol)
		{
			int index = -1;
			for (int i = 0; i < func_at.extrefs.Length(); i++) if (func_at.extrefs[i] == symbol) { index = i; break; }
			if (index < 0) {
				index = func_at.extrefs.Length();
				func_at.extrefs << symbol;
			}
			return XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceExternal, index));
		}
		XA::ExpressionTree MakeSymbolReference(XA::Function & func_at, const string & path)
		{
			auto s = L"S:" + path;
			return MakeReference(func_at, s);
		}
		XA::ExpressionTree MakeAddressOf(const XA::ExpressionTree & of)
		{
			auto result = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformTakePointer, XA::ReferenceFlagInvoke));
			XA::TH::AddTreeInput(result, of, XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 1, 0));
			XA::TH::AddTreeOutput(result, XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 1));
			return result;
		}

		class XObject : public LObject
		{
		public:
			virtual void EncodeSymbols(XI::Module & dest) = 0;
		};
		class XFunctionOverload : public XObject
		{
		public:
			virtual LObject * SupplyInstance(LObject * instance) = 0;
		};
		class XFunction : public XObject
		{
		public:
			virtual LObject * GetOverload(const string & ocn) = 0;
			virtual LObject * GetOverload(int argc, const string * argv) = 0;
		};
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
			virtual void EncodeSymbols(XI::Module & dest) override {}
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
				// TODO: IMPLEMENT RETURN COMPUTABLE
				throw ObjectHasNoSuchOverloadException(this, argc, argv);
			}
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual string ToString(void) const override { return L"sizeof"; }
		};
		class XModuleOf : public XInternal
		{
			LContext & _ctx;
			string _name;
		public:
			XModuleOf(LContext & ctx, const string & name) : _ctx(ctx), _name(name) {}
			virtual ~XModuleOf(void) override {}
			virtual LObject * GetType(void) override
			{
				SafePointer<LObject> void_type = _ctx.QueryObject(NameVoid);
				return _ctx.QueryTypePointer(void_type);
			}
			virtual LObject * Invoke(int argc, LObject ** argv) override { throw ObjectHasNoSuchOverloadException(this, argc, argv); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
			{
				if (_name.Length()) return MakeAddressOf(MakeReference(func, L"M:" + _name));
				else return MakeAddressOf(MakeReference(func, L"M"));
			}
			virtual string ToString(void) const override { return L"module"; }
		};
		class XInterfaceOf : public XInternal
		{
			LContext & _ctx;
			string _name;
		public:
			XInterfaceOf(LContext & ctx, const string & name) : _ctx(ctx), _name(name) {}
			virtual ~XInterfaceOf(void) override {}
			virtual LObject * GetType(void) override
			{
				SafePointer<LObject> void_type = _ctx.QueryObject(NameVoid);
				return _ctx.QueryTypePointer(void_type);
			}
			virtual LObject * Invoke(int argc, LObject ** argv) override { throw ObjectHasNoSuchOverloadException(this, argc, argv); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { return MakeAddressOf(MakeReference(func, L"I:" + _name)); }
			virtual string ToString(void) const override { return L"interface"; }
		};
		class XType : public XObject
		{
		public:
			virtual Class GetClass(void) override { return Class::Type; }
			virtual LObject * Invoke(int argc, LObject ** argv) override
			{
				try {
					ObjectArray<XType> input_types(argc);
					for (int i = 0; i < argc; i++) {
						SafePointer<LObject> type = argv[i]->GetType();
						if (type->GetClass() != Class::Type) throw InvalidStateException();
						input_types.Append(static_cast<XType *>(type.Inner()));
					}
					if (argc == 1) {

						// TODO: IMPLEMENT CONSTRUCTOR/CAST

					} else if (argc == 0) {
						SafePointer<LObject> ctor = GetConstructorInit();
						//static_cast<XFunctionOverload *>(ctor.Inner())->SupplyInstance()

						// TODO: IMPLEMENT INIT CONSTRUCTOR

					} else {
						Array<string> cnl(argc);
						for (auto & t : input_types) cnl << t.GetCanonicalType();

						// TODO: IMPLEMENT CONSTRUCTOR

					}
				} catch (...) { throw ObjectHasNoSuchOverloadException(this, argc, argv); }
			}
			virtual string GetCanonicalType(void) = 0;
			virtual void OverrideArgumentSpecification(XA::ArgumentSpecification spec) = 0;
			virtual void OverrideLanguageSemantics(XI::Module::Class::Nature spec) = 0;
			virtual XA::ArgumentSpecification GetArgumentSpecification(void) = 0;
			virtual LObject * GetConstructorInit(void) = 0;
			virtual LObject * GetConstructorCopy(void) = 0;
			virtual LObject * GetConstructorZero(void) = 0;
			virtual LObject * GetConstructorMove(void) = 0;
			virtual LObject * GetDestructor(void) = 0;
			virtual LObject * GetConstructorCast(LObject * from_type) = 0;
			virtual LObject * GetCastMethod(LObject * to_type) = 0;
			virtual int GetConstructorCastPriority(LObject * from_type) = 0;
			virtual int GetCastMethodPriority(LObject * to_type) = 0;
		};
		class NullObject : public XObject
		{
		public:
			NullObject(void) {}
			virtual ~NullObject(void) override {}
			virtual Class GetClass(void) override { return Class::Null; }
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetType(void) override { throw ObjectHasNoTypeException(this); }
			virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
			virtual LObject * Invoke(int argc, LObject ** argv) override { throw ObjectHasNoSuchOverloadException(this, argc, argv); }
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { return XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceNull)); }
			virtual void EncodeSymbols(XI::Module & dest) override {}
			virtual string ToString(void) const override { return L"null"; }
		};
		class Alias : public XObject
		{
			string _name, _path, _dest;
			bool _local;
		public:
			Alias(const string & name, const string & path, const string & to, bool cn_alias, bool local) : _name(name), _path(path), _local(local) { if (cn_alias) _dest = L"T:" + to; else _dest = L"S:" + to; }
			virtual ~Alias(void) override {}
			virtual Class GetClass(void) override { return Class::Alias; }
			virtual string GetName(void) override { return _name; }
			virtual string GetFullName(void) override { return _path; }
			virtual bool IsDefinedLocally(void) override { return _local; }
			virtual LObject * GetType(void) override { throw ObjectHasNoTypeException(this); }
			virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
			virtual LObject * Invoke(int argc, LObject ** argv) override { throw ObjectHasNoSuchOverloadException(this, argc, argv); }
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest) override { if (_local) dest.aliases.Append(_path, _dest); }
			virtual string ToString(void) const override { return L"alias " + _path + L" --> " + _dest; }
			bool IsTypeAlias(void) const { return _dest[0] == L'T'; }
			string GetDestination(void) const { return _dest.Fragment(2, -1); }
		};
		class CanonicalType : public XType
		{
			LContext & _ctx;
			string _cn;
		public:
			CanonicalType(const string & cn, LContext & ctx) : _cn(cn), _ctx(ctx) {}
			virtual ~CanonicalType(void) override {}
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetType(void) override
			{
				try {
					SafePointer<LObject> void_type = _ctx.QueryObject(NameVoid);
					return _ctx.QueryTypePointer(void_type);
				} catch (...) { throw ObjectHasNoTypeException(this); }
			}
			virtual LObject * GetMember(const string & name) override
			{
				// TODO: IMPLEMENT MEMBER QUERY FOR PROTOTYPE CLASS INSTANCE

				throw ObjectHasNoSuchMemberException(this, name);
			}
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest) override {}
			virtual string GetCanonicalType(void) override { return _cn; }
			virtual void OverrideArgumentSpecification(XA::ArgumentSpecification spec) override { throw InvalidArgumentException(); }
			virtual void OverrideLanguageSemantics(XI::Module::Class::Nature spec) override { throw InvalidArgumentException(); }
			virtual XA::ArgumentSpecification GetArgumentSpecification(void) override
			{
				XI::Module::TypeReference ref(_cn);
				auto cls = ref.GetReferenceClass();
				if (cls == XI::Module::TypeReference::Class::Class) {
					SafePointer<LObject> desc = _ctx.QueryObject(ref.GetClassName());
					if (desc->GetClass() != Class::Type) throw InvalidStateException();
					return static_cast<XType *>(desc.Inner())->GetArgumentSpecification();
				} else if (cls == XI::Module::TypeReference::Class::AbstractPlaceholder) {
					return XA::TH::MakeSpec(XA::ArgumentSemantics::Unknown, 0, 0);
				} else if (cls == XI::Module::TypeReference::Class::Array) {
					SafePointer<CanonicalType> element = new CanonicalType(ref.GetArrayElement().QueryCanonicalName(), _ctx);
					auto volume = ref.GetArrayVolume();
					auto element_spec = element->GetArgumentSpecification();
					return XA::TH::MakeSpec(XA::ArgumentSemantics::Object, element_spec.size.num_bytes * volume, element_spec.size.num_words * volume);
				} else if (cls == XI::Module::TypeReference::Class::Pointer) {
					return XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 1);
				} else if (cls == XI::Module::TypeReference::Class::Reference) {
					return XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 1);
				} else if (cls == XI::Module::TypeReference::Class::Function) {
					return XA::TH::MakeSpec(XA::ArgumentSemantics::Unknown, 0, 0);
				} else if (cls == XI::Module::TypeReference::Class::AbstractInstance) {
					SafePointer<CanonicalType> base = new CanonicalType(ref.GetAbstractInstanceBase().QueryCanonicalName(), _ctx);
					return base->GetArgumentSpecification();
				} else return XA::TH::MakeSpec(XA::ArgumentSemantics::Unknown, 0, 0);
			}
			virtual LObject * GetConstructorInit(void) override
			{
				XI::Module::TypeReference ref(_cn);
				auto cls = ref.GetReferenceClass();
				if (cls == XI::Module::TypeReference::Class::Class) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::AbstractPlaceholder) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Array) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Pointer) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Reference) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Function) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::AbstractInstance) {
					// TODO: IMPLEMENT
				} else throw InvalidStateException();
			}
			virtual LObject * GetConstructorCopy(void) override
			{
				XI::Module::TypeReference ref(_cn);
				auto cls = ref.GetReferenceClass();
				if (cls == XI::Module::TypeReference::Class::Class) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::AbstractPlaceholder) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Array) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Pointer) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Reference) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Function) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::AbstractInstance) {
					// TODO: IMPLEMENT
				} else throw InvalidStateException();
			}
			virtual LObject * GetConstructorZero(void) override
			{
				XI::Module::TypeReference ref(_cn);
				auto cls = ref.GetReferenceClass();
				if (cls == XI::Module::TypeReference::Class::Class) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::AbstractPlaceholder) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Array) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Pointer) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Reference) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Function) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::AbstractInstance) {
					// TODO: IMPLEMENT
				} else throw InvalidStateException();
			}
			virtual LObject * GetConstructorMove(void) override
			{
				XI::Module::TypeReference ref(_cn);
				auto cls = ref.GetReferenceClass();
				if (cls == XI::Module::TypeReference::Class::Class) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::AbstractPlaceholder) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Array) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Pointer) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Reference) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Function) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::AbstractInstance) {
					// TODO: IMPLEMENT
				} else throw InvalidStateException();
			}
			virtual LObject * GetDestructor(void) override
			{
				XI::Module::TypeReference ref(_cn);
				auto cls = ref.GetReferenceClass();
				if (cls == XI::Module::TypeReference::Class::Class) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::AbstractPlaceholder) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Array) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Pointer) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Reference) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Function) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::AbstractInstance) {
					// TODO: IMPLEMENT
				} else throw InvalidStateException();
			}
			virtual LObject * GetConstructorCast(LObject * from_type) override
			{
				XI::Module::TypeReference ref(_cn);
				auto cls = ref.GetReferenceClass();
				if (cls == XI::Module::TypeReference::Class::Class) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::AbstractPlaceholder) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Array) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Pointer) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Reference) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Function) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::AbstractInstance) {
					// TODO: IMPLEMENT
				} else throw InvalidStateException();
			}
			virtual LObject * GetCastMethod(LObject * to_type) override
			{
				XI::Module::TypeReference ref(_cn);
				auto cls = ref.GetReferenceClass();
				if (cls == XI::Module::TypeReference::Class::Class) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::AbstractPlaceholder) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Array) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Pointer) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Reference) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Function) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::AbstractInstance) {
					// TODO: IMPLEMENT
				} else throw InvalidStateException();
			}
			virtual int GetConstructorCastPriority(LObject * from_type) override
			{
				XI::Module::TypeReference ref(_cn);
				auto cls = ref.GetReferenceClass();
				if (cls == XI::Module::TypeReference::Class::Class) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::AbstractPlaceholder) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Array) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Pointer) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Reference) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Function) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::AbstractInstance) {
					// TODO: IMPLEMENT
				} else return -1;
			}
			virtual int GetCastMethodPriority(LObject * to_type) override
			{
				XI::Module::TypeReference ref(_cn);
				auto cls = ref.GetReferenceClass();
				if (cls == XI::Module::TypeReference::Class::Class) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::AbstractPlaceholder) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Array) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Pointer) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Reference) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::Function) {
					// TODO: IMPLEMENT
				} else if (cls == XI::Module::TypeReference::Class::AbstractInstance) {
					// TODO: IMPLEMENT
				} else return -1;
			}
			virtual string ToString(void) const override { return L"type " + XI::Module::TypeReference(_cn).ToString(); }
		};
		class Namespace : public XObject
		{
			string _name, _path;
			LContext & _ctx;
			Volumes::ObjectDictionary<string, LObject> _members;
		public:
			Namespace(const string & name, const string & path, LContext & ctx) : _name(name), _path(path), _ctx(ctx) {}
			virtual ~Namespace(void) override {}
			virtual Class GetClass(void) override { return Class::Namespace; }
			virtual string GetName(void) override { return _name; }
			virtual string GetFullName(void) override { return _path; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetType(void) override { throw ObjectHasNoTypeException(this); }
			virtual LObject * GetMember(const string & name) override
			{
				auto member = _members[name];
				if (member && member->GetClass() == Class::Alias) {
					auto alias = static_cast<Alias *>(member);
					if (alias->IsTypeAlias()) return new CanonicalType(alias->GetDestination(), _ctx);
					else return _ctx.QueryObject(alias->GetDestination());
				}
				if (member) { member->Retain(); return member; } else throw ObjectHasNoSuchMemberException(this, name);
			}
			virtual LObject * Invoke(int argc, LObject ** argv) override { throw ObjectHasNoSuchOverloadException(this, argc, argv); }
			virtual void AddMember(const string & name, LObject * child) override { if (!_members.Append(name, child)) throw ObjectMemberRedefinitionException(this, name); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest) override { for (auto & m : _members) static_cast<XObject *>(m.value.Inner())->EncodeSymbols(dest); }
			virtual string ToString(void) const override { if (_path.Length()) return L"namespace " + _path; else return L"root namespace"; }
		};
		class BaseType : public XType
		{
			LContext & _ctx;
			string _name, _path;
			bool _local;
			XA::ArgumentSpecification _type_spec;
			XI::Module::Class::Nature _nature;
			Volumes::Dictionary<string, string> _attributes;
		public:
			BaseType(const string & name, const string & path, bool local, LContext & ctx) : _name(name), _path(path), _local(local), _ctx(ctx)
			{
				_type_spec = XA::TH::MakeSpec(XA::ArgumentSemantics::Object, 0, 0);
				_nature = XI::Module::Class::Nature::Standard;
			}
			virtual ~BaseType(void) override {}
			virtual string GetName(void) override { return _name; }
			virtual string GetFullName(void) override { return _path; }
			virtual bool IsDefinedLocally(void) override { return _local; }
			virtual LObject * GetType(void) override
			{
				try {
					SafePointer<LObject> void_type = _ctx.QueryObject(NameVoid);
					return _ctx.QueryTypePointer(void_type);
				} catch (...) { throw ObjectHasNoTypeException(this); }
			}
			virtual LObject * GetMember(const string & name) override
			{
				// auto member = _members[name];
				// if (member) { member->Retain(); return member; } else throw ObjectHasNoSuchMemberException(this, name);

				// TODO: IMPLEMENT

				throw ObjectHasNoSuchMemberException(this, name);
			}
			virtual void AddMember(const string & name, LObject * child) override
			{
				// TODO: IMPLEMENT

				throw LException(this);
			}
			virtual void AddAttribute(const string & key, const string & value) override { if (!_attributes.Append(key, value)) throw ObjectMemberRedefinitionException(this, key); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { return MakeAddressOf(MakeSymbolReference(func, _path)); }
			virtual void EncodeSymbols(XI::Module & dest) override
			{
				if (!_local) return;
				XI::Module::Class self;
				self.class_nature = _nature;
				self.instance_spec = _type_spec;

				// TODO: IMPLEMENT
				// Interface parent_class;
				// Array<Interface> interfaces_implements;
				// Volumes::Dictionary<string, Variable> fields;		// NAME
				// Volumes::Dictionary<string, Property> properties;	// NAME
				// Volumes::Dictionary<string, Function> methods;		// NAME:SCN

				self.attributes = _attributes;
				dest.classes.Append(_path, self);
			}
			virtual string GetCanonicalType(void) override { return XI::Module::TypeReference::MakeClassReference(_path); }
			virtual void OverrideArgumentSpecification(XA::ArgumentSpecification spec) override { _type_spec = spec; }
			virtual void OverrideLanguageSemantics(XI::Module::Class::Nature spec) override { _nature = spec; }
			virtual XA::ArgumentSpecification GetArgumentSpecification(void) override { return _type_spec; }
			virtual LObject * GetConstructorInit(void) override
			{
				SafePointer<LObject> ctor = GetMember(NameConstructor);
				if (ctor->GetClass() != Class::Function) throw InvalidStateException();
				auto ctor_overload = static_cast<XFunction *>(ctor.Inner())->GetOverload(0, 0);
				ctor_overload->Retain();
				return ctor_overload;
			}
			virtual LObject * GetConstructorCopy(void) override
			{
				SafePointer<LObject> ctor = GetMember(NameConstructor);
				if (ctor->GetClass() != Class::Function) throw InvalidStateException();
				auto self_ref_cn = XI::Module::TypeReference::MakeReference(GetCanonicalType());
				auto ctor_overload = static_cast<XFunction *>(ctor.Inner())->GetOverload(1, &self_ref_cn);
				ctor_overload->Retain();
				return ctor_overload;
			}
			virtual LObject * GetConstructorZero(void) override
			{
				SafePointer<LObject> ctor = GetMember(NameConstructorZero);
				if (ctor->GetClass() != Class::Function) throw InvalidStateException();
				auto ctor_overload = static_cast<XFunction *>(ctor.Inner())->GetOverload(0, 0);
				ctor_overload->Retain();
				return ctor_overload;
			}
			virtual LObject * GetConstructorMove(void) override
			{
				SafePointer<LObject> ctor = GetMember(NameConstructorMove);
				if (ctor->GetClass() != Class::Function) throw InvalidStateException();
				auto ctor_overload = static_cast<XFunction *>(ctor.Inner())->GetOverload(0, 0);
				ctor_overload->Retain();
				return ctor_overload;
			}
			virtual LObject * GetDestructor(void) override
			{
				SafePointer<LObject> dtor = GetMember(NameDestructor);
				if (dtor->GetClass() != Class::Function) throw InvalidStateException();
				auto dtor_overload = static_cast<XFunction *>(dtor.Inner())->GetOverload(0, 0);
				dtor_overload->Retain();
				return dtor_overload;
			}
			virtual LObject * GetConstructorCast(LObject * from_type) override
			{
				if (!from_type || from_type->GetClass() != Class::Type) throw InvalidArgumentException();
				SafePointer<LObject> ctor = GetMember(NameConstructor);
				if (ctor->GetClass() != Class::Function) throw InvalidStateException();
				auto from_ref_cn = XI::Module::TypeReference::MakeReference(static_cast<XType *>(from_type)->GetCanonicalType());
				auto ctor_overload = static_cast<XFunction *>(ctor.Inner())->GetOverload(1, &from_ref_cn);
				ctor_overload->Retain();
				return ctor_overload;
			}
			virtual LObject * GetCastMethod(LObject * to_type) override
			{
				if (!to_type || to_type->GetClass() != Class::Type) throw InvalidArgumentException();
				SafePointer<LObject> conv = GetMember(NameConverter);
				if (conv->GetClass() != Class::Function) throw InvalidStateException();
				auto sign = XI::Module::TypeReference::MakeFunction(static_cast<XType *>(to_type)->GetCanonicalType(), 0);
				auto conv_overload = static_cast<XFunction *>(conv.Inner())->GetOverload(sign);
				conv_overload->Retain();
				return conv_overload;
			}
			virtual int GetConstructorCastPriority(LObject * from_type) override
			{
				if (!from_type || from_type->GetClass() != Class::Type) throw InvalidArgumentException();
				auto type = static_cast<XType *>(from_type);
				if (type->GetCanonicalType() == GetCanonicalType()) return CastPriorityIdentity;
				try {
					SafePointer<LObject> cast = GetConstructorCast(from_type);
					return CastPriorityConverter;
				} catch (...) { return CastPriorityNoCast; }
			}
			virtual int GetCastMethodPriority(LObject * to_type) override
			{
				if (!to_type || to_type->GetClass() != Class::Type) throw InvalidArgumentException();
				auto type = static_cast<XType *>(to_type);
				if (type->GetCanonicalType() == GetCanonicalType()) return CastPriorityIdentity;
				try {
					SafePointer<LObject> cast = GetCastMethod(to_type);
					return CastPriorityConverter;
				} catch (...) { return CastPriorityNoCast; }
			}
			virtual string ToString(void) const override { return L"class " + _path; }
		};
		class FunctionOverload : public XFunctionOverload
		{
			LContext & _ctx;
			string _name, _path, _cn;
			bool _local, _has_inline;
			uint _flags;
			Point _vft_info;
			XA::Function _contents;
			XA::ExpressionTree _inline_tree;
			Volumes::Dictionary<string, string> _attributes;
			string _import_func, _import_library;

			// TODO: IMPLEMENT INLINE
			// TODO: IMPLEMENT PROTOTYPE
		public:
			FunctionOverload(const string & name, const string & path, const string & cn, bool local, LContext & ctx) : _name(name), _path(path), _cn(cn), _local(local), _ctx(ctx)
			{
				_has_inline = false;
				_flags = 0;
				_vft_info = Point(-1, -1);

				// TODO: IMPLEMENT
			}
			virtual ~FunctionOverload(void) override {}
			virtual Class GetClass(void) override { return Class::FunctionOverload; }
			virtual string GetName(void) override { return _name; }
			virtual string GetFullName(void) override { return _path; }
			virtual bool IsDefinedLocally(void) override { return _local; }
			virtual LObject * GetType(void) override
			{
				if (_flags & XI::Module::Function::FunctionInstance) throw ObjectHasNoTypeException(this);
				if (_flags & XI::Module::Function::FunctionThrows) throw ObjectHasNoTypeException(this);
				return new CanonicalType(XI::Module::TypeReference::MakePointer(_cn), _ctx);
			}
			virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
			virtual LObject * Invoke(int argc, LObject ** argv) override
			{
				throw Exception();
				// TODO: IMPLEMENT
			}
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { if (!_attributes.Append(key, value)) throw ObjectMemberRedefinitionException(this, key); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
			{
				if (_flags & XI::Module::Function::FunctionInstance) throw ObjectIsNotEvaluatableException(this);
				if (_flags & XI::Module::Function::FunctionThrows) throw ObjectIsNotEvaluatableException(this);
				return MakeAddressOf(MakeSymbolReference(func, _path));
			}
			virtual void EncodeSymbols(XI::Module & dest) override
			{
				if (!_local) return;
				if (_flags & XI::Module::Function::FunctionInstance) {

					// TODO: IMPLEMENT

				} else {
					XI::Module::Function func;
					func.attributes = _attributes;
					func.vft_index = _vft_info;
					func.code_flags = _flags;
					if (_import_func.Length()) {
						if (_import_library.Length()) XI::MakeFunction(func, _import_func, _import_library);
						else XI::MakeFunction(func, _import_func);
					} else XI::MakeFunction(func, _contents);
					dest.functions.Append(_path, func);
				}
			}
			virtual string ToString(void) const override { return L"function overload " + _path; }
			virtual LObject * SupplyInstance(LObject * instance) override
			{
				// TODO: IMPLEMENT
				throw InvalidArgumentException();
			}
			uint & GetFlags(void) { return _flags; }
			XA::Function & GetContents(void) { return _contents; }
			void MakeImport(const string & func, const string & lib) { _import_func = func; _import_library = lib; }
			void SetInline(const XA::ExpressionTree & tree) { _inline_tree = tree; _has_inline = true; }
		};
		class Function : public XFunction
		{
			Volumes::ObjectDictionary<string, FunctionOverload> _overloads;
			SafePointer<FunctionOverload> _singular;
			string _name, _path;
			bool _class;
		public:
			Function(const string & name, const string & path, bool cls) : _name(name), _path(path), _class(cls) {}
			virtual ~Function(void) override {}
			virtual Class GetClass(void) override { return Class::Function; }
			virtual string GetName(void) override { return _name; }
			virtual string GetFullName(void) override { return _path; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetType(void) override { if (_singular) return _singular->GetType(); else throw ObjectHasNoTypeException(this); }
			virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
			virtual LObject * Invoke(int argc, LObject ** argv) override
			{
				if (_singular) return _singular->Invoke(argc, argv); else {

					// TODO: SELECT OVERLOAD
					throw ObjectHasNoSuchOverloadException(this, argc, argv);

				}
			}
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { if (_singular) return _singular->Evaluate(func, error_ctx); else throw ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest) override { for (auto & o : _overloads) static_cast<XObject *>(o.value.Inner())->EncodeSymbols(dest); }
			virtual string ToString(void) const override { return L"function " + _path; }
			virtual LObject * GetOverload(const string & ocn) override
			{
				auto result = _overloads[ocn];
				if (!result) throw ObjectHasNoSuchMemberException(this, _name + L":" + ocn);
				return result;
			}
			virtual LObject * GetOverload(int argc, const string * argv) override
			{
				for (auto & o : _overloads) {
					SafePointer< Array<XI::Module::TypeReference> > sgn = XI::Module::TypeReference(o.key).GetFunctionSignature();
					if (sgn->Length() != argc + 1) continue;
					bool valid = true;
					for (int i = 0; i < argc; i++) if (sgn->ElementAt(i + 1).QueryCanonicalName() != argv[i]) { valid = false; break; }
					if (valid) return o.value;
				}
				Array<string> list(1);
				list.Append(argv, argc);
				throw ObjectHasNoSuchMemberException(this, _name + L":" + XI::Module::TypeReference::MakeFunction(L"", &list));
			}
			LObject * AddOverload(LObject * retval, int argc, LObject ** argv, uint flags, LContext & ctx)
			{
				// TODO: IMPLEMENT PROTOTYPE

				if (!retval || retval->GetClass() != Class::Type) throw InvalidArgumentException();
				if (argc && !argv) throw InvalidArgumentException();
				Array<string> acn(argc);
				for (int i = 0; i < argc; i++) {
					if (!argv[i] || argv[i]->GetClass() != Class::Type) throw InvalidArgumentException();
					acn << static_cast<XType *>(argv[i])->GetCanonicalType();
				}
				string rvcn = static_cast<XType *>(retval)->GetCanonicalType();
				string fcn = XI::Module::TypeReference::MakeFunction(rvcn, &acn);
				string local_name = _name + L":" + fcn;
				string local_path = _path + L":" + fcn;
				SafePointer<FunctionOverload> overload;
				if (_class && (flags & FunctionMethod)) {

					// TODO: IMPLEMENT METHOD REGISTRATION
					return 0;

				} else {
					if ((flags & FunctionVirtual) || (flags & FunctionMethod) || (flags & FunctionThisCall)) throw InvalidArgumentException();
					overload = new FunctionOverload(local_name, local_path, fcn, true, ctx);
					if (flags & FunctionInitializer) overload->GetFlags() |= XI::Module::Function::FunctionInitialize;
					if (flags & FunctionFinalizer) overload->GetFlags() |= XI::Module::Function::FunctionShutdown;
					if (flags & FunctionMain) overload->GetFlags() |= XI::Module::Function::FunctionEntryPoint;
					if (flags & FunctionThrows) overload->GetFlags() |= XI::Module::Function::FunctionThrows;
					auto & func = overload->GetContents();
					func.retval = static_cast<XType *>(retval)->GetArgumentSpecification();
					for (int i = 0; i < argc; i++) func.inputs << static_cast<XType *>(argv[i])->GetArgumentSpecification();
					if (flags & FunctionThrows) func.inputs << XA::TH::MakeSpec(XA::ArgumentSemantics::ErrorData, 0, 1);
					if ((flags & FunctionInitializer) || (flags & FunctionMain)) {
						if (func.inputs.Length() > 1) throw InvalidArgumentException();
						if (func.retval.semantics != XA::ArgumentSemantics::Unclassified || func.retval.size.num_bytes || func.retval.size.num_words) {
							throw InvalidArgumentException();
						}
						if (func.inputs.Length() == 1) {
							if (func.inputs[0].semantics != XA::ArgumentSemantics::ErrorData || func.inputs[0].size.num_bytes != 0 || func.inputs[0].size.num_words != 1) {
								throw InvalidArgumentException();
							}
						}
					}
					if (flags & FunctionFinalizer) {
						if (func.inputs.Length()) throw InvalidArgumentException();
						if (func.retval.semantics != XA::ArgumentSemantics::Unclassified || func.retval.size.num_bytes || func.retval.size.num_words) {
							throw InvalidArgumentException();
						}
					}
				}
				if (_overloads.IsEmpty() && !_singular) _singular = overload;
				else if (_singular) _singular.SetReference(0);
				_overloads.Append(fcn, overload);
				return overload;
			}
		};
		class XComputable : public XObject
		{
		public:
			virtual LObject * GetMember(const string & name) override
			{
				// TODO: IMPLEMENT
				throw ObjectHasNoSuchMemberException(this, name);
			}
			virtual LObject * Invoke(int argc, LObject ** argv) override
			{
				// TODO: IMPLEMENT
				throw ObjectHasNoSuchOverloadException(this, argc, argv);
			}
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
		};
		class Literal : public XComputable
		{
			LContext & _ctx;
			string _name, _path;
			bool _local;
			XI::Module::Literal _data;
		public:
			Literal(LContext & ctx, bool value) : _ctx(ctx), _local(false)
			{
				_data.contents = XI::Module::Literal::Class::Boolean;
				_data.length = 1;
				_data.data_uint64 = 0;
				_data.data_boolean = value;
			}
			Literal(LContext & ctx, uint64 value) : _ctx(ctx), _local(false)
			{
				_data.contents = XI::Module::Literal::Class::UnsignedInteger;
				_data.length = value > 0xFFFFFFFF ? 8 : 4;
				_data.data_uint64 = value;
			}
			Literal(LContext & ctx, double value) : _ctx(ctx), _local(false)
			{
				_data.contents = XI::Module::Literal::Class::FloatingPoint;
				_data.length = 8;
				_data.data_double = value;
			}
			Literal(LContext & ctx, const string & value) : _ctx(ctx), _local(false)
			{
				_data.contents = XI::Module::Literal::Class::String;
				_data.length = 0;
				_data.data_uint64 = 0;
				_data.data_string = value;
			}
			Literal(const Literal & src) : _ctx(src._ctx), _local(false)
			{
				_data.contents = src._data.contents;
				_data.length = src._data.length;
				_data.data_uint64 = src._data.data_uint64;
				_data.data_string = src._data.data_string;
			}
			virtual ~Literal(void) override {}
			virtual Class GetClass(void) override { return Class::Literal; }
			virtual string GetName(void) override { return _name; }
			virtual string GetFullName(void) override { return _path; }
			virtual bool IsDefinedLocally(void) override { return _local; }
			virtual LObject * GetType(void) override
			{
				if (_data.contents == XI::Module::Literal::Class::Boolean) {
					if (_data.length == 1) {
						return _ctx.QueryObject(NameBoolean);
					} else throw ObjectHasNoTypeException(this);
				} else if (_data.contents == XI::Module::Literal::Class::SignedInteger) {
					if (_data.length == 1) {
						return _ctx.QueryObject(NameInt8);
					} else if (_data.length == 2) {
						return _ctx.QueryObject(NameInt16);
					} else if (_data.length == 4) {
						return _ctx.QueryObject(NameInt32);
					} else if (_data.length == 8) {
						return _ctx.QueryObject(NameInt64);
					} else throw ObjectHasNoTypeException(this);
				} else if (_data.contents == XI::Module::Literal::Class::UnsignedInteger) {
					if (_data.length == 1) {
						return _ctx.QueryObject(NameUInt8);
					} else if (_data.length == 2) {
						return _ctx.QueryObject(NameUInt16);
					} else if (_data.length == 4) {
						return _ctx.QueryObject(NameUInt32);
					} else if (_data.length == 8) {
						return _ctx.QueryObject(NameUInt64);
					} else throw ObjectHasNoTypeException(this);
				} else if (_data.contents == XI::Module::Literal::Class::FloatingPoint) {
					if (_data.length == 4) {
						return _ctx.QueryObject(NameFloat32);
					} else if (_data.length == 8) {
						return _ctx.QueryObject(NameFloat64);
					} else throw ObjectHasNoTypeException(this);
				} else if (_data.contents == XI::Module::Literal::Class::String) {
					SafePointer<LObject> ucs = _ctx.QueryObject(NameUInt32);
					return _ctx.QueryTypePointer(ucs);
				} else throw ObjectHasNoTypeException(this);
			}
			virtual void AddAttribute(const string & key, const string & value) override { if (!_data.attributes.Append(key, value)) throw ObjectMemberRedefinitionException(this, key); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
			{
				if (!_path.Length()) {
					if (_data.contents == XI::Module::Literal::Class::String) {
						SafePointer<DataBlock> data = _data.data_string.EncodeSequence(Encoding::UTF32, true);
						int offset = -1;
						for (int i = 0; i <= func.data.Length() - data->Length(); i++) {
							if (MemoryCompare(func.data.GetBuffer() + i, data->GetBuffer(), data->Length()) == 0) { offset = i; break; }
						}
						if (offset < 0) {
							offset = func.data.Length();
							func.data.Append(*data);
						}
						return MakeAddressOf(XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceData, offset)));
					} else {
						int nwords = func.data.Length() / _data.length;
						int offset = -1;
						for (int i = 0; i < nwords; i++) {
							if (MemoryCompare(func.data.GetBuffer() + _data.length * i, &_data.data_uint64, _data.length) == 0) { offset = _data.length * i; break; }
						}
						if (offset < 0) {
							while (func.data.Length() % _data.length) func.data.Append(0);
							offset = func.data.Length();
							func.data.SetLength(offset + _data.length);
							MemoryCopy(func.data.GetBuffer() + offset, &_data.data_uint64, _data.length);
						}
						return XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceData, offset));
					}
				} else return MakeSymbolReference(func, _path);
			}
			virtual void EncodeSymbols(XI::Module & dest) override { if (_local) dest.literals.Append(_path, _data); }
			virtual string ToString(void) const override { return L"function " + _path; }
			void Attach(const string & name, const string & path) { _name = name; _path = path; _local = true; }
			int QueryValue(void)
			{
				if (_data.contents == XI::Module::Literal::Class::UnsignedInteger || _data.contents == XI::Module::Literal::Class::SignedInteger) {
					return _data.data_sint64;
				} else throw InvalidStateException();
			}
			string QueryString(void) { if (_data.contents == XI::Module::Literal::Class::String) { return _data.data_string; } else throw InvalidStateException(); }
		};

		LContext::LContext(const string & module) : _module_name(module)
		{
			_root_ns = new Namespace(L"", L"", *this);
			_subsystem = uint(XI::Module::ExecutionSubsystem::ConsoleUI);
		}
		LContext::~LContext(void) {}
		void LContext::MakeSubsystemConsole(void) { _subsystem = uint(XI::Module::ExecutionSubsystem::ConsoleUI); }
		void LContext::MakeSubsystemGUI(void) { _subsystem = uint(XI::Module::ExecutionSubsystem::GUI); }
		void LContext::MakeSubsystemNone(void) { _subsystem = uint(XI::Module::ExecutionSubsystem::NoUI); }
		void LContext::MakeSubsystemLibrary(void) { _subsystem = uint(XI::Module::ExecutionSubsystem::Library); }
		bool LContext::IncludeModule(const string & name, Streaming::Stream * module_data)
		{
			// TODO: IMPLEMENT

			return false;
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
			SafePointer<LObject> ns = new Namespace(name, prefix + name, *this);
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
				SafePointer<LObject> alias = new Alias(name, prefix + name, static_cast<XType *>(destination)->GetCanonicalType(), true, true);
				create_under->AddMember(name, alias);
				return alias;
			} else if (destination->GetFullName().Length()) {
				SafePointer<LObject> alias = new Alias(name, prefix + name, destination->GetFullName(), false, true);
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
			SafePointer<LObject> cls = new BaseType(name, prefix + name, true, *this);
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
			SafePointer<LObject> func = new Function(name, prefix + name, create_under->GetClass() == Class::Type);
			create_under->AddMember(name, func);
			return func;
		}
		LObject * LContext::CreateFunctionOverload(LObject * create_under, LObject * retval, int argc, LObject ** argv, uint flags)
		{
			if (!create_under || create_under->GetClass() != Class::Function) throw InvalidArgumentException();
			return static_cast<Function *>(create_under)->AddOverload(retval, argc, argv, flags, *this);
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
			auto spec = static_cast<XType *>(cls)->GetArgumentSpecification();
			static_cast<XType *>(cls)->OverrideArgumentSpecification(XA::TH::MakeSpec(value, spec.size));
		}
		void LContext::SetClassInstanceSize(LObject * cls, XA::ObjectSize value)
		{
			if (!cls || cls->GetClass() != Class::Type) throw InvalidArgumentException();
			auto spec = static_cast<XType *>(cls)->GetArgumentSpecification();
			static_cast<XType *>(cls)->OverrideArgumentSpecification(XA::TH::MakeSpec(spec.semantics, value));
		}
		void LContext::MarkClassAsCore(LObject * cls)
		{
			if (!cls || cls->GetClass() != Class::Type) throw InvalidArgumentException();
			static_cast<XType *>(cls)->OverrideLanguageSemantics(XI::Module::Class::Nature::Core);
		}
		void LContext::MarkClassAsStandard(LObject * cls)
		{
			if (!cls || cls->GetClass() != Class::Type) throw InvalidArgumentException();
			static_cast<XType *>(cls)->OverrideLanguageSemantics(XI::Module::Class::Nature::Standard);
		}
		void LContext::MarkClassAsInterface(LObject * cls)
		{
			if (!cls || cls->GetClass() != Class::Type) throw InvalidArgumentException();
			static_cast<XType *>(cls)->OverrideLanguageSemantics(XI::Module::Class::Nature::Interface);
		}
		void LContext::QueryFunctionImplementation(LObject * func, XA::Function & code)
		{
			if (!func || func->GetClass() != Class::FunctionOverload) throw InvalidArgumentException();
			code = static_cast<FunctionOverload *>(func)->GetContents();
		}
		void LContext::SupplyFunctionImplementation(LObject * func, const XA::Function & code)
		{
			if (!func || func->GetClass() != Class::FunctionOverload) throw InvalidArgumentException();
			static_cast<FunctionOverload *>(func)->GetContents() = code;
		}
		void LContext::SupplyFunctionImplementation(LObject * func, const string & name, const string & lib)
		{
			if (!func || func->GetClass() != Class::FunctionOverload) throw InvalidArgumentException();
			static_cast<FunctionOverload *>(func)->MakeImport(name, lib);
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
		LObject * LContext::QueryStaticArray(LObject * type, int volume)
		{
			if (!type || type->GetClass() != Class::Type) throw InvalidArgumentException();
			if (volume < 0) throw InvalidArgumentException();
			auto cn = static_cast<XType *>(type)->GetCanonicalType();
			if (XI::Module::TypeReference(cn).GetReferenceClass() == XI::Module::TypeReference::Class::Reference) throw InvalidArgumentException();
			return new CanonicalType(XI::Module::TypeReference::MakeArray(cn, volume), *this);
		}
		LObject * LContext::QueryTypePointer(LObject * type)
		{
			if (!type || type->GetClass() != Class::Type) throw InvalidArgumentException();
			auto cn = static_cast<XType *>(type)->GetCanonicalType();
			if (XI::Module::TypeReference(cn).GetReferenceClass() == XI::Module::TypeReference::Class::Reference) throw InvalidArgumentException();
			return new CanonicalType(XI::Module::TypeReference::MakePointer(cn), *this);
		}
		LObject * LContext::QueryTypeReference(LObject * type)
		{
			if (!type || type->GetClass() != Class::Type) throw InvalidArgumentException();
			auto cn = static_cast<XType *>(type)->GetCanonicalType();
			if (XI::Module::TypeReference(cn).GetReferenceClass() == XI::Module::TypeReference::Class::Reference) throw InvalidArgumentException();
			return new CanonicalType(XI::Module::TypeReference::MakeReference(cn), *this);
		}
		LObject * LContext::QueryFunctionPointer(LObject * retval, int argc, LObject ** argv)
		{
			if (argc && !argv) throw InvalidArgumentException();
			if (!retval || retval->GetClass() != Class::Type) throw InvalidArgumentException();
			for (int i = 0; i < argc; i++) if (!argv[i] || argv[i]->GetClass() != Class::Type) throw InvalidArgumentException();
			string rv = static_cast<XType *>(retval)->GetCanonicalType();
			Array<string> args(argc);
			for (int i = 0; i < argc; i++) args << static_cast<XType *>(argv[i])->GetCanonicalType();
			return new CanonicalType(XI::Module::TypeReference::MakePointer(XI::Module::TypeReference::MakeFunction(rv, &args)), *this);
		}
		LObject * LContext::QueryAbstractType(int index) { return new CanonicalType(XI::Module::TypeReference::MakeAbstractPlaceholder(index), *this); }
		LObject * LContext::QueryTypeOfOperator(void) { return new XTypeOf; }
		LObject * LContext::QuerySizeOfOperator(void) { return new XSizeOf(*this); }
		LObject * LContext::QueryModuleOperator(void) { return new XModuleOf(*this, L""); }
		LObject * LContext::QueryModuleOperator(const string & name) { return new XModuleOf(*this, name); }
		LObject * LContext::QueryInterfaceOperator(const string & name) { return new XInterfaceOf(*this, name); }
		LObject * LContext::QueryLiteral(bool value) { return new Literal(*this, value); }
		LObject * LContext::QueryLiteral(uint64 value) { return new Literal(*this, value); }
		LObject * LContext::QueryLiteral(double value) { return new Literal(*this, value); }
		LObject * LContext::QueryLiteral(const string & value) { return new Literal(*this, value); }
		LObject * LContext::QueryDetachedLiteral(LObject * base)
		{
			if (!base || base->GetClass() != Class::Literal) throw InvalidArgumentException();
			if (!static_cast<Literal *>(base)->GetFullName().Length()) { base->Retain(); return base; }
			else return new Literal(*static_cast<Literal *>(base));
		}
		void LContext::AttachLiteral(LObject * literal, LObject * attach_under, const string & name)
		{
			if (!attach_under) throw InvalidArgumentException();
			if (attach_under->GetClass() != Class::Namespace && attach_under->GetClass() != Class::Type) throw InvalidArgumentException();
			auto prefix = attach_under->GetFullName();
			if (prefix.Length()) prefix += L".";
			static_cast<Literal *>(literal)->Attach(name, prefix + name);
			attach_under->AddMember(name, literal);
		}
		int LContext::QueryLiteralValue(LObject * literal)
		{
			if (!literal || literal->GetClass() != Class::Literal) throw InvalidArgumentException();
			return static_cast<Literal *>(literal)->QueryValue();
		}
		string LContext::QueryLiteralString(LObject * literal)
		{
			if (!literal || literal->GetClass() != Class::Literal) throw InvalidArgumentException();
			return static_cast<Literal *>(literal)->QueryString();
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
			module.resources = _rsrc;
			static_cast<XObject *>(_root_ns.Inner())->EncodeSymbols(module);

			// TODO: IMPLEMENT
			// Array<string> modules_depends_on;
			// Volumes::Dictionary<string, Variable> variables;		// FQN
			// SafePointer<DataBlock> data;

			module.Save(dest);
		}
	}
}