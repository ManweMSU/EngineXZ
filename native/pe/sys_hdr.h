#pragma once

#include <EngineRuntime.h>

namespace Engine
{
	namespace XN
	{
		constexpr int dos_page_size			= 512;
		constexpr int dos_paragraph_size	= 16;
		constexpr int windows_block_size	= 0x200;
		constexpr int windows_page_size		= 0x1000;

		constexpr uint16 pe_machine_unknown	= 0x0000;
		constexpr uint16 pe_machine_80386	= 0x014C;
		constexpr uint16 pe_machine_x86_64	= 0x8664;
		constexpr uint16 pe_machine_arm64	= 0xAA64;

		constexpr uint16 pe_flag_executable	= 0x0002;
		constexpr uint16 pe_flag_largeaware	= 0x0020;
		constexpr uint16 pe_flag_machine32	= 0x0100;
		constexpr uint16 pe_flag_nodebug	= 0x0200;
		constexpr uint16 pe_flag_runswap	= 0x0C00;
		constexpr uint16 pe_flag_library	= 0x2000;

		constexpr uint16 pe_subsys_unknown	= 0x0000;
		constexpr uint16 pe_subsys_windows	= 0x0002;
		constexpr uint16 pe_subsys_console	= 0x0003;
		constexpr uint16 pe_subsys_efi		= 0x000A;

		constexpr uint16 pe_library_rnd_awr	= 0x0020;
		constexpr uint16 pe_library_dynbase	= 0x0040;
		constexpr uint16 pe_library_nx_awr	= 0x0100;
		constexpr uint16 pe_library_no_seh	= 0x0400;
		constexpr uint16 pe_library_trm_awr	= 0x8000;

		constexpr uint32 pe_section_code	= 0x00000020;
		constexpr uint32 pe_section_idata	= 0x00000040;
		constexpr uint32 pe_section_udata	= 0x00000080;
		constexpr uint32 pe_section_execute	= 0x20000000;
		constexpr uint32 pe_section_read	= 0x40000000;
		constexpr uint32 pe_section_write	= 0x80000000;

		constexpr uint32 pe_reloc_no		= 0x0000;
		constexpr uint32 pe_reloc_32		= 0x3000;
		constexpr uint32 pe_reloc_64		= 0xA000;

		constexpr uint32 pe_rt_icon			= 0x03;
		constexpr uint32 pe_rt_rcdata		= 0x0A;
		constexpr uint32 pe_rt_version		= 0x10;
		constexpr uint32 pe_rt_icon_group	= 0x0E;
		constexpr uint32 pe_rt_manifest		= 0x18;
		constexpr uint32 pe_unicode_cp		= 1200;

		struct MZHeader
		{
			uint16 signature; // MZ
			uint16 last_page_size;
			uint16 num_pages;
			uint16 num_relocs;
			uint16 header_paragraph_size;
			uint16 min_paragraphs_required;
			uint16 max_paragraphs_required;
			uint16 set_ss;
			uint16 set_sp;
			uint16 checksum;
			uint16 set_ip;
			uint16 set_cs;
			uint16 reloc_table_offset;
			uint16 overlay;
			uint16 reserved_0;
			uint16 reserved_1;
			uint16 reserved_2;
			uint16 reserved_3;
			uint16 oem_ident;
			uint16 oem_info;
			uint8 reserved_4[20];
			uint32 pe_header_offset;
		};
		struct MZRelocation
		{
			uint16 offset;
			uint16 segment;
		};

		struct PEHeader
		{
			uint32 signature; // PE\0\0
			uint16 machine;
			uint16 num_sections;
			uint32 creation_date;
			uint32 unused_0;
			uint32 unused_1;
			uint16 exhdr_size;
			uint16 flags;
		};
		struct PEHeaderEx32
		{
			uint16 signature; // 0x010B
			uint8 linker_version_major;
			uint8 linker_version_minor;
			uint32 code_size;
			uint32 data_size;
			uint32 data_unset_size;
			uint32 entry_point_rva;
			uint32 code_base_rva;
			uint32 data_base_rva;
			uint32 image_base; // 0x00400000
			uint32 section_alignment; // 0x1000
			uint32 file_alignment; // 0x200
			uint16 min_os_version_major;
			uint16 min_os_version_minor;
			uint16 image_version_major;
			uint16 image_version_minor;
			uint16 subsystem_version_major;
			uint16 subsystem_version_minor;
			uint32 reserved_0;
			uint32 image_memory_size;
			uint32 headers_size;
			uint32 checksum;
			uint16 subsystem;
			uint16 dynamic_library_flags;
			uint32 desired_stack_size;
			uint32 initial_stack_size;
			uint32 desired_heap_size;
			uint32 initial_heap_size;
			uint32 reserved_1;
			uint32 num_tables; // 6
		};
		struct PEHeaderEx64
		{
			uint16 signature; // 0x020B
			uint8 linker_version_major;
			uint8 linker_version_minor;
			uint32 code_size;
			uint32 data_size;
			uint32 data_unset_size;
			uint32 entry_point_rva;
			uint32 code_base_rva;
			uint64 image_base; // 0x00400000
			uint32 section_alignment; // 0x1000
			uint32 file_alignment; // 0x200
			uint16 min_os_version_major;
			uint16 min_os_version_minor;
			uint16 image_version_major;
			uint16 image_version_minor;
			uint16 subsystem_version_major;
			uint16 subsystem_version_minor;
			uint32 reserved_0;
			uint32 image_memory_size;
			uint32 headers_size;
			uint32 checksum;
			uint16 subsystem;
			uint16 dynamic_library_flags;
			uint64 desired_stack_size;
			uint64 initial_stack_size;
			uint64 desired_heap_size;
			uint64 initial_heap_size;
			uint32 reserved_1;
			uint32 num_tables; // 6
		};
		struct PETableHeader
		{
			uint32 table_rva;
			uint32 table_size;
		};
		struct PEHeaderFull32
		{
			PEHeader hdr;
			PEHeaderEx32 hdr_ex;
			PETableHeader export_table;
			PETableHeader import_table;
			PETableHeader resource_table;
			PETableHeader exception_table;
			PETableHeader certificate_table;
			PETableHeader relocation_table;
		};
		struct PEHeaderFull64
		{
			PEHeader hdr;
			PEHeaderEx64 hdr_ex;
			PETableHeader export_table;
			PETableHeader import_table;
			PETableHeader resource_table;
			PETableHeader exception_table;
			PETableHeader certificate_table;
			PETableHeader relocation_table;
		};
		struct PESection
		{
			char name[8];
			uint32 memory_size;
			uint32 memory_rva;
			uint32 file_size;
			uint32 file_offset;
			uint32 unused_0;
			uint32 unused_1;
			uint16 unused_2;
			uint16 unused_3;
			uint32 flags;
		};
		struct PESuperHeader32
		{
			PEHeaderFull32 hdr;
			PESection section[5];
		};
		struct PESuperHeader64
		{
			PEHeaderFull64 hdr;
			PESection section[5];
		};

		struct PERelocationBlock
		{
			uint32 page_rva;
			uint32 block_size;
		};
		struct PEImportTableEntry
		{
			uint32 local_import_table_rva;
			uint32 creation_date;
			uint32 forwarder_index;
			uint32 dll_name_rva;
			uint32 address_table_rva;
		};
		struct PEResourceTableHeader
		{
			uint32 unused;
			uint32 creation_date;
			uint16 version_major;
			uint16 version_minor;
			uint16 num_named_entries;
			uint16 num_indexed_entries;
		};
		struct PEResourceTableItem
		{
			union {
				uint32 name_rsa;
				uint32 index;
			};
			uint32 item_rsa;
		};
		struct PEResourceItem
		{
			uint32 data_rva;
			uint32 size;
			uint32 codepage;
			uint32 reserved;
		};

		struct VIBlockHeader
		{
			uint16 length;
			uint16 value_length;
			uint16 type;
		};
		struct VICommonInfo
		{
			uint32 signature; // 0xFEEF04BD
			uint32 struct_version;
			uint16 file_version_minor;
			uint16 file_version_major;
			uint16 file_version_build;
			uint16 file_version_secondary;
			uint16 product_version_minor;
			uint16 product_version_major;
			uint16 product_version_build;
			uint16 product_version_secondary;
			uint32 flags_mask;
			uint32 flags;
			uint32 os;
			uint32 type;
			uint32 subtype;
			uint32 date_ms;
			uint32 date_ls;
		};

		ENGINE_PACKED_STRUCTURE(ICOHeader)
			uint16 reserved;
			uint16 type;
			uint16 count;
		ENGINE_END_PACKED_STRUCTURE
		ENGINE_PACKED_STRUCTURE(ICOFileItem)
			uint8 width;
			uint8 height;
			uint8 colors;
			uint8 reserved;
			uint16 planes;
			uint16 bpp;
			uint32 size;
			uint32 offset;
		ENGINE_END_PACKED_STRUCTURE
		ENGINE_PACKED_STRUCTURE(ICOResourceItem)
			uint8 width;
			uint8 height;
			uint8 colors;
			uint8 reserved;
			uint16 planes;
			uint16 bpp;
			uint32 size;
			uint16 index;
		ENGINE_END_PACKED_STRUCTURE
	}
}