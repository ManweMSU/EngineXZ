#pragma once

#include "xi_module.h"
#include "../xasm/xa_trans.h"

namespace Engine
{
	namespace XI
	{
		struct PretranslateDesc
		{
			Platform arch;
			XA::CallingConvention conv;
		};
		void PretranslateFunction(Module::Function & subject, const PretranslateDesc * sys_list, int sys_list_length);
		void PretranslateModule(Module & subject, const PretranslateDesc * sys_list, int sys_list_length);
	}
}