#pragma once

#include "xw_com.h"

namespace Engine
{
	namespace XW
	{
		struct VulkanProfileDesc
		{
			uint vulkan_version_major;
			uint vulkan_version_minor;
			uint spirv_version_major;
			uint spirv_version_minor;
			uint glsl_version_major;
			uint glsl_version_minor;
		};
		PrecompilationStatus VulkanCompile(const string & in, const string & out, const string & log, VulkanProfileDesc & profile);
	}
}