#pragma once

#include "xc_buffer.h"

namespace Engine
{
	namespace XC
	{
		class IAttachIO : public Object
		{
		public:
			virtual bool Communicate(const string & init_title, const string & init_preset) noexcept = 0;
			virtual bool CreateConsole(ConsoleState ** state, ConsoleCreateMode * mode) noexcept = 0;
			virtual bool LaunchService(void) noexcept = 0;
		};

		IAttachIO * CreateAttachIO(const string & io);
	}
}