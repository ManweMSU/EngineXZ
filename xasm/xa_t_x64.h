#pragma once

#include "xa_trans.h"

namespace Engine
{
	namespace XA
	{
		IAssemblyTranslator * CreateTranslatorX64(Environment osenv);
	}
}