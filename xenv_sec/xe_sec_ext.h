#pragma once

#include "xe_sec_core.h"
#include "../xenv/xe_loader.h"

namespace Engine
{
	namespace XE
	{
		namespace Security
		{
			ISecurityExtension * CreateStandardSecurityExtension(ITrustProvider * trust, bool needs_trusted_chain) noexcept;
		}
	}
}