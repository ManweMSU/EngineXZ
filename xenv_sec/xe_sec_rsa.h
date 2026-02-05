#pragma once

#include "xe_sec_core.h"

namespace Engine
{
	namespace XE
	{
		namespace Security
		{
			IKey * LoadKeyRSA(const DataBlock * representation, ICryptographyAcceleration * acceleration = 0) noexcept;
			bool CreateKeyRSA(const void * pdata, int plength, const void * qdata, int qlength, uint64 pexp, IKey ** key_prvt, ICryptographyAcceleration * acceleration = 0) noexcept;
		}
	}
}