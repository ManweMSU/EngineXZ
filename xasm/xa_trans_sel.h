#pragma once

#include "xa_trans.h"

namespace Engine
{
	namespace XA
	{
		IAssemblyTranslator * CreatePlatformTranslator(void);
		IAssemblyTranslator * CreatePlatformTranslator(Platform platform, CallingConvention conv);
	}
}