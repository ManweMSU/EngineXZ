#include "xl_var.h"

#include "xl_func.h"
#include "xl_prop.h"

namespace Engine
{
	namespace XL
	{
		LObject * XComputable::GetMember(const string & name)
		{
			try {
				SafePointer<LObject> type = GetType();
				ObjectArray<XType> conf(0x20);
				static_cast<XType *>(type.Inner())->GetTypesConformsTo(conf);
				for (auto & c : conf) try {
					SafePointer<LObject> member = c.GetMember(name);
					SafePointer<LObject> cast = static_cast<XType *>(type.Inner())->TransformTo(this, &c, false);
					if (member->GetClass() == Class::Field) {
						return static_cast<XField *>(member.Inner())->SetInstance(cast);
					} else if (member->GetClass() == Class::Property) {
						return static_cast<XProperty *>(member.Inner())->SetInstance(cast);
					} else if (member->GetClass() == Class::Function) {
						return static_cast<XFunction *>(member.Inner())->SetInstance(cast);
					} else if (member->GetClass() == Class::FunctionOverload) {
						return static_cast<XFunctionOverload *>(member.Inner())->SetInstance(cast);
					} else {
						member->Retain(); return member;
					}
				} catch (...) {}
				throw ObjectHasNoSuchMemberException(this, name);
			} catch (...) { throw ObjectHasNoSuchMemberException(this, name); }
		}
		LObject * XComputable::Invoke(int argc, LObject ** argv)
		{
			SafePointer<LObject> inv = GetMember(OperatorInvoke);
			return inv->Invoke(argc, argv);
		}
		void XComputable::AddMember(const string & name, LObject * child) { throw LException(this); }

		class Literal : public XLiteral
		{
			LContext & _ctx;
			string _name, _path;
			bool _local;
			XI::Module::Literal _data;
		public:
			Literal(LContext & ctx, bool value) : _ctx(ctx), _local(false)
			{
				_data.contents = XI::Module::Literal::Class::Boolean;
				_data.length = 1;
				_data.data_uint64 = 0;
				_data.data_boolean = value;
			}
			Literal(LContext & ctx, uint64 value) : _ctx(ctx), _local(false)
			{
				if (value > 0xFFFFFFFF) {
					_data.contents = XI::Module::Literal::Class::UnsignedInteger;
					_data.length = 8;
				} else if (value > 0x7FFFFFFF) {
					_data.contents = XI::Module::Literal::Class::UnsignedInteger;
					_data.length = 4;
				} else {
					_data.contents = XI::Module::Literal::Class::SignedInteger;
					_data.length = 4;
				}
				_data.data_uint64 = value;
			}
			Literal(LContext & ctx, double value) : _ctx(ctx), _local(false)
			{
				_data.contents = XI::Module::Literal::Class::FloatingPoint;
				_data.length = 8;
				_data.data_double = value;
			}
			Literal(LContext & ctx, const string & value) : _ctx(ctx), _local(false)
			{
				_data.contents = XI::Module::Literal::Class::String;
				_data.length = 0;
				_data.data_uint64 = 0;
				_data.data_string = value;
			}
			Literal(LContext & ctx, const XI::Module::Literal & data) : _ctx(ctx), _local(false), _data(data) {}
			Literal(const Literal & src) : _ctx(src._ctx), _local(false)
			{
				_data.contents = src._data.contents;
				_data.length = src._data.length;
				_data.data_uint64 = src._data.data_uint64;
				_data.data_string = src._data.data_string;
			}
			virtual ~Literal(void) override {}
			virtual Class GetClass(void) override { return Class::Literal; }
			virtual string GetName(void) override { return _name; }
			virtual string GetFullName(void) override { return _path; }
			virtual bool IsDefinedLocally(void) override { return _local; }
			virtual LObject * GetType(void) override
			{
				SafePointer<LObject> type;
				if (_data.contents == XI::Module::Literal::Class::Boolean) {
					if (_data.length == 1) {
						type = _ctx.QueryObject(NameBoolean);
					} else throw ObjectHasNoTypeException(this);
				} else if (_data.contents == XI::Module::Literal::Class::SignedInteger) {
					if (_data.length == 1) {
						type = _ctx.QueryObject(NameInt8);
					} else if (_data.length == 2) {
						type = _ctx.QueryObject(NameInt16);
					} else if (_data.length == 4) {
						type = _ctx.QueryObject(NameInt32);
					} else if (_data.length == 8) {
						type = _ctx.QueryObject(NameInt64);
					} else throw ObjectHasNoTypeException(this);
				} else if (_data.contents == XI::Module::Literal::Class::UnsignedInteger) {
					if (_data.length == 1) {
						type = _ctx.QueryObject(NameUInt8);
					} else if (_data.length == 2) {
						type = _ctx.QueryObject(NameUInt16);
					} else if (_data.length == 4) {
						type = _ctx.QueryObject(NameUInt32);
					} else if (_data.length == 8) {
						type = _ctx.QueryObject(NameUInt64);
					} else throw ObjectHasNoTypeException(this);
				} else if (_data.contents == XI::Module::Literal::Class::FloatingPoint) {
					if (_data.length == 4) {
						type = _ctx.QueryObject(NameFloat32);
					} else if (_data.length == 8) {
						type = _ctx.QueryObject(NameFloat64);
					} else throw ObjectHasNoTypeException(this);
				} else if (_data.contents == XI::Module::Literal::Class::String) {
					SafePointer<LObject> ucs = _ctx.QueryObject(NameUInt32);
					type = _ctx.QueryTypePointer(ucs);
				} else throw ObjectHasNoTypeException(this);
				if (type->GetClass() != Class::Type) throw InvalidStateException();
				type->Retain();
				return type;
			}
			virtual void AddAttribute(const string & key, const string & value) override { if (!_data.attributes.Append(key, value)) throw ObjectMemberRedefinitionException(this, key); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
			{
				if (!_path.Length()) {
					if (_data.contents == XI::Module::Literal::Class::String) {
						SafePointer<DataBlock> data = _data.data_string.EncodeSequence(Encoding::UTF32, true);
						int offset = -1;
						for (int i = 0; i <= func.data.Length() - data->Length(); i++) {
							if (MemoryCompare(func.data.GetBuffer() + i, data->GetBuffer(), data->Length()) == 0) { offset = i; break; }
						}
						if (offset < 0) {
							offset = func.data.Length();
							func.data.Append(*data);
						}
						return MakeAddressOf(XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceData, offset)), XA::TH::MakeSize(data->Length(), 0));
					} else {
						int nwords = func.data.Length() / _data.length;
						int offset = -1;
						for (int i = 0; i < nwords; i++) {
							if (MemoryCompare(func.data.GetBuffer() + _data.length * i, &_data.data_uint64, _data.length) == 0) { offset = _data.length * i; break; }
						}
						if (offset < 0) {
							while (func.data.Length() % _data.length) func.data.Append(0);
							offset = func.data.Length();
							func.data.SetLength(offset + _data.length);
							MemoryCopy(func.data.GetBuffer() + offset, &_data.data_uint64, _data.length);
						}
						return XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceData, offset));
					}
				} else return MakeSymbolReference(func, _path);
			}
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override { if (_local) dest.literals.Append(_path, _data); }
			virtual string ToString(void) const override { if (_path.Length()) return L"literal " + _path; else return L"nameless literal"; }
			virtual bool GetWarpMode(void) override { return true; }
			virtual LObject * UnwarpedGetType(void) override { return GetType(); }
			virtual XA::ExpressionTree UnwarpedEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { return Evaluate(func, error_ctx); }
			virtual void Attach(const string & name, const string & path, bool as_local) override { _name = name; _path = path; _local = as_local; }
			virtual int QueryValueAsInteger(void) override
			{
				if (_data.contents == XI::Module::Literal::Class::UnsignedInteger || _data.contents == XI::Module::Literal::Class::SignedInteger) {
					if (_data.length == 8) return _data.data_sint64;
					else if (_data.length == 4) return _data.data_sint32;
					else if (_data.length == 2) return _data.data_sint16;
					else if (_data.length == 1) return _data.data_sint8;
					else return 0;
				} else throw InvalidStateException();
			}
			virtual string QueryValueAsString(void) override { if (_data.contents == XI::Module::Literal::Class::String) { return _data.data_string; } else throw InvalidStateException(); }
			virtual XLiteral * Clone(void) override { return new Literal(*this); }
			virtual XI::Module::Literal & Expose(void) override { return _data; }
		};
		class StaticComputableProvider : public Object, public IComputableProvider
		{
			SafePointer<XType> _type;
			XA::ExpressionTree _tree;
		public:
			StaticComputableProvider(XType * of_type, const XA::ExpressionTree & with_tree) : _tree(with_tree) { _type.SetRetain(of_type); }
			virtual ~StaticComputableProvider(void) override {}
			virtual Object * ComputableProviderQueryObject(void) override { return this; }
			virtual XType * ComputableGetType(void) override { _type->Retain(); return _type; }
			virtual XA::ExpressionTree ComputableEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { return _tree; }
		};
		class GenericComputable : public XComputable
		{
			LContext & _ctx;
			SafePointer<Object> _object;
			IComputableProvider * _provider;
		public:
			GenericComputable(LContext & ctx, IComputableProvider * provider) : _ctx(ctx), _provider(provider) { _object.SetRetain(_provider->ComputableProviderQueryObject()); }
			virtual ~GenericComputable(void) override {}
			virtual Class GetClass(void) override { return Class::Variable; }
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return true; }
			virtual LObject * GetType(void) override
			{
				SafePointer<XType> primary = _provider->ComputableGetType();
				if (primary->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Reference) {
					return static_cast<XReference *>(primary.Inner())->GetElementType();
				} else { primary->Retain(); return primary; }
			}
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
			{
				SafePointer<XType> primary = _provider->ComputableGetType();
				if (primary->GetCanonicalTypeClass() == XI::Module::TypeReference::Class::Reference) {
					SafePointer<XType> element_type = static_cast<XReference *>(primary.Inner())->GetElementType();
					auto ref_value = _provider->ComputableEvaluate(func, error_ctx);
					return MakeAddressFollow(ref_value, element_type->GetArgumentSpecification().size);
				} else return _provider->ComputableEvaluate(func, error_ctx);
			}
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
			virtual string ToString(void) const override { return L"generic computable"; }
			virtual bool GetWarpMode(void) override { return true; }
			virtual LObject * UnwarpedGetType(void) override { return _provider->ComputableGetType(); }
			virtual XA::ExpressionTree UnwarpedEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { return _provider->ComputableEvaluate(func, error_ctx); }
		};
		class Variable : public XComputable
		{
			LContext & _ctx;
			string _name, _path;
			bool _local;
			string _type_cn;
			XA::ObjectSize _offset;
			Volumes::Dictionary<string, string> _attributes;
		public:
			Variable(LContext & ctx, const string & name, const string & path, bool local, XType * type, XA::ObjectSize offset) :
				_ctx(ctx), _name(name), _path(path), _local(local), _offset(offset) { _type_cn = type->GetCanonicalType(); }
			virtual ~Variable(void) override {}
			virtual Class GetClass(void) override { return Class::Variable; }
			virtual string GetName(void) override { return _name; }
			virtual string GetFullName(void) override { return _path; }
			virtual bool IsDefinedLocally(void) override { return _local; }
			virtual LObject * GetType(void) override
			{
				auto ref = XI::Module::TypeReference(_type_cn);
				if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Reference) {
					SafePointer<XType> type = CreateType(_type_cn, _ctx);
					return static_cast<XReference *>(type.Inner())->GetElementType();
				} else { return CreateType(_type_cn, _ctx); }
			}
			virtual void AddAttribute(const string & key, const string & value) override { if (!_attributes.Append(key, value)) throw ObjectMemberRedefinitionException(this, key); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override
			{
				auto ref = XI::Module::TypeReference(_type_cn);
				if (ref.GetReferenceClass() == XI::Module::TypeReference::Class::Reference) {
					SafePointer<XType> type = CreateType(_type_cn, _ctx);
					SafePointer<XType> element_type = static_cast<XReference *>(type.Inner())->GetElementType();
					return MakeAddressFollow(MakeSymbolReference(func, _path), element_type->GetArgumentSpecification().size);
				} else return MakeSymbolReference(func, _path);
			}
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override
			{
				if (!_local) return;
				SafePointer<XType> type = CreateType(_type_cn, _ctx);
				XI::Module::Variable var;
				var.attributes = _attributes;
				var.type_canonical_name = _type_cn;
				var.offset = _offset;
				var.size = type->GetArgumentSpecification().size;
				dest.variables.Append(_path, var);
			}
			virtual string ToString(void) const override { return L"variable " + _path; }
			virtual bool GetWarpMode(void) override { return true; }
			virtual LObject * UnwarpedGetType(void) override { return CreateType(_type_cn, _ctx); }
			virtual XA::ExpressionTree UnwarpedEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { return MakeSymbolReference(func, _path); }
		};
		class Null : public XComputable
		{
			LContext & _ctx;
			string _type_cn;
		public:
			Null(XType * type) : _ctx(type->GetContext()) { _type_cn = type->GetCanonicalType(); }
			virtual ~Null(void) override {}
			virtual Class GetClass(void) override { return Class::NullLiteral; }
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetType(void) override { return CreateType(_type_cn, _ctx); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { return MakeAddressOf(XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceNull)), XA::TH::MakeSize(0, 1)); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
			virtual string ToString(void) const override { return L"null literal"; }
			virtual bool GetWarpMode(void) override { return true; }
			virtual LObject * UnwarpedGetType(void) override { return GetType(); }
			virtual XA::ExpressionTree UnwarpedEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { return Evaluate(func, error_ctx); }
		};
		class Unwarped : public XComputable
		{
			SafePointer<XComputable> _inner;
		public:
			Unwarped(XComputable * com) { _inner.SetRetain(com); }
			virtual ~Unwarped(void) override {}
			virtual Class GetClass(void) override { return Class::Variable; }
			virtual string GetName(void) override { return _inner->GetName(); }
			virtual string GetFullName(void) override { return _inner->GetFullName(); }
			virtual bool IsDefinedLocally(void) override { return _inner->IsDefinedLocally(); }
			virtual LObject * GetType(void) override { return _inner->UnwarpedGetType(); }
			virtual void AddAttribute(const string & key, const string & value) override { _inner->AddAttribute(key, value); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { return _inner->UnwarpedEvaluate(func, error_ctx); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
			virtual string ToString(void) const override { return L"unwarped " + _inner->ToString(); }
			virtual bool GetWarpMode(void) override { return false; }
			virtual LObject * UnwarpedGetType(void) override { return _inner->UnwarpedGetType(); }
			virtual XA::ExpressionTree UnwarpedEvaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { return _inner->UnwarpedEvaluate(func, error_ctx); }
		};

		XLiteral * CreateLiteral(LContext & ctx, bool value) { return new Literal(ctx, value); }
		XLiteral * CreateLiteral(LContext & ctx, uint64 value) { return new Literal(ctx, value); }
		XLiteral * CreateLiteral(LContext & ctx, double value) { return new Literal(ctx, value); }
		XLiteral * CreateLiteral(LContext & ctx, const string & value) { return new Literal(ctx, value); }
		XLiteral * CreateLiteral(LContext & ctx, const XI::Module::Literal & data) { return new Literal(ctx, data); }
		XComputable * CreateNullLiteral(LContext & ctx)
		{
			SafePointer<XType> type = CreateType(XI::Module::TypeReference::MakePointer(XI::Module::TypeReference::MakeClassReference(NameVoid)), ctx);
			return new Null(type);
		}
		XComputable * CreateComputable(LContext & ctx, XType * of_type, const XA::ExpressionTree & with_tree)
		{
			SafePointer<StaticComputableProvider> provider = new StaticComputableProvider(of_type, with_tree);
			return CreateComputable(ctx, provider);
		}
		XComputable * CreateComputable(LContext & ctx, IComputableProvider * provider) { return new GenericComputable(ctx, provider); }
		XComputable * CreateVariable(LContext & ctx, const string & name, const string & path, bool local, XType * type, XA::ObjectSize offset) { return new Variable(ctx, name, path, local, type, offset); }
		LObject * UnwarpObject(LObject * object)
		{
			if (object->GetClass() != Class::Variable) { object->Retain(); return object; }
			auto com = static_cast<XComputable *>(object);
			if (!com->GetWarpMode()) { com->Retain(); return com; }
			return new Unwarped(com);
		}
	}
}