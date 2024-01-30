#pragma once

#include "xl_lal.h"

namespace Engine
{
	namespace XL
	{
		LObject * CheckInlinePossibility(LObject * function, LObject * instance, LObject * arg1, LObject * arg2);
		bool MayHaveInline(const string & name, const string & cls);
		bool TryForCanonicalInline(XA::Function & func, XA::ExpressionTree & node, LObject * fver);
	}
}