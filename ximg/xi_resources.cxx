﻿#include "xi_resources.h"

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
		
		void LoadIconDirectory(const Volumes::ObjectDictionary<string, DataBlock> & rsrc, int id, const XI_Icon ** ppdir, int * pcount)
		{
			auto icon_data = rsrc.GetElementByKey(L"ICD:" + string(id));
			if (!icon_data || !*icon_data) {
				*ppdir = 0;
				*pcount = 0;
				return;
			}
			*ppdir = reinterpret_cast<const XI_Icon *>((*icon_data)->GetBuffer());
			*pcount = (*icon_data)->Length() / sizeof(XI_Icon);
		}
		Codec::Frame * LoadIcon(const Volumes::ObjectDictionary<string, DataBlock> & rsrc, int id)
		{
			auto icon_data = rsrc.GetElementByKey(L"ICO:" + string(id));
			if (!icon_data || !*icon_data) return 0;
			Streaming::MemoryStream stream((*icon_data)->GetBuffer(), (*icon_data)->Length());
			return Codec::DecodeFrame(&stream);
		}
		int LanguageToIndex(const string & language)
		{
			int result = 0;
			if (language.Length() != 2) throw InvalidArgumentException();
			for (int i = 1; i >= 0; i--) {
				if (language[i] < L'a' || language[i] > L'z') throw InvalidArgumentException();
				result *= (L'z' - L'a' + 1);
				result += (language[i] - L'a');
			}
			return result + 1;
		}
		DataBlock * EncodeDictionary(const Volumes::Dictionary<string, string> & dict)
		{
			SafePointer<DataBlock> data = new DataBlock(0x1000);
			for (auto & v : dict) {
				SafePointer<DataBlock> key = v.key.EncodeSequence(Encoding::UTF16, true);
				SafePointer<DataBlock> value = v.value.EncodeSequence(Encoding::UTF16, true);
				data->Append(*key);
				data->Append(*value);
			}
			data->Retain();
			return data;
		}
		Volumes::Dictionary<string, string> * DecodeDictionary(const DataBlock * data)
		{
			auto utf16 = reinterpret_cast<const uint16 *>(data->GetBuffer());
			auto length = data->Length() / 2;
			SafePointer< Volumes::Dictionary<string, string> > result = new Volumes::Dictionary<string, string>;
			int pos = 0;
			while (pos < length) {
				int base = pos;
				while (pos < length && utf16[pos]) pos++; pos++;
				auto key = string(utf16 + base, -1, Encoding::UTF16);
				base = pos;
				while (pos < length && utf16[pos]) pos++; pos++;
				auto value = string(utf16 + base, -1, Encoding::UTF16);
				result->Append(key, value);
			}
			result->Retain();
			return result;
		}

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
		void AddModuleLocalization(Volumes::ObjectDictionary<string, DataBlock> & rsrc, const string & language, const Volumes::Dictionary<string, string> & localization)
		{
			auto name = L"LOC:" + string(LanguageToIndex(language));
			SafePointer<DataBlock> data = EncodeDictionary(localization);
			rsrc.Append(name, data);
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
		Codec::Frame * LoadModuleIcon(const Volumes::ObjectDictionary<string, DataBlock> & rsrc, int id, Point size)
		{
			const XI_Icon * dir;
			int count;
			LoadIconDirectory(rsrc, id, &dir, &count);
			if (!count) return 0;
			int best_index = 0;
			double best_diff = abs(int(dir[0].width) - size.x) + abs(int(dir[0].height) - size.y);
			for (int i = 1; i < count; i++) {
				double diff = abs(int(dir[i].width) - size.x) + abs(int(dir[i].height) - size.y);
				if (diff < best_diff) { best_diff = diff; best_index = i; }
			}
			return LoadIcon(rsrc, dir[best_index].index);
		}
		Codec::Frame * LoadModuleIcon(const Volumes::ObjectDictionary<string, DataBlock> & rsrc, int id, double dpi)
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
		Volumes::Dictionary<string, string> * LoadModuleLocalization(Volumes::ObjectDictionary<string, DataBlock> & rsrc, const string & language)
		{
			if (!language.Length()) return 0;
			auto name = L"LOC:" + string(LanguageToIndex(language));
			auto data = rsrc.GetObjectByKey(name);
			if (!data) return 0;
			return DecodeDictionary(data);
		}
	}
}