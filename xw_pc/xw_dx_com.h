#pragma once

#include "xw_com.h"

namespace Engine
{
	namespace XW
	{
		struct DXProfileDesc
		{
			string vertex_shader_profile;
			string pixel_shader_profile;
		};
		PrecompilationStatus DXCompile(const string & in, const string & out, const string & log, DXProfileDesc & profile);
	}
}