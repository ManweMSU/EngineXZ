#pragma once

#include "xi_module.h"

namespace Engine
{
	namespace XI
	{
		// Classes auxiliorum usi
		// ICD - collectorium imaginum (icon), ICD#1 - icon moduli
		// ICO - imago/icon
		// LOC - localizationes
		// REG - configurationes, REG#1 - meta data
		// VER - data versionum, VER#1
		// XC  - configuratio consolatorii, XC#1 - configuratio, XC#2 - imago
		// XDL - liber dynamicus internus

		constexpr const widechar * MetadataKeyModuleName = L"NomenModuli";
		constexpr const widechar * MetadataKeyCompanyName = L"CreatorModuli";
		constexpr const widechar * MetadataKeyCopyright = L"IuraExempli";
		constexpr const widechar * MetadataKeyVersion = L"Versio";

		struct AssemblyVersionReplacement { uint32 MustBe; uint32 Mask; };
		struct AssemblyVersionInformation
		{
			uint32 ThisModuleVersion;
			Array<AssemblyVersionReplacement> ReplacesVersions = Array<AssemblyVersionReplacement>(1);
			Volumes::Dictionary<string, uint32> ModuleVersionsNeeded;
		};

		void AddModuleMetadata(Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, const Volumes::Dictionary<string, string> & meta);
		void AddModuleIcon(Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, int id, Codec::Image * image);
		void AddModuleLocalization(Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, const string & language, const Volumes::Dictionary<string, string> & localization);
		void AddModuleVersionInformation(Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, const AssemblyVersionInformation & info);

		Volumes::Dictionary<string, string> * LoadModuleMetadata(const Volumes::ObjectDictionary<uint64, DataBlock> & rsrc);
		Codec::Image * LoadModuleIcon(const Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, int id);
		Codec::Frame * LoadModuleIcon(const Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, int id, Point size);
		Codec::Frame * LoadModuleIcon(const Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, int id, double dpi);
		Volumes::Dictionary<string, string> * LoadModuleLocalization(Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, const string & language);
		bool LoadModuleVersionInformation(const Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, AssemblyVersionInformation & info);
	}
}