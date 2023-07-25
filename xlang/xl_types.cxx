#include "xl_types.h"

#include "xl_base.h"
#include "xl_func.h"
#include "xl_var.h"
#include "xl_lit.h"

// TODO: REMOVE
#include "../xasm/xa_dasm.h"
// TODO: END REMOVE

namespace Engine
{
	namespace XL
	{
		Class XType::GetClass(void) { return Class::Type; }
		LObject * XType::GetType(void)
		{
			try {
				SafePointer<LObject> void_type = GetContext().QueryObject(NameVoid);
				return GetContext().QueryTypePointer(void_type);
			} catch (...) { throw ObjectHasNoTypeException(this); }
		}
		LObject * XType::Invoke(int argc, LObject ** argv)
		{
			try {
				ObjectArray<XType> input_types(argc);
				for (int i = 0; i < argc; i++) {
					SafePointer<LObject> type = argv[i]->GetType();
					if (type->GetClass() != Class::Type) throw InvalidArgumentException();
					input_types.Append(static_cast<XType *>(type.Inner()));
				}
				if (argc == 1) {
					return PerformTypeCast(this, argv[0], CastPriorityExplicit, true);
				} else if (argc == 0) {
					SafePointer<LObject> ctor = GetConstructorInit();
					return ConstructObject(this, ctor, 0, 0);
				} else {
					SafePointer<LObject> ctor = GetMember(NameConstructor);
					return ConstructObject(this, ctor, argc, argv);
				}
			} catch (...) { throw ObjectHasNoSuchOverloadException(this, argc, argv); }
		}

		enum class PointerConstructorClass { Init, Copy };
		class PointerConstructorInstance : public XMethodOverload
		{
			LContext & _ctx;
			PointerConstructorClass _cls;
			SafePointer<LObject> _instance;
			class _computable : public Object, public IComputableProvider
			{
			public:
				PointerConstructorClass _cls;
				SafePointer<XType> _void;
				SafePointer<LObject> _instance, _source;
			public:
				_computable(void) {}
				virtual ~_computable(void) override {}
				virtual Object * ComputableProviderQueryObject(void) override { return this; }
				virtual XType * ComputableGetType(void) override { _void->Retain(); return _void; }
				virtual XA::ExpressionTree ComputableEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
				{
					if (_cls == PointerConstructorClass::Copy) {
						SafePointer<LObject> type = _instance->GetType();
						SafePointer<LObject> uw;
						if (static_cast<XType *>(type.Inner())->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Reference) {
							uw = UnwarpObject(_source);
						} else uw.SetRetain(_source);
						auto tree = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformBlockTransfer, XA::ReferenceFlagInvoke));
						XA::TH::AddTreeInput(tree, _instance->Evaluate(func, error_ctx), XA::TH::MakeSpec(0, 1));
						XA::TH::AddTreeInput(tree, uw->Evaluate(func, error_ctx), XA::TH::MakeSpec(0, 1));
						XA::TH::AddTreeOutput(tree, XA::TH::MakeSpec(0, 1));
						return tree;
					} else if (_cls == PointerConstructorClass::Init) {
						return XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceNull));
					} else throw InvalidStateException();
				}
			};
		public:
			PointerConstructorInstance(LContext & ctx, PointerConstructorClass cls, LObject * instance) : _ctx(ctx), _cls(cls) { _instance.SetRetain(instance); }
			virtual ~PointerConstructorInstance(void) override {}
			virtual Class GetClass(void) override { return Class::MethodOverload; }
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetType(void) override { throw ObjectHasNoTypeException(this); }
			virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
			virtual LObject * Invoke(int argc, LObject ** argv) override
			{
				if (_cls == PointerConstructorClass::Init && argc) throw ObjectHasNoSuchOverloadException(this, argc, argv);
				if (_cls == PointerConstructorClass::Copy && argc != 1) throw ObjectHasNoSuchOverloadException(this, argc, argv);
				SafePointer<_computable> com = new _computable;
				SafePointer<LObject> type = _instance->GetType();
				com->_cls = _cls;
				com->_void = CreateType(XI::Module::TypeReference::MakeClassReference(NameVoid), _ctx);
				com->_instance = _instance;
				if (argc) com->_source = PerformTypeCast(static_cast<XType *>(type.Inner()), argv[0], CastPriorityConverter);
				return CreateComputable(_ctx, com);
			}
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual string ToString(void) const override { return L"instanced pointer built-in constructor"; }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
			virtual VirtualFunctionDesc & GetVFDesc(void) override { throw InvalidStateException(); }
			virtual XClass * GetInstanceType(void) override { throw InvalidStateException(); }
			virtual LContext & GetContext(void) override { return _ctx; }
			virtual string GetCanonicalType(void) override { throw InvalidStateException(); }
		};
		class PointerConstructor : public XFunctionOverload
		{
			LContext & _ctx;
			PointerConstructorClass _cls;
		public:
			PointerConstructor(LContext & ctx, PointerConstructorClass cls) : _ctx(ctx), _cls(cls) {}
			virtual ~PointerConstructor(void) override {}
			virtual Class GetClass(void) override { return Class::FunctionOverload; }
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetType(void) override { throw ObjectHasNoTypeException(this); }
			virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
			virtual LObject * Invoke(int argc, LObject ** argv) override { throw ObjectHasNoSuchOverloadException(this, argc, argv); }
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual string ToString(void) const override { return L"pointer built-in constructor"; }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
			virtual XMethodOverload * SetInstance(LObject * instance) override { return new PointerConstructorInstance(_ctx, _cls, instance); }
			virtual VirtualFunctionDesc & GetVFDesc(void) override { throw InvalidStateException(); }
			virtual FunctionImplementationDesc & GetImplementationDesc(void) override { throw InvalidStateException(); }
			virtual FunctionInlineDesc & GetInlineDesc(void) override { throw InvalidStateException(); }
			virtual bool NeedsInstance(void) override { return true; }
			virtual uint & GetFlags(void) override { throw InvalidStateException(); }
			virtual XClass * GetInstanceType(void) override { throw InvalidStateException(); }
			virtual LContext & GetContext(void) override { return _ctx; }
			virtual string GetCanonicalType(void) override { throw InvalidStateException(); }
		};
		class ReferenceComputable : public Object, public IComputableProvider
		{
			SafePointer<XType> _class;
			SafePointer<LObject> _instance;
		public:
			ReferenceComputable(XType * type, LObject * instance) { _class.SetRetain(type); _instance.SetRetain(instance); }
			virtual ~ReferenceComputable(void) override {}
			virtual Object * ComputableProviderQueryObject(void) override { return this; }
			virtual XType * ComputableGetType(void) override { return static_cast<XType *>(_class->GetContext().QueryTypeReference(_class)); }
			virtual XA::ExpressionTree ComputableEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
			{
				return MakeAddressOf(_instance->Evaluate(func, error_ctx), _class->GetArgumentSpecification().size);
			}
		};
		class ReinterpretComputable : public Object, public IComputableProvider
		{
			SafePointer<XType> _type;
			SafePointer<LObject> _instance;
		public:
			ReinterpretComputable(XType * type, LObject * instance) { _type.SetRetain(type); _instance.SetRetain(instance); }
			virtual ~ReinterpretComputable(void) override {}
			virtual Object * ComputableProviderQueryObject(void) override { return this; }
			virtual XType * ComputableGetType(void) override { _type->Retain(); return _type; }
			virtual XA::ExpressionTree ComputableEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { return _instance->Evaluate(func, error_ctx); }
		};

		class TypeClass : public XClass
		{
			struct _interface_info
			{
				SafePointer<TypeClass> _class;
				int _vft_index;
				XA::ObjectSize _vft_offset, _base_offset;
			};

			LContext & _ctx;
			string _name, _path, _cn;
			bool _local;
			XI::Module::TypeReference _ref;
			XA::ArgumentSpecification _type_spec;
			XI::Module::Class::Nature _nature;
			Volumes::ObjectDictionary<string, LObject> _members;
			Volumes::Dictionary<string, string> _attributes;
			int _vft_index, _last_vft_index;
			XA::ObjectSize _vft_offset;
			_interface_info _parent;
			Array<_interface_info> _interfaces;
		public:
			TypeClass(const string & name, const string & path, bool local, LContext & ctx) :
				_name(name), _path(path), _cn(XI::Module::TypeReference::MakeClassReference(path)), _local(local), _ref(_cn), _ctx(ctx), _interfaces(0x10)
			{
				_type_spec = XA::TH::MakeSpec(XA::ArgumentSemantics::Object, 0, 0);
				_nature = XI::Module::Class::Nature::Standard;
				_vft_index = _last_vft_index = -1;
				_vft_offset = XA::TH::MakeSize(-1, -1);
				_parent._vft_index = 0;
				_parent._vft_offset = XA::TH::MakeSize(-1, -1);
				_parent._base_offset = XA::TH::MakeSize(-1, -1);
			}
			virtual ~TypeClass(void) override {}
			virtual string GetName(void) override { return _name; }
			virtual string GetFullName(void) override { return _path; }
			virtual bool IsDefinedLocally(void) override { return _local; }
			virtual LObject * GetMember(const string & name) override
			{
				auto member = _members[name];
				if (member && member->GetClass() == Class::Alias) {
					auto alias = static_cast<XAlias *>(member);
					if (alias->IsTypeAlias()) return CreateType(alias->GetDestination(), _ctx);
					else return _ctx.QueryObject(alias->GetDestination());
				}
				if (member) { member->Retain(); return member; } else throw ObjectHasNoSuchMemberException(this, name);
			}
			virtual void AddMember(const string & name, LObject * child) override { if (!_members.Append(name, child)) throw ObjectMemberRedefinitionException(this, name); }
			virtual void AddAttribute(const string & key, const string & value) override { if (!_attributes.Append(key, value)) throw ObjectMemberRedefinitionException(this, key); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { return MakeAddressOf(MakeSymbolReference(func, _path), XA::TH::MakeSize(0, 1)); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override
			{
				if (!_local) return;
				XI::Module::Class self;
				self.class_nature = _nature;
				self.instance_spec = _type_spec;
				self.parent_class.interface_name = _parent._class ? _parent._class->GetFullName() : L"";
				self.parent_class.vft_pointer_offset = _parent._class && _vft_index < 0 ? _parent._vft_offset : _vft_offset;
				for (auto & i : _interfaces) {
					XI::Module::Interface ie;
					ie.interface_name = i._class->GetFullName();
					ie.vft_pointer_offset = i._vft_offset;
					self.interfaces_implements << ie;
				}

				// TODO: IMPLEMENT
				// Volumes::Dictionary<string, Variable> fields;		// NAME
				// Volumes::Dictionary<string, Property> properties;	// NAME
				// Volumes::Dictionary<string, Function> methods;		// NAME:SCN

				self.attributes = _attributes;
				dest.classes.Append(_path, self);
				for (auto & m : _members) static_cast<XObject *>(m.value.Inner())->EncodeSymbols(dest, Class::Type);
			}
			virtual LContext & GetContext(void) override { return _ctx; }
			virtual string GetCanonicalType(void) override { return _cn; }
			virtual XI::Module::TypeReference::Class GetCanonicalTypeClass(void) override { return _ref.GetReferenceClass(); }
			virtual const XI::Module::TypeReference & GetTypeReference(void) override { return _ref; }
			virtual XA::ArgumentSpecification GetArgumentSpecification(void) override { return _type_spec; }
			virtual LObject * GetConstructorInit(void) override
			{
				auto ctor = _members[NameConstructor];
				if (ctor) {
					if (ctor->GetClass() != Class::Function) throw InvalidStateException();
					auto ctor_fd = static_cast<XFunction *>(ctor);
					auto ctor_over = ctor_fd->GetOverloadT(0, 0, true);
					ctor_over->Retain();
					return ctor_over;
				} else throw ObjectHasNoSuchMemberException(this, NameConstructor);
			}
			virtual LObject * GetConstructorCopy(void) override
			{
				auto ctor = _members[NameConstructor];
				if (ctor) {
					if (ctor->GetClass() != Class::Function) throw InvalidStateException();
					auto ctor_fd = static_cast<XFunction *>(ctor);
					try {
						auto self_ref = XI::Module::TypeReference::MakeReference(GetCanonicalType());
						auto ctor_over = ctor_fd->GetOverloadT(1, &self_ref, true);
						ctor_over->Retain();
						return ctor_over;
					} catch (...) {}
					auto self_cn = GetCanonicalType();
					auto ctor_over = ctor_fd->GetOverloadT(1, &self_cn, true);
					ctor_over->Retain();
					return ctor_over;
				} else throw ObjectHasNoSuchMemberException(this, NameConstructor);
			}
			virtual LObject * GetConstructorZero(void) override
			{
				auto ctor = _members[NameConstructorZero];
				if (ctor) {
					if (ctor->GetClass() != Class::Function) throw InvalidStateException();
					auto ctor_fd = static_cast<XFunction *>(ctor);
					auto ctor_over = ctor_fd->GetOverloadT(0, 0, true);
					ctor_over->Retain();
					return ctor_over;
				} else {
					SafePointer<LObject> result = GetConstructorInit();
					if (static_cast<XFunctionOverload *>(result.Inner())->GetFlags() & XI::Module::Function::FunctionThrows) {
						throw ObjectHasNoSuchMemberException(this, NameConstructorZero);
					}
					result->Retain();
					return result;
				}
			}
			virtual LObject * GetConstructorMove(void) override
			{
				auto ctor = _members[NameConstructorMove];
				if (ctor) {
					if (ctor->GetClass() != Class::Function) throw InvalidStateException();
					auto ctor_fd = static_cast<XFunction *>(ctor);
					auto self_ref = XI::Module::TypeReference::MakeReference(GetCanonicalType());
					auto ctor_over = ctor_fd->GetOverloadT(1, &self_ref, true);
					ctor_over->Retain();
					return ctor_over;
				} else return GetConstructorCopy();
			}
			virtual LObject * GetConstructorCast(XType * from_type) override
			{
				auto ctor = _members[NameConstructor];
				if (ctor) {
					if (ctor->GetClass() != Class::Function) throw InvalidStateException();
					auto ctor_fd = static_cast<XFunction *>(ctor);
					try {
						auto type_ref = XI::Module::TypeReference::MakeReference(from_type->GetCanonicalType());
						auto ctor_over = ctor_fd->GetOverloadT(1, &type_ref, true);
						ctor_over->Retain();
						return ctor_over;
					} catch (...) {}
					auto type_cn = from_type->GetCanonicalType();
					auto ctor_over = ctor_fd->GetOverloadT(1, &type_cn, true);
					ctor_over->Retain();
					return ctor_over;
				} else throw ObjectHasNoSuchMemberException(this, NameConstructor);
			}
			virtual LObject * GetCastMethod(XType * to_type) override
			{
				auto conv = _members[NameConverter];
				if (conv) {
					if (conv->GetClass() != Class::Function) throw InvalidStateException();
					auto conv_fd = static_cast<XFunction *>(conv);
					auto sign = XI::Module::TypeReference::MakeFunction(to_type->GetCanonicalType(), 0);
					auto conv_over = conv_fd->GetOverloadT(sign, true);
					conv_over->Retain();
					return conv_over;
				} else throw ObjectHasNoSuchMemberException(this, NameConverter);
			}
			virtual LObject * GetDestructor(void) override
			{
				auto dtor = _members[NameDestructor];
				if (dtor) {
					if (dtor->GetClass() != Class::Function) throw InvalidStateException();
					auto dtor_overload = static_cast<XFunction *>(dtor)->GetOverloadT(0, 0, true);
					dtor_overload->Retain();
					return dtor_overload;
				} else return CreateNull();
			}
			virtual void GetTypesConformsTo(ObjectArray<XType> & types) override
			{
				SafePointer<XType> ref = CreateType(XI::Module::TypeReference::MakeReference(GetCanonicalType()), _ctx);
				types.Append(this);
				types.Append(ref);

				// TODO: ADD PARENT TYPES AND THEIR REFS
				//   TYPE	-> PARENT
				//   TYPE	-> PARENT&
			}
			virtual LObject * TransformTo(LObject * subject, XType * type, bool cast_explicit) override
			{
				if (cast_explicit) {
					if (type->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Pointer &&
						_type_spec.size.num_bytes == 0 && _type_spec.size.num_words == 1) {
						SafePointer<ReinterpretComputable> com = new ReinterpretComputable(type, subject);
						return CreateComputable(_ctx, com);
					}
				} else {
					if (type->GetCanonicalType() == GetCanonicalType()) { subject->Retain(); return subject; }
					else if (type->GetCanonicalType() == XI::Module::TypeReference::MakeReference(GetCanonicalType())) {
						SafePointer<ReferenceComputable> com = new ReferenceComputable(this, subject);
						return CreateComputable(_ctx, com);
					}
				}
				throw LException(this);

				// TODO: IMPLEMENT
				// TODO: SIMILARITY CASTS (2)
				//   TYPE	-> PARENT
				//   TYPE	-> PARENT&
				// TODO: REVERSE SIMILARITY CASTS (0)
				//   PARENT  -> TYPE&
				//   INTPTR  -> TYPE*
			}
			virtual XI::Module::Class::Nature GetLanguageSemantics(void) override { return _nature; }
			virtual void OverrideArgumentSpecification(XA::ArgumentSpecification spec) override { _type_spec = spec; }
			virtual void OverrideLanguageSemantics(XI::Module::Class::Nature spec) override { _nature = spec; }
			virtual void AdoptParentClass(XClass * parent) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual void AdoptInterface(XClass * parent) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual XClass * GetParentClass(void) override { if (_parent._class) _parent._class->Retain(); return _parent._class; }
			virtual int GetInterfaceCount(void) override { return _interfaces.Length(); }
			virtual XClass * GetInterface(int index) override { _interfaces[index]._class->Retain(); return _interfaces[index]._class; }
			virtual string ToString(void) const override { return L"class " + _path; }
		};
		class TypeArray : public XArray
		{
			LContext & _ctx;
			string _cn;
			XI::Module::TypeReference _ref;
		public:
			TypeArray(const string & cn, LContext & ctx) : _cn(cn), _ctx(ctx), _ref(_cn) {}
			virtual ~TypeArray(void) override {}
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetMember(const string & name) override
			{
				// TODO: IMPLEMENT
				// [] always
				throw ObjectHasNoSuchMemberException(this, name);
			}
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
			virtual LContext & GetContext(void) override { return _ctx; }
			virtual string GetCanonicalType(void) override { return _cn; }
			virtual XI::Module::TypeReference::Class GetCanonicalTypeClass(void) override { return _ref.GetReferenceClass(); }
			virtual const XI::Module::TypeReference & GetTypeReference(void) override { return _ref; }
			virtual XA::ArgumentSpecification GetArgumentSpecification(void) override
			{
				SafePointer<XType> element = CreateType(_ref.GetArrayElement().QueryCanonicalName(), _ctx);
				auto volume = _ref.GetArrayVolume();
				auto element_spec = element->GetArgumentSpecification();
				return XA::TH::MakeSpec(XA::ArgumentSemantics::Object, element_spec.size.num_bytes * volume, element_spec.size.num_words * volume);
			}
			virtual LObject * GetConstructorInit(void) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetConstructorCopy(void) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetConstructorZero(void) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetConstructorMove(void) override
			{
				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual LObject * GetConstructorCast(XType * from_type) override { throw ObjectHasNoSuchMemberException(this, NameConstructor); }
			virtual LObject * GetCastMethod(XType * to_type) override { throw ObjectHasNoSuchMemberException(this, NameConverter); }
			virtual LObject * GetDestructor(void) override
			{
				// TODO: REWORK
				// SafePointer<XType> element = GetElementType();
				// SafePointer<XType> type_void = CreateType(XI::Module::TypeReference::MakeClassReference(NameVoid), _ctx);
				// SafePointer<LObject> dtor = element->GetDestructor();
				// if (dtor->GetClass() == Class::Null) { dtor->Retain(); return dtor; }
				// LObject * self = this;
				// auto array_dtor = _ctx.CreatePrivateFunction(L"perde_ordinem", type_void, 1, &self, 0);
				// auto & impl = static_cast<XFunctionOverload *>(array_dtor)->GetImplementationDesc();
				// if (!impl._xa.instset.Length()) {
				// 	SafePointer<LObject> self_ptr = _ctx.QueryTypePointer(this);
				// 	SafePointer<LObject> subs = GetMember(OperatorSubscript);
				// 	impl._xa.inputs[0].semantics = XA::ArgumentSemantics::This;
				// 	impl._xa.inputs[0].size = XA::TH::MakeSize(0, 1);
				// 	auto vol = GetVolume();
				// 	auto xdtor = static_cast<XFunctionOverload *>(dtor.Inner());
				// 	auto xsubs = static_cast<XFunctionOverload *>(subs.Inner());
				// 	SafePointer<LObject> self = CreateComputable(_ctx, static_cast<XType *>(self_ptr.Inner()),
				// 		MakeAddressFollow(XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceArgument, 0)), GetArgumentSpecification().size));
				// 	SafePointer<LObject> xsubs_inst = xsubs->SetInstance(self);
				// 	for (int i = 0; i < vol; i++) {
				// 		SafePointer<LObject> index = CreateLiteral(_ctx, uint64(0));
				// 		SafePointer<LObject> element_object = xsubs_inst->Invoke(1, index.InnerRef());
				// 		SafePointer<LObject> dtor_inst = xdtor->SetInstance(element_object);
				// 		SafePointer<LObject> dtor_inv = dtor_inst->Invoke(0, 0);
				// 		impl._xa.instset << XA::TH::MakeStatementExpression(dtor_inv->Evaluate(impl._xa, 0));
				// 	}
				// 	impl._xa.instset << XA::TH::MakeStatementReturn();

				// 	// TODO: REMOVE
				// 	IO::Console console;
				// 	XA::DisassemblyFunction(impl._xa, &console);
				// 	// TODO: END REMOVE
				// }
				// array_dtor->Retain();
				// return array_dtor;

				// TODO: IMPLEMENT
				throw LException(this);
			}
			virtual void GetTypesConformsTo(ObjectArray<XType> & types) override
			{
				SafePointer<XType> self_ref = CreateType(XI::Module::TypeReference::MakeReference(GetCanonicalType()), _ctx);
				SafePointer<XType> element = GetElementType();
				SafePointer<XType> element_ptr = CreateType(XI::Module::TypeReference::MakePointer(element->GetCanonicalType()), _ctx);
				SafePointer<XType> element_ptr_ref = CreateType(XI::Module::TypeReference::MakeReference(element_ptr->GetCanonicalType()), _ctx);
				types.Append(this);
				types.Append(self_ref);
				types.Append(element_ptr);
				types.Append(element_ptr_ref);
			}
			virtual LObject * TransformTo(LObject * subject, XType * type, bool cast_explicit) override
			{
				if (!cast_explicit) {
					if (GetCanonicalType() == type->GetCanonicalType()) { subject->Retain(); return subject; }
					auto & element = _ref.GetArrayElement();
					auto & ref = type->GetTypeReference();
					if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Reference &&
						ref.GetReferenceDestination().QueryCanonicalName() == GetCanonicalType()) {
						SafePointer<ReferenceComputable> com = new ReferenceComputable(this, subject);
						return CreateComputable(_ctx, com);
					}
					auto element_ptr = XI::Module::TypeReference::MakePointer(element.QueryCanonicalName());
					SafePointer<XType> element_ptr_type = CreateType(element_ptr, _ctx);
					if (ref.QueryCanonicalName() == element_ptr) {
						SafePointer<LObject> take_addr = _ctx.QueryAddressOfOperator();
						SafePointer<LObject> address = take_addr->Invoke(1, &subject);
						SafePointer<ReinterpretComputable> com = new ReinterpretComputable(element_ptr_type, subject);
						return CreateComputable(_ctx, com);
					}
					if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Reference &&
						ref.GetReferenceDestination().QueryCanonicalName() == element_ptr) {
						SafePointer<LObject> take_addr = _ctx.QueryAddressOfOperator();
						SafePointer<LObject> address = take_addr->Invoke(1, &subject);
						SafePointer<ReinterpretComputable> com = new ReinterpretComputable(element_ptr_type, subject);
						SafePointer<LObject> reint = CreateComputable(_ctx, com);
						SafePointer<ReferenceComputable> com2 = new ReferenceComputable(element_ptr_type, reint);
						return CreateComputable(_ctx, com2);
					}
				}
				throw LException(this);
			}
			virtual XType * GetElementType(void) override { return CreateType(_ref.GetArrayElement().QueryCanonicalName(), _ctx); }
			virtual int GetVolume(void) override { return _ref.GetArrayVolume(); }
			virtual string ToString(void) const override { return L"type " + _ref.ToString(); }
		};
		class TypePointer : public XPointer
		{
			LContext & _ctx;
			string _cn;
			XI::Module::TypeReference _ref;
			class _pointer_operator_computable : public Object, public IComputableProvider
			{
			public:
				string _op;
				SafePointer<XPointer> _ptr_type;
				SafePointer<XType> _retval;
				SafePointer<LObject> _instance;
				ObjectArray<LObject> _argv;
				ObjectArray<XType> _argt;
			public:
				_pointer_operator_computable(void) : _argv(0x20), _argt(0x20) {}
				virtual ~_pointer_operator_computable(void) override {}
				virtual Object * ComputableProviderQueryObject(void) override { return this; }
				virtual XType * ComputableGetType(void) override { _retval->Retain(); return _retval; }
				virtual XA::ExpressionTree ComputableEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
				{
					if (_op == OperatorInvoke) {
						auto node = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformInvoke, XA::ReferenceFlagInvoke));
						XA::TH::AddTreeInput(node, _instance->Evaluate(func, error_ctx), _ptr_type->GetArgumentSpecification());
						for (int i = 0; i < _argv.Length(); i++) XA::TH::AddTreeInput(node, _argv[i].Evaluate(func, error_ctx), _argt[i].GetArgumentSpecification());
						SafePointer<LObject> dtor = _retval->GetDestructor();
						if (dtor->GetClass() == Class::Null) XA::TH::AddTreeOutput(node, _retval->GetArgumentSpecification());
						else XA::TH::AddTreeOutput(node, _retval->GetArgumentSpecification(), XA::TH::MakeFinal(MakeSymbolReferenceL(func, dtor->GetFullName())));
						return node;
					} else if (_op == OperatorSubscript) {
						SafePointer<XType> pdest = _ptr_type->GetElementType();
						auto node = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformAddressOffset, XA::ReferenceFlagInvoke));
						XA::TH::AddTreeInput(node, MakeAddressFollow(_instance->Evaluate(func, error_ctx), pdest->GetArgumentSpecification().size), pdest->GetArgumentSpecification());
						XA::TH::AddTreeInput(node, _argv[0].Evaluate(func, error_ctx), _argt[0].GetArgumentSpecification());
						XA::TH::AddTreeInput(node, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLiteral)), pdest->GetArgumentSpecification());
						XA::TH::AddTreeOutput(node, pdest->GetArgumentSpecification());
						return MakeAddressOf(node, pdest->GetArgumentSpecification().size);
					} else if (_op == OperatorFollow) {
						return _instance->Evaluate(func, error_ctx);
					} else if (_op == OperatorAssign) {
						auto node = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformBlockTransfer, XA::ReferenceFlagInvoke));
						XA::TH::AddTreeInput(node, _instance->Evaluate(func, error_ctx), _ptr_type->GetArgumentSpecification());
						XA::TH::AddTreeInput(node, _argv[0].Evaluate(func, error_ctx), _ptr_type->GetArgumentSpecification());
						XA::TH::AddTreeOutput(node, _ptr_type->GetArgumentSpecification());
						return node;
					} else throw InvalidStateException();
				}
			};
			class _pointer_operator_instance : public XMethodOverload
			{
				string _op;
				SafePointer<XPointer> _ptr_type;
				SafePointer<LObject> _instance;
			public:
				_pointer_operator_instance(const string & op, XPointer * ptr, LObject * inst) : _op(op) { _ptr_type.SetRetain(ptr); _instance.SetRetain(inst); }
				virtual ~_pointer_operator_instance(void) override {}
				virtual Class GetClass(void) override { return Class::MethodOverload; }
				virtual string GetName(void) override { return L""; }
				virtual string GetFullName(void) override { return L""; }
				virtual bool IsDefinedLocally(void) override { return false; }
				virtual LObject * GetType(void) override { throw ObjectHasNoTypeException(this); }
				virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
				virtual LObject * Invoke(int argc, LObject ** argv) override
				{
					SafePointer<_pointer_operator_computable> com = new _pointer_operator_computable;
					com->_op = _op;
					com->_ptr_type = _ptr_type;
					com->_instance = _instance;
					auto & ref = _ptr_type->GetTypeReference();
					if (_op == OperatorInvoke && ref.GetPointerDestination().GetReferenceClass() == XI::Module::TypeReference::Class::Function) {
						SafePointer< Array<XI::Module::TypeReference> > sgn = ref.GetPointerDestination().GetFunctionSignature();
						if (sgn->Length() != argc + 1) throw ObjectHasNoSuchOverloadException(this, argc, argv);
						com->_retval = CreateType(sgn->ElementAt(0).QueryCanonicalName(), GetContext());
						for (int i = 0; i < argc; i++) {
							SafePointer<XType> type_need = CreateType(sgn->ElementAt(i + 1).QueryCanonicalName(), GetContext());
							SafePointer<LObject> casted = PerformTypeCast(type_need, argv[i], CastPriorityConverter);
							if (type_need->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Reference) casted = UnwarpObject(casted);
							com->_argv.Append(casted);
							com->_argt.Append(type_need);
						}
					} else if (_op == OperatorSubscript && ref.GetPointerDestination().GetReferenceClass() != XI::Module::TypeReference::Class::Function) {
						if (argc != 1) throw ObjectHasNoSuchOverloadException(this, argc, argv);
						SafePointer<XType> argt = CreateType(XI::Module::TypeReference::MakeClassReference(NameUIntPtr), _ptr_type->GetContext());
						SafePointer<LObject> arg = PerformTypeCast(argt, argv[0], CastPriorityConverter);
						com->_retval = CreateType(XI::Module::TypeReference::MakeReference(ref.GetPointerDestination().QueryCanonicalName()), _ptr_type->GetContext());
						com->_argv.Append(arg);
						com->_argt.Append(argt);
					} else if (_op == OperatorFollow && ref.GetPointerDestination().GetReferenceClass() != XI::Module::TypeReference::Class::Function) {
						if (argc != 0) throw ObjectHasNoSuchOverloadException(this, argc, argv);
						com->_retval = CreateType(XI::Module::TypeReference::MakeReference(ref.GetPointerDestination().QueryCanonicalName()), _ptr_type->GetContext());
					} else if (_op == OperatorAssign) {
						if (argc != 1) throw ObjectHasNoSuchOverloadException(this, argc, argv);
						SafePointer<LObject> arg = PerformTypeCast(_ptr_type, argv[0], CastPriorityConverter);
						com->_retval.SetRetain(_ptr_type);
						com->_argv.Append(arg);
						com->_argt.Append(_ptr_type);
					} else throw ObjectHasNoSuchOverloadException(this, argc, argv);
					return CreateComputable(_ptr_type->GetContext(), com);
				}
				virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
				virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
				virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
				virtual string ToString(void) const override { return L"instanced pointer built-in operator"; }
				virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
				virtual VirtualFunctionDesc & GetVFDesc(void) override { throw InvalidStateException(); }
				virtual XClass * GetInstanceType(void) override { throw InvalidStateException(); }
				virtual LContext & GetContext(void) override { return _ptr_type->GetContext(); }
				virtual string GetCanonicalType(void) override { throw InvalidStateException(); }
			};
			class _pointer_operator : public XFunctionOverload
			{
				string _op;
				SafePointer<XPointer> _ptr_type;
			public:
				_pointer_operator(XPointer * ptr, const string & op) : _op(op) { _ptr_type.SetRetain(ptr); }
				virtual ~_pointer_operator(void) override {}
				virtual Class GetClass(void) override { return Class::FunctionOverload; }
				virtual string GetName(void) override { return L""; }
				virtual string GetFullName(void) override { return L""; }
				virtual bool IsDefinedLocally(void) override { return false; }
				virtual LObject * GetType(void) override { throw ObjectHasNoTypeException(this); }
				virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
				virtual LObject * Invoke(int argc, LObject ** argv) override { throw ObjectHasNoSuchOverloadException(this, argc, argv); }
				virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
				virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
				virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
				virtual string ToString(void) const override { return L"pointer built-in operator"; }
				virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
				virtual XMethodOverload * SetInstance(LObject * instance) override { return new _pointer_operator_instance(_op, _ptr_type, instance); }
				virtual VirtualFunctionDesc & GetVFDesc(void) override { throw InvalidStateException(); }
				virtual FunctionImplementationDesc & GetImplementationDesc(void) override { throw InvalidStateException(); }
				virtual FunctionInlineDesc & GetInlineDesc(void) override { throw InvalidStateException(); }
				virtual bool NeedsInstance(void) override { return true; }
				virtual uint & GetFlags(void) override { throw InvalidStateException(); }
				virtual XClass * GetInstanceType(void) override { throw InvalidStateException(); }
				virtual LContext & GetContext(void) override { return _ptr_type->GetContext(); }
				virtual string GetCanonicalType(void) override { throw InvalidStateException(); }
			};
		public:
			TypePointer(const string & cn, LContext & ctx) : _cn(cn), _ctx(ctx), _ref(_cn) {}
			virtual ~TypePointer(void) override {}
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetMember(const string & name) override
			{
				if (name == OperatorInvoke || name == OperatorSubscript || name == OperatorFollow ||
					name == OperatorAssign) return new _pointer_operator(this, name);
				throw ObjectHasNoSuchMemberException(this, name);
			}
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
			virtual LContext & GetContext(void) override { return _ctx; }
			virtual string GetCanonicalType(void) override { return _cn; }
			virtual XI::Module::TypeReference::Class GetCanonicalTypeClass(void) override { return _ref.GetReferenceClass(); }
			virtual const XI::Module::TypeReference & GetTypeReference(void) override { return _ref; }
			virtual XA::ArgumentSpecification GetArgumentSpecification(void) override { return XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 1); }
			virtual LObject * GetConstructorInit(void) override { return new PointerConstructor(_ctx, PointerConstructorClass::Init); }
			virtual LObject * GetConstructorCopy(void) override { return new PointerConstructor(_ctx, PointerConstructorClass::Copy); }
			virtual LObject * GetConstructorZero(void) override { return new PointerConstructor(_ctx, PointerConstructorClass::Init); }
			virtual LObject * GetConstructorMove(void) override { return new PointerConstructor(_ctx, PointerConstructorClass::Copy); }
			virtual LObject * GetConstructorCast(XType * from_type) override { throw ObjectHasNoSuchMemberException(this, NameConstructor); }
			virtual LObject * GetCastMethod(XType * to_type) override { throw ObjectHasNoSuchMemberException(this, NameConverter); }
			virtual LObject * GetDestructor(void) override { return CreateNull(); }
			virtual void GetTypesConformsTo(ObjectArray<XType> & types) override
			{
				SafePointer<XType> self_ref = CreateType(XI::Module::TypeReference::MakeReference(GetCanonicalType()), _ctx);
				SafePointer<XType> nothing_ptr = CreateType(XI::Module::TypeReference::MakePointer(XI::Module::TypeReference::MakeClassReference(NameVoid)), _ctx);
				SafePointer<XType> nothing_ptr_ref = CreateType(XI::Module::TypeReference::MakeReference(nothing_ptr->GetCanonicalType()), _ctx);
				types.Append(this);
				types.Append(self_ref);

				// TODO: IMPLEMENT
				// TODO: SIMILARITY CASTS (2)
				//   TYPE * -> PARENT *
				//   TYPE * -> PARENT * &
				
				types.Append(nothing_ptr);
				types.Append(nothing_ptr_ref);
			}
			virtual LObject * TransformTo(LObject * subject, XType * type, bool cast_explicit) override
			{
				if (cast_explicit) {

					// TODO: REVERSE SIMILARITY CASTS (0)
					//   PARENT * -> TYPE *
					
					auto dest_size = type->GetArgumentSpecification().size;
					SafePointer<LObject> dtor = type->GetDestructor();
					if (dest_size.num_bytes == 0 && dest_size.num_words == 1 && dtor->GetClass() == Class::Null) {
						SafePointer<ReinterpretComputable> com = new ReinterpretComputable(type, subject);
						return CreateComputable(_ctx, com);
					}
				} else {
					if (type->GetCanonicalType() == GetCanonicalType()) { subject->Retain(); return subject; }
					auto & dest = type->GetTypeReference();
					if (dest.GetReferenceClass() == XI::Module::TypeReference::Class::Reference &&
						dest.GetReferenceDestination().QueryCanonicalName() == GetCanonicalType()) {
						SafePointer<ReferenceComputable> com = new ReferenceComputable(this, subject);
						return CreateComputable(_ctx, com);
					}

					// TODO: SIMILARITY CASTS (2)
					//   TYPE * -> PARENT *
					//   TYPE * -> PARENT * &

					auto void_ptr = XI::Module::TypeReference::MakePointer(XI::Module::TypeReference::MakeClassReference(NameVoid));
					SafePointer<XType> void_ptr_type = CreateType(void_ptr, _ctx);
					if (dest.QueryCanonicalName() == void_ptr) {
						SafePointer<ReinterpretComputable> com = new ReinterpretComputable(void_ptr_type, subject);
						return CreateComputable(_ctx, com);
					}
					if (dest.QueryCanonicalName() == XI::Module::TypeReference::MakeReference(void_ptr)) {
						SafePointer<ReinterpretComputable> com = new ReinterpretComputable(void_ptr_type, subject);
						SafePointer<LObject> void_com = CreateComputable(_ctx, com);
						SafePointer<ReferenceComputable> com2 = new ReferenceComputable(void_ptr_type, void_com);
						return CreateComputable(_ctx, com2);
					}
				}
				throw LException(this);
			}
			virtual XType * GetElementType(void) override { return CreateType(_ref.GetPointerDestination().QueryCanonicalName(), _ctx); }
			virtual string ToString(void) const override { return L"type " + _ref.ToString(); }
		};
		class TypeReference : public XReference
		{
			LContext & _ctx;
			string _cn;
			XI::Module::TypeReference _ref;
		public:
			TypeReference(const string & cn, LContext & ctx) : _cn(cn), _ctx(ctx), _ref(_cn) {}
			virtual ~TypeReference(void) override {}
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
			virtual LContext & GetContext(void) override { return _ctx; }
			virtual string GetCanonicalType(void) override { return _cn; }
			virtual XI::Module::TypeReference::Class GetCanonicalTypeClass(void) override { return _ref.GetReferenceClass(); }
			virtual const XI::Module::TypeReference & GetTypeReference(void) override { return _ref; }
			virtual XA::ArgumentSpecification GetArgumentSpecification(void) override { return XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 1); }
			virtual LObject * GetConstructorInit(void) override { throw ObjectHasNoSuchMemberException(this, NameConstructor); }
			virtual LObject * GetConstructorCopy(void) override { return new PointerConstructor(_ctx, PointerConstructorClass::Copy); }
			virtual LObject * GetConstructorZero(void) override { return new PointerConstructor(_ctx, PointerConstructorClass::Init); }
			virtual LObject * GetConstructorMove(void) override { return GetConstructorCopy(); }
			virtual LObject * GetConstructorCast(XType * from_type) override { throw ObjectHasNoSuchMemberException(this, NameConstructor); }
			virtual LObject * GetCastMethod(XType * to_type) override { throw ObjectHasNoSuchMemberException(this, NameConverter); }
			virtual LObject * GetDestructor(void) override { return CreateNull(); }
			virtual void GetTypesConformsTo(ObjectArray<XType> & types) override { types.Append(this); }
			virtual LObject * TransformTo(LObject * subject, XType * type, bool cast_explicit) override
			{
				if (type->GetCanonicalType() == GetCanonicalType() && !cast_explicit) { subject->Retain(); return subject; }
				throw LException(this);
			}
			virtual XType * GetElementType(void) override { return CreateType(_ref.GetReferenceDestination().QueryCanonicalName(), _ctx); }
			virtual string ToString(void) const override { return L"type " + _ref.ToString(); }
		};
		class TypeFunction : public XFunctionType
		{
			LContext & _ctx;
			string _cn;
			XI::Module::TypeReference _ref;
		public:
			TypeFunction(const string & cn, LContext & ctx) : _cn(cn), _ctx(ctx), _ref(_cn) {}
			virtual ~TypeFunction(void) override {}
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
			virtual LContext & GetContext(void) override { return _ctx; }
			virtual string GetCanonicalType(void) override { return _cn; }
			virtual XI::Module::TypeReference::Class GetCanonicalTypeClass(void) override { return _ref.GetReferenceClass(); }
			virtual const XI::Module::TypeReference & GetTypeReference(void) override { return _ref; }
			virtual XA::ArgumentSpecification GetArgumentSpecification(void) override { return XA::TH::MakeSpec(XA::ArgumentSemantics::Unknown, 0, 0); }
			virtual LObject * GetConstructorInit(void) override { throw ObjectHasNoSuchMemberException(this, NameConstructor); }
			virtual LObject * GetConstructorCopy(void) override { throw ObjectHasNoSuchMemberException(this, NameConstructor); }
			virtual LObject * GetConstructorZero(void) override { throw ObjectHasNoSuchMemberException(this, NameConstructorZero); }
			virtual LObject * GetConstructorMove(void) override { throw ObjectHasNoSuchMemberException(this, NameConstructorMove); }
			virtual LObject * GetConstructorCast(XType * from_type) override { throw ObjectHasNoSuchMemberException(this, NameConstructor); }
			virtual LObject * GetCastMethod(XType * to_type) override { throw ObjectHasNoSuchMemberException(this, NameConverter); }
			virtual LObject * GetDestructor(void) override { throw ObjectHasNoSuchMemberException(this, NameDestructor); }
			virtual void GetTypesConformsTo(ObjectArray<XType> & types) override {}
			virtual LObject * TransformTo(LObject * subject, XType * type, bool cast_explicit) override { throw LException(this); }
			virtual int GetArgumentCount(void) override
			{
				SafePointer< Array<XI::Module::TypeReference> > sgn = _ref.GetFunctionSignature();
				return sgn->Length() - 1;
			}
			virtual XType * GetArgument(int index) override
			{
				SafePointer< Array<XI::Module::TypeReference> > sgn = _ref.GetFunctionSignature();
				return CreateType(sgn->ElementAt(index + 1).QueryCanonicalName(), _ctx);
			}
			virtual XType * GetReturnValue(void) override
			{
				SafePointer< Array<XI::Module::TypeReference> > sgn = _ref.GetFunctionSignature();
				return CreateType(sgn->ElementAt(0).QueryCanonicalName(), _ctx);
			}
			virtual string ToString(void) const override { return L"type " + _ref.ToString(); }
		};

		XType * CreateType(const string & cn, LContext & ctx)
		{
			XI::Module::TypeReference ref(cn);
			if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Class) {
				SafePointer<LObject> object = ctx.QueryObject(ref.GetClassName());
				if (object->GetClass() != Class::Type) throw InvalidStateException();
				auto rv = static_cast<XType *>(object.Inner());
				rv->Retain();
				return rv;
			} else if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Array) {
				return new TypeArray(cn, ctx);
			} else if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Pointer) {
				return new TypePointer(cn, ctx);
			} else if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Reference) {
				return new TypeReference(cn, ctx);
			} else if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Function) {
				return new TypeFunction(cn, ctx);
			} else throw InvalidArgumentException();
		}
		XClass * CreateClass(const string & name, const string & path, bool local, LContext & ctx) { return new TypeClass(name, path, local, ctx); }

		class ConstructObjectProvider : public Object, public IComputableProvider
		{
		public:
			string _dtor_ref;
			SafePointer<XType> _retval;
			SafePointer<LObject> _init_expr;
		public:
			ConstructObjectProvider(void) {}
			virtual ~ConstructObjectProvider(void) override {}
			virtual Object * ComputableProviderQueryObject(void) override { return this; }
			virtual XType * ComputableGetType(void) override { _retval->Retain(); return _retval; }
			virtual XA::ExpressionTree ComputableEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
			{
				auto node = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformTemporary, XA::ReferenceFlagInvoke));
				XA::TH::AddTreeInput(node, _init_expr->Evaluate(func, error_ctx), XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 0));
				XA::TH::AddTreeOutput(node, _retval->GetArgumentSpecification(),
					XA::TH::MakeFinal(_dtor_ref.Length() ? MakeSymbolReferenceL(func, _dtor_ref) : XA::TH::MakeRef()));
				return node;
			}
		};
		int CheckCastPossibility(XType * dest, XType * src, int min_level)
		{
			if (min_level <= CastPriorityIdentity && src) {
				if (dest->GetCanonicalType() == src->GetCanonicalType()) return CastPriorityIdentity;
			}
			if (min_level <= CastPrioritySimilar) {
				if (src) {
					ObjectArray<XType> conf(0x20);
					src->GetTypesConformsTo(conf);
					for (auto & c : conf) if (dest->GetCanonicalType() == c.GetCanonicalType()) return CastPrioritySimilar;
				} else {
					auto & ref = dest->GetTypeReference();
					if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Pointer) return CastPrioritySimilar;
					if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Reference &&
						ref.GetReferenceDestination().GetReferenceClass() == XI::Module::TypeReference::Class::Pointer) return CastPrioritySimilar;
				}
			}
			if (min_level <= CastPriorityConverter && src) {
				try {
					SafePointer<LObject> conv = src->GetMember(NameConverter);
					if (conv->GetClass() != Class::Function) throw Exception();
					Array<string> fcnl(0x20);
					static_cast<XFunction *>(conv.Inner())->ListOverloads(fcnl, true);
					for (auto & fcn : fcnl) {
						ObjectArray<XType> conf2(0x20);
						SafePointer< Array<XI::Module::TypeReference> > sgn = XI::Module::TypeReference(fcn).GetFunctionSignature();
						if (sgn->Length() > 1) continue;
						SafePointer<XType> retval = CreateType(sgn->ElementAt(0).QueryCanonicalName(), src->GetContext());
						retval->GetTypesConformsTo(conf2);
						for (auto & c : conf2) if (dest->GetCanonicalType() == c.GetCanonicalType()) return CastPriorityConverter;
					}
				} catch (...) {}
				ObjectArray<XType> conf(0x20);
				src->GetTypesConformsTo(conf);
				for (auto & c : conf) {
					try {
						SafePointer<LObject> ctor = dest->GetConstructorCast(&c);
						return CastPriorityConverter;
					} catch (...) {}
				}
			}
			return CastPriorityNoCast;
		}
		LObject * ConstructObject(XType * of_type, LObject * with_ctor, int argc, LObject ** argv)
		{
			if (with_ctor->GetClass() == Class::Function) {
				auto ctor = static_cast<XFunction *>(with_ctor);
				SafePointer<LObject> instance = CreateComputable(of_type->GetContext(), of_type, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceInit)));
				instance = UnwarpObject(instance);
				SafePointer<XMethod> ctor_over = ctor->SetInstance(instance);
				SafePointer<LObject> expr = ctor_over->Invoke(argc, argv);
				SafePointer<ConstructObjectProvider> provider = new ConstructObjectProvider;
				SafePointer<LObject> dtor = of_type->GetDestructor();
				if (dtor->GetClass() != Class::Null) provider->_dtor_ref = dtor->GetFullName();
				provider->_retval.SetRetain(of_type);
				provider->_init_expr = expr;
				return CreateComputable(of_type->GetContext(), provider);
			} else if (with_ctor->GetClass() == Class::FunctionOverload) {
				auto ctor = static_cast<XFunctionOverload *>(with_ctor);
				SafePointer<LObject> instance = CreateComputable(of_type->GetContext(), of_type, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceInit)));
				instance = UnwarpObject(instance);
				SafePointer<XMethodOverload> ctor_over = ctor->SetInstance(instance);
				SafePointer<LObject> expr = ctor_over->Invoke(argc, argv);
				SafePointer<ConstructObjectProvider> provider = new ConstructObjectProvider;
				SafePointer<LObject> dtor = of_type->GetDestructor();
				if (dtor->GetClass() != Class::Null) provider->_dtor_ref = dtor->GetFullName();
				provider->_retval.SetRetain(of_type);
				provider->_init_expr = expr;
				return CreateComputable(of_type->GetContext(), provider);
			} else throw InvalidArgumentException();
		}
		LObject * PerformTypeCast(XType * dest, LObject * src, int min_level, bool enforce_copy)
		{
			SafePointer<LObject> src_type_a = src->GetType();
			if (src_type_a->GetClass() != Class::Type) throw InvalidStateException();
			auto src_type = static_cast<XType *>(src_type_a.Inner());
			if (min_level <= CastPriorityIdentity) {
				if (dest->GetCanonicalType() == src_type->GetCanonicalType()) {
					if (enforce_copy) {
						SafePointer<LObject> ctor = dest->GetConstructorCopy();
						return ConstructObject(dest, ctor, 1, &src);
					} else { src->Retain(); return src; }
				}
			}
			if (min_level <= CastPrioritySimilar) {
				if (src->GetClass() == Class::NullLiteral) {
					auto & ref = dest->GetTypeReference();
					if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Pointer) {
						SafePointer<ReinterpretComputable> com = new ReinterpretComputable(dest, src);
						return CreateComputable(dest->GetContext(), com);
					}
					if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Reference &&
						ref.GetReferenceDestination().GetReferenceClass() == XI::Module::TypeReference::Class::Pointer) {
						SafePointer<ReinterpretComputable> com = new ReinterpretComputable(dest, src);
						SafePointer<LObject> rein = CreateComputable(dest->GetContext(), com);
						SafePointer<ReferenceComputable> ref_com = new ReferenceComputable(dest, src);
						return CreateComputable(dest->GetContext(), ref_com);
					}
				} else {
					ObjectArray<XType> conf(0x20);
					src_type->GetTypesConformsTo(conf);
					for (auto & c : conf) if (dest->GetCanonicalType() == c.GetCanonicalType()) {
						if (enforce_copy) {
							SafePointer<LObject> ctor = dest->GetConstructorCopy();
							SafePointer<LObject> subj = src_type->TransformTo(src, &c, false);
							return ConstructObject(dest, ctor, 1, subj.InnerRef());
						} else return src_type->TransformTo(src, &c, false);
					}
				}
			}
			if (min_level <= CastPriorityConverter) {
				try {
					SafePointer<LObject> conv = src_type->GetMember(NameConverter);
					if (conv->GetClass() != Class::Function) throw Exception();
					Array<string> fcnl(0x20);
					static_cast<XFunction *>(conv.Inner())->ListOverloads(fcnl, true);
					for (auto & fcn : fcnl) {
						ObjectArray<XType> conf2(0x20);
						SafePointer< Array<XI::Module::TypeReference> > sgn = XI::Module::TypeReference(fcn).GetFunctionSignature();
						if (sgn->Length() > 1) continue;
						SafePointer<XType> retval = CreateType(sgn->ElementAt(0).QueryCanonicalName(), src_type->GetContext());
						SafePointer<XType> act_retval;
						if (retval->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Reference) {
							act_retval = static_cast<XReference *>(retval.Inner())->GetElementType();
						} else act_retval.SetRetain(retval);
						act_retval->GetTypesConformsTo(conf2);
						for (auto & c : conf2) if (dest->GetCanonicalType() == c.GetCanonicalType()) {
							try {
								SafePointer<LObject> conv = src_type->GetCastMethod(retval);
								SafePointer<LObject> result; // retval ~ c == dest
								if (src->GetClass() == Class::Literal) {
									auto lit = ProcessLiteralConvert(src_type->GetContext(), static_cast<XLiteral *>(src), retval);
									if (lit) result = lit;
								}
								if (!result) {
									if (conv->GetClass() != Class::FunctionOverload) throw Exception();
									SafePointer<XMethodOverload> method = static_cast<XFunctionOverload *>(conv.Inner())->SetInstance(src);
									result = method->Invoke(0, 0);
								}
								return act_retval->TransformTo(result, dest, false);
							} catch (...) {}
						}
					}
				} catch (...) {}
				try {
					ObjectArray<XType> conf(0x20);
					SafePointer<XType> pre_dest;
					src_type->GetTypesConformsTo(conf);
					if (dest->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Class) {
						pre_dest.SetRetain(dest);
					} else if (dest->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Reference) {
						pre_dest = static_cast<XReference *>(dest)->GetElementType();
					} else throw Exception();
					for (auto & c : conf) {
						try {
							SafePointer<LObject> ctor = pre_dest->GetConstructorCast(&c);
							SafePointer<LObject> src_trans = src_type->TransformTo(src, &c, false);
							SafePointer<LObject> object = ConstructObject(pre_dest, ctor, 1, src_trans.InnerRef());
							return pre_dest->TransformTo(object, dest, false);
						} catch (...) {}
					}
				} catch (...) {}
			}
			if (min_level <= CastPriorityExplicit) return src_type->TransformTo(src, dest, true);
			throw LException(dest);
		}
		LObject * InitInstance(LObject * instance, LObject * with_value)
		{
			SafePointer<LObject> uw = UnwarpObject(instance);
			SafePointer<LObject> type = uw->GetType();
			SafePointer<LObject> copy_ctor = static_cast<XType *>(type.Inner())->GetConstructorCopy();
			SafePointer<LObject> copy_ctor_inst = static_cast<XFunctionOverload *>(copy_ctor.Inner())->SetInstance(uw);
			return copy_ctor_inst->Invoke(1, &with_value);
		}
		LObject * InitInstance(LObject * instance, int argc, LObject ** argv)
		{
			SafePointer<LObject> uw = UnwarpObject(instance);
			if (argc == 0) {
				SafePointer<LObject> type = uw->GetType();
				SafePointer<LObject> init_ctor = static_cast<XType *>(type.Inner())->GetConstructorInit();
				SafePointer<LObject> init_ctor_inst = static_cast<XFunctionOverload *>(init_ctor.Inner())->SetInstance(uw);
				return init_ctor_inst->Invoke(0, 0);
			} else {
				SafePointer<LObject> type = uw->GetType();
				SafePointer<LObject> ctor = type->GetMember(NameConstructor);
				if (ctor->GetClass() != Class::Function) throw InvalidStateException();
				SafePointer<LObject> ctor_inst = static_cast<XFunction *>(ctor.Inner())->SetInstance(uw);
				return ctor_inst->Invoke(argc, argv);
			}
		}
		LObject * ZeroInstance(LObject * instance)
		{
			SafePointer<LObject> uw = UnwarpObject(instance);
			SafePointer<LObject> type = uw->GetType();
			SafePointer<LObject> zero_ctor = static_cast<XType *>(type.Inner())->GetConstructorZero();
			SafePointer<LObject> zero_ctor_inst = static_cast<XFunctionOverload *>(zero_ctor.Inner())->SetInstance(uw);
			return zero_ctor_inst->Invoke(0, 0);
		}
		LObject * DestroyInstance(LObject * instance)
		{
			SafePointer<LObject> uw = UnwarpObject(instance);
			SafePointer<LObject> type = uw->GetType();
			SafePointer<LObject> dtor = static_cast<XType *>(type.Inner())->GetDestructor();
			if (dtor->GetClass() == Class::Null) {
				return 0;
			} else {
				SafePointer<LObject> dtor_inst = static_cast<XFunctionOverload *>(dtor.Inner())->SetInstance(uw);
				return dtor_inst->Invoke(0, 0);
			}
		}
	}
}