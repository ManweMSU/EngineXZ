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

		void AddModuleMetadata(Volumes::ObjectDictionary<string, DataBlock> & rsrc, const Volumes::Dictionary<string, string> & meta)
		{
			SafePointer<Storage::Registry> registry = Storage::CreateRegistry();
			SafePointer<Streaming::Stream> stream = new Streaming::MemoryStream(0x1000);
			for (auto & m : meta) {
				registry->CreateValue(m.key, Storage::RegistryValueType::String);
				registry->SetValue(m.key, m.value);
			}
			registry->Save(stream);
			stream->Seek(0, Streaming::Begin);
			SafePointer<DataBlock> data = stream->ReadAll();
			rsrc.Update(L"REG:1", data);
		}
		void AddModuleIcon(Volumes::ObjectDictionary<string, DataBlock> & rsrc, int id, Codec::Image * image)
		{
			SafePointer<DataBlock> icon_dir_data = new DataBlock(1);
			icon_dir_data->SetLength(image->Frames.Length() * sizeof(XI_Icon));
			auto icon_dir = reinterpret_cast<XI_Icon *>(icon_dir_data->GetBuffer());
			int index = 1;
			for (int i = 0; i < image->Frames.Length(); i++) {
				while (rsrc.ElementExists(L"ICO:" + string(index))) index++;
				auto & f = image->Frames[i];
				icon_dir[i].width = f.GetWidth();
				icon_dir[i].height = f.GetHeight();
				icon_dir[i].dpi = f.DpiUsage;
				icon_dir[i].index = index;
				SafePointer<Streaming::Stream> stream = new Streaming::MemoryStream(0x10000);
				Codec::EncodeFrame(stream, &f, Codec::ImageFormatEngine);
				stream->Seek(0, Streaming::Begin);
				SafePointer<DataBlock> data = stream->ReadAll();
				rsrc.Update(L"ICO:" + string(index), data);
				index++;
			}
			rsrc.Update(L"ICD:" + string(id), icon_dir_data);
		}
		Volumes::Dictionary<string, string> * LoadModuleMetadata(const Volumes::ObjectDictionary<string, DataBlock> & rsrc)
		{
			SafePointer< Volumes::Dictionary<string, string> > result = new Volumes::Dictionary<string, string>;
			auto data = rsrc.GetElementByKey(L"REG:1");
			if (data && *data) {
				Streaming::MemoryStream stream((*data)->GetBuffer(), (*data)->Length());
				SafePointer<Storage::Registry> reg = Storage::LoadRegistry(&stream);
				if (reg) for (auto & key : reg->GetValues()) result->Append(key, reg->GetValueString(key));
			}
			result->Retain();
			return result;
		}
		Codec::Image * LoadModuleIcon(const Volumes::ObjectDictionary<string, DataBlock> & rsrc, int id)
		{
			// TODO: IMPLEMENT
			return 0;
		}
		Codec::Frame * LoadModuleIcon(const Volumes::ObjectDictionary<string, DataBlock> & rsrc, int id, Point size)
		{
			// TODO: IMPLEMENT
			return 0;
		}
		Codec::Frame * LoadModuleIcon(const Volumes::ObjectDictionary<string, DataBlock> & rsrc, int id, double dpi)
		{
			// TODO: IMPLEMENT
			return 0;
		}
	}
}