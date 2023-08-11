#include "xv_oapi.h"

#include "../xlang/xl_types.h"
#include "../xlang/xl_func.h"
#include "../xlang/xl_base.h"

namespace Engine
{
	namespace XV
	{
		class VOperator : public XL::XObject
		{
		public:
			virtual XL::Class GetClass(void) override { return XL::Class::Internal; }
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetType(void) override { throw XL::ObjectHasNoTypeException(this); }
			virtual LObject * GetMember(const string & name) override { throw XL::ObjectHasNoSuchMemberException(this, name); }
			virtual void AddMember(const string & name, LObject * child) override { throw XL::LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw XL::ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw XL::ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, XL::Class parent) override {}
		};
		class VOperatorNew : public VOperator
		{
			XL::LContext & _ctx;
		public:
			VOperatorNew(XL::LContext & ctx) : _ctx(ctx) {}
			virtual ~VOperatorNew(void) override {}
			virtual LObject * Invoke(int argc, LObject ** argv) override
			{
				if (argc < 1) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
				return CreateNew(_ctx, argv[0], argc - 1, argv + 1);
			}
		};
		class VOperatorConstruct : public VOperator
		{
			XL::LContext & _ctx;
		public:
			VOperatorConstruct(XL::LContext & ctx) : _ctx(ctx) {}
			virtual ~VOperatorConstruct(void) override {}
			virtual LObject * Invoke(int argc, LObject ** argv) override
			{
				if (argc < 1) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
				return CreateConstruct(_ctx, argv[0], argc - 1, argv + 1);
			}
		};

		XL::LObject * CreateOperatorNew(XL::LContext & ctx) { return new VOperatorNew(ctx); }
		XL::LObject * CreateOperatorConstruct(XL::LContext & ctx) { return new VOperatorConstruct(ctx); }
		XL::LObject * CreateNew(XL::LContext & ctx, XL::LObject * type, int argc, XL::LObject ** argv)
		{
			if (type->GetClass() != XL::Class::Type) throw InvalidStateException();

			// TODO: IMPLEMENT
			throw Exception();
		}
		XL::LObject * CreateConstruct(XL::LContext & ctx, XL::LObject * at, int argc, XL::LObject ** argv)
		{
			SafePointer<XL::LObject> at_type = at->GetType();
			if (at_type->GetClass() != XL::Class::Type) throw InvalidStateException();
			if (static_cast<XL::XType *>(at_type.Inner())->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Pointer) throw InvalidArgumentException();
			SafePointer<XL::LObject> follow = at->GetMember(XL::OperatorFollow);
			SafePointer<XL::LObject> instance = follow->Invoke(0, 0);
			SafePointer<XL::LObject> type = instance->GetType();
			if (static_cast<XL::XType *>(type.Inner())->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Class) {
				SafePointer<XL::LObject> ctor;
				ctor = instance->GetMember(XL::NameConstructor);
				return ctor->Invoke(argc, argv);
			} else {
				auto xtype = static_cast<XL::XType *>(type.Inner());
				SafePointer<XL::LObject> ctor;
				if (argc == 1) ctor = xtype->GetConstructorCopy();
				else if (argc == 0) ctor = xtype->GetConstructorInit();
				else throw InvalidArgumentException();
				SafePointer<XL::LObject> ctor_inst = static_cast<XL::XFunctionOverload *>(ctor.Inner())->SetInstance(instance);
				return ctor_inst->Invoke(0, 0);
			}
		}
		XL::LObject * CreateDelete(XL::LObject * object)
		{
			SafePointer<XL::LObject> type = object->GetType();
			if (type->GetClass() != XL::Class::Type) throw InvalidStateException();
			if (static_cast<XL::XType *>(type.Inner())->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Pointer) throw InvalidArgumentException();
			SafePointer<XL::LObject> follow = object->GetMember(XL::OperatorFollow);
			SafePointer<XL::LObject> destruction_subject = follow->Invoke(0, 0);
			return CreateDestruct(destruction_subject);
		}
		XL::LObject * CreateFree(XL::LObject * object)
		{
			SafePointer<XL::LObject> type = object->GetType();
			if (type->GetClass() != XL::Class::Type) throw InvalidStateException();
			if (static_cast<XL::XType *>(type.Inner())->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Pointer) throw InvalidArgumentException();
			SafePointer<XL::LObject> memory_free = static_cast<XL::XType *>(type.Inner())->GetContext().QueryObject(L"memoria.dimitte");
			return memory_free->Invoke(1, &object);
		}
		XL::LObject * CreateDestruct(XL::LObject * object)
		{
			SafePointer<XL::LObject> type = object->GetType();
			if (type->GetClass() != XL::Class::Type) throw InvalidStateException();
			auto xtype = static_cast<XL::XType *>(type.Inner());
			if (xtype->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Class) {
				SafePointer<XL::LObject> dtor;
				try { dtor = object->GetMember(XL::NameDestructor); } catch (...) { return XL::CreateNull(); }
				return dtor->Invoke(0, 0);
			} else {
				SafePointer<XL::LObject> dtor = xtype->GetDestructor();
				if (dtor->GetClass() == XL::Class::Null) { dtor->Retain(); return dtor; }
				SafePointer<XL::LObject> dtor_inst = static_cast<XL::XFunctionOverload *>(dtor.Inner())->SetInstance(object);
				return dtor_inst->Invoke(0, 0);
			}
		}
	}
}