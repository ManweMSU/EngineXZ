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

		Volumes::Dictionary<string, string> * LoadModuleMetadata(const Volumes::ObjectDictionary<uint64, DataBlock> & rsrc);
		Codec::Image * LoadModuleIcon(const Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, int id);
		Codec::Frame * LoadModuleIcon(const Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, int id, int sx, int sy);
		Codec::Frame * LoadModuleIcon(const Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, int id, double dpi);
	}
}