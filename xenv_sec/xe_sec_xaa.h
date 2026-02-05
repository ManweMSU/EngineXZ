#pragma once

#include "xe_sec_core.h"
#include "../xasm/xa_trans.h"

namespace Engine
{
	namespace XA
	{
		namespace Security
		{
			XE::Security::ICryptographyAcceleration * CreateSyntheticCryptographyAcceleration(IAssemblyTranslator * translator, IExecutableLinker * linker) noexcept;
		}
	}
}