#include "xl_types.h"

#include "xl_base.h"
#include "xl_func.h"

namespace Engine
{
	namespace XL
	{
		Class XType::GetClass(void) { return Class::Type; }
		LObject * XType::GetType(void)
		{
			try {
				SafePointer<LObject> void_type = GetContext().QueryObject(NameVoid);
				return GetContext().QueryTypePointer(void_type);
			} catch (...) { throw ObjectHasNoTypeException(this); }
		}
		LObject * XType::Invoke(int argc, LObject ** argv)
		{
			try {
				ObjectArray<XType> input_types(argc);
				for (int i = 0; i < argc; i++) {
					SafePointer<LObject> type = argv[i]->GetType();
					if (type->GetClass() != Class::Type) throw InvalidArgumentException();
					input_types.Append(static_cast<XType *>(type.Inner()));
				}
				if (argc == 1) {
					// TODO: SEARCH FOR CONSTRUCTOR FIRST

					return PerformTypeCast(this, argv[0], CastPriorityExplicit);
				} else if (argc == 0) {
					SafePointer<LObject> ctor = GetConstructorInit();
					//static_cast<XFunctionOverload *>(ctor.Inner())->SupplyInstance()

					// TODO: IMPLEMENT INIT CONSTRUCTOR
					throw ObjectHasNoSuchOverloadException(this, argc, argv);

				} else {
					Array<string> cnl(argc);
					for (auto & t : input_types) cnl << t.GetCanonicalType();

					// TODO: IMPLEMENT CONSTRUCTOR
					throw ObjectHasNoSuchOverloadException(this, argc, argv);

				}
			} catch (...) { throw ObjectHasNoSuchOverloadException(this, argc, argv); }
		}

		class TypeClass : public XClass
		{
			struct _interface_info
			{
				SafePointer<TypeClass> _class;
				int _vft_index;
				XA::ObjectSize _vft_offset, _base_offset;
			};

			LContext & _ctx;
			string _name, _path, _cn;
			bool _local;
			XI::Module::TypeReference _ref;
			XA::ArgumentSpecification _type_spec;
			XI::Module::Class::Nature _nature;
			Volumes::ObjectDictionary<string, LObject> _members;
			Volumes::Dictionary<string, string> _attributes;
			int _vft_index, _last_vft_index;
			XA::ObjectSize _vft_offset;
			_interface_info _parent;
			Array<_interface_info> _interfaces;
		public:
			TypeClass(const string & name, const string & path, bool local, LContext & ctx) :
				_name(name), _path(path), _cn(XI::Module::TypeReference::MakeClassReference(path)), _local(local), _ref(_cn), _ctx(ctx), _interfaces(0x10)
			{
				_type_spec = XA::TH::MakeSpec(XA::ArgumentSemantics::Object, 0, 0);
				_nature = XI::Module::Class::Nature::Standard;
				_vft_index = _last_vft_index = -1;
				_vft_offset = XA::TH::MakeSize(-1, -1);
				_parent._vft_index = 0;
				_parent._vft_offset = XA::TH::MakeSize(-1, -1);
				_parent._base_offset = XA::TH::MakeSize(-1, -1);
			}
			virtual ~TypeClass(void) override {}
			virtual string GetName(void) override { return _name; }
			virtual string GetFullName(void) override { return _path; }
			virtual bool IsDefinedLocally(void) override { return _local; }
			virtual LObject * GetMember(const string & name) override
			{
				// TODO: IMPLEMENT ADD @

				auto member = _members[name];
				if (member && member->GetClass() == Class::Alias) {
					auto alias = static_cast<XAlias *>(member);
					if (alias->IsTypeAlias()) return CreateType(alias->GetDestination(), _ctx);
					else return _ctx.QueryObject(alias->GetDestination());
				}
				if (member) { member->Retain(); return member; } else throw ObjectHasNoSuchMemberException(this, name);
			}
			virtual void AddMember(const string & name, LObject * child) override { if (!_members.Append(name, child)) throw ObjectMemberRedefinitionException(this, name); }
			virtual void AddAttribute(const string & key, const string & value) override { if (!_attributes.Append(key, value)) throw ObjectMemberRedefinitionException(this, key); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { return MakeAddressOf(MakeSymbolReference(func, _path), XA::TH::MakeSize(0, 1)); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override
			{
				if (!_local) return;
				XI::Module::Class self;
				self.class_nature = _nature;
				self.instance_spec = _type_spec;
				self.parent_class.interface_name = _parent._class ? _parent._class->GetFullName() : L"";
				self.parent_class.vft_pointer_offset = _parent._class && _vft_index < 0 ? _parent._vft_offset : _vft_offset;
				for (auto & i : _interfaces) {
					XI::Module::Interface ie;
					ie.interface_name = i._class->GetFullName();
					ie.vft_pointer_offset = i._vft_offset;
					self.interfaces_implements << ie;
				}

				// TODO: IMPLEMENT
				// Volumes::Dictionary<string, Variable> fields;		// NAME
				// Volumes::Dictionary<string, Property> properties;	// NAME
				// Volumes::Dictionary<string, Function> methods;		// NAME:SCN

				self.attributes = _attributes;
				dest.classes.Append(_path, self);
				for (auto & m : _members) static_cast<XObject *>(m.value.Inner())->EncodeSymbols(dest, Class::Type);
			}
			virtual LContext & GetContext(void) override { return _ctx; }
			virtual string GetCanonicalType(void) override { return _cn; }
			virtual XI::Module::TypeReference::Class GetCanonicalTypeClass(void) override { return _ref.GetReferenceClass(); }
			virtual const XI::Module::TypeReference & GetTypeReference(void) override { return _ref; }
			virtual XA::ArgumentSpecification GetArgumentSpecification(void) override { return _type_spec; }
			virtual LObject * GetConstructorInit(void) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetConstructorCopy(void) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetConstructorZero(void) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetConstructorMove(void) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetDestructor(void) override
			{
				auto dtor = _members[NameDestructor];
				if (dtor) {
					if (dtor->GetClass() != Class::Function) throw InvalidStateException();
					auto dtor_overload = static_cast<XFunction *>(dtor)->GetOverloadT(0, 0, true);
					dtor_overload->Retain();
					return dtor_overload;
				} else return CreateNull();
			}
			virtual LObject * GetConstructorCast(XType * from_type) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetCastMethod(XType * to_type) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual int GetConstructorCastPriority(XType * from_type) override
			{
				if (from_type->GetCanonicalType() == GetCanonicalType()) return CastPriorityIdentity;

				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual int GetCastMethodPriority(XType * to_type) override
			{
				if (to_type->GetCanonicalType() == GetCanonicalType()) return CastPriorityIdentity;

				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual void OverrideArgumentSpecification(XA::ArgumentSpecification spec) override { _type_spec = spec; }
			virtual void OverrideLanguageSemantics(XI::Module::Class::Nature spec) override { _nature = spec; }
			virtual void AdoptParentClass(XClass * parent) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual void AdoptInterface(XClass * parent) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual XClass * GetParentClass(void) override { if (_parent._class) _parent._class->Retain(); return _parent._class; }
			virtual int GetInterfaceCount(void) override { return _interfaces.Length(); }
			virtual XClass * GetInterface(int index) override { _interfaces[index]._class->Retain(); return _interfaces[index]._class; }
			virtual string ToString(void) const override { return L"class " + _path; }
		};
		class TypeArray : public XArray
		{
			LContext & _ctx;
			string _cn;
			XI::Module::TypeReference _ref;
		public:
			TypeArray(const string & cn, LContext & ctx) : _cn(cn), _ctx(ctx), _ref(_cn) {}
			virtual ~TypeArray(void) override {}
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetMember(const string & name) override
			{
				// TODO: IMPLEMENT
				// [] always
				// @ always
				throw ObjectHasNoSuchMemberException(this, name);
			}
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
			virtual LContext & GetContext(void) override { return _ctx; }
			virtual string GetCanonicalType(void) override { return _cn; }
			virtual XI::Module::TypeReference::Class GetCanonicalTypeClass(void) override { return _ref.GetReferenceClass(); }
			virtual const XI::Module::TypeReference & GetTypeReference(void) override { return _ref; }
			virtual XA::ArgumentSpecification GetArgumentSpecification(void) override
			{
				SafePointer<XType> element = CreateType(_ref.GetArrayElement().QueryCanonicalName(), _ctx);
				auto volume = _ref.GetArrayVolume();
				auto element_spec = element->GetArgumentSpecification();
				return XA::TH::MakeSpec(XA::ArgumentSemantics::Object, element_spec.size.num_bytes * volume, element_spec.size.num_words * volume);
			}
			virtual LObject * GetConstructorInit(void) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetConstructorCopy(void) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetConstructorZero(void) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetConstructorMove(void) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetDestructor(void) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetConstructorCast(XType * from_type) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetCastMethod(XType * to_type) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual int GetConstructorCastPriority(XType * from_type) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual int GetCastMethodPriority(XType * to_type) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual XType * GetElementType(void) override { return CreateType(_ref.GetArrayElement().QueryCanonicalName(), _ctx); }
			virtual int GetVolume(void) override { return _ref.GetArrayVolume(); }
			virtual string ToString(void) const override { return L"type " + _ref.ToString(); }
		};
		class TypePointer : public XPointer
		{
			LContext & _ctx;
			string _cn;
			XI::Module::TypeReference _ref;
		public:
			TypePointer(const string & cn, LContext & ctx) : _cn(cn), _ctx(ctx), _ref(_cn) {}
			virtual ~TypePointer(void) override {}
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetMember(const string & name) override
			{
				// TODO: IMPLEMENT
				// () for func ptr
				// [] for non-func ptr
				// @ always
				// ^ for non-func ptr --> similar ref
				// ! always -> boolean
				// = always with ptr
				throw ObjectHasNoSuchMemberException(this, name);
			}
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
			virtual LContext & GetContext(void) override { return _ctx; }
			virtual string GetCanonicalType(void) override { return _cn; }
			virtual XI::Module::TypeReference::Class GetCanonicalTypeClass(void) override { return _ref.GetReferenceClass(); }
			virtual const XI::Module::TypeReference & GetTypeReference(void) override { return _ref; }
			virtual XA::ArgumentSpecification GetArgumentSpecification(void) override { return XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 1); }
			virtual LObject * GetConstructorInit(void) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetConstructorCopy(void) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetConstructorZero(void) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetConstructorMove(void) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetDestructor(void) override { return CreateNull(); }
			virtual LObject * GetConstructorCast(XType * from_type) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetCastMethod(XType * to_type) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual int GetConstructorCastPriority(XType * from_type) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual int GetCastMethodPriority(XType * to_type) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual XType * GetElementType(void) override { return CreateType(_ref.GetPointerDestination().QueryCanonicalName(), _ctx); }
			virtual string ToString(void) const override { return L"type " + _ref.ToString(); }
		};
		class TypeReference : public XReference
		{
			LContext & _ctx;
			string _cn;
			XI::Module::TypeReference _ref;
		public:
			TypeReference(const string & cn, LContext & ctx) : _cn(cn), _ctx(ctx), _ref(_cn) {}
			virtual ~TypeReference(void) override {}
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
			virtual LContext & GetContext(void) override { return _ctx; }
			virtual string GetCanonicalType(void) override { return _cn; }
			virtual XI::Module::TypeReference::Class GetCanonicalTypeClass(void) override { return _ref.GetReferenceClass(); }
			virtual const XI::Module::TypeReference & GetTypeReference(void) override { return _ref; }
			virtual XA::ArgumentSpecification GetArgumentSpecification(void) override { return XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 1); }
			virtual LObject * GetConstructorInit(void) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetConstructorCopy(void) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetConstructorZero(void) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetConstructorMove(void) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetDestructor(void) override { return CreateNull(); }
			virtual LObject * GetConstructorCast(XType * from_type) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetCastMethod(XType * to_type) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual int GetConstructorCastPriority(XType * from_type) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual int GetCastMethodPriority(XType * to_type) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual XType * GetElementType(void) override { return CreateType(_ref.GetReferenceDestination().QueryCanonicalName(), _ctx); }
			virtual string ToString(void) const override { return L"type " + _ref.ToString(); }
		};
		class TypeFunction : public XFunctionType
		{
			LContext & _ctx;
			string _cn;
			XI::Module::TypeReference _ref;
		public:
			TypeFunction(const string & cn, LContext & ctx) : _cn(cn), _ctx(ctx), _ref(_cn) {}
			virtual ~TypeFunction(void) override {}
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
			virtual LContext & GetContext(void) override { return _ctx; }
			virtual string GetCanonicalType(void) override { return _cn; }
			virtual XI::Module::TypeReference::Class GetCanonicalTypeClass(void) override { return _ref.GetReferenceClass(); }
			virtual const XI::Module::TypeReference & GetTypeReference(void) override { return _ref; }
			virtual XA::ArgumentSpecification GetArgumentSpecification(void) override { return XA::TH::MakeSpec(XA::ArgumentSemantics::Unknown, 0, 0); }
			virtual LObject * GetConstructorInit(void) override { throw ObjectHasNoSuchMemberException(this, NameConstructor); }
			virtual LObject * GetConstructorCopy(void) override { throw ObjectHasNoSuchMemberException(this, NameConstructor); }
			virtual LObject * GetConstructorZero(void) override { throw ObjectHasNoSuchMemberException(this, NameConstructorZero); }
			virtual LObject * GetConstructorMove(void) override { throw ObjectHasNoSuchMemberException(this, NameConstructorMove); }
			virtual LObject * GetDestructor(void) override { throw ObjectHasNoSuchMemberException(this, NameDestructor); }
			virtual LObject * GetConstructorCast(XType * from_type) override { throw ObjectHasNoSuchMemberException(this, NameConstructor); }
			virtual LObject * GetCastMethod(XType * to_type) override { throw ObjectHasNoSuchMemberException(this, NameConverter); }
			virtual int GetConstructorCastPriority(XType * from_type) override
			{
				if (from_type->GetCanonicalType() == GetCanonicalType()) return CastPriorityIdentity;
				else return CastPriorityNoCast;
			}
			virtual int GetCastMethodPriority(XType * to_type) override
			{
				if (to_type->GetCanonicalType() == GetCanonicalType()) return CastPriorityIdentity;
				else return CastPriorityNoCast;
			}
			virtual int GetArgumentCount(void) override
			{
				SafePointer< Array<XI::Module::TypeReference> > sgn = _ref.GetFunctionSignature();
				return sgn->Length() - 1;
			}
			virtual XType * GetArgument(int index) override
			{
				SafePointer< Array<XI::Module::TypeReference> > sgn = _ref.GetFunctionSignature();
				return CreateType(sgn->ElementAt(index + 1).QueryCanonicalName(), _ctx);
			}
			virtual XType * GetReturnValue(void) override
			{
				SafePointer< Array<XI::Module::TypeReference> > sgn = _ref.GetFunctionSignature();
				return CreateType(sgn->ElementAt(0).QueryCanonicalName(), _ctx);
			}
			virtual string ToString(void) const override { return L"type " + _ref.ToString(); }
		};

		XType * CreateType(const string & cn, LContext & ctx)
		{
			XI::Module::TypeReference ref(cn);
			if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Class) {
				SafePointer<LObject> object = ctx.QueryObject(ref.GetClassName());
				if (object->GetClass() != Class::Type) throw InvalidStateException();
				auto rv = static_cast<XType *>(object.Inner());
				rv->Retain();
				return rv;
			} else if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Array) {
				return new TypeArray(cn, ctx);
			} else if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Pointer) {
				return new TypePointer(cn, ctx);
			} else if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Reference) {
				return new TypeReference(cn, ctx);
			} else if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Function) {
				return new TypeFunction(cn, ctx);
			} else throw InvalidArgumentException();
		}
		XClass * CreateClass(const string & name, const string & path, bool local, LContext & ctx) { return new TypeClass(name, path, local, ctx); }

		int CheckCastPossibility(XType * dest, XType * src, int min_level)
		{
			int ctor = dest->GetConstructorCastPriority(src);
			int cast = src->GetCastMethodPriority(dest);
			int level = max(ctor, cast);
			return level < min_level ? -1 : level;
		}
		LObject * PerformTypeCast(XType * dest, LObject * src, int min_level)
		{
			SafePointer<LObject> src_type = src->GetType();
			int ctor = dest->GetConstructorCastPriority(static_cast<XType *>(src_type.Inner()));
			int cast = static_cast<XType *>(src_type.Inner())->GetCastMethodPriority(dest);
			int level = max(ctor, cast);
			if (level < min_level) throw ObjectHasNoSuitableCast(src, src_type, dest);
			if (level == CastPriorityIdentity) { src->Retain(); return src; }

			// TODO: IMPLEMENT
			throw LException(dest);
		}
	}
}