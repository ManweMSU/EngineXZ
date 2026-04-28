#include <EngineRuntime.h>

#include "../xexec/xx_com.h"
#include "../xexec/xx_com_store.h"
#include "uiml.h"

using namespace Engine;
using namespace Engine::IO;
using namespace Engine::IO::ConsoleControl;
using namespace Engine::Streaming;
using namespace Engine::Storage;

Console console;

struct {
	SafePointer<StringTable> localization;
	string store_integration;
	string input;
	string output;
	string system;
	bool silent = false;
	bool nologo = false;
	bool enable_warnings = true;
	bool warnings_as_errors = false;
	bool build_style_library = false;
	bool time_estimations = false;
	bool write_color_tables = true;
	bool write_string_tables = true;
	bool print_mount_events = false;
	Array<string> include = Array<string>(0x10);
	Array<string> preload = Array<string>(0x10);
} state;

string Localized(int id)
{
	try {
		if (!state.localization) throw InvalidStateException();
		return state.localization->GetString(id);
	} catch (...) { return FormatString(L"LOC(%0)", id); }
}
void ProcessCommandLine(void)
{
	SafePointer< Array<string> > args = GetCommandLine();
	for (int i = 1; i < args->Length(); i++) {
		auto & arg = args->ElementAt(i);
		if (arg[0] == L':' || arg[0] == L'-') {
			for (int j = 1; j < arg.Length(); j++) {
				if (arg[j] == L'A') {
					state.warnings_as_errors = true;
				} else if (arg[j] == L'L') {
					state.build_style_library = true;
				} else if (arg[j] == L'N') {
					state.nologo = true;
				} else if (arg[j] == L'S') {
					state.silent = true;
				} else if (arg[j] == L'T') {
					state.time_estimations = true;
				} else if (arg[j] == L'a') {
					state.enable_warnings = false;
				} else if (arg[j] == L'l') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					state.include << ExpandPath(args->ElementAt(i));
				} else if (arg[j] == L'o') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					if (state.output.Length()) {
						console << TextColor(12) << Localized(202) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					state.output = ExpandPath(args->ElementAt(i));
				} else if (arg[j] == L'p') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					state.preload << ExpandPath(args->ElementAt(i));
				} else if (arg[j] == L't') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					auto & arg2 = args->ElementAt(i);
					if (arg2 == L"win")			state.system = L"Windows";
					else if (arg2 == L"mac")	state.system = L"MacOSX";
					else if (arg2 == L"lnx")	state.system = L"Linux";
					else {
						console << TextColor(12) << Localized(205) << TextColorDefault() << LineFeed();
						throw Exception();
					}
				} else if (arg[j] == L'v') {
					state.print_mount_events = true;
				} else if (arg[j] == L'x') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					auto & arg2 = args->ElementAt(i);
					if (arg2 == L"color")		state.write_color_tables = false;
					else if (arg2 == L"linea")	state.write_string_tables = false;
					else {
						console << TextColor(12) << Localized(205) << TextColorDefault() << LineFeed();
						throw Exception();
					}
				} else {
					console << TextColor(12) << Localized(204) << TextColorDefault() << LineFeed();
					throw Exception();
				}
			}
		} else {
			if (state.input.Length()) {
				console << TextColor(12) << Localized(201) << TextColorDefault() << LineFeed();
				throw Exception();
			}
			state.input = ExpandPath(arg);
		}
	}
}

void PrintReferencelessError(const string & desc, bool warning_styled = false)
{
	try {
		console.SetTextColor(warning_styled ? ConsoleColor::Yellow : ConsoleColor::Red);
		console.WriteLine(FormatString(warning_styled ? Localized(304) : Localized(303), desc));
		console.SetTextColor(ConsoleColor::Default);
	} catch (...) {}
}
void PrintSourceReferenceError(const string & desc, int offset, int length, bool warning_styled = false)
{
	try {
		FileStream source(state.input, AccessRead, OpenExisting);
		TextReader reader(&source);
		string text;
		DynamicString result;
		while (!reader.EofReached()) {
			auto line = reader.ReadLine();
			if (line.Length() > 1 && line[0] == L'#' && line[1] == L'#') result << L'\n';
			else result << line << L'\n';
		}
		text = result.ToString();
		int lb = offset, le = offset, ln = 1;
		for (int i = offset; i >= 0; i--) if (text[i] == L'\n') ln++;
		while (lb && (text[lb - 1] >= 32 || text[lb - 1] == L'\t')) lb--;
		while (text[lb] == L' ' || text[lb] == L'\t') lb++;
		while (le < text.Length() - 1 && (text[le + 1] >= 32 || text[le + 1] == L'\t')) le++;
		string line = text.Fragment(lb, le - lb + 1).Replace(L'\t', L' ');
		console.SetTextColor(warning_styled ? ConsoleColor::Yellow : ConsoleColor::Red);
		console.WriteLine(FormatString(warning_styled ? Localized(302) : Localized(301), desc, ln));
		console.SetTextColor(ConsoleColor::Default);
		console.WriteLine(line);
		console.SetTextColor(warning_styled ? ConsoleColor::DarkYellow : ConsoleColor::DarkRed);
		console << string(L' ', line.Fragment(0, offset - lb).GetEncodedLength(Encoding::UTF32));
		console << string(L'~', line.Fragment(offset - lb, length).GetEncodedLength(Encoding::UTF32));
		console << LineFeed();
		console.SetTextColor(ConsoleColor::Default);
	} catch (...) { PrintReferencelessError(desc, warning_styled); }
}
void PrintInformation(const string & desc)
{
	try {
		console.SetTextColor(ConsoleColor::Cyan);
		console.WriteLine(FormatString(Localized(305), desc));
		console.SetTextColor(ConsoleColor::Default);
	} catch (...) {}
}
class Verifyier : public UI::IResourceResolver, public UI::Format::IMissingStylesReporter
{
public:
	bool status_fine;
	Verifyier(void) : status_fine(true) {}
public:
	virtual Graphics::IBitmap * GetTexture(const string & Name) override
	{
		if (!state.silent && state.enable_warnings) PrintInformation(FormatString(Localized(601), Name));
		status_fine = false;
		return 0;
	}
	virtual Graphics::IFont * GetFont(const string & Name) override
	{
		if (!state.silent && state.enable_warnings) PrintInformation(FormatString(Localized(602), Name));
		status_fine = false;
		return 0;
	}
	virtual UI::Template::Shape * GetApplication(const string & Name) override
	{
		if (!state.silent && state.enable_warnings) PrintInformation(FormatString(Localized(603), Name));
		status_fine = false;
		return 0;
	}
	virtual UI::Template::ControlTemplate * GetDialog(const string & Name) override
	{
		if (!state.silent && state.enable_warnings) PrintInformation(FormatString(Localized(604), Name));
		status_fine = false;
		return 0;
	}
	virtual UI::Template::ControlReflectedBase * CreateCustomTemplate(const string & Class) override { return 0; }
	virtual void ReportStyleIsMissing(const string & Name, const string & Class) override
	{
		if (!state.silent && state.enable_warnings) PrintInformation(FormatString(Localized(605), Name, Class));
		status_fine = false;
	}
};
class WarningCollector : public UI::Markup::IWarningReporter
{
public:
	Array<UI::Markup::WarningDesc> warnings = Array<UI::Markup::WarningDesc>(0x10);
public:
	virtual void ReportWarning(const UI::Markup::WarningDesc & desc) override { warnings << desc; }
};
class VirtualFileSystem : public UI::Markup::IFileProvider
{
#ifdef ENGINE_WINDOWS
	static constexpr const widechar * _mount_prefix = L"\\\\.\\XUICCM\\";
#else
	static constexpr const widechar * _mount_prefix = L"/xuiccm/";
#endif
	Volumes::ObjectDictionary<string, Archive> _mount_points;
	Volumes::Dictionary<string, string> _link_points;
	uint _mcount;
private:
	bool _file_exists(const string & path)
	{
		Volumes::ObjectDictionary<string, Archive>::Element * mount = 0;
		auto current = _mount_points.GetRoot();
		while (current) {
			if (string::Compare(current->GetValue().key, path) > 0) current = current->GetLeft(); else {
				mount = current;
				current = current->GetRight();
			}
		}
		if (mount && path.Fragment(0, mount->GetValue().key.Length()) == mount->GetValue().key) {
			auto & key = mount->GetValue().key;
			auto arcpath = path.Fragment(key.Length(), -1);
			return mount->GetValue().value->FindArchiveFile(arcpath) != 0;
		} else return FileExists(path);
	}
	void _get_file_media(const string & path, Archive ** arc, string & relpath)
	{
		Volumes::ObjectDictionary<string, Archive>::Element * mount = 0;
		auto current = _mount_points.GetRoot();
		while (current) {
			if (string::Compare(current->GetValue().key, path) > 0) current = current->GetLeft(); else {
				mount = current;
				current = current->GetRight();
			}
		}
		if (mount && path.Fragment(0, mount->GetValue().key.Length()) == mount->GetValue().key) {
			*arc = mount->GetValue().value;
			relpath = path.Fragment(mount->GetValue().key.Length(), -1);
		} else {
			*arc = 0;
			relpath = path;
		}
	}
	string _file_redirect(const string & path)
	{
		Volumes::Dictionary<string, string>::Element * redirect = 0;
		auto current = _link_points.GetRoot();
		while (current) {
			if (string::Compare(current->GetValue().key, path) > 0) current = current->GetLeft(); else {
				redirect = current;
				current = current->GetRight();
			}
		}
		if (redirect && path.Fragment(0, redirect->GetValue().key.Length()) == redirect->GetValue().key) {
			return redirect->GetValue().value + path.Fragment(redirect->GetValue().key.Length(), -1);
		} else return path;
	}
public:
	VirtualFileSystem(void) : _mcount(0) {}
	virtual string LocateFile(const string & reference_file_name) override
	{
		auto redirect_file_name = ExpandPath(reference_file_name);
		while (true) {
			while (true) {
				auto new_rfn = _file_redirect(redirect_file_name);
				if (new_rfn == redirect_file_name) break;
				if (state.print_mount_events) {
					console.SetTextColor(ConsoleColor::Blue);
					console.WriteLine(redirect_file_name + L" -> " + new_rfn);
					console.SetTextColor(ConsoleColor::Default);
				}
				redirect_file_name = new_rfn;
			}
			if (_file_exists(redirect_file_name)) return redirect_file_name;
			int pos, len = redirect_file_name.Length();
			if (MemoryCompare(static_cast<const widechar *>(redirect_file_name), _mount_prefix, sizeof(widechar) * StringLength(_mount_prefix)) == 0) {
				pos = StringLength(_mount_prefix);
			} else pos = 1;
			SafePointer<Archive> new_mount;
			string new_mount_base_path, archive_name;
			while (pos < len) {
				if (redirect_file_name[pos] == L'/' || redirect_file_name[pos] == L'\\') {
					auto subpath = redirect_file_name.Fragment(0, pos);
					try {
						archive_name = subpath + L".uiarc";
						SafePointer<Stream> stream = OpenFile(archive_name);
						new_mount = OpenArchive(stream);
						if (new_mount) { new_mount_base_path = subpath; break; }
					} catch (...) {}
				}
				pos++;
			}
			if (new_mount) {
				string new_mount_name = _mount_prefix + string(_mcount, DecimalBase, 4);
				_mcount++;
				_mount_points.Append(new_mount_name, new_mount);
				_link_points.Append(new_mount_base_path, new_mount_name);
				if (state.print_mount_events) {
					console.SetTextColor(ConsoleColor::Blue);
					console.WriteLine(FormatString(Localized(153), archive_name, new_mount_name));
					console.SetTextColor(ConsoleColor::Default);
				}
			} else return L"";
		}
	}
	virtual Streaming::Stream * OpenFile(const string & effective_file_name) override
	{
		Archive * arc;
		string arcpath;
		_get_file_media(effective_file_name, &arc, arcpath);
		if (arc) {
			auto index = arc->FindArchiveFile(arcpath);
			return arc->QueryFileStream(index);
		} else return new FileStream(effective_file_name, AccessRead, OpenExisting);
	}
};

int Main(void)
{
	try {
		Codec::InitializeDefaultCodecs();
		UI::CurrentScaleFactor = 1.0;
		Assembly::CurrentLocale = Assembly::GetCurrentUserLocale();
		#ifdef ENGINE_WINDOWS
		state.system = L"Windows";
		#endif
		#ifdef ENGINE_MACOSX
		state.system = L"MacOSX";
		#endif
		#ifdef ENGINE_LINUX
		state.system = L"Linux";
		#endif
		auto root = Path::GetDirectory(GetExecutablePath());
		SafePointer<Registry> uic_conf = XX::LoadConfiguration(root + L"/xuicc.ini");
		state.store_integration = uic_conf->GetValueString(L"Entheca");
		auto language_override = uic_conf->GetValueString(L"Lingua");
		if (language_override.Length()) Assembly::CurrentLocale = language_override;
		auto localizations = uic_conf->GetValueString(L"Locale");
		if (localizations.Length()) {
			try {
				SafePointer<Stream> table = new FileStream(root + L"/" + localizations + L"/" + Assembly::CurrentLocale + L".ecst", AccessRead, OpenExisting);
				state.localization = new StringTable(table);
			} catch (...) {}
			if (!state.localization) {
				auto language_default = uic_conf->GetValueString(L"LinguaDefalta");
				try {
					SafePointer<Stream> table = new FileStream(root + L"/" + localizations + L"/" + language_default + L".ecst", AccessRead, OpenExisting);
					state.localization = new StringTable(table);
				} catch (...) {}
			}
		}
		ProcessCommandLine();
		if (state.input.Length() && !state.output.Length()) {
			string ext;
			if (state.build_style_library) ext = L"estl"; else ext = L"eui";
			state.output = ExpandPath(Path::GetDirectory(state.input) + L"/" + Path::GetFileNameWithoutExtension(state.input) + L"." + ext);
		}
		SafePointer<RegistryNode> inc = uic_conf->OpenNode(L"Liber");
		for (auto & v : inc->GetValues()) state.include << ExpandPath(root + L"/" + inc->GetValueString(v));
		if (state.store_integration.Length()) try {
			SafePointer<Registry> store_conf = XX::LoadConfiguration(root + "/" + state.store_integration);
			auto store_db_path = store_conf->GetValueString(L"DatabasePath");
			SafePointer<Stream> store_db_stream = new FileStream(store_db_path, AccessRead, OpenExisting);
			XX::InstallationDatabase store_db;
			Reflection::RestoreFromBinaryObject(store_db, store_db_stream);
			for (auto & p : store_db.Products) {
				if (p.Identifier == L"runtime") {
					string legacy_uic_root = Path::GetDirectory(ExpandPath(p.Path + L"/Tools/uicc.ini"));
					SafePointer<Registry> legacy_uic_conf = XX::LoadConfiguration(p.Path + L"/Tools/uicc.ini");
					state.include << ExpandPath(legacy_uic_root + L"/" + legacy_uic_conf->GetValueString(L"Include"));
				}
				for (auto & e : p.Plugins) if (e.Target == L"xuicc") state.include << ExpandPath(e.File);
			}
		} catch (...) {}
		SafePointer<RegistryNode> subordering = uic_conf->OpenNode(L"Subordo");
		UI::Markup::SetSuborderingTable(subordering);
	} catch (...) { return 0x40; }
	if (!state.nologo && !state.silent) {
		console << Localized(1) << LineFeedSequence;
		console << Localized(2) << LineFeedSequence;
		console << FormatString(Localized(3), ENGINE_VI_APPVERSION) << LineFeedSequence << LineFeedSequence;
	}
	if (state.input.Length()) {
		uint32 time = GetTimerValue();
		SafePointer<WarningCollector> collector = new WarningCollector;
		SafePointer<VirtualFileSystem> vfs = new VirtualFileSystem;
		UI::Markup::SetFileProvider(vfs);
		UI::Markup::SetWarningReporterCallback(collector);
		UI::Markup::InputDesc idesc;
		UI::Markup::ErrorDesc edesc;
		idesc.main_uiml = state.input;
		idesc.build_as_style = state.build_style_library;
		idesc.include = state.include;
		SafePointer<UI::Format::InterfaceTemplateImage> image = UI::Markup::CompileInterface(idesc, edesc);
		if (edesc.status != UI::Markup::ErrorClass::OK) {
			if (!state.silent) {
				string message;
				if (edesc.status == UI::Markup::ErrorClass::ObjectRedifinition) message = Localized(401);
				else if (edesc.status == UI::Markup::ErrorClass::SourceAccess) message = FormatString(Localized(402), edesc.desc);
				else if (edesc.status == UI::Markup::ErrorClass::UndefinedObject) message = Localized(403);
				else if (edesc.status == UI::Markup::ErrorClass::UnexpectedLexem) message = Localized(404);
				else if (edesc.status == UI::Markup::ErrorClass::NumericConstantTypeMismatch) message = Localized(405);
				else if (edesc.status == UI::Markup::ErrorClass::MainInvalidToken) message = Localized(406);
				else if (edesc.status == UI::Markup::ErrorClass::InvalidSystemColor) message = Localized(407);
				else if (edesc.status == UI::Markup::ErrorClass::InvalidProperty) message = Localized(408);
				else if (edesc.status == UI::Markup::ErrorClass::InvalidLocaleIdentifier) message = Localized(409);
				else if (edesc.status == UI::Markup::ErrorClass::InvalidEffect) message = Localized(410);
				else if (edesc.status == UI::Markup::ErrorClass::InvalidConstantType) message = Localized(411);
				else if (edesc.status == UI::Markup::ErrorClass::IncludedInvalidToken) message = FormatString(Localized(412), edesc.desc);
				else message = Localized(413);
				PrintSourceReferenceError(message, edesc.position, edesc.length, false);
			}
			return 1;
		}
		for (auto & w : collector->warnings) {
			if (!state.silent && state.enable_warnings) {
				string message;
				if (w.status == UI::Markup::WarningClass::InvalidControlParent) {
					auto classes = w.desc.Split(L',');
					message = FormatString(Localized(501), classes[0], classes[1]);
				} else if (w.status == UI::Markup::WarningClass::UnknownPlatformName) {
					message = FormatString(Localized(502), w.desc);
				} else message = Localized(505);
				if (w.position >= 0) PrintSourceReferenceError(message, w.position, w.length, state.warnings_as_errors ? false : true);
				else PrintReferencelessError(message, state.warnings_as_errors ? false : true);
			}
			if (state.warnings_as_errors) return 1;
		}
		try {
			Verifyier verifyer;
			bool verification_undone = false;
			UI::InterfaceTemplate interface;
			UI::InterfaceTemplate preloaded_interface;
			SafePointer<UI::Format::InterfaceTemplateImage> clone = image->Clone();
			clone->Specialize(L"", state.system, 0.0);
			if (state.preload.Length()) {
				for (auto & p : state.preload) {
					try {
						FileStream preload_stream(p, AccessRead, OpenExisting);
						SafePointer<UI::Format::InterfaceTemplateImage> preload_image = new UI::Format::InterfaceTemplateImage(&preload_stream, L"", system, 0.0);
						preload_image->Compile(preloaded_interface);
						clone->Compile(interface, preloaded_interface, 0, &verifyer, &verifyer);
					} catch (...) {
						verification_undone = true;
						verifyer.status_fine = false;
						if (!state.silent && state.enable_warnings) PrintInformation(FormatString(Localized(606), p));
					}
				}
			} else clone->Compile(interface, 0, &verifyer, &verifyer);
			if (!verifyer.status_fine) {
				if (!verification_undone && !state.silent) {
					string message;
					if (state.enable_warnings) message = Localized(503); else message = Localized(504);
					PrintReferencelessError(message, true);
				}
			}
		} catch (...) { if (!state.silent) PrintInformation(Localized(607)); }
		try {
			if (!state.write_color_tables) {
				for (int i = 0; i < image->Assets.Length(); i++) for (int j = 0; j < image->Assets[i].Colors.Length(); j++) image->Assets[i].Colors[j].Name = L"";
			}
			uint32 flags = state.write_string_tables ? UI::Format::EncodeFlags::EncodeStringNames : 0;
			FileStream dest(state.output, AccessReadWrite, CreateAlways);
			image->Encode(&dest, flags);
		} catch (...) {
			if (!state.silent) {
				console.SetTextColor(IO::ConsoleColor::Red);
				console.WriteLine(FormatString(Localized(151), state.output));
				console.SetTextColor(IO::ConsoleColor::Default);
			}
			return 1;
		}
		if (!state.silent && state.time_estimations) {
			console.SetTextColor(IO::ConsoleColor::Green);
			console.WriteLine(FormatString(Localized(152), GetTimerValue() - time));
			console.SetTextColor(IO::ConsoleColor::Default);
		}
	} else {
		if (!state.silent) try {
			auto length = Localized(100).ToInt32();
			for (int i = 0; i < length; i++) console << Localized(101 + i) << LineFeedSequence;
		} catch (...) {}
	}
	return 0;
}