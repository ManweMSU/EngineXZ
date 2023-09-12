#include "xl_base.h"

#include "xl_types.h"

namespace Engine
{
	namespace XL
	{
		class NullObject : public XObject
		{
		public:
			NullObject(void) {}
			virtual ~NullObject(void) override {}
			virtual Class GetClass(void) override { return Class::Null; }
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetType(void) override { throw ObjectHasNoTypeException(this); }
			virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
			virtual void ListMembers(Volumes::Dictionary<string, Class> & list) override {}
			virtual LObject * Invoke(int argc, LObject ** argv) override { throw ObjectHasNoSuchOverloadException(this, argc, argv); }
			virtual void ListInvokations(LObject * first, Volumes::List<InvokationDesc> & list) override {}
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { return XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceNull)); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
			virtual string ToString(void) const override { return L"null"; }
		};
		class Namespace : public XObject
		{
			string _name, _path;
			LContext & _ctx;
			Volumes::ObjectDictionary<string, LObject> _members;
		public:
			Namespace(const string & name, const string & path, LContext & ctx) : _name(name), _path(path), _ctx(ctx) {}
			virtual ~Namespace(void) override {}
			virtual Class GetClass(void) override { return Class::Namespace; }
			virtual string GetName(void) override { return _name; }
			virtual string GetFullName(void) override { return _path; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetType(void) override { throw ObjectHasNoTypeException(this); }
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
			virtual void ListMembers(Volumes::Dictionary<string, Class> & list) override
			{
				for (auto & m : _members) {
					if (m.value->GetClass() == Class::Alias) {
						auto alias = static_cast<XAlias *>(m.value.Inner());
						if (alias->IsTypeAlias()) list.Append(m.key, Class::Type); else {
							SafePointer<XL::LObject> object = _ctx.QueryObject(alias->GetDestination());
							list.Append(m.key, object->GetClass());
						}
					} else list.Append(m.key, m.value->GetClass());
				}
			}
			virtual LObject * Invoke(int argc, LObject ** argv) override { throw ObjectHasNoSuchOverloadException(this, argc, argv); }
			virtual void ListInvokations(LObject * first, Volumes::List<InvokationDesc> & list) override {}
			virtual void AddMember(const string & name, LObject * child) override { if (!_members.Append(name, child)) throw ObjectMemberRedefinitionException(this, name); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override { for (auto & m : _members) static_cast<XObject *>(m.value.Inner())->EncodeSymbols(dest, Class::Namespace); }
			virtual string ToString(void) const override { if (_path.Length()) return L"namespace " + _path; else return L"root namespace"; }
		};
		class Scope : public XObject
		{
			Volumes::ObjectDictionary<string, LObject> _members;
		public:
			Scope(void) {}
			virtual ~Scope(void) override {}
			virtual Class GetClass(void) override { return Class::Scope; }
			virtual string GetName(void) override { return L""; }
			virtual string GetFullName(void) override { return L""; }
			virtual bool IsDefinedLocally(void) override { return false; }
			virtual LObject * GetType(void) override { throw ObjectHasNoTypeException(this); }
			virtual LObject * GetMember(const string & name) override
			{
				auto member = _members[name];
				if (member) { member->Retain(); return member; } else throw ObjectHasNoSuchMemberException(this, name);
			}
			virtual void ListMembers(Volumes::Dictionary<string, Class> & list) override { for (auto & m : _members) list.Append(m.key, m.value->GetClass()); }
			virtual LObject * Invoke(int argc, LObject ** argv) override { throw ObjectHasNoSuchOverloadException(this, argc, argv); }
			virtual void ListInvokations(LObject * first, Volumes::List<InvokationDesc> & list) override {}
			virtual void AddMember(const string & name, LObject * child) override { if (!_members.Append(name, child)) throw ObjectMemberRedefinitionException(this, name); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override {}
			virtual string ToString(void) const override { return L"scope"; }
		};
		class Alias : public XAlias
		{
			string _name, _path, _dest;
			bool _local;
		public:
			Alias(const string & name, const string & path, const string & to, bool cn_alias, bool local) : _name(name), _path(path), _local(local) { if (cn_alias) _dest = L"T:" + to; else _dest = L"S:" + to; }
			Alias(const string & name, const string & path, const string & to, bool local) : _name(name), _path(path), _dest(to), _local(local) {}
			virtual ~Alias(void) override {}
			virtual Class GetClass(void) override { return Class::Alias; }
			virtual string GetName(void) override { return _name; }
			virtual string GetFullName(void) override { return _path; }
			virtual bool IsDefinedLocally(void) override { return _local; }
			virtual LObject * GetType(void) override { throw ObjectHasNoTypeException(this); }
			virtual LObject * GetMember(const string & name) override { throw ObjectHasNoSuchMemberException(this, name); }
			virtual void ListMembers(Volumes::Dictionary<string, Class> & list) override {}
			virtual LObject * Invoke(int argc, LObject ** argv) override { throw ObjectHasNoSuchOverloadException(this, argc, argv); }
			virtual void ListInvokations(LObject * first, Volumes::List<InvokationDesc> & list) override {}
			virtual void AddMember(const string & name, LObject * child) override { throw LException(this); }
			virtual void AddAttribute(const string & key, const string & value) override { throw ObjectHasNoAttributesException(this); }
			virtual XA::ExpressionTree Evaluate(XA::Function & func, XA::ExpressionTree * error_ctx) override { throw ObjectIsNotEvaluatableException(this); }
			virtual void EncodeSymbols(XI::Module & dest, Class parent) override { if (_local) dest.aliases.Append(_path, _dest); }
			virtual string ToString(void) const override { return L"alias " + _path + L" --> " + _dest; }
			virtual bool IsTypeAlias(void) const override { return _dest[0] == L'T'; }
			virtual string GetDestination(void) const override { return _dest.Fragment(2, -1); }
		};

		XObject * CreateNull(void) { return new NullObject; }
		XObject * CreateNamespace(const string & name, const string & path, LContext & ctx) { return new Namespace(name, path, ctx); }
		XObject * CreateScope(void) { return new Scope; }
		XAlias * CreateAlias(const string & name, const string & path, const string & to, bool cn_alias, bool local) { return new Alias(name, path, to, cn_alias, local); }
		XAlias * CreateAliasRaw(const string & name, const string & path, const string & to, bool local) { return new Alias(name, path, to, local); }
	}
}