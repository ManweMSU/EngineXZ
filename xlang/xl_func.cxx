#include "xl_func.h"

#include "xl_var.h"
#include "xl_lit.h"

#include "../ximg/xi_function.h"

namespace Engine
{
	namespace XL
	{
		class MethodOverload : public XMethodOverload
		{
			SafePointer<XFunctionOverload> _parent;
			SafePointer<LObject> _instance;

			class _invoke_provider : public Object, public IComputableProvider
			{
			public:
				XA::ExpressionTree _tree_node;
				string _self_ref, _dtor_ref;
				bool _throws;
				SafePointer<XMethodOverload> _self;
				SafePointer<XType> _retval;
				SafePointer<LObject> _instance;
				ObjectArray<LObject> _args;
			public:
				_invoke_provider(void) : _args(0x10) {}
				virtual ~_invoke_provider(void) override {}
				virtual Object * ComputableProviderQueryObject(void) override { return this; }
				virtual XType * ComputableGetType(void) override { _retval->Retain(); return _retval; }
				virtual XA::ExpressionTree ComputableEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
				{
					if (_throws && !error_ctx) throw ObjectMayThrow(_self);
					_tree_node.inputs.Clear();
					_tree_node.inputs << MakeAddressOf(_instance->Evaluate(func, error_ctx), _self->GetInstanceType()->GetArgumentSpecification().size);
					for (auto & a : _args) _tree_node.inputs << a.Evaluate(func, error_ctx);
					if (_throws) _tree_node.inputs << MakeAddressOf(*error_ctx, XA::TH::MakeSize(0, 2));
					_tree_node.self = MakeSymbolReferenceL(func, _self_ref);
					_tree_node.self.ref_flags = XA::ReferenceFlagInvoke;
					_tree_node.retval_final.final = _dtor_ref.Length() ? MakeSymbolReferenceL(func, _dtor_ref) : XA::TH::MakeRef();
					if (_throws) {
						XA::ExpressionTree catch_node = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform,
							XA::TransformBreakIf, XA::ReferenceFlagInvoke));
						XA::TH::AddTreeInput(catch_node, _tree_node, _tree_node.retval_spec);
						XA::TH::AddTreeInput(catch_node, *error_ctx, XA::TH::MakeSpec(0, 1));
						XA::TH::AddTreeInput(catch_node, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLiteral)), XA::TH::MakeSpec());
						XA::TH::AddTreeOutput(catch_node, _tree_node.retval_spec);
						return catch_node;
					} else return _tree_node;
				}
			};
		public:
			MethodOverload(XFunctionOverload * parent, LObject * instance) { _parent.SetRetain(parent); _instance.SetRetain(instance); }
			virtual ~MethodOverload(void) override {}
			virtual Class GetClass(void) override { return Class::MethodOverload; }
			virtual string GetName(void) override { return _parent->GetName(); }
			virtual string GetFullName(void) override { return _parent->GetFullName(); }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetType(void) override { return _parent->GetType(); }
			virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
			virtual LObject * Invoke(int argc, LObject ** argv) override
			{
				if (_parent->GetFlags() & XI::Module::Function::FunctionInstance) {
					if (argc == 0 && _instance->GetClass() == Class::Literal) {
						auto op = GetName().Fragment(0, GetName().FindFirst(L':'));
						SafePointer<LObject> lit = ProcessLiteralTransform(GetContext(), op, static_cast<XLiteral *>(_instance.Inner()), 0);
						if (lit) { lit->Retain(); return lit; }
					}
					auto & vfd = GetVFDesc();
					if (vfd.vf_index >= 0 && vfd.vft_index >= 0) {

						// TODO: IMPLEMENT VIRTUAL CALL
						throw LException(this);

					} else {
						auto ct = GetCanonicalType();
						SafePointer<_invoke_provider> provider = new _invoke_provider;
						SafePointer< Array<XI::Module::TypeReference> > sgn = XI::Module::TypeReference(ct).GetFunctionSignature();
						if (sgn->Length() != argc + 1) throw ObjectHasNoSuchOverloadException(this, argc, argv);
						bool use_thiscall = (_parent->GetFlags() & XI::Module::Function::FunctionThisCall);
						provider->_throws = (_parent->GetFlags() & XI::Module::Function::FunctionThrows);
						provider->_instance = _instance;
						provider->_retval = CreateType(sgn->ElementAt(0).QueryCanonicalName(), GetContext());
						provider->_self_ref = _parent->GetFullName();
						provider->_self.SetRetain(this);
						SafePointer<LObject> dtor = provider->_retval->GetDestructor();
						if (dtor->GetClass() != Class::Null) provider->_dtor_ref = dtor->GetFullName();
						provider->_tree_node.retval_spec = provider->_retval->GetArgumentSpecification();
						provider->_tree_node.input_specs << XA::TH::MakeSpec(use_thiscall ? XA::ArgumentSemantics::This : XA::ArgumentSemantics::Unclassified, 0, 1);
						for (int i = 0; i < argc; i++) {
							SafePointer<XType> type_need = CreateType(sgn->ElementAt(i + 1).QueryCanonicalName(), GetContext());
							SafePointer<LObject> casted = PerformTypeCast(type_need, argv[i], CastPriorityConverter);
							if (type_need->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Reference) casted = UnwarpObject(casted);
							provider->_args.Append(casted);
							provider->_tree_node.input_specs << type_need->GetArgumentSpecification();
						}
						if (provider->_throws) provider->_tree_node.input_specs << XA::TH::MakeSpec(XA::ArgumentSemantics::ErrorData, 0, 1);
						return CreateComputable(GetContext(), provider);
					}
				} else return _parent->Invoke(argc, argv);
			}
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { return _parent->Evaluate(func, error_ctx); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
			virtual string ToString(void) const override { return _parent->ToString() + L" with instance " + _instance->ToString(); }
			virtual VirtualFunctionDesc & GetVFDesc(void) override { return _parent->GetVFDesc(); }
			virtual XClass * GetInstanceType(void) override { return _parent->GetInstanceType(); }
			virtual LContext & GetContext(void) override { return _parent->GetContext(); }
			virtual string GetCanonicalType(void) override { return _parent->GetCanonicalType(); }
		};
		class FunctionOverload : public XFunctionOverload
		{
			LContext & _ctx;
			string _name, _path, _cn;
			bool _local;
			uint _flags;
			FunctionImplementationDesc _impl;
			FunctionInlineDesc _inl;
			VirtualFunctionDesc _virt;
			Volumes::Dictionary<string, string> _attributes;
			SafePointer<XClass> _instance_type;

			class _invoke_provider : public Object, public IComputableProvider
			{
			public:
				XA::ExpressionTree _tree_node;
				string _self_ref, _dtor_ref;
				bool _throws;
				SafePointer<LObject> _self;
				SafePointer<XType> _retval;
				ObjectArray<LObject> _args;
			public:
				_invoke_provider(void) : _args(0x10) {}
				virtual ~_invoke_provider(void) override {}
				virtual Object * ComputableProviderQueryObject(void) override { return this; }
				virtual XType * ComputableGetType(void) override { _retval->Retain(); return _retval; }
				virtual XA::ExpressionTree ComputableEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
				{
					if (_throws && !error_ctx) throw ObjectMayThrow(_self);
					_tree_node.inputs.Clear();
					for (auto & a : _args) _tree_node.inputs << a.Evaluate(func, error_ctx);
					if (_throws) _tree_node.inputs << MakeAddressOf(*error_ctx, XA::TH::MakeSize(0, 2));
					_tree_node.self = MakeSymbolReferenceL(func, _self_ref);
					_tree_node.self.ref_flags = XA::ReferenceFlagInvoke;
					_tree_node.retval_final.final = _dtor_ref.Length() ? MakeSymbolReferenceL(func, _dtor_ref) : XA::TH::MakeRef();
					if (_throws) {
						XA::ExpressionTree catch_node = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform,
							XA::TransformBreakIf, XA::ReferenceFlagInvoke));
						XA::TH::AddTreeInput(catch_node, _tree_node, _tree_node.retval_spec);
						XA::TH::AddTreeInput(catch_node, *error_ctx, XA::TH::MakeSpec(0, 1));
						XA::TH::AddTreeInput(catch_node, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLiteral)), XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 0));
						XA::TH::AddTreeOutput(catch_node, _tree_node.retval_spec);
						return catch_node;
					} else return _tree_node;
				}
			};
		public:
			FunctionOverload(LContext & ctx, const string & name, const string & path, const string & cn, bool local, XClass * instance_type) :
				_ctx(ctx), _name(name), _path(path), _cn(cn), _local(local), _flags(0)
			{
				_instance_type.SetRetain(instance_type);
				_impl._pure = _inl._has_inline = false;
				_virt.vft_index = _virt.vf_index = -1;
				_virt.vfp_offset = _virt.vftp_offset = XA::TH::MakeSize(-1, -1);
				_virt.base_offset = XA::TH::MakeSize(0, 0);
				
				// TODO: ADD BUILT-IN INLINE
			}
			virtual ~FunctionOverload(void) override {}
			virtual Class GetClass(void) override { return Class::FunctionOverload; }
			virtual string GetName(void) override { return _name; }
			virtual string GetFullName(void) override { return _path; }
			virtual bool IsDefinedLocally(void) override { return _local; }
			virtual LObject * GetType(void) override
			{
				if (_flags & XI::Module::Function::FunctionInstance) throw ObjectHasNoTypeException(this);
				if (_flags & XI::Module::Function::FunctionThrows) throw ObjectHasNoTypeException(this);
				return CreateType(XI::Module::TypeReference::MakePointer(_cn), _ctx);
			}
			virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
			virtual LObject * Invoke(int argc, LObject ** argv) override
			{
				if (_flags & XI::Module::Function::FunctionInstance) throw ObjectHasNoSuchOverloadException(this, argc, argv);
				if (argc == 2 && argv[0]->GetClass() == Class::Literal && argv[1]->GetClass() == Class::Literal) {
					auto op = _path.Fragment(0, _path.FindFirst(L':'));
					SafePointer<LObject> lit = ProcessLiteralTransform(_ctx, op, static_cast<XLiteral *>(argv[0]), static_cast<XLiteral *>(argv[1]));
					if (lit) { lit->Retain(); return lit; }
				}
				SafePointer<_invoke_provider> provider = new _invoke_provider;
				SafePointer< Array<XI::Module::TypeReference> > sgn = XI::Module::TypeReference(_cn).GetFunctionSignature();
				if (sgn->Length() != argc + 1) throw ObjectHasNoSuchOverloadException(this, argc, argv);
				provider->_throws = (_flags & XI::Module::Function::FunctionThrows);
				provider->_retval = CreateType(sgn->ElementAt(0).QueryCanonicalName(), GetContext());
				provider->_self_ref = _path;
				provider->_self.SetRetain(this);
				SafePointer<LObject> dtor = provider->_retval->GetDestructor();
				if (dtor->GetClass() != Class::Null) provider->_dtor_ref = dtor->GetFullName();
				provider->_tree_node.retval_spec = provider->_retval->GetArgumentSpecification();
				for (int i = 0; i < argc; i++) {
					SafePointer<XType> type_need = CreateType(sgn->ElementAt(i + 1).QueryCanonicalName(), GetContext());
					SafePointer<LObject> casted = PerformTypeCast(type_need, argv[i], CastPriorityConverter);
					if (type_need->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Reference) casted = UnwarpObject(casted);
					provider->_args.Append(casted);
					provider->_tree_node.input_specs << type_need->GetArgumentSpecification();
				}
				if (provider->_throws) provider->_tree_node.input_specs << XA::TH::MakeSpec(XA::ArgumentSemantics::ErrorData, 0, 1);
				return CreateComputable(GetContext(), provider);
			}
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { if (!_attributes.Append(key, value)) throw ObjectMemberRedefinitionException(this, key); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
			{
				if (_flags & XI::Module::Function::FunctionInstance) throw ObjectIsNotEvaluatableException(this);
				if (_flags & XI::Module::Function::FunctionThrows) throw ObjectIsNotEvaluatableException(this);
				return MakeAddressOf(MakeSymbolReference(func, _path), XA::TH::MakeSize(0, 1));
			}
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override
			{
				if (!_local) return;
				XI::Module::Function func;
				func.attributes = _attributes;
				func.vft_index.x = _virt.vft_index;
				func.vft_index.y = _virt.vf_index;
				func.code_flags = _flags;
				if (!_impl._pure) {
					if (_impl._import_func.Length()) {
						if (_impl._import_library.Length()) XI::MakeFunction(func, _impl._import_func, _impl._import_library);
						else XI::MakeFunction(func, _impl._import_func);
					} else XI::MakeFunction(func, _impl._xa);
				} else XI::MakeFunction(func);
				if (_flags & XI::Module::Function::FunctionInstance) {
					if (parent == Class::Type) {
						int del = _path.FindFirst(L':');
						auto path = _path.Fragment(0, del);
						del = path.FindLast(L'.');
						auto type = path.Fragment(0, del);
						auto type_ref = dest.classes[type];
						if (!type_ref) throw InvalidStateException();
						type_ref->methods.Append(_name, func);
					} else if (parent == Class::Property) {

						// TODO: IMPLEMENT

					} else throw InvalidArgumentException();
				} else dest.functions.Append(_path, func);
			}
			virtual string ToString(void) const override { return L"function overload " + _path; }
			virtual XMethodOverload * SetInstance(LObject * instance) override { return new MethodOverload(this, instance); }
			virtual VirtualFunctionDesc & GetVFDesc(void) override { return _virt; }
			virtual FunctionImplementationDesc & GetImplementationDesc(void) override { return _impl; }
			virtual FunctionInlineDesc & GetInlineDesc(void) override { return _inl; }
			virtual bool NeedsInstance(void) override { return (_flags & XI::Module::Function::FunctionInstance) != 0; }
			virtual uint & GetFlags(void) override { return _flags; }
			virtual XClass * GetInstanceType(void) override { return _instance_type; }
			virtual LContext & GetContext(void) override { return _ctx; }
			virtual string GetCanonicalType(void) override { return _cn; }
		};
		class Method : public XMethod
		{
			SafePointer<XFunction> _parent;
			SafePointer<LObject> _instance;
		public:
			Method(XFunction * parent, LObject * instance) { _parent.SetRetain(parent); _instance.SetRetain(instance); }
			virtual ~Method(void) override {}
			virtual Class GetClass(void) override { return Class::Method; }
			virtual string GetName(void) override { return _parent->GetName(); }
			virtual string GetFullName(void) override { return _parent->GetFullName(); }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetType(void) override { return _parent->GetType(); }
			virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
			virtual LObject * Invoke(int argc, LObject ** argv) override
			{
				SafePointer<XMethodOverload> over = GetOverloadV(argc, argv);
				return over->Invoke(argc, argv);
			}
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { return _parent->Evaluate(func, error_ctx); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
			virtual string ToString(void) const override { return _parent->ToString() + L" with instance " + _instance->ToString(); }
			virtual XMethodOverload * GetOverloadT(const string & ocn) override
			{
				SafePointer<XFunctionOverload> over = _parent->GetOverloadT(ocn, true);
				return over->SetInstance(_instance);
			}
			virtual XMethodOverload * GetOverloadT(int argc, const string * argv) override
			{
				SafePointer<XFunctionOverload> over = _parent->GetOverloadT(argc, argv, true);
				return over->SetInstance(_instance);
			}
			virtual XMethodOverload * GetOverloadV(int argc, LObject ** argv) override
			{
				SafePointer<XFunctionOverload> over = _parent->GetOverloadV(argc, argv, true);
				return over->SetInstance(_instance);
			}
			virtual XClass * GetInstanceType(void) override { return _parent->GetInstanceType(); }
			virtual LContext & GetContext(void) override { return _parent->GetContext(); }
		};
		class Function : public XFunction
		{
			LContext & _ctx;
			Volumes::ObjectDictionary<string, XFunctionOverload> _overloads;
			SafePointer<XFunctionOverload> _singular;
			SafePointer<XClass> _instance_type;
			string _name, _path;
		public:
			Function(LContext & ctx, const string & name, const string & path, XClass * instance_type) : _ctx(ctx), _name(name), _path(path) { _instance_type.SetRetain(instance_type); }
			virtual ~Function(void) override {}
			virtual Class GetClass(void) override { return Class::Function; }
			virtual string GetName(void) override { return _name; }
			virtual string GetFullName(void) override { return _path; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetType(void) override { if (_singular) return _singular->GetType(); else throw ObjectHasNoTypeException(this); }
			virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
			virtual LObject * Invoke(int argc, LObject ** argv) override
			{
				SafePointer<XFunctionOverload> over = GetOverloadV(argc, argv);
				return over->Invoke(argc, argv);
			}
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { if (_singular) return _singular->Evaluate(func, error_ctx); else throw ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override { for (auto & o : _overloads) o.value->EncodeSymbols(dest, parent); }
			virtual string ToString(void) const override { return L"function " + _path; }
			virtual XFunctionOverload * GetOverloadT(const string & ocn, bool allow_instance = false) override
			{
				auto result = _overloads[ocn];
				if (!result || (result->NeedsInstance() && !allow_instance)) throw ObjectHasNoSuchMemberException(this, _name + L":" + ocn);
				return result;
			}
			virtual XFunctionOverload * GetOverloadT(int argc, const string * argv, bool allow_instance = false) override
			{
				for (auto & o : _overloads) {
					if (o.value->NeedsInstance() && !allow_instance) continue;
					SafePointer< Array<XI::Module::TypeReference> > sgn = XI::Module::TypeReference(o.key).GetFunctionSignature();
					if (sgn->Length() != argc + 1) continue;
					bool valid = true;
					for (int i = 0; i < argc; i++) if (sgn->ElementAt(i + 1).QueryCanonicalName() != argv[i]) { valid = false; break; }
					if (valid) return o.value;
				}
				Array<string> list(1);
				list.Append(argv, argc);
				throw ObjectHasNoSuchMemberException(this, _name + L":" + XI::Module::TypeReference::MakeFunction(L"", &list));
			}
			virtual XFunctionOverload * GetOverloadV(int argc, LObject ** argv, bool allow_instance = false) override
			{
				if (_singular) { _singular->Retain(); return _singular; } else {
					string best_compliance_key;
					int best_compliance_level = CastPriorityNoCast;
					for (auto & o : _overloads) {
						if (o.value->NeedsInstance() && !allow_instance) continue;
						SafePointer< Array<XI::Module::TypeReference> > sgn = XI::Module::TypeReference(o.key).GetFunctionSignature();
						if (sgn->Length() != argc + 1) continue;
						int compliance_level = CastPriorityIdentity;
						for (int i = 0; i < argc; i++) {
							SafePointer<LObject> arg_type = argv[i]->GetType();
							if (arg_type->GetClass() != Class::Type) throw InvalidStateException();
							auto type_from = static_cast<XType *>(arg_type.Inner());
							if (argv[i]->GetClass() == Class::NullLiteral) type_from = 0;
							SafePointer<XType> type_to = CreateType(sgn->ElementAt(i + 1).QueryCanonicalName(), _ctx);
							int type_compliance_level = CheckCastPossibility(type_to, type_from, CastPriorityConverter);
							if (type_compliance_level < compliance_level) compliance_level = type_compliance_level;
						}
						if (compliance_level > best_compliance_level) {
							best_compliance_level = compliance_level;
							best_compliance_key = o.key;
						}
					}
					if (best_compliance_level >= CastPriorityConverter) {
						auto overload = _overloads[best_compliance_key];
						overload->Retain();
						return overload;
					}
					throw ObjectHasNoSuchOverloadException(this, argc, argv);
				}
			}
			virtual XFunctionOverload * AddOverload(XType * retval, int argc, XType ** argv, uint flags, bool local) override
			{
				if (!retval || retval->GetClass() != Class::Type) throw InvalidArgumentException();
				if (argc && !argv) throw InvalidArgumentException();
				string fcn;
				{
					Array<string> acn(argc);
					for (int i = 0; i < argc; i++) {
						if (!argv[i] || argv[i]->GetClass() != Class::Type) throw InvalidArgumentException();
						acn << static_cast<XType *>(argv[i])->GetCanonicalType();
					}
					string rvcn = static_cast<XType *>(retval)->GetCanonicalType();
					fcn = XI::Module::TypeReference::MakeFunction(rvcn, &acn);
				}
				string local_name = _name + L":" + fcn;
				string local_path = _path + L":" + fcn;
				SafePointer<XFunctionOverload> overload;
				if (_instance_type && (flags & FunctionMethod)) {
					if ((flags & FunctionInitializer) || (flags & FunctionFinalizer) || (flags & FunctionMain)) throw InvalidArgumentException();
					overload = new FunctionOverload(_ctx, local_name, local_path, fcn, local, _instance_type);
					overload->GetFlags() |= XI::Module::Function::FunctionInstance;
					if (flags & FunctionThrows) overload->GetFlags() |= XI::Module::Function::FunctionThrows;
					if (flags & FunctionThisCall) overload->GetFlags() |= XI::Module::Function::FunctionThisCall;
					if (flags & FunctionVirtual) {
						if (flags & FunctionPureCall) overload->GetImplementationDesc()._pure = true;

						// TODO: HANDLE VIRTUAL API, SET VIRTUAL FUNCTION PROPS
						// FunctionVirtual = 0x01,

					}
					auto & func = overload->GetImplementationDesc()._xa;
					func.retval = static_cast<XType *>(retval)->GetArgumentSpecification();
					func.inputs << XA::TH::MakeSpec(flags & FunctionThisCall ? XA::ArgumentSemantics::This : XA::ArgumentSemantics::Unclassified, 0, 1);
					for (int i = 0; i < argc; i++) func.inputs << static_cast<XType *>(argv[i])->GetArgumentSpecification();
					if (flags & FunctionThrows) func.inputs << XA::TH::MakeSpec(XA::ArgumentSemantics::ErrorData, 0, 1);
					if (_name == NameConstructorZero || _name == NameDestructor) {
						if (func.inputs.Length() > 1) throw InvalidArgumentException();
						if (func.retval.semantics != XA::ArgumentSemantics::Unclassified || func.retval.size.num_bytes || func.retval.size.num_words) {
							throw InvalidArgumentException();
						}
					}
					if (_name == NameConverter) {
						if (func.inputs.Length() > 1) throw InvalidArgumentException();
					}
					if (_name == NameConstructorMove) {
						if (func.inputs.Length() != 2) throw InvalidArgumentException();
						if (func.retval.semantics != XA::ArgumentSemantics::Unclassified || func.retval.size.num_bytes || func.retval.size.num_words) {
							throw InvalidArgumentException();
						}
						if (func.inputs[1].semantics != XA::ArgumentSemantics::Unclassified || func.inputs[1].size.num_bytes || func.inputs[1].size.num_words != 1) {
							throw InvalidArgumentException();
						}
					}
					if (_name == NameConstructor) {
						if (func.retval.semantics != XA::ArgumentSemantics::Unclassified || func.retval.size.num_bytes || func.retval.size.num_words) {
							throw InvalidArgumentException();
						}
					}
				} else {
					if ((flags & FunctionVirtual) || (flags & FunctionMethod) || (flags & FunctionThisCall) || (flags & FunctionPureCall)) throw InvalidArgumentException();
					overload = new FunctionOverload(_ctx, local_name, local_path, fcn, local, 0);
					if (flags & FunctionInitializer) overload->GetFlags() |= XI::Module::Function::FunctionInitialize;
					if (flags & FunctionFinalizer) overload->GetFlags() |= XI::Module::Function::FunctionShutdown;
					if (flags & FunctionMain) overload->GetFlags() |= XI::Module::Function::FunctionEntryPoint;
					if (flags & FunctionThrows) overload->GetFlags() |= XI::Module::Function::FunctionThrows;
					auto & func = overload->GetImplementationDesc()._xa;
					func.retval = static_cast<XType *>(retval)->GetArgumentSpecification();
					for (int i = 0; i < argc; i++) func.inputs << static_cast<XType *>(argv[i])->GetArgumentSpecification();
					if (flags & FunctionThrows) func.inputs << XA::TH::MakeSpec(XA::ArgumentSemantics::ErrorData, 0, 1);
					if ((flags & FunctionInitializer) || (flags & FunctionMain)) {
						if (func.inputs.Length() > 1) throw InvalidArgumentException();
						if (func.retval.semantics != XA::ArgumentSemantics::Unclassified || func.retval.size.num_bytes || func.retval.size.num_words) {
							throw InvalidArgumentException();
						}
						if (func.inputs.Length() == 1) {
							if (func.inputs[0].semantics != XA::ArgumentSemantics::ErrorData || func.inputs[0].size.num_bytes != 0 || func.inputs[0].size.num_words != 1) {
								throw InvalidArgumentException();
							}
						}
					}
					if (flags & FunctionFinalizer) {
						if (func.inputs.Length()) throw InvalidArgumentException();
						if (func.retval.semantics != XA::ArgumentSemantics::Unclassified || func.retval.size.num_bytes || func.retval.size.num_words) {
							throw InvalidArgumentException();
						}
					}
				}
				if (_overloads.IsEmpty() && !_singular) _singular = overload;
				else if (_singular) _singular.SetReference(0);
				_overloads.Append(fcn, overload);
				return overload;
			}
			virtual XFunctionOverload * AddOverload(XType * retval, int argc, XType ** argv, uint flags, bool local, Point vfi) override
			{
				auto result = AddOverload(retval, argc, argv, flags, local);
				auto & desc = result->GetVFDesc();
				desc.vft_index = vfi.x;
				desc.vf_index = vfi.y;

				// TODO: UPDATE VF INFO

				return result;
			}
			virtual void ListOverloads(Array<string> & fcn, bool allow_instance) override
			{
				for (auto & o : _overloads) {
					if (o.value->NeedsInstance() && !allow_instance) continue;
					fcn << o.key;
				}
			}
			virtual XClass * GetInstanceType(void) override { return _instance_type; }
			virtual XMethod * SetInstance(LObject * instance) override { return new Method(this, instance); }
			virtual LContext & GetContext(void) override { return _ctx; }
		};

		XFunction * CreateFunction(LContext & ctx, const string & name, const string & path, XClass * instance_type) { return new Function(ctx, name, path, instance_type); }
	}
}