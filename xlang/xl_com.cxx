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
		XA::ExpressionTree MakeOffset(const XA::ExpressionTree & obj, const XA::ObjectSize & by, const XA::ObjectSize & obj_size, const XA::ObjectSize & new_size)
		{
			auto result = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformAddressOffset, XA::ReferenceFlagInvoke));
			XA::TH::AddTreeInput(result, obj, XA::TH::MakeSpec(obj_size));
			XA::TH::AddTreeInput(result, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLiteral)), XA::TH::MakeSpec(by));
			XA::TH::AddTreeInput(result, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceLiteral)), XA::TH::MakeSpec(1, 0));
			XA::TH::AddTreeOutput(result, XA::TH::MakeSpec(new_size));
			return result;
		}
		XA::ExpressionTree MakeBlt(const XA::ExpressionTree & dest, const XA::ExpressionTree & src, const XA::ObjectSize & size)
		{
			auto result = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformBlockTransfer, XA::ReferenceFlagInvoke));
			XA::TH::AddTreeInput(result, dest, XA::TH::MakeSpec(size));
			XA::TH::AddTreeInput(result, src, XA::TH::MakeSpec(size));
			XA::TH::AddTreeOutput(result, XA::TH::MakeSpec(size));
			return result;
		}
		XA::ExpressionTree MakeConstant(XA::Function & hdlr, const void * pdata, int size, int align)
		{
			int offset = -1;
			for (int i = 0; i <= hdlr.data.Length() - size; i += align) {
				if (pdata) {
					if (MemoryCompare(hdlr.data.GetBuffer() + i, pdata, size) == 0) { offset = i; break; }
				} else {
					bool zero = true;
					for (int j = 0; j < size; j++) if (hdlr.data[i + j]) { zero = false; break; }
					if (zero) { offset = i; break; }
				}
			}
			if (offset < 0) {
				while (hdlr.data.Length() % align) hdlr.data << 0;
				offset = hdlr.data.Length();
				hdlr.data.SetLength(offset + size);
				if (pdata) {
					MemoryCopy(hdlr.data.GetBuffer() + offset, pdata, size);
				} else {
					for (int j = 0; j < size; j++) hdlr.data[offset + j] = 0;
				}
			}
			return XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceData, offset));
		}
	}
}