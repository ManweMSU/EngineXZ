#include "xl_synth.h"

#include "xl_func.h"
#include "xl_prop.h"
#include "xl_code.h"

namespace Engine
{
	namespace XL
	{
		XClass * GetStaticArrayPseudoClass(XArray * on_array)
		{
			auto & ctx = on_array->GetContext();
			auto name = L"_@" + on_array->GetCanonicalType();
			try {
				SafePointer<LObject> cls = ctx.GetPrivateNamespace()->GetMember(name);
				return static_cast<XClass *>(cls.Inner());
			} catch (...) {}
			SafePointer<XClass> cls = CreateClass(name, ctx.GetPrivateNamespace()->GetFullName() + L"." + name, true, ctx);
			ctx.GetPrivateNamespace()->AddMember(name, cls);
			cls->OverrideLanguageSemantics(XI::Module::Class::Nature::Core);
			cls->OverrideArgumentSpecification(on_array->GetArgumentSpecification());
			return cls;
		}
		class StaticArrayCreateInit : public ISimpifiedFunctionInitCallback
		{
			SafePointer<XArray> array_type;
			uint _flags;
			int _index, _length;
		public:
			StaticArrayCreateInit(XArray * on_array, uint flags) : _flags(flags), _index(0)
			{
				_length = on_array->GetVolume();
				array_type.SetRetain(on_array);
			}
			virtual void GetNextInit(LObject * self, LObject * argument, LObject ** init, LObject ** revert) override
			{
				if (_index >= _length) { *init = *revert = 0; return; }
				SafePointer<LObject> dest = array_type->ExtractElement(self, _index);
				if (_flags & CreateMethodConstructorInit) {
					*init = InitInstance(dest, 0, 0);
					*revert = DestroyInstance(dest);
				} else if (_flags & CreateMethodConstructorCopy) {
					SafePointer<LObject> src = array_type->ExtractElement(argument, _index);
					*init = InitInstance(dest, src);
					*revert = DestroyInstance(dest);
				} else if (_flags & CreateMethodConstructorMove) {
					SafePointer<LObject> src = array_type->ExtractElement(argument, _index);
					*init = MoveInstance(dest, src);
					*revert = 0;
				} else if (_flags & CreateMethodConstructorZero) {
					SafePointer<LObject> src = array_type->ExtractElement(argument, _index);
					*init = ZeroInstance(dest);
					*revert = 0;
				} else if (_flags & CreateMethodDestructor) {
					*init = DestroyInstance(dest);
					*revert = 0;
				}
				_index++;
			}
		};
		class DefaultCopyConstructorInit : public IFunctionInitCallback
		{
			XClass * _cls, * _parent;
			ObjectArray<LObject> _fields;
			ObjectArray<Object> _retain;
			int _index;
		public:
			DefaultCopyConstructorInit(XClass * on_class) : _cls(on_class), _fields(0x40), _retain(0x100), _index(0)
			{
				on_class->ListFields(_fields);
				_parent = on_class->GetParentClass();
			}
			virtual void GetNextInit(LObject * arguments_scope, FunctionInitDesc & desc) override
			{
				SafePointer<LObject> init = arguments_scope->GetMember(L"valor");
				desc.init.Clear();
				if (_index < _fields.Length()) {
					SafePointer<LObject> init_field = init->GetMember(_fields[_index].GetName());
					_retain.Append(init_field);
					desc.subject = _fields.ElementAt(_index);
					desc.init.InsertLast(init_field);
				} else if (_index == _fields.Length() && _parent) {
					SafePointer<LObject> parent = _cls->TransformTo(init, _parent, false);
					_retain.Append(parent);
					desc.subject = _cls;
					desc.init.InsertLast(parent);
				} else desc.subject = 0;
				_index++;
			}
		};

		LObject * CreateStaticArrayRoutine(XArray * on_array, uint flags)
		{
			auto & ctx = on_array->GetContext();
			auto cls = GetStaticArrayPseudoClass(on_array);
			auto name = L"@" + string(flags);
			try {
				SafePointer<LObject> fd = cls->GetMember(name);
				Array<string> over(0x10);
				static_cast<XFunction *>(fd.Inner())->ListOverloads(over, true);
				if (over.Length() != 1) throw Exception();
				auto fover = static_cast<XFunction *>(fd.Inner())->GetOverloadT(over[0], true);
				fover->Retain();
				return fover;
			} catch (...) {}
			SafePointer<XType> element_type = on_array->GetElementType();
			if (flags & CreateMethodAssign) {
				SafePointer<XType> arg_type = CreateType(XI::Module::TypeReference::MakeReference(on_array->GetCanonicalType()), ctx);
				uint ff = FunctionMethod | FunctionThisCall;
				XFunctionOverload * asgn;
				SafePointer<LObject> asgn_fd = element_type->GetMember(OperatorAssign);
				if (asgn_fd->GetClass() == Class::Function) {
					auto et = element_type->GetCanonicalType();
					auto et_ref = XI::Module::TypeReference::MakeReference(et);
					try { asgn = static_cast<XFunction *>(asgn_fd.Inner())->GetOverloadT(1, &et_ref, true); } catch (...) { asgn = 0; }
					if (!asgn) asgn = static_cast<XFunction *>(asgn_fd.Inner())->GetOverloadT(1, &et, true);
				} else if (asgn_fd->GetClass() == Class::FunctionOverload) {
					asgn = static_cast<XFunctionOverload *>(asgn_fd.Inner());
				} else throw InvalidStateException();
				if (asgn->Throws()) ff |= FunctionThrows;
				LObject * fd = ctx.CreateFunction(cls, name);
				LObject * func = ctx.CreateFunctionOverload(fd, arg_type, 1, reinterpret_cast<LObject **>(arg_type.InnerRef()), ff);
				string arg_name = L"A";
				FunctionContextDesc desc;
				desc.retval = arg_type;
				desc.instance = on_array;
				desc.argc = 1;
				desc.argvt = reinterpret_cast<LObject **>(arg_type.InnerRef());
				desc.argvn = &arg_name;
				desc.this_name = L"E";
				desc.flags = ff;
				desc.vft_init = 0;
				desc.vft_init_seq = 0;
				desc.create_init_sequence = desc.create_shutdown_sequence = false;
				desc.init_callback = 0;
				SafePointer<LFunctionContext> fctx = new LFunctionContext(ctx, func, desc);
				auto volume = on_array->GetVolume();
				auto self = fctx->GetInstance();
				SafePointer<LObject> src = fctx->GetRootScope()->GetMember(arg_name);
				for (int index = 0; index < volume; index++) {
					SafePointer<LObject> dest_element = on_array->ExtractElement(self, index);
					SafePointer<LObject> src_element = on_array->ExtractElement(src, index);
					SafePointer<LObject> asgn_method = dest_element->GetMember(OperatorAssign);
					SafePointer<LObject> expr = asgn_method->Invoke(1, src_element.InnerRef());
					fctx->EncodeExpression(expr);
				}
				fctx->EncodeReturn(self);
				fctx->EndEncoding();
				fctx.SetReference(0);
				func->Retain();
				return func;
			} else {
				SafePointer<XType> void_type = CreateType(XI::Module::TypeReference::MakeClassReference(NameVoid), ctx);
				SafePointer<LObject> arg_type;
				uint ff = FunctionMethod | FunctionThisCall;
				if (flags & CreateMethodConstructorInit) {
					SafePointer<LObject> element_ctor = element_type->GetConstructorInit();
					if (static_cast<XFunctionOverload *>(element_ctor.Inner())->Throws()) ff |= FunctionThrows;
				} else if (flags & CreateMethodConstructorCopy) {
					SafePointer<LObject> element_ctor = element_type->GetConstructorCopy();
					if (static_cast<XFunctionOverload *>(element_ctor.Inner())->Throws()) ff |= FunctionThrows;
					arg_type = ctx.QueryTypeReference(on_array);
				} else if (flags & CreateMethodConstructorMove) {
					SafePointer<LObject> element_ctor = element_type->GetConstructorMove();
					if (static_cast<XFunctionOverload *>(element_ctor.Inner())->Throws()) throw InvalidArgumentException();
					arg_type = ctx.QueryTypeReference(on_array);
				} else if (flags & CreateMethodConstructorZero) {
					SafePointer<LObject> element_ctor = element_type->GetConstructorZero();
					if (static_cast<XFunctionOverload *>(element_ctor.Inner())->Throws()) throw InvalidArgumentException();
				} else if (flags & CreateMethodDestructor) {
					SafePointer<LObject> element_dtor = element_type->GetDestructor();
					if (element_dtor->GetClass() == Class::Null) { element_dtor->Retain(); return element_dtor; }
				} else throw InvalidArgumentException();
				LObject * fd = ctx.CreateFunction(cls, name);
				LObject * func = ctx.CreateFunctionOverload(fd, void_type, arg_type ? 1 : 0, arg_type.InnerRef(), ff);
				StaticArrayCreateInit init(on_array, flags);
				SimpifiedFunctionContextDesc desc;
				desc.instance = on_array;
				desc.argument = arg_type;
				desc.flags = ff;
				desc.init_callback = &init;
				SafePointer<LFunctionContext> fctx = new LFunctionContext(ctx, func, desc);
				fctx.SetReference(0);
				func->Retain();
				return func;
			}
		}
		void CreateDefaultImplementation(XClass * on_class, uint flags, ObjectArray<LObject> & vft_init)
		{
			auto & ctx = on_class->GetContext();
			SafePointer<LObject> class_ref = ctx.QueryTypeReference(on_class);
			SafePointer<LObject> void_type = CreateType(XI::Module::TypeReference::MakeClassReference(NameVoid), ctx);
			auto parent_class = on_class->GetParentClass();
			ObjectArray<LObject> fields(0x80);
			ObjectArray<XType> subj_types(0x80);
			on_class->ListFields(fields);
			if (parent_class) subj_types.Append(parent_class);
			for (auto & f : fields) { SafePointer<LObject> type = f.GetType(); subj_types.Append(static_cast<XType *>(type.Inner())); }
			bool discard = false;
			bool throws = false;
			if (flags & CreateMethodConstructorInit) {
				try {
					for (auto & s : subj_types) {
						SafePointer<LObject> ctor = s.GetConstructorInit();
						if (static_cast<XFunctionOverload *>(ctor.Inner())->Throws()) throws = true;
					}
				} catch (...) { discard = true; }
			} else if (flags & CreateMethodConstructorCopy) {
				try {
					for (auto & s : subj_types) {
						SafePointer<LObject> ctor = s.GetConstructorCopy();
						if (static_cast<XFunctionOverload *>(ctor.Inner())->Throws()) throws = true;
					}
				} catch (...) { discard = true; }
			} else if (flags & CreateMethodConstructorMove) {
				try {
					for (auto & s : subj_types) {
						SafePointer<LObject> ctor = s.GetConstructorMove();
						if (static_cast<XFunctionOverload *>(ctor.Inner())->Throws()) throws = true;
					}
				} catch (...) { discard = true; }
			} else if (flags & CreateMethodConstructorZero) {
				try {
					for (auto & s : subj_types) {
						SafePointer<LObject> ctor = s.GetConstructorZero();
						if (static_cast<XFunctionOverload *>(ctor.Inner())->Throws()) throws = true;
					}
				} catch (...) { discard = true; }
			} else if (flags & CreateMethodDestructor) {
				bool all_null = true;
				for (auto & s : subj_types) {
					SafePointer<LObject> dtor = s.GetDestructor();
					if (dtor->GetClass() != Class::Null) { all_null = false; break; }
				}
				if (all_null) discard = true;
			} else if (flags & CreateMethodAssign) {
				try {
					for (auto & s : subj_types) {
						XFunctionOverload * asgn;
						SafePointer<LObject> asgn_fd = s.GetMember(OperatorAssign);
						if (asgn_fd->GetClass() == Class::Function) {
							auto et = s.GetCanonicalType();
							auto et_ref = XI::Module::TypeReference::MakeReference(et);
							try { asgn = static_cast<XFunction *>(asgn_fd.Inner())->GetOverloadT(1, &et_ref, true); } catch (...) { asgn = 0; }
							if (!asgn) asgn = static_cast<XFunction *>(asgn_fd.Inner())->GetOverloadT(1, &et, true);
						} else if (asgn_fd->GetClass() == Class::FunctionOverload) {
							asgn = static_cast<XFunctionOverload *>(asgn_fd.Inner());
						} else throw Exception();
						if (asgn->Throws()) throws = true;
					}
				} catch (...) { discard = true; }
			}
			if (discard) return;
			int argc = 0;
			uint ff = FunctionMethod | FunctionThisCall;
			if (throws) ff |= FunctionThrows;
			LObject * func;
			if (flags & CreateMethodAssign) {
				try {
					auto fd = ctx.CreateFunction(on_class, OperatorAssign);
					func = ctx.CreateFunctionOverload(fd, class_ref, 1, class_ref.InnerRef(), ff);
				} catch (...) { return; }
				string src_name = L"valor";
				FunctionContextDesc desc;
				desc.retval = class_ref;
				desc.instance = on_class;
				desc.argc = 1;
				desc.argvt = class_ref.InnerRef();
				desc.argvn = &src_name;
				desc.this_name = L"";
				desc.flags = ff;
				desc.vft_init = 0;
				desc.vft_init_seq = 0;
				desc.create_init_sequence = desc.create_shutdown_sequence = false;
				desc.init_callback = 0;
				SafePointer<LFunctionContext> fctx = new LFunctionContext(ctx, func, desc);
				auto self = fctx->GetInstance();
				SafePointer<LObject> src = fctx->GetRootScope()->GetMember(src_name);
				for (auto & f : fields) {
					SafePointer<LObject> f_dest = static_cast<XField &>(f).SetInstance(self);
					SafePointer<LObject> f_src = static_cast<XField &>(f).SetInstance(src);
					SafePointer<LObject> asgn_method = f_dest->GetMember(OperatorAssign);
					SafePointer<LObject> expr = asgn_method->Invoke(1, f_src.InnerRef());
					fctx->EncodeExpression(expr);
				}
				if (parent_class) {
					SafePointer<LObject> self_parent = on_class->TransformTo(self, parent_class, false);
					SafePointer<LObject> src_parent = on_class->TransformTo(src, parent_class, false);
					SafePointer<LObject> asgn_method = self_parent->GetMember(OperatorAssign);
					SafePointer<LObject> expr = asgn_method->Invoke(1, src_parent.InnerRef());
					fctx->EncodeExpression(expr);
				}
				fctx->EncodeReturn(self);
				fctx->EndEncoding();
			} else {
				try {
					if (flags & CreateMethodConstructorInit) {
						auto fd = ctx.CreateFunction(on_class, NameConstructor);
						func = ctx.CreateFunctionOverload(fd, void_type, 0, 0, ff);
					} else if (flags & CreateMethodConstructorCopy) {
						argc = 1;
						auto fd = ctx.CreateFunction(on_class, NameConstructor);
						func = ctx.CreateFunctionOverload(fd, void_type, 1, class_ref.InnerRef(), ff);
					} else if (flags & CreateMethodConstructorMove) {
						if (throws) return;
						argc = 1;
						auto fd = ctx.CreateFunction(on_class, NameConstructorMove);
						func = ctx.CreateFunctionOverload(fd, void_type, 1, class_ref.InnerRef(), ff);
					} else if (flags & CreateMethodConstructorZero) {
						if (throws) return;
						auto fd = ctx.CreateFunction(on_class, NameConstructorZero);
						func = ctx.CreateFunctionOverload(fd, void_type, 0, 0, ff);
					} else if (flags & CreateMethodDestructor) {
						if (throws) return;
						auto fd = ctx.CreateFunction(on_class, NameDestructor);
						func = ctx.CreateFunctionOverload(fd, void_type, 0, 0, ff);
					}
				} catch (...) { return; }
				DefaultCopyConstructorInit callback(on_class);
				string src_name = L"valor";
				FunctionContextDesc desc;
				desc.retval = void_type;
				desc.instance = on_class;
				desc.argc = argc;
				desc.argvt = argc ? class_ref.InnerRef() : 0;
				desc.argvn = argc ? &src_name : 0;
				desc.this_name = L"";
				desc.flags = ff;
				desc.vft_init = 0;
				desc.vft_init_seq = 0;
				desc.create_init_sequence = desc.create_shutdown_sequence = false;
				desc.init_callback = 0;
				if (flags & CreateMethodConstructorInit) {
					desc.vft_init = on_class;
					desc.vft_init_seq = &vft_init;
					desc.create_init_sequence = true;
				} else if (flags & CreateMethodConstructorCopy) {
					desc.vft_init = on_class;
					desc.vft_init_seq = &vft_init;
					desc.create_init_sequence = true;
					desc.init_callback = &callback;
				} else if (flags & CreateMethodConstructorMove) {
					desc.vft_init = on_class;
					desc.vft_init_seq = &vft_init;
				} else if (flags & CreateMethodConstructorZero) {
					desc.vft_init = on_class;
					desc.vft_init_seq = &vft_init;
				} else if (flags & CreateMethodDestructor) desc.create_shutdown_sequence = true;
				SafePointer<LFunctionContext> fctx = new LFunctionContext(ctx, func, desc);
				if (flags & CreateMethodConstructorMove) {
					auto self = fctx->GetInstance();
					SafePointer<LObject> src = fctx->GetRootScope()->GetMember(L"valor");
					for (auto & f : fields) {
						SafePointer<LObject> f_dest = static_cast<XField &>(f).SetInstance(self);
						SafePointer<LObject> f_src = static_cast<XField &>(f).SetInstance(src);
						SafePointer<LObject> expr = MoveInstance(f_dest, f_src);
						fctx->EncodeExpression(expr);
					}
					if (parent_class) {
						SafePointer<LObject> self_parent = on_class->TransformTo(self, parent_class, false);
						SafePointer<LObject> src_parent = on_class->TransformTo(src, parent_class, false);
						SafePointer<LObject> expr = MoveInstance(self_parent, src_parent);
						fctx->EncodeExpression(expr);
					}
				} else if (flags & CreateMethodConstructorZero) {
					auto self = fctx->GetInstance();
					for (auto & f : fields) {
						SafePointer<LObject> f_dest = static_cast<XField &>(f).SetInstance(self);
						SafePointer<LObject> expr = ZeroInstance(f_dest);
						fctx->EncodeExpression(expr);
					}
					if (parent_class) {
						SafePointer<LObject> self_parent = on_class->TransformTo(self, parent_class, false);
						SafePointer<LObject> expr = ZeroInstance(self_parent);
						fctx->EncodeExpression(expr);
					}
				}
				fctx->EndEncoding();
			}
		}
		void CreateDefaultImplementations(XClass * on_class, uint flags, ObjectArray<LObject> & vft_init)
		{
			if (flags & CreateMethodConstructorInit) CreateDefaultImplementation(on_class, CreateMethodConstructorInit, vft_init);
			if (flags & CreateMethodConstructorCopy) CreateDefaultImplementation(on_class, CreateMethodConstructorCopy, vft_init);
			if (flags & CreateMethodConstructorMove) CreateDefaultImplementation(on_class, CreateMethodConstructorMove, vft_init);
			if (flags & CreateMethodConstructorZero) CreateDefaultImplementation(on_class, CreateMethodConstructorZero, vft_init);
			if (flags & CreateMethodDestructor) CreateDefaultImplementation(on_class, CreateMethodDestructor, vft_init);
			if (flags & CreateMethodAssign) CreateDefaultImplementation(on_class, CreateMethodAssign, vft_init);
		}
	}
}