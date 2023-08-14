#include "xv_oapi.h"

#include "../xlang/xl_types.h"
#include "../xlang/xl_func.h"
#include "../xlang/xl_base.h"
#include "../xlang/xl_code.h"

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
				try {
					if (argc < 1) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					return CreateNew(_ctx, argv[0], argc - 1, argv + 1);
				} catch (...) { throw XL::ObjectHasNoSuchOverloadException(this, argc, argv); }
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
				try {
					if (argc < 1) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					return CreateConstruct(_ctx, argv[0], argc - 1, argv + 1);
				} catch (...) { throw XL::ObjectHasNoSuchOverloadException(this, argc, argv); }
			}
		};

		XL::LObject * CreateOperatorNew(XL::LContext & ctx) { return new VOperatorNew(ctx); }
		XL::LObject * CreateOperatorConstruct(XL::LContext & ctx) { return new VOperatorConstruct(ctx); }
		XL::LObject * CreateNew(XL::LContext & ctx, XL::LObject * type, int argc, XL::LObject ** argv)
		{
			if (type->GetClass() != XL::Class::Type) throw InvalidStateException();
			auto xtype = static_cast<XL::XType *>(type);
			SafePointer<XL::XFunctionOverload> ctor;
			SafePointer<XL::XType> retval;
			ObjectArray<XL::XType> inputs(argc);
			if (xtype->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Class) {
				ObjectArray<XL::XType> conf(0x10);
				xtype->GetTypesConformsTo(conf);
				bool is_object_type = false;
				for (auto & c : conf) if (c.GetCanonicalType() == XI::Module::TypeReference::MakeClassReference(L"objectum")) { is_object_type = true; break; }
				SafePointer<XL::LObject> ctor_fd = type->GetMember(XL::NameConstructor);
				if (ctor_fd->GetClass() != XL::Class::Function) throw InvalidStateException();
				ctor = static_cast<XL::XFunction *>(ctor_fd.Inner())->GetOverloadV(argc, argv, true);
				if (is_object_type) {
					SafePointer<XL::LObject> sptr = ctx.QueryObject(L"adl");
					SafePointer<XL::LObject> sptr_op_inst = sptr->GetMember(XL::OperatorSubscript);
					SafePointer<XL::LObject> sptr_inst = sptr_op_inst->Invoke(1, &type);
					if (sptr_inst->GetClass() != XL::Class::Type) throw InvalidStateException();
					retval.SetRetain(static_cast<XL::XType *>(sptr_inst.Inner()));
				} else retval = XL::CreateType(XI::Module::TypeReference::MakePointer(xtype->GetCanonicalType()), ctx);
				auto ct = ctor->GetCanonicalType();
				auto tr = XI::Module::TypeReference(ct);
				SafePointer< Array<XI::Module::TypeReference> > sgn = tr.GetFunctionSignature();
				for (int i = 1; i < sgn->Length(); i++) {
					auto & aref = sgn->ElementAt(i);
					if (aref.GetReferenceClass() == XI::Module::TypeReference::Class::Reference) {
						SafePointer<XL::XType> arg = XL::CreateType(aref.QueryCanonicalName(), ctx);
						inputs.Append(arg);
					} else {
						SafePointer<XL::XType> arg = XL::CreateType(XI::Module::TypeReference::MakeReference(aref.QueryCanonicalName()), ctx);
						inputs.Append(arg);
					}
				}
			} else if (xtype->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Pointer) {
				retval = XL::CreateType(XI::Module::TypeReference::MakePointer(xtype->GetCanonicalType()), ctx);
				SafePointer<XL::LObject> ctor_obj;
				if (argc == 1) {
					ctor_obj = xtype->GetConstructorCopy();
					SafePointer<XL::XType> ref = XL::CreateType(XI::Module::TypeReference::MakeReference(xtype->GetCanonicalType()), ctx);
					inputs.Append(ref);
				} else if (argc == 0) {
					ctor_obj = xtype->GetConstructorInit();
				} else throw InvalidArgumentException();
				ctor.SetRetain(static_cast<XL::XFunctionOverload *>(ctor_obj.Inner()));
			} else if (xtype->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Array) {
				retval = XL::CreateType(XI::Module::TypeReference::MakePointer(xtype->GetCanonicalType()), ctx);
				SafePointer<XL::LObject> ctor_obj;
				if (argc == 1) {
					ctor_obj = xtype->GetConstructorCopy();
					SafePointer<XL::XType> ref = XL::CreateType(XI::Module::TypeReference::MakeReference(xtype->GetCanonicalType()), ctx);
					inputs.Append(ref);
				} else if (argc == 0) {
					ctor_obj = xtype->GetConstructorInit();
				} else throw InvalidArgumentException();
				ctor.SetRetain(static_cast<XL::XFunctionOverload *>(ctor_obj.Inner()));
			} else throw InvalidArgumentException();
			string path = L"@crea." + xtype->GetCanonicalType();
			for (auto & i : inputs) path += L"@" + i.GetCanonicalType();
			try {
				SafePointer<XL::LObject> func_create = ctx.QueryObject(path);
				return func_create->Invoke(argc, argv);
			} catch (...) {}
			XL::LObject * ns = ctx.GetRootNamespace();
			while (true) {
				auto del = path.FindFirst(L'.');
				if (del < 0) break;
				ns = ctx.CreateNamespace(ns, path.Fragment(0, del));
				path = path.Fragment(del + 1, -1);
			}
			Array<XL::LObject *> in_args(0x10);
			Array<string> in_names(0x10);
			for (auto & i : inputs) in_args << &i;
			for (int i = 0; i < in_args.Length(); i++) in_names << (L"A@" + string(i));
			auto new_func_fd = ctx.CreateFunction(ns, path);
			auto new_func = ctx.CreateFunctionOverload(new_func_fd, retval, in_args.Length(), in_args.GetBuffer(), XL::FunctionThrows);
			XL::FunctionContextDesc desc;
			desc.retval = retval;
			desc.instance = 0;
			desc.argc = in_args.Length();
			desc.argvt = in_args.GetBuffer();
			desc.argvn = in_names.GetBuffer();
			desc.flags = XL::FunctionThrows;
			desc.vft_init = 0;
			desc.vft_init_seq = 0;
			desc.create_init_sequence = desc.create_shutdown_sequence = false;
			desc.init_callback = 0;
			SafePointer<XL::LFunctionContext> fctx = new XL::LFunctionContext(ctx, new_func, desc);
			SafePointer<XL::LObject> size_of_op = ctx.QuerySizeOfOperator();
			SafePointer<XL::LObject> allocator = ctx.QueryObject(L"memoria.alloca");
			SafePointer<XL::LObject> deallocator = ctx.QueryObject(L"memoria.dimitte");
			SafePointer<XL::XType> ptr_type = XL::CreateType(XI::Module::TypeReference::MakePointer(xtype->GetCanonicalType()), ctx);
			SafePointer<XL::LObject> type_size = size_of_op->Invoke(1, &type);
			SafePointer<XL::LObject> allocator_inv = allocator->Invoke(1, type_size.InnerRef());
			SafePointer<XL::LObject> allocator_inv_reint = XL::PerformTypeCast(ptr_type, allocator_inv, XL::CastPriorityExplicit);
			SafePointer<XL::LObject> memory = fctx->EncodeCreateVariable(ptr_type, allocator_inv_reint);
			SafePointer<XL::LObject> null_ptr = ctx.QueryNullLiteral();
			SafePointer<XL::LObject> comparator = ctx.QueryObject(XL::OperatorEqual);
			XL::LObject * cmp_list[2] = { memory, null_ptr };
			SafePointer<XL::LObject> comparator_inv = comparator->Invoke(2, cmp_list);
			XL::LObject * scope;
			fctx->OpenIfBlock(comparator_inv, &scope);
			SafePointer<XL::LObject> out_of_memory_literal = ctx.QueryLiteral(uint64(2));
			fctx->EncodeThrow(out_of_memory_literal);
			fctx->CloseIfElseBlock();
			fctx->OpenTryBlock(&scope);
			Array<XL::LObject *> arg_var_list(0x10);
			for (int i = 0; i < in_args.Length(); i++) {
				SafePointer<XL::LObject> arg_var = fctx->GetRootScope()->GetMember(L"A@" + string(i));
				arg_var_list.Append(arg_var);
			}
			SafePointer<XL::LObject> construct = CreateConstruct(ctx, memory, arg_var_list.Length(), arg_var_list.GetBuffer());
			fctx->EncodeExpression(construct);
			fctx->EncodeReturn(memory);
			SafePointer<XL::LObject> intptr_type = ctx.QueryObject(XL::NameUIntPtr);
			fctx->OpenCatchBlock(&scope, L"E@0", L"E@1", intptr_type, intptr_type);
			SafePointer<XL::LObject> deallocator_inv = deallocator->Invoke(1, memory.InnerRef());
			fctx->EncodeExpression(deallocator_inv);
			SafePointer<XL::LObject> throw_e0 = scope->GetMember(L"E@0");
			SafePointer<XL::LObject> throw_e1 = scope->GetMember(L"E@1");
			fctx->EncodeThrow(throw_e0, throw_e1);
			fctx->CloseCatchBlock();
			fctx->EndEncoding();
			return new_func->Invoke(argc, argv);
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
				return ctor_inst->Invoke(argc, argv);
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