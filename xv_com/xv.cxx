#include <EngineRuntime.h>

#include "../xexec/xx_com.h"
#include "../xv/xv_compiler.h"

using namespace Engine;
using namespace Engine::IO;
using namespace Engine::IO::ConsoleControl;
using namespace Engine::Streaming;
using namespace Engine::Storage;

Console console;

struct {
	SafePointer<StringTable> localization;
	Array<string> module_search_paths = Array<string>(0x10);
	bool silent = false;
	bool nologo = false;
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
string ErrorDescription(XV::CompilerStatus status)
{
	try {
		if (!state.localization) throw InvalidStateException();
		return state.localization->GetString(400 + int(status));
	} catch (...) { return Localized(303); }
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
				} else if (arg[j] == L'l') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					state.module_search_paths << ExpandPath(args->ElementAt(i));
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
void PrintCompilerError(XV::CompilerStatusDesc & desc)
{
	console.SetTextColor(12);
	console.WriteLine(FormatString(Localized(301), ErrorDescription(desc.status)));
	if (desc.error_line_no > 0) console.WriteLine(FormatString(Localized(302), desc.error_line_no));
	console.SetTextColor(-1);
	if (desc.error_line_pos >= 0 && desc.error_line_len > 0) {
		console.WriteLine(desc.error_line);
		console.SetTextColor(4);
		if (desc.error_line_pos >= 0 && desc.error_line_len > 0) {
			console.Write(string(L' ', desc.error_line_pos));
			console.Write(string(L'~', desc.error_line_len));
			console.LineFeed();
		}
		console.SetTextColor(-1);
	} else if (desc.error_line.Length()) {
		console.SetTextColor(12);
		console.WriteLine(FormatString(Localized(304), desc.error_line));
		console.SetTextColor(-1);
	}
}

int Main(void)
{
	try {
		Codec::InitializeDefaultCodecs();
		Assembly::CurrentLocale = Assembly::GetCurrentUserLocale();
		auto root = Path::GetDirectory(GetExecutablePath());
		SafePointer<Registry> xv_conf = XX::LoadConfiguration(root + L"/xv.ini");
		try {
			auto core = xv_conf->GetValueString(L"XE");
			if (core) XX::IncludeComponent(state.module_search_paths, root + L"/" + core);
		} catch (...) {}
		try {
			auto store = xv_conf->GetValueString(L"Entheca");
			if (store.Length()) XX::IncludeStoreIntegration(state.module_search_paths, root + L"/" + store);
		} catch (...) {}
		auto language_override = xv_conf->GetValueString(L"Lingua");
		if (language_override.Length()) Assembly::CurrentLocale = language_override;
		auto localizations = xv_conf->GetValueString(L"Locale");
		if (localizations.Length()) {
			try {
				SafePointer<Stream> table = new FileStream(root + L"/" + localizations + L"/" + Assembly::CurrentLocale + L".ecst", AccessRead, OpenExisting);
				state.localization = new StringTable(table);
			} catch (...) {}
			if (!state.localization) {
				auto language_default = xv_conf->GetValueString(L"LinguaDefalta");
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
			string output;
			SafePointer<XV::ICompilerCallback> callback = XV::CreateCompilerCallback(0, 0, state.module_search_paths.GetBuffer(), state.module_search_paths.Length(), 0);
			if (state.output.Length()) output = L"?" + state.output;
			else if (state.output_path.Length()) output = state.output_path;
			else output = Path::GetDirectory(state.input);
			XV::CompilerStatusDesc desc;
			XV::CompileModule(state.input, output, callback, desc);
			if (desc.status != XV::CompilerStatus::Success) {
				if (!state.silent) PrintCompilerError(desc);
				return int(desc.status);
			}
			return 0;
		} catch (...) { return 0x3F; }
	} else {
		if (!state.silent) try {
			auto length = Localized(100).ToInt32();
			for (int i = 0; i < length; i++) console << Localized(101 + i) << LineFeedSequence;
		} catch (...) {}
		return 0;
	}
}