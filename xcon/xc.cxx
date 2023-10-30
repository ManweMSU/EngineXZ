#include <EngineRuntime.h>

#include "xc_buffer.h"
#include "xc_window.h"
#include "xc_io.h"

using namespace Engine;
using namespace Engine::IO;
using namespace Engine::XC;
using namespace Engine::Streaming;

struct {
	string preset_file, attach_io_channel, title;
} state;

int Main(void)
{
	try {
		Windows::GetWindowSystem();
		Codec::InitializeDefaultCodecs();
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
			} else throw InvalidArgumentException();
		}
		if (!state.preset_file.Length()) state.preset_file = ExpandPath(Path::GetDirectory(GetExecutablePath()) + L"/xc.ini");
		if (!state.title.Length()) state.title = ENGINE_VI_APPNAME;
	} catch (...) { return 1; }
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
		if (attach) Windows::GetWindowSystem()->SubmitTask(CreateFunctionalTask([attach]() { attach->LaunchService(); }));
	} catch (...) { return 3; }
	RunConsoleWindow();
	if (attach) attach->CloseService();
	return 0;
}