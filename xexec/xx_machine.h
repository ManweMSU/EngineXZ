#pragma once

#include <EngineRuntime.h>

namespace Engine
{
	namespace XX
	{
		enum EnvironmentFlags : uint {
			EnvironmentLoggerNull		= 0x0000,
			EnvironmentLoggerConsole	= 0x0001,
			EnvironmentLoggerFile		= 0x0002,

			EnvironmentConsoleDisable	= 0x0000,
			EnvironmentConsoleSystem	= 0x0008,
			EnvironmentConsoleXC		= 0x0010,

			EnvironmentWindowsDisable	= 0x0000,
			EnvironmentWindowsAllow		= 0x0020,
			EnvironmentWindowsDelegate	= 0x0040,

			EnvironmentLoggerMask		= 0x0003,
			EnvironmentConsoleMask		= 0x0018,
			EnvironmentWindowsMask		= 0x0060,
		};
		struct EnvironmentDesc
		{
			uint flags_allow;
			uint flags_default;
			string xx_init_path;
		};

		int Main(const EnvironmentDesc & desc) noexcept;
	}
}