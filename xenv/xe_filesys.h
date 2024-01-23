#pragma once

#include "xe_loader.h"

namespace Engine
{
	namespace XE
	{
		void ActivateFileIO(StandardLoader & ldr, const string & exec_path, const string * argv, int argc);
		SafePointer< Array<string> > GetCommandLineFileIO(StandardLoader & ldr);
	}
}