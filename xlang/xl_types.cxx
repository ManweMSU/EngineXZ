#include "xl_types.h"

#include "xl_base.h"
#include "xl_func.h"
#include "xl_var.h"
#include "xl_lit.h"
#include "xl_synth.h"

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
		void XType::ListInvokations(LObject * first, Volumes::List<InvokationDesc> & list)
		{
			bool regular = false;
			try {
				SafePointer<LObject> ctor = GetMember(NameConstructor);
				ctor->ListInvokations(first, list);
				regular = true;
			} catch (...) {}
			if (!regular) {
				SafePointer<LObject> type_void = GetContext().QueryObject(NameVoid);
				try {
					SafePointer<LObject> ctor = GetConstructorInit();
					InvokationDesc result;
					Volumes::KeyValuePair< SafePointer<LObject>, Class > rv(type_void, Class::Null);
					result.arglist.InsertLast(rv);
					list.InsertLast(result);
				} catch (...) {}
				try {
					SafePointer<LObject> self_ref = GetContext().QueryTypeReference(this);
					SafePointer<LObject> ctor = GetConstructorCopy();
					InvokationDesc result;
					Volumes::KeyValuePair< SafePointer<LObject>, Class > rv(type_void, Class::Null);
					Volumes::KeyValuePair< SafePointer<LObject>, Class > in(self_ref, Class::Null);
					result.arglist.InsertLast(rv);
					result.arglist.InsertLast(in);
					list.InsertLast(result);
				} catch (...) {}
			}
			for (auto & inv : list) {
				inv.arglist.RemoveFirst();
				SafePointer<LObject> self;
				self.SetRetain(this);
				Volumes::KeyValuePair< SafePointer<LObject>, Class > rv(self, Class::Null);
				inv.arglist.InsertFirst(rv);
			}
		}

		enum class PointerConstructorClass { Init, Copy };
		class TBaseMethodOverload : public XMethodOverload
		{
		public:
			virtual Class GetClass(void) override { return Class::MethodOverload; }
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetType(void) override { throw ObjectHasNoTypeException(this); }
			virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
			virtual void ListMembers(Volumes::Dictionary<string, Class> & list) override {}
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
			virtual VirtualFunctionDesc & GetVFDesc(void) override { throw InvalidStateException(); }
			virtual XClass * GetInstanceType(void) override { throw InvalidStateException(); }
			virtual string GetCanonicalType(void) override { throw InvalidStateException(); }
			virtual LObject * InvokeNoVirtual(int argc, LObject ** argv) override { return Invoke(argc, argv); }
		};
		class TBaseFunctionOverload : public XFunctionOverload
		{
			uint _internal_flags;
		public:
			virtual Class GetClass(void) override { return Class::FunctionOverload; }
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetType(void) override { throw ObjectHasNoTypeException(this); }
			virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
			virtual void ListMembers(Volumes::Dictionary<string, Class> & list) override {}
			virtual LObject * Invoke(int argc, LObject ** argv) override { throw ObjectHasNoSuchOverloadException(this, argc, argv); }
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
			virtual VirtualFunctionDesc & GetVFDesc(void) override { throw InvalidStateException(); }
			virtual FunctionImplementationDesc & GetImplementationDesc(void) override { throw InvalidStateException(); }
			virtual bool NeedsInstance(void) override { return true; }
			virtual bool Throws(void) override { return false; }
			virtual uint & GetFlags(void) override { _internal_flags = 0; return _internal_flags; }
			virtual XClass * GetInstanceType(void) override { throw InvalidStateException(); }
			virtual string GetCanonicalType(void) override { throw InvalidStateException(); }
			virtual bool CheckForInline(void) override { return true; }
		};
		class PointerConstructorInstance : public TBaseMethodOverload
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
			virtual void ListInvokations(LObject * first, Volumes::List<InvokationDesc> & list) override {}
			virtual string ToString(void) const override { return L"instanced pointer built-in constructor"; }
			virtual LContext & GetContext(void) override { return _ctx; }
		};
		class PointerConstructor : public TBaseFunctionOverload
		{
			LContext & _ctx;
			PointerConstructorClass _cls;
		public:
			PointerConstructor(LContext & ctx, PointerConstructorClass cls) : _ctx(ctx), _cls(cls) {}
			virtual ~PointerConstructor(void) override {}
			virtual void ListInvokations(LObject * first, Volumes::List<InvokationDesc> & list) override {}
			virtual string ToString(void) const override { return L"pointer built-in constructor"; }
			virtual XMethodOverload * SetInstance(LObject * instance) override { return new PointerConstructorInstance(_ctx, _cls, instance); }
			virtual LContext & GetContext(void) override { return _ctx; }
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
		class OffsetComputable : public Object, public IComputableProvider
		{
			SafePointer<XType> _type;
			SafePointer<LObject> _instance, _index;
			XA::ObjectSize _size;
			bool _backward;
		public:
			OffsetComputable(XType * type, LObject * instance, XA::ObjectSize offs, bool backward) : _size(offs), _backward(backward) { _type.SetRetain(type); _instance.SetRetain(instance); }
			OffsetComputable(XType * type, LObject * instance, XA::ObjectSize element, LObject * index) : _size(element), _backward(false) { _type.SetRetain(type); _instance.SetRetain(instance); _index.SetRetain(index); }
			virtual ~OffsetComputable(void) override {}
			virtual Object * ComputableProviderQueryObject(void) override { return this; }
			virtual XType * ComputableGetType(void) override { _type->Retain(); return _type; }
			virtual XA::ExpressionTree ComputableEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
			{
				auto node = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformAddressOffset, XA::ReferenceFlagInvoke));
				SafePointer<LObject> intype = _instance->GetType();
				XA::TH::AddTreeInput(node, _instance->Evaluate(func, error_ctx), static_cast<XType *>(intype.Inner())->GetArgumentSpecification());
				if (_index) {
					SafePointer<LObject> xtype = _index->GetType();
					XA::TH::AddTreeInput(node, _index->Evaluate(func, error_ctx), static_cast<XType *>(xtype.Inner())->GetArgumentSpecification());
					XA::TH::AddTreeInput(node, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLiteral)), XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, _size));
				} else {
					XA::TH::AddTreeInput(node, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLiteral)), XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, _size));
					if (_backward) XA::TH::AddTreeInput(node, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLiteral)), XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, -1, 0));
				}
				XA::TH::AddTreeOutput(node, _type->GetArgumentSpecification());
				return node;
			}
		};
		class VirtualInitComputable : public Object, public IComputableProvider
		{
			SafePointer<XType> _type;
			SafePointer<LObject> _instance;
			int _index;
			string _symbol;
		public:
			VirtualInitComputable(XType * type, LObject * instance, int index, const string & symbol) : _index(index), _symbol(symbol) { _type.SetRetain(type); _instance.SetRetain(instance); }
			virtual ~VirtualInitComputable(void) override {}
			virtual Object * ComputableProviderQueryObject(void) override { return this; }
			virtual XType * ComputableGetType(void) override { _type->Retain(); return _type; }
			virtual XA::ExpressionTree ComputableEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
			{
				return MakeBlt(
					MakeOffset(_instance->Evaluate(func, error_ctx), XA::TH::MakeSize(0, _index), XA::TH::MakeSize(0, _index + 1), XA::TH::MakeSize(0, 1)),
					MakeAddressOf(MakeSymbolReference(func, _symbol), XA::TH::MakeSize(0, 1)), XA::TH::MakeSize(0, 1)
				);
			}
		};

		class TypeClass : public XClass
		{
			struct _interface_info
			{
				TypeClass * _class;
				int _vft_index;
				XA::ObjectSize _vft_offset, _base_offset;
			};

			LContext & _ctx;
			string _name, _path, _cn;
			bool _local, _locked;
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
				_locked = false;
				_type_spec = XA::TH::MakeSpec(XA::ArgumentSemantics::Object, 0, 0);
				_nature = XI::Module::Class::Nature::Standard;
				_vft_index = -1;
				_last_vft_index = 0;
				_vft_offset = XA::TH::MakeSize(-1, -1);
				_parent._class = 0;
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
			virtual void ListMembers(Volumes::Dictionary<string, Class> & list) override { for (auto & m : _members) list.Append(m.key, m.value->GetClass()); }
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
				for (auto & i : _interfaces) {
					ref = CreateType(XI::Module::TypeReference::MakeReference(i._class->GetCanonicalType()), _ctx);
					types.Append(i._class);
					types.Append(ref);
				}
				if (_parent._class) _parent._class->GetTypesConformsTo(types);
			}
			virtual LObject * TransformTo(LObject * subject, XType * type, bool cast_explicit) override
			{
				if (cast_explicit) {
					if (type->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Reference) {
						SafePointer<XType> cls = static_cast<XReference *>(type)->GetElementType();
						if (cls->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Class) {
							try {
								auto xcls = static_cast<TypeClass *>(cls.Inner());
								XA::ObjectSize rebase = xcls->GetRebase(this);
								SafePointer<OffsetComputable> offs_com = new OffsetComputable(xcls, subject, rebase, true);
								SafePointer<XComputable> offs = CreateComputable(_ctx, offs_com);
								SafePointer<ReferenceComputable> ref_com = new ReferenceComputable(xcls, offs);
								return CreateComputable(_ctx, ref_com);
							} catch (...) {}
						}
					}
					if (type->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Pointer &&
						_type_spec.size.num_bytes == 0 && _type_spec.size.num_words == 1) {
						SafePointer<ReinterpretComputable> com = new ReinterpretComputable(type, subject);
						return CreateComputable(_ctx, com);
					}
				} else {
					bool ref;
					string class_cn;
					if (type->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Class) {
						ref = false;
						class_cn = type->GetCanonicalType();
					} else if (type->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Reference) {
						SafePointer<XType> element = static_cast<XReference *>(type)->GetElementType();
						if (element->GetCanonicalTypeClass() != XI::Module::TypeReference::Class::Class) throw LException(this);
						ref = true;
						class_cn = element->GetCanonicalType();
					} else throw LException(this);
					if (class_cn == _cn) {
						if (ref) {
							SafePointer<ReferenceComputable> com = new ReferenceComputable(this, subject);
							return CreateComputable(_ctx, com);
						} else {
							SafePointer<LObject> unwarp = UnwarpObject(subject);
							SafePointer<LObject> subj_type = unwarp->GetType();
							if (static_cast<XType *>(subj_type.Inner())->GetCanonicalType() == _cn) { subject->Retain(); return subject; }
							SafePointer<ReinterpretComputable> com = new ReinterpretComputable(this, subject);
							return CreateComputable(_ctx, com);
						}
					}
					for (auto & i : _interfaces) if (i._class->GetCanonicalType() == class_cn) {
						if (i._base_offset.num_bytes || i._base_offset.num_words) {
							SafePointer<OffsetComputable> offs = new OffsetComputable(i._class, subject, i._base_offset, false);
							SafePointer<LObject> shifted = CreateComputable(_ctx, offs);
							if (ref) {
								SafePointer<ReferenceComputable> com = new ReferenceComputable(i._class, shifted);
								return CreateComputable(_ctx, com);
							} else { shifted->Retain(); return shifted; }
						} else {
							if (ref) {
								SafePointer<ReferenceComputable> com = new ReferenceComputable(i._class, subject);
								return CreateComputable(_ctx, com);
							} else {
								SafePointer<ReinterpretComputable> com = new ReinterpretComputable(i._class, subject);
								return CreateComputable(_ctx, com);
							}
						}
					}
					if (_parent._class) return _parent._class->TransformTo(subject, type, cast_explicit);
				}
				throw LException(this);
			}
			virtual bool IsLocked(void) override { return _locked; }
			virtual XI::Module::Class::Nature GetLanguageSemantics(void) override { return _nature; }
			virtual void OverrideArgumentSpecification(XA::ArgumentSpecification spec) override { _type_spec = spec; }
			virtual void OverrideLanguageSemantics(XI::Module::Class::Nature spec) override { _nature = spec; }
			virtual void UpdateInternals(void) override
			{
				_last_vft_index = 0;
				Volumes::Stack<XClass *> chain;
				auto current = GetParentClass();
				while (current) {
					chain.Push(current);
					current = current->GetParentClass();
				}
				bool first = true;
				while (!chain.IsEmpty()) {
					auto cls = chain.Pop();
					auto src = static_cast<TypeClass *>(cls);
					_last_vft_index += src->_interfaces.Length();
					if (first) {
						_vft_index = src->_vft_index;
						_vft_offset = src->_vft_offset;
						_parent._vft_index = _vft_index;
						_parent._vft_offset = _vft_offset;
						first = false;
					}
				}
				for (auto & i : _interfaces) {
					_last_vft_index++;
					i._vft_index = _last_vft_index;
				}
				for (auto & m : _members) if (m.value->GetClass() == Class::Function) {
					auto xfunc = static_cast<XFunction *>(m.value.Inner());
					Array<string> over(0x20);
					xfunc->ListOverloads(over, true);
					for (auto & o : over) {
						auto ofunc = xfunc->GetOverloadT(o, true);
						auto & vfd = ofunc->GetVFDesc();
						if (vfd.vf_index >= 0 && vfd.vft_index >= 0) {
							if (vfd.vft_index) {
								vfd.vftp_offset = _interfaces[vfd.vft_index - 1]._vft_offset;
								vfd.base_offset = _interfaces[vfd.vft_index - 1]._base_offset;
							} else {
								vfd.vftp_offset = _vft_offset;
								vfd.base_offset = XA::TH::MakeSize(0, 0);
							}
						}
					}
				}
			}
			virtual void AdoptParentClass(XClass * parent, bool alternate) override
			{
				if (_parent._class || _interfaces.Length()) throw InvalidStateException();
				auto source = static_cast<TypeClass *>(parent);
				if (_nature == XI::Module::Class::Nature::Interface && source->_nature != XI::Module::Class::Nature::Interface) throw InvalidArgumentException();
				if (_nature != XI::Module::Class::Nature::Interface && source->_nature == XI::Module::Class::Nature::Interface) throw InvalidArgumentException();
				if (alternate) {
					_type_spec = source->_type_spec;
					_nature = source->_nature;
					_last_vft_index = source->_last_vft_index;
					_vft_index = source->_vft_index;
					_vft_offset = source->_vft_offset;
					_parent._vft_index = _vft_index;
					_parent._vft_offset = _vft_offset;
				}
				_parent._class = source;
				_parent._base_offset = XA::TH::MakeSize(0, 0);
			}
			virtual void AdoptInterface(XClass * interface) override
			{
				auto source = static_cast<TypeClass *>(interface);
				if (_nature == XI::Module::Class::Nature::Interface) throw InvalidArgumentException();
				if (source->_nature != XI::Module::Class::Nature::Interface) throw InvalidArgumentException();
				ObjectArray<XType> conf(0x20);
				GetTypesConformsTo(conf);
				for (auto & c : conf) if (c.GetCanonicalType() == interface->GetCanonicalType()) return;
				while (_type_spec.size.num_bytes & 0x07) _type_spec.size.num_bytes++;
				_last_vft_index++;
				_interface_info info;
				info._class = source;
				info._vft_index = _last_vft_index;
				info._vft_offset = info._base_offset = _type_spec.size;
				_interfaces.Append(info);
				_type_spec.size.num_words++;
			}
			virtual void AdoptInterface(XClass * interface, int vft_index, XA::ObjectSize vft_offset) override
			{
				auto source = static_cast<TypeClass *>(interface);
				if (_nature == XI::Module::Class::Nature::Interface) throw InvalidArgumentException();
				if (source->_nature != XI::Module::Class::Nature::Interface) throw InvalidArgumentException();
				ObjectArray<XType> conf(0x20);
				GetTypesConformsTo(conf);
				for (auto & c : conf) if (c.GetCanonicalType() == interface->GetCanonicalType()) return;
				_last_vft_index = max(_last_vft_index, vft_index);
				_interface_info info;
				info._class = source;
				info._vft_index = vft_index;
				info._vft_offset = info._base_offset = vft_offset;
				_interfaces.Append(info);
			}
			virtual XClass * GetParentClass(void) override { return _parent._class; }
			virtual int GetInterfaceCount(void) override { return _interfaces.Length(); }
			virtual XClass * GetInterface(int index) override { return _interfaces[index]._class; }
			virtual XA::ObjectSize GetPrimaryVFT(void) override { return _vft_offset; }
			virtual void SetPrimaryVFT(XA::ObjectSize offset) override
			{
				_vft_offset = offset;
				if (_vft_offset.num_bytes != 0xFFFFFFFF || _vft_offset.num_words != 0xFFFFFFFF) _vft_index = 0;
				else _vft_index = -1;
			}
			virtual VirtualFunctionDesc FindVirtualFunctionInfo(const string & name, const string & sign, uint & flags) override
			{
				try {
					SafePointer<LObject> func = GetMember(name);
					if (func->GetClass() != Class::Function) throw Exception();
					auto overload = static_cast<XFunction *>(func.Inner())->GetOverloadT(sign, true);
					auto info = overload->GetVFDesc();
					if (info.vf_index >= 0 && info.vft_index >= 0) {
						flags = overload->GetFlags();
						return info;
					}
				} catch (...) {}
				for (auto & i : _interfaces) {
					auto info = i._class->FindVirtualFunctionInfo(name, sign, flags);
					if (info.vf_index >= 0 && info.vft_index >= 0) {
						info.base_offset = i._base_offset;
						info.vftp_offset = i._vft_offset;
						info.vft_index = i._vft_index;
						return info;
					}
				}
				if (_parent._class) return _parent._class->FindVirtualFunctionInfo(name, sign, flags);
				VirtualFunctionDesc zero;
				zero.base_offset = zero.vfp_offset = zero.vftp_offset = XA::TH::MakeSize(0xFFFFFFFF, 0xFFFFFFFF);
				zero.vf_index = zero.vft_index = -1;
				return zero;
			}
			virtual int SizeOfPrimaryVFT(void) override
			{
				int size = 0;
				if (_parent._class) size = _parent._class->SizeOfPrimaryVFT();
				for (auto & m : _members) if (m.value->GetClass() == Class::Function) {
					auto func = static_cast<XFunction *>(m.value.Inner());
					Array<string> list(0x20);
					func->ListOverloads(list, true);
					for (auto & l : list) {
						auto overload = func->GetOverloadT(l, true);
						auto & vfd = overload->GetVFDesc();
						if (vfd.vft_index == 0) size = max(size, vfd.vf_index + 1);
					}
				}
				return size;
			}
			virtual void GetRangeVFT(int & first, int & last) override
			{
				if (_vft_index >= 0) {
					first = 0; last = _last_vft_index;
				} else {
					if (_last_vft_index) { first = 1; last = _last_vft_index; }
					else first = last = -1;
				}
			}
			virtual void GetRangeVF(int vft, int & first, int & last) override
			{
				first = last = -1;
				for (auto & m : _members) if (m.value->GetClass() == Class::Function) {
					auto func = static_cast<XFunction *>(m.value.Inner());
					Array<string> list(0x20);
					func->ListOverloads(list, true);
					for (auto & l : list) {
						auto overload = func->GetOverloadT(l, true);
						auto & vfd = overload->GetVFDesc();
						if (vfd.vft_index == vft) {
							if (first == -1) first = last = vfd.vf_index; else {
								if (vfd.vf_index < first) first = vfd.vf_index;
								if (vfd.vf_index > last) last = vfd.vf_index;
							}
						}
					}
				}
				for (auto & i : _interfaces) if (i._vft_index == vft) {
					int local_first, local_last;
					i._class->GetRangeVF(0, local_first, local_last);
					if (first == -1) {
						first = local_first;
						last = local_last;
					} else {
						if (local_first < first) first = local_first;
						if (local_last > last) last = local_last;
					}
				}
				if (_parent._class) {
					int local_first, local_last;
					_parent._class->GetRangeVF(vft, local_first, local_last);
					if (first == -1) {
						first = local_first;
						last = local_last;
					} else {
						if (local_first < first) first = local_first;
						if (local_last > last) last = local_last;
					}
				}
			}
			virtual XA::ObjectSize GetOffsetVFT(int vft) override
			{
				if (vft == 0) return _vft_offset;
				for (auto & i : _interfaces) if (i._vft_index == vft) return i._vft_offset;
				if (_parent._class) return _parent._class->GetOffsetVFT(vft);
				return _vft_offset;
			}
			virtual LObject * GetCurrentImplementationForVF(int vft, int vf) override
			{
				for (auto & m : _members) if (m.value->GetClass() == Class::Function) {
					auto func = static_cast<XFunction *>(m.value.Inner());
					Array<string> list(0x20);
					func->ListOverloads(list, true);
					for (auto & l : list) {
						auto overload = func->GetOverloadT(l, true);
						if (overload->GetImplementationDesc()._pure) continue;
						auto & vfd = overload->GetVFDesc();
						if (vfd.vft_index == vft && vfd.vf_index == vf) return overload;
					}
				}
				for (auto & i : _interfaces) if (i._vft_index == vft) {
					auto impl = i._class->GetCurrentImplementationForVF(0, vf);
					if (impl) return impl;
				}
				if (_parent._class) return _parent._class->GetCurrentImplementationForVF(vft, vf);
				return 0;
			}
			virtual LObject * CreateVFT(int vft, ObjectArray<LObject> & init_seq) override
			{
				auto name = FormatString(L"%0_%1", NameVFT, vft);
				try {
					SafePointer<LObject> vft_var = _ctx.QueryObject(_path + L"." + name);
					if (vft_var->GetClass() != Class::Variable) throw InvalidStateException();
					return vft_var;
				} catch (...) {}
				SafePointer<XType> type_void = CreateType(XI::Module::TypeReference::MakeClassReference(NameVoid), _ctx);
				int vf_min, vf_max;
				GetRangeVF(vft, vf_min, vf_max);
				auto vft_var = _ctx.CreateVariable(this, name, type_void, XA::TH::MakeSize(0, vf_max + 1));
				for (int i = vf_min; i <= vf_max; i++) {
					auto impl = GetCurrentImplementationForVF(vft, i);
					if (impl) {
						SafePointer<VirtualInitComputable> com = new VirtualInitComputable(type_void, vft_var, i, impl->GetFullName());
						SafePointer<XComputable> init = CreateComputable(_ctx, com);
						init_seq.Append(init);
					}
				}
				return vft_var;
			}
			virtual XA::ObjectSize GetRebase(XClass * for_class) override
			{
				if (for_class->GetCanonicalType() == GetCanonicalType()) return XA::TH::MakeSize(0, 0);
				for (auto & i : _interfaces) try {
					auto rebase = i._class->GetRebase(for_class);
					return XA::TH::MakeSize(rebase.num_bytes + i._base_offset.num_bytes, rebase.num_words + i._base_offset.num_words);
				} catch (...) {}
				if (_parent._class) return _parent._class->GetRebase(for_class);
				throw InvalidArgumentException();
			}
			virtual void ListFields(ObjectArray<LObject> & list) override { for (auto & m : _members) if (m.value->GetClass() == Class::Field) list.Append(m.value); }
			virtual void Lock(bool lock) override { _locked = lock; }
			virtual string ToString(void) const override { return L"class " + _path; }
		};
		class TypeArray : public XArray
		{
			LContext & _ctx;
			string _cn;
			XI::Module::TypeReference _ref;

			class _array_instanced_method : public TBaseMethodOverload
			{
				SafePointer<XArray> _base;
				SafePointer<LObject> _instance;
				string _method;

				LObject * _internal_create_literal(int value)
				{
					XI::Module::Literal lit;
					lit.contents = XI::Module::Literal::Class::SignedInteger;
					lit.length = 4;
					lit.data_sint64 = value;
					return CreateLiteral(GetContext(), lit);
				}
			public:
				_array_instanced_method(XArray * base, LObject * instance, const string & method) : _method(method) { _base.SetRetain(base); _instance.SetRetain(instance); }
				virtual ~_array_instanced_method(void) override {}
				virtual LObject * Invoke(int argc, LObject ** argv) override
				{
					if (_method == OperatorSubscript) {
						if (!argc) throw ObjectHasNoSuchOverloadException(this, argc, argv);
						SafePointer<XType> element = _base->GetElementType();
						SafePointer<XType> type_intptr = CreateType(XI::Module::TypeReference::MakeClassReference(NameUIntPtr), GetContext());
						SafePointer<LObject> index = PerformTypeCast(type_intptr, argv[0], CastPriorityConverter);
						SafePointer<OffsetComputable> com = new OffsetComputable(element, _instance, element->GetArgumentSpecification().size, index.Inner());
						SafePointer<XComputable> offs = CreateComputable(GetContext(), com);
						if (argc > 1) {
							SafePointer<LObject> op = offs->GetMember(OperatorSubscript);
							return op->Invoke(argc - 1, argv + 1);
						} else { offs->Retain(); return offs; }
					} else if (_method == IteratorBegin) {
						return _internal_create_literal(0);
					} else if (_method == IteratorEnd) {
						return _internal_create_literal(_base->GetVolume() - 1);
					} else if (_method == IteratorPostEnd) {
						return _internal_create_literal(_base->GetVolume());
					} else if (_method == IteratorPreBegin) {
						return _internal_create_literal(-1);
					} else throw ObjectHasNoSuchOverloadException(this, argc, argv);
				}
				virtual void ListInvokations(LObject * first, Volumes::List<InvokationDesc> & list) override
				{
					if (_method == OperatorSubscript) {
						SafePointer<LObject> type = _base->GetContext().QueryObject(NameUIntPtr);
						SafePointer<LObject> element = _base->GetElementType();
						InvokationDesc own;
						Volumes::List<InvokationDesc> derived;
						Volumes::KeyValuePair< SafePointer<LObject>, Class > rv(element, Class::Null);
						Volumes::KeyValuePair< SafePointer<LObject>, Class > in(type, Class::Null);
						own.arglist.InsertLast(rv);
						own.arglist.InsertLast(in);
						list.InsertLast(own);
						try {
							SafePointer<LObject> fake_object = CreateComputable(_base->GetContext(), static_cast<XType *>(element.Inner()), XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceNull)));
							SafePointer<LObject> ds = element->GetMember(OperatorSubscript);
							if (ds->GetClass() == Class::Function) {
								ds = static_cast<XFunction *>(ds.Inner())->SetInstance(fake_object);
							} else if (ds->GetClass() == Class::FunctionOverload) {
								ds = static_cast<XFunctionOverload *>(ds.Inner())->SetInstance(fake_object);
							}
							ds->ListInvokations(0, derived);
						} catch (...) {}
						for (auto & d : derived) {
							auto local_rv = d.arglist.GetFirst();
							d.arglist.InsertAfter(local_rv, in);
							list.InsertLast(d);
						}
					} else if (_method == IteratorBegin || _method == IteratorEnd || _method == IteratorPostEnd || _method == IteratorPreBegin) {
						SafePointer<LObject> type = _base->GetContext().QueryObject(NameInt32);
						Volumes::KeyValuePair< SafePointer<LObject>, Class > rv(type, Class::Null);
						InvokationDesc result;
						result.arglist.InsertLast(rv);
						list.InsertLast(result);
					}
				}
				virtual string ToString(void) const override { return L"instanced array built-in method " + _method; }
				virtual LContext & GetContext(void) override { return _base->GetContext(); }
			};
			class _array_method : public TBaseFunctionOverload
			{
				SafePointer<XArray> _base;
				string _method;
			public:
				_array_method(XArray * base, const string & method) : _method(method) { _base.SetRetain(base); }
				virtual ~_array_method(void) override {}
				virtual void ListInvokations(LObject * first, Volumes::List<InvokationDesc> & list) override {}
				virtual string ToString(void) const override { return L"array built-in method " + _method; }
				virtual XMethodOverload * SetInstance(LObject * instance) override { return new _array_instanced_method(_base, instance, _method); }
				virtual LContext & GetContext(void) override { return _base->GetContext(); }
			};
		public:
			TypeArray(const string & cn, LContext & ctx) : _cn(cn), _ctx(ctx), _ref(_cn) {}
			virtual ~TypeArray(void) override {}
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetMember(const string & name) override
			{
				if (name == OperatorSubscript || name == IteratorBegin || name == IteratorEnd || name == IteratorPreBegin || name == IteratorPostEnd) {
					return new _array_method(this, name);
				} else if (name == OperatorAssign) {
					return CreateStaticArrayRoutine(this, CreateMethodAssign);
				} else throw ObjectHasNoSuchMemberException(this, name);
			}
			virtual void ListMembers(Volumes::Dictionary<string, Class> & list) override
			{
				list.Append(OperatorSubscript, Class::FunctionOverload);
				list.Append(IteratorBegin, Class::FunctionOverload);
				list.Append(IteratorEnd, Class::FunctionOverload);
				list.Append(IteratorPreBegin, Class::FunctionOverload);
				list.Append(IteratorPostEnd, Class::FunctionOverload);
				list.Append(OperatorAssign, Class::FunctionOverload);
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
			virtual LObject * GetConstructorInit(void) override { return CreateStaticArrayRoutine(this, CreateMethodConstructorInit); }
			virtual LObject * GetConstructorCopy(void) override { return CreateStaticArrayRoutine(this, CreateMethodConstructorCopy); }
			virtual LObject * GetConstructorZero(void) override { return CreateStaticArrayRoutine(this, CreateMethodConstructorZero); }
			virtual LObject * GetConstructorMove(void) override { return CreateStaticArrayRoutine(this, CreateMethodConstructorMove); }
			virtual LObject * GetConstructorCast(XType * from_type) override { throw ObjectHasNoSuchMemberException(this, NameConstructor); }
			virtual LObject * GetCastMethod(XType * to_type) override { throw ObjectHasNoSuchMemberException(this, NameConverter); }
			virtual LObject * GetDestructor(void) override { return CreateStaticArrayRoutine(this, CreateMethodDestructor); }
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
					if (type->GetCanonicalType() == GetCanonicalType()) {
						SafePointer<LObject> unwarped = UnwarpObject(subject);
						SafePointer<LObject> unwarped_type = unwarped->GetType();
						if (static_cast<XType *>(unwarped_type.Inner())->GetCanonicalType() == GetCanonicalType()) { subject->Retain(); return subject; } else {
							SafePointer<ReinterpretComputable> com = new ReinterpretComputable(type, subject);
							return CreateComputable(_ctx, com);
						}
					}
					auto element = _ref.GetArrayElement();
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
						SafePointer<ReinterpretComputable> com = new ReinterpretComputable(element_ptr_type, address);
						return CreateComputable(_ctx, com);
					}
					if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Reference &&
						ref.GetReferenceDestination().QueryCanonicalName() == element_ptr) {
						SafePointer<LObject> take_addr = _ctx.QueryAddressOfOperator();
						SafePointer<LObject> address = take_addr->Invoke(1, &subject);
						SafePointer<ReinterpretComputable> com = new ReinterpretComputable(element_ptr_type, address);
						SafePointer<LObject> reint = CreateComputable(_ctx, com);
						SafePointer<ReferenceComputable> com2 = new ReferenceComputable(element_ptr_type, reint);
						return CreateComputable(_ctx, com2);
					}
				}
				throw LException(this);
			}
			virtual bool IsLocked(void) override { SafePointer<XType> inner = GetElementType(); return inner->IsLocked(); }
			virtual XType * GetElementType(void) override { return CreateType(_ref.GetArrayElement().QueryCanonicalName(), _ctx); }
			virtual int GetVolume(void) override { return _ref.GetArrayVolume(); }
			virtual LObject * ExtractElement(LObject * instance, int index) override
			{
				SafePointer<XType> element = GetElementType();
				auto size = element->GetArgumentSpecification().size;
				auto offset = XA::TH::MakeSize(size.num_bytes * index, size.num_words * index);
				SafePointer<OffsetComputable> com = new OffsetComputable(element, instance, offset, false);
				return CreateComputable(GetContext(), com);
			}
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
			class _pointer_operator_instance : public TBaseMethodOverload
			{
				string _op;
				SafePointer<XPointer> _ptr_type;
				SafePointer<LObject> _instance;
			public:
				_pointer_operator_instance(const string & op, XPointer * ptr, LObject * inst) : _op(op) { _ptr_type.SetRetain(ptr); _instance.SetRetain(inst); }
				virtual ~_pointer_operator_instance(void) override {}
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
				virtual void ListInvokations(LObject * first, Volumes::List<InvokationDesc> & list) override
				{
					auto & ref = _ptr_type->GetTypeReference();
					if (_op == OperatorInvoke && ref.GetPointerDestination().GetReferenceClass() == XI::Module::TypeReference::Class::Function) {
						SafePointer< Array<XI::Module::TypeReference> > sgn = ref.GetPointerDestination().GetFunctionSignature();
						InvokationDesc result;
						for (auto & t : *sgn) {
							SafePointer<LObject> type = CreateType(t.QueryCanonicalName(), _ptr_type->GetContext());
							Volumes::KeyValuePair< SafePointer<LObject>, Class > pair(type, Class::Null);
							result.arglist.InsertLast(pair);
						}
						list.InsertLast(result);
					} else if (_op == OperatorSubscript && ref.GetPointerDestination().GetReferenceClass() != XI::Module::TypeReference::Class::Function) {
						SafePointer<LObject> type_result = _ptr_type->GetElementType();
						SafePointer<LObject> type_index = CreateType(XI::Module::TypeReference::MakeClassReference(NameUIntPtr), _ptr_type->GetContext());
						InvokationDesc result;
						result.arglist.InsertLast(Volumes::KeyValuePair< SafePointer<LObject>, Class >(type_result, Class::Null));
						result.arglist.InsertLast(Volumes::KeyValuePair< SafePointer<LObject>, Class >(type_index, Class::Null));
						list.InsertLast(result);
					}
				}
				virtual string ToString(void) const override { return L"instanced pointer built-in operator"; }
				virtual LContext & GetContext(void) override { return _ptr_type->GetContext(); }
			};
			class _pointer_operator : public TBaseFunctionOverload
			{
				string _op;
				SafePointer<XPointer> _ptr_type;
			public:
				_pointer_operator(XPointer * ptr, const string & op) : _op(op) { _ptr_type.SetRetain(ptr); }
				virtual ~_pointer_operator(void) override {}
				virtual void ListInvokations(LObject * first, Volumes::List<InvokationDesc> & list) override {}
				virtual string ToString(void) const override { return L"pointer built-in operator"; }
				virtual XMethodOverload * SetInstance(LObject * instance) override { return new _pointer_operator_instance(_op, _ptr_type, instance); }
				virtual LContext & GetContext(void) override { return _ptr_type->GetContext(); }
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
			virtual void ListMembers(Volumes::Dictionary<string, Class> & list) override
			{
				list.Append(OperatorInvoke, Class::FunctionOverload);
				list.Append(OperatorSubscript, Class::FunctionOverload);
				list.Append(OperatorFollow, Class::FunctionOverload);
				list.Append(OperatorAssign, Class::FunctionOverload);
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
				if (_ref.GetPointerDestination().GetReferenceClass() == XI::Module::TypeReference::Class::Class) {
					SafePointer<XType> cls = GetElementType();
					ObjectArray<XType> cls_conf(0x10);
					cls->GetTypesConformsTo(cls_conf);
					for (auto & c : cls_conf) if (c.GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Class) {
						SafePointer<XType> class_ptr = CreateType(XI::Module::TypeReference::MakePointer(c.GetCanonicalType()), _ctx);
						SafePointer<XType> class_ptr_ref = CreateType(XI::Module::TypeReference::MakeReference(class_ptr->GetCanonicalType()), _ctx);
						types.Append(class_ptr);
						types.Append(class_ptr_ref);
					}
				} else {
					SafePointer<XType> self_ref = CreateType(XI::Module::TypeReference::MakeReference(GetCanonicalType()), _ctx);
					types.Append(this);
					types.Append(self_ref);
				}
				SafePointer<XType> nothing_ptr = CreateType(XI::Module::TypeReference::MakePointer(XI::Module::TypeReference::MakeClassReference(NameVoid)), _ctx);
				SafePointer<XType> nothing_ptr_ref = CreateType(XI::Module::TypeReference::MakeReference(nothing_ptr->GetCanonicalType()), _ctx);
				types.Append(nothing_ptr);
				types.Append(nothing_ptr_ref);
			}
			virtual LObject * TransformTo(LObject * subject, XType * type, bool cast_explicit) override
			{
				if (cast_explicit) {
					if (_ref.GetPointerDestination().GetReferenceClass() == XI::Module::TypeReference::Class::Class) {
						bool ref = false;
						SafePointer<XType> src_cls = GetElementType();
						SafePointer<XType> dest_cls;
						dest_cls.SetRetain(type);
						if (dest_cls->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Reference) {
							ref = true;
							dest_cls = static_cast<XReference *>(dest_cls.Inner())->GetElementType();
						}
						if (dest_cls->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Pointer) {
							dest_cls = static_cast<XPointer *>(dest_cls.Inner())->GetElementType();
							if (dest_cls->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Class) {
								try {
									SafePointer<XType> src_cls_ref = CreateType(XI::Module::TypeReference::MakeReference(src_cls->GetCanonicalType()), _ctx);
									SafePointer<XType> dest_cls_ref = CreateType(XI::Module::TypeReference::MakeReference(dest_cls->GetCanonicalType()), _ctx);
									SafePointer<XType> dest_cls_ptr = CreateType(XI::Module::TypeReference::MakePointer(dest_cls->GetCanonicalType()), _ctx);
									SafePointer<ReinterpretComputable> reint_com = new ReinterpretComputable(src_cls_ref, subject);
									SafePointer<XComputable> reint = CreateComputable(_ctx, reint_com);
									SafePointer<LObject> casted = src_cls->TransformTo(reint, dest_cls_ref, true);
									SafePointer<LObject> casted_unwarped = UnwarpObject(casted);
									SafePointer<ReinterpretComputable> reint2_com = new ReinterpretComputable(dest_cls_ptr, casted_unwarped);
									SafePointer<XComputable> reint2 = CreateComputable(_ctx, reint2_com);
									if (ref) {
										SafePointer<ReferenceComputable> ref_com = new ReferenceComputable(dest_cls_ptr, reint2);
										return CreateComputable(_ctx, ref_com);
									} else { reint2->Retain(); return reint2; }
								} catch (...) {}
							}
						}
					}
					auto dest_size = type->GetArgumentSpecification().size;
					SafePointer<LObject> dtor = type->GetDestructor();
					if (dest_size.num_bytes == 0 && dest_size.num_words == 1 && dtor->GetClass() == Class::Null) {
						SafePointer<ReinterpretComputable> com = new ReinterpretComputable(type, subject);
						return CreateComputable(_ctx, com);
					}
				} else {
					if (type->GetCanonicalType() == GetCanonicalType()) {
						SafePointer<LObject> unwarped = UnwarpObject(subject);
						SafePointer<LObject> unwarped_type = unwarped->GetType();
						if (static_cast<XType *>(unwarped_type.Inner())->GetCanonicalType() == GetCanonicalType()) { subject->Retain(); return subject; } else {
							SafePointer<ReinterpretComputable> com = new ReinterpretComputable(type, subject);
							return CreateComputable(_ctx, com);
						}
					}
					auto & dest = type->GetTypeReference();
					if (dest.GetReferenceClass() == XI::Module::TypeReference::Class::Reference &&
						dest.GetReferenceDestination().QueryCanonicalName() == GetCanonicalType()) {
						SafePointer<ReferenceComputable> com = new ReferenceComputable(this, subject);
						return CreateComputable(_ctx, com);
					}
					if (_ref.GetPointerDestination().GetReferenceClass() == XI::Module::TypeReference::Class::Class) {
						bool ref = false;
						SafePointer<XType> src_cls = GetElementType();
						SafePointer<XType> dest_cls;
						dest_cls.SetRetain(type);
						if (dest_cls->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Reference) {
							ref = true;
							dest_cls = static_cast<XReference *>(dest_cls.Inner())->GetElementType();
						}
						if (dest_cls->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Pointer) {
							dest_cls = static_cast<XPointer *>(dest_cls.Inner())->GetElementType();
							if (dest_cls->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Class) {
								try {
									SafePointer<XType> src_cls_ref = CreateType(XI::Module::TypeReference::MakeReference(src_cls->GetCanonicalType()), _ctx);
									SafePointer<XType> dest_cls_ref = CreateType(XI::Module::TypeReference::MakeReference(dest_cls->GetCanonicalType()), _ctx);
									SafePointer<XType> dest_cls_ptr = CreateType(XI::Module::TypeReference::MakePointer(dest_cls->GetCanonicalType()), _ctx);
									SafePointer<ReinterpretComputable> reint_com = new ReinterpretComputable(src_cls_ref, subject);
									SafePointer<XComputable> reint = CreateComputable(_ctx, reint_com);
									SafePointer<LObject> casted = src_cls->TransformTo(reint, dest_cls_ref, false);
									SafePointer<LObject> casted_unwarped = UnwarpObject(casted);
									SafePointer<ReinterpretComputable> reint2_com = new ReinterpretComputable(dest_cls_ptr, casted_unwarped);
									SafePointer<XComputable> reint2 = CreateComputable(_ctx, reint2_com);
									if (ref) {
										SafePointer<ReferenceComputable> ref_com = new ReferenceComputable(dest_cls_ptr, reint2);
										return CreateComputable(_ctx, ref_com);
									} else { reint2->Retain(); return reint2; }
								} catch (...) {}
							}
						}
					}
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
			virtual bool IsLocked(void) override { return false; }
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
			virtual void ListMembers(Volumes::Dictionary<string, Class> & list) override {}
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
			virtual bool IsLocked(void) override { return false; }
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
			virtual void ListMembers(Volumes::Dictionary<string, Class> & list) override {}
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
			virtual bool IsLocked(void) override { return false; }
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
		int GetConvertorTraitsImpact(XFunctionOverload * func)
		{
			int impact = 0;
			auto attr = func->GetFlags();
			if (attr & XI::Module::Function::ConvertorExpensive) impact += CastCostExpensive;
			else impact += CastCostUndefined;
			if (attr & XI::Module::Function::ConvertorExpanding) impact += CastQualityExpanding;
			if (attr & XI::Module::Function::ConvertorNarrowing) impact += CastQualityNarrowing;
			if (attr & XI::Module::Function::ConvertorSimilar) impact += CastQualityIdentical;
			return impact;
		}
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
						for (auto & c : conf2) if (dest->GetCanonicalType() == c.GetCanonicalType()) {
							auto conv_over = static_cast<XFunction *>(conv.Inner())->GetOverloadT(fcn, true);
							return CastPriorityConverter + GetConvertorTraitsImpact(conv_over);
						}
					}
				} catch (...) {}
				ObjectArray<XType> conf(0x20);
				SafePointer<XType> pre_dest;
				src->GetTypesConformsTo(conf);
				if (dest->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Class) {
					pre_dest.SetRetain(dest);
				} else if (dest->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Reference) {
					pre_dest = static_cast<XReference *>(dest)->GetElementType();
				} else return CastPriorityNoCast;
				for (auto & c : conf) {
					try {
						SafePointer<LObject> ctor = pre_dest->GetConstructorCast(&c);
						if (ctor->GetClass() == Class::FunctionOverload) {
							return CastPriorityConverter + GetConvertorTraitsImpact(static_cast<XFunctionOverload *>(ctor.Inner()));
						} else return CastPriorityConverter;
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
				try {
					SafePointer<LObject> type = uw->GetType();
					SafePointer<LObject> ctor = type->GetMember(NameConstructor);
					if (ctor->GetClass() != Class::Function) throw InvalidStateException();
					SafePointer<LObject> ctor_inst = static_cast<XFunction *>(ctor.Inner())->SetInstance(uw);
					return ctor_inst->Invoke(argc, argv);
				} catch (...) { if (argc == 1) return InitInstance(instance, argv[0]); else throw; }
			}
		}
		LObject * MoveInstance(LObject * instance, LObject * with_value)
		{
			SafePointer<LObject> uw = UnwarpObject(instance);
			SafePointer<LObject> type = uw->GetType();
			SafePointer<LObject> move_ctor = static_cast<XType *>(type.Inner())->GetConstructorMove();
			SafePointer<LObject> move_ctor_inst = static_cast<XFunctionOverload *>(move_ctor.Inner())->SetInstance(uw);
			return move_ctor_inst->Invoke(1, &with_value);
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
				SafePointer<XMethodOverload> dtor_inst = static_cast<XFunctionOverload *>(dtor.Inner())->SetInstance(uw);
				return dtor_inst->InvokeNoVirtual(0, 0);
			}
		}
	}
}