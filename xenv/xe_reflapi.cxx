#include "xe_reflapi.h"

#include "../ximg/xi_resources.h"
#include "../xasm/xa_type_helper.h"

#define XE_TRY_INTRO try {
#define XE_TRY_OUTRO(DRV) } catch (Engine::InvalidArgumentException &) { ectx.error_code = 3; ectx.error_subcode = 0; return DRV; } \
catch (Engine::InvalidFormatException &) { ectx.error_code = 4; ectx.error_subcode = 0; return DRV; } \
catch (Engine::InvalidStateException &) { ectx.error_code = 5; ectx.error_subcode = 0; return DRV; } \
catch (Engine::OutOfMemoryException &) { ectx.error_code = 2; ectx.error_subcode = 0; return DRV; } \
catch (Engine::IO::FileAccessException & e) { ectx.error_code = 6; ectx.error_subcode = e.code; return DRV; } \
catch (Engine::Exception &) { ectx.error_code = 1; ectx.error_subcode = 0; return DRV; } \
catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return DRV; }

namespace Engine
{
	namespace XE
	{
		template<class F> void LookUpHierarchy(const ExecutionContext & ctx, const ClassSymbol * base, uintptr rebase, bool as_interface, bool as_parent, F & hdlr)
		{
			if (hdlr(base, rebase, as_interface, as_parent)) return;
			if (base->GetParentClassName().Length()) {
				auto smbl = ctx.GetSymbol(base->GetParentClassName());
				if (!smbl || smbl->GetSymbolType() != SymbolType::Class) throw InvalidStateException();
				LookUpHierarchy(ctx, static_cast<const ClassSymbol *>(smbl), rebase, as_interface, true, hdlr);
			}
			SafePointer< Array<string> > il = base->ListInterfaces();
			if (!il) throw OutOfMemoryException();
			for (auto & in : il->Elements()) {
				auto local_rebase = reinterpret_cast<uintptr>(base->CastToInterface(in, 0));
				auto smbl = ctx.GetSymbol(in);
				if (!smbl || smbl->GetSymbolType() != SymbolType::Class) throw InvalidStateException();
				LookUpHierarchy(ctx, static_cast<const ClassSymbol *>(smbl), rebase + local_rebase, true, false, hdlr);
			}
		}
		template<class F> void LookUpHierarchy(const ExecutionContext & ctx, const ClassSymbol * base, F & hdlr) { LookUpHierarchy(ctx, base, 0, false, false, hdlr); }

		class RModule : public Object
		{
		public:
			struct VersionInfo { uint major, minor, subver, build; VersionInfo(void) {} ~VersionInfo(void) {} };
			struct ResourceInfo { string type; uintptr id; };
		private:
			const Module * _mdl;
		public:
			RModule(const Module * mdl) : _mdl(mdl) {}
			virtual ~RModule(void) override {}
			virtual const Module * GetHandle(void) noexcept { return _mdl; }
			virtual string GetName(ErrorContext & ectx) noexcept { XE_TRY_INTRO string s; _mdl->GetName(s); return s; XE_TRY_OUTRO(L"") }
			virtual string GetCreator(ErrorContext & ectx) noexcept { XE_TRY_INTRO string s; uint a, b, c, d; _mdl->GetAssembler(s, a, b, c, d); return s; XE_TRY_OUTRO(L"") }
			virtual VersionInfo GetCreatorVersion(void) noexcept
			{
				VersionInfo vi;
				ZeroMemory(&vi, sizeof(vi));
				try {
					string s;
					_mdl->GetAssembler(s, vi.major, vi.minor, vi.subver, vi.build);
				} catch (...) {}
				return vi;
			}
			virtual SafePointer< Array<ResourceInfo> > GetResources(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< Array<ResourceInfo> > result = new Array<ResourceInfo>(0x40);
				for (auto & rsrc : _mdl->GetResources()) {
					ResourceInfo info;
					int resid;
					XI::ReadResourceID(rsrc.key, info.type, resid);
					info.id = resid;
					result->Append(info);
				}
				return result;
				XE_TRY_OUTRO(0)
			}
		};
		class RType : public Object
		{
			const Module * _ctx;
			string _cn;
			XI::Module::TypeReference _ref;
		public:
			RType(const Module * context, const string & cn) : _ctx(context), _cn(cn), _ref(_cn) {}
			virtual ~RType(void) override {}
			virtual string ToString(void) const override { try { return _cn; } catch (...) { return L""; } }
			virtual bool IsClass(void) noexcept { return _ref.GetReferenceClass() == XI::Module::TypeReference::Class::Class; }
			virtual bool IsTemplated(void) noexcept
			{
				if (_ref.GetReferenceClass() == XI::Module::TypeReference::Class::Class) {
					return _ref.GetClassName().Fragment(0, 12) == L"@praeformae.";
				} else return false;
			}
			virtual bool IsFunction(void) noexcept { return _ref.GetReferenceClass() == XI::Module::TypeReference::Class::Function; }
			virtual bool IsArray(void) noexcept { return _ref.GetReferenceClass() == XI::Module::TypeReference::Class::Array; }
			virtual bool IsPointer(void) noexcept { return _ref.GetReferenceClass() == XI::Module::TypeReference::Class::Pointer; }
			virtual bool IsReference(void) noexcept { return _ref.GetReferenceClass() == XI::Module::TypeReference::Class::Reference; }
			virtual const Module * GetContext(void) noexcept { return _ctx; }
			virtual const void * GetClassHandle(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!IsClass()) throw Exception();
				auto smbl = _ctx->GetExecutionContext().GetSymbol(_ref.GetClassName());
				if (!smbl) throw InvalidStateException();
				return smbl;
				XE_TRY_OUTRO(0);
			}
			virtual SafePointer<RType> GetCompoundInstance(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (IsFunction()) {
					SafePointer< Array<XI::Module::TypeReference> > sgn = _ref.GetFunctionSignature();
					return new RType(_ctx, sgn->ElementAt(0).QueryCanonicalName());
				} else if (IsPointer()) return new RType(_ctx, _ref.GetPointerDestination().QueryCanonicalName());
				else if (IsReference()) return new RType(_ctx, _ref.GetReferenceDestination().QueryCanonicalName());
				else if (IsArray()) return new RType(_ctx, _ref.GetArrayElement().QueryCanonicalName());
				else throw Exception();
				XE_TRY_OUTRO(0);
			}
			virtual int GetArrayLength(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!IsArray()) throw Exception();
				return _ref.GetArrayVolume();
				XE_TRY_OUTRO(0);
			}
			virtual SafePointer< ObjectArray<RType> > GetArgumentTypes(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< ObjectArray<RType> > result = new ObjectArray<RType>(0x10);
				if (IsFunction()) {
					SafePointer< Array<XI::Module::TypeReference> > sgn = _ref.GetFunctionSignature();
					for (int i = 1; i < sgn->Length(); i++) {
						SafePointer<RType> arg = new RType(_ctx, sgn->ElementAt(i).QueryCanonicalName());
						result->Append(arg);
					}
				} else if (IsTemplated()) {
					auto icn = _ref.GetClassName().Fragment(12, -1);
					auto iref = XI::Module::TypeReference(icn);
					while (iref.GetReferenceClass() == XI::Module::TypeReference::Class::AbstractInstance) {
						auto sref = iref.GetAbstractInstanceBase();
						auto tref = iref.GetAbstractInstanceParameterType();
						auto tidx = iref.GetAbstractInstanceParameterIndex();
						while (result->Length() <= tidx) result->Append(0);
						SafePointer<RType> arg = new RType(_ctx, tref.QueryCanonicalName());
						result->SetElement(arg, tidx);
						MemoryCopy(&iref, &sref, sizeof(iref));
					}
				} else throw Exception();
				return result;
				XE_TRY_OUTRO(0);
			}
			virtual string GetBaseName(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (IsTemplated()) {
					auto icn = _ref.GetClassName().Fragment(12, -1);
					auto iref = XI::Module::TypeReference(icn);
					while (iref.GetReferenceClass() == XI::Module::TypeReference::Class::AbstractInstance) {
						auto sref = iref.GetAbstractInstanceBase();
						MemoryCopy(&iref, &sref, sizeof(iref));
					}
					return iref.GetClassName();
				} else if (IsClass()) return _ref.GetClassName(); else throw Exception();
				XE_TRY_OUTRO(L"");
			}
		};
		class RSymbol : public DynamicObject
		{
		protected:
			const SymbolObject * _smbl;
			const Module * _ctx;
		public:
			RSymbol(const SymbolObject * smbl, const Module * ctx) : _smbl(smbl), _ctx(ctx) {}
			virtual ~RSymbol(void) override {}
			virtual void * GetType(void) noexcept override { return 0; }
			virtual uint GetSymbolClass(void) noexcept = 0;
			virtual const Module * GetContext(void) noexcept { return _ctx; }
			virtual const void * GetHandle(void) noexcept = 0;
			virtual SafePointer<Object> GetAttributes(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< Volumes::Dictionary<string, string> > attr = new Volumes::Dictionary<string, string>(*_smbl->GetAttributes());
				return CreateMetadataDictionary(attr);
				XE_TRY_OUTRO(0)
			}
			const ExecutionContext & GetExecutionContext(void) const noexcept { return _ctx->GetExecutionContext(); }
			static SafePointer<RSymbol> WrapSymbol(const Module * context, const SymbolObject * handle);
		};
		class RValueSymbol : public RSymbol
		{
			const LiteralSymbol * _lit;
			const VariableSymbol * _var;
		public:
			RValueSymbol(const LiteralSymbol * smbl, const Module * ctx) : RSymbol(smbl, ctx), _lit(smbl), _var(0) {}
			RValueSymbol(const VariableSymbol * smbl, const Module * ctx) : RSymbol(smbl, ctx), _lit(0), _var(smbl) {}
			virtual ~RValueSymbol(void) override {}
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept override
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				if (cls->GetClassName() == L"objectum") {
					Retain(); return static_cast<Object *>(this);
				} else if (cls->GetClassName() == L"objectum_dynamicum") {
					Retain(); return static_cast<DynamicObject *>(this);
				} else if (cls->GetClassName() == L"repulsus.symboli") {
					Retain(); return static_cast<RSymbol *>(this);
				} else if (cls->GetClassName() == L"repulsus.symbolum_valoris") {
					Retain(); return static_cast<RValueSymbol *>(this);
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual uint GetSymbolClass(void) noexcept override { return 2; }
			virtual const void * GetHandle(void) noexcept override { return 0; }
			virtual void * GetAddress(void) noexcept { return _lit ? _lit->GetSymbolEntity() : _var->GetSymbolEntity(); }
			virtual SafePointer<RType> GetType(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_lit) {
					auto data_type = _lit->GetValueType();
					if (data_type == Reflection::PropertyType::String) {
						return new RType(_ctx, L"PCnint32");
					} else if (data_type == Reflection::PropertyType::Double) {
						return new RType(_ctx, L"Cdfrac");
					} else if (data_type == Reflection::PropertyType::Float) {
						return new RType(_ctx, L"Cfrac");
					} else if (data_type == Reflection::PropertyType::UInt64) {
						return new RType(_ctx, L"Cnint64");
					} else if (data_type == Reflection::PropertyType::Int64) {
						return new RType(_ctx, L"Cint64");
					} else if (data_type == Reflection::PropertyType::UInt32) {
						return new RType(_ctx, L"Cnint32");
					} else if (data_type == Reflection::PropertyType::Int32) {
						return new RType(_ctx, L"Cint32");
					} else if (data_type == Reflection::PropertyType::UInt16) {
						return new RType(_ctx, L"Cnint16");
					} else if (data_type == Reflection::PropertyType::Int16) {
						return new RType(_ctx, L"Cint16");
					} else if (data_type == Reflection::PropertyType::UInt8) {
						return new RType(_ctx, L"Cnint8");
					} else if (data_type == Reflection::PropertyType::Int8) {
						return new RType(_ctx, L"Cint8");
					} else if (data_type == Reflection::PropertyType::Boolean) {
						return new RType(_ctx, L"Clogicum");
					} else throw InvalidStateException();
				} else if (_var) return new RType(_ctx, _var->GetType());
				else throw InvalidStateException();
				XE_TRY_OUTRO(0)
			}
			virtual bool IsConstant(void) noexcept { return _lit != 0; }
		};
		class RFunctionSymbol : public RSymbol
		{
			const FunctionSymbol * _func;
		public:
			RFunctionSymbol(const FunctionSymbol * smbl, const Module * ctx) : RSymbol(smbl, ctx), _func(smbl) {}
			virtual ~RFunctionSymbol(void) override {}
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept override
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				if (cls->GetClassName() == L"objectum") {
					Retain(); return static_cast<Object *>(this);
				} else if (cls->GetClassName() == L"objectum_dynamicum") {
					Retain(); return static_cast<DynamicObject *>(this);
				} else if (cls->GetClassName() == L"repulsus.symboli") {
					Retain(); return static_cast<RSymbol *>(this);
				} else if (cls->GetClassName() == L"repulsus.symbolum_functionis") {
					Retain(); return static_cast<RFunctionSymbol *>(this);
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual uint GetSymbolClass(void) noexcept override { return 4; }
			virtual const void * GetHandle(void) noexcept override { return 0; }
			virtual void * GetAddress(void) noexcept { return _func->GetSymbolEntity(); }
			virtual SafePointer<RType> GetType(ErrorContext & ectx) noexcept { XE_TRY_INTRO return new RType(_ctx, _func->GetType()); XE_TRY_OUTRO(0) }
			virtual bool IsThrowing(void) noexcept { return _func->GetFlags() & XI::Module::Function::FunctionThrows; }
			virtual bool IsInstance(void) noexcept { return _func->GetFlags() & XI::Module::Function::FunctionInstance; }
			virtual bool IsThisCall(void) noexcept { return _func->GetFlags() & XI::Module::Function::FunctionThisCall; }
			const FunctionSymbol * Expose(void) const noexcept { return _func; }
		};
		class RClassMethod : public Object
		{
			const Module * _ctx;
			const ClassSymbol * _cls;
			string _name;
		public:
			RClassMethod(const Module * ctx, const ClassSymbol * cls, const string & name) : _ctx(ctx), _cls(cls), _name(name) {}
			virtual ~RClassMethod(void) override {}
			virtual string GetName(ErrorContext & ectx) noexcept { XE_TRY_INTRO return _name; XE_TRY_OUTRO(0) }
			virtual SafePointer<Object> GetAttributes(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto attrs = _cls->GetMethodAttributes(_name);
				if (!attrs) throw InvalidStateException();
				SafePointer< Volumes::Dictionary<string, string> > attr = new Volumes::Dictionary<string, string>(*attrs);
				return CreateMetadataDictionary(attr);
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<RFunctionSymbol> GetImplementation(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto imp = _cls->GetMethodImplementation(_name);
				if (!imp || imp->Length() == 0) throw Exception();
				auto smbl = _ctx->GetExecutionContext().GetSymbol(*imp);
				if (!smbl || smbl->GetSymbolType() != SymbolType::Function) throw InvalidStateException();
				auto wrap = RSymbol::WrapSymbol(_ctx, smbl);
				SafePointer<RFunctionSymbol> result;
				result.SetRetain(static_cast<RFunctionSymbol *>(wrap.Inner()));
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual void GetVirtualImplementation(uintptr & rebasement, uintptr & vftoffs, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				int vft_index, vf_index;
				_cls->GetMethodVFI(_name, &vft_index, &vf_index);
				if (vft_index < 0 || vf_index < 0) throw Exception();
				vftoffs = vf_index * sizeof(handle);
				if (vft_index) {
					uintptr offset = 0;
					bool found = false;
					int intcnt = 0;
					auto enumf = [&offset, &found, &intcnt, vft_index](const ClassSymbol * smbl, uintptr rebase, bool isint, bool isparent) -> bool {
						if (isint && !isparent) {
							intcnt++;
							if (intcnt == vft_index) {
								offset = rebase;
								found = true;
								return true;
							} else return false;
						} else return false;
					};
					LookUpHierarchy(_ctx->GetExecutionContext(), _cls, enumf);
					if (!found) throw InvalidStateException();
					rebasement = offset;
				} else rebasement = 0;
				XE_TRY_OUTRO()
			}
			const ExecutionContext & GetExecutionContext(void) const noexcept { return _ctx->GetExecutionContext(); }
		};
		class RClassField : public Object
		{
			const Module * _ctx;
			const ClassSymbol * _cls;
			string _name;
		public:
			RClassField(const Module * ctx, const ClassSymbol * cls, const string & name) : _ctx(ctx), _cls(cls), _name(name) {}
			virtual ~RClassField(void) override {}
			virtual string GetName(ErrorContext & ectx) noexcept { XE_TRY_INTRO return _name; XE_TRY_OUTRO(0) }
			virtual SafePointer<Object> GetAttributes(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto attrs = _cls->GetFieldAttributes(_name);
				if (!attrs) throw InvalidStateException();
				SafePointer< Volumes::Dictionary<string, string> > attr = new Volumes::Dictionary<string, string>(*attrs);
				return CreateMetadataDictionary(attr);
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<RType> GetType(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto pcn = _cls->GetFieldType(_name);
				if (!pcn) throw InvalidStateException();
				return new RType(_ctx, *pcn);
				XE_TRY_OUTRO(0)
			}
			virtual handle GetAddress(handle object) noexcept { return _cls->GetFieldAddress(_name, object); }
		};
		class RClassProperty : public Object
		{
			const Module * _ctx;
			const ClassSymbol * _cls;
			string _name;
		public:
			RClassProperty(const Module * ctx, const ClassSymbol * cls, const string & name) : _ctx(ctx), _cls(cls), _name(name) {}
			virtual ~RClassProperty(void) override {}
			virtual string GetName(ErrorContext & ectx) noexcept { XE_TRY_INTRO return _name; XE_TRY_OUTRO(0) }
			virtual SafePointer<Object> GetAttributes(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto attrs = _cls->GetPropertyAttributes(_name);
				if (!attrs) throw InvalidStateException();
				SafePointer< Volumes::Dictionary<string, string> > attr = new Volumes::Dictionary<string, string>(*attrs);
				return CreateMetadataDictionary(attr);
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<RType> GetType(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto pcn = _cls->GetPropertyType(_name);
				if (!pcn) throw InvalidStateException();
				return new RType(_ctx, *pcn);
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<RClassMethod> GetReadMethod(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				string method;
				_cls->GetPropertyMethods(_name, &method, 0);
				if (!method.Length()) throw Exception();
				return new RClassMethod(_ctx, _cls, method);
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<RClassMethod> GetWriteMethod(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				string method;
				_cls->GetPropertyMethods(_name, 0, &method);
				if (!method.Length()) throw Exception();
				return new RClassMethod(_ctx, _cls, method);
				XE_TRY_OUTRO(0)
			}
		};
		class RClassSymbol : public RSymbol
		{
			const ClassSymbol * _cls;
		public:
			RClassSymbol(const ClassSymbol * smbl, const Module * ctx) : RSymbol(smbl, ctx), _cls(smbl) {}
			virtual ~RClassSymbol(void) override {}
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept override
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				if (cls->GetClassName() == L"objectum") {
					Retain(); return static_cast<Object *>(this);
				} else if (cls->GetClassName() == L"objectum_dynamicum") {
					Retain(); return static_cast<DynamicObject *>(this);
				} else if (cls->GetClassName() == L"repulsus.symboli") {
					Retain(); return static_cast<RSymbol *>(this);
				} else if (cls->GetClassName() == L"repulsus.symbolum_generis") {
					Retain(); return static_cast<RClassSymbol *>(this);
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual uint GetSymbolClass(void) noexcept override { return 1; }
			virtual const void * GetHandle(void) noexcept override { return _cls; }
			virtual string GetClassName(ErrorContext & ectx) noexcept { XE_TRY_INTRO return _cls->GetClassName(); XE_TRY_OUTRO(L""); }
			virtual uint GetSemantics(void) noexcept { return uint(_cls->GetClassSemantics()); }
			virtual uintptr GetSize(void) noexcept { return _cls->GetInstanceSize(); }
			virtual SafePointer<RClassSymbol> GetParentClass(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (_cls->GetParentClassName().Length()) {
					auto smbl = _ctx->GetExecutionContext().GetSymbol(_cls->GetParentClassName());
					if (!smbl || smbl->GetSymbolType() != SymbolType::Class) throw InvalidStateException();
					auto wrapped = RSymbol::WrapSymbol(_ctx, smbl);
					wrapped->Retain();
					return static_cast<RClassSymbol *>(wrapped.Inner());
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer< ObjectArray<RClassSymbol> > GetParentClasses(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< ObjectArray<RClassSymbol> > result = new ObjectArray<RClassSymbol>(0x10);
				SafePointer<RClassSymbol> current;
				current.SetRetain(this);
				while (true) {
					current = current->GetParentClass(ectx);
					if (ectx.error_code == 1) { ectx.error_code = ectx.error_subcode = 0; break; }
					else if (ectx.error_code != 0) return 0;
					result->Append(current);
				}
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer< ObjectArray<RClassSymbol> > GetInterfaces(bool all, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< ObjectArray<RClassSymbol> > result = new ObjectArray<RClassSymbol>(0x40);
				if (all) {
					auto enumf = [result, ctx = _ctx](const ClassSymbol * smbl, uintptr rebase, bool isint, bool isparent) -> bool {
						if (isint) {
							auto wcls = RSymbol::WrapSymbol(ctx, smbl);
							result->Append(static_cast<RClassSymbol *>(wcls.Inner()));
						}
						return false;
					};
					LookUpHierarchy(_ctx->GetExecutionContext(), _cls, enumf);
				} else {
					SafePointer< Array<string> > il = _cls->ListInterfaces();
					if (!il) throw OutOfMemoryException();
					for (auto & in : il->Elements()) {
						auto smbl = _ctx->GetExecutionContext().GetSymbol(in);
						if (!smbl || smbl->GetSymbolType() != SymbolType::Class) throw InvalidStateException();
						auto wcls = RSymbol::WrapSymbol(_ctx, smbl);
						result->Append(static_cast<RClassSymbol *>(wcls.Inner()));
					}
				}
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual void * ConvertToClass(RClassSymbol * cls, void * object, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				bool found = false;
				uintptr rebasement = 0;
				auto enumf = [&found, &rebasement, clsto = cls](const ClassSymbol * smbl, uintptr rebase, bool isint, bool isparent) -> bool {
					if (smbl == clsto->_cls) {
						found = true;
						rebasement = rebase;
						return true;
					} else return false;
				};
				LookUpHierarchy(_ctx->GetExecutionContext(), _cls, enumf);
				if (found) return reinterpret_cast<handle>(reinterpret_cast<uintptr>(object) + rebasement);
				else throw Exception();
				XE_TRY_OUTRO(0)
			}
			virtual void * ConvertFromClass(RClassSymbol * cls, void * object, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				bool found = false;
				uintptr rebasement = 0;
				auto enumf = [&found, &rebasement, clsto = cls](const ClassSymbol * smbl, uintptr rebase, bool isint, bool isparent) -> bool {
					if (smbl == clsto->_cls) {
						found = true;
						rebasement = rebase;
						return true;
					} else return false;
				};
				LookUpHierarchy(_ctx->GetExecutionContext(), _cls, enumf);
				if (found) return reinterpret_cast<handle>(reinterpret_cast<uintptr>(object) - rebasement);
				else throw Exception();
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer< ObjectArray<RClassField> > GetFieldsA(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< ObjectArray<RClassField> > result = new ObjectArray<RClassField>(0x20);
				SafePointer< Array<string> > names = _cls->ListFields();
				for (auto & name : names->Elements()) {
					SafePointer<RClassField> obj = new RClassField(_ctx, _cls, name);
					result->Append(obj);
				}
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer< ObjectArray<RClassField> > GetFieldsB(const string & attr, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< ObjectArray<RClassField> > result = new ObjectArray<RClassField>(0x20);
				SafePointer< Array<string> > names = _cls->ListFields(attr);
				for (auto & name : names->Elements()) {
					SafePointer<RClassField> obj = new RClassField(_ctx, _cls, name);
					result->Append(obj);
				}
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer< ObjectArray<RClassField> > GetFieldsC(const string & attr, const string & value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< ObjectArray<RClassField> > result = new ObjectArray<RClassField>(0x20);
				SafePointer< Array<string> > names = _cls->ListFields(attr, value);
				for (auto & name : names->Elements()) {
					SafePointer<RClassField> obj = new RClassField(_ctx, _cls, name);
					result->Append(obj);
				}
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer< ObjectArray<RClassProperty> > GetPropertiesA(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< ObjectArray<RClassProperty> > result = new ObjectArray<RClassProperty>(0x20);
				SafePointer< Array<string> > names = _cls->ListProperties();
				for (auto & name : names->Elements()) {
					SafePointer<RClassProperty> obj = new RClassProperty(_ctx, _cls, name);
					result->Append(obj);
				}
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer< ObjectArray<RClassProperty> > GetPropertiesB(const string & attr, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< ObjectArray<RClassProperty> > result = new ObjectArray<RClassProperty>(0x20);
				SafePointer< Array<string> > names = _cls->ListProperties(attr);
				for (auto & name : names->Elements()) {
					SafePointer<RClassProperty> obj = new RClassProperty(_ctx, _cls, name);
					result->Append(obj);
				}
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer< ObjectArray<RClassProperty> > GetPropertiesC(const string & attr, const string & value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< ObjectArray<RClassProperty> > result = new ObjectArray<RClassProperty>(0x20);
				SafePointer< Array<string> > names = _cls->ListProperties(attr, value);
				for (auto & name : names->Elements()) {
					SafePointer<RClassProperty> obj = new RClassProperty(_ctx, _cls, name);
					result->Append(obj);
				}
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer< ObjectArray<RClassMethod> > GetMethodsA(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< ObjectArray<RClassMethod> > result = new ObjectArray<RClassMethod>(0x20);
				SafePointer< Array<string> > names = _cls->ListMethods();
				for (auto & name : names->Elements()) {
					SafePointer<RClassMethod> obj = new RClassMethod(_ctx, _cls, name);
					result->Append(obj);
				}
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer< ObjectArray<RClassMethod> > GetMethodsB(const string & attr, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< ObjectArray<RClassMethod> > result = new ObjectArray<RClassMethod>(0x20);
				SafePointer< Array<string> > names = _cls->ListMethods(attr);
				for (auto & name : names->Elements()) {
					SafePointer<RClassMethod> obj = new RClassMethod(_ctx, _cls, name);
					result->Append(obj);
				}
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer< ObjectArray<RClassMethod> > GetMethodsC(const string & attr, const string & value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< ObjectArray<RClassMethod> > result = new ObjectArray<RClassMethod>(0x20);
				SafePointer< Array<string> > names = _cls->ListMethods(attr, value);
				for (auto & name : names->Elements()) {
					SafePointer<RClassMethod> obj = new RClassMethod(_ctx, _cls, name);
					result->Append(obj);
				}
				return result;
				XE_TRY_OUTRO(0)
			}
		};
		class RInvocationAdapter : public Object
		{
		public:
			typedef void (* AdapterRoutine) (void * retval, void * self, void ** argv, ErrorContext & ectx);
			struct AdapterDesc {
				string signature_cn;
				void * direct_call_address;
				uintptr rebasement, vft_offset;
				bool virtual_call, func_throws, func_instance, func_thiscall;
			};
		private:
			SafePointer<XA::IExecutable> _exec;
			AdapterRoutine _routine;
		public:
			RInvocationAdapter(XA::IExecutable * exec, AdapterRoutine routine) : _routine(routine) { _exec.SetRetain(exec); }
			virtual ~RInvocationAdapter(void) override {}
			virtual void Invoke(void * retval, void * self, void ** argv, ErrorContext & ectx) noexcept { _routine(retval, self, argv, ectx); }
			static XA::ArgumentSpecification GetTypeInstanceSpec(const ExecutionContext & ctx, const XI::Module::TypeReference & ref)
			{
				auto rcls = ref.GetReferenceClass();
				if (rcls == XI::Module::TypeReference::Class::Pointer || rcls == XI::Module::TypeReference::Class::Reference) {
					return XA::TH::MakeSpec(0, 1);
				} else if (rcls == XI::Module::TypeReference::Class::Array) {
					auto vol = ref.GetArrayVolume();
					auto ispec = GetTypeInstanceSpec(ctx, ref.GetArrayElement());
					return XA::TH::MakeSpec(XA::ArgumentSemantics::Object, ispec.size.num_bytes * vol, ispec.size.num_words * vol);
				} else if (rcls == XI::Module::TypeReference::Class::Class) {
					auto smbl = ctx.GetSymbol(ref.GetClassName());
					if (!smbl || smbl->GetSymbolType() != SymbolType::Class) throw InvalidStateException();
					auto cls = static_cast<const ClassSymbol *>(smbl);
					return XA::TH::MakeSpec(cls->GetClassSemantics(), cls->GetInstanceSize(), 0);
				} else throw InvalidStateException();
			}
			static SafePointer< ObjectArray<RInvocationAdapter> > CreateAdapters(const ExecutionContext & ctx, const Array<AdapterDesc> & desc)
			{
				SafePointer< ObjectArray<RInvocationAdapter> > result = new ObjectArray<RInvocationAdapter>(desc.Length());
				Array<XA::Function> xaf(desc.Length());
				Array<XA::TranslatedFunction> tf(desc.Length());
				for (auto & d : desc) {
					xaf << XA::Function();
					auto & f = xaf.LastElement();
					f.retval = XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 0);
					f.inputs << XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 1) << XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 1);
					f.inputs << XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 1) << XA::TH::MakeSpec(XA::ArgumentSemantics::ErrorData, 0, 1);
					if (!d.virtual_call) {
						f.data.SetLength(sizeof(handle));
						MemoryCopy(f.data.GetBuffer(), &d.direct_call_address, sizeof(handle));
					}
					XA::ExpressionTree self, vft, call;
					self = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceArgument, 1));
					vft = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformFollowPointer, XA::ReferenceFlagInvoke));
					XA::TH::AddTreeInput(vft, self, XA::TH::MakeSpec(0, 1));
					XA::TH::AddTreeOutput(vft, XA::TH::MakeSpec(d.rebasement, 1));
					if (d.rebasement) {
						self = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformAddressOffset, XA::ReferenceFlagInvoke));
						XA::TH::AddTreeInput(self, vft, XA::TH::MakeSpec(d.rebasement, 1));
						XA::TH::AddTreeInput(self, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLiteral)), XA::TH::MakeSpec(d.rebasement, 0));
						XA::TH::AddTreeOutput(self, XA::TH::MakeSpec(0, 1));
						vft = self;
						self = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformTakePointer, XA::ReferenceFlagInvoke));
						XA::TH::AddTreeInput(self, vft, XA::TH::MakeSpec(0, 1));
						XA::TH::AddTreeOutput(self, XA::TH::MakeSpec(0, 1));
					}
					call = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformFollowPointer, XA::ReferenceFlagInvoke));
					XA::TH::AddTreeInput(call, vft, XA::TH::MakeSpec(0, 1));
					XA::TH::AddTreeOutput(call, XA::TH::MakeSpec(d.vft_offset, 1));
					vft = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformAddressOffset, XA::ReferenceFlagInvoke));
					XA::TH::AddTreeInput(vft, call, XA::TH::MakeSpec(d.vft_offset, 1));
					XA::TH::AddTreeInput(vft, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLiteral)), XA::TH::MakeSpec(d.vft_offset, 0));
					XA::TH::AddTreeOutput(vft, XA::TH::MakeSpec(0, 1));
					call = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformInvoke, XA::ReferenceFlagInvoke));
					if (d.virtual_call) XA::TH::AddTreeInput(call, vft, XA::TH::MakeSpec(0, 1));
					else XA::TH::AddTreeInput(call, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceData, 0)), XA::TH::MakeSpec(0, 1));
					if (d.func_instance) XA::TH::AddTreeInput(call, self, XA::TH::MakeSpec(d.func_thiscall ? XA::ArgumentSemantics::This : XA::ArgumentSemantics::Unclassified, 0, 1));
					XI::Module::TypeReference ftype(d.signature_cn);
					SafePointer< Array<XI::Module::TypeReference> > fsign = ftype.GetFunctionSignature();
					for (int i = 1; i < fsign->Length(); i++) {
						auto rcls = fsign->ElementAt(i).GetReferenceClass();
						auto aspec = GetTypeInstanceSpec(ctx, fsign->ElementAt(i));
						XA::ExpressionTree argv, arg;
						argv = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformFollowPointer, XA::ReferenceFlagInvoke));
						XA::TH::AddTreeInput(argv, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceArgument, 2)), XA::TH::MakeSpec(0, 1));
						XA::TH::AddTreeOutput(argv, XA::TH::MakeSpec(0, i));
						if (i > 1) {
							arg = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformAddressOffset, XA::ReferenceFlagInvoke));
							XA::TH::AddTreeInput(arg, argv, XA::TH::MakeSpec(0, i));
							XA::TH::AddTreeInput(arg, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLiteral)), XA::TH::MakeSpec(0, i - 1));
							XA::TH::AddTreeOutput(arg, XA::TH::MakeSpec(0, 1));
						} else arg = argv;
						if (rcls == XI::Module::TypeReference::Class::Pointer || rcls == XI::Module::TypeReference::Class::Reference) {
							XA::TH::AddTreeInput(call, arg, aspec);
						} else {
							argv = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformFollowPointer, XA::ReferenceFlagInvoke));
							XA::TH::AddTreeInput(argv, arg, XA::TH::MakeSpec(0, 1));
							XA::TH::AddTreeOutput(argv, aspec);
							XA::TH::AddTreeInput(call, argv, aspec);
						}
					}
					if (d.func_throws) XA::TH::AddTreeInput(call, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceArgument, 3)), XA::TH::MakeSpec(XA::ArgumentSemantics::ErrorData, 0, 1));
					auto rspec = GetTypeInstanceSpec(ctx, fsign->ElementAt(0));
					XA::TH::AddTreeOutput(call, rspec);
					if (rspec.size.num_bytes || rspec.size.num_words) {
						self = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformFollowPointer, XA::ReferenceFlagInvoke));
						XA::TH::AddTreeInput(self, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceArgument, 0)), XA::TH::MakeSpec(0, 1));
						XA::TH::AddTreeOutput(self, rspec);
						vft = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformBlockTransfer, XA::ReferenceFlagInvoke));
						XA::TH::AddTreeInput(vft, self, rspec);
						XA::TH::AddTreeInput(vft, call, rspec);
						XA::TH::AddTreeOutput(vft, rspec);
						f.instset << XA::TH::MakeStatementReturn(vft);
					} else f.instset << XA::TH::MakeStatementReturn(call);
				}
				if (!ctx.TranslateFunctions(xaf, tf)) throw Exception();
				Volumes::Dictionary<string, XA::TranslatedFunction *> mapping;
				for (int i = 0; i < tf.Length(); i++) mapping.Append(i, &tf[i]);
				SafePointer<XA::IExecutable> exec = ctx.LinkFunctions(mapping);
				for (int i = 0; i < tf.Length(); i++) {
					SafePointer<RInvocationAdapter> adapter = new RInvocationAdapter(exec, exec->GetEntryPoint<AdapterRoutine>(i));
					result->Append(adapter);
				}
				return result;
			}
		};

		class ReflectionAPI : public IAPIExtension
		{
			static SafePointer<RModule> _open_module_handle(const Module * module, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!module) throw InvalidArgumentException();
				return new RModule(module);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<RModule> _find_module(const Module * context, const string & name, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!context) throw InvalidArgumentException();
				auto handle = context->GetExecutionContext().GetModule(name);
				if (!handle) throw IO::FileAccessException(IO::Error::FileNotFound);
				return new RModule(handle);
				XE_TRY_OUTRO(0)
			}
			static SafePointer< ObjectArray<RModule> > _list_modules(const Module * context, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!context) throw InvalidArgumentException();
				SafePointer< ObjectArray<RModule> > result = new ObjectArray<RModule>(0x40);
				SafePointer< Volumes::Dictionary<string, const Module *> > query = context->GetExecutionContext().GetLoadedModules();
				if (!query) throw OutOfMemoryException();
				for (auto & q : query->Elements()) {
					SafePointer<RModule> module = new RModule(q.value);
					result->Append(module);
				}
				return result;
				XE_TRY_OUTRO(0)
			}
			static SafePointer<RType> _open_type_handle(const Module * context, const ClassSymbol * handle, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!context || !handle) throw InvalidArgumentException();
				auto cn = XI::Module::TypeReference::MakeClassReference(handle->GetClassName());
				return new RType(context, cn);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<RSymbol> _open_class_symbol(const Module * context, const ClassSymbol * handle, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return RSymbol::WrapSymbol(context, handle);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<RSymbol> _get_symbol(const Module * context, const string & name, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!context) throw InvalidArgumentException();
				auto smbl = context->GetExecutionContext().GetSymbol(name);
				if (!smbl) throw IO::FileAccessException(IO::Error::FileNotFound);
				return RSymbol::WrapSymbol(context, smbl);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<RSymbol> _find_symbol_a(const Module * context, uint type, const string & attr, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!context) throw InvalidArgumentException();
				const SymbolObject * smbl = 0;
				if (type & 1) smbl = context->GetExecutionContext().GetSymbol(SymbolType::Class, attr);
				if (!smbl && type & 2) {
					smbl = context->GetExecutionContext().GetSymbol(SymbolType::Literal, attr);
					if (!smbl) smbl = context->GetExecutionContext().GetSymbol(SymbolType::Variable, attr);
				}
				if (!smbl && type & 4) smbl = context->GetExecutionContext().GetSymbol(SymbolType::Function, attr);
				if (!smbl) throw IO::FileAccessException(IO::Error::FileNotFound);
				return RSymbol::WrapSymbol(context, smbl);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<RSymbol> _find_symbol_b(const Module * context, uint type, const string & attr, const string & value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!context) throw InvalidArgumentException();
				const SymbolObject * smbl = 0;
				if (type & 1) smbl = context->GetExecutionContext().GetSymbol(SymbolType::Class, attr, value);
				if (!smbl && type & 2) {
					smbl = context->GetExecutionContext().GetSymbol(SymbolType::Literal, attr, value);
					if (!smbl) smbl = context->GetExecutionContext().GetSymbol(SymbolType::Variable, attr, value);
				}
				if (!smbl && type & 4) smbl = context->GetExecutionContext().GetSymbol(SymbolType::Function, attr, value);
				if (!smbl) throw IO::FileAccessException(IO::Error::FileNotFound);
				return RSymbol::WrapSymbol(context, smbl);
				XE_TRY_OUTRO(0)
			}
			static SafePointer< ObjectArray<RSymbol> > _get_symbols_a(const Module * context, uint type, const string & attr, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!context) throw InvalidArgumentException();
				SafePointer< Volumes::Dictionary<string, const SymbolObject *> > q1, q2, q3, q4;
				if (type & 1) q1 = context->GetExecutionContext().GetSymbols(SymbolType::Class, attr);
				if (type & 2) {
					q2 = context->GetExecutionContext().GetSymbols(SymbolType::Literal, attr);
					q3 = context->GetExecutionContext().GetSymbols(SymbolType::Variable, attr);
				}
				if (type & 4) q4 = context->GetExecutionContext().GetSymbols(SymbolType::Function, attr);
				SafePointer< ObjectArray<RSymbol> > result = new ObjectArray<RSymbol>(0x100);
				if (q1) for (auto & q : q1->Elements()) {
					SafePointer<RSymbol> smbl = RSymbol::WrapSymbol(context, q.value);
					result->Append(smbl);
				}
				if (q2) for (auto & q : q2->Elements()) {
					SafePointer<RSymbol> smbl = RSymbol::WrapSymbol(context, q.value);
					result->Append(smbl);
				}
				if (q3) for (auto & q : q3->Elements()) {
					SafePointer<RSymbol> smbl = RSymbol::WrapSymbol(context, q.value);
					result->Append(smbl);
				}
				if (q4) for (auto & q : q4->Elements()) {
					SafePointer<RSymbol> smbl = RSymbol::WrapSymbol(context, q.value);
					result->Append(smbl);
				}
				return result;
				XE_TRY_OUTRO(0)
			}
			static SafePointer< ObjectArray<RSymbol> > _get_symbols_b(const Module * context, uint type, const string & attr, const string & value, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!context) throw InvalidArgumentException();
				SafePointer< Volumes::Dictionary<string, const SymbolObject *> > q1, q2, q3, q4;
				if (type & 1) q1 = context->GetExecutionContext().GetSymbols(SymbolType::Class, attr, value);
				if (type & 2) {
					q2 = context->GetExecutionContext().GetSymbols(SymbolType::Literal, attr, value);
					q3 = context->GetExecutionContext().GetSymbols(SymbolType::Variable, attr, value);
				}
				if (type & 4) q4 = context->GetExecutionContext().GetSymbols(SymbolType::Function, attr, value);
				SafePointer< ObjectArray<RSymbol> > result = new ObjectArray<RSymbol>(0x100);
				if (q1) for (auto & q : q1->Elements()) {
					SafePointer<RSymbol> smbl = RSymbol::WrapSymbol(context, q.value);
					result->Append(smbl);
				}
				if (q2) for (auto & q : q2->Elements()) {
					SafePointer<RSymbol> smbl = RSymbol::WrapSymbol(context, q.value);
					result->Append(smbl);
				}
				if (q3) for (auto & q : q3->Elements()) {
					SafePointer<RSymbol> smbl = RSymbol::WrapSymbol(context, q.value);
					result->Append(smbl);
				}
				if (q4) for (auto & q : q4->Elements()) {
					SafePointer<RSymbol> smbl = RSymbol::WrapSymbol(context, q.value);
					result->Append(smbl);
				}
				return result;
				XE_TRY_OUTRO(0)
			}
			static SafePointer< ObjectArray<RInvocationAdapter> > _create_adapters_a(SafePointer<RFunctionSymbol> * funcs, int count, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (count <= 0) throw InvalidArgumentException();
				Array<RInvocationAdapter::AdapterDesc> desc(count);
				for (int i = 0; i < count; i++) {
					RInvocationAdapter::AdapterDesc ad;
					if (!funcs[i] || funcs[i]->IsInstance()) throw InvalidArgumentException();
					ad.signature_cn = funcs[i]->Expose()->GetType();
					ad.direct_call_address = funcs[i]->GetAddress();
					ad.rebasement = ad.vft_offset = 0;
					ad.virtual_call = ad.func_instance = ad.func_thiscall = false;
					ad.func_throws = funcs[i]->IsThrowing();
					desc << ad;
				}
				return RInvocationAdapter::CreateAdapters(funcs[0]->GetExecutionContext(), desc);
				XE_TRY_OUTRO(0)
			}
			static SafePointer< ObjectArray<RInvocationAdapter> > _create_adapters_b(SafePointer<RClassMethod> * funcs, int count, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (count <= 0) throw InvalidArgumentException();
				Array<RInvocationAdapter::AdapterDesc> desc(count);
				for (int i = 0; i < count; i++) {
					ErrorContext ectx;
					ectx.error_code = ectx.error_subcode = 0;
					RInvocationAdapter::AdapterDesc ad;
					if (!funcs[i]) throw InvalidArgumentException();
					SafePointer<RFunctionSymbol> impl = funcs[i]->GetImplementation(ectx);
					if (ectx.error_code) throw InvalidArgumentException();
					ad.signature_cn = impl->Expose()->GetType();
					ad.func_throws = impl->IsThrowing();
					ad.func_instance = impl->IsInstance();
					ad.func_thiscall = impl->IsThisCall();
					funcs[i]->GetVirtualImplementation(ad.rebasement, ad.vft_offset, ectx);
					if (ectx.error_code == 0) {
						ad.direct_call_address = 0;
						ad.virtual_call = true;
					} else if (ectx.error_code == 1) {
						ad.direct_call_address = impl->GetAddress();
						ad.rebasement = ad.vft_offset = 0;
						ad.virtual_call = false;
					} else throw InvalidArgumentException();
					desc << ad;
				}
				return RInvocationAdapter::CreateAdapters(funcs[0]->GetExecutionContext(), desc);
				XE_TRY_OUTRO(0)
			}
		public:
			ReflectionAPI(void) {}
			virtual ~ReflectionAPI(void) override {}
			virtual const void * ExposeRoutine(const string & routine_name) noexcept override
			{
				if (string::Compare(routine_name, L"rp_sp0") < 0) {
					if (string::Compare(routine_name, L"rp_mp0") < 0) {
						if (string::Compare(routine_name, L"rp_ci1") < 0) {
							if (string::Compare(routine_name, L"rp_ci0") == 0) return reinterpret_cast<const void *>(&_create_adapters_a);
						} else {
							if (string::Compare(routine_name, L"rp_gx0") < 0) {
								if (string::Compare(routine_name, L"rp_ci1") == 0) return reinterpret_cast<const void *>(&_create_adapters_b);
							} else {
								if (string::Compare(routine_name, L"rp_gx0") == 0) return reinterpret_cast<const void *>(&_open_type_handle);
							}
						}
					} else {
						if (string::Compare(routine_name, L"rp_mr0") < 0) {
							if (string::Compare(routine_name, L"rp_mp0") == 0) return reinterpret_cast<const void *>(&_list_modules);
						} else {
							if (string::Compare(routine_name, L"rp_mx0") < 0) {
								if (string::Compare(routine_name, L"rp_mr0") == 0) return reinterpret_cast<const void *>(&_find_module);
							} else {
								if (string::Compare(routine_name, L"rp_mx0") == 0) return reinterpret_cast<const void *>(&_open_module_handle);
							}
						}
					}
				} else {
					if (string::Compare(routine_name, L"rp_sr0") < 0) {
						if (string::Compare(routine_name, L"rp_sp1") < 0) {
							if (string::Compare(routine_name, L"rp_sp0") == 0) return reinterpret_cast<const void *>(&_get_symbol);
						} else {
							if (string::Compare(routine_name, L"rp_sp2") < 0) {
								if (string::Compare(routine_name, L"rp_sp1") == 0) return reinterpret_cast<const void *>(&_get_symbols_a);
							} else {
								if (string::Compare(routine_name, L"rp_sp2") == 0) return reinterpret_cast<const void *>(&_get_symbols_b);
							}
						}
					} else {
						if (string::Compare(routine_name, L"rp_sr1") < 0) {
							if (string::Compare(routine_name, L"rp_sr0") == 0) return reinterpret_cast<const void *>(&_find_symbol_a);
						} else {
							if (string::Compare(routine_name, L"rp_sx0") < 0) {
								if (string::Compare(routine_name, L"rp_sr1") == 0) return reinterpret_cast<const void *>(&_find_symbol_b);
							} else {
								if (string::Compare(routine_name, L"rp_sx0") == 0) return reinterpret_cast<const void *>(&_open_class_symbol);
							}
						}
					}
				}
				return 0;
			}
			virtual const void * ExposeInterface(const string & interface) noexcept override { return 0; }
		};

		void ActivateReflectionAPI(StandardLoader & ldr)
		{
			SafePointer<ReflectionAPI> api = new ReflectionAPI;
			if (!ldr.RegisterAPIExtension(api)) throw Exception();
		}
		SafePointer<RSymbol> RSymbol::WrapSymbol(const Module * context, const SymbolObject * handle)
		{
			if (!context || !handle) throw InvalidArgumentException();
			if (handle->GetSymbolType() == SymbolType::Variable) {
				return new RValueSymbol(static_cast<const VariableSymbol *>(handle), context);
			} else if (handle->GetSymbolType() == SymbolType::Literal) {
				return new RValueSymbol(static_cast<const LiteralSymbol *>(handle), context);
			} else if (handle->GetSymbolType() == SymbolType::Function) {
				return new RFunctionSymbol(static_cast<const FunctionSymbol *>(handle), context);
			} else if (handle->GetSymbolType() == SymbolType::Class) {
				return new RClassSymbol(static_cast<const ClassSymbol *>(handle), context);
			} else throw InvalidArgumentException();
		}
	}
}