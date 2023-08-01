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
			SafePointer<XType> _type;
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
			Field(XClass * cls, XType * type, const string & path, const string & name) : _path(path), _name(name), _cls(cls) { _offset = XA::TH::MakeSize(0, 0); _type.SetRetain(type); }
			virtual ~Field(void) override {}
			virtual Class GetClass(void) override { return Class::Field; }
			virtual string GetName(void) override { return _name; }
			virtual string GetFullName(void) override { return _path; }
			virtual bool IsDefinedLocally(void) override { return true; }
			virtual LObject * GetType(void) override { _type->Retain(); return _type; }
			virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
			virtual LObject * Invoke(int argc, LObject ** argv) override { throw ObjectHasNoSuchOverloadException(this, argc, argv); }
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { if (!_attributes.Append(key, value)) throw ObjectMemberRedefinitionException(this, key); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override
			{
				XI::Module::Variable field;
				field.attributes = _attributes;
				field.type_canonical_name = _type->GetCanonicalType();
				field.offset = _offset;
				field.size = _type->GetArgumentSpecification().size;
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
				SafePointer<_computable> com = new _computable(_type, instance, this);
				return CreateComputable(GetContext(), com);
			}
			virtual LContext & GetContext(void) override { return _cls->GetContext(); }
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
			// TODO: IMPLEMENT
			throw Exception();
		}
	}
}