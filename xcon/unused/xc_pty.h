#pragma once

#include "xc_buffer.h"

namespace Engine
{
	namespace XC
	{
		class IPseudoTerminal : public Object
		{
		public:
			virtual bool CreateAttachedProcess(const string & exec, const string * argv, int argc) noexcept = 0;
			virtual void WriteOutputStream(const char * data, int length) noexcept = 0;
		};

		IPseudoTerminal * CreatePseudoTerminal(ConsoleState * console);
	}
}