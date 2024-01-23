#pragma once

#include "xe_loader.h"

namespace Engine
{
	namespace XE
	{
		void ActivateWindowsIO(StandardLoader & ldr);
		void ControlWindowsIO(StandardLoader & ldr, const XE::Module * module);
	}
}