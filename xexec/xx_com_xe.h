#pragma once

#include "xx_com.h"
#include "../xenv/xe_loader.h"

namespace Engine
{
	namespace XX
	{
		void IncludeComponent(XE::StandardLoader & loader, const string & manifest);
		void IncludeStoreIntegration(XE::StandardLoader & loader, const string & intfile);
	}
}