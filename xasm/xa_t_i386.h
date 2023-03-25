#pragma once

#include "xa_trans.h"

namespace Engine
{
	namespace XA
	{
		IAssemblyTranslator * CreateTranslatorX86i386(CallingConvention conv);
	}
}