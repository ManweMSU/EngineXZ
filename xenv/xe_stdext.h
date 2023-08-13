#pragma once

#include "xe_loader.h"

namespace Engine
{
	namespace XE
	{
		IAPIExtension * CreateFPU(void);
		IAPIExtension * CreateMMU(void);
		IAPIExtension * CreateSPU(void);
		IAPIExtension * CreateMiscUnit(void);
	}
}