#include "xv_ext_rpc.h"

#include "../xlang/xl_types.h"
#include "../xlang/xl_func.h"

namespace Engine
{
	namespace XV
	{
		struct RPCMap
		{
			Volumes::List<string> interfaces;
			Volumes::Dictionary<int, SafePointer<XL::LObject> > selectors;
		};
		void CreateRPCMap(XL::LObject * cls, RPCMap & map)
		{
			if (cls->GetClass() != XL::Class::Type) throw InvalidArgumentException();
			auto xtype = static_cast<XL::XType *>(cls);
			if (xtype->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) throw InvalidArgumentException();
			auto xcls = static_cast<XL::XClass *>(cls);
			if (xcls->GetInterfaceCount()) throw InvalidArgumentException();
			if (xcls->GetArgumentSpecification().size.num_bytes != 0 || xcls->GetArgumentSpecification().size.num_words != 2) throw InvalidArgumentException();
			map.interfaces.InsertLast(cls->GetFullName());
			Volumes::Dictionary<string, XL::Class> items;
			cls->ListMembers(items);
			for (auto & i : items) {
				SafePointer<XL::LObject> obj = xcls->GetMember(i.key);
				if (obj->GetClass() == XL::Class::Function) {
					Array<string> variants(0x20);
					auto xfunc = static_cast<XL::XFunction *>(obj.Inner());
					xfunc->ListOverloads(variants, true);
					for (auto & v : variants) {
						auto xver = xfunc->GetOverloadT(v, true);
						auto selector = xver->GetVFDesc().vf_index;
						if (selector >= 13) {
							SafePointer<XL::LObject> ver;
							ver.SetRetain(xver);
							map.selectors.Append(selector, ver);
						}
					}
				}
			}
			if (xcls->GetFullName() != L"remota.objectum") CreateRPCMap(xcls->GetParentClass(), map);
		}
		void EncodeRPCError(XL::LContext & ctx, XL::LFunctionContext & encoder, int error, int suberror)
		{
			SafePointer<XL::LObject> ERROR = ctx.QueryLiteral(uint64(error));
			SafePointer<XL::LObject> SUBERROR = ctx.QueryLiteral(uint64(suberror));
			encoder.EncodeThrow(ERROR, SUBERROR);
		}
		void EncodeRPCSerializationError(XL::LContext & ctx, XL::LFunctionContext & encoder) { EncodeRPCError(ctx, encoder, 9, 4); }
		void EncodeRPCRestore(XL::LContext & ctx, XL::LFunctionContext & encoder, XL::LObject * dest, XL::LObject * in)
		{
			XL::LObject * scope;
			XL::LObject * argv[2];
			encoder.OpenTryBlock(&scope);
			SafePointer<XL::LObject> deserialize = ctx.QueryObject(L"serializatio.deserializa");
			argv[0] = dest;
			argv[1] = in;
			deserialize = deserialize->Invoke(2, argv);
			encoder.EncodeExpression(deserialize);
			encoder.OpenCatchBlock(&scope, L"", L"", 0, 0);
			EncodeRPCSerializationError(ctx, encoder);
			encoder.CloseCatchBlock();
		}
		void EncodeRPCSerialize(XL::LContext & ctx, XL::LFunctionContext & encoder, XL::LObject * from, XL::LObject * ex)
		{
			XL::LObject * scope;
			XL::LObject * argv[2];
			encoder.OpenTryBlock(&scope);
			SafePointer<XL::LObject> serialize = ctx.QueryObject(L"serializatio.serializa");
			argv[0] = from;
			argv[1] = ex;
			serialize = serialize->Invoke(2, argv);
			encoder.EncodeExpression(serialize);
			encoder.OpenCatchBlock(&scope, L"", L"", 0, 0);
			EncodeRPCSerializationError(ctx, encoder);
			encoder.CloseCatchBlock();
		}
		void DefineRPCInterfaceName(XL::LObject * cls, const RPCMap & map)
		{
			auto xcls = static_cast<XL::XClass *>(cls);
			auto & ctx = xcls->GetContext();
			SafePointer<XL::LObject> linea = XL::CreateType(L"Clinea", ctx);
			auto inv_func = ctx.CreateFunction(xcls, L"para_protocollum");
			auto inv_ver = ctx.CreateFunctionOverload(inv_func, linea, 0, 0, XL::FunctionThrows | XL::FunctionOverride | XL::FunctionMethod | XL::FunctionThisCall);
			if (!ctx.IsIdle()) {
				XL::FunctionContextDesc desc;
				desc.retval = linea;
				desc.instance = cls;
				desc.argc = 0;
				desc.argvt = 0;
				desc.argvn = 0;
				desc.this_name = L"ego";
				desc.flags = XL::FunctionThrows | XL::FunctionOverride | XL::FunctionMethod | XL::FunctionThisCall;
				desc.vft_init = 0;
				desc.vft_init_seq = 0;
				desc.create_init_sequence = desc.create_shutdown_sequence = false;
				desc.init_callback = 0;
				XL::LFunctionContext encoder(ctx, inv_ver, desc);
				SafePointer<XL::LObject> name = ctx.QueryLiteral(map.interfaces.GetFirst()->GetValue());
				encoder.EncodeReturn(name);
				encoder.EndEncoding();
			}
		}
		void DefineRPCLocalDynamicCast(XL::LObject * cls, const RPCMap & map)
		{
			auto xcls = static_cast<XL::XClass *>(cls);
			auto & ctx = xcls->GetContext();
			SafePointer<XL::LObject> nihil_ptr = XL::CreateType(L"PCnihil", ctx);
			SafePointer<XL::LObject> linea_ref = XL::CreateType(L"RClinea", ctx);
			SafePointer<XL::LObject> comparator = ctx.QueryObject(L"==");
			SafePointer<XL::LObject> either = ctx.QueryLogicalOrOperator();
			string name = L"cls";
			auto inv_func = ctx.CreateFunction(xcls, L"converte_dynamice");
			auto inv_ver_0 = ctx.CreateFunctionOverload(inv_func, nihil_ptr, 1, nihil_ptr.InnerRef(), XL::FunctionThrows | XL::FunctionOverride | XL::FunctionMethod | XL::FunctionThisCall);
			auto inv_ver_1 = ctx.CreateFunctionOverload(inv_func, nihil_ptr, 1, linea_ref.InnerRef(), XL::FunctionThrows | XL::FunctionOverride | XL::FunctionMethod | XL::FunctionThisCall);
			if (!ctx.IsIdle()) {
				XL::FunctionContextDesc desc_0, desc_1;
				desc_0.retval = nihil_ptr;
				desc_0.instance = cls;
				desc_0.argc = 1;
				desc_0.argvt = nihil_ptr.InnerRef();
				desc_0.argvn = &name;
				desc_0.this_name = L"ego";
				desc_0.flags = XL::FunctionThrows | XL::FunctionOverride | XL::FunctionMethod | XL::FunctionThisCall;
				desc_0.vft_init = 0;
				desc_0.vft_init_seq = 0;
				desc_0.create_init_sequence = desc_0.create_shutdown_sequence = false;
				desc_0.init_callback = 0;
				desc_1.retval = nihil_ptr;
				desc_1.instance = cls;
				desc_1.argc = 1;
				desc_1.argvt = linea_ref.InnerRef();
				desc_1.argvn = &name;
				desc_1.this_name = L"ego";
				desc_1.flags = XL::FunctionThrows | XL::FunctionOverride | XL::FunctionMethod | XL::FunctionThisCall;
				desc_1.vft_init = 0;
				desc_1.vft_init_seq = 0;
				desc_1.create_init_sequence = desc_1.create_shutdown_sequence = false;
				desc_1.init_callback = 0;
				XL::LFunctionContext encoder_0(ctx, inv_ver_0, desc_0);
				XL::LFunctionContext encoder_1(ctx, inv_ver_1, desc_1);
				SafePointer<XL::LObject> type_0 = encoder_0.GetRootScope()->GetMember(name);
				SafePointer<XL::LObject> type_1 = encoder_1.GetRootScope()->GetMember(name);
				SafePointer<XL::LObject> self_1 = encoder_1.GetRootScope()->GetMember(desc_1.this_name);
				XL::LObject * argv[2];
				SafePointer<XL::LObject> expr = ctx.QueryObject(L"repulsus.expone_symbolum");
				SafePointer<XL::LObject> expr2 = ctx.QueryModuleOperator(L"");
				argv[0] = expr2;
				argv[1] = type_0;
				expr = expr->Invoke(2, argv);
				expr = expr->GetMember(XL::OperatorFollow);
				expr = expr->Invoke(0, 0);
				expr = expr->GetMember(L"nomen");
				expr2 = encoder_0.GetInstance()->GetMember(L"converte_dynamice");
				expr2 = expr2->Invoke(1, expr.InnerRef());
				encoder_0.EncodeReturn(expr2);
				ObjectArray<XL::LObject> orargs(0x10);
				Array<XL::LObject *> orargv(0x10);
				XL::LObject * scope;
				for (auto & i : map.interfaces) {
					expr = ctx.QueryLiteral(i);
					argv[0] = type_1;
					argv[1] = expr;
					expr = comparator->Invoke(2, argv);
					orargs.Append(expr);
					orargv.Append(expr);
				}
				expr = either->Invoke(orargv.Length(), orargv.GetBuffer());
				encoder_1.OpenIfBlock(expr, &scope);
				expr = encoder_1.GetInstance()->GetMember(L"contine");
				expr = expr->Invoke(0, 0);
				encoder_1.EncodeExpression(expr);
				encoder_1.EncodeReturn(self_1);
				encoder_1.CloseIfElseBlock();
				EncodeRPCError(ctx, encoder_1, 1, 0);
				encoder_0.EndEncoding();
				encoder_1.EndEncoding();
			}
		}
		void DefineRPCRemoteInterfaceName(XL::LObject * cls)
		{
			auto & ctx = static_cast<XL::XClass *>(cls)->GetContext();
			SafePointer<XL::LObject> linea = XL::CreateType(L"Clinea", ctx);
			auto inv_func = ctx.CreateFunction(cls, L"para_protocollum");
			auto inv_ver = ctx.CreateFunctionOverload(inv_func, linea, 0, 0, XL::FunctionThrows | XL::FunctionOverride | XL::FunctionMethod | XL::FunctionThisCall);
			if (!ctx.IsIdle()) {
				XL::FunctionContextDesc desc;
				desc.retval = linea;
				desc.instance = cls;
				desc.argc = 0;
				desc.argvt = 0;
				desc.argvn = 0;
				desc.this_name = L"ego";
				desc.flags = XL::FunctionThrows | XL::FunctionOverride | XL::FunctionMethod | XL::FunctionThisCall;
				desc.vft_init = 0;
				desc.vft_init_seq = 0;
				desc.create_init_sequence = desc.create_shutdown_sequence = false;
				desc.init_callback = 0;
				XL::LFunctionContext encoder(ctx, inv_ver, desc);
				SafePointer<XL::LObject> reg = encoder.GetInstance()->GetMember(L"_reg");
				SafePointer<XL::LObject> num = encoder.GetInstance()->GetMember(L"_num");
				SafePointer<XL::LObject> helper = ctx.QueryObject(L"remota.objectum.__para_protocollum_objecti_remoti");
				XL::LObject * argv[2];
				argv[0] = reg;
				argv[1] = num;
				helper = helper->Invoke(2, argv);
				encoder.EncodeReturn(helper);
				encoder.EndEncoding();
			}
		}
		void DefineRPCRemoteDynamicCast(XL::LObject * cls)
		{
			auto & ctx = static_cast<XL::XClass *>(cls)->GetContext();
			SafePointer<XL::LObject> nihil_ptr = XL::CreateType(L"PCnihil", ctx);
			SafePointer<XL::LObject> linea_ref = XL::CreateType(L"RClinea", ctx);
			string name = L"cls";
			auto inv_func = ctx.CreateFunction(cls, L"converte_dynamice");
			auto inv_ver_0 = ctx.CreateFunctionOverload(inv_func, nihil_ptr, 1, nihil_ptr.InnerRef(), XL::FunctionThrows | XL::FunctionOverride | XL::FunctionMethod | XL::FunctionThisCall);
			auto inv_ver_1 = ctx.CreateFunctionOverload(inv_func, nihil_ptr, 1, linea_ref.InnerRef(), XL::FunctionThrows | XL::FunctionOverride | XL::FunctionMethod | XL::FunctionThisCall);
			if (!ctx.IsIdle()) {
				XL::FunctionContextDesc desc_0, desc_1;
				desc_0.retval = nihil_ptr;
				desc_0.instance = cls;
				desc_0.argc = 1;
				desc_0.argvt = nihil_ptr.InnerRef();
				desc_0.argvn = &name;
				desc_0.this_name = L"ego";
				desc_0.flags = XL::FunctionThrows | XL::FunctionOverride | XL::FunctionMethod | XL::FunctionThisCall;
				desc_0.vft_init = 0;
				desc_0.vft_init_seq = 0;
				desc_0.create_init_sequence = desc_0.create_shutdown_sequence = false;
				desc_0.init_callback = 0;
				desc_1.retval = nihil_ptr;
				desc_1.instance = cls;
				desc_1.argc = 1;
				desc_1.argvt = linea_ref.InnerRef();
				desc_1.argvn = &name;
				desc_1.this_name = L"ego";
				desc_1.flags = XL::FunctionThrows | XL::FunctionOverride | XL::FunctionMethod | XL::FunctionThisCall;
				desc_1.vft_init = 0;
				desc_1.vft_init_seq = 0;
				desc_1.create_init_sequence = desc_1.create_shutdown_sequence = false;
				desc_1.init_callback = 0;
				XL::LFunctionContext encoder_0(ctx, inv_ver_0, desc_0);
				XL::LFunctionContext encoder_1(ctx, inv_ver_1, desc_1);
				SafePointer<XL::LObject> type_0 = encoder_0.GetRootScope()->GetMember(name);
				SafePointer<XL::LObject> type_1 = encoder_1.GetRootScope()->GetMember(name);
				XL::LObject * argv[3];
				SafePointer<XL::LObject> expr = ctx.QueryObject(L"repulsus.expone_symbolum");
				SafePointer<XL::LObject> expr2 = ctx.QueryModuleOperator(L"");
				argv[0] = expr2;
				argv[1] = type_0;
				expr = expr->Invoke(2, argv);
				expr = expr->GetMember(XL::OperatorFollow);
				expr = expr->Invoke(0, 0);
				expr = expr->GetMember(L"nomen");
				expr2 = encoder_0.GetInstance()->GetMember(L"converte_dynamice");
				expr2 = expr2->Invoke(1, expr.InnerRef());
				encoder_0.EncodeReturn(expr2);
				SafePointer<XL::LObject> reg = encoder_1.GetInstance()->GetMember(L"_reg");
				SafePointer<XL::LObject> num = encoder_1.GetInstance()->GetMember(L"_num");
				SafePointer<XL::LObject> helper = ctx.QueryObject(L"remota.objectum.__converte_objectum_remotum_dynamice");
				argv[0] = reg;
				argv[1] = num;
				argv[2] = type_1;
				helper = helper->Invoke(3, argv);
				encoder_1.EncodeReturn(helper);
				encoder_0.EndEncoding();
				encoder_1.EndEncoding();
			}
		}
		void DefineRPCInvocator(XL::LObject * cls, const RPCMap & map)
		{
			auto xcls = static_cast<XL::XClass *>(cls);
			auto & ctx = xcls->GetContext();
			SafePointer<XL::LObject> object = XL::CreateType(L"Cremota.objectum", ctx);
			SafePointer<XL::LObject> nihil = XL::CreateType(L"Cnihil", ctx);
			SafePointer<XL::LObject> integer = XL::CreateType(L"Cint", ctx);
			SafePointer<XL::LObject> dstor_ptr = XL::CreateType(L"PCserializatio.deserializator", ctx);
			SafePointer<XL::LObject> stor_ptr = XL::CreateType(L"PCserializatio.serializator", ctx);
			XL::LObject * argv[3];
			string names[3];
			argv[0] = integer;
			argv[1] = dstor_ptr;
			argv[2] = stor_ptr;
			names[0] = L"selector";
			names[1] = L"in";
			names[2] = L"ex";
			auto inv_func = ctx.CreateFunction(xcls, L"invoca");
			auto inv_ver = ctx.CreateFunctionOverload(inv_func, nihil, 3, argv, XL::FunctionThrows | XL::FunctionOverride | XL::FunctionMethod | XL::FunctionThisCall);
			if (!ctx.IsIdle()) {
				XL::FunctionContextDesc desc;
				desc.retval = nihil;
				desc.instance = cls;
				desc.argc = 3;
				desc.argvt = argv;
				desc.argvn = names;
				desc.this_name = L"ego";
				desc.flags = XL::FunctionThrows | XL::FunctionOverride | XL::FunctionMethod | XL::FunctionThisCall;
				desc.vft_init = 0;
				desc.vft_init_seq = 0;
				desc.create_init_sequence = desc.create_shutdown_sequence = false;
				desc.init_callback = 0;
				XL::LFunctionContext encoder(ctx, inv_ver, desc);
				SafePointer<XL::LObject> selector = encoder.GetRootScope()->GetMember(names[0]);
				SafePointer<XL::LObject> in = encoder.GetRootScope()->GetMember(names[1]);
				SafePointer<XL::LObject> ex = encoder.GetRootScope()->GetMember(names[2]);
				SafePointer<XL::LObject> comparator = ctx.QueryObject(L"==");
				{
					SafePointer<XL::LObject> _1 = ctx.QueryLiteral(1ULL);
					XL::LObject * argv[2];
					argv[0] = selector;
					argv[1] = _1;
					SafePointer<XL::LObject> expr = comparator->Invoke(2, argv);
					XL::LObject * scope;
					encoder.OpenIfBlock(expr, &scope);
					encoder.OpenTryBlock(&scope);
					expr = encoder.GetInstance()->GetMember(L"para_protocollum");
					expr = expr->Invoke(0, 0);
					SafePointer<XL::LObject> serialize = ctx.QueryObject(L"serializatio.serializa");
					argv[0] = expr;
					argv[1] = ex;
					serialize = serialize->Invoke(2, argv);
					encoder.EncodeExpression(serialize);
					encoder.OpenCatchBlock(&scope, L"", L"", 0, 0);
					EncodeRPCSerializationError(ctx, encoder);
					encoder.CloseCatchBlock();
					encoder.EncodeReturn(0);
					encoder.CloseIfElseBlock();
				}
				{
					SafePointer<XL::LObject> _2 = ctx.QueryLiteral(2ULL);
					XL::LObject * argv[2];
					argv[0] = selector;
					argv[1] = _2;
					SafePointer<XL::LObject> expr = comparator->Invoke(2, argv);
					XL::LObject * scope;
					encoder.OpenIfBlock(expr, &scope);
					expr = XL::CreateType(L"Clinea", ctx);
					SafePointer<XL::LObject> prot = encoder.EncodeCreateVariable(expr);
					EncodeRPCRestore(ctx, encoder, prot, in);
					expr = encoder.GetInstance()->GetMember(L"converte_dynamice");
					expr = expr->Invoke(1, prot.InnerRef());
					SafePointer<XL::LObject> objptr = XL::CreateType(L"PCremota.objectum", ctx);
					SafePointer<XL::LObject> objautoptr = ctx.QueryObject(L"adl");
					objautoptr = objautoptr->GetMember(XL::OperatorSubscript);
					objautoptr = objautoptr->Invoke(1, object.InnerRef());
					expr = objptr->Invoke(1, expr.InnerRef());
					expr = objautoptr->Invoke(1, expr.InnerRef());
					expr = encoder.EncodeCreateVariable(objautoptr, expr);
					EncodeRPCSerialize(ctx, encoder, expr, ex);
					encoder.EncodeReturn(0);
					encoder.CloseIfElseBlock();
				}
				for (auto & s : map.selectors) {
					auto xfunc_base = static_cast<XL::XFunctionOverload *>(s.value.Inner());
					auto xfunc_base_cn = xfunc_base->GetCanonicalType();
					XI::Module::TypeReference xfunc_base_ref(xfunc_base_cn);
					SafePointer< Array<XI::Module::TypeReference> > sgn = xfunc_base_ref.GetFunctionSignature();
					SafePointer<XL::LObject> SEL = ctx.QueryLiteral(uint64(s.key));
					XL::LObject * argv[2];
					argv[0] = selector;
					argv[1] = SEL;
					SafePointer<XL::LObject> cond = comparator->Invoke(2, argv);
					XL::LObject * scope;
					encoder.OpenIfBlock(cond, &scope);
					ObjectArray<XL::LObject> argv_ref(0x20);
					Array<XL::LObject *> argv_list(0x20);
					for (int i = 1; i < sgn->Length(); i++) {
						SafePointer<XL::LObject> type;
						if (sgn->ElementAt(i).GetReferenceClass() == XI::Module::TypeReference::Class::Reference) {
							type = XL::CreateType(sgn->ElementAt(i).GetReferenceDestination().QueryCanonicalName(), ctx);
						} else {
							type = XL::CreateType(sgn->ElementAt(i).QueryCanonicalName(), ctx);
						}
						SafePointer<XL::LObject> var = encoder.EncodeCreateVariable(type);
						argv_ref.Append(var);
						argv_list.Append(var);
						EncodeRPCRestore(ctx, encoder, var, in);
					}
					SafePointer<XL::LObject> retval = XL::CreateType(sgn->ElementAt(0).QueryCanonicalName(), ctx);
					cond = xfunc_base->SetInstance(encoder.GetInstance());
					cond = cond->Invoke(argv_list.Length(), argv_list.GetBuffer());
					if (retval->GetFullName() == L"nihil") {
						encoder.EncodeExpression(cond);
					} else {
						retval = encoder.EncodeCreateVariable(retval, cond);
						EncodeRPCSerialize(ctx, encoder, retval, ex);
					}
					encoder.EncodeReturn(0);
					encoder.CloseIfElseBlock();
				}
				EncodeRPCError(ctx, encoder, 9, 3);
				encoder.EndEncoding();
			}
		}
		void DefineRPCRemoteClassFactory(XL::LObject * interface, XL::LObject * remote)
		{
			auto & ctx = static_cast<XL::XClass *>(interface)->GetContext();
			SafePointer<XL::LObject> nint64 = XL::CreateType(L"Cnint64", ctx);
			SafePointer<XL::LObject> regestor_ptr = XL::CreateType(L"PCremota.regestor", ctx);
			SafePointer<XL::LObject> objectum = XL::CreateType(L"Cremota.objectum", ctx);
			SafePointer<XL::LObject> objectum_autoptr = ctx.QueryObject(L"adl");
			objectum_autoptr = objectum_autoptr->GetMember(XL::OperatorSubscript);
			objectum_autoptr = objectum_autoptr->Invoke(1, objectum.InnerRef());
			XL::LObject * argv[2];
			string names[2];
			argv[0] = regestor_ptr;
			argv[1] = nint64;
			names[0] = L"r";
			names[1] = L"n";
			auto inv_func = ctx.CreateFunction(interface, L"__crea_versionem_remotam");
			auto inv_ver = ctx.CreateFunctionOverload(inv_func, objectum_autoptr, 2, argv, 0);
			if (!ctx.IsIdle()) {
				XL::FunctionContextDesc desc;
				desc.retval = objectum_autoptr;
				desc.instance = 0;
				desc.argc = 2;
				desc.argvt = argv;
				desc.argvn = names;
				desc.flags = 0;
				desc.vft_init = 0;
				desc.vft_init_seq = 0;
				desc.create_init_sequence = desc.create_shutdown_sequence = false;
				desc.init_callback = 0;
				XL::LFunctionContext encoder(ctx, inv_ver, desc);
				SafePointer<XL::LObject> r = encoder.GetRootScope()->GetMember(names[0]);
				SafePointer<XL::LObject> n = encoder.GetRootScope()->GetMember(names[1]);
				XL::LObject * scope;
				encoder.OpenTryBlock(&scope);
				SafePointer<XL::LObject> resp = encoder.EncodeCreateVariable(objectum_autoptr);
				argv[0] = n;
				argv[1] = r;
				SafePointer<XL::LObject> expr = XV::CreateNew(ctx, remote, 2, argv);
				SafePointer<XL::LObject> expr2 = resp->GetMember(L"contine");
				expr2 = expr2->Invoke(1, expr.InnerRef());
				encoder.EncodeExpression(expr2);
				encoder.EncodeReturn(resp);
				encoder.OpenCatchBlock(&scope, L"", L"", 0, 0);
				resp = objectum_autoptr->Invoke(0, 0);
				encoder.EncodeReturn(resp);
				encoder.CloseCatchBlock();
				encoder.EndEncoding();
			}
		}
		void DefineRPCRemoteBase(XL::LObject * cls, ObjectArray<XL::LObject> & init_list, const RPCMap & map, XL::LObject ** ctor_ver, XL::LObject ** dtor_ver)
		{
			auto & ctx = static_cast<XL::XClass *>(cls)->GetContext();
			SafePointer<XL::LObject> nihil = XL::CreateType(L"Cnihil", ctx);
			SafePointer<XL::LObject> objectum_ptr = XL::CreateType(L"PCobjectum", ctx);
			SafePointer<XL::LObject> nint64 = XL::CreateType(L"Cnint64", ctx);
			SafePointer<XL::LObject> regestor = XL::CreateType(L"Cremota.regestor", ctx);
			SafePointer<XL::LObject> regestor_autoptr = ctx.QueryObject(L"adl");
			regestor_autoptr = regestor_autoptr->GetMember(XL::OperatorSubscript);
			regestor_autoptr = regestor_autoptr->Invoke(1, regestor.InnerRef());
			ctx.CreateFieldRegular(cls, L"_num", nint64, true);
			ctx.CreateFieldRegular(cls, L"_reg", regestor_autoptr, true);
			XL::LObject * argv[2];
			string names[2];
			argv[0] = nint64;
			argv[1] = objectum_ptr;
			names[0] = L"num";
			names[1] = L"reg";
			auto ctor_func = ctx.CreateFunction(cls, XL::NameConstructor);
			auto dtor_func = ctx.CreateFunction(cls, XL::NameDestructor);
			*ctor_ver = ctx.CreateFunctionOverload(ctor_func, nihil, 2, argv, XL::FunctionMethod | XL::FunctionThisCall);
			*dtor_ver = ctx.CreateFunctionOverload(dtor_func, nihil, 0, 0, XL::FunctionOverride | XL::FunctionMethod | XL::FunctionThisCall);
		}
		void FinishRPCRemoteBase(XL::LObject * cls, ObjectArray<XL::LObject> & init_list, const RPCMap & map, XL::LObject * ctor_ver, XL::LObject * dtor_ver)
		{
			auto & ctx = static_cast<XL::XClass *>(cls)->GetContext();
			if (!ctx.IsIdle()) {
				SafePointer<XL::LObject> nihil = XL::CreateType(L"Cnihil", ctx);
				SafePointer<XL::LObject> objectum_ptr = XL::CreateType(L"PCobjectum", ctx);
				SafePointer<XL::LObject> nint64 = XL::CreateType(L"Cnint64", ctx);
				SafePointer<XL::LObject> regestor_ptr = XL::CreateType(L"PCremota.regestor", ctx);
				XL::LObject * argv[2];
				string names[2];
				argv[0] = nint64;
				argv[1] = objectum_ptr;
				names[0] = L"num";
				names[1] = L"reg";
				XL::FunctionContextDesc ctor_dest, dtor_dest;
				ctor_dest.retval = dtor_dest.retval = nihil;
				ctor_dest.instance = dtor_dest.instance = cls;
				ctor_dest.argc = 2;
				ctor_dest.argvt = argv;
				ctor_dest.argvn = names;
				dtor_dest.argc = 0;
				dtor_dest.argvt = 0;
				dtor_dest.argvn = 0;
				ctor_dest.this_name = ctor_dest.this_name = L"ego";
				ctor_dest.flags = XL::FunctionMethod | XL::FunctionThisCall;
				dtor_dest.flags = XL::FunctionOverride | XL::FunctionMethod | XL::FunctionThisCall;
				ctor_dest.vft_init = cls;
				ctor_dest.vft_init_seq = &init_list;
				ctor_dest.create_init_sequence = true;
				ctor_dest.create_shutdown_sequence = false;
				dtor_dest.vft_init = 0;
				dtor_dest.vft_init_seq = 0;
				dtor_dest.create_init_sequence = false;
				dtor_dest.create_shutdown_sequence = true;
				ctor_dest.init_callback = dtor_dest.init_callback = 0;
				XL::LFunctionContext ctor_enc(ctx, ctor_ver, ctor_dest);
				XL::LFunctionContext dtor_enc(ctx, dtor_ver, dtor_dest);
				SafePointer<XL::LObject> expr = ctor_enc.GetInstance()->GetMember(L"_num");
				SafePointer<XL::LObject> expr2 = ctor_enc.GetRootScope()->GetMember(L"num");
				expr = expr->GetMember(XL::OperatorAssign);
				expr = expr->Invoke(1, expr2.InnerRef());
				ctor_enc.EncodeExpression(expr);
				expr = ctor_enc.GetInstance()->GetMember(L"_reg");
				expr2 = ctor_enc.GetRootScope()->GetMember(L"reg");
				expr = expr->GetMember(L"contine");
				expr2 = regestor_ptr->Invoke(1, expr2.InnerRef());
				expr = expr->Invoke(1, expr2.InnerRef());
				ctor_enc.EncodeExpression(expr);
				expr = dtor_enc.GetInstance()->GetMember(L"_reg");
				expr2 = dtor_enc.GetInstance()->GetMember(L"_num");
				SafePointer<XL::LObject> expr3 = ctx.QueryObject(L"remota.objectum.__perde_objectum_remotum");
				argv[0] = expr;
				argv[1] = expr2;
				expr3 = expr3->Invoke(2, argv);
				dtor_enc.EncodeExpression(expr3);
				ctor_enc.EndEncoding();
				dtor_enc.EndEncoding();
			}
		}
		void DefineRPCRemoteProperties(XL::LObject * cls)
		{
			auto & ctx = static_cast<XL::XClass *>(cls)->GetContext();
			SafePointer<XL::LObject> logicum = XL::CreateType(L"Clogicum", ctx);
			SafePointer<XL::LObject> nint64 = XL::CreateType(L"Cnint64", ctx);
			SafePointer<XL::LObject> objectum_ptr = XL::CreateType(L"PCobjectum", ctx);
			auto rem_func = ctx.CreateFunction(cls, L"remotum_est@adipisce");
			auto num_func = ctx.CreateFunction(cls, L"numerus_regerendi@adipisce");
			auto reg_func = ctx.CreateFunction(cls, L"regestor@adipisce");
			auto rem_ver = ctx.CreateFunctionOverload(rem_func, logicum, 0, 0, XL::FunctionOverride | XL::FunctionMethod | XL::FunctionThisCall);
			auto num_ver = ctx.CreateFunctionOverload(num_func, nint64, 0, 0, XL::FunctionOverride | XL::FunctionMethod | XL::FunctionThisCall);
			auto reg_ver = ctx.CreateFunctionOverload(reg_func, objectum_ptr, 0, 0, XL::FunctionOverride | XL::FunctionMethod | XL::FunctionThisCall);
			if (!ctx.IsIdle()) {
				XL::FunctionContextDesc desc;
				desc.instance = cls;
				desc.argc = 0;
				desc.argvt = 0;
				desc.argvn = 0;
				desc.this_name = L"ego";
				desc.flags = XL::FunctionOverride | XL::FunctionMethod | XL::FunctionThisCall;
				desc.vft_init = 0;
				desc.vft_init_seq = 0;
				desc.create_init_sequence = desc.create_shutdown_sequence = false;
				desc.init_callback = 0;
				desc.retval = logicum;
				XL::LFunctionContext rem_encoder(ctx, rem_ver, desc);
				desc.retval = nint64;
				XL::LFunctionContext num_encoder(ctx, num_ver, desc);
				desc.retval = objectum_ptr;
				XL::LFunctionContext reg_encoder(ctx, reg_ver, desc);
				SafePointer<XL::LObject> expr = ctx.QueryLiteral(true);
				rem_encoder.EncodeReturn(expr);
				expr = num_encoder.GetInstance()->GetMember(L"_num");
				num_encoder.EncodeReturn(expr);
				expr = reg_encoder.GetInstance()->GetMember(L"_reg");
				reg_encoder.EncodeReturn(expr);
				rem_encoder.EndEncoding();
				num_encoder.EndEncoding();
				reg_encoder.EndEncoding();
			}
		}
		void DefineRPCRemoteSelectors(XL::LObject * cls, const RPCMap & map)
		{
			auto & ctx = static_cast<XL::XClass *>(cls)->GetContext();
			for (auto & s : map.selectors) {
				auto name = s.value->GetName();
				ObjectArray<XL::LObject> argv_types_ref(0x20);
				Array<XL::LObject *> argv_types(0x20);
				Array<string> argv_names(0x20);
				SafePointer<XL::LObject> retval;
				auto xfunc_base = static_cast<XL::XFunctionOverload *>(s.value.Inner());
				auto xfunc_base_cn = xfunc_base->GetCanonicalType();
				XI::Module::TypeReference xfunc_base_ref(xfunc_base_cn);
				SafePointer< Array<XI::Module::TypeReference> > sgn = xfunc_base_ref.GetFunctionSignature();
				retval = XL::CreateType(sgn->ElementAt(0).QueryCanonicalName(), ctx);
				for (int i = 1; i < sgn->Length(); i++) {
					SafePointer<XL::LObject> type = XL::CreateType(sgn->ElementAt(i).QueryCanonicalName(), ctx);
					argv_types_ref.Append(type);
					argv_types.Append(type);
					argv_names.Append(FormatString(L"A%0", i));
				}
				auto new_func = ctx.CreateFunction(cls, name.Fragment(0, name.FindFirst(L':')));
				auto new_ver = ctx.CreateFunctionOverload(new_func, retval, argv_types.Length(), argv_types.GetBuffer(), XL::FunctionThrows | XL::FunctionOverride | XL::FunctionMethod | XL::FunctionThisCall);
				if (!ctx.IsIdle()) {
					XL::FunctionContextDesc desc;
					desc.retval = retval;
					desc.instance = cls;
					desc.argc = argv_types.Length();
					desc.argvt = argv_types.GetBuffer();
					desc.argvn = argv_names.GetBuffer();
					desc.this_name = L"ego";
					desc.flags = XL::FunctionThrows | XL::FunctionOverride | XL::FunctionMethod | XL::FunctionThisCall;
					desc.vft_init = 0;
					desc.vft_init_seq = 0;
					desc.create_init_sequence = desc.create_shutdown_sequence = false;
					desc.init_callback = 0;
					XL::LFunctionContext encoder(ctx, new_ver, desc);
					SafePointer<XL::LObject> reg = encoder.GetInstance()->GetMember(L"_reg");
					SafePointer<XL::LObject> num = encoder.GetInstance()->GetMember(L"_num");
					reg = reg->GetMember(XL::OperatorFollow);
					reg = reg->Invoke(0, 0);
					SafePointer<XL::LObject> expr = reg->GetMember(L"initia_invocationem");
					expr = expr->Invoke(0, 0);
					SafePointer<XL::LObject> type = expr->GetType();
					SafePointer<XL::LObject> stor = encoder.EncodeCreateVariable(type, expr);
					SafePointer<XL::LObject> serialize = ctx.QueryObject(L"serializatio.serializa");
					XL::LObject * scope;
					XL::LObject * argv[2];
					if (argv_types.Length()) {
						encoder.OpenTryBlock(&scope);
						for (auto & name : argv_names) {
							expr = encoder.GetRootScope()->GetMember(name);
							argv[0] = expr;
							argv[1] = stor;
							expr = serialize->Invoke(2, argv);
							encoder.EncodeExpression(expr);
						}
						encoder.OpenCatchBlock(&scope, L"", L"", 0, 0);
						expr = reg->GetMember(L"ende_invocationem");
						expr = expr->Invoke(0, 0);
						encoder.EncodeExpression(expr);
						EncodeRPCSerializationError(ctx, encoder);
						encoder.CloseCatchBlock();
					}
					type = ctx.QueryLiteral(uint64(s.key));
					argv[0] = num;
					argv[1] = type;
					expr = reg->GetMember(L"exordi_invocationem");
					expr = expr->Invoke(2, argv);
					type = expr->GetType();
					SafePointer<XL::LObject> dstor = encoder.EncodeCreateVariable(type, expr);
					if (retval->GetFullName() != L"nihil") {
						SafePointer<XL::LObject> resp = encoder.EncodeCreateVariable(retval);
						serialize = ctx.QueryObject(L"serializatio.deserializa");
						encoder.OpenTryBlock(&scope);
						argv[0] = resp;
						argv[1] = dstor;
						expr = serialize->Invoke(2, argv);
						encoder.EncodeExpression(expr);
						encoder.OpenCatchBlock(&scope, L"", L"", 0, 0);
						expr = reg->GetMember(L"ende_invocationem");
						expr = expr->Invoke(0, 0);
						encoder.EncodeExpression(expr);
						EncodeRPCSerializationError(ctx, encoder);
						encoder.CloseCatchBlock();
						encoder.EncodeExpression(expr);
						encoder.EncodeReturn(resp);
					} else {
						expr = reg->GetMember(L"ende_invocationem");
						expr = expr->Invoke(0, 0);
						encoder.EncodeExpression(expr);
					}
					encoder.EndEncoding();
				}
			}
		}
		void CreateRPCRemoteClass(XL::LObject * cls, ObjectArray<XL::LObject> & init_list, const RPCMap & map, XL::LObject ** cls_ref)
		{
			auto & ctx = static_cast<XL::XClass *>(cls)->GetContext();
			if (!ctx.IsIdle()) {
				XL::LObject * ctor, * dtor;
				XL::LObject * type = ctx.CreateClass(cls, L"__versio_remota");
				ctx.LockClass(type, true);
				ctx.AdoptParentClass(type, cls);
				DefineRPCRemoteBase(type, init_list, map, &ctor, &dtor);
				DefineRPCRemoteProperties(type);
				DefineRPCRemoteInterfaceName(type);
				DefineRPCRemoteDynamicCast(type);
				DefineRPCRemoteSelectors(type, map);
				CreateTypeServiceRoutines(type);
				ctx.LockClass(type, false);
				ctx.AlignInstanceSize(type);
				FinishRPCRemoteBase(type, init_list, map, ctor, dtor);
				type->Retain();
				*cls_ref = type;
			}
		}
		void CreateRPCServiceRoutines(XL::LObject * cls)
		{
			RPCMap map;
			CreateRPCMap(cls, map);
			DefineRPCInterfaceName(cls, map);
			DefineRPCLocalDynamicCast(cls, map);
			DefineRPCInvocator(cls, map);
		}
		void CreateRPCServiceObjects(XL::LObject * cls, ObjectArray<XL::LObject> & init_list)
		{
			SafePointer<XL::LObject> remote_class;
			RPCMap map;
			CreateRPCMap(cls, map);
			CreateRPCRemoteClass(cls, init_list, map, remote_class.InnerRef());
			DefineRPCRemoteClassFactory(cls, remote_class);
		}
	}
}