#pragma once

#include "../xenv/xe_loader.h"

namespace Engine
{
	namespace XX
	{
		Storage::Registry * LoadConfiguration(const string & from_file);
		void IncludeComponent(XE::StandardLoader & loader, const string & manifest);
		void IncludeComponent(Array<string> & module_paths, const string & manifest);
		void IncludeStoreIntegration(XE::StandardLoader & loader, const string & intfile);
		void IncludeStoreIntegration(Array<string> & module_paths, const string & intfile);
	}
}