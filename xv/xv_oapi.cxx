#include "xv_oapi.h"

#include "../xlang/xl_types.h"
#include "../xlang/xl_func.h"
#include "../xlang/xl_base.h"
#include "../xlang/xl_code.h"
#include "../xlang/xl_var.h"

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
				xa.instset << XA::TH::MakeStatementReturn(blt);
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
				xa.instset << XA::TH::MakeStatementExpression(blt);
				auto ret = XL::MakeBlt(XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceRetVal)), XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceArgument, 0)), XA::TH::MakeSize(0, 1));
				xa.instset << XA::TH::MakeStatementReturn(ret);
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
			} catch (...) {}
			try {
				SafePointer<XL::LObject> type_string = ctx.QueryObject(L"linea");
				auto fd = ctx.CreateFunction(enum_cls, XL::NameConverter);
				auto func = ctx.CreateFunctionOverload(fd, type_string, 0, 0, XL::FunctionMethod | XL::FunctionThisCall | XL::FunctionThrows);
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
			} catch (...) {}
			try {
				auto fd = ctx.CreateFunction(enum_cls, selector_prev_name);
				auto func = ctx.CreateFunctionOverload(fd, enum_cls, 0, 0, XL::FunctionMethod | XL::FunctionThisCall);
				auto & impl = static_cast<XL::XFunctionOverload *>(func)->GetImplementationDesc();
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
			} catch (...) {}
			try {
				auto fd = ctx.CreateFunction(cls, L"para_classem");
				auto func = ctx.CreateFunctionOverload(fd, type_void_ptr, 0, 0, XL::FunctionMethod | XL::FunctionThisCall | XL::FunctionOverride);
				XL::FunctionContextDesc desc;
				desc.retval = type_void_ptr;
				desc.instance = cls; desc.argc = 0; desc.argvt = 0; desc.argvn = 0;
				desc.flags = XL::FunctionMethod | XL::FunctionThisCall | XL::FunctionOverride;
				desc.vft_init = 0; desc.vft_init_seq = 0; desc.init_callback = 0;
				desc.create_init_sequence = desc.create_shutdown_sequence = false;
				SafePointer<XL::LFunctionContext> fctx = new XL::LFunctionContext(ctx, func, desc);
				fctx->EncodeReturn(cls);
				fctx->EndEncoding();
			} catch (...) {}
		}
	}
}