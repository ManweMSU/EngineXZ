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

		class XObject : public LObject
		{
		public:
			virtual void EncodeSymbols(XI::Module & dest, Class parent) = 0;
		};
	}
}