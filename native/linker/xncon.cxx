#include <EngineRuntime.h>

#include "../../ximg/xi_function.h"
#include "../../ximg/xi_resources.h"
#include "../../xasm/xa_trans.h"
#include "asm_trans.h"
#include "sys_hdr.h"
#include "lnk_x64.h"

using namespace Engine;
using namespace Engine::IO;
using namespace Engine::IO::ConsoleControl;
using namespace Engine::Streaming;
using namespace Engine::Storage;

Console console;

enum class DOSEnvironment { Unknown, BIN286, COM, MZ };

struct {
	bool silent = false, nologo = false;
	string input_xx, input_metadata, input_mz, output_pe;
	Array<string> path_xo = Array<string>(0x10);
	Volumes::Dictionary<uint, string> resources;
	Platform arch = Platform::Unknown;
	XA::Environment osenv = XA::Environment::Unknown;
	DOSEnvironment dosenv = DOSEnvironment::Unknown;
} state;

void ProcessCommandLine(void)
{
	SafePointer< Array<string> > args = GetCommandLine();
	for (int i = 1; i < args->Length(); i++) {
		auto & arg = args->ElementAt(i);
		if (arg[0] == L':' || arg[0] == L'-') {
			for (int j = 1; j < arg.Length(); j++) {
				if (arg[j] == L'N') {
					state.nologo = true;
				} else if (arg[j] == L'S') {
					state.silent = true;
				} else if (arg[j] == L'a') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << L"Argumentum non sufficit." << TextColorDefault() << LineFeed();
						throw Exception();
					}
					if (state.input_metadata.Length()) {
						console << TextColor(12) << L"Redefinitio semitae." << TextColorDefault() << LineFeed();
						throw Exception();
					}
					state.input_metadata = ExpandPath(args->ElementAt(i));
				} else if (arg[j] == L'b') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << L"Argumentum non sufficit." << TextColorDefault() << LineFeed();
						throw Exception();
					}
					if (state.input_mz.Length()) {
						console << TextColor(12) << L"Redefinitio semitae." << TextColorDefault() << LineFeed();
						throw Exception();
					}
					state.input_mz = ExpandPath(args->ElementAt(i));
				} else if (arg[j] == L'l') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << L"Argumentum non sufficit." << TextColorDefault() << LineFeed();
						throw Exception();
					}
					state.path_xo << ExpandPath(args->ElementAt(i));
				} else if (arg[j] == L'm') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << L"Argumentum non sufficit." << TextColorDefault() << LineFeed();
						throw Exception();
					}
					if (state.arch != Platform::Unknown || state.dosenv != DOSEnvironment::Unknown) {
						console << TextColor(12) << L"Redefinitio machinae." << TextColorDefault() << LineFeed();
						throw Exception();
					}
					if (args->ElementAt(i) == L"win-x86") {
						state.arch = Platform::X86;
						state.osenv = XA::Environment::Windows;
					} else if (args->ElementAt(i) == L"win-x64") {
						state.arch = Platform::X64;
						state.osenv = XA::Environment::Windows;
					} else if (args->ElementAt(i) == L"win-arm64") {
						state.arch = Platform::ARM64;
						state.osenv = XA::Environment::Windows;
					} else if (args->ElementAt(i) == L"efi-x64") {
						state.arch = Platform::X64;
						state.osenv = XA::Environment::EFI;
					} else if (args->ElementAt(i) == L"80286") {
						state.dosenv = DOSEnvironment::BIN286;
					} else if (args->ElementAt(i) == L"dos-com") {
						state.dosenv = DOSEnvironment::COM;
					} else if (args->ElementAt(i) == L"dos-exe") {
						state.dosenv = DOSEnvironment::MZ;
					} else {
						console << TextColor(12) << L"Machina ignota." << TextColorDefault() << LineFeed();
						throw Exception();
					}
				} else if (arg[j] == L'o') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << L"Argumentum non sufficit." << TextColorDefault() << LineFeed();
						throw Exception();
					}
					if (state.output_pe.Length()) {
						console << TextColor(12) << L"Redefinitio semitae." << TextColorDefault() << LineFeed();
						throw Exception();
					}
					state.output_pe = ExpandPath(args->ElementAt(i));
				} else if (arg[j] == L'x') {
					i += 2; if (i >= args->Length()) {
						console << TextColor(12) << L"Argumentum non sufficit." << TextColorDefault() << LineFeed();
						throw Exception();
					}
					try {
						if (!state.resources.Append(args->ElementAt(i - 1).ToUInt32(), ExpandPath(args->ElementAt(i)))) {
							console << TextColor(12) << L"Redefinitio auxilii." << TextColorDefault() << LineFeed();
							throw Exception();
						}
					} catch (...) {
						console << TextColor(12) << L"Argumentum falsum." << TextColorDefault() << LineFeed();
						throw Exception();
					}
				} else {
					console << TextColor(12) << L"Optio ignota." << TextColorDefault() << LineFeed();
					throw Exception();
				}
			}
		} else {
			if (state.input_xx.Length()) {
				console << TextColor(12) << L"Redefinitio moduli." << TextColorDefault() << LineFeed();
				throw Exception();
			}
			state.input_xx = ExpandPath(arg);
		}
	}
	if (state.input_xx.Length() && state.arch == Platform::Unknown && state.dosenv == DOSEnvironment::Unknown) {
		console << TextColor(12) << L"Machina nulla." << TextColorDefault() << LineFeed();
		throw Exception();
	}
}

int Main(void)
{
	try {
		Codec::InitializeDefaultCodecs();
		ProcessCommandLine();
	} catch (...) { return 0x40; }
	if (!state.nologo && !state.silent) {
		console << L"XN Contextor" << LineFeedSequence;
		console << L"(C) Engine Software. 2025" << LineFeedSequence;
		console << L"Versio " << ENGINE_VI_APPVERSION << L"." << LineFeedSequence << LineFeedSequence;
	}
	if (state.input_xx.Length()) {
		try {
			state.path_xo << Path::GetDirectory(state.input_xx);
			if (!state.output_pe.Length()) {
				auto path = Path::GetDirectory(state.input_xx);
				auto name = Path::GetFileNameWithoutExtension(state.input_xx);
				if (state.osenv == XA::Environment::Windows) {
					state.output_pe = ExpandPath(path + L"/" + name + L".exe");
				} else if (state.osenv == XA::Environment::EFI) {
					state.output_pe = ExpandPath(path + L"/" + name + L".efi");
				} else if (state.dosenv == DOSEnvironment::COM) {
					state.output_pe = ExpandPath(path + L"/" + name + L".com");
				} else if (state.dosenv == DOSEnvironment::MZ) {
					state.output_pe = ExpandPath(path + L"/" + name + L".exe");
				} else {
					state.output_pe = ExpandPath(path + L"/" + name + L".bin");
				}
			}
			if (state.dosenv != DOSEnvironment::Unknown) {
				string assembly;
				try {
					SafePointer<Stream> input = new FileStream(state.input_xx, AccessRead, OpenExisting);
					SafePointer<TextReader> rdr = new TextReader(input);
					assembly = rdr->ReadAll();
				} catch (FileAccessException & e) {
					if (!state.silent) console << TextColor(12) << FormatString(L"Error legendi limae: \"%0\".", state.input_xx) << TextColorDefault() << LineFeed();
					return e.code;
				}
				XN::AssemblyOutput output;
				XN::AssemblyError error;
				XN::AssemblyState istate;
				if (state.dosenv == DOSEnvironment::BIN286) {
					istate.initial_segment = 0;
					istate.initial_offset = 0;
				} else if (state.dosenv == DOSEnvironment::COM) {
					istate.initial_segment = 0;
					istate.initial_offset = 0x100;
				} else if (state.dosenv == DOSEnvironment::MZ) {
					istate.initial_segment = 0;
					istate.initial_offset = 0;
				}
				XN::Assembly(assembly, istate, output, error);
				if (error.code != XN::AssemblyErrorCode::Success) {
					if (!state.silent) {
						string desc;
						if (error.code == XN::AssemblyErrorCode::InvalidTokenInput) desc = L"verbum falsum";
						else if (error.code == XN::AssemblyErrorCode::AnotherTokenExpected) desc = L"verbum inopinatum";
						else if (error.code == XN::AssemblyErrorCode::UnknownInstruction) desc = L"iussum ignotum";
						else if (error.code == XN::AssemblyErrorCode::UnknownRegister) desc = L"celula ignota";
						else if (error.code == XN::AssemblyErrorCode::UnknownTranslatorHint) desc = L"suggestio ignota";
						else if (error.code == XN::AssemblyErrorCode::UnknownLabel) desc = FormatString(L"titulus ignotus \"%0\"", error.object);
						else if (error.code == XN::AssemblyErrorCode::IllegalAddressMode) desc = L"modus alloquendi falsus";
						else if (error.code == XN::AssemblyErrorCode::IllegalEncoding) desc = L"forma iussi falsa";
						else if (error.code == XN::AssemblyErrorCode::SegmentOverflow) desc = L"alluvio segmenti";
						else desc = L"ignotus";
						console << TextColor(12) << FormatString(L"Error translatoris #%1: %0.", desc, uint(error.code)) << TextColorDefault() << LineFeed();
						if (error.position != -1) {
							int linenum = 1;
							int orgnum = -1;
							for (int i = 0; i < int(error.position); i++) {
								if (assembly[i] == L'\n') { linenum++; orgnum = -1; }
								else if (assembly[i] != L' ' && assembly[i] != L'\t' && assembly[i] != L'\r' && orgnum < 0) orgnum = i;
							}
							if (orgnum < 0) orgnum = error.position;
							int end = orgnum;
							while (end < assembly.Length() && assembly[end] != L'\n' && assembly[end] != L'\r') end++;
							console << TextColor(12) << FormatString(L"Linea #%0.", linenum) << TextColorDefault() << LineFeed();
							console << TextColor(12) << assembly.Fragment(orgnum, end - orgnum);
							if (error.position == assembly.Length()) console << TextColor(15) << TextBackground(4) << L"[FL]" << TextBackgroundDefault();
							console << TextColorDefault() << LineFeed();
							if (error.position - orgnum > 0) console << string(L' ', error.position - orgnum);
							console << TextColor(14) << L"^" << TextColorDefault() << LineFeed();
						}
					}
					return uint(error.code);
				}
				if (state.dosenv == DOSEnvironment::BIN286) {
					if (output.required_stack || output.required_memory || output.desired_memory) {
						if (!state.silent) console << TextColor(14) << L"Ammonitum: Definitiones memoriae neglectae sunt in modo puro." << TextColorDefault() << LineFeed();
					}
					if (output.entry_point_offset || output.entry_point_segment_relocate) {
						if (!state.silent) console << TextColor(14) << L"Ammonitum: Initus neglectus est in modo puro." << TextColorDefault() << LineFeed();
					}
					try {
						SafePointer<Stream> stream = new FileStream(state.output_pe, AccessWrite, CreateAlways);
						stream->WriteArray(&output.data);
					} catch (FileAccessException & e) {
						if (!state.silent) console << TextColor(12) << FormatString(L"Error scribendi limae: \"%0\".", state.output_pe) << TextColorDefault() << LineFeed();
						return e.code;
					}
				} else if (state.dosenv == DOSEnvironment::COM) {
					if (output.data.Length() > 0xFF00) {
						if (!state.silent) console << TextColor(12) << L"Longitudo COM limae nimia est." << TextColorDefault() << LineFeed();
						return 1;
					}
					if (output.relocate_segments.Length()) {
						if (!state.silent) console << TextColor(12) << L"COM lima relocationem sustinet non." << TextColorDefault() << LineFeed();
						return 1;
					}
					if (output.entry_point_offset != 0x100 || output.entry_point_segment_relocate) {
						if (!state.silent) console << TextColor(12) << L"COM lima initum non-zeroticum sustinet non." << TextColorDefault() << LineFeed();
						return 1;
					}
					if (output.required_stack || output.required_memory || output.desired_memory) {
						if (!state.silent) console << TextColor(14) << L"Ammonitum: Definitiones memoriae neglectae sunt in COM lima." << TextColorDefault() << LineFeed();
					}
					try {
						SafePointer<Stream> stream = new FileStream(state.output_pe, AccessWrite, CreateAlways);
						stream->WriteArray(&output.data);
					} catch (FileAccessException & e) {
						if (!state.silent) console << TextColor(12) << FormatString(L"Error scribendi limae: \"%0\".", state.output_pe) << TextColorDefault() << LineFeed();
						return e.code;
					}
				} else if (state.dosenv == DOSEnvironment::MZ) {
					XN::MZHeader hdr;
					Array<XN::MZRelocation> reloc(1);
					ZeroMemory(&hdr, sizeof(hdr));
					while (output.data.Length() % XN::dos_paragraph_size) output.data << 0;
					reloc.SetLength(output.relocate_segments.Length());
					for (int i = 0; i < output.relocate_segments.Length(); i++) {
						reloc[i].offset = output.relocate_segments[i] & 0xFFFF;
						reloc[i].segment = (output.relocate_segments[i] & 0xF0000) >> 4;
					}
					MemoryCopy(&hdr.signature, "MZ", 2);
					hdr.num_relocs = reloc.Length();
					while (reloc.Length() % 4) reloc << XN::MZRelocation { 0, 0 };
					hdr.header_paragraph_size = (sizeof(hdr) + reloc.Length() * sizeof(XN::MZRelocation)) / XN::dos_paragraph_size;
					auto full_size = hdr.header_paragraph_size * XN::dos_paragraph_size + output.data.Length();
					hdr.num_pages = (full_size + XN::dos_page_size - 1) / XN::dos_page_size;
					hdr.last_page_size = (full_size - (hdr.num_pages - 1) * XN::dos_page_size) % XN::dos_page_size;
					auto mem_min_alloc = (output.required_memory + XN::dos_paragraph_size - 1) / XN::dos_paragraph_size;
					auto mem_max_alloc = (output.desired_memory + XN::dos_paragraph_size - 1) / XN::dos_paragraph_size;
					auto stack_alloc = (output.required_stack + XN::dos_paragraph_size - 1) / XN::dos_paragraph_size;
					if (mem_max_alloc < mem_min_alloc) mem_max_alloc = mem_min_alloc;
					hdr.min_paragraphs_required = mem_min_alloc + stack_alloc;
					hdr.max_paragraphs_required = mem_max_alloc + stack_alloc;
					hdr.set_ss = output.data.Length() / XN::dos_paragraph_size + mem_min_alloc;
					hdr.set_sp = stack_alloc * XN::dos_paragraph_size;
					hdr.set_ip = output.entry_point_offset;
					hdr.set_cs = output.entry_point_segment_relocate;
					hdr.reloc_table_offset = sizeof(hdr);
					if (!output.required_stack) {
						if (!state.silent) console << TextColor(14) << L"Ammonitum: Magnitudo acervi definita non est." << TextColorDefault() << LineFeed();
					}
					try {
						SafePointer<Stream> stream = new FileStream(state.output_pe, AccessWrite, CreateAlways);
						stream->Write(&hdr, sizeof(hdr));
						stream->Write(reloc.GetBuffer(), reloc.Length() * sizeof(XN::MZRelocation));
						stream->WriteArray(&output.data);
					} catch (FileAccessException & e) {
						if (!state.silent) console << TextColor(12) << FormatString(L"Error scribendi limae: \"%0\".", state.output_pe) << TextColorDefault() << LineFeed();
						return e.code;
					}
				}
			} else {
				if (!state.input_mz.Length()) {
					if (!state.silent) console << TextColor(12) << L"MZ modulus bacilis nullus." << TextColorDefault() << LineFeed();
					return 3;
				}
				SafePointer<DataBlock> stub, super_header;
				try {
					XN::MZHeader hdr;
					SafePointer<Stream> stream = new FileStream(state.input_mz, AccessRead, OpenExisting);
					stream->Read(&hdr, sizeof(hdr));
					if (MemoryCompare(&hdr.signature, "MZ", 2) != 0) throw InvalidFormatException();
					if (hdr.num_relocs && hdr.reloc_table_offset < sizeof(hdr)) throw InvalidFormatException();
					if (hdr.header_paragraph_size * XN::dos_paragraph_size < sizeof(hdr)) throw InvalidFormatException();
					int image_size;
					image_size = (hdr.num_pages - 1) * XN::dos_page_size + (hdr.last_page_size ? hdr.last_page_size : XN::dos_page_size);
					if (image_size < sizeof(hdr)) throw InvalidFormatException();
					stream->Seek(0, Begin);
					stub = stream->ReadBlock(image_size);
					while (stub->Length() % XN::windows_block_size) stub->Append(0);
					auto & refhdr = *reinterpret_cast<XN::MZHeader *>(stub->GetBuffer());
					refhdr.checksum = 0;
					refhdr.reserved_0 = refhdr.reserved_1 = refhdr.reserved_2 = refhdr.reserved_3 = 0;
					refhdr.oem_ident = refhdr.oem_info = 0;
					ZeroMemory(&refhdr.reserved_4, sizeof(refhdr.reserved_4));
					refhdr.pe_header_offset = stub->Length();
				} catch (...) { if (!state.silent) console << TextColor(12) << FormatString(L"Error legendi limae: \"%0\".", state.input_mz) << TextColorDefault() << LineFeed(); return 2; }
				super_header = new DataBlock(XN::windows_block_size);
				super_header->SetLength((XN::PEFormatHeaderSize(state.arch) + XN::windows_block_size - 1) / XN::windows_block_size * XN::windows_block_size);
				ZeroMemory(super_header->GetBuffer(), super_header->Length());
				XN::LinkerInput input;
				XN::LinkerOutput output;
				XN::LinkerError error;
				input.path_xx = state.input_xx;
				input.path_xo = state.path_xo;
				input.arch = state.arch;
				input.osenv = state.osenv;
				input.base_rva = XN::Align(stub->Length() + super_header->Length(), XN::windows_page_size);
				input.path_attr = state.input_metadata.Length() ? state.input_metadata : state.input_xx;
				input.resources = state.resources;
				XN::Link(input, output, error);
				if (error.code != XN::LinkerErrorCode::Success) {
					if (!state.silent) {
						string desc;
						if (error.code == XN::LinkerErrorCode::MainModuleLoadingError) desc = L"error onerandi moduli primi";
						else if (error.code == XN::LinkerErrorCode::ResourceLoadingError) desc = L"error onerandi auxilii";
						else if (error.code == XN::LinkerErrorCode::ModuleNotFound) desc = L"modulus importatus ignotus";
						else if (error.code == XN::LinkerErrorCode::LocalImportInModule) desc = L"importatio localis prohibita est";
						else if (error.code == XN::LinkerErrorCode::UnknownSymbolReference) desc = L"symbolus ignotus";
						else if (error.code == XN::LinkerErrorCode::IllegalSymbolReference) desc = L"symbolus prohibitus";
						else if (error.code == XN::LinkerErrorCode::NoMainEntryPoint) desc = L"introitus nullus";
						else if (error.code == XN::LinkerErrorCode::NoRootEntryPoint) desc = L"introitus radicalis nullus";
						else if (error.code == XN::LinkerErrorCode::WrongABI) desc = L"protocollum falsum";
						else desc = L"ignotus";
						console << TextColor(12) << FormatString(L"Error contexendi #%1: %0 - %2.", desc, uint(error.code), error.object) << TextColorDefault() << LineFeed();
					}
					return uint(error.code);
				}
				if (state.arch == Platform::X86 || state.arch == Platform::ARM) {
					auto & shdr = *reinterpret_cast<XN::PESuperHeader32 *>(super_header->GetBuffer());
					MemoryCopy(&shdr.hdr.hdr.signature, "PE\0\0", 4);
					if (state.arch == Platform::X86) shdr.hdr.hdr.machine = XN::pe_machine_80386;
					shdr.hdr.hdr.num_sections = 0;
					shdr.hdr.hdr.exhdr_size = sizeof(XN::PEHeaderFull32) - sizeof(XN::PEHeader);
					shdr.hdr.hdr.flags = XN::pe_flag_executable | XN::pe_flag_machine32;
					shdr.hdr.hdr_ex.signature = 0x010B;
					shdr.hdr.hdr_ex.linker_version_major = ENGINE_VI_VERSIONMAJOR;
					shdr.hdr.hdr_ex.linker_version_minor = ENGINE_VI_VERSIONMINOR;
					shdr.hdr.hdr_ex.code_size = output.cs ? output.cs->Length() : 0;
					shdr.hdr.hdr_ex.data_size = 0;
					if (output.ds) shdr.hdr.hdr_ex.data_size += output.ds->Length();
					if (output.ls) shdr.hdr.hdr_ex.data_size += output.ls->Length();
					if (output.is) shdr.hdr.hdr_ex.data_size += output.is->Length();
					if (output.rs) shdr.hdr.hdr_ex.data_size += output.rs->Length();
					shdr.hdr.hdr_ex.data_unset_size = 0;
					shdr.hdr.hdr_ex.entry_point_rva = output.entry_rva;
					shdr.hdr.hdr_ex.code_base_rva = output.cs_rva;
					shdr.hdr.hdr_ex.data_base_rva = output.ds_rva;
					shdr.hdr.hdr_ex.image_base = output.image_base;
					shdr.hdr.hdr_ex.section_alignment = XN::windows_page_size;
					shdr.hdr.hdr_ex.file_alignment = XN::windows_block_size;
					if (output.minimal_os_major >= 0 && output.minimal_os_minor >= 0) {
						shdr.hdr.hdr_ex.min_os_version_major = output.minimal_os_major;
						shdr.hdr.hdr_ex.min_os_version_minor = output.minimal_os_minor;
					} else {
						shdr.hdr.hdr_ex.min_os_version_major = state.osenv == XA::Environment::Windows ? 5 : 0;
						shdr.hdr.hdr_ex.min_os_version_minor = state.osenv == XA::Environment::Windows ? 1 : 0;
					}
					shdr.hdr.hdr_ex.subsystem_version_major = shdr.hdr.hdr_ex.min_os_version_major;
					shdr.hdr.hdr_ex.subsystem_version_minor = shdr.hdr.hdr_ex.min_os_version_minor;
					shdr.hdr.hdr_ex.image_memory_size = input.base_rva;
					if (output.cs) shdr.hdr.hdr_ex.image_memory_size += XN::Align(output.cs->Length(), XN::windows_page_size);
					if (output.ds) shdr.hdr.hdr_ex.image_memory_size += XN::Align(output.ds->Length(), XN::windows_page_size);
					if (output.ls) shdr.hdr.hdr_ex.image_memory_size += XN::Align(output.ls->Length(), XN::windows_page_size);
					if (output.is) shdr.hdr.hdr_ex.image_memory_size += XN::Align(output.is->Length(), XN::windows_page_size);
					if (output.rs) shdr.hdr.hdr_ex.image_memory_size += XN::Align(output.rs->Length(), XN::windows_page_size);
					shdr.hdr.hdr_ex.headers_size = stub->Length() + super_header->Length();
					if (state.osenv == XA::Environment::Windows) {
						if (output.subsystem == XI::Module::ExecutionSubsystem::ConsoleUI) shdr.hdr.hdr_ex.subsystem = XN::pe_subsys_console;
						else shdr.hdr.hdr_ex.subsystem = XN::pe_subsys_windows;
					} else if (state.osenv == XA::Environment::EFI) {
						shdr.hdr.hdr_ex.subsystem = XN::pe_subsys_efi;
					}
					shdr.hdr.hdr_ex.dynamic_library_flags = XN::pe_library_dynbase | XN::pe_library_nx_awr | XN::pe_library_trm_awr;
					shdr.hdr.hdr_ex.desired_stack_size = output.desired_stack;
					shdr.hdr.hdr_ex.initial_stack_size = output.required_stack;
					shdr.hdr.hdr_ex.desired_heap_size = output.desired_heap;
					shdr.hdr.hdr_ex.initial_heap_size = output.required_heap;
					shdr.hdr.hdr_ex.num_tables = (sizeof(XN::PEHeaderFull32) - sizeof(XN::PEHeader) - sizeof(XN::PEHeaderEx32)) / sizeof(XN::PETableHeader);
					if (output.is) {
						shdr.hdr.import_table.table_rva = output.is_rva;
						shdr.hdr.import_table.table_size = output.is_size;
					}
					if (output.rs) {
						shdr.hdr.resource_table.table_rva = output.rs_rva;
						shdr.hdr.resource_table.table_size = output.rs_size;
					}
					if (output.ls) {
						shdr.hdr.relocation_table.table_rva = output.ls_rva;
						shdr.hdr.relocation_table.table_size = output.ls_size;
					}
					XN::AddSection(shdr, "CODEX\0\0\0",  output.cs, output.cs_rva, XN::pe_section_code | XN::pe_section_read | XN::pe_section_execute);
					XN::AddSection(shdr, "DATA\0\0\0\0", output.ds, output.ds_rva, XN::pe_section_idata | XN::pe_section_read | XN::pe_section_write);
					XN::AddSection(shdr, "RELOCA\0\0",   output.ls, output.ls_rva, XN::pe_section_idata | XN::pe_section_read);
					XN::AddSection(shdr, "IMPORT\0\0",   output.is, output.is_rva, XN::pe_section_idata | XN::pe_section_read);
					XN::AddSection(shdr, "AUXIL\0\0\0",  output.rs, output.rs_rva, XN::pe_section_idata | XN::pe_section_read);
				} else if (state.arch == Platform::X64 || state.arch == Platform::ARM64) {
					auto & shdr = *reinterpret_cast<XN::PESuperHeader64 *>(super_header->GetBuffer());
					MemoryCopy(&shdr.hdr.hdr.signature, "PE\0\0", 4);
					if (state.arch == Platform::X64) shdr.hdr.hdr.machine = XN::pe_machine_x86_64;
					else if (state.arch == Platform::ARM64) shdr.hdr.hdr.machine = XN::pe_machine_arm64;
					shdr.hdr.hdr.num_sections = 0;
					shdr.hdr.hdr.exhdr_size = sizeof(XN::PEHeaderFull64) - sizeof(XN::PEHeader);
					shdr.hdr.hdr.flags = XN::pe_flag_executable | XN::pe_flag_largeaware;
					shdr.hdr.hdr_ex.signature = 0x020B;
					shdr.hdr.hdr_ex.linker_version_major = ENGINE_VI_VERSIONMAJOR;
					shdr.hdr.hdr_ex.linker_version_minor = ENGINE_VI_VERSIONMINOR;
					shdr.hdr.hdr_ex.code_size = output.cs ? output.cs->Length() : 0;
					shdr.hdr.hdr_ex.data_size = 0;
					if (output.ds) shdr.hdr.hdr_ex.data_size += output.ds->Length();
					if (output.ls) shdr.hdr.hdr_ex.data_size += output.ls->Length();
					if (output.is) shdr.hdr.hdr_ex.data_size += output.is->Length();
					if (output.rs) shdr.hdr.hdr_ex.data_size += output.rs->Length();
					shdr.hdr.hdr_ex.data_unset_size = 0;
					shdr.hdr.hdr_ex.entry_point_rva = output.entry_rva;
					shdr.hdr.hdr_ex.code_base_rva = output.cs_rva;
					shdr.hdr.hdr_ex.image_base = output.image_base;
					shdr.hdr.hdr_ex.section_alignment = XN::windows_page_size;
					shdr.hdr.hdr_ex.file_alignment = XN::windows_block_size;
					if (output.minimal_os_major >= 0 && output.minimal_os_minor >= 0) {
						shdr.hdr.hdr_ex.min_os_version_major = output.minimal_os_major;
						shdr.hdr.hdr_ex.min_os_version_minor = output.minimal_os_minor;
					} else {
						if (state.osenv == XA::Environment::Windows) {
							if (state.arch == Platform::X64) {
								shdr.hdr.hdr_ex.min_os_version_major = 6;
								shdr.hdr.hdr_ex.min_os_version_minor = 0;
							} else if (state.arch == Platform::ARM64) {
								shdr.hdr.hdr_ex.min_os_version_major = 6;
								shdr.hdr.hdr_ex.min_os_version_minor = 2;
							}
						}
					}
					shdr.hdr.hdr_ex.subsystem_version_major = shdr.hdr.hdr_ex.min_os_version_major;
					shdr.hdr.hdr_ex.subsystem_version_minor = shdr.hdr.hdr_ex.min_os_version_minor;
					shdr.hdr.hdr_ex.image_memory_size = input.base_rva;
					if (output.cs) shdr.hdr.hdr_ex.image_memory_size += XN::Align(output.cs->Length(), XN::windows_page_size);
					if (output.ds) shdr.hdr.hdr_ex.image_memory_size += XN::Align(output.ds->Length(), XN::windows_page_size);
					if (output.ls) shdr.hdr.hdr_ex.image_memory_size += XN::Align(output.ls->Length(), XN::windows_page_size);
					if (output.is) shdr.hdr.hdr_ex.image_memory_size += XN::Align(output.is->Length(), XN::windows_page_size);
					if (output.rs) shdr.hdr.hdr_ex.image_memory_size += XN::Align(output.rs->Length(), XN::windows_page_size);
					shdr.hdr.hdr_ex.headers_size = stub->Length() + super_header->Length();
					if (state.osenv == XA::Environment::Windows) {
						if (output.subsystem == XI::Module::ExecutionSubsystem::ConsoleUI) shdr.hdr.hdr_ex.subsystem = XN::pe_subsys_console;
						else shdr.hdr.hdr_ex.subsystem = XN::pe_subsys_windows;
					} else if (state.osenv == XA::Environment::EFI) {
						shdr.hdr.hdr_ex.subsystem = XN::pe_subsys_efi;
					}
					shdr.hdr.hdr_ex.dynamic_library_flags = XN::pe_library_dynbase | XN::pe_library_nx_awr | XN::pe_library_rnd_awr | XN::pe_library_trm_awr | XN::pe_library_no_seh;
					shdr.hdr.hdr_ex.desired_stack_size = output.desired_stack;
					shdr.hdr.hdr_ex.initial_stack_size = output.required_stack;
					shdr.hdr.hdr_ex.desired_heap_size = output.desired_heap;
					shdr.hdr.hdr_ex.initial_heap_size = output.required_heap;
					shdr.hdr.hdr_ex.num_tables = (sizeof(XN::PEHeaderFull64) - sizeof(XN::PEHeader) - sizeof(XN::PEHeaderEx64)) / sizeof(XN::PETableHeader);
					if (output.is) {
						shdr.hdr.import_table.table_rva = output.is_rva;
						shdr.hdr.import_table.table_size = output.is_size;
					}
					if (output.rs) {
						shdr.hdr.resource_table.table_rva = output.rs_rva;
						shdr.hdr.resource_table.table_size = output.rs_size;
					}
					if (output.ls) {
						shdr.hdr.relocation_table.table_rva = output.ls_rva;
						shdr.hdr.relocation_table.table_size = output.ls_size;
					}
					XN::AddSection(shdr, "CODEX\0\0\0",  output.cs, output.cs_rva, XN::pe_section_code | XN::pe_section_read | XN::pe_section_execute);
					XN::AddSection(shdr, "DATA\0\0\0\0", output.ds, output.ds_rva, XN::pe_section_idata | XN::pe_section_read | XN::pe_section_write);
					XN::AddSection(shdr, "RELOCA\0\0",   output.ls, output.ls_rva, XN::pe_section_idata | XN::pe_section_read);
					XN::AddSection(shdr, "IMPORT\0\0",   output.is, output.is_rva, XN::pe_section_idata | XN::pe_section_read);
					XN::AddSection(shdr, "AUXIL\0\0\0",  output.rs, output.rs_rva, XN::pe_section_idata | XN::pe_section_read);
				} else throw InvalidStateException();
				try {
					SafePointer<Stream> stream = new FileStream(state.output_pe, AccessWrite, CreateAlways);
					stream->WriteArray(stub);
					stream->WriteArray(super_header);
					if (output.cs) stream->WriteArray(output.cs);
					if (output.ds) stream->WriteArray(output.ds);
					if (output.ls) stream->WriteArray(output.ls);
					if (output.is) stream->WriteArray(output.is);
					if (output.rs) stream->WriteArray(output.rs);
				} catch (...) { if (!state.silent) console << TextColor(12) << FormatString(L"Error scribendi limae: \"%0\".", state.output_pe) << TextColorDefault() << LineFeed(); return 2; }
			}
		} catch (...) { return 0x3F; }
		return 0;
	} else {
		if (!state.silent) try {
			console << L"Compositura imperati:" << LineFeedSequence;
			console << L"  xncon <modulus.xx> -NSablmox" << LineFeedSequence;
			console << LineFeedSequence;
			console << L"<modulus.xx> - XI aut ASM modulus, objectum contexendi," << LineFeedSequence;
			console << L"-N           - proloquium nullum," << LineFeedSequence;
			console << L"-S           - modus silens," << LineFeedSequence;
			console << L"-a           - semitam XI moduli attributorum definit," << LineFeedSequence;
			console << L"-b           - semitam MZ moduli basis definit," << LineFeedSequence;
			console << L"-l           - semitam librorum addit," << LineFeedSequence;
			console << L"-m           - machinam destinationis definit, valores possibiles:" << LineFeedSequence;
			console << L"                  win-x86   - Windows in i386," << LineFeedSequence;
			console << L"                  win-x64   - Windows in x86-64," << LineFeedSequence;
			console << L"                  win-arm64 - Windows in ARM64," << LineFeedSequence;
			console << L"                  efi-x64   - UEFI in x86-64," << LineFeedSequence;
			console << L"                  dos-com   - DOS in 80286," << LineFeedSequence;
			console << L"                  dos-exe   - DOS in 80286," << LineFeedSequence;
			console << L"                  80286     - 80286 purus," << LineFeedSequence;
			console << L"-o           - semitam proventus definit," << LineFeedSequence;
			console << L"-x           - data auxilii addit (-x <numerus> <lima datorum>)." << LineFeedSequence;
		} catch (...) {}
		return 0;
	}
}