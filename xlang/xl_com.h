#pragma once

#include "xl_lal.h"
#include "../ximg/xi_module.h"
#include "../xasm/xa_type_helper.h"

namespace Engine
{
	namespace XL
	{
		XA::ObjectReference MakeReferenceL(XA::Function & func_at, const string & symbol);
		XA::ObjectReference MakeSymbolReferenceL(XA::Function & func_at, const string & path);
		XA::ExpressionTree MakeReference(XA::Function & func_at, const string & symbol);
		XA::ExpressionTree MakeSymbolReference(XA::Function & func_at, const string & path);
		XA::ExpressionTree MakeAddressOf(const XA::ExpressionTree & of, const XA::ObjectSize & entity);
		XA::ExpressionTree MakeAddressFollow(const XA::ExpressionTree & of, const XA::ObjectSize & entity);
		XA::ExpressionTree MakeOffset(const XA::ExpressionTree & obj, const XA::ObjectSize & by, const XA::ObjectSize & obj_size, const XA::ObjectSize & new_size);
		XA::ExpressionTree MakeBlt(const XA::ExpressionTree & dest, const XA::ExpressionTree & src, const XA::ObjectSize & size);

		class XObject : public LObject
		{
		public:
			virtual void EncodeSymbols(XI::Module & dest, Class parent) = 0;
		};
		struct VirtualFunctionDesc
		{
			int vft_index, vf_index;
			XA::ObjectSize vftp_offset, vfp_offset, base_offset;
		};
	}
}