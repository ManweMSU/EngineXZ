#include "xi_module.h"

namespace Engine
{
	namespace XI
	{
		namespace Format
		{
			namespace DS
			{
				ENGINE_PACKED_STRUCTURE(XI_Header)
					uint64 signature;		// "xximago"
					uint32 format_version;	// 0
					uint32 image_size;
					uint32 image_flags;
					uint32 target_subsystem;
					uint32 info_segment_offset;
					uint32 info_segment_size;
					uint32 data_segment_offset;
					uint32 data_segment_size;
					uint32 rsrc_segment_offset;
					uint32 rsrc_segment_size;
					uint32 module_name_offset;
					uint32 module_assembler_offset;
					uint32 module_assembler_version_major;
					uint32 module_assembler_version_minor;
					uint32 module_assembler_version_subversion;
					uint32 module_assembler_version_build;
					uint32 import_list_offset;
					uint32 import_list_size;
					uint32 symbol_list_offset;
					uint32 symbol_list_size;
					uint32 resource_list_offset;
					uint32 resource_list_size;
				ENGINE_END_PACKED_STRUCTURE
				ENGINE_PACKED_STRUCTURE(XI_Resource)
					uint32 resource_type;
					uint32 resource_id;
					uint32 resource_data_offset;
					uint32 resource_data_size;
				ENGINE_END_PACKED_STRUCTURE
			}

			template<class T> const T * ReadObjects(const DataBlock & data, uint32 at, uint32 number)
			{
				if (at >= uint32(data.Length()) || uint32(data.Length()) - at < uint64(sizeof(T)) * number) throw InvalidFormatException();
				return reinterpret_cast<const T *>(data.GetBuffer() + at);
			}
			string ReadString(const DataBlock & data, uint32 offset)
			{
				if (offset >= uint32(data.Length())) throw InvalidFormatException();
				int length = 0;
				while (offset + length < data.Length() && data[offset + length]) length++;
				return string(data.GetBuffer() + offset, length, Encoding::UTF8);
			}
			
			void ValidateHeader(const DS::XI_Header & hdr) { if (MemoryCompare(&hdr.signature, "xximago", 8) || hdr.format_version || hdr.image_size < sizeof(hdr)) throw InvalidFormatException(); }
			void DecodeResources(Module & dest, const DS::XI_Header & hdr, const DataBlock & rsrc)
			{
				auto rtable = ReadObjects<DS::XI_Resource>(rsrc, hdr.resource_list_offset, hdr.resource_list_size);
				for (uint i = 0; i < hdr.resource_list_size; i++) {
					auto & ri = rtable[i];
					uint64 key = (uint64(ri.resource_id) << 32) | uint64(ri.resource_type);
					if (ri.resource_data_offset > uint32(rsrc.Length()) || ri.resource_data_size > uint32(rsrc.Length())) throw InvalidArgumentException();
					if (ri.resource_data_offset + ri.resource_data_size > uint32(rsrc.Length())) throw InvalidArgumentException();
					SafePointer<DataBlock> rd = new DataBlock(1);
					rd->SetLength(ri.resource_data_size);
					MemoryCopy(rd->GetBuffer(), rsrc.GetBuffer() + ri.resource_data_offset, ri.resource_data_size);
					dest.resources.Append(key, rd);
				}
			}
			void RestoreModule(Module & dest, Streaming::Stream * src)
			{
				DS::XI_Header hdr;
				src->Seek(0, Streaming::Begin);
				src->Read(&hdr, sizeof(hdr));
				ValidateHeader(hdr);
				SafePointer<DataBlock> info, rsrc;
				if (hdr.info_segment_size > 0x7FFFFFFF || hdr.data_segment_size > 0x7FFFFFFF || hdr.rsrc_segment_size > 0x7FFFFFFF) throw InvalidFormatException();
				if (hdr.info_segment_size) {
					src->Seek(hdr.info_segment_offset, Streaming::Begin);
					info = src->ReadBlock(hdr.info_segment_size);
				}
				if (hdr.rsrc_segment_size) {
					src->Seek(hdr.rsrc_segment_offset, Streaming::Begin);
					rsrc = src->ReadBlock(hdr.rsrc_segment_size);
				}
				if (info) {
					dest.module_import_name = ReadString(*info, hdr.module_name_offset);
					dest.assembler_name = ReadString(*info, hdr.module_assembler_offset);
					dest.assembler_version.major = hdr.module_assembler_version_major;
					dest.assembler_version.minor = hdr.module_assembler_version_minor;
					dest.assembler_version.subver = hdr.module_assembler_version_subversion;
					dest.assembler_version.build = hdr.module_assembler_version_build;
				} else {
					dest.assembler_version.major = dest.assembler_version.minor = 0;
					dest.assembler_version.subver = dest.assembler_version.build = 0;
				}
				if (rsrc) DecodeResources(dest, hdr, *rsrc);
			}
		}

		Module::Module(void)
		{
			module_import_name = L"module";
			assembler_name = L"XI";
			assembler_version.major = assembler_version.minor = 0;
			assembler_version.subver = assembler_version.build = 0;
		}
		Module::Module(Streaming::Stream * source) { Format::RestoreModule(*this, source); }
		Module::~Module(void) {}
		uint64 MakeResourceID(const string & type, int id)
		{
			uint64 result = uint64(id) << 32;
			type.Fragment(0, 4).Encode(&result, Encoding::ANSI, false);
			return result;
		}
		void ReadResourceID(uint64 rid, string & type, int & id)
		{
			type = string(&rid, 4, Encoding::ANSI);
			id = rid >> 32;
		}
	}
}