#pragma once

#include "xc_pty.h"

namespace Engine
{
	namespace XC
	{
		struct TerminalProcessingState
		{
			int encoding_mode;
			CaretStyle caret_revert_style;

			// TODO: IMPLEMENT
		};

		void ProcessTerminalInput(TerminalProcessingState & state, IPseudoTerminal & pty, ConsoleState & console, const char * input, int length, int & length_used);
		void TerminalStateInit(TerminalProcessingState & state);
	}
}