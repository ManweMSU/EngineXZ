#include <EngineRuntime.h>

#include "../xexec/xx_com.h"
#include "../xw/xw_decompiler.h"

using namespace Engine;
using namespace Engine::IO;
using namespace Engine::IO::ConsoleControl;
using namespace Engine::Streaming;
using namespace Engine::Storage;

Console console;

struct {
	SafePointer<StringTable> localization;
	Array<string> module_search_paths_v = Array<string>(0x10);
	Array<string> module_search_paths_w = Array<string>(0x10);
	bool silent = false;
	bool nologo = false;
	bool assembly_version_control = false;
	bool human_readable = false;
	bool human_readable_c = false;
	bool interface_for_c = false;
	uint bundle_flags = 0;
	uint system_flags = 0;
	string input;
	string output;
	string output_path;
} state;

string Localized(int id)
{
	try {
		if (!state.localization) throw InvalidStateException();
		return state.localization->GetString(id);
	} catch (...) { return FormatString(L"LOC(%0)", id); }
}
string ErrorDescription(XW::DecompilerStatus status)
{
	try {
		if (!state.localization) throw InvalidStateException();
		return state.localization->GetString(400 + int(status));
	} catch (...) { return Localized(302); }
}
uint ProcessBundleWords(const string & words)
{
	uint result = 0;
	auto ww = words.Split(L',');
	for (auto & w : ww) {
		if (w == L"purus") result |= XW::DecompilerFlagRawOutput;
		else if (w == L"egsu") result |= XW::DecompilerFlagBundleToEGSU;
		else if (w == L"xo") result |= XW::DecompilerFlagBundleToXO;
		else if (w == L"c") result |= XW::DecompilerFlagBundleToCXX;
		else throw InvalidFormatException();
	}
	return result;
}
uint ProcessSystemWords(const string & words)
{
	uint result = 0;
	auto ww = words.Split(L',');
	for (auto & w : ww) {
		if (w == L"maxime") result |= XW::DecompilerFlagProduceMaximas;
		else if (w == L"locale") result |= XW::DecompilerFlagForThisMachine;
		else if (w == L"hlsl") result |= XW::DecompilerFlagProduceHLSL;
		else if (w == L"msl") result |= XW::DecompilerFlagProduceMSL;
		else if (w == L"glsl") result |= XW::DecompilerFlagProduceGLSL;
		else throw InvalidFormatException();
	}
	return result;
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
				} else if (arg[j] == L'O') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					if (state.output.Length() || state.output_path.Length()) {
						console << TextColor(12) << Localized(202) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					state.output_path = ExpandPath(args->ElementAt(i));
				} else if (arg[j] == L'S') {
					state.silent = true;
				} else if (arg[j] == L'a') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					if (args->ElementAt(i) == L"nulle") {
						state.assembly_version_control = false;
					} else if (args->ElementAt(i) == L"modera") {
						state.assembly_version_control = true;
					} else {
						console << TextColor(12) << Localized(205) << TextColorDefault() << LineFeed();
						throw Exception();
					}
				} else if (arg[j] == L'c') {
					state.interface_for_c = true;
				} else if (arg[j] == L'h') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					if (args->ElementAt(i) == L"g:humane") {
						state.human_readable = true;
					} else if (args->ElementAt(i) == L"g:compacte") {
						state.human_readable = false;
					} else if (args->ElementAt(i) == L"c:humane") {
						state.human_readable_c = true;
					} else if (args->ElementAt(i) == L"c:compacte") {
						state.human_readable_c = false;
					} else {
						console << TextColor(12) << Localized(205) << TextColorDefault() << LineFeed();
						throw Exception();
					}
				} else if (arg[j] == L'l') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					try {
						state.bundle_flags = ProcessBundleWords(args->ElementAt(i));
					} catch (...) {
						console << TextColor(12) << Localized(205) << TextColorDefault() << LineFeed();
						throw;
					}
				} else if (arg[j] == L'o') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					if (state.output.Length() || state.output_path.Length()) {
						console << TextColor(12) << Localized(202) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					state.output = ExpandPath(args->ElementAt(i));
				} else if (arg[j] == L's') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					try {
						state.system_flags = ProcessSystemWords(args->ElementAt(i));
					} catch (...) {
						console << TextColor(12) << Localized(205) << TextColorDefault() << LineFeed();
						throw;
					}
				} else if (arg[j] == L'v') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					auto path = ExpandPath(args->ElementAt(i));
					state.module_search_paths_v << path;
				} else if (arg[j] == L'w') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					auto path = ExpandPath(args->ElementAt(i));
					state.module_search_paths_w << path;
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
	if (state.bundle_flags & XW::DecompilerFlagBundleToCXX) state.interface_for_c = true;
}
void PrintDecompilerError(XW::DecompilerStatusDesc & desc)
{
	console.SetTextColor(12);
	console.WriteLine(Localized(301));
	console.WriteLine(ErrorDescription(desc.status));
	if (desc.language != XW::ShaderLanguage::Unknown) {
		if (desc.language == XW::ShaderLanguage::HLSL) console.WriteLine(FormatString(Localized(303), Localized(306)));
		if (desc.language == XW::ShaderLanguage::MSL) console.WriteLine(FormatString(Localized(303), Localized(307)));
		if (desc.language == XW::ShaderLanguage::GLSL) console.WriteLine(FormatString(Localized(303), Localized(308)));
	}
	if (desc.object.Length()) console.WriteLine(FormatString(Localized(304), desc.object));
	if (desc.addendum.Length()) console.WriteLine(FormatString(Localized(305), desc.addendum));
	console.SetTextColor(-1);
}

int Main(void)
{
	try {
		Assembly::CurrentLocale = Assembly::GetCurrentUserLocale();
		auto root = Path::GetDirectory(GetExecutablePath());
		SafePointer<Registry> xw_conf = XX::LoadConfiguration(root + L"/xw.ini");
		state.assembly_version_control = xw_conf->GetValueBoolean(L"ModeraVersiones");
		state.human_readable = xw_conf->GetValueBoolean(L"HumaneGP");
		state.human_readable_c = xw_conf->GetValueBoolean(L"HumaneC");
		state.interface_for_c = xw_conf->GetValueBoolean(L"TituliC");
		state.bundle_flags = ProcessBundleWords(xw_conf->GetValueString(L"Liber"));
		state.system_flags = ProcessSystemWords(xw_conf->GetValueString(L"Systemae"));
		try {
			auto core = xw_conf->GetValueString(L"XE");
			if (core.Length()) XX::IncludeComponent(&state.module_search_paths_v, &state.module_search_paths_w, root + L"/" + core);
		} catch (...) {}
		try {
			auto store = xw_conf->GetValueString(L"Entheca");
			if (store.Length()) XX::IncludeStoreIntegration(&state.module_search_paths_v, &state.module_search_paths_w, root + L"/" + store);
		} catch (...) {}
		auto language_override = xw_conf->GetValueString(L"Lingua");
		if (language_override.Length()) Assembly::CurrentLocale = language_override;
		auto localizations = xw_conf->GetValueString(L"Locale");
		if (localizations.Length()) {
			try {
				SafePointer<Stream> table = new FileStream(root + L"/" + localizations + L"/" + Assembly::CurrentLocale + L".ecst", AccessRead, OpenExisting);
				state.localization = new StringTable(table);
			} catch (...) {}
			if (!state.localization) {
				auto language_default = xw_conf->GetValueString(L"LinguaDefalta");
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
			SafePointer<XW::IDecompilerCallback> library_callback = XW::CreateDecompilerCallback(
				state.module_search_paths_v.GetBuffer(), state.module_search_paths_v.Length(),
				state.module_search_paths_w.GetBuffer(), state.module_search_paths_w.Length(), 0);
			SafePointer<XW::IDecompilerCallback> main_callback;
			XW::DecompileDesc desc;
			try {
				SafePointer<Streaming::FileStream> stream = new Streaming::FileStream(state.input, Streaming::AccessRead, Streaming::OpenExisting);
				desc.root_module = stream->ReadAll();
				string src_dir = IO::Path::GetDirectory(state.input);
				main_callback = XW::CreateDecompilerCallback(0, 0, &src_dir, 1, library_callback);
			} catch (...) {
				if (!state.silent) {
					console.SetTextColor(12);
					console.WriteLine(FormatString(Localized(501), state.input));
					console.SetTextColor(-1);
				}
				return 0x3E;
			}
			desc.callback = main_callback;
			desc.flags = 0;
			if (state.assembly_version_control) desc.flags |= XW::DecompilerFlagVersionControl;
			if (state.human_readable) desc.flags |= XW::DecompilerFlagHumanReadable;
			if (state.human_readable_c) desc.flags |= XW::DecompilerFlagHumanReadableC;
			if (state.interface_for_c) desc.flags |= XW::DecompilerFlagProduceCXX;
			desc.flags |= state.bundle_flags | state.system_flags;
			XW::Decompile(desc);
			if (desc.status.status == XW::DecompilerStatus::Success) {
				if (!desc.output_objects.Length()) {
					if (!state.silent) {
						console.SetTextColor(14);
						console.WriteLine(Localized(503));
						console.SetTextColor(-1);
					}
					return 0x3D;
				}
				int rv = 0;
				if (state.output.Length()) {
					if (desc.output_objects.Length() > 1) {
						if (!state.silent) {
							console.SetTextColor(14);
							console.WriteLine(Localized(504));
							console.WriteLine(Localized(505));
							console.SetTextColor(-1);
						}
						rv = 0x3C;
					}
					auto name = ExpandPath(state.output);
					try {
						SafePointer<FileStream> out = new FileStream(name, AccessWrite, CreateAlways);
						desc.output_objects[0].GetPortionData()->CopyTo(out);
						Unix::SetFileAccessRights(out->Handle(), Unix::AccessRightRegular, Unix::AccessRightRegular, Unix::AccessRightRegular);
					} catch (...) {
						if (!state.silent) {
							console.SetTextColor(12);
							console.WriteLine(FormatString(Localized(502), name));
							console.SetTextColor(-1);
						}
						return 0x3B;
					}
				} else {
					if (!state.output_path.Length()) {
						state.output_path = ExpandPath(Path::GetDirectory(state.input) + L"/" + Path::GetFileNameWithoutExtension(state.input));
					}
					auto last = state.output_path[state.output_path.Length() - 1];
					if (last == L'/' || last == L'\\') {
						state.output_path = ExpandPath(state.output_path + L"/" + Path::GetFileNameWithoutExtension(state.input));
					}
					for (auto & o : desc.output_objects) {
						auto name = state.output_path;
						if (o.GetPortionPostfix().Length()) name += L"." + o.GetPortionPostfix();
						if (o.GetPortionExtension().Length()) name += L"." + o.GetPortionExtension();
						try {
							SafePointer<FileStream> out = new FileStream(name, AccessWrite, CreateAlways);
							o.GetPortionData()->CopyTo(out);
							Unix::SetFileAccessRights(out->Handle(), Unix::AccessRightRegular, Unix::AccessRightRegular, Unix::AccessRightRegular);
						} catch (...) {
							if (!state.silent) {
								console.SetTextColor(12);
								console.WriteLine(FormatString(Localized(502), name));
								console.SetTextColor(-1);
							}
							return 0x3B;
						}
					}
				}
				return rv;
			} else {
				if (!state.silent) PrintDecompilerError(desc.status);
				return int(desc.status.status);
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