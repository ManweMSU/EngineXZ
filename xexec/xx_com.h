#pragma once

#include <EngineRuntime.h>

namespace Engine
{
	namespace XX
	{
		Storage::Registry * LoadConfiguration(const string & from_file);
		void IncludeComponent(Array<string> * module_paths_v, Array<string> * module_paths_w, const string & manifest);
		void IncludeStoreIntegration(Array<string> * module_paths_v, Array<string> * module_paths_w, const string & intfile);
	}
}