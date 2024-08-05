#include "xv_oapi.h"

#include "../xlang/xl_types.h"
#include "../xlang/xl_func.h"
#include "../xlang/xl_base.h"
#include "../xlang/xl_code.h"
#include "../xlang/xl_var.h"
#include "xv_meta.h"

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
			virtual void ListMembers(Volumes::Dictionary<string, XL::Class> & list) override {}
			virtual void AddMember(const string & name, LObject * child) override { throw XL::LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw XL::ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw XL::ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, XL::Class parent) override {}
		};
		class VOperatorNew : public VOperator
		{
			XL::LContext & _ctx;
		public:
			static void ListInitOverloads(SafePointer<XL::LObject> & type, SafePointer<XL::LObject> & rv, Volumes::KeyValuePair< SafePointer<XL::LObject>, XL::Class > & a0,
				Volumes::List<XL::InvokationDesc> & list)
			{
				Volumes::List<XL::InvokationDesc> internal;
				type->ListInvokations(0, internal);
				for (auto & i : internal) {
					i.arglist.RemoveFirst();
					i.arglist.InsertFirst(a0);
					i.arglist.InsertFirst(Volumes::KeyValuePair< SafePointer<XL::LObject>, XL::Class >(rv, XL::Class::Null));
					list.InsertLast(i);
				}
			}
			VOperatorNew(XL::LContext & ctx) : _ctx(ctx) {}
			virtual ~VOperatorNew(void) override {}
			virtual LObject * Invoke(int argc, LObject ** argv, LObject ** actual) override
			{
				try {
					if (argc < 1) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					if (actual) { *actual = this; Retain(); }
					return CreateNew(_ctx, argv[0], argc - 1, argv + 1);
				} catch (...) { throw XL::ObjectHasNoSuchOverloadException(this, argc, argv); }
			}
			virtual void ListInvokations(XL::LObject * first, Volumes::List<XL::InvokationDesc> & list) override
			{
				if (first && first->GetClass() == XL::Class::Type) {
					SafePointer<XL::LObject> type;
					SafePointer<XL::LObject> rv = _ctx.QueryTypePointer(first);
					type.SetRetain(first);
					Volumes::KeyValuePair< SafePointer<XL::LObject>, XL::Class > a0(0, XL::Class::Type);
					ListInitOverloads(type, rv, a0, list);
				} else {
					XL::InvokationDesc result;
					result.arglist.InsertLast(Volumes::KeyValuePair< SafePointer<XL::LObject>, XL::Class >(0, XL::Class::Null));
					result.arglist.InsertLast(Volumes::KeyValuePair< SafePointer<XL::LObject>, XL::Class >(0, XL::Class::Type));
					list.InsertLast(result);
				}
			}
		};
		class VOperatorConstruct : public VOperator
		{
			XL::LContext & _ctx;
		public:
			VOperatorConstruct(XL::LContext & ctx) : _ctx(ctx) {}
			virtual ~VOperatorConstruct(void) override {}
			virtual LObject * Invoke(int argc, LObject ** argv, LObject ** actual) override
			{
				try {
					if (argc < 1) throw XL::ObjectHasNoSuchOverloadException(this, argc, argv);
					if (actual) { *actual = this; Retain(); }
					return CreateConstruct(_ctx, argv[0], argc - 1, argv + 1);
				} catch (...) { throw XL::ObjectHasNoSuchOverloadException(this, argc, argv); }
			}
			virtual void ListInvokations(XL::LObject * first, Volumes::List<XL::InvokationDesc> & list) override
			{
				if (first) {
					SafePointer<XL::LObject> type;
					SafePointer<XL::LObject> ptr = _ctx.QueryTypePointer(first);
					SafePointer<XL::LObject> rv = _ctx.QueryObject(XL::NameVoid);
					type.SetRetain(first);
					Volumes::KeyValuePair< SafePointer<XL::LObject>, XL::Class > a0(ptr, XL::Class::Null);
					VOperatorNew::ListInitOverloads(type, rv, a0, list);
				} else {
					XL::InvokationDesc result;
					result.arglist.InsertLast(Volumes::KeyValuePair< SafePointer<XL::LObject>, XL::Class >(0, XL::Class::Null));
					result.arglist.InsertLast(Volumes::KeyValuePair< SafePointer<XL::LObject>, XL::Class >(0, XL::Class::Variable));
					list.InsertLast(result);
				}
			}
		};
		class VNamespace : public XL::XObject
		{
			XL::LContext & _ctx;
			Volumes::Dictionary<string, string> _dict;
		public:
			VNamespace(XL::LContext & ctx, const Volumes::Dictionary<string, string> & dict) : _ctx(ctx), _dict(dict) {}
			virtual ~VNamespace(void) override {}
			virtual XL::Class GetClass(void) override { return XL::Class::Namespace; }
			virtual string GetName(void) override { return Lexic::IdentifierDefs; }
			virtual string GetFullName(void) override { return Lexic::IdentifierDefs; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetType(void) override { throw XL::ObjectHasNoTypeException(this); }
			virtual LObject * GetMember(const string & name) override
			{
				auto value = _dict.GetElementByKey(name);
				if (value) return _ctx.QueryLiteral(*value); else return _ctx.QueryLiteral(string(L""));
			}
			virtual void ListMembers(Volumes::Dictionary<string, XL::Class> & list) override {}
			virtual LObject * Invoke(int argc, LObject ** argv, LObject ** actual) override { throw XL::ObjectHasNoSuchOverloadException(this, argc, argv); }
			virtual void ListInvokations(XL::LObject * first, Volumes::List<XL::InvokationDesc> & list) override {}
			virtual void AddMember(const string & name, LObject * child) override { throw XL::LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw XL::ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw XL::ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, XL::Class parent) override {}
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
			path = path.Replace(L".", L"@.");
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
		XL::LObject * CreateDynamicCast(XL::LObject * object, XL::LObject * type_into)
		{
			if (type_into->GetClass() != XL::Class::Type) throw InvalidArgumentException();
			auto xtype = static_cast<XL::XType *>(type_into);
			if (xtype->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) throw InvalidArgumentException();
			try {
				ObjectArray<XL::XType> conf(0x10);
				SafePointer<XL::LObject> type_into_ptr = xtype->GetContext().QueryTypePointer(type_into);
				SafePointer<XL::LObject> cast;
				try {
					SafePointer<XL::LObject> follow = object->GetMember(XL::OperatorFollow);
					SafePointer<XL::LObject> instance = follow->Invoke(0, 0);
					cast = instance->GetMember(L"converte_dynamice");
				} catch (...) {}
				if (!cast) cast = object->GetMember(L"converte_dynamice");
				SafePointer<XL::LObject> cast_inv = cast->Invoke(1, &type_into);
				cast_inv = XL::PerformTypeCast(static_cast<XL::XType *>(type_into_ptr.Inner()), cast_inv, XL::CastPriorityExplicit);
				xtype->GetTypesConformsTo(conf);
				bool is_object = false;
				for (auto & c : conf) if (c.GetFullName() == L"objectum") { is_object = true; break; }
				if (is_object) {
					SafePointer<XL::LObject> safe_ptr = xtype->GetContext().QueryObject(L"adl");
					SafePointer<XL::LObject> safe_ptr_inst_op = safe_ptr->GetMember(XL::OperatorSubscript);
					SafePointer<XL::LObject> safe_ptr_inst = safe_ptr_inst_op->Invoke(1, &type_into);
					if (safe_ptr_inst->GetClass() != XL::Class::Type) throw InvalidStateException();
					return XL::PerformTypeCast(static_cast<XL::XType *>(safe_ptr_inst.Inner()), cast_inv, XL::CastPriorityConverter);
				} else { cast_inv->Retain(); return cast_inv; }
			} catch (...) {}
			SafePointer<XL::LObject> instance;
			SafePointer<XL::XType> dest_type = XL::CreateType(XI::Module::TypeReference::MakeReference(xtype->GetCanonicalType()), xtype->GetContext());
			try {
				SafePointer<XL::LObject> follow = object->GetMember(XL::OperatorFollow);
				instance = follow->Invoke(0, 0);
			} catch (...) {}
			if (!instance) instance.SetRetain(object);
			SafePointer<XL::LObject> src_type = instance->GetType();
			try { return static_cast<XL::XType *>(src_type.Inner())->TransformTo(instance, dest_type, false); }
			catch (...) {}
			return static_cast<XL::XType *>(src_type.Inner())->TransformTo(instance, dest_type, true);
		}
		XL::LObject * CreateDefinitionNamespace(XL::LContext & ctx, const Volumes::Dictionary<string, string> & dict) { return new VNamespace(ctx, dict); }

		void EncodeSelectorRetval(XA::Function & xa, int rv, const XA::ArgumentSpecification & rv_spec)
		{
			auto rreg = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceRetVal));
			auto sreg = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceData, rv));
			if (rv_spec.size.num_words) sreg = XL::MakeAddressOf(sreg, rv_spec.size);
			auto blt = XL::MakeBlt(rreg, sreg, rv_spec.size);
			xa.instset << XA::TH::MakeStatementReturn(blt);
		}
		void EncodeSelectorRange(XA::Function & xa, const XA::ExpressionTree & subj, const XA::ArgumentSpecification & subj_spec,
			const Volumes::KeyValuePair<int, int> * first, int length, int dropback, const XA::ArgumentSpecification & rv_spec)
		{
			if (length == 1) {
				auto cmp = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformIntegerNEQ, XA::ReferenceFlagInvoke));
				XA::TH::AddTreeInput(cmp, subj, subj_spec);
				XA::TH::AddTreeInput(cmp, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceData, first[0].key)), subj_spec);
				XA::TH::AddTreeOutput(cmp, subj_spec);
				xa.instset << XA::TH::MakeStatementJump(1, cmp);
				EncodeSelectorRetval(xa, first[0].value, rv_spec);
				EncodeSelectorRetval(xa, dropback, rv_spec);
			} else if (length > 1) {
				int med = length / 2;
				auto cmp = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformIntegerUGE, XA::ReferenceFlagInvoke));
				XA::TH::AddTreeInput(cmp, subj, subj_spec);
				XA::TH::AddTreeInput(cmp, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceData, first[med].key)), subj_spec);
				XA::TH::AddTreeOutput(cmp, subj_spec);
				int ip_jge = xa.instset.Length();
				xa.instset << XA::TH::MakeStatementJump(0, cmp);
				EncodeSelectorRange(xa, subj, subj_spec, first, med, dropback, rv_spec);
				xa.instset[ip_jge].attachment.num_bytes = xa.instset.Length() - ip_jge - 1;
				EncodeSelectorRange(xa, subj, subj_spec, first + med, length - med, dropback, rv_spec);
			} else EncodeSelectorRetval(xa, dropback, rv_spec);
		}
		void EncodeSelector(XA::Function & xa, const XA::ExpressionTree & subj, const XA::ArgumentSpecification & subj_spec,
			const Volumes::Dictionary<uint64, Volumes::KeyValuePair<int, int> > & mapping, int dropback, const XA::ArgumentSpecification & rv_spec)
		{
			Array< Volumes::KeyValuePair<int, int> > linear(mapping.Count());
			for (auto & m : mapping) linear.Append(m.value);
			EncodeSelectorRange(xa, subj, subj_spec, linear.GetBuffer(), linear.Length(), dropback, rv_spec);
		}
		void CreateEnumerationConstructInit(XL::XClass * enum_cls, XL::XClass * base)
		{
			auto & ctx = enum_cls->GetContext();
			try {
				SafePointer<XL::LObject> type_void = ctx.QueryObject(XL::NameVoid);
				auto fd = ctx.CreateFunction(enum_cls, XL::NameConstructor);
				auto func = ctx.CreateFunctionOverload(fd, type_void, 1, reinterpret_cast<XL::LObject **>(&base), XL::FunctionMethod | XL::FunctionThisCall);
				auto & impl = static_cast<XL::XFunctionOverload *>(func)->GetImplementationDesc();
				auto & xa = impl._xa;
				auto spec = enum_cls->GetArgumentSpecification();
				auto self = XL::MakeAddressFollow(XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceArgument, 0)), spec.size);
				auto blt = XL::MakeBlt(self, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceArgument, 1)), spec.size);
				if (!ctx.IsIdle()) xa.instset << XA::TH::MakeStatementReturn(blt);
			} catch (...) {}
			try {
				SafePointer<XL::LObject> type_ref = ctx.QueryTypeReference(enum_cls);
				auto fd = ctx.CreateFunction(enum_cls, XL::OperatorAssign);
				auto func = ctx.CreateFunctionOverload(fd, type_ref, 1, reinterpret_cast<XL::LObject **>(&base), XL::FunctionMethod | XL::FunctionThisCall);
				auto & impl = static_cast<XL::XFunctionOverload *>(func)->GetImplementationDesc();
				auto & xa = impl._xa;
				auto spec = enum_cls->GetArgumentSpecification();
				auto self = XL::MakeAddressFollow(XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceArgument, 0)), spec.size);
				auto blt = XL::MakeBlt(self, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceArgument, 1)), spec.size);
				if (!ctx.IsIdle()) xa.instset << XA::TH::MakeStatementExpression(blt);
				auto ret = XL::MakeBlt(XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceRetVal)), XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceArgument, 0)), XA::TH::MakeSize(0, 1));
				if (!ctx.IsIdle()) xa.instset << XA::TH::MakeStatementReturn(ret);
			} catch (...) {}
		}
		void CreateEnumerationToString(Volumes::ObjectDictionary<string, XL::LObject> & db, XL::XClass * enum_cls)
		{
			string selector_name = L"_linea";
			auto & ctx = enum_cls->GetContext();
			try {
				SafePointer<XL::XType> type_string_literal = XL::CreateType(XI::Module::TypeReference::MakePointer(XI::Module::TypeReference::MakeClassReference(XL::NameUInt32)), ctx);
				auto fd = ctx.CreateFunction(enum_cls, selector_name);
				auto func = ctx.CreateFunctionOverload(fd, type_string_literal, 0, 0, XL::FunctionMethod | XL::FunctionThisCall);
				if (!ctx.IsIdle()) {
					auto & impl = static_cast<XL::XFunctionOverload *>(func)->GetImplementationDesc();
					auto & xa = impl._xa;
					auto spec = enum_cls->GetArgumentSpecification();
					auto ptr_spec = XA::TH::MakeSpec(0, 1);
					auto self = XL::MakeAddressFollow(XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceArgument, 0)), spec.size);
					Volumes::Dictionary<uint64, Volumes::KeyValuePair<int, int> > mapping;
					for (auto & lit : db) {
						auto & xlit = static_cast<XL::XLiteral *>(lit.value.Inner())->Expose();
						int value_offset = xa.data.Length();
						xa.data.SetLength(value_offset + 8);
						MemoryCopy(xa.data.GetBuffer() + value_offset, &xlit.data_uint64, 8);
						int text_offset = xa.data.Length();
						SafePointer<DataBlock> text = lit.value->GetFullName().EncodeSequence(Encoding::UTF32, true);
						while (text->Length() & 0x7) text->Append(0);
						xa.data << *text;
						mapping.Append(xlit.data_uint64, Volumes::KeyValuePair<int, int>(value_offset, text_offset));
					}
					int dropback_offset = xa.data.Length();
					xa.data << L'?';
					while (xa.data.Length() & 0x7) xa.data << 0;
					EncodeSelector(xa, self, spec, mapping, dropback_offset, ptr_spec);
				}
			} catch (...) {}
			try {
				SafePointer<XL::LObject> type_string = ctx.QueryObject(L"linea");
				auto fd = ctx.CreateFunction(enum_cls, XL::NameConverter);
				auto func = ctx.CreateFunctionOverload(fd, type_string, 0, 0, XL::FunctionMethod | XL::FunctionThisCall | XL::FunctionThrows);
				if (!ctx.IsIdle()) {
					XL::FunctionContextDesc desc;
					desc.retval = type_string;
					desc.instance = enum_cls;
					desc.argc = 0; desc.argvt = 0; desc.argvn = 0;
					desc.flags = XL::FunctionMethod | XL::FunctionThisCall | XL::FunctionThrows;
					desc.vft_init = 0; desc.vft_init_seq = 0; desc.init_callback = 0;
					desc.create_init_sequence = desc.create_shutdown_sequence = false;
					SafePointer<XL::LFunctionContext> fctx = new XL::LFunctionContext(ctx, func, desc);
					SafePointer<XL::LObject> selector = fctx->GetInstance()->GetMember(selector_name);
					SafePointer<XL::LObject> ret = selector->Invoke(0, 0);
					fctx->EncodeReturn(ret);
					fctx->EndEncoding();
				}
			} catch (...) {}
		}
		void CreateEnumerationIterations(Volumes::ObjectDictionary<string, XL::LObject> & db, XL::XClass * enum_cls)
		{
			uint64 neutal = -1;
			string selector_next_name = L"_proximus";
			string selector_prev_name = L"_pristinus";
			auto & ctx = enum_cls->GetContext();
			auto base = enum_cls->GetParentClass();
			while (true) {
				bool ok = true;
				for (auto & e : db) {
					auto & xlit = static_cast<XL::XLiteral *>(e.value.Inner())->Expose();
					if (MemoryCompare(&xlit.data_uint64, &neutal, xlit.length) == 0) { ok = false; break; }
				}
				if (ok) break;
				neutal--;
			}
			try {
				auto fd = ctx.CreateFunction(enum_cls, selector_next_name);
				auto func = ctx.CreateFunctionOverload(fd, enum_cls, 0, 0, XL::FunctionMethod | XL::FunctionThisCall);
				auto & impl = static_cast<XL::XFunctionOverload *>(func)->GetImplementationDesc();
				if (!ctx.IsIdle()) {
					auto & xa = impl._xa;
					auto spec = enum_cls->GetArgumentSpecification();
					auto self = XL::MakeAddressFollow(XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceArgument, 0)), spec.size);
					Volumes::Dictionary<uint64, Volumes::KeyValuePair<int, int> > mapping;
					auto current = db.GetFirst();
					while (current) {
						auto next = current->GetNext();
						auto & xlit = static_cast<XL::XLiteral *>(current->GetValue().value.Inner())->Expose();
						int value_offset = xa.data.Length();
						int subst_offset = xa.data.Length() + 8;
						xa.data.SetLength(value_offset + 16);
						MemoryCopy(xa.data.GetBuffer() + value_offset, &xlit.data_uint64, 8);
						if (next) {
							auto & xlit_nx = static_cast<XL::XLiteral *>(next->GetValue().value.Inner())->Expose();
							MemoryCopy(xa.data.GetBuffer() + subst_offset, &xlit_nx.data_uint64, 8);
						} else MemoryCopy(xa.data.GetBuffer() + subst_offset, &neutal, 8);
						mapping.Append(xlit.data_uint64, Volumes::KeyValuePair<int, int>(value_offset, subst_offset));
						current = next;
					}
					int dropback_offset = xa.data.Length();
					xa.data.SetLength(dropback_offset + 8);
					MemoryCopy(xa.data.GetBuffer() + dropback_offset, &neutal, 8);
					EncodeSelector(xa, self, spec, mapping, dropback_offset, spec);
				}
			} catch (...) {}
			try {
				auto fd = ctx.CreateFunction(enum_cls, selector_prev_name);
				auto func = ctx.CreateFunctionOverload(fd, enum_cls, 0, 0, XL::FunctionMethod | XL::FunctionThisCall);
				auto & impl = static_cast<XL::XFunctionOverload *>(func)->GetImplementationDesc();
				if (!ctx.IsIdle()) {
					auto & xa = impl._xa;
					auto spec = enum_cls->GetArgumentSpecification();
					auto self = XL::MakeAddressFollow(XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceArgument, 0)), spec.size);
					Volumes::Dictionary<uint64, Volumes::KeyValuePair<int, int> > mapping;
					auto current = db.GetLast();
					while (current) {
						auto next = current->GetPrevious();
						auto & xlit = static_cast<XL::XLiteral *>(current->GetValue().value.Inner())->Expose();
						int value_offset = xa.data.Length();
						int subst_offset = xa.data.Length() + 8;
						xa.data.SetLength(value_offset + 16);
						MemoryCopy(xa.data.GetBuffer() + value_offset, &xlit.data_uint64, 8);
						if (next) {
							auto & xlit_nx = static_cast<XL::XLiteral *>(next->GetValue().value.Inner())->Expose();
							MemoryCopy(xa.data.GetBuffer() + subst_offset, &xlit_nx.data_uint64, 8);
						} else MemoryCopy(xa.data.GetBuffer() + subst_offset, &neutal, 8);
						mapping.Append(xlit.data_uint64, Volumes::KeyValuePair<int, int>(value_offset, subst_offset));
						current = next;
					}
					int dropback_offset = xa.data.Length();
					xa.data.SetLength(dropback_offset + 8);
					MemoryCopy(xa.data.GetBuffer() + dropback_offset, &neutal, 8);
					EncodeSelector(xa, self, spec, mapping, dropback_offset, spec);
				}
			} catch (...) {}
			try {
				SafePointer<XL::LObject> type_ref = ctx.QueryTypeReference(enum_cls);
				auto fd = ctx.CreateFunction(enum_cls, XL::OperatorIncrement);
				auto func = ctx.CreateFunctionOverload(fd, type_ref, 0, 0, XL::FunctionMethod | XL::FunctionThisCall);
				XL::FunctionContextDesc desc;
				desc.retval = type_ref;
				desc.instance = enum_cls;
				desc.argc = 0; desc.argvt = 0; desc.argvn = 0;
				desc.flags = XL::FunctionMethod | XL::FunctionThisCall;
				desc.vft_init = 0; desc.vft_init_seq = 0; desc.init_callback = 0;
				desc.create_init_sequence = desc.create_shutdown_sequence = false;
				SafePointer<XL::LFunctionContext> fctx = new XL::LFunctionContext(ctx, func, desc);
				SafePointer<XL::LObject> selector = fctx->GetInstance()->GetMember(selector_next_name);
				SafePointer<XL::LObject> selector_inv = selector->Invoke(0, 0);
				SafePointer<XL::LObject> asgn = fctx->GetInstance()->GetMember(XL::OperatorAssign);
				SafePointer<XL::LObject> expr = asgn->Invoke(1, selector_inv.InnerRef());
				fctx->EncodeExpression(expr);
				fctx->EncodeReturn(fctx->GetInstance());
				fctx->EndEncoding();
			} catch (...) {}
			try {
				SafePointer<XL::LObject> type_ref = ctx.QueryTypeReference(enum_cls);
				auto fd = ctx.CreateFunction(enum_cls, XL::OperatorDecrement);
				auto func = ctx.CreateFunctionOverload(fd, type_ref, 0, 0, XL::FunctionMethod | XL::FunctionThisCall);
				XL::FunctionContextDesc desc;
				desc.retval = type_ref;
				desc.instance = enum_cls;
				desc.argc = 0; desc.argvt = 0; desc.argvn = 0;
				desc.flags = XL::FunctionMethod | XL::FunctionThisCall;
				desc.vft_init = 0; desc.vft_init_seq = 0; desc.init_callback = 0;
				desc.create_init_sequence = desc.create_shutdown_sequence = false;
				SafePointer<XL::LFunctionContext> fctx = new XL::LFunctionContext(ctx, func, desc);
				SafePointer<XL::LObject> selector = fctx->GetInstance()->GetMember(selector_prev_name);
				SafePointer<XL::LObject> selector_inv = selector->Invoke(0, 0);
				SafePointer<XL::LObject> asgn = fctx->GetInstance()->GetMember(XL::OperatorAssign);
				SafePointer<XL::LObject> expr = asgn->Invoke(1, selector_inv.InnerRef());
				fctx->EncodeExpression(expr);
				fctx->EncodeReturn(fctx->GetInstance());
				fctx->EndEncoding();
			} catch (...) {}
			try {
				auto fd = ctx.CreateFunction(enum_cls, XL::IteratorBegin);
				auto func = ctx.CreateFunctionOverload(fd, enum_cls, 0, 0, 0);
				XL::FunctionContextDesc desc;
				desc.retval = enum_cls;
				desc.instance = 0; desc.argc = 0; desc.argvt = 0; desc.argvn = 0; desc.flags = 0;
				desc.vft_init = 0; desc.vft_init_seq = 0; desc.init_callback = 0;
				desc.create_init_sequence = desc.create_shutdown_sequence = false;
				SafePointer<XL::LFunctionContext> fctx = new XL::LFunctionContext(ctx, func, desc);
				auto first = db.GetFirst();
				SafePointer<XL::LObject> rv;
				if (!first) {
					rv = XL::CreateLiteral(ctx, neutal);
					rv = XL::PerformTypeCast(base, rv, XL::CastPriorityConverter);
				} else rv = first->GetValue().value;
				fctx->EncodeReturn(rv);
				fctx->EndEncoding();
			} catch (...) {}
			try {
				auto fd = ctx.CreateFunction(enum_cls, XL::IteratorEnd);
				auto func = ctx.CreateFunctionOverload(fd, enum_cls, 0, 0, 0);
				XL::FunctionContextDesc desc;
				desc.retval = enum_cls;
				desc.instance = 0; desc.argc = 0; desc.argvt = 0; desc.argvn = 0; desc.flags = 0;
				desc.vft_init = 0; desc.vft_init_seq = 0; desc.init_callback = 0;
				desc.create_init_sequence = desc.create_shutdown_sequence = false;
				SafePointer<XL::LFunctionContext> fctx = new XL::LFunctionContext(ctx, func, desc);
				auto first = db.GetLast();
				SafePointer<XL::LObject> rv;
				if (!first) {
					rv = XL::CreateLiteral(ctx, neutal);
					rv = XL::PerformTypeCast(base, rv, XL::CastPriorityConverter);
				} else rv = first->GetValue().value;
				fctx->EncodeReturn(rv);
				fctx->EndEncoding();
			} catch (...) {}
			try {
				auto fd = ctx.CreateFunction(enum_cls, XL::IteratorPreBegin);
				auto func = ctx.CreateFunctionOverload(fd, enum_cls, 0, 0, 0);
				XL::FunctionContextDesc desc;
				desc.retval = enum_cls;
				desc.instance = 0; desc.argc = 0; desc.argvt = 0; desc.argvn = 0; desc.flags = 0;
				desc.vft_init = 0; desc.vft_init_seq = 0; desc.init_callback = 0;
				desc.create_init_sequence = desc.create_shutdown_sequence = false;
				SafePointer<XL::LFunctionContext> fctx = new XL::LFunctionContext(ctx, func, desc);
				SafePointer<XL::LObject> rv = XL::CreateLiteral(ctx, neutal);
				rv = XL::PerformTypeCast(base, rv, XL::CastPriorityConverter);
				fctx->EncodeReturn(rv);
				fctx->EndEncoding();
			} catch (...) {}
			try {
				auto fd = ctx.CreateFunction(enum_cls, XL::IteratorPostEnd);
				auto func = ctx.CreateFunctionOverload(fd, enum_cls, 0, 0, 0);
				XL::FunctionContextDesc desc;
				desc.retval = enum_cls;
				desc.instance = 0; desc.argc = 0; desc.argvt = 0; desc.argvn = 0; desc.flags = 0;
				desc.vft_init = 0; desc.vft_init_seq = 0; desc.init_callback = 0;
				desc.create_init_sequence = desc.create_shutdown_sequence = false;
				SafePointer<XL::LFunctionContext> fctx = new XL::LFunctionContext(ctx, func, desc);
				SafePointer<XL::LObject> rv = XL::CreateLiteral(ctx, neutal);
				rv = XL::PerformTypeCast(base, rv, XL::CastPriorityConverter);
				fctx->EncodeReturn(rv);
				fctx->EndEncoding();
			} catch (...) {}
		}

		bool ClassImplements(XL::LObject * cls, const string & impl)
		{
			if (cls->GetClass() != XL::Class::Type) return false;
			ObjectArray<XL::XType> conf(0x10);
			static_cast<XL::XType *>(cls)->GetTypesConformsTo(conf);
			for (auto & c : conf) if (c.GetFullName() == impl) return true;
			return false;
		}
		bool IsValidEnumerationBase(XL::LObject * type)
		{
			if (type->GetClass() != XL::Class::Type) return false;
			if (static_cast<XL::XType *>(type)->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) return false;
			auto xcls = static_cast<XL::XClass *>(type);
			auto spec = xcls->GetArgumentSpecification();
			if (xcls->GetLanguageSemantics() != XI::Module::Class::Nature::Core) return false;
			if (spec.semantics != XA::ArgumentSemantics::Integer && spec.semantics != XA::ArgumentSemantics::SignedInteger) return false;
			if (spec.size.num_words) return false;
			if (spec.size.num_bytes != 1 && spec.size.num_bytes != 2 && spec.size.num_bytes != 4 && spec.size.num_bytes != 8) return false;
			return true;
		}
		bool CreateEnumerationValue(Volumes::ObjectDictionary<string, XL::LObject> & enum_db, XL::LObject * enum_type, const string & name, XL::LObject * value)
		{
			auto base = static_cast<XL::XClass *>(enum_type)->GetParentClass();
			SafePointer<XL::LObject> act_value;
			if (value) {
				act_value = XL::PerformTypeCast(base, value, XL::CastPriorityConverter);
			} else {
				uint64 mx = 0;
				for (auto & o : enum_db) {
					auto & lit = static_cast<XL::XLiteral *>(o.value.Inner())->Expose();
					if (lit.data_uint64 > mx) mx = lit.data_uint64;
				}
				SafePointer<XL::LObject> lit = XL::CreateLiteral(base->GetContext(), mx + 1);
				act_value = XL::PerformTypeCast(base, lit, XL::CastPriorityConverter);
			}
			if (act_value->GetClass() != XL::Class::Literal) return false;
			SafePointer<XL::XLiteral> act_value_copy = static_cast<XL::XLiteral *>(act_value.Inner())->Clone();
			try { base->GetContext().AttachLiteral(act_value_copy, enum_type, name); }
			catch (...) { return false; }
			bool dupl = false;
			for (auto & o : enum_db) {
				auto & lit_new = act_value_copy->Expose();
				auto & lit = static_cast<XL::XLiteral *>(o.value.Inner())->Expose();
				if (lit.data_uint64 == lit_new.data_uint64) { dupl = true; break; }
			}
			if (!dupl) enum_db.Append(name, act_value_copy);
			return true;
		}
		void CreateEnumerationRoutines(Volumes::ObjectDictionary<string, XL::LObject> & enum_db, XL::LObject * enum_type)
		{
			auto xcls = static_cast<XL::XClass *>(enum_type);
			auto xbase = xcls->GetParentClass();
			CreateEnumerationConstructInit(xcls, xbase);
			CreateEnumerationToString(enum_db, xcls);
			CreateEnumerationIterations(enum_db, xcls);
		}
		void CreateTypeServiceRoutines(XL::LObject * cls)
		{
			ObjectArray<XL::XType> conf(0x10);
			auto xcls = static_cast<XL::XClass *>(cls);
			xcls->GetTypesConformsTo(conf);
			bool is_dynamic = false;
			for (auto & c : conf) if (c.GetFullName() == L"dynamicum" || c.GetFullName() == L"objectum_dynamicum") { is_dynamic = true; break; }
			if (!is_dynamic) return;
			auto & ctx = xcls->GetContext();
			SafePointer<XL::LObject> type_void = ctx.QueryObject(XL::NameVoid);
			SafePointer<XL::LObject> type_void_ptr = ctx.QueryTypePointer(type_void);
			try {
				string arg_name = L"cls";
				auto fd = ctx.CreateFunction(cls, L"converte_dynamice");
				auto func = ctx.CreateFunctionOverload(fd, type_void_ptr, 1, type_void_ptr.InnerRef(), XL::FunctionMethod | XL::FunctionThisCall | XL::FunctionThrows | XL::FunctionOverride);
				if (!ctx.IsIdle()) {
					XL::FunctionContextDesc desc;
					desc.retval = type_void_ptr;
					desc.instance = cls; desc.argc = 1;
					desc.argvt = type_void_ptr.InnerRef();
					desc.argvn = &arg_name;
					desc.flags = XL::FunctionMethod | XL::FunctionThisCall | XL::FunctionThrows | XL::FunctionOverride;
					desc.vft_init = 0; desc.vft_init_seq = 0; desc.init_callback = 0;
					desc.create_init_sequence = desc.create_shutdown_sequence = false;
					SafePointer<XL::LFunctionContext> fctx = new XL::LFunctionContext(ctx, func, desc);
					SafePointer<XL::LObject> arg = fctx->GetRootScope()->GetMember(arg_name);
					SafePointer<XL::LObject> equality = ctx.QueryObject(XL::OperatorEqual);
					SafePointer<XL::LObject> make_addr = ctx.QueryAddressOfOperator();
					for (auto & c : conf) if (c.GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Class) {
						ObjectArray<XL::XType> conf2(0x10);
						c.GetTypesConformsTo(conf2);
						bool is_object = false;
						for (auto & c2 : conf2) if (c2.GetFullName() == L"objectum") { is_object = true; break; }
						XL::LObject * argv[2] = { arg, &c };
						SafePointer<XL::LObject> expr_if = equality->Invoke(2, argv);
						XL::LObject * scope;
						fctx->OpenIfBlock(expr_if, &scope);
						SafePointer<XL::LObject> casted = xcls->TransformTo(fctx->GetInstance(), &c, false);
						if (is_object) {
							SafePointer<XL::LObject> retain = fctx->GetInstance()->GetMember(L"contine");
							SafePointer<XL::LObject> retain_inv = retain->Invoke(0, 0);
							fctx->EncodeExpression(retain_inv);
						}
						SafePointer<XL::LObject> addr = make_addr->Invoke(1, casted.InnerRef());
						fctx->EncodeReturn(addr);
						fctx->CloseIfElseBlock();
					}
					try {
						Array<string> expone_vl(0x10);
						SafePointer<XL::LObject> expone = cls->GetMember(L"expone_classem");
						if (expone->GetClass() != XL::Class::Function) throw Exception();
						static_cast<XL::XFunction *>(expone.Inner())->ListOverloads(expone_vl, true);
						for (auto & e : expone_vl) {
							SafePointer< Array<XI::Module::TypeReference> > sgn = XI::Module::TypeReference(e).GetFunctionSignature();
							if (sgn->Length() > 1) continue;
							bool auto_ptr;
							SafePointer<XL::XType> type = XL::CreateType(sgn->ElementAt(0).QueryCanonicalName(), ctx);
							if (type->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Class) {
								auto_ptr = true;
								SafePointer<XL::LObject> decl_type = type->GetMember(L"genus_objectum");
								if (decl_type->GetClass() != XL::Class::Type) continue;
								type.SetRetain(static_cast<XL::XType *>(decl_type.Inner()));
							} else if (type->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Pointer) {
								auto_ptr = false;
								type = static_cast<XL::XPointer *>(type.Inner())->GetElementType();
							} else continue;
							XL::LObject * argv[2] = { arg, type.Inner() };
							SafePointer<XL::LObject> expr_if = equality->Invoke(2, argv);
							XL::LObject * scope;
							fctx->OpenIfBlock(expr_if, &scope);
							SafePointer<XL::LObject> method = static_cast<XL::XFunction *>(expone.Inner())->GetOverloadT(e, true)->SetInstance(fctx->GetInstance());
							SafePointer<XL::LObject> rv_expr = method->Invoke(0, 0);
							if (auto_ptr) {
								SafePointer<XL::LObject> rv_expr_type = rv_expr->GetType();
								SafePointer<XL::LObject> rv = fctx->EncodeCreateVariable(rv_expr_type, rv_expr);
								SafePointer<XL::LObject> follow = rv->GetMember(XL::OperatorFollow);
								SafePointer<XL::LObject> follow_inv = follow->Invoke(0, 0);
								SafePointer<XL::LObject> retain = follow_inv->GetMember(L"contine");
								SafePointer<XL::LObject> retain_inv = retain->Invoke(0, 0);
								fctx->EncodeExpression(retain_inv);
								fctx->EncodeReturn(rv);
							} else fctx->EncodeReturn(rv_expr);
							fctx->CloseIfElseBlock();
						}
					} catch (...) {}
					SafePointer<XL::LObject> not_implemented_error = XL::CreateLiteral(ctx, uint64(1));
					fctx->EncodeThrow(not_implemented_error);
					fctx->EndEncoding();
				}
			} catch (...) {}
			try {
				auto fd = ctx.CreateFunction(cls, L"para_classem");
				auto func = ctx.CreateFunctionOverload(fd, type_void_ptr, 0, 0, XL::FunctionMethod | XL::FunctionThisCall | XL::FunctionOverride);
				if (!ctx.IsIdle()) {
					XL::FunctionContextDesc desc;
					desc.retval = type_void_ptr;
					desc.instance = cls; desc.argc = 0; desc.argvt = 0; desc.argvn = 0;
					desc.flags = XL::FunctionMethod | XL::FunctionThisCall | XL::FunctionOverride;
					desc.vft_init = 0; desc.vft_init_seq = 0; desc.init_callback = 0;
					desc.create_init_sequence = desc.create_shutdown_sequence = false;
					SafePointer<XL::LFunctionContext> fctx = new XL::LFunctionContext(ctx, func, desc);
					fctx->EncodeReturn(cls);
					fctx->EndEncoding();
				}
			} catch (...) {}
		}

		class ContextCaptureProxy : public XL::XObject, XL::IFunctionInitCallback
		{
			struct _object_capture_info {
				SafePointer<XL::LObject> capture_from;
				string field_name;
			};
			struct _object_request_info {
				SafePointer<XL::LObject> object;
				SafePointer<XL::LObject> holder;
				uint object_origin_class; // 0 - static, 1 - capture by copying, 2 - capture field by capturing instance
			};
			
			XL::LContext & _ctx;
			XL::LObject * _cls;
			XL::LFunctionContext * _fctx;
			Array<XL::LObject *> _vlist;
			Volumes::Dictionary<string, _object_capture_info> _capture;
			_object_capture_info _self_capture;
			bool _self_capture_object;
			int _counter;

			static bool _is_object_static(XL::LObject * object)
			{
				auto oc = object->GetClass();
				if (oc == XL::Class::Method) return false;
				if (oc == XL::Class::MethodOverload) return false;
				if (oc == XL::Class::Variable) return false;
				if (oc == XL::Class::InstancedProperty) return false;
				return true;
			}
			static bool _is_object_capture_proxy(XL::LObject * object) { return object->GetClass() == XL::Class::Internal && object->ToString() == L"VContextCaptureProxy"; }
			void _get_member_ex(const string & name, _object_request_info & info)
			{
				try {
					SafePointer<XL::LObject> object = _fctx->GetInstance()->GetMember(Lexic::IdentifierTVPP + name);
					info.object = object;
					info.holder.SetRetain(_fctx->GetInstance());
					info.object_origin_class = 1;
					return;
				} catch (...) {}
				auto cpt = _capture.GetElementByKey(name);
				if (cpt) {
					info.object = _fctx->GetInstance()->GetMember(cpt->field_name);
					info.holder.SetRetain(_fctx->GetInstance());
					info.object_origin_class = 1;
					return;
				}
				for (auto & v : _vlist) {
					try {
						_object_request_info intinfo;
						if (_is_object_capture_proxy(v)) {
							static_cast<ContextCaptureProxy *>(v)->_get_member_ex(name, intinfo);
						} else {
							intinfo.object = v->GetMember(name);
							intinfo.holder.SetRetain(v);
							if (v->GetClass() == XL::Class::Namespace || v->GetClass() == XL::Class::Type || _is_object_static(intinfo.object)) {
								intinfo.object_origin_class = 0;
							} else if (v->GetClass() == XL::Class::Scope) {
								intinfo.object_origin_class = 1;
							} else {
								intinfo.object_origin_class = 2;
							}
						}
						if (intinfo.object_origin_class == 0) {
							info = intinfo;
							return;
						} else if (intinfo.object_origin_class == 1) {
							_object_capture_info cptinfo;
							cptinfo.capture_from = intinfo.object;
							cptinfo.field_name = L"_@" + name;
							_capture.Append(name, cptinfo);
							SafePointer<XL::LObject> type = intinfo.object->GetType();
							_ctx.CreateField(_cls, cptinfo.field_name, type, true);
							info.object = _fctx->GetInstance()->GetMember(cptinfo.field_name);
							info.holder.SetRetain(_fctx->GetInstance());
							info.object_origin_class = 1;
							return;
						} else if (intinfo.object_origin_class == 2) {		
							if (!_self_capture.capture_from) {
								_self_capture.capture_from = intinfo.holder;
								_self_capture.field_name = L"_@@ego";
								SafePointer<XL::LObject> base_type = intinfo.holder->GetType();
								SafePointer<XL::LObject> type;
								if (ClassImplements(base_type, L"objectum")) {
									SafePointer<XL::LObject> safe_ptr = _ctx.QueryObject(L"adl");
									SafePointer<XL::LObject> safe_ptr_inst_op = safe_ptr->GetMember(XL::OperatorSubscript);
									type = safe_ptr_inst_op->Invoke(1, base_type.InnerRef());
									_self_capture_object = true;
								} else {
									type = _ctx.QueryTypePointer(base_type);
									_self_capture_object = false;
								}
								_ctx.CreateField(_cls, _self_capture.field_name, type, true);
							}
							SafePointer<XL::LObject> self_ptr = _fctx->GetInstance()->GetMember(_self_capture.field_name);
							SafePointer<XL::LObject> self_ptr_follow = self_ptr->GetMember(XL::OperatorFollow);
							SafePointer<XL::LObject> self = self_ptr_follow->Invoke(0, 0);
							info.object = self->GetMember(name);
							info.holder = self;
							info.object_origin_class = 2;
							return;
						}
					} catch (...) {}
				}
				throw XL::ObjectHasNoSuchMemberException(this, name);
			}
		public:
			ContextCaptureProxy(XL::LContext & ctx, XL::LObject * cls, XL::LObject ** vlist, int vlen) : _ctx(ctx), _cls(cls), _vlist(0x10) { _counter = 0; _vlist.Append(vlist, vlen); }
			virtual ~ContextCaptureProxy(void) override {}
			virtual string ToString(void) const override { return L"VContextCaptureProxy"; }
			virtual XL::Class GetClass(void) override { return XL::Class::Internal; }
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetType(void) override { throw XL::ObjectHasNoTypeException(this); }
			virtual LObject * GetMember(const string & name) override { _object_request_info info; _get_member_ex(name, info); info.object->Retain(); return info.object; }
			virtual void ListMembers(Volumes::Dictionary<string, XL::Class> & list) override
			{
				Volumes::Dictionary<string, XL::Class> il;
				_fctx->GetInstance()->ListMembers(il);
				for (auto & m : il) if (m.key.Fragment(0, string(Lexic::IdentifierTVPP).Length()) == Lexic::IdentifierTVPP) {
					list.Append(m.key.Fragment(string(Lexic::IdentifierTVPP).Length(), -1), m.value);
				}
				for (auto & v : _vlist) v->ListMembers(list);
			}
			virtual LObject * Invoke(int argc, LObject ** argv, LObject ** actual) override { throw XL::ObjectHasNoSuchOverloadException(this, argc, argv); }
			virtual void ListInvokations(XL::LObject * first, Volumes::List<XL::InvokationDesc> & list) override {}
			virtual void AddMember(const string & name, LObject * child) override { throw XL::LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw XL::ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw XL::ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, XL::Class parent) override {}
			XL::LContext & GetContext(void) { return _ctx; }
			XL::LObject * GetClassObject(void) { return _cls; }
			void SetFunctionContext(XL::LFunctionContext * fctx) { _fctx = fctx; }
			XL::LObject * CreateInstance(void)
			{
				Array<XL::LObject *> argv(0x20);
				if (_self_capture.capture_from) argv.Append(_self_capture.capture_from);
				for (auto & c : _capture) argv.Append(c.value.capture_from);
				return CreateNew(_ctx, _cls, argv.Length(), argv.GetBuffer());
			}
			XL::LObject * RaiseSignal(XL::LFunctionContext * fctx)
			{
				SafePointer<XL::LObject> field = fctx->GetInstance()->GetMember(L"_@@signale");
				SafePointer<XL::LObject> field_follow = field->GetMember(XL::OperatorFollow);
				SafePointer<XL::LObject> signal = field_follow->Invoke(0, 0);
				SafePointer<XL::LObject> raise = signal->GetMember(L"erige");
				return raise->Invoke(0, 0);
			}
			void CreateConstructor(ObjectArray<XL::LObject> & vft_init)
			{
				SafePointer<XL::LObject> type_void = _ctx.QueryObject(XL::NameVoid);
				auto fd = _ctx.CreateFunction(_cls, XL::NameConstructor);
				ObjectArray<XL::LObject> retain(0x20);
				Array<XL::LObject *> argv(0x20);
				Array<string> names(0x20);
				if (_self_capture.capture_from) {
					SafePointer<XL::LObject> type = _self_capture.capture_from->GetType();
					SafePointer<XL::LObject> type_ref = _ctx.QueryTypeReference(type);
					retain.Append(type_ref); argv.Append(type_ref);
					names.Append(L"@A0");
				}
				int counter = 0;
				for (auto & c : _capture) {
					counter++;
					SafePointer<XL::LObject> type = c.value.capture_from->GetType();
					SafePointer<XL::LObject> type_ref = _ctx.QueryTypeReference(type);
					retain.Append(type_ref); argv.Append(type_ref);
					names.Append(L"@A" + string(counter));
				}
				auto func = _ctx.CreateFunctionOverload(fd, type_void, argv.Length(), argv.GetBuffer(), XL::FunctionMethod | XL::FunctionThisCall | XL::FunctionThrows);
				XL::FunctionContextDesc desc;
				desc.retval = type_void; desc.instance = _cls;
				desc.argc = argv.Length(); desc.argvt = argv.GetBuffer(); desc.argvn = names.GetBuffer();
				desc.flags = XL::FunctionMethod | XL::FunctionThisCall | XL::FunctionThrows;
				desc.vft_init = _cls; desc.vft_init_seq = &vft_init;
				desc.create_init_sequence = true; desc.create_shutdown_sequence = false;
				desc.init_callback = this;
				SafePointer<XL::LFunctionContext> fctx = new XL::LFunctionContext(_ctx, func, desc);
				if (_self_capture.capture_from) {
					SafePointer<XL::LObject> self_ptr = fctx->GetInstance()->GetMember(_self_capture.field_name);
					SafePointer<XL::LObject> self_src = fctx->GetRootScope()->GetMember(L"@A0");
					SafePointer<XL::LObject> operator_address_of = _ctx.QueryAddressOfOperator();
					self_src = operator_address_of->Invoke(1, self_src.InnerRef());
					if (_self_capture_object) {
						SafePointer<XL::LObject> self_asgn = self_ptr->GetMember(L"contine");
						SafePointer<XL::LObject> expr = self_asgn->Invoke(1, self_src.InnerRef());
						fctx->EncodeExpression(expr);
					} else {
						SafePointer<XL::LObject> self_asgn = self_ptr->GetMember(XL::OperatorAssign);
						SafePointer<XL::LObject> expr = self_asgn->Invoke(1, self_src.InnerRef());
						fctx->EncodeExpression(expr);
					}
				}
				fctx->EndEncoding();
			}
			virtual void GetNextInit(LObject * arguments_scope, XL::FunctionInitDesc & desc) override
			{
				desc.init.Clear();
				auto element = _capture.ElementAt(_counter);
				if (element) {
					_counter++;
					SafePointer<XL::LObject> field = _cls->GetMember(element->GetValue().value.field_name);
					SafePointer<XL::LObject> from = arguments_scope->GetMember(L"@A" + string(_counter));
					desc.subject = field;
					desc.init.InsertLast(from);
				} else desc.subject = 0;
			}
		};
		void BeginContextCapture(XL::LObject * base_class, XL::LObject ** vlist, int vlen, XL::LObject ** capture, XL::LObject ** function)
		{
			auto xbase = static_cast<XL::XType *>(base_class);
			auto & ctx = xbase->GetContext();
			auto cls = ctx.CreatePrivateClass();
			ctx.AdoptParentClass(cls, base_class);
			*capture = new ContextCaptureProxy(ctx, cls, vlist, vlen);
			SafePointer<XL::LObject> void_type = ctx.QueryObject(XL::NameVoid);
			auto fd = ctx.CreateFunction(cls, L"_exeque");
			auto func = ctx.CreateFunctionOverload(fd, void_type, 0, 0, XL::FunctionMethod | XL::FunctionThisCall);
			func->Retain();
			*function = func;
		}
		void ConfigureContextCapture(XL::LObject * capture, XL::LObject * function, XL::LFunctionContext ** fctx)
		{
			auto & ctx = static_cast<ContextCaptureProxy *>(capture)->GetContext();
			SafePointer<XL::LObject> void_type = ctx.QueryObject(XL::NameVoid);
			XL::FunctionContextDesc desc;
			desc.retval = void_type;
			desc.instance = static_cast<ContextCaptureProxy *>(capture)->GetClassObject();
			desc.argc = 0; desc.argvt = 0; desc.argvn = 0;
			desc.flags = XL::FunctionMethod | XL::FunctionThisCall;
			desc.vft_init = 0; desc.vft_init_seq = 0;
			desc.create_init_sequence = desc.create_shutdown_sequence = false;
			desc.init_callback = 0;
			*fctx = new XL::LFunctionContext(ctx, function, desc);
			static_cast<ContextCaptureProxy *>(capture)->SetFunctionContext(*fctx);
		}
		void EndContextCapture(XL::LObject * capture, ObjectArray<XL::LObject> & vft_init, XL::LObject ** task)
		{
			auto xcapt = static_cast<ContextCaptureProxy *>(capture);
			auto & ctx = xcapt->GetContext();
			auto cls = xcapt->GetClassObject();
			ctx.CreateClassDefaultMethods(xcapt->GetClassObject(), XL::CreateMethodDestructor, vft_init);
			try {
				string name = L"";
				SafePointer<XL::LObject> void_type = ctx.QueryObject(XL::NameVoid);
				SafePointer<XL::LObject> object_type = ctx.QueryObject(L"objectum");
				SafePointer<XL::LObject> object_type_ptr = ctx.QueryTypePointer(object_type);
				auto fd = ctx.CreateFunction(cls, L"exeque");
				auto func = ctx.CreateFunctionOverload(fd, void_type, 1, object_type_ptr.InnerRef(), XL::FunctionMethod | XL::FunctionThisCall | XL::FunctionOverride);
				XL::FunctionContextDesc desc;
				desc.retval = void_type; desc.instance = cls;
				desc.argc = 1; desc.argvt = object_type_ptr.InnerRef(); desc.argvn = &name;
				desc.flags = XL::FunctionMethod | XL::FunctionThisCall | XL::FunctionOverride;
				desc.vft_init = 0; desc.vft_init_seq = 0; desc.init_callback = 0;
				desc.create_init_sequence = desc.create_shutdown_sequence = false;
				SafePointer<XL::LFunctionContext> fctx = new XL::LFunctionContext(ctx, func, desc);
				SafePointer<XL::LObject> delegate = fctx->GetInstance()->GetMember(L"_exeque");
				SafePointer<XL::LObject> delegate_inv = delegate->Invoke(0, 0);
				fctx->EncodeExpression(delegate_inv);
				fctx->EndEncoding();
			} catch (...) { throw; }
			xcapt->CreateConstructor(vft_init);
			*task = xcapt->CreateInstance();
		}

		string RegularizeTypeName(const XI::Module::TypeReference & tr);
		string RegularizeObjectName(const string & name)
		{
			string n;
			if (name.Fragment(0, 12) == L"@praeformae.") {
				int i = 12, l = 0;
				while (i < name.Length()) {
					if (name[i] == L'(') l++;
					else if (name[i] == L')') l--;
					else if (name[i] == L'.' && l == 0) break;
					i++;
				}
				auto pcn = name.Fragment(12, i - 12);
				auto ref = XI::Module::TypeReference(pcn);
				Array<string> args_inv(0x10);
				DynamicString base;
				while (ref.GetReferenceClass() == XI::Module::TypeReference::Class::AbstractInstance) {
					auto param = ref.GetAbstractInstanceParameterType();
					auto base = ref.GetAbstractInstanceBase();
					args_inv << RegularizeTypeName(param);
					MemoryCopy(&ref, &base, sizeof(base));
				}
				base += RegularizeTypeName(ref) + L"[";
				for (int i = args_inv.Length() - 1; i >= 0; i--) {
					base += args_inv[i];
					if (i) base += L", ";
				}
				base += "]";
				n = base.ToString() + name.Fragment(i, -1);
			} else n = name;
			int index = n.FindFirst(L':');
			return n.Fragment(0, index);
		}
		string RegularizeTypeName(const XI::Module::TypeReference & tr)
		{
			if (tr.GetReferenceClass() == XI::Module::TypeReference::Class::Class) {
				return RegularizeObjectName(tr.GetClassName());
			} else if (tr.GetReferenceClass() == XI::Module::TypeReference::Class::Array) {
				return L"ordo [" + string(tr.GetArrayVolume()) + L"] " + RegularizeTypeName(tr.GetArrayElement());
			} else if (tr.GetReferenceClass() == XI::Module::TypeReference::Class::Pointer) {
				return L"@" + RegularizeTypeName(tr.GetPointerDestination());
			} else if (tr.GetReferenceClass() == XI::Module::TypeReference::Class::Reference) {
				return L"~" + RegularizeTypeName(tr.GetReferenceDestination());
			} else if (tr.GetReferenceClass() == XI::Module::TypeReference::Class::Function) {
				SafePointer< Array<XI::Module::TypeReference> > sgn = tr.GetFunctionSignature();
				DynamicString result;
				result += L"functio (" + RegularizeTypeName(sgn->ElementAt(0)) + L") (";
				for (int i = 1; i < sgn->Length(); i++) {
					if (i > 1) result += L", ";
					result += RegularizeTypeName(sgn->ElementAt(i));
				}
				result += L")";
				return result.ToString();
			} else return L"";
		}
		string RegularizeCanonicalName(const XI::Module::TypeReference & tr)
		{
			if (tr.GetReferenceClass() == XI::Module::TypeReference::Class::Class) {
				auto clsname = tr.GetClassName();
				if (clsname.Fragment(0, 12) == L"@praeformae.") {
					int i = 12, l = 0;
					while (i < clsname.Length()) {
						if (clsname[i] == L'(') l++;
						else if (clsname[i] == L')') l--;
						else if (clsname[i] == L'.' && l == 0) break;
						i++;
					}
					auto pcn = clsname.Fragment(12, i - 12);
					auto ref = XI::Module::TypeReference(pcn);
					Array<string> args_inv(0x10);
					string base;
					while (ref.GetReferenceClass() == XI::Module::TypeReference::Class::AbstractInstance) {
						auto param = ref.GetAbstractInstanceParameterType();
						auto base = ref.GetAbstractInstanceBase();
						args_inv << RegularizeCanonicalName(param);
						MemoryCopy(&ref, &base, sizeof(base));
					}
					base = RegularizeCanonicalName(ref);
					for (int i = args_inv.Length() - 1; i >= 0; i--) {
						base = XI::Module::TypeReference::MakeInstance(base, args_inv.Length() - 1 - i, args_inv[i]);
					}
					clsname = base + clsname.Fragment(i, -1);
					return clsname;
				} else return XI::Module::TypeReference::MakeClassReference(clsname);
			} else if (tr.GetReferenceClass() == XI::Module::TypeReference::Class::Array) {
				return XI::Module::TypeReference::MakeArray(RegularizeCanonicalName(tr.GetArrayElement()), tr.GetArrayVolume());
			} else if (tr.GetReferenceClass() == XI::Module::TypeReference::Class::Pointer) {
				return XI::Module::TypeReference::MakePointer(RegularizeCanonicalName(tr.GetPointerDestination()));
			} else if (tr.GetReferenceClass() == XI::Module::TypeReference::Class::Reference) {
				return XI::Module::TypeReference::MakeReference(RegularizeCanonicalName(tr.GetReferenceDestination()));
			} else if (tr.GetReferenceClass() == XI::Module::TypeReference::Class::Function) {
				SafePointer< Array<XI::Module::TypeReference> > sgn = tr.GetFunctionSignature();
				Array<string> args(0x10);
				string rv = RegularizeCanonicalName(sgn->ElementAt(0));
				for (int i = 1; i < sgn->Length(); i++) args << RegularizeCanonicalName(sgn->ElementAt(i));
				return XI::Module::TypeReference::MakeFunction(rv, &args);
			} else return L"";
		}
		string GetPurePath(XL::LContext & ctx, XL::LObject * object)
		{
			string name = object->GetFullName();
			return GetPurePath(ctx, name);
		}
		string GetPurePath(XL::LContext & ctx, const string & object)
		{
			string name = object;
			if (name.Fragment(0, 12) == L"@praeformae.") {
				int i = 12, l = 0;
				while (i < name.Length()) {
					if (name[i] == L'(') l++;
					else if (name[i] == L')') l--;
					else if (name[i] == L'.' && l == 0) break;
					i++;
				}
				auto pcn = name.Fragment(12, i - 12);
				auto ref = XI::Module::TypeReference(pcn);
				while (ref.GetReferenceClass() == XI::Module::TypeReference::Class::AbstractInstance) {
					auto base = ref.GetAbstractInstanceBase();
					MemoryCopy(&ref, &base, sizeof(base));
				}
				string base = ref.GetClassName();
				name = base + name.Fragment(i, -1);
				return name.Fragment(0, name.FindFirst(L':'));
			}
			return name;
		}
		string GetObjectFullName(XL::LContext & ctx, XL::LObject * object) { return RegularizeObjectName(object->GetFullName()); }
		string GetTypeCanonicalName(XL::LContext & ctx, XL::LObject * object)
		{
			if (object->GetClass() == XL::Class::Type) {
				auto & tr = static_cast<XL::XType *>(object)->GetTypeReference();
				return RegularizeCanonicalName(tr);
			} else if (object->GetClass() == XL::Class::FunctionOverload || object->GetClass() == XL::Class::MethodOverload) {
				auto path = object->GetFullName();
				auto cn = path.Fragment(path.FindFirst(L':') + 1, -1);
				return RegularizeCanonicalName(XI::Module::TypeReference(cn));
			} else if (object->GetClass() == XL::Class::Function || object->GetClass() == XL::Class::Method) {
				return L"";
			} else {
				SafePointer<XL::LObject> type = object->GetType();
				return GetTypeCanonicalName(ctx, type);
			}
		}
		string GetTypeFullName(XL::LContext & ctx, XL::LObject * object)
		{
			auto & tr = static_cast<XL::XType *>(object)->GetTypeReference();
			return RegularizeTypeName(tr);
		}
		string GetLiteralValue(XL::LContext & ctx, XL::LObject * object)
		{
			auto & lit = static_cast<XL::XLiteral *>(object)->Expose();
			if (lit.contents == XI::Module::Literal::Class::Boolean) {
				if (lit.data_boolean) return Lexic::LiteralYes;
				else return Lexic::LiteralNo;
			} else if (lit.contents == XI::Module::Literal::Class::UnsignedInteger) {
				if (lit.length == 8) return string(lit.data_uint64);
				else if (lit.length == 4) return string(lit.data_uint32);
				else if (lit.length == 2) return string(lit.data_uint16);
				else if (lit.length == 1) return string(lit.data_uint8);
			} else if (lit.contents == XI::Module::Literal::Class::SignedInteger) {
				if (lit.length == 8) return string(lit.data_sint64);
				else if (lit.length == 4) return string(lit.data_sint32);
				else if (lit.length == 2) return string(lit.data_sint16);
				else if (lit.length == 1) return string(lit.data_sint8);
			} else if (lit.contents == XI::Module::Literal::Class::FloatingPoint) {
				if (lit.length == 8) return string(lit.data_double);
				else if (lit.length == 4) return string(lit.data_float);
			} else if (lit.contents == XI::Module::Literal::Class::String) {
				return lit.data_string;
			}
			return L"";
		}
		void GetFields(XL::LObject * cls, Array<string> & names)
		{
			ObjectArray<XL::LObject> result(0x10);
			static_cast<XL::XClass *>(cls)->ListFields(result);
			for (auto & f : result) names.Append(f.GetName());
		}
		bool NameIsPrivate(const string & name)
		{
			auto spl = name.Fragment(0, name.FindFirst(L':')).Split(L'.');
			for (auto & s : spl) if (s[0] == L'_') return true;
			return false;
		}
	}
}