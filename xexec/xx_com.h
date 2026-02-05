#pragma once

#include <EngineRuntime.h>

namespace Engine
{
	namespace XX
	{
		struct SecuritySettings
		{
			bool ValidateTrust;
			bool ValidateTrustForQuarantine;
			string TrustedCertificates;
			string UntrustedCertificates;
		};

		Storage::Registry * LoadConfiguration(const string & from_file);
		void IncludeComponent(Array<string> * module_paths_v, Array<string> * module_paths_w, const string & manifest);
		void IncludeStoreIntegration(Array<string> * module_paths_v, Array<string> * module_paths_w, const string & intfile);
		string LocateEnvironmentConfiguration(const string & intfile);

		void LoadSecuritySettings(SecuritySettings & settings, const Storage::RegistryNode * xe);
		void LoadSecuritySettings(SecuritySettings & settings, const string & xe_conf);
		void UpdateSecuritySettings(const SecuritySettings & settings, const string & xe_conf);
	}
}