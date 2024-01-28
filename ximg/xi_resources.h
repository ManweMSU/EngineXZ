#pragma once

#include "xi_module.h"

namespace Engine
{
	namespace XI
	{
		constexpr const widechar * MetadataKeyModuleName = L"NomenModuli";
		constexpr const widechar * MetadataKeyCompanyName = L"CreatorModuli";
		constexpr const widechar * MetadataKeyCopyright = L"IuraExempli";
		constexpr const widechar * MetadataKeyVersion = L"Versio";

		void AddModuleMetadata(Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, const Volumes::Dictionary<string, string> & meta);
		void AddModuleIcon(Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, int id, Codec::Image * image);
		void AddModuleLocalization(Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, const string & language, const Volumes::Dictionary<string, string> & localization);

		Volumes::Dictionary<string, string> * LoadModuleMetadata(const Volumes::ObjectDictionary<uint64, DataBlock> & rsrc);
		Codec::Image * LoadModuleIcon(const Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, int id);
		Codec::Frame * LoadModuleIcon(const Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, int id, Point size);
		Codec::Frame * LoadModuleIcon(const Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, int id, double dpi);
		Volumes::Dictionary<string, string> * LoadModuleLocalization(Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, const string & language);
	}
}