#include <EngineRuntime.h>

#include "xc_buffer.h"
#include "xc_window.h"
#include "xc_io.h"
#include "../xexec/xx_app_activate.h"

using namespace Engine;
using namespace Engine::IO;
using namespace Engine::XC;
using namespace Engine::Streaming;

struct {
	string preset_file, attach_io_channel, title;
	string export_text;
	bool export_text_mode;
} state;

int ExportRoutine(void)
{
	try {
		Windows::SaveFileInfo info;
		info.Title = state.title;
		info.AppendExtension = true;
		info.Format = 0;
		info.Formats << Windows::FileFormat();
		info.Formats.LastElement().Extensions << L"txt";
		info.Formats.LastElement().Description = L"UTF-8";
		info.Formats << Windows::FileFormat();
		info.Formats.LastElement().Extensions << L"txt";
		info.Formats.LastElement().Description = L"UTF-16 LE";
		info.Formats << Windows::FileFormat();
		info.Formats.LastElement().Extensions << L"txt";
		info.Formats.LastElement().Description = L"UTF-32 LE";
		WindowSubsystemInitialize();
		if (!Windows::GetWindowSystem()->SaveFileDialog(&info, 0, 0)) return 5;
		if (!info.File.Length()) return 0;
		SafePointer<FileStream> stream = new FileStream(info.File, AccessWrite, CreateAlways);
		Unix::SetFileAccessRights(stream->Handle(), Unix::AccessRightRegular, Unix::AccessRightRegular, Unix::AccessRightRegular);
		Encoding encoding = Encoding::ANSI;
		if (info.Format == 0) encoding = Encoding::UTF8;
		else if (info.Format == 1) encoding = Encoding::UTF16;
		else if (info.Format == 2) encoding = Encoding::UTF32;
		SafePointer<TextWriter> writer = new TextWriter(stream, encoding);
		writer->WriteEncodingSignature();
		writer->Write(state.export_text);
		return 0;
	} catch (...) { return 5; }
}
int Main(void)
{
	try {
		#ifdef ENGINE_MACOSX
		Engine::IO::SetStandardOutput(Engine::IO::InvalidHandle);
		Engine::IO::SetStandardInput(Engine::IO::InvalidHandle);
		Engine::IO::SetStandardError(Engine::IO::InvalidHandle);
		#endif
		Windows::GetWindowSystem();
		Codec::InitializeDefaultCodecs();
		state.export_text_mode = false;
		SafePointer< Array<string> > args = GetCommandLine();
		for (int i = 1; i < args->Length(); i++) {
			auto & arg = args->ElementAt(i);
			if (arg == L"--xx-adnecte") {
				i++; if (i >= args->Length()) throw InvalidArgumentException();
				state.attach_io_channel = args->ElementAt(i);
			} else if (arg == L"--forma") {
				i++; if (i >= args->Length()) throw InvalidArgumentException();
				state.preset_file = ExpandPath(args->ElementAt(i));
			} else if (arg == L"--titulus") {
				i++; if (i >= args->Length()) throw InvalidArgumentException();
				state.title = args->ElementAt(i);
			} else if (arg == L"--exporta") {
				i++; if (i >= args->Length()) throw InvalidArgumentException();
				state.export_text_mode = true;
				state.export_text = args->ElementAt(i);
			} else throw InvalidArgumentException();
		}
		if (!state.preset_file.Length()) state.preset_file = ExpandPath(Path::GetDirectory(GetExecutablePath()) + L"/xc.ini");
		if (!state.title.Length()) state.title = ENGINE_VI_APPNAME;
	} catch (...) { return 1; }
	if (state.export_text_mode) { return ExportRoutine(); }
	ConsoleCreateMode console_create_mode;
	SafePointer<ConsoleState> console;
	SafePointer<IAttachIO> attach;
	if (state.attach_io_channel.Length()) {
		try {
			attach = CreateAttachIO(state.attach_io_channel);
			if (!attach->Communicate(state.title, state.preset_file)) throw Exception();
			if (!attach->CreateConsole(console.InnerRef(), &console_create_mode)) throw Exception();
		} catch (...) { return 4; }
	} else try {
		LoadConsolePreset(state.title, state.preset_file, console.InnerRef(), &console_create_mode);
	} catch (...) { return 2; }
	try {
		WindowSubsystemInitialize();
		CreateConsoleWindow(console, console_create_mode);
		if (attach) Windows::GetWindowSystem()->SubmitTask(CreateFunctionalTask([attach]() {
			attach->LaunchService();
			XX::EnforceApplicationActivation();
		}));
	} catch (...) { return 3; }
	RunConsoleWindow();
	if (attach) attach->CloseService();
	ExitProcess(0);
	return 0;
}