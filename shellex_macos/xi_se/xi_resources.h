#pragma once

#include "xi_module.h"

namespace Engine
{
	namespace XI
	{
		Codec::Image * LoadModuleIcon(const Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, int id);
		Codec::Frame * LoadModuleIcon(const Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, int id, int sx, int sy);
		Codec::Frame * LoadModuleIcon(const Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, int id, double dpi);
	}
}