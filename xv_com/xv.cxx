#include <EngineRuntime.h>

#include "../xexec/xx_com.h"
#include "../xv/xv_compiler.h"
#include "../xcon/common/xc_channel.h"

using namespace Engine;
using namespace Engine::IO;
using namespace Engine::IO::ConsoleControl;
using namespace Engine::Streaming;
using namespace Engine::Storage;

Console console;

struct {
	SafePointer<StringTable> localization;
	Volumes::Dictionary<string, string> defines;
	Array<string> module_search_paths_v = Array<string>(0x10);
	Array<string> module_search_paths_w = Array<string>(0x10);
	Array<string> documentation_list = Array<string>(0x10);
	Volumes::Set<string> import_list;
	uint language_mode = 1;
	bool silent = false;
	bool nologo = false;
	bool launch = false;
	bool version_control = false;
	bool direct_mode = false;
	bool debug_mode = false;
	bool assembly_version_control;
	string input;
	string output;
	string output_path;
	string xx_path;
	Array<string> launch_args = Array<string>(0x10);
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
				if (arg[j] == L'D') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					auto defname = args->ElementAt(i);
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					auto defvalue = args->ElementAt(i);
					auto defvaluepref = defvalue.Fragment(0, 2);
					if (defvaluepref == L"C:") {
						state.defines.Append(defname, defvalue.Fragment(2, -1));
					} else if (defvaluepref == L"T:") {
						int sep = defvalue.FindLast(L':');
						auto fn = defvalue.Fragment(2, sep - 2);
						try {
							SafePointer<Registry> reg = XX::LoadConfiguration(fn);
							auto key = defvalue.Fragment(sep + 1, -1);
							state.defines.Append(defname, reg->GetValueString(key));
						} catch (...) {
							console << TextColor(12) << Localized(401) << L": " << fn << TextColorDefault() << LineFeed();
							throw Exception();
						}
					} else if (defvaluepref == L"S:") {
						int sep = defvalue.FindLast(L':');
						auto fn = defvalue.Fragment(2, sep - 2);
						try {
							SafePointer<Stream> stream = new FileStream(fn, AccessRead, OpenExisting);
							SafePointer<TextReader> rdr = new TextReader(stream);
							int no = defvalue.Fragment(sep + 1, -1).ToUInt32();
							int current = 0;
							string value;
							while (!rdr->EofReached()) {
								auto line = rdr->ReadLine();
								current++;
								if (current == no) { value = line; break; }
							}
							state.defines.Append(defname, value);
						} catch (...) {
							console << TextColor(12) << Localized(401) << L": " << fn << TextColorDefault() << LineFeed();
							throw Exception();
						}
					} else {
						console << TextColor(12) << Localized(204) << TextColorDefault() << LineFeed();
						throw Exception();
					}
				} else if (arg[j] == L'L') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					if (state.language_mode != 1) {
						console << TextColor(12) << Localized(206) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					auto & lang = args->ElementAt(i);
					if (string::CompareIgnoreCase(lang, L"v") == 0) state.language_mode = XV::CompilerFlagLanguageV;
					else if (string::CompareIgnoreCase(lang, L"xv") == 0) state.language_mode = XV::CompilerFlagLanguageV;
					else if (string::CompareIgnoreCase(lang, L"w") == 0) state.language_mode = XV::CompilerFlagLanguageW;
					else if (string::CompareIgnoreCase(lang, L"xw") == 0) state.language_mode = XV::CompilerFlagLanguageW;
					else {
						console << TextColor(12) << Localized(207) << TextColorDefault() << LineFeed();
						throw Exception();
					}
				} else if (arg[j] == L'M') {
					state.debug_mode = true;
				} else if (arg[j] == L'N') {
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
				} else if (arg[j] == L'P') {
					state.import_list.RemoveElement(L"canonicalis");
				} else if (arg[j] == L'S') {
					state.silent = true;
				} else if (arg[j] == L'V') {
					state.version_control = true;
				} else if (arg[j] == L'X') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					state.xx_path = args->ElementAt(i);
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
						console << TextColor(12) << Localized(204) << TextColorDefault() << LineFeed();
						throw Exception();
					}
				} else if (arg[j] == L'd') {
					state.direct_mode = true;
					state.launch = true;
				} else if (arg[j] == L'i') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					state.import_list.AddElement(args->ElementAt(i));
				} else if (arg[j] == L'l') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					auto path = ExpandPath(args->ElementAt(i));
					state.module_search_paths_v << path;
					state.module_search_paths_w << path;
				} else if (arg[j] == L'm') {
					i++; if (i >= args->Length()) {
						console << TextColor(12) << Localized(203) << TextColorDefault() << LineFeed();
						throw Exception();
					}
					state.documentation_list << ExpandPath(args->ElementAt(i));
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
				} else if (arg[j] == L'r') {
					state.launch = true;
					while (true) {
						i++;
						if (i >= args->Length()) break;
						state.launch_args << args->ElementAt(i);
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
	if (state.language_mode == 1 && state.input.Length()) {
		auto ext = IO::Path::GetExtension(state.input).LowerCase();
		if (ext == L"w" || ext == L"xw") state.language_mode = XV::CompilerFlagLanguageW;
		else state.language_mode = XV::CompilerFlagLanguageV;
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
		state.import_list.AddElement(L"canonicalis");
		Codec::InitializeDefaultCodecs();
		Assembly::CurrentLocale = Assembly::GetCurrentUserLocale();
		auto root = Path::GetDirectory(GetExecutablePath());
		SafePointer<Registry> xv_conf = XX::LoadConfiguration(root + L"/xv.ini");
		state.assembly_version_control = xv_conf->GetValueBoolean(L"ModeraVersiones");
		state.xx_path = xv_conf->GetValueString(L"XX");
		try {
			auto core = xv_conf->GetValueString(L"XE");
			if (core.Length()) XX::IncludeComponent(&state.module_search_paths_v, &state.module_search_paths_w, root + L"/" + core);
		} catch (...) {}
		try {
			auto store = xv_conf->GetValueString(L"Entheca");
			if (store.Length()) XX::IncludeStoreIntegration(&state.module_search_paths_v, &state.module_search_paths_w, root + L"/" + store);
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
			SafePointer<XV::ICompilerCallback> callback, local_callback;
			if (state.language_mode == XV::CompilerFlagLanguageV) {
				callback = XV::CreateCompilerCallback(0, 0, state.module_search_paths_v.GetBuffer(), state.module_search_paths_v.Length(), 0, state.language_mode);
			} else if (state.language_mode == XV::CompilerFlagLanguageW) {
				callback = XV::CreateCompilerCallback(0, 0, state.module_search_paths_w.GetBuffer(), state.module_search_paths_w.Length(), 0, state.language_mode);
			}
			string output;
			if (state.output.Length()) output = L"?" + state.output;
			else if (state.output_path.Length()) output = state.output_path;
			else output = Path::GetDirectory(state.input);
			XV::CompileDesc desc;
			Array<uint32> input_module_string(0x1000);
			try {
				SafePointer<Streaming::FileStream> stream = new Streaming::FileStream(state.input, Streaming::AccessRead, Streaming::OpenExisting);
				if (state.version_control && state.output.Length()) try {
					SafePointer<Streaming::FileStream> dest = new Streaming::FileStream(state.output, Streaming::AccessRead, Streaming::OpenExisting);
					if (DateTime::GetFileAlterTime(stream->Handle()) <= DateTime::GetFileAlterTime(dest->Handle())) return 0;
				} catch (...) {}
				SafePointer<Streaming::TextReader> reader = new Streaming::TextReader(stream);
				while (!reader->EofReached()) {
					auto code = reader->ReadChar();
					if (code != 0xFFFFFFFF) input_module_string << code;
				}
				string src_dir = IO::Path::GetDirectory(state.input);
				local_callback = CreateCompilerCallback(&src_dir, 1, &src_dir, 1, callback, state.language_mode);
			} catch (...) {
				desc.status.status = XV::CompilerStatus::FileAccessFailure;
				desc.status.error_line = state.input;
				desc.status.error_line_pos = desc.status.error_line_no = desc.status.error_line_len = 0;
				if (!state.silent) PrintCompilerError(desc.status);
				return int(desc.status.status);
			}
			desc.flags = XV::CompilerFlagSystemConsole | XV::CompilerFlagMakeModule | (state.language_mode & XV::CompilerFlagLanguageMask);
			if (state.documentation_list.Length()) desc.flags |= XV::CompilerFlagMakeManual;
			if (state.assembly_version_control) desc.flags |= XV::CompilerFlagVersionControl;
			if (state.debug_mode) {
				desc.flags |= XV::CompilerFlagDebugData;
				desc.source_full_path = state.input;
			}
			desc.module_name = IO::Path::GetFileNameWithoutExtension(state.input);
			desc.input = &input_module_string;
			desc.meta = 0;
			desc.callback = local_callback;
			desc.imports = state.import_list;
			desc.defines = state.defines;
			XV::Compile(desc);
			if (desc.status.status == XV::CompilerStatus::Success && !state.direct_mode) {
				if (output[0] == L'?') output = output.Fragment(1, -1);
				else output = IO::ExpandPath(output + L"/" + desc.output_module->GetOutputModuleName() + L"." + desc.output_module->GetOutputModuleExtension());
				try {
					SafePointer<Streaming::Stream> stream = new Streaming::FileStream(output, Streaming::AccessWrite, Streaming::CreateAlways);
					auto data = desc.output_module->GetOutputModuleData();
					data->CopyTo(stream);
				} catch (...) {
					desc.status.status = XV::CompilerStatus::FileAccessFailure;
					desc.status.error_line = output;
					desc.status.error_line_pos = desc.status.error_line_no = desc.status.error_line_len = 0;
					if (!state.silent) PrintCompilerError(desc.status);
					return int(desc.status.status);
				}
			}
			if (state.documentation_list.Length() && desc.output_volume) {
				SafePointer<XV::ManualVolume> base, deletes;
				try {
					SafePointer<Stream> base_stream = new FileStream(state.documentation_list[0], AccessRead, OpenExisting);
					base = new XV::ManualVolume(base_stream);
				} catch (...) {}
				if (base) base->Update(desc.output_volume, deletes.InnerRef()); else base = desc.output_volume;
				try {
					SafePointer<Stream> output = new FileStream(state.documentation_list[0], AccessReadWrite, CreateAlways);
					base->Save(output);
				} catch (...) {
					desc.status.status = XV::CompilerStatus::FileAccessFailure;
					desc.status.error_line = state.documentation_list[0];
					desc.status.error_line_len = desc.status.error_line_no = desc.status.error_line_pos = 0;
					if (!state.silent) PrintCompilerError(desc.status);
					return int(desc.status.status);
				}
				if (deletes) try {
					SafePointer<Stream> output = new FileStream(state.documentation_list[0] + L".deleta", AccessReadWrite, CreateAlways);
					deletes->Save(output);
				} catch (...) {}
				for (int i = 1; i < state.documentation_list.Length(); i++) {
					try {
						SafePointer<Stream> output = new FileStream(state.documentation_list[i], AccessReadWrite, CreateAlways);
						base->Save(output);
					} catch (...) {}
				}
			}
			if (desc.status.status != XV::CompilerStatus::Success) {
				if (!state.silent) PrintCompilerError(desc.status);
				return int(desc.status.status);
			} else if (state.launch) {
				Array<string> args(0x10);
				SafePointer<XC::IChannelServer> srvr;
				if (state.direct_mode) {
					string srvr_path;
					srvr = XC::CreateChannelServer();
					srvr->GetChannelPath(srvr_path);
					args << state.input;
					args << L"--xx-recte";
					args << srvr_path;
					SafePointer<TaskQueue> queue = new TaskQueue;
					if (!queue->ProcessAsSeparateThread()) throw Exception();
					queue->SubmitTask(CreateFunctionalTask([srvr, mdl = desc.output_module]() {
						SafePointer<Stream> stream = mdl->GetOutputModuleData();
						SafePointer<DataBlock> data_send = stream->ReadAll();
						while (true) {
							SafePointer<XC::IChannel> cn = srvr->Accept();
							if (!cn) { srvr->Close(); return; }
							XC::Request req;
							req.verb = 0x10EE;
							req.data = data_send;
							if (!cn->SendRequest(req)) { srvr->Close(); return; }
						}
					}));
				} else args << output;
				args << state.launch_args;
				SafePointer<Process> process = CreateCommandProcess(state.xx_path, &args);
				if (!process) {
					if (srvr) srvr->Close();
					if (!state.silent) console << TextColor(12) << Localized(205) << TextColorDefault() << LineFeed();
					return 1;
				}
				process->Wait();
				if (srvr) srvr->Close();
				return process->GetExitCode();
			} else return 0;
		} catch (...) { return 0x3F; }
	} else {
		if (!state.silent) try {
			auto length = Localized(100).ToInt32();
			for (int i = 0; i < length; i++) console << Localized(101 + i) << LineFeedSequence;
		} catch (...) {}
		return 0;
	}
}