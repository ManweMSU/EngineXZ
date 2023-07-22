#pragma once

#include "xl_var.h"
#include "xl_types.h"

namespace Engine
{
	namespace XL
	{
		XLiteral * ProcessLiteralTransform(LContext & ctx, const string & op, XLiteral * a, XLiteral * b);
		XLiteral * ProcessLiteralConvert(LContext & ctx, XLiteral * value, XType * type);
	}
}