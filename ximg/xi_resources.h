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

		void AddModuleMetadata(Volumes::ObjectDictionary<string, DataBlock> & rsrc, const Volumes::Dictionary<string, string> & meta);
		void AddModuleIcon(Volumes::ObjectDictionary<string, DataBlock> & rsrc, int id, Codec::Image * image);

		Volumes::Dictionary<string, string> * LoadModuleMetadata(const Volumes::ObjectDictionary<string, DataBlock> & rsrc);
		Codec::Image * LoadModuleIcon(const Volumes::ObjectDictionary<string, DataBlock> & rsrc, int id);
		Codec::Frame * LoadModuleIcon(const Volumes::ObjectDictionary<string, DataBlock> & rsrc, int id, Point size);
		Codec::Frame * LoadModuleIcon(const Volumes::ObjectDictionary<string, DataBlock> & rsrc, int id, double dpi);
	}
}