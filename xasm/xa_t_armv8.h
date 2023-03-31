#pragma once

#include "xa_trans.h"

namespace Engine
{
	namespace XA
	{
		IAssemblyTranslator * CreateTranslatorARMv8(CallingConvention conv);
	}
}