#include <Cor/Cor.h>
#include <Consolatorium/Consolatorium.h>
#include <Formationes/Formationes.h>
#include <Imagines/Imagines.h>

using namespace ESSE;
using namespace ESSE::IO;

constexpr uint installation_mode_regular		= 0;
constexpr uint installation_mode_create_folder	= 1;
constexpr uint installation_mode_symlink_at_bin	= 2;
constexpr uint installation_mode_exec_handler	= 3;
constexpr uint installation_mode_app_metadata	= 4;

struct installation_media_creation_desc {
	union {
		struct
		{
			bool add_xx = true, add_xv = true, add_gui_apps = true, add_xx_sec = false;
			bool enable_hard_security = false, sign_modules = false;
		};
		bool values[6];
	};
};
struct installation_media_file_record {
	string source_file;
	string installation_path;
	struct { uint user, group, world; } permission;
	uint mode;
};
struct installation_icon_record {
	uint internal_name, width, height;
	string file_name;
};

string ModeString(void) {
	auto arch = System::GetSystemArchitecture();
	if (arch == System::Architecture::X86_32) return U"x86";
	else if (arch == System::Architecture::X86_64) return U"x64";
	else if (arch == System::Architecture::ARMv7_T32) return U"arm";
	else if (arch == System::Architecture::ARMv8_A64) return U"arm64";
	else return U"ignotum";
}
string ReadInstallationPath(Formationes::Archive * arc, uintptr index) { return ExpandPath(U"/" + arc->GetFileName(index)); }
string DesktopFileEscape(const string & str, bool escape_semicolon)
{
	dynamic_string_ucs4 result;
	for (uintptr i = 0; i < str.GetLength(); i++) {
		if (str[i] == U' ') result += U"\\s";
		else if (str[i] == U'\n') result += U"\\n";
		else if (str[i] == U'\t') result += U"\\t";
		else if (str[i] == U'\r') result += U"\\r";
		else if (str[i] == U'\\') result += U"\\\\";
		else if (str[i] == U';' && escape_semicolon) result += U"\\;";
		else result += str[i];
	}
	return result;
}
string XmlFileEscape(const string & str)
{
	dynamic_string_ucs4 result;
	for (uintptr i = 0; i < str.GetLength(); i++) {
		if (str[i] == U'<') result += U"&lt;";
		else if (str[i] == U'>') result += U"&gt;";
		else if (str[i] == U'&') result += U"&amp;";
		else result += str[i];
	}
	return result;
}
void DesktopFileWriteLocalized(TextEncoder & enc, const string & key, Formationes::RegistryNode * reg, const string & path)
{
	auto node = reg->OpenNode(path);
	if (node) for (auto & v : node->GetValues()) {
		auto value = node->GetValueString(v);
		if (v == U"_") enc.WriteLine(key + U"=" + DesktopFileEscape(value, false));
		else enc.WriteLine(key + U"[" + v + U"]=" + DesktopFileEscape(value, false));
	}
}
void PrintCheckBox(Console & console, const string & title, bool state, bool selected)
{
	if (selected) {
		console.SetBackgroundColor(ConsoleColor::Blue);
		console.SetTextColor(ConsoleColor::Black);
	} else {
		console.SetBackgroundColor(ConsoleColor::Black);
		console.SetTextColor(ConsoleColor::White);
	}
	if (state) console.Write(U" [ X ] ");
	else console.Write(U" [   ] ");
	console.Write(title);
	console.Write(U" ");
	console.SetBackgroundColor(ConsoleColor::Black);
	console.LineFeed();
}
int Invoke(const string & set_wd, const string & exec, const string & arg1 = U"", const string & arg2 = U"", const string & arg3 = U"", const string & arg4 = U"", const string & arg5 = U"", const string & arg6 = U"")
{
	CreateProcessDesc desc;
	desc.flags = CreateProcessSearchPath | CreateProcessOverrideWorkingDirectory;
	desc.image = exec;
	desc.working_directory = set_wd;
	if (arg1.GetLength()) desc.command_line << arg1;
	if (arg2.GetLength()) desc.command_line << arg2;
	if (arg3.GetLength()) desc.command_line << arg3;
	if (arg4.GetLength()) desc.command_line << arg4;
	if (arg5.GetLength()) desc.command_line << arg5;
	if (arg6.GetLength()) desc.command_line << arg6;
	auto process = CreateProcess(desc);
	process->Wait();
	return process->GetExitCode();
}
void IndexDirectory(array<installation_media_file_record> & imfr, const string & directory, const string & install_at_prefix, bool (* mode_correct) (const string & pure_name, installation_media_file_record & rec))
{
	auto files = EnumerateFiles(directory, U"", FileSearch::FileSearchMainEntries | FileSearch::FileSearchRecursive);
	for (auto & f : *files) {
		installation_media_file_record rec;
		rec.source_file = ExpandPath(directory + U"/" + f);
		rec.installation_path = ExpandPath(install_at_prefix + f);
		rec.mode = 0;
		auto file = FileStream::Create(rec.source_file, FileAccess::AccessRead, FileCreationMode::OpenExisting);
		GetFilePermissions(file->GetIOHandle(), &rec.permission.user, &rec.permission.group, &rec.permission.world);
		bool add = mode_correct ? mode_correct(f, rec) : true;
		if (add) imfr.Append(rec);
	}
}
void IndexCreateFolder(array<installation_media_file_record> & imfr, const string & install_at_path)
{
	installation_media_file_record rec;
	rec.installation_path = ExpandPath(install_at_path);
	rec.mode = installation_mode_create_folder;
	rec.permission.user = rec.permission.group = rec.permission.world = 0;
	imfr.Append(rec);
}
void InstallIcon(const array<string> & xdg_data_dirs, const array<installation_icon_record> & idb, uint index, const string & as, array<string> & uninstall)
{
	for (auto & d : xdg_data_dirs) {
		auto local_root = ExpandPath(d + U"/icons/hicolor");
		oref<array<string>> entries;
		try { entries = EnumerateFiles(local_root, U"", FileSearch::FileSearchDirectories); } catch (...) {}
		if (entries) for (auto & e : *entries) if (e[0] >= U'0' && e[0] <= U'9') try {
			auto ex_index = e.FindFirst(U'x');
			auto at_index = e.FindFirst(U'@');
			if (ex_index < 0) continue;
			Index2 size;
			if (at_index > ex_index) {
				size.x = e.Substring(0, ex_index).ToUInt32();
				size.y = e.Substring(ex_index + 1, at_index - ex_index - 1).ToUInt32();
				uint scale = e.Substring(at_index + 1, -1).ToUInt32();
				size.x *= scale;
				size.y *= scale;
			} else {
				size.x = e.Substring(0, ex_index).ToUInt32();
				size.y = e.Substring(ex_index + 1, -1).ToUInt32();
			}
			const installation_icon_record * rec = 0;
			int diff = 0;
			for (auto & i : idb) if (i.internal_name == index && i.width >= size.x && i.height >= size.y) {
				int ldiff = (i.width - size.x) + (i.height - size.y);
				if (!rec || ldiff < diff) { rec = &i; diff = ldiff; }
			}
			if (rec) {
				auto linkat = ExpandPath(local_root + U"/" + e + as);
				try { if (GetFileType(linkat) == FileType::SymbolicLink) RemoveFile(linkat); } catch (...) {}
				CreateSymbolicLink(linkat, rec->file_name);
				uninstall.Append(linkat);
			}
		} catch (...) {}
	}
}

bool mode_correct_xv(const string & pure_name, installation_media_file_record & rec)
{
	if (Path::GetExtension(pure_name).GetLength() == 0) {
		rec.permission.user = FileAccess::AccessAll;
		rec.permission.group = rec.permission.world = FileAccess::AccessReadOnly;
		rec.mode = installation_mode_symlink_at_bin;
	} else {
		rec.permission.user = FileAccess::AccessReadWrite;
		rec.permission.group = rec.permission.world = FileAccess::AccessRead;
		rec.mode = installation_mode_regular;
	}
	return true;
}
bool mode_correct_xv_meta(const string & pure_name, installation_media_file_record & rec)
{
	if (pure_name == U"_esse_imdt.ecsr") {
		rec.installation_path = ExpandPath(Path::GetDirectory(rec.installation_path) + U"/xvm");
		rec.permission.user = rec.permission.group = rec.permission.world = 0;
		rec.mode = installation_mode_app_metadata;
		return true;
	} else return false;
}
bool mode_correct_xx(const string & pure_name, installation_media_file_record & rec)
{
	if (pure_name == U"xx/xx" || pure_name == U"xx/xxf" || pure_name == U"xxsc" || pure_name == U"xc/xc") {
		rec.permission.user = FileAccess::AccessAll;
		rec.permission.group = rec.permission.world = FileAccess::AccessReadOnly;
	} else {
		rec.permission.user = FileAccess::AccessReadWrite;
		rec.permission.group = rec.permission.world = FileAccess::AccessRead;
	}
	if (pure_name == U"xx/xx") rec.mode = installation_mode_exec_handler;
	else if (pure_name == U"xx/xxf") rec.mode = installation_mode_symlink_at_bin;
	else rec.mode = installation_mode_regular;
	return true;
}
bool mode_correct_xx_meta(const string & pure_name, installation_media_file_record & rec)
{
	if (pure_name == U"_esse_imdt.ecsr") {
		rec.installation_path = ExpandPath(Path::GetDirectory(rec.installation_path) + U"/xxsc");
		rec.permission.user = rec.permission.group = rec.permission.world = 0;
		rec.mode = installation_mode_app_metadata;
		return true;
	} else return false;
}
bool mode_correct_xesec(const string & pure_name, installation_media_file_record & rec)
{
	if (pure_name.Substring(0, 4) == U"_obj") {
		return false;
	} else if (pure_name == U"_esse_imdt.ecsr") {
		rec.installation_path = ExpandPath(Path::GetDirectory(rec.installation_path) + U"/xesec");
		rec.permission.user = rec.permission.group = rec.permission.world = 0;
		rec.mode = installation_mode_app_metadata;
		return true;
	} else if (pure_name == U"xesec") {
		rec.permission.user = FileAccess::AccessAll;
		rec.permission.group = rec.permission.world = FileAccess::AccessReadOnly;
		rec.mode = installation_mode_regular;
		return true;
	} else {
		rec.permission.user = FileAccess::AccessReadWrite;
		rec.permission.group = rec.permission.world = FileAccess::AccessRead;
		rec.mode = installation_mode_regular;
		return true;
	}
}
bool mode_correct_lxi(const string & pure_name, installation_media_file_record & rec)
{
	if (pure_name.Substring(0, 4) == U"_obj") {
		return false;
	} else {
		rec.permission.user = FileAccess::AccessAll;
		rec.permission.group = rec.permission.world = FileAccess::AccessReadOnly;
		rec.mode = installation_mode_symlink_at_bin;
		return true;
	}
}

ESSE_MAIN_ROUTINE {
	try {
		auto console = CreateConsole();
		auto args = GetCommandLine();
		if (args->GetLength() < 2) {
			console->WriteLine(ESSE_META_NOMEN_APPLICATIONIS);
			console->LineFeed();
			console->WriteLine(U"Ute imperatum");
			console->WriteLine(U"  xx-lxi --deinstalla");
			console->WriteLine(U"acsi usor radicalis pro deinstallatione systemae XX.");
		} else if (args->ElementAt(1) == U"--verbum-modi" && args->GetLength() == 2) {
			console->WriteLine(ModeString());
		} else if (args->ElementAt(1) == U"--crea-installationem-cui" && args->GetLength() == 4) {
			array<installation_media_file_record> files(0x100);
			installation_media_creation_desc desc;
			auto xroot = ExpandPath(args->ElementAt(2));
			auto package = ExpandPath(args->ElementAt(3));
			uint check = 0;
			bool cancel = false;
			console->SetInputMode(ConsoleInputMode::Raw);
			console->AlternateScreenBuffer(true);
			while (true) {
				console->SetBackgroundColor(ConsoleColor::Black);
				console->SetTextColor(ConsoleColor::White);
				console->ClearScreen();
				auto size = console->GetDimensions();
				console->SetBackgroundColor(ConsoleColor::White);
				console->SetTextColor(ConsoleColor::Black);
				string title = U"CREATOR INSTALLATORIS SYSTEMAE XX";
				if (title.GetLength() > size.x) title = title.Substring(0, size.x);
				console->Write(string(U' ', (size.x - title.GetLength()) / 2));
				console->Write(title);
				console->Write(string(U' ', size.x - title.GetLength() - (size.x - title.GetLength()) / 2));
				console->SetCaretPosition(ConsolePosition(0, 1));
				console->SetBackgroundColor(ConsoleColor::Black);
				console->SetTextColor(ConsoleColor::White);
				console->LineFeed();
				PrintCheckBox(*console, U"Installa XX", desc.add_xx, check == 0);
				PrintCheckBox(*console, U"Installa XV", desc.add_xv, check == 1);
				PrintCheckBox(*console, U"Installa applicationes fenestrosas", desc.add_gui_apps, check == 2);
				PrintCheckBox(*console, U"Installa applicationem securitatis", desc.add_xx_sec, check == 3);
				PrintCheckBox(*console, U"Modus securitatis severus", desc.enable_hard_security, check == 4);
				PrintCheckBox(*console, U"Subscribe modulos", desc.sign_modules, check == 5);
				console->LineFeed();
				console->SetBackgroundColor(ConsoleColor::White);
				console->SetTextColor(ConsoleColor::Black);
				console->WriteFormatted(U" \0331F\x2193\0330F/\0331F\x2191\0330F - navigatio, \0331FSPATIUM\0330F - status, \0331F\x21B5\0330F - crea, \0331FESC\0330F - cancella ");
				ConsoleEventDesc event;
				console->ReadEvent(event);
				if (event.event == ConsoleInputEvent::KeyInput) {
					if (event.virtual_key_code == VirtualKeyCodes::Down) { check = (check + 1) % 6; }
					else if (event.virtual_key_code == VirtualKeyCodes::Up) { check = (check + 5) % 6; }
					else if (event.virtual_key_code == VirtualKeyCodes::Escape) { cancel = true; break; }
				} else if (event.event == ConsoleInputEvent::CharacterInput) {
					if (event.character == U' ') desc.values[check] = !desc.values[check];
					else if (event.character == U'\r') break;
				}
			}
			console->SetBackgroundColor(ConsoleColor::Default);
			console->SetTextColor(ConsoleColor::Default);
			console->AlternateScreenBuffer(false);
			console->SetInputMode(ConsoleInputMode::Echo);
			if (!desc.add_xx && !desc.add_xv && !desc.add_xx_sec) cancel = true;
			if (cancel) return 2;
			if (desc.add_xv || desc.add_xx) {
				int status;
				if (desc.add_gui_apps) status = Invoke(xroot, U"bash", U"./xv_release/build_linux_" + ModeString() + U".sh");
				else status = Invoke(xroot, U"bash", U"./xv_release/build_linux_" + ModeString() + U".sh", U"nogui");
				if (status) return status;
			}
			if (desc.add_xx) {
				int status;
				if (desc.add_gui_apps) status = Invoke(xroot, U"bash", U"./xx_release/build_linux_" + ModeString() + U".sh");
				else status = Invoke(xroot, U"bash", U"./xx_release/build_linux_" + ModeString() + U".sh", U"nogui");
				if (status) return status;
			}
			if (desc.add_xx_sec) {
				int status = Invoke(xroot, U"esse", U"xenv_sec_app/xesec.ertproj", U"-Nra", ModeString());
				if (status) return status;
			}
			if (desc.add_xx) {
				auto status = Invoke(xroot, U"xv", U"shellex_linux/xx-lxi.xv", U"-NX", xroot + U"/xx_release/_build/linux_" + ModeString() + U"/xx/xx", U"-dr", U"/opt/engine-software/xx", xroot + U"/xx_release/_build/linux_" + ModeString() + U"/store.ecso");
				if (status) return status;
				auto link = MemoryStream::Create(0x1000);
				auto reg = Formationes::Registry::Create();
				reg->CreateValue(U"DatabasePath", Formationes::RegistryValueType::String);
				reg->SetValue(U"DatabasePath", U"/opt/engine-software/xx/store.ecso");
				reg->Save(link);
				auto linkf = FileStream::Create(xroot + U"/xx_release/_build/linux_" + ModeString() + U"/store.ecs", FileAccess::AccessReadWrite, FileCreationMode::CreateAlways);
				linkf->WriteBlock(link->GetStorage());
				if (desc.add_xv) {
					linkf = FileStream::Create(xroot + U"/xv_release/_build/linux_" + ModeString() + U"/store.ecs", FileAccess::AccessReadWrite, FileCreationMode::CreateAlways);
					linkf->WriteBlock(link->GetStorage());
				}
				if (desc.add_xx_sec) {
					linkf = FileStream::Create(xroot + U"/xenv_sec_app/_build/linux_" + ModeString() + U"_release/store.ecs", FileAccess::AccessReadWrite, FileCreationMode::CreateAlways);
					linkf->WriteBlock(link->GetStorage());
				}
			}
			if (desc.enable_hard_security && desc.add_xx) {
				auto stream = FileStream::Create(xroot + U"/xx_release/_build/linux_" + ModeString() + U"/xe.ini", FileAccess::AccessReadWrite, FileCreationMode::OpenExisting);
				auto registry = Formationes::Registry::LoadGeneric(stream);
				registry->SetValue(U"ConvalidaConfisionem", true);
				stream->SetLength(0);
				stream->Seek(0, SeekOrigin::Begin);
				registry->SaveToText(stream, Unicode::Encoding::UTF8);
			}
			if (desc.add_xx) {
				console->SetTextColor(ConsoleColor::White);
				console->WriteLine(U"Adde certificatos fideles in collectorium proximum:");
				console->SetTextColor(ConsoleColor::Green);
				console->WriteLine(U"  " + ExpandPath(xroot + U"/xx_release/_build/linux_" + ModeString() + U"/fidelitas"));
				console->SetTextColor(ConsoleColor::White);
				console->WriteLine(U"Adde certificatos infideles in collectorium proximum:");
				console->SetTextColor(ConsoleColor::Red);
				console->WriteLine(U"  " + ExpandPath(xroot + U"/xx_release/_build/linux_" + ModeString() + U"/infidelitas"));
				console->SetTextColor(ConsoleColor::White);
				console->WriteLine(U"Adde indentitatem tuam in collectorium proximum per subscriptione modulorum:");
				console->SetTextColor(ConsoleColor::Cyan);
				console->WriteLine(U"  " + ExpandPath(xroot + U"/xx_release/_build/linux_" + ModeString() + U"/fidelitas"));
				console->SetTextColor(ConsoleColor::White);
				console->WriteLine(U"Pressa \'RETURN\' per duratione.");
				console->ReadLine();
			}
			if (desc.sign_modules && desc.add_xx) {
				auto sroot = xroot + U"/xx_release/_build/linux_" + ModeString();
				auto identities = EnumerateFiles(sroot, U"*.xeindentitas", FileSearch::FileSearchMainEntries | FileSearch::FileSearchRecursive);
				auto modules = EnumerateFiles(sroot, U"*.xo;*.xx", FileSearch::FileSearchMainEntries | FileSearch::FileSearchRecursive);
				if (!identities->GetLength()) {
					console->WriteLineFormatted(U"\033C*INDENTITAS NULLA.\033-*");
					return 3;
				}
				auto ifile = sroot + U"/" + identities->ElementAt(0);
				string password;
				console->WriteLineFormatted(U"\033A*LIMA INDENTITATIS: " + ifile + U"\033-*");
				console->WriteFormatted(U"\033E*CLAVIS INDENTITATIS: \033-*");
				console->SetInputMode(ConsoleInputMode::RawNoInterrupt);
				while (true) {
					ConsoleEventDesc event;
					console->ReadEvent(event);
					if (event.event == ConsoleInputEvent::CharacterInput) {
						if (event.character >= U' ') password += string(event.character, 1);
						else if (event.character == U'\b') password = password.Substring(0, password.GetLength() - 1);
						else if (event.character == U'\r') break;
					} else if (event.event == ConsoleInputEvent::KeyInput) {
						if (event.virtual_key_code == VirtualKeyCodes::Escape) { password = U""; break; }
					}
				}
				console->SetInputMode(ConsoleInputMode::Echo);
				console->LineFeed();
				for (uintptr i = 0; i < modules->GetLength(); i++) {
					auto m = ExpandPath(sroot + U"/" + modules->ElementAt(i));
					console->ClearLine();
					console->WriteFormatted(FormatString(U"\033E*SUBSCRIBO MODULOS\033-*: [ \033A*%0%%\033-* ] ", i * 100 / modules->GetLength()));
					auto status = Invoke(sroot, U"xi", m, U"-Sfpo", U"s", ifile, password.GetLength() ? password : U"-", m);
					if (status) {
						console->WriteLineFormatted(U"\033C*ERROR SUBSCRIBENDI.\033-*");
						return status;
					}
					console->ClearLine();
					console->WriteFormatted(FormatString(U"\033E*SUBSCRIBO MODULOS\033-*: [ \033A*%0%%\033-* ] ", (i + 1) * 100 / modules->GetLength()));
				}
				console->LineFeed();
				for (auto & i : *identities) RemoveFile(sroot + U"/" + i);
			}
			if (desc.add_xv) {
				IndexDirectory(files, xroot + U"/xv_release/_build/linux_" + ModeString(), U"/opt/engine-software/xv/", mode_correct_xv);
				if (desc.add_gui_apps) IndexDirectory(files, xroot + U"/xv_mm/_build/linux_" + ModeString() + U"_release", U"/opt/engine-software/xv/", mode_correct_xv_meta);
			}
			if (desc.add_xx) {
				IndexCreateFolder(files, U"/opt/engine-software/xx/fidelitas");
				IndexCreateFolder(files, U"/opt/engine-software/xx/infidelitas");
				IndexDirectory(files, xroot + U"/xx_release/_build/linux_" + ModeString(), U"/opt/engine-software/xx/", mode_correct_xx);
				if (desc.add_gui_apps) IndexDirectory(files, xroot + U"/xx_xxsc/_build/linux_" + ModeString() + U"_release", U"/opt/engine-software/xx/", mode_correct_xx_meta);
			}
			if (desc.add_xx_sec) {
				IndexDirectory(files, xroot + U"/xenv_sec_app/_build/linux_" + ModeString() + U"_release", U"/opt/engine-software/xesec/", mode_correct_xesec);
			}
			IndexDirectory(files, xroot + U"/shellex_linux/_build/linux_" + ModeString() + U"_release", U"/opt/engine-software/", mode_correct_lxi);
			auto pool = owrap(new ThreadPool);
			auto package_stream = FileStream::Create(package, FileAccess::AccessReadWrite, FileCreationMode::CreateAlways);
			auto archive = Formationes::NewArchive::Create(package_stream, files.GetLength(), Formationes::ArchiveFlags::Create64bit | Formationes::ArchiveFlags::UseMetadata);
			for (uintptr i = 0; i < files.GetLength(); i++) {
				auto & f = files[i];
				auto index = i + 1;
				if (f.source_file.GetLength()) console->WriteFormatted(FormatString(U"Addo \"\033B*%0\033-*\"...", f.source_file));
				console->WriteFormatted(U"Addo \033B*collectorium\033-*...");
				try {
					if (f.source_file.GetLength()) {
						auto source = FileStream::Create(f.source_file, FileAccess::AccessRead, FileCreationMode::OpenExisting);
						Compression::MethodChain chains[3];
						chains[0] = Compression::MethodChain(Compression::Method::FusedLempelZivWelchHuffman9bit);
						chains[1] = Compression::MethodChain(Compression::Method::FusedLempelZivWelchHuffman10bit);
						chains[2] = Compression::MethodChain(Compression::Method::LempelZivWelch, Compression::Method::Huffman);
						archive->SetFileData(index, source, chains, 3, Compression::Quality::Sequential, pool);
					}
					archive->SetFileName(index, f.installation_path.Substring(1, -1));
					archive->SetFilePermissions(index, f.permission.user, f.permission.group, f.permission.world);
					archive->SetFileUserData(index, f.mode);
				} catch (...) {
					console->WriteLineFormatted(U"\033C*ERROR\033-*");
					throw;
				}
				console->WriteLineFormatted(U"\033A*BENE!\033-*");
			}
			console->WriteFormatted(FormatString(U"Creo \"\033B*%0\033-*\"...", package));
			try { archive->Finalize(); } catch (...) {
				console->WriteLineFormatted(U"\033C*ERROR\033-*");
				throw;
			}
			console->WriteLineFormatted(U"\033A*BENE!\033-*");
		} else if (args->ElementAt(1) == U"--installa" && args->GetLength() == 3) {
			auto package = ExpandPath(args->ElementAt(2));
			if (IsProcessElevated()) {
				string stage;
				try {
					auto environment = GetEnvironment();
					array<string> xdg_data_dirs(0x10);
					if (environment) {
						auto xdg_data_dirs_var = environment->GetElementByKey(U"XDG_DATA_DIRS");
						if (xdg_data_dirs_var) xdg_data_dirs = SplitString(*xdg_data_dirs_var, U':'); else {
							xdg_data_dirs.Append(U"/usr/local/share/");
							xdg_data_dirs.Append(U"/usr/share/");
						}
					}
					array<string> extra_uninstall(0x100);
					bool update_desktop_database = false;
					Set<string> mime_update_list;
					stage = U"Aperio \"" + package + U"\"";
					auto archive_stream = FileStream::Create(package, FileAccess::AccessRead, FileCreationMode::OpenExisting);
					stage = U"Expono \"" + package + U"\"";
					auto archive = Formationes::Archive::Open(archive_stream);
					console->WriteLineFormatted(U"\033E*INSTALLO SYSTEMAM XX\033-*");
					for (uintptr index = 1; index <= archive->GetFileCount(); index++) {
						auto ud = archive->GetFileUserData(index);
						auto path = ReadInstallationPath(archive, index);
						if (!archive->GetFileName(index).GetLength()) continue;
						if (ud == installation_mode_regular || ud == installation_mode_symlink_at_bin || ud == installation_mode_exec_handler) {
							console->WriteLineFormatted(U"\033A*" + path + U"\033-*");
							stage = U"Creo collectoria \"" + path + U"\"";
							uint perm[3];
							CreateDirectoryTree(Path::GetDirectory(path));
							archive->GetFilePermissions(index, perm, perm + 1, perm + 2);
							stage = U"Creo limas \"" + path + U"\"";
							auto source = archive->QueryFileStream(index);
							auto dest = FileStream::Create(path, FileAccess::AccessWrite, FileCreationMode::CreateAlways);
							stage = U"Scribo permissiones \"" + path + U"\"";
							SetFilePermissions(dest->GetIOHandle(), perm[0], perm[1], perm[2]);
							stage = U"Scribo data \"" + path + U"\"";
							source->CopyToUntilEof(dest);
						} else if (ud == installation_mode_create_folder) {
							console->WriteLineFormatted(U"\033A*" + path + U"/\033-*");
							stage = U"Creo collectoria \"" + path + U"\"";
							CreateDirectoryTree(path);
						}
					}
					for (uintptr index = 1; index <= archive->GetFileCount(); index++) {
						auto ud = archive->GetFileUserData(index);
						auto path = ReadInstallationPath(archive, index);
						if (!archive->GetFileName(index).GetLength()) continue;
						if (ud == installation_mode_symlink_at_bin || ud == installation_mode_exec_handler) {
							auto linkat = ExpandPath(U"/usr/bin/" + Path::GetFileName(path));
							console->WriteLineFormatted(U"\033B*" + linkat + U" --> " + path + U"\033-*");
							stage = U"Creo adhaesionem \"" + linkat + U"\"";
							try { if (GetFileType(linkat) == FileType::SymbolicLink) RemoveFile(linkat); } catch (...) {}
							CreateSymbolicLink(linkat, path);
							extra_uninstall.Append(linkat);
						}
					}
					for (uintptr index = 1; index <= archive->GetFileCount(); index++) {
						auto ud = archive->GetFileUserData(index);
						auto path = ReadInstallationPath(archive, index);
						if (!archive->GetFileName(index).GetLength()) continue;
						if (ud == installation_mode_exec_handler) {
							string extfile = U"/etc/binfmt.d/xximago.conf";
							console->WriteLineFormatted(U"\033D**.xx ... --> xx *.xx ...\033-*");
							stage = U"Creo extensionem cordis \"" + extfile + U"\"";
							try {
								string desc = U":xximago:M:0:xximago\\x00::" + path + U":";
								auto register_stream = FileStream::Create(U"/proc/sys/fs/binfmt_misc/register", FileAccess::AccessWrite, FileCreationMode::OpenExisting);
								auto extfile_stream = FileStream::Create(extfile, FileAccess::AccessWrite, FileCreationMode::CreateAlways);
								SetFilePermissions(extfile_stream->GetIOHandle(), FileAccess::AccessReadWrite, FileAccess::AccessNo, FileAccess::AccessNo);
								try { owrap(new TextEncoder(register_stream, Unicode::Encoding::UTF8))->WriteLine(desc); } catch (...) {}
								try { owrap(new TextEncoder(extfile_stream, Unicode::Encoding::UTF8))->WriteLine(desc); } catch (...) {}
								extra_uninstall.Append(extfile);
							} catch (...) {}
						}
					}
					for (uintptr index = 1; index <= archive->GetFileCount(); index++) {
						auto ud = archive->GetFileUserData(index);
						auto path = ReadInstallationPath(archive, index);
						if (!archive->GetFileName(index).GetLength()) continue;
						if (ud == installation_mode_app_metadata) {
							update_desktop_database = true;
							array<installation_icon_record> icons_db(0x40);
							auto icons_root = path + U".icones/";
							console->WriteLineFormatted(U"\033D*" + path + U" --> >_<\033-*");
							stage = U"Onero metadatam applicationis \"" + path + U"\"";
							auto registry = Formationes::Registry::Load(archive->QueryFileStream(index));
							auto icons = registry->OpenNode(U"Icones");
							if (icons) for (auto & i : icons->GetValues()) {
								auto stream = MemoryStream::Create(0x10000);
								stream->SetLength(icons->GetValueBinarySize(i));
								stream->Seek(0, SeekOrigin::Begin);
								icons->GetValueBinary(i, stream->GetData());
								auto image = Picturae::DecodeImage(stream);
								for (auto & picture : *image) {
									auto & desc = picture.GetDesc();
									auto picture_path = icons_root + i + U"/" + string(desc.width) + U"x" + string(desc.height) + U".png";
									stage = U"Extraho iconem \"" + picture_path + U"\"";
									CreateDirectoryTree(Path::GetDirectory(picture_path));
									auto picture_stream = FileStream::Create(picture_path, FileAccess::AccessReadWrite, FileCreationMode::CreateAlways);
									Picturae::Encode(picture_stream, &picture, Picturae::ImageFormatPNG);
									SetFilePermissions(picture_stream->GetIOHandle(), FileAccess::AccessReadWrite, FileAccess::AccessRead, FileAccess::AccessRead);
									icons_db.Append(installation_icon_record { .internal_name = i.ToUInt32(), .width = desc.width, .height = desc.height, .file_name = picture_path });
								}
							}
							string appid = U"com." + registry->GetValueString(U"Applicatio/IndentitasAuthoris") + U"." + registry->GetValueString(U"Applicatio/IndentitasApplicationis");
							oref<TextEncoder> desktop_encoder;
							for (auto & d : xdg_data_dirs) {
								try {
									auto file_name = ExpandPath(d + U"/applications/" + appid + U".desktop");
									auto stream = FileStream::Create(file_name, FileAccess::AccessWrite, FileCreationMode::CreateAlways);
									extra_uninstall.Append(file_name);
									SetFilePermissions(stream->GetIOHandle(), FileAccess::AccessReadWrite, FileAccess::AccessRead, FileAccess::AccessRead);
									desktop_encoder = owrap(new TextEncoder(stream, Unicode::Encoding::UTF8));
									break;
								} catch (...) {}
							}
							if (!desktop_encoder) continue;
							desktop_encoder->WriteLine(U"[Desktop Entry]");
							desktop_encoder->WriteLine(U"Version=1.5");
							desktop_encoder->WriteLine(U"Type=Application");
							DesktopFileWriteLocalized(*desktop_encoder, U"Name", registry, U"Applicatio/Nomen");
							DesktopFileWriteLocalized(*desktop_encoder, U"Comment", registry, U"Applicatio/Descriptio");
							auto icon_index = registry->GetValueInteger(U"Applicatio/IndexIconis");
							if (icon_index) {
								InstallIcon(xdg_data_dirs, icons_db, icon_index, U"/apps/" + appid + U".png", extra_uninstall);
								desktop_encoder->WriteLine(U"Icon=" + appid);
							}
							desktop_encoder->WriteLine(U"Exec=" + path + U" %U");
							desktop_encoder->WriteLine(U"StartupWMClass=" + appid);
							array<string> mime_list(0x20);
							auto file_formats = registry->OpenNode(U"FormatiLimarum");
							if (file_formats && file_formats->GetSubnodes().GetLength()) {
								string mime_update;
								oref<TextEncoder> mime_encoder;
								for (auto & d : xdg_data_dirs) {
									try {
										auto file_name = ExpandPath(d + U"/mime/packages/" + appid + U".xml");
										auto stream = FileStream::Create(file_name, FileAccess::AccessWrite, FileCreationMode::CreateAlways);
										extra_uninstall.Append(file_name);
										SetFilePermissions(stream->GetIOHandle(), FileAccess::AccessReadWrite, FileAccess::AccessRead, FileAccess::AccessRead);
										mime_encoder = owrap(new TextEncoder(stream, Unicode::Encoding::UTF8));
										mime_update = ExpandPath(d + U"/mime");
										break;
									} catch (...) {}
								}
								if (mime_encoder) {
									mime_encoder->WriteLine(U"<?xml version=\"1.0\"?>");
									mime_encoder->WriteLine(U"<mime-info xmlns=\'http://www.freedesktop.org/standards/shared-mime-info\'>");
									for (auto & ffn : file_formats->GetSubnodes()) {
										auto file_format = file_formats->OpenNode(ffn);
										if (file_format) {
											icon_index = file_format->GetValueInteger(U"IndexIconis");
											auto description = file_format->OpenNode(U"Descriptio");
											auto extension = file_format->GetValueString(U"Extensio");
											auto mime_type = U"application/vnd.engine-software." + extension.Lowercased();
											mime_encoder->WriteLine(U"\t<mime-type type=\"" + mime_type + U"\">");
											if (description) for (auto & v : description->GetValues()) {
												string lang = v != U"_" ? U" xml:lang=\"" + v + U"\"" : U"";
												string value = description->GetValueString(v);
												mime_encoder->WriteLine(U"\t\t<comment" + lang + U">" + XmlFileEscape(value) + U"</comment>");
											}
											mime_encoder->WriteLine(U"\t\t<glob pattern=\"*." + extension.Lowercased() + U"\"/>");
											mime_encoder->WriteLine(U"\t</mime-type>");
											mime_list.Append(mime_type);
											if (icon_index) InstallIcon(xdg_data_dirs, icons_db, icon_index, U"/mimetypes/" + mime_type.Replace(U'/', U'-') + U".png", extra_uninstall);
										}
									}
									mime_encoder->WriteLine(U"</mime-info>");
									mime_encoder.Clear();
									mime_update_list.AddElement(mime_update);
								}
							}
							auto uri_schemes = registry->OpenNode(U"Protocolla");
							if (uri_schemes) for (auto & usn : uri_schemes->GetSubnodes()) {
								auto uri_scheme = uri_schemes->OpenNode(usn);
								if (uri_scheme) {
									auto scheme = uri_scheme->GetValueString(U"Schema");
									mime_list.Append(U"x-scheme-handler/" + scheme);
								}
							}
							if (mime_list.GetLength()) {
								desktop_encoder->Write(U"MimeType=");
								for (auto m : mime_list) desktop_encoder->Write(DesktopFileEscape(m, true) + U";");
								desktop_encoder->LineFeed();
							}
						}
					}
					auto setup_log_stream = FileStream::Create(U"/opt/engine-software/xx-lxi.log", FileAccess::AccessWrite, FileCreationMode::CreateAlways);
					auto setup_log = owrap(new TextEncoder(setup_log_stream, Unicode::Encoding::UTF8));
					SetFilePermissions(setup_log_stream->GetIOHandle(), FileAccess::AccessRead, FileAccess::AccessRead, FileAccess::AccessRead);
					for (auto & e : extra_uninstall) setup_log->WriteLine(e);
					if (update_desktop_database) setup_log->WriteLine(U"#");
					if (update_desktop_database) try { Invoke(U"/", U"update-desktop-database"); } catch (...) {}
					for (auto & mime_update : mime_update_list) try { Invoke(U"/", U"update-mime-database", mime_update); } catch (...) {}
				} catch (Exception & e) {
					console->WriteLineFormatted(FormatString(U"\033C*ERROR INSTALLATIONIS: %0:%1\033-*", e.GetError().error_code, e.GetError().error_subcode));
					console->WriteLineFormatted(U"\0334*STADIUM: " + stage + U"\033-*");
					return e.GetError().error_code;
				} catch (...) {
					console->WriteLineFormatted(U"\033C*ERROR INSTALLATIONIS IGNOTUS\033-*");
					console->WriteLineFormatted(U"\0334*STADIUM: " + stage + U"\033-*");
					return 2;
				}
			} else {
				console->WriteLineFormatted(U"\033E*ELEVO AD USORE RADICALE PRO INSTALLATIONE\033-*");
				return Invoke(GetCurrentDirectory(), U"sudo", GetExecutablePath(), U"--installa", package);
			}
		} else if (args->ElementAt(1) == U"--deinstalla" && args->GetLength() == 2) {
			if (IsProcessElevated()) {
				SetCurrentDirectory(U"/");
				auto setup_log_file = U"/opt/engine-software/xx-lxi.log";
				try {
					auto setup_log_stream = FileStream::Create(setup_log_file, FileAccess::AccessRead, FileCreationMode::OpenExisting);
					auto setup_log = owrap(new TextDecoder(setup_log_stream, Unicode::Encoding::UTF8));
					while (!setup_log->IsEOF()) {
						auto file = setup_log->ReadLine();
						if (file == U"#") {
							auto environment = GetEnvironment();
							array<string> xdg_data_dirs(0x10);
							if (environment) {
								auto xdg_data_dirs_var = environment->GetElementByKey(U"XDG_DATA_DIRS");
								if (xdg_data_dirs_var) xdg_data_dirs = SplitString(*xdg_data_dirs_var, U':'); else {
									xdg_data_dirs.Append(U"/usr/local/share/");
									xdg_data_dirs.Append(U"/usr/share/");
								}
							}
							try { Invoke(U"/", U"update-desktop-database"); } catch (...) {}
							for (auto & d : xdg_data_dirs) try {
								auto mime_update = ExpandPath(d + U"/mime");
								if (GetFileType(mime_update) == FileType::Directory) Invoke(U"/", U"update-mime-database", mime_update);
							} catch (...) {}
						} else if (file.GetLength()) try { RemoveFile(file); } catch (...) {}
					}
					setup_log.Clear();
					setup_log_stream.Clear();
					RemoveFile(setup_log_file);
				} catch (...) {}
				try { RemoveEntireDirectory(U"/opt/engine-software/xx"); } catch (...) {}
				try { RemoveEntireDirectory(U"/opt/engine-software/xv"); } catch (...) {}
				try { RemoveEntireDirectory(U"/opt/engine-software/xesec"); } catch (...) {}
				try { RemoveFile(U"/opt/engine-software/xx-lxi"); } catch (...) {}
				try { RemoveDirectory(U"/opt/engine-software"); } catch (...) {}
			} else {
				console->WriteLineFormatted(U"\033E*ELEVO AD USORE RADICALE PRO DEINSTALLATIONE\033-*");
				return Invoke(GetCurrentDirectory(), U"sudo", GetExecutablePath(), U"--deinstalla");
			}
		} else return 1;
		return 0;
	} catch (...) { return -1; }
}