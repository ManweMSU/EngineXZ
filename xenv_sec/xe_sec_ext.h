#pragma once

#include "xe_sec_core.h"
#include "../xenv/xe_loader.h"

namespace Engine
{
	namespace XE
	{
		namespace Security
		{
			enum class SecurityLevel : uint { None = 0, ValidateQuarantine = 1, ValidateAll = 2 };

			ISecurityExtension * CreateStandardSecurityExtension(ITrustProvider * trust, SecurityLevel security_level) noexcept;
			void ActivateSecurityAPI(StandardLoader & ldr);
		}
	}
}