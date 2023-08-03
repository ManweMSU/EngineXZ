#include "xl_prop.h"

namespace Engine
{
	namespace XL
	{
		class Field : public XField
		{
			string _path, _name;
			XA::ObjectSize _offset;
			XClass * _cls;
			string _type_cn;
			Volumes::Dictionary<string, string> _attributes;

			class _computable : public Object, public IComputableProvider
			{
				SafePointer<XType> _type;
				SafePointer<LObject> _instance;
				SafePointer<XField> _field;
			public:
				_computable(XType * type, LObject * instance, XField * field) { _type.SetRetain(type); _instance.SetRetain(instance); _field.SetRetain(field); }
				virtual ~_computable(void) override {}
				virtual Object * ComputableProviderQueryObject(void) override { return this; }
				virtual XType * ComputableGetType(void) override { _type->Retain(); return _type; }
				virtual XA::ExpressionTree ComputableEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
				{
					auto node = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformAddressOffset, XA::ReferenceFlagInvoke));
					XA::TH::AddTreeInput(node, _instance->Evaluate(func, error_ctx), _field->GetInstanceType()->GetArgumentSpecification());
					XA::TH::AddTreeInput(node, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLiteral)), XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, _field->GetOffset()));
					XA::TH::AddTreeOutput(node, _type->GetArgumentSpecification());
					return node;
				}
			};
		public:
			Field(XClass * cls, XType * type, const string & path, const string & name) : _path(path), _name(name), _cls(cls) { _offset = XA::TH::MakeSize(0, 0); _type_cn = type->GetCanonicalType(); }
			virtual ~Field(void) override {}
			virtual Class GetClass(void) override { return Class::Field; }
			virtual string GetName(void) override { return _name; }
			virtual string GetFullName(void) override { return _path; }
			virtual bool IsDefinedLocally(void) override { return true; }
			virtual LObject * GetType(void) override { return CreateType(_type_cn, _cls->GetContext()); }
			virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
			virtual LObject * Invoke(int argc, LObject ** argv) override { throw ObjectHasNoSuchOverloadException(this, argc, argv); }
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { if (!_attributes.Append(key, value)) throw ObjectMemberRedefinitionException(this, key); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override
			{
				XI::Module::Variable field;
				SafePointer<XType> xtype = CreateType(_type_cn, _cls->GetContext());
				field.attributes = _attributes;
				field.type_canonical_name = _type_cn;
				field.offset = _offset;
				field.size = xtype->GetArgumentSpecification().size;
				auto del = _path.FindLast(L'.');
				auto type = _path.Fragment(0, del);
				auto type_ref = dest.classes[type];
				if (!type_ref) throw InvalidStateException();
				type_ref->fields.Append(_name, field);
			}
			virtual XA::ObjectSize & GetOffset(void) override { return _offset; }
			virtual XClass * GetInstanceType(void) override { return _cls; }
			virtual XComputable * SetInstance(LObject * instance) override
			{
				SafePointer<XType> xtype = CreateType(_type_cn, _cls->GetContext());
				SafePointer<_computable> com = new _computable(xtype, instance, this);
				return CreateComputable(GetContext(), com);
			}
			virtual LContext & GetContext(void) override { return _cls->GetContext(); }
			virtual string ToString(void) const override { return L"field " + _path; }
		};
		class InstancedProperty : public XInstancedProperty
		{
			SafePointer<XProperty> _prop;
			SafePointer<LObject> _instance;
		public:
			InstancedProperty(XProperty * prop, LObject * instance) { _prop.SetRetain(prop); _instance.SetRetain(instance); }
			virtual ~InstancedProperty(void) override {}
			virtual Class GetClass(void) override { return Class::InstancedProperty; }
			virtual string GetName(void) override { return _prop->GetName(); }
			virtual string GetFullName(void) override { return _prop->GetFullName(); }
			virtual bool IsDefinedLocally(void) override { return _prop->IsDefinedLocally(); }
			virtual LObject * GetType(void) override { return _prop->GetType(); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
			{
				SafePointer<LObject> getter = GetGetter();
				SafePointer<LObject> inv = getter->Invoke(0, 0);
				return inv->Evaluate(func, error_ctx);
			}
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
			virtual bool GetWarpMode(void) override { return true; }
			virtual LObject * UnwarpedGetType(void) override { return GetType(); }
			virtual XA::ExpressionTree UnwarpedEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { return Evaluate(func, error_ctx); }
			virtual XMethodOverload * GetSetter(void) override
			{
				auto method = _prop->GetSetter();
				auto del = method.FindFirst(L':');
				SafePointer<LObject> md = _instance->GetMember(method.Fragment(0, del));
				if (md->GetClass() != Class::Method) throw InvalidStateException();
				return static_cast<XMethod *>(md.Inner())->GetOverloadT(method.Fragment(del + 1, -1));
			}
			virtual XMethodOverload * GetGetter(void) override
			{
				auto method = _prop->GetGetter();
				auto del = method.FindFirst(L':');
				SafePointer<LObject> md = _instance->GetMember(method.Fragment(0, del));
				if (md->GetClass() != Class::Method) throw InvalidStateException();
				return static_cast<XMethod *>(md.Inner())->GetOverloadT(method.Fragment(del + 1, -1));
			}
			virtual XClass * GetInstanceType(void) override { return _prop->GetInstanceType(); }
			virtual LContext & GetContext(void) override { return _prop->GetContext(); }
		};
		class Property : public XProperty
		{
			string _path, _name;
			string _setter, _getter;
			XClass * _cls;
			string _type_cn;
			Volumes::Dictionary<string, string> _attributes;
		public:
			Property(XClass * cls, XType * type, const string & path, const string & name) : _path(path), _name(name), _cls(cls) { _type_cn = type->GetCanonicalType(); }
			virtual ~Property(void) override {}
			virtual Class GetClass(void) override { return Class::Property; }
			virtual string GetName(void) override { return _name; }
			virtual string GetFullName(void) override { return _path; }
			virtual bool IsDefinedLocally(void) override { return true; }
			virtual LObject * GetType(void) override { return CreateType(_type_cn, _cls->GetContext()); }
			virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
			virtual LObject * Invoke(int argc, LObject ** argv) override { throw ObjectHasNoSuchOverloadException(this, argc, argv); }
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { if (!_attributes.Append(key, value)) throw ObjectMemberRedefinitionException(this, key); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override
			{
				XI::Module::Property prop;
				prop.attributes = _attributes;
				prop.type_canonical_name = _type_cn;
				prop.setter_name = _setter;
				prop.getter_name = _getter;
				if (parent == Class::Type) {
					auto del = _path.FindLast(L'.');
					auto type = _path.Fragment(0, del);
					auto type_ref = dest.classes[type];
					if (!type_ref) throw InvalidStateException();
					type_ref->properties.Append(_name, prop);
				} else throw InvalidArgumentException();
			}
			virtual string GetSetter(void) override { return _setter; }
			virtual string GetGetter(void) override { return _getter; }
			virtual void SetSetter(const string & method_fqn) override { _setter = method_fqn; }
			virtual void SetGetter(const string & method_fqn) override { _getter = method_fqn; }
			virtual XClass * GetInstanceType(void) override { return _cls; }
			virtual XInstancedProperty * SetInstance(LObject * instance) override { return new InstancedProperty(this, instance); }
			virtual LContext & GetContext(void) override { return _cls->GetContext(); }
			virtual string ToString(void) const override { return L"property " + _path; }
		};

		XField * CreateField(XClass * on, XType * of_type, const string & name, XA::ObjectSize offset)
		{
			SafePointer<Field> field = new Field(on, of_type, on->GetFullName() + L"." + name, name);
			field->GetOffset() = offset;
			on->AddMember(name, field);
			return field;
		}
		XProperty * CreateProperty(XClass * on, XType * of_type, const string & name)
		{
			if (of_type->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Reference) throw InvalidArgumentException();
			ObjectArray<XType> conf;
			on->GetTypesConformsTo(conf);
			string setter, getter;
			for (auto & c : conf) if (c.GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Class) {
				try {
					SafePointer<LObject> prop = c.GetMember(name);
					if (prop->GetClass() == Class::Property) {
						setter = static_cast<XProperty *>(prop.Inner())->GetSetter();
						getter = static_cast<XProperty *>(prop.Inner())->GetGetter();
						break;
					}
				} catch (...) {}
			}
			SafePointer<Property> prop = new Property(on, of_type, on->GetFullName() + L"." + name, name);
			on->AddMember(name, prop);
			prop->SetSetter(setter);
			prop->SetGetter(getter);
			return prop;
		}
	}
}