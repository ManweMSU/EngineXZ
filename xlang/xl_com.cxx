#include "xl_com.h"

namespace Engine
{
	namespace XL
	{
		XA::ObjectReference MakeReferenceL(XA::Function & func_at, const string & symbol)
		{
			int index = -1;
			for (int i = 0; i < func_at.extrefs.Length(); i++) if (func_at.extrefs[i] == symbol) { index = i; break; }
			if (index < 0) {
				index = func_at.extrefs.Length();
				func_at.extrefs << symbol;
			}
			return XA::TH::MakeRef(XA::ReferenceExternal, index);
		}
		XA::ObjectReference MakeSymbolReferenceL(XA::Function & func_at, const string & path)
		{
			auto s = L"S:" + path;
			return MakeReferenceL(func_at, s);
		}
		XA::ExpressionTree MakeReference(XA::Function & func_at, const string & symbol) { return XA::TH::MakeTree(MakeReferenceL(func_at, symbol)); }
		XA::ExpressionTree MakeSymbolReference(XA::Function & func_at, const string & path)
		{
			auto s = L"S:" + path;
			return MakeReference(func_at, s);
		}
		XA::ExpressionTree MakeAddressOf(const XA::ExpressionTree & of, const XA::ObjectSize & entity)
		{
			auto result = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformTakePointer, XA::ReferenceFlagInvoke));
			XA::TH::AddTreeInput(result, of, XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, entity));
			XA::TH::AddTreeOutput(result, XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 1));
			return result;
		}
		XA::ExpressionTree MakeAddressFollow(const XA::ExpressionTree & of, const XA::ObjectSize & entity)
		{
			auto result = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformFollowPointer, XA::ReferenceFlagInvoke));
			XA::TH::AddTreeInput(result, of, XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 1));
			XA::TH::AddTreeOutput(result, XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, entity));
			return result;
		}
	}
}