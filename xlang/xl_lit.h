#pragma once

#include "xl_var.h"

namespace Engine
{
	namespace XL
	{
		XLiteral * ProcessLiteralTransform(LContext & ctx, const string & op, XLiteral * a, XLiteral * b);

		// TODO: ADD CONVERTER TRANSFORM
	}
}