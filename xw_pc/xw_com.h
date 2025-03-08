#pragma once

#include <EngineRuntime.h>

namespace Engine
{
	namespace XW
	{
		enum class PrecompilationStatus {
			Success				= 0x00,
			InvalidInputFile	= 0x01,
			NoCodeForPlatform	= 0x02,
			NoPlatformCompiler	= 0x03,
			CompilationError	= 0x04,
			OutputError			= 0x05,
			InternalError		= 0x06,
		};
	}
}