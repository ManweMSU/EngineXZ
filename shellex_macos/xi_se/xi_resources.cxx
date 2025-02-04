#include "xi_resources.h"

namespace Engine
{
	namespace XI
	{
		ENGINE_PACKED_STRUCTURE(XI_Icon)
			uint32 width;
			uint32 height;
			double dpi;
			uint32 index;
		ENGINE_END_PACKED_STRUCTURE
		
		void LoadIconDirectory(const Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, int id, const XI_Icon ** ppdir, int * pcount)
		{
			auto icon_data = rsrc.GetElementByKey(MakeResourceID(L"ICD", id));
			if (!icon_data || !*icon_data) {
				*ppdir = 0;
				*pcount = 0;
				return;
			}
			*ppdir = reinterpret_cast<const XI_Icon *>((*icon_data)->GetBuffer());
			*pcount = (*icon_data)->Length() / sizeof(XI_Icon);
		}
		Codec::Frame * LoadIcon(const Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, int id)
		{
			auto icon_data = rsrc.GetElementByKey(MakeResourceID(L"ICO", id));
			if (!icon_data || !*icon_data) return 0;
			Streaming::MemoryStream stream((*icon_data)->GetBuffer(), (*icon_data)->Length());
			return Codec::DecodeFrame(&stream);
		}

		Codec::Image * LoadModuleIcon(const Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, int id)
		{
			SafePointer<Codec::Image> result = new Codec::Image;
			const XI_Icon * dir;
			int count;
			LoadIconDirectory(rsrc, id, &dir, &count);
			for (int i = 0; i < count; i++) {
				SafePointer<Codec::Frame> frame = LoadIcon(rsrc, dir[i].index);
				result->Frames.Append(frame);
			}
			result->Retain();
			return result;
		}
		Codec::Frame * LoadModuleIcon(const Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, int id, int sx, int sy)
		{
			const XI_Icon * dir;
			int count;
			LoadIconDirectory(rsrc, id, &dir, &count);
			if (!count) return 0;
			int best_index = 0;
			double best_diff = abs(int(dir[0].width) - sx) + abs(int(dir[0].height) - sy);
			for (int i = 1; i < count; i++) {
				double diff = abs(int(dir[i].width) - sx) + abs(int(dir[i].height) - sy);
				if (diff < best_diff) { best_diff = diff; best_index = i; }
			}
			return LoadIcon(rsrc, dir[best_index].index);
		}
		Codec::Frame * LoadModuleIcon(const Volumes::ObjectDictionary<uint64, DataBlock> & rsrc, int id, double dpi)
		{
			const XI_Icon * dir;
			int count;
			LoadIconDirectory(rsrc, id, &dir, &count);
			if (!count) return 0;
			int best_index = 0;
			double best_diff = abs(dir[0].dpi - dpi);
			for (int i = 1; i < count; i++) {
				double diff = abs(dir[i].dpi - dpi);
				if (diff < best_diff) { best_diff = diff; best_index = i; }
			}
			return LoadIcon(rsrc, dir[best_index].index);
		}
	}
}