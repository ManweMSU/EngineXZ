#include "lnk_x64.h"
#include "enc_i286.h"
#include "sys_hdr.h"
#include "../../xasm/xa_trans_sel.h"
#include "../../xasm/xa_type_helper.h"

namespace Engine
{
	namespace XN
	{
		struct LinkerCodeFragment
		{
			bool referenced;
			XA::TranslatedFunction func;
			uint code_rva, data_rva, cs_offset;
		};
		struct ResourceItem
		{
			SafePointer<DataBlock> data;
			Volumes::Dictionary<string, ResourceItem> items_named;
			Volumes::Dictionary<uint32, ResourceItem> items_indexed;
		};
		class LinkerFunctionLoader : public XI::IFunctionLoader
		{
			SafePointer<XA::IAssemblyTranslator> _trans;
			Volumes::Dictionary<string, LinkerCodeFragment> & _codes;
			LinkerError _error;
		public:
			LinkerFunctionLoader(Volumes::Dictionary<string, LinkerCodeFragment> & codes, XA::IAssemblyTranslator * trans) : _codes(codes)
			{
				_trans.SetRetain(trans);
				_error.code = LinkerErrorCode::Success;
			}
			bool LoadError(LinkerError & error)
			{
				if (_error.code != LinkerErrorCode::Success) {
					error = _error;
					return true;
				} else return false;
			}
			void SetError(LinkerErrorCode code, const string & object)
			{
				if (_error.code == LinkerErrorCode::Success) {
					_error.code = code;
					_error.object = object;
				}
			}
			void HandlePlatformFunction(const string & symbol, const XA::TranslatedFunction & func) noexcept
			{
				try {
					LinkerCodeFragment frag;
					frag.referenced = false;
					frag.code_rva = frag.data_rva = frag.cs_offset = 0;
					frag.func = func;
					_codes.Append(symbol, frag);
				} catch (...) { SetError(LinkerErrorCode::WrongABI, symbol); }
			}
			void HandleAbstractFunction(const string & symbol, const XA::Function & func) noexcept
			{
				XA::TranslatedFunction trf;
				if (!_trans->Translate(trf, func)) SetError(LinkerErrorCode::WrongABI, symbol);
				HandlePlatformFunction(symbol, trf);
			}
			virtual Platform GetArchitecture(void) noexcept override { return _trans->GetPlatform(); }
			virtual XA::Environment GetEnvironment(void) noexcept override { return _trans->GetEnvironment(); }
			virtual void HandleAbstractFunction(const string & symbol, const XI::Module::Function & fin, Streaming::Stream * fout) noexcept override
			{
				try {
					XA::Function func;
					func.Load(fout);
					HandleAbstractFunction(symbol, func);
				} catch (...) { SetError(LinkerErrorCode::WrongABI, symbol); }
			}
			virtual void HandlePlatformFunction(const string & symbol, const XI::Module::Function & fin, Streaming::Stream * fout) noexcept override
			{
				try {
					XA::TranslatedFunction func;
					func.Load(fout);
					HandlePlatformFunction(symbol, func);
				} catch (...) { SetError(LinkerErrorCode::WrongABI, symbol); }
			}
			virtual void HandleNearImport(const string & symbol, const XI::Module::Function & fin, const string & func_name) noexcept override
			{
				SetError(LinkerErrorCode::LocalImportInModule, symbol);
			}
			virtual void HandleFarImport(const string & symbol, const XI::Module::Function & fin, const string & func_name, const string & lib_name) noexcept override
			{
				XA::TranslatedFunction func;
				Array<uint32> refoffs(1);
				if (_trans->GetPlatform() == Platform::X86) {
					const string * attr;
					if (attr = fin.attributes.GetElementByKey(AttributeStdCalll)) {
						uint words_unroll = 0;
						i286_EncoderContext enc(func.code, EncoderMode::i386_32);
						try { words_unroll = attr->ToUInt32(); } catch (...) {}
						if (words_unroll) {
							enc.encode_arop_reg_imm(AROP::SUB, 4, Reg::ESP, words_unroll * 4);
							enc.encode_push(Reg::EBP);
							enc.encode_mov_reg_reg(4, Reg::EBP, Reg::ESP);
							for (uint i = 0; i < words_unroll + 1; i++) {
								enc.encode_mov_reg_mem(4, Reg::EAX, Reg::EBP, Reg::NO, 4 * (i + words_unroll + 1));
								enc.encode_mov_mem_reg(4, Reg::EBP, Reg::NO, 4 * (i + 1), Reg::EAX);
							}
							enc.encode_pop(Reg::EBP);
						}
					}
					refoffs << (func.code.Length() + 1);
					// MOV EAX, [address-of-import-table-entry] == jump address
					func.code << 0xA1;
					func.code << 0x00 << 0x00 << 0x00 << 0x00;
					// JMP EAX
					func.code << 0xFF << 0xE0;
				} else if (_trans->GetPlatform() == Platform::X64) {
					refoffs << 2;
					// MOV RAX, [address-of-import-table-entry] == jump address
					func.code << 0x48 << 0xA1;
					func.code << 0x00 << 0x00 << 0x00 << 0x00 << 0x00 << 0x00 << 0x00 << 0x00;
					// JMP RAX
					func.code << 0xFF << 0xE0;
				} else if (_trans->GetPlatform() == Platform::ARM64) {

					// TODO: IMPLEMENT

				} else throw Exception();
				while (func.code.Length() & 0xF) func.code.Append(0);
				func.extrefs.Append(L"/" + lib_name + L"/" + func_name, refoffs);
				HandlePlatformFunction(symbol, func);
			}
			virtual void HandleLoadError(const string & symbol, const XI::Module::Function & fin, XI::LoadFunctionError error) noexcept override
			{
				SetError(LinkerErrorCode::WrongABI, symbol);
			}
		};

		string EscapeStringForXml(const string & input)
		{
			DynamicString result;
			for (int i = 0; i < input.Length(); i++) {
				auto c = input[i];
				if (c == L'\"') {
					result += L"&quot;";
				} else if (c == L'\'') {
					result += L"&apos;";
				} else if (c == L'<') {
					result += L"&lt;";
				} else if (c == L'>') {
					result += L"&gt;";
				} else if (c == L'&') {
					result += L"&amp;";
				} else result += c;
			}
			return result.ToString();
		}
		void LinkDataWrite(DataBlock * data, const void * src, int length)
		{
			auto offset = data->Length();
			data->SetLength(offset + length);
			MemoryCopy(data->GetBuffer() + offset, src, length);
		}
		void LinkResourceStringEncode(DataBlock * data, const string & text)
		{
			SafePointer<DataBlock> block = text.EncodeSequence(Encoding::UTF16, false);
			uint16 length = block->Length();
			LinkDataWrite(data, &length, 2);
			LinkDataWrite(data, block->GetBuffer(), block->Length());
			while (data->Length() & 3) data->Append(0);
		}
		void LinkResourceEncode(DataBlock * data, ResourceItem & item, uint ds_rva)
		{
			if (item.data) {
				PEResourceItem hdr;
				hdr.data_rva = ds_rva + data->Length() + sizeof(hdr);
				hdr.size = item.data->Length();
				hdr.codepage = pe_unicode_cp;
				hdr.reserved = 0;
				LinkDataWrite(data, &hdr, sizeof(hdr));
				LinkDataWrite(data, item.data->GetBuffer(), item.data->Length());
				while (data->Length() & 3) data->Append(0);
			} else {
				PEResourceTableHeader hdr;
				Array<PEResourceTableItem> leaves(0x20);
				ZeroMemory(&hdr, sizeof(hdr));
				hdr.num_named_entries = item.items_named.Count();
				hdr.num_indexed_entries = item.items_indexed.Count();
				leaves.SetLength(hdr.num_named_entries + hdr.num_indexed_entries);
				int table_offset = data->Length();
				int table_size = sizeof(hdr) + sizeof(PEResourceTableItem) * (hdr.num_named_entries + hdr.num_indexed_entries);
				data->SetLength(table_offset + table_size);
				int index = 0;
				for (auto & si : item.items_named) {
					leaves[index].name_rsa = data->Length();
					LinkResourceStringEncode(data, si.key);
					leaves[index].item_rsa = data->Length();
					if (!si.value.data) leaves[index].item_rsa |= 0x80000000;
					LinkResourceEncode(data, si.value, ds_rva);
					index++;
				}
				for (auto & si : item.items_indexed) {
					leaves[index].index = si.key;
					leaves[index].item_rsa = data->Length();
					if (!si.value.data) leaves[index].item_rsa |= 0x80000000;
					LinkResourceEncode(data, si.value, ds_rva);
					index++;
				}
				MemoryCopy(data->GetBuffer() + table_offset, &hdr, sizeof(hdr));
				MemoryCopy(data->GetBuffer() + table_offset + sizeof(hdr), leaves.GetBuffer(), sizeof(PEResourceTableItem) * leaves.Length());
			}
		}
		void LinkResourceStore(ResourceItem & root, uint32 type, uint32 id, const string & name, uint32 lang, DataBlock * data)
		{
			ResourceItem * td, * rd, * ld;
			root.items_indexed.Append(type, ResourceItem());
			td = root.items_indexed[type];
			if (id) {
				td->items_indexed.Append(id, ResourceItem());
				rd = td->items_indexed[id];
			} else {
				td->items_named.Append(name, ResourceItem());
				rd = td->items_named[name];
			}
			rd->items_indexed.Append(lang, ResourceItem());
			ld = rd->items_indexed[lang];
			ld->data.SetRetain(data);
		}
		DataBlock * LinkResourceVersionChildrenEncode(DataBlock ** children, int length)
		{
			SafePointer<DataBlock> result = new DataBlock(0x100);
			for (int i = 0; i < length; i++) {
				result->Append(*children[i]);
				while (result->Length() & 3) result->Append(0);
			}
			result->Retain();
			return result;
		}
		DataBlock * LinkResourceVersionEncode(const string & key, const void * value, int value_length, const void * children, int children_length, bool is_text = false)
		{
			SafePointer<DataBlock> result = new DataBlock(0x100);
			SafePointer<DataBlock> key_enc = key.EncodeSequence(Encoding::UTF16, true);
			VIBlockHeader hdr;
			ZeroMemory(&hdr, sizeof(hdr));
			result->SetLength(sizeof(hdr));
			result->Append(*key_enc);
			if (value_length) {
				while (result->Length() & 3) result->Append(0);
				LinkDataWrite(result, value, value_length);
			}
			if (children_length) {
				while (result->Length() & 3) result->Append(0);
				LinkDataWrite(result, children, children_length);
			}
			hdr.value_length = is_text ? value_length / 2 : value_length;
			hdr.length = result->Length();
			hdr.type = is_text;
			MemoryCopy(result->GetBuffer(), &hdr, sizeof(hdr));
			result->Retain();
			return result;
		}
		DataBlock * LinkResourceVersionStringEncode(const string & key, const string & value)
		{
			SafePointer<DataBlock> enc = value.EncodeSequence(Encoding::UTF16, true);
			return LinkResourceVersionEncode(key, enc->GetBuffer(), enc->Length(), 0, 0, true);
		}
		void LinkResourceVersionStringEncode(ObjectArray<DataBlock> & hdlr, const string & key, const string & value)
		{
			SafePointer<DataBlock> block = LinkResourceVersionStringEncode(key, value);
			hdlr.Append(block);
		}
		void LinkResourceCreateVersion(ResourceItem & root, const XI::Module & mdl, const Volumes::Dictionary<string, string> & mdt)
		{
			uint32 neutral_locale = 0x040004B0;
			uint16 vi[4];
			ZeroMemory(&vi, sizeof(vi));
			try {
				auto version = mdt.GetElementByKey(XI::MetadataKeyVersion);
				if (version) {
					auto parts = version->Split(L'.');
					for (int i = 0; i < min(parts.Length(), 4); i++) vi[i] = parts[i].ToUInt32();
				}
			} catch (...) {}
			VICommonInfo common;
			ZeroMemory(&common, sizeof(common));
			common.signature = 0xFEEF04BD;
			common.file_version_major = common.product_version_major = vi[0];
			common.file_version_minor = common.product_version_minor = vi[1];
			common.file_version_secondary = common.product_version_secondary = vi[2];
			common.file_version_build = common.product_version_build = vi[3];
			common.flags_mask = 0x3F;
			common.os = 0x00040004; // 32-bit Windows NT
			common.type = 0x00000001; // Application
			Array<DataBlock *> blocks(0x20);
			ObjectArray<DataBlock> strings(0x20);
			auto metadata_company = mdt.GetElementByKey(XI::MetadataKeyCompanyName);
			auto metadata_mdlname = mdt.GetElementByKey(XI::MetadataKeyModuleName);
			auto metadata_version = mdt.GetElementByKey(XI::MetadataKeyVersion);
			auto metadata_copyright = mdt.GetElementByKey(XI::MetadataKeyCopyright);
			if (metadata_company) LinkResourceVersionStringEncode(strings, L"CompanyName", *metadata_company);
			if (metadata_copyright) LinkResourceVersionStringEncode(strings, L"LegalCopyright", *metadata_copyright);
			if (metadata_mdlname) {
				LinkResourceVersionStringEncode(strings, L"FileDescription", *metadata_mdlname);
				LinkResourceVersionStringEncode(strings, L"ProductName", *metadata_mdlname);
			}
			if (metadata_version) {
				LinkResourceVersionStringEncode(strings, L"FileVersion", *metadata_version);
				LinkResourceVersionStringEncode(strings, L"ProductVersion", *metadata_version);
			}
			LinkResourceVersionStringEncode(strings, L"InternalName", mdl.module_import_name);
			LinkResourceVersionStringEncode(strings, L"OriginalFilename", mdl.module_import_name + L".exe");
			for (auto & s : strings) blocks.Append(&s);
			SafePointer<DataBlock> strings_array = LinkResourceVersionChildrenEncode(blocks, blocks.Length());
			blocks.Clear();
			strings.Clear();
			SafePointer<DataBlock> string_table = LinkResourceVersionEncode(string(neutral_locale, HexadecimalBaseLowerCase, 8), 0, 0, strings_array->GetBuffer(), strings_array->Length(), true);
			SafePointer<DataBlock> string_block = LinkResourceVersionEncode(L"StringFileInfo", 0, 0, string_table->GetBuffer(), string_table->Length(), true);
			neutral_locale = (neutral_locale >> 16) | (neutral_locale << 16);
			SafePointer<DataBlock> var_item = LinkResourceVersionEncode(L"Translation", &neutral_locale, sizeof(neutral_locale), 0, 0);
			SafePointer<DataBlock> var_block = LinkResourceVersionEncode(L"VarFileInfo", 0, 0, var_item->GetBuffer(), var_item->Length());
			blocks << string_block;
			blocks << var_block;
			var_block = LinkResourceVersionChildrenEncode(blocks, blocks.Length());
			SafePointer<DataBlock> viroot = LinkResourceVersionEncode(L"VS_VERSION_INFO", &common, sizeof(common), var_block->GetBuffer(), var_block->Length());
			LinkResourceStore(root, pe_rt_version, 1, L"", 0, viroot);
		}
		void LinkResourceCreateManifest(ResourceItem & root, const XI::Module & mdl, const Volumes::Dictionary<string, string> & mdt)
		{
			string version = L"0.0.0.0";
			string identifier = mdl.module_import_name;
			string description = mdl.module_import_name;
			const string * attr;
			if (attr = mdt.GetElementByKey(XI::MetadataKeyVersion)) {
				version = EscapeStringForXml(*attr);
			}
			if (attr = mdt.GetElementByKey(XI::MetadataKeyModuleName)) {
				description = EscapeStringForXml(*attr);
				identifier = EscapeStringForXml(attr->Replace(L' ', L""));
				if (attr = mdt.GetElementByKey(XI::MetadataKeyCompanyName)) {
					identifier = EscapeStringForXml(attr->Replace(L' ', L"")) + L"." + identifier;
				}
			}
			Streaming::MemoryStream stream(0x10000);
			Streaming::TextWriter wri(&stream, Encoding::UTF8);
			wri << L"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>";
			wri << L"<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">";
			wri << L"<assemblyIdentity version=\"" << version << L"\" processorArchitecture=\"*\" name=\"" << identifier << L"\" type=\"win32\"/>";
			wri << L"<description>" << description << L"</description>";
			wri << L"<dependency><dependentAssembly>";
			wri << L"<assemblyIdentity type=\"win32\" name=\"Microsoft.Windows.Common-Controls\" version=\"6.0.0.0\" processorArchitecture=\"*\" publicKeyToken=\"6595b64144ccf1df\" language=\"*\"/>";
			wri << L"</dependentAssembly></dependency>";
			wri << L"<application xmlns=\"urn:schemas-microsoft-com:asm.v3\">";
			wri << L"<windowsSettings>";
			wri << L"<dpiAware xmlns=\"http://schemas.microsoft.com/SMI/2005/WindowsSettings\">true</dpiAware>";
			wri << L"</windowsSettings>";
			wri << L"<windowsSettings xmlns:ws2=\"http://schemas.microsoft.com/SMI/2016/WindowsSettings\">";
			wri << L"<ws2:longPathAware>true</ws2:longPathAware>";
			wri << L"</windowsSettings>";
			wri << L"</application>";
			wri << L"<compatibility xmlns=\"urn:schemas-microsoft-com:compatibility.v1\"><application>";
			wri << L"<supportedOS Id=\"{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}\"/>";
			wri << L"<supportedOS Id=\"{1f676c76-80e1-4239-95bb-83d0f6d0da78}\"/>";
			wri << L"<supportedOS Id=\"{4a2f28e3-53b9-4441-ba9c-d69d4a4a6e38}\"/>";
			wri << L"<supportedOS Id=\"{35138b9a-5d96-4fbd-8e2d-a2440225f93a}\"/>";
			wri << L"<supportedOS Id=\"{e2011457-1546-43c5-a5fe-008deee3d3f0}\"/>";
			wri << L"</application></compatibility>";
			wri << L"<trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v2\"><security><requestedPrivileges>";
			wri << L"<requestedExecutionLevel level=\"asInvoker\" uiAccess=\"FALSE\"></requestedExecutionLevel>";
			wri << L"</requestedPrivileges></security></trustInfo>";
			wri << L"</assembly>";
			stream.Seek(0, Streaming::Begin);
			SafePointer<DataBlock> rsrc = stream.ReadAll();
			LinkResourceStore(root, pe_rt_manifest, 1, L"", 0, rsrc);
		}
		void ReferCodeFragment(Volumes::Dictionary<string, LinkerCodeFragment> & codes, const string & name)
		{
			auto smbl = codes.GetElementByKey(name);
			if (!smbl || smbl->referenced) return;
			smbl->referenced = true;
			for (auto & der : smbl->func.extrefs) {
				auto & er = der.key;
				if (er[0] == L'S' && er[1] == L':') ReferCodeFragment(codes, er.Fragment(2, -1));
			}
		}
		void ZeroExpandBlock(DataBlock * data, int to_size) { data->SetLength(to_size); ZeroMemory(data->GetBuffer(), to_size); }
		void LoadModuleEntries(XI::Module * mdl, string & entry, Array<string> & init, Array<string> & sdwn)
		{
			for (auto & f : mdl->functions) {
				if (f.value.code_flags & XI::Module::Function::FunctionEntryPoint) entry = f.key;
				if (f.value.code_flags & XI::Module::Function::FunctionInitialize) init << f.key;
				if (f.value.code_flags & XI::Module::Function::FunctionShutdown) sdwn << f.key;
			}
		}
		bool LoadModuleByName(const string & name, const LinkerInput & input, string & entry, Array<string> & init, Array<string> & sdwn, ObjectArray<XI::Module> & modules, LinkerError & error)
		{
			for (auto & m : modules) if (m.module_import_name == name) return true;
			SafePointer<XI::Module> mdl;
			for (auto & path : input.path_xo) {
				try {
					auto full_path = IO::ExpandPath(path + L"/" + name + L".xo");
					SafePointer<Streaming::Stream> stream = new Streaming::FileStream(full_path, Streaming::AccessRead, Streaming::OpenExisting);
					mdl = new XI::Module(stream, XI::Module::ModuleLoadFlags::LoadExecute);
				} catch (...) {}
			}
			if (!mdl || mdl->module_import_name != name) {
				error.code = LinkerErrorCode::ModuleNotFound;
				error.object = name;
				return false;
			}
			modules.Append(mdl);
			for (auto & dep : mdl->modules_depends_on) if (!LoadModuleByName(dep, input, entry, init, sdwn, modules, error)) return false;
			LoadModuleEntries(mdl, entry, init, sdwn);
			return true;
		}
		XI::Module * LoadModuleByPath(const string & name, const LinkerInput & input, string & entry, Array<string> & init, Array<string> & sdwn, ObjectArray<XI::Module> & modules, LinkerError & error)
		{
			SafePointer<XI::Module> mdl;
			try {
				SafePointer<Streaming::Stream> stream = new Streaming::FileStream(name, Streaming::AccessRead, Streaming::OpenExisting);
				mdl = new XI::Module(stream, XI::Module::ModuleLoadFlags::LoadExecute);
			} catch (...) {
				error.code = LinkerErrorCode::MainModuleLoadingError;
				error.object = name;
				return 0;
			}
			for (auto & m : modules) if (m.module_import_name == mdl->module_import_name) return &m;
			modules.Append(mdl);
			for (auto & dep : mdl->modules_depends_on) if (!LoadModuleByName(dep, input, entry, init, sdwn, modules, error)) return 0;
			LoadModuleEntries(mdl, entry, init, sdwn);
			return mdl;
		}
		XA::Function CreateEntryPoint(string & entry, Array<string> & init, Array<string> & sdwn)
		{
			XA::Function result;
			result.retval = XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 0);
			for (int i = 0; i < 16; i++) result.data << 0;
			auto eptr = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformTakePointer, XA::ReferenceFlagInvoke));
			auto echk = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceTransform, XA::TransformVectorNotZero, XA::ReferenceFlagInvoke));
			XA::TH::AddTreeInput(eptr, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceData, 0)), XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 2));
			XA::TH::AddTreeOutput(eptr, XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 1));
			XA::TH::AddTreeInput(echk, XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceData, 0)), XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 1));
			XA::TH::AddTreeOutput(echk, XA::TH::MakeSpec(XA::ArgumentSemantics::Unclassified, 0, 1));
			for (auto & f : init.Elements()) {
				int eindex = result.extrefs.Length();
				result.extrefs << (L"S:" + f);
				auto inv = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceExternal, eindex, XA::ReferenceFlagInvoke));
				XA::TH::AddTreeInput(inv, eptr, XA::TH::MakeSpec(XA::ArgumentSemantics::ErrorData, 0, 1));
				XA::TH::AddTreeOutput(inv, result.retval);
				result.instset << XA::TH::MakeStatementExpression(inv);
				result.instset << XA::TH::MakeStatementJump(0, echk);
			}
			int eindex = result.extrefs.Length();
			result.extrefs << (L"S:" + entry);
			auto inv = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceExternal, eindex, XA::ReferenceFlagInvoke));
			XA::TH::AddTreeInput(inv, eptr, XA::TH::MakeSpec(XA::ArgumentSemantics::ErrorData, 0, 1));
			XA::TH::AddTreeOutput(inv, result.retval);
			result.instset << XA::TH::MakeStatementExpression(inv);
			for (auto & f : sdwn.InversedElements()) {
				int eindex = result.extrefs.Length();
				result.extrefs << (L"S:" + f);
				auto inv = XA::TH::MakeTree(XA::TH::MakeRef(XA::ReferenceExternal, eindex, XA::ReferenceFlagInvoke));
				XA::TH::AddTreeOutput(inv, result.retval);
				result.instset << XA::TH::MakeStatementExpression(inv);
			}
			auto retindex = result.instset.Length();
			result.instset << XA::TH::MakeStatementReturn();
			for (int i = 0; i < result.instset.Length(); i++) {
				auto & o = result.instset[i];
				if (o.opcode == XA::OpcodeConditionalJump) o.attachment.num_bytes = retindex - i - 1;
			}
			return result;
		}
		void Link(const LinkerInput & input, LinkerOutput & output, LinkerError & error)
		{
			output.cs.SetReference(0);
			output.ds.SetReference(0);
			output.ls.SetReference(0);
			output.is.SetReference(0);
			output.rs.SetReference(0);
			ZeroMemory(&output, sizeof(output));
			error.code = LinkerErrorCode::Success;
			error.object = L"";
			ObjectArray<XI::Module> modules(0x20);
			Array<string> init(0x20), sdwn(0x20);
			string entry;
			auto main = LoadModuleByPath(input.path_xx, input, entry, init, sdwn, modules, error);
			if (!main) return;
			SafePointer<XI::Module> mdt;
			if (input.osenv == XA::Environment::Windows) {
				try {
					SafePointer<Streaming::Stream> stream = new Streaming::FileStream(input.path_attr, Streaming::AccessRead, Streaming::OpenExisting);
					mdt = new XI::Module(stream, XI::Module::ModuleLoadFlags::LoadResources);
				} catch (...) { error.code = LinkerErrorCode::MainModuleLoadingError; error.object = input.path_attr; return; }
			} else mdt.SetRetain(main);
			output.image_base = 0x00400000;
			SafePointer<XA::IAssemblyTranslator> trans = XA::CreatePlatformTranslator(input.arch, input.osenv);
			if (!trans) { error.code = LinkerErrorCode::WrongABI; error.object = input.path_xx; return; }
			auto entry_func = CreateEntryPoint(entry, init, sdwn);
			init.Clear(); sdwn.Clear(); entry = L"_@INITUS";
			Volumes::Dictionary<string, LinkerCodeFragment> codes; // symbol - fragment
			LinkerFunctionLoader loader(codes, trans);
			loader.HandleAbstractFunction(entry, entry_func);
			uint32 code_mem_load = 0;
			uint32 data_mem_load = 0;
			for (auto & m : modules) {
				for (auto & f : m.functions) XI::LoadFunction(f.key, f.value, &loader);
				for (auto & c : m.classes) for (auto & f : c.value.methods) {
					if ((f.value.code_flags & XI::Module::Function::FunctionClassMask) == XI::Module::Function::FunctionClassNull) continue;
					XI::LoadFunction(c.key + L"." + f.key, f.value, &loader);
				}
				for (auto & l : m.literals) {
					if (!m.data) m.data = new DataBlock(0x100);
					while (m.data->Length() % 8) m.data->Append(0);
					int offset = m.data->Length();
					XI::Module::Variable var;
					if (l.value.contents == XI::Module::Literal::Class::String) {
						SafePointer<DataBlock> enc = l.value.data_string.EncodeSequence(Encoding::UTF32, true);
						m.data->Append(*enc);
						var.size = XA::TH::MakeSize(enc->Length(), 0);
					} else {
						m.data->SetLength(offset + l.value.length);
						MemoryCopy(m.data->GetBuffer() + offset, &l.value.data_uint64, l.value.length);
						var.size = XA::TH::MakeSize(l.value.length, 0);
					}
					var.attributes = l.value.attributes;
					var.offset = XA::TH::MakeSize(offset, 0);
					m.variables.Append(l.key, var);
				}
				m.literals.Clear();
				data_mem_load += m.data->Length();
			}
			if (loader.LoadError(error)) return;
			ReferCodeFragment(codes, entry);
			auto code_element = codes.GetFirst();
			while (code_element) {
				auto next = code_element->GetNext();
				if (!code_element->GetValue().value.referenced) codes.Remove(code_element->GetValue().key);
				code_element = next;
			}
			for (auto & c : codes) {
				code_mem_load += c.value.func.code.Length();
				data_mem_load += c.value.func.data.Length();
			}
			uint32 rva_base = input.base_rva;
			output.cs_rva = rva_base;
			output.cs = new DataBlock(windows_block_size);
			ZeroExpandBlock(output.cs, Align(code_mem_load, windows_block_size));
			rva_base += Align(code_mem_load, windows_page_size);
			output.ds_rva = rva_base;
			output.ds = new DataBlock(windows_block_size);
			ZeroExpandBlock(output.ds, Align(data_mem_load, windows_block_size));
			rva_base += Align(data_mem_load, windows_page_size);
			Volumes::Dictionary<string, uint32> variables; // symbol - RVA
			Volumes::List< Volumes::KeyValuePair<string, uint32> > imports; // symbol - offset at CS
			Array<uint32> relocations(0x1000); // a list of RVAs
			code_mem_load = data_mem_load = 0;
			for (auto & m : modules) {
				for (auto & v : m.variables) {
					uint32 var_rva = v.value.offset.num_bytes + trans->GetWordSize() * v.value.offset.num_words;
					var_rva += data_mem_load + output.ds_rva;
					variables.Append(v.key, var_rva);
				}
				if (m.data) {
					MemoryCopy(output.ds->GetBuffer() + data_mem_load, m.data->GetBuffer(), m.data->Length());
					data_mem_load += m.data->Length();
				}
			}
			for (auto & c : codes) {
				MemoryCopy(output.ds->GetBuffer() + data_mem_load, c.value.func.data.GetBuffer(), c.value.func.data.Length());
				for (auto & e : c.value.func.extrefs) if (e.key[0] == L'/') {
					for (auto & offs : e.value) imports.InsertLast(Volumes::KeyValuePair<string, uint32>(e.key, code_mem_load + offs));
				}
				c.value.code_rva = output.cs_rva + code_mem_load;
				c.value.data_rva = output.ds_rva + data_mem_load;
				c.value.cs_offset = code_mem_load;
				code_mem_load += c.value.func.code.Length();
				data_mem_load += c.value.func.data.Length();
			}
			for (auto & c : codes) {
				auto & func = c.value.func;
				for (auto & cl : func.code_reloc) {
					if (trans->GetWordSize() == 8) {
						*reinterpret_cast<uint64 *>(func.code.GetBuffer() + cl) += output.image_base + c.value.code_rva;
					} else if (trans->GetWordSize() == 4) {
						*reinterpret_cast<uint32 *>(func.code.GetBuffer() + cl) += output.image_base + c.value.code_rva;
					}
					relocations << (c.value.code_rva + cl);
				}
				for (auto & dl : func.data_reloc) {
					if (trans->GetWordSize() == 8) {
						*reinterpret_cast<uint64 *>(func.code.GetBuffer() + dl) += output.image_base + c.value.data_rva;
					} else if (trans->GetWordSize() == 4) {
						*reinterpret_cast<uint32 *>(func.code.GetBuffer() + dl) += output.image_base + c.value.data_rva;
					}
					relocations << (c.value.code_rva + dl);
				}
				for (auto & ext : func.extrefs) {
					if (ext.key[0] == L'S' && ext.key[1] == L':') {
						auto smbl = ext.key.Fragment(2, -1);
						uint32 ext_rva;
						auto ext_code = codes.GetElementByKey(smbl);
						if (ext_code) {
							ext_rva = ext_code->code_rva;
						} else {
							auto ext_var = variables.GetElementByKey(smbl);
							if (ext_var) {
								ext_rva = *ext_var;
							} else {
								error.code = LinkerErrorCode::UnknownSymbolReference;
								error.object = ext.key;
								return;
							}
						}
						for (auto & rl : ext.value) {
							if (trans->GetWordSize() == 8) {
								*reinterpret_cast<uint64 *>(func.code.GetBuffer() + rl) = output.image_base + ext_rva;
							} else if (trans->GetWordSize() == 4) {
								*reinterpret_cast<uint32 *>(func.code.GetBuffer() + rl) = output.image_base + ext_rva;
							}
							relocations << (c.value.code_rva + rl);
						}
					} else if (ext.key[0] == L'/') {
						for (auto & rl : ext.value) {
							if (trans->GetWordSize() == 8) {
								*reinterpret_cast<uint64 *>(func.code.GetBuffer() + rl) = 0;
							} else if (trans->GetWordSize() == 4) {
								*reinterpret_cast<uint32 *>(func.code.GetBuffer() + rl) = 0;
							}
							relocations << (c.value.code_rva + rl);
						}
					} else {
						error.code = LinkerErrorCode::IllegalSymbolReference;
						error.object = ext.key;
						return;
					}
				}
				MemoryCopy(output.cs->GetBuffer() + c.value.cs_offset, func.code.GetBuffer(), func.code.Length());
			}
			auto entry_code = codes.GetElementByKey(entry);
			if (!entry_code) throw InvalidStateException();
			output.entry_rva = entry_code->code_rva;
			modules.Clear();
			entry = L"";
			entry_func.Clear();
			codes.Clear();
			variables.Clear();
			if (relocations.Length()) {
				uint16 rel_flags = 0;
				if (trans->GetWordSize() == 8) rel_flags = pe_reloc_64;
				else if (trans->GetWordSize() == 4) rel_flags = pe_reloc_32;
				SortArray(relocations);
				output.ls_rva = rva_base;
				output.ls = new DataBlock(windows_block_size);
				int s = 0;
				while (s < relocations.Length()) {
					int e = 0;
					while (e < relocations.Length() && relocations[e] / windows_page_size == relocations[s] / windows_page_size) e++;
					PERelocationBlock rlb;
					rlb.page_rva = relocations[s] / windows_page_size * windows_page_size;
					rlb.block_size = sizeof(rlb) + 2 * (e - s);
					while (rlb.block_size & 3) rlb.block_size++;
					auto rlb_offs = output.ls->Length();
					output.ls->SetLength(rlb_offs + rlb.block_size);
					ZeroMemory(output.ls->GetBuffer() + rlb_offs, rlb.block_size);
					MemoryCopy(output.ls->GetBuffer() + rlb_offs, &rlb, sizeof(rlb));
					for (int i = 0; i < e - s; i++) {
						uint16 desc = rel_flags;
						desc |= relocations[s + i] % windows_page_size;
						MemoryCopy(output.ls->GetBuffer() + rlb_offs + sizeof(rlb) + 2 * i, &desc, 2);
					}
					s = e;
				}
				output.ls_size = output.ls->Length();
				while (output.ls->Length() % windows_block_size) output.ls->Append(0);
				rva_base += Align(output.ls->Length(), windows_page_size);
			}
			relocations.Clear();
			if (!imports.IsEmpty()) {
				Volumes::Dictionary< string, Volumes::List< Volumes::KeyValuePair<string, uint32> > > itbl; // DLL - SYMBOL - CS OFFSET
				for (auto & i : imports) {
					auto sep = i.key.FindLast(L'/');
					auto dll = i.key.Fragment(1, sep - 1);
					auto smbl = i.key.Fragment(sep + 1, -1);
					itbl.Append(dll, Volumes::List< Volumes::KeyValuePair<string, uint32> >());
					auto dll_entry = itbl.GetElementByKey(dll);
					if (!dll_entry) throw InvalidStateException();
					dll_entry->InsertLast(Volumes::KeyValuePair<string, uint32>(smbl, i.value));
				}
				output.is_rva = rva_base;
				output.is = new DataBlock(windows_block_size);
				Array<PEImportTableEntry> dlit(0x10);
				dlit.SetLength(itbl.Count() + 1);
				output.is_size = dlit.Length() * sizeof(PEImportTableEntry);
				output.is->SetLength(output.is_size);
				ZeroMemory(dlit.GetBuffer(), output.is_size);
				int index = 0;
				auto scale = trans->GetWordSize() / 4;
				for (auto & dll : itbl) {
					SafePointer<DataBlock> dll_name = (dll.key + L".dll").EncodeSequence(Encoding::ANSI, true);
					while (dll_name->Length() & 7) dll_name->Append(0);
					dlit[index].dll_name_rva = output.is_rva + output.is->Length();
					LinkDataWrite(output.is, dll_name->GetBuffer(), dll_name->Length());
					Array<uint32> id(0x1000);
					id.SetLength((dll.value.Count() + 1) * scale);
					if (id.Length() & 1) id.Append(0);
					ZeroMemory(id.GetBuffer(), id.Length() * 4);
					int id_index = 0;
					for (auto & smbl : dll.value) {
						SafePointer<DataBlock> hint = (L"00" + smbl.key).EncodeSequence(Encoding::ANSI, true);
						hint->ElementAt(0) = hint->ElementAt(1) = 0;
						while (hint->Length() & 7) hint->Append(0);
						id[id_index] = output.is_rva + output.is->Length();
						LinkDataWrite(output.is, hint->GetBuffer(), hint->Length());
						id_index += scale;
					}
					dlit[index].local_import_table_rva = output.is_rva + output.is->Length();
					LinkDataWrite(output.is, id.GetBuffer(), id.Length() * 4);
					dlit[index].address_table_rva = output.is_rva + output.is->Length();
					LinkDataWrite(output.is, id.GetBuffer(), id.Length() * 4);
					id_index = 0;
					for (auto & smbl : dll.value) {
						uint32 func_rva = dlit[index].address_table_rva + 4 * id_index;
						if (scale == 2) {
							*reinterpret_cast<uint64 *>(output.cs->GetBuffer() + smbl.value) = output.image_base + func_rva;
						} else if (scale == 1) {
							*reinterpret_cast<uint32 *>(output.cs->GetBuffer() + smbl.value) = output.image_base + func_rva;
						}
						id_index += scale;
					}
					index++;
				}
				MemoryCopy(output.is->GetBuffer(), dlit.GetBuffer(), output.is_size);
				while (output.is->Length() % windows_block_size) output.is->Append(0);
				rva_base += Align(output.is->Length(), windows_page_size);
			}
			imports.Clear();
			trans.SetReference(0);
			output.subsystem = mdt->subsystem;
			SafePointer< Volumes::Dictionary<string, string> > metadata = XI::LoadModuleMetadata(mdt->resources);
			if (input.osenv == XA::Environment::Windows) {
				output.rs_rva = rva_base;
				output.rs = new DataBlock(windows_block_size);
				ResourceItem resource_root;
				LinkResourceCreateVersion(resource_root, *mdt, *metadata);
				SafePointer<Codec::Image> icon = XI::LoadModuleIcon(mdt->resources, 1);
				if (icon && icon->Frames.Length()) {
					for (int i = icon->Frames.Length() - 1; i >= 0; i--) {
						auto w = icon->Frames[i].GetWidth();
						auto h = icon->Frames[i].GetHeight();
						if (w == 256 && h == 256) continue;
						if (w == 64 && h == 64) continue;
						if (w == 48 && h == 48) continue;
						if (w == 32 && h == 32) continue;
						if (w == 24 && h == 24) continue;
						if (w == 16 && h == 16) continue;
						icon->Frames.Remove(i);
					}
					if (icon->Frames.Length()) {
						SafePointer<Streaming::MemoryStream> mem = new Streaming::MemoryStream(0x10000);
						Codec::EncodeImage(mem, icon, Codec::ImageFormatWindowsIcon);
						const uint8 * data = reinterpret_cast<const uint8 *>(mem->GetBuffer());
						int length = mem->Length();
						SafePointer<DataBlock> icon_desc = new DataBlock(0x100);
						auto & hdr = *reinterpret_cast<const ICOHeader *>(data);
						LinkDataWrite(icon_desc, &hdr, sizeof(hdr));
						for (int i = 0; i < hdr.count; i++) {
							ICOResourceItem rhdr;
							auto & shdr = *reinterpret_cast<const ICOFileItem *>(data + sizeof(hdr) + i * sizeof(ICOFileItem));
							MemoryCopy(&rhdr, &shdr, sizeof(rhdr));
							rhdr.index = i + 1;
							LinkDataWrite(icon_desc, &rhdr, sizeof(rhdr));
							SafePointer<DataBlock> icon_frame = new DataBlock(0x100);
							LinkDataWrite(icon_frame, data + shdr.offset, shdr.size);
							LinkResourceStore(resource_root, pe_rt_icon, rhdr.index, L"", 0, icon_frame);
						}
						LinkResourceStore(resource_root, pe_rt_icon_group, 1, L"", 0, icon_desc);
					}
				}
				icon.SetReference(0);
				LinkResourceCreateManifest(resource_root, *mdt, *metadata);
				for (auto & rsrc : input.resources) {
					SafePointer<DataBlock> rsrc_data;
					try {
						SafePointer<Streaming::Stream> stream = new Streaming::FileStream(rsrc.value, Streaming::AccessRead, Streaming::OpenExisting);
						rsrc_data = stream->ReadAll();
					} catch (...) { error.code = LinkerErrorCode::ResourceLoadingError; error.object = rsrc.value; return; }
					LinkResourceStore(resource_root, pe_rt_rcdata, 0, rsrc.key, 0, rsrc_data);
				}
				LinkResourceEncode(output.rs, resource_root, output.rs_rva);
				output.rs_size = output.rs->Length();
				while (output.rs->Length() % windows_block_size) output.rs->Append(0);
			}
			output.desired_stack = 0x100000;
			output.required_stack = windows_page_size;
			output.desired_heap = 0x100000;
			output.required_heap = windows_page_size;
			const string * attr;
			if (attr = metadata->GetElementByKey(AttributeRequiredStack)) try {
				output.required_stack = Align(attr->ToUInt32(), windows_page_size);
			} catch (...) {}
			if (attr = metadata->GetElementByKey(AttributeRequiredHeap)) try {
				output.required_heap = Align(attr->ToUInt32(), windows_page_size);
			} catch (...) {}
			if (attr = metadata->GetElementByKey(AttributeDesiredStack)) try {
				output.desired_stack = Align(attr->ToUInt32(), windows_page_size);
			} catch (...) {}
			if (attr = metadata->GetElementByKey(AttributeDesiredHeap)) try {
				output.desired_heap = Align(attr->ToUInt32(), windows_page_size);
			} catch (...) {}
		}
		uint Align(uint number, uint alignment) { return (number + alignment - 1) / alignment * alignment; }
		void AddSection(PESuperHeader32 & hdr, const char * name, DataBlock * ss, uint32 rva, uint32 flags)
		{
			if (!ss) return;
			auto index = hdr.hdr.hdr.num_sections;
			hdr.hdr.hdr.num_sections++;
			auto basement = index ? hdr.section[index - 1].file_offset + hdr.section[index - 1].file_size : hdr.hdr.hdr_ex.headers_size;
			MemoryCopy(hdr.section[index].name, name, 8);
			hdr.section[index].memory_size = XN::Align(ss->Length(), hdr.hdr.hdr_ex.section_alignment);
			hdr.section[index].memory_rva = rva;
			hdr.section[index].file_offset = basement;
			hdr.section[index].file_size = ss->Length();
			hdr.section[index].flags = flags;
		}
		void AddSection(PESuperHeader64 & hdr, const char * name, DataBlock * ss, uint32 rva, uint32 flags)
		{
			if (!ss) return;
			auto index = hdr.hdr.hdr.num_sections;
			hdr.hdr.hdr.num_sections++;
			auto basement = index ? hdr.section[index - 1].file_offset + hdr.section[index - 1].file_size : hdr.hdr.hdr_ex.headers_size;
			MemoryCopy(hdr.section[index].name, name, 8);
			hdr.section[index].memory_size = XN::Align(ss->Length(), hdr.hdr.hdr_ex.section_alignment);
			hdr.section[index].memory_rva = rva;
			hdr.section[index].file_offset = basement;
			hdr.section[index].file_size = ss->Length();
			hdr.section[index].flags = flags;
		}
		uint PEFormatHeaderSize(Platform arch)
		{
			if (arch == Platform::X86 || arch == Platform::ARM) return sizeof(PESuperHeader32);
			else if (arch == Platform::X64 || arch == Platform::ARM64) return sizeof(PESuperHeader64);
			else return 0;
		}
	}
}