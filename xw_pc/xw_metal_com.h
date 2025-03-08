#pragma once

#include "xw_com.h"

namespace Engine
{
	namespace XW
	{
		struct MetalProfileDesc
		{
			string metal_language_version;
			string target_system;
		};
		PrecompilationStatus MetalCompile(const string & in, const string & out, const string & log, MetalProfileDesc & profile);
	}
}