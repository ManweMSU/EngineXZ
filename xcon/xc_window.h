#pragma once

#include "xc_buffer.h"

namespace Engine
{
	namespace XC
	{
		void WindowSubsystemInitialize(void);
		void CreateConsoleWindow(ConsoleState * state, ConsoleCreateMode & desc, const Box & at);
		void CreateConsoleWindow(ConsoleState * state, ConsoleCreateMode & desc);
		void RunConsoleWindow(void) noexcept;
	}
}