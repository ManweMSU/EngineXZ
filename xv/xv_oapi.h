#pragma once

#include "../xlang/xl_lal.h"

namespace Engine
{
	namespace XV
	{
		XL::LObject * CreateOperatorNew(XL::LContext & ctx);
		XL::LObject * CreateOperatorConstruct(XL::LContext & ctx);

		XL::LObject * CreateNew(XL::LContext & ctx, XL::LObject * type, int argc, XL::LObject ** argv);
		XL::LObject * CreateConstruct(XL::LContext & ctx, XL::LObject * at, int argc, XL::LObject ** argv);
		XL::LObject * CreateDelete(XL::LObject * object);
		XL::LObject * CreateFree(XL::LObject * object);
		XL::LObject * CreateDestruct(XL::LObject * object);
	}
}