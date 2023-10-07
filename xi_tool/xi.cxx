#include <EngineRuntime.h>

#include "../ximg/xi_resources.h"
#include "../ximg/xi_pret.h"
#include "../xexec/xx_com.h"

using namespace Engine;
using namespace Engine::IO;
using namespace Engine::IO::ConsoleControl;
using namespace Engine::Streaming;
using namespace Engine::Storage;

Console console;

constexpr uint InspectSymbols		= 0x001;
constexpr uint InspectDependencies	= 0x002;
constexpr uint InspectModuleStamp	= 0x004;
constexpr uint InspectMetadata		= 0x008;
constexpr uint InspectIcon			= 0x010;
constexpr uint InspectResources		= 0x020;

struct {
	SafePointer<StringTable> localization;
	bool silent = false;
	bool nologo = false;
	uint inspect_mask = 0;
	string input;
	string output;
	Array<XI::PretranslateDesc> pret_list = Array<XI::PretranslateDesc>(0x10);
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
				if (arg[j] == L'N') {
					state.nologo = true;
				} else if (arg[j] == L'S') {
					state.silent = true;
				} else if (arg[j] == L'i') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					auto & arg2 = args->ElementAt(i);
					for (int k = 0; k < arg2.Length(); k++) {
						if (arg2[k] == L's') state.inspect_mask |= InspectSymbols;
						else if (arg2[k] == L'd') state.inspect_mask |= InspectDependencies;
						else if (arg2[k] == L't') state.inspect_mask |= InspectModuleStamp;
						else if (arg2[k] == L'm') state.inspect_mask |= InspectMetadata;
						else if (arg2[k] == L'i') state.inspect_mask |= InspectIcon;
						else if (arg2[k] == L'r') state.inspect_mask |= InspectResources; else {
							console << TextColor(12) << Localized(205) << TextColorDefault() << LineFeed();
							throw Exception();
						}
					}
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
				} else if (arg[j] == L't') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					auto & arg2 = args->ElementAt(i);
					auto prt = arg2.Split(L'-');
					XI::PretranslateDesc desc;
					if (prt.Length() != 2) {
						console << TextColor(12) << Localized(205) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					if (prt[0] == L"win") desc.conv = XA::CallingConvention::Windows;
					else if (prt[0] == L"mac") desc.conv = XA::CallingConvention::Unix; else {
						console << TextColor(12) << Localized(205) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					if (prt[1] == L"x86") desc.arch = Platform::X86;
					else if (prt[1] == L"x64") desc.arch = Platform::X64;
					else if (prt[1] == L"arm") desc.arch = Platform::ARM;
					else if (prt[1] == L"arm64") desc.arch = Platform::ARM64; else {
						console << TextColor(12) << Localized(205) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					state.pret_list << desc;
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
void TableView(const string ** columns, int num_cols, int num_items)
{
	Array<int> widths(1);
	widths.SetLength(num_cols);
	ZeroMemory(widths.GetBuffer(), widths.Length() * sizeof(int));
	for (int i = 0; i < num_items; i++) {
		for (int c = 0; c < num_cols; c++) {
			int len = columns[c][i].Length();
			if (len > widths[c]) widths[c] = len;
		}
	}
	for (int i = 0; i < num_items; i++) {
		for (int c = 0; c < num_cols; c++) {
			int len = columns[c][i].Length();
			console << columns[c][i];
			if (c == num_cols - 1) console << LineFeed();
			else console << string(L' ', widths[c] + 1 - len);
		}
	}
}

int Main(void)
{
	try {
		Codec::InitializeDefaultCodecs();
		Assembly::CurrentLocale = Assembly::GetCurrentUserLocale();
		auto root = Path::GetDirectory(GetExecutablePath());
		SafePointer<Registry> xi_conf = XX::LoadConfiguration(root + L"/xi.ini");
		auto language_override = xi_conf->GetValueString(L"Lingua");
		if (language_override.Length()) Assembly::CurrentLocale = language_override;
		auto localizations = xi_conf->GetValueString(L"Locale");
		if (localizations.Length()) {
			try {
				SafePointer<Stream> table = new FileStream(root + L"/" + localizations + L"/" + Assembly::CurrentLocale + L".ecst", AccessRead, OpenExisting);
				state.localization = new StringTable(table);
			} catch (...) {}
			if (!state.localization) {
				auto language_default = xi_conf->GetValueString(L"LinguaDefalta");
				try {
					SafePointer<Stream> table = new FileStream(root + L"/" + localizations + L"/" + language_default + L".ecst", AccessRead, OpenExisting);
					state.localization = new StringTable(table);
				} catch (...) {}
			}
		}
		ProcessCommandLine();
	} catch (...) { return 0x40; }
	if (!state.nologo && !state.silent) {
		console << Localized(1) << LineFeedSequence;
		console << Localized(2) << LineFeedSequence;
		console << FormatString(Localized(3), ENGINE_VI_APPVERSION) << LineFeedSequence << LineFeedSequence;
	}
	if (state.input.Length()) {
		try {
			SafePointer<XI::Module> module;
			try {
				SafePointer<Stream> stream = new FileStream(state.input, AccessRead, OpenExisting);
				try { module = new XI::Module(stream); } catch (...) {
					console << TextColor(12) << Localized(302) << TextColorDefault() << LineFeed();
					return 2;
				}
			} catch (...) {
				console << TextColor(12) << Localized(301) << TextColorDefault() << LineFeed();
				return 1;
			}
			if (state.inspect_mask & InspectSymbols) {
				console << Localized(401) << LineFeed();
				Volumes::Set<string> smbls;
				for (auto & c : module->literals) smbls.AddElement(c.key + Localized(402));
				for (auto & c : module->classes) smbls.AddElement(c.key + Localized(403));
				for (auto & c : module->variables) smbls.AddElement(c.key + Localized(404));
				for (auto & c : module->functions) smbls.AddElement(c.key + Localized(405));
				for (auto & c : module->aliases) smbls.AddElement(c.key + Localized(406));
				for (auto & c : module->prototypes) smbls.AddElement(c.key + Localized(407));
				for (auto & s : smbls) console << s << LineFeed();
			}
			if (state.inspect_mask & InspectDependencies) {
				console << Localized(408) << LineFeed();
				for (auto & d : module->modules_depends_on) console << d << LineFeed();
			}
			if (state.inspect_mask & InspectModuleStamp) {
				string ss;
				if (module->subsystem == XI::Module::ExecutionSubsystem::ConsoleUI) ss = Localized(410);
				else if (module->subsystem == XI::Module::ExecutionSubsystem::GUI) ss = Localized(411);
				else if (module->subsystem == XI::Module::ExecutionSubsystem::NoUI) ss = Localized(412);
				else if (module->subsystem == XI::Module::ExecutionSubsystem::Library) ss = Localized(413);
				console << FormatString(Localized(409), module->module_import_name,
					module->assembler_name, module->assembler_version.major, module->assembler_version.minor,
					module->assembler_version.subver, module->assembler_version.build, ss) << LineFeed();
			}
			if (state.inspect_mask & InspectMetadata) try {
				SafePointer< Volumes::Dictionary<string, string> > meta = XI::LoadModuleMetadata(module->resources);
				console << Localized(414) << LineFeed();
				Array<string> keys(0x10), values(0x10);
				for (auto & m : meta->Elements()) { keys << m.key; values << m.value; }
				const string * table[] = { keys.GetBuffer(), values.GetBuffer() };
				TableView(table, 2, keys.Length());
			} catch (...) {}
			if (state.inspect_mask & InspectIcon) try {
				Array<Color> palette(0x10);
				for (int i = 0; i < 16; i++) palette << Color(Math::ColorF(static_cast<Math::StandardColor>(i)));
				SafePointer<Codec::Frame> icon = XI::LoadModuleIcon(module->resources, 1, Point(16, 16));
				if (!icon) throw Exception();
				console << Localized(415) << LineFeed();
				for (int y = 0; y < icon->GetHeight(); y++) {
					for (int x = 0; x < icon->GetWidth(); x++) {
						auto color = Color(icon->ReadPixel(x, y));
						if (color.a < 128) console << L"  "; else {
							int index = -1;
							int dist;
							for (int i = 0; i < palette.Length(); i++) {
								int ld = abs(int(color.r) - palette[i].r) + abs(int(color.g) - palette[i].g) + abs(int(color.b) - palette[i].b);
								if (index < 0 || ld < dist) { index = i; dist = ld; }
							}
							console << TextBackground(index) << L"  " << TextBackgroundDefault();
						}
					}
					console << LineFeed();
				}
			} catch (...) {}
			if (state.inspect_mask & InspectResources) {
				console << Localized(416) << LineFeed();
				Array<string> types(0x10), ids(0x10), sizes(0x10);
				types << Localized(417);
				ids << Localized(418);
				sizes << Localized(419);
				for (auto & r : module->resources) {
					auto prt = r.key.Split(L':');
					if (prt.Length() != 2) continue;
					types << prt[0];
					ids << prt[1];
					sizes << string(r.value->Length());
				}
				const string * table[] = { types.GetBuffer(), ids.GetBuffer(), sizes.GetBuffer() };
				TableView(table, 3, types.Length());
			}
			if (state.pret_list.Length()) {
				try {
					XI::PretranslateModule(*module, state.pret_list.GetBuffer(), state.pret_list.Length());
				} catch (InvalidArgumentException &) {
					console << TextColor(12) << Localized(303) << TextColorDefault() << LineFeed();
					return 3;
				}
				if (!state.output.Length()) {
					auto dir = Path::GetDirectory(state.input);
					auto name = Path::GetFileNameWithoutExtension(state.input);
					auto ext = Path::GetExtension(state.input);
					state.output = ExpandPath(dir + L"/" + name + L"-praet." + ext);
				}
				try {
					SafePointer<Stream> stream = new FileStream(state.output, AccessReadWrite, CreateAlways);
					module->Save(stream);
				} catch (...) {
					console << TextColor(12) << Localized(304) << TextColorDefault() << LineFeed();
					return 4;
				}
			}
		} catch (...) { return 0x3F; }
	} else {
		if (!state.silent) try {
			auto length = Localized(100).ToInt32();
			for (int i = 0; i < length; i++) console << Localized(101 + i) << LineFeedSequence;
		} catch (...) {}
		return 0;
	}
}