﻿#pragma once

#include "xi_module.h"
#include "../xasm/xa_trans_sel.h"

namespace Engine
{
	namespace XI
	{
		struct PretranslateDesc
		{
			Platform arch;
			XA::Environment osenv;
		};
		void PretranslateFunction(Module::Function & subject, const PretranslateDesc * sys_list, int sys_list_length);
		void PretranslateModule(Module & subject, const PretranslateDesc * sys_list, int sys_list_length);
	}
}