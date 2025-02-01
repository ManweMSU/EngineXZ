#include <EngineRuntime.h>

using namespace Engine;
using namespace Engine::IO;
using namespace Engine::Windows;

#ifdef ENGINE_WINDOWS
constexpr const widechar * xx_path = L"\\xx\\xxf.exe";
#endif
#ifdef ENGINE_MACOSX
constexpr const widechar * xx_path = L"/../../XX.app/Contents/MacOS/xxf";
#endif

class Callback : public IApplicationCallback
{
	static void _launch_xx(const string & image) noexcept
	{
		try {
			auto xx = ExpandPath(Path::GetDirectory(GetExecutablePath()) + xx_path);
			SafePointer<Process> process;
			if (image.Length()) {
				Array<string> args(1);
				args << image;
				process = CreateProcess(xx, &args);
			} else process = CreateProcess(xx);
		} catch (...) {}
	}
public:
	virtual bool IsHandlerEnabled(ApplicationHandler event) override
	{
		if (event == ApplicationHandler::CreateFile) return true;
		else if (event == ApplicationHandler::OpenExactFile) return true;
		else if (event == ApplicationHandler::Terminate) return true;
		else return false;
	}
	virtual void CreateNewFile(void) override
	{
		GetWindowSystem()->SubmitTask(CreateFunctionalTask([]() {
			_launch_xx(L"");
			GetWindowSystem()->ExitMainLoop();
		}));
	}
	virtual bool OpenExactFile(const string & path) override
	{
		GetWindowSystem()->SubmitTask(CreateFunctionalTask([image = path]() {
			_launch_xx(image);
			GetWindowSystem()->ExitMainLoop();
		}));
		return true;
	}
	virtual bool Terminate(void) override { GetWindowSystem()->ExitMainLoop(); }
};

int Main(void)
{
	Callback callback;
	SafePointer< Array<string> > args = GetCommandLine();
	auto system = GetWindowSystem();
	system->SetCallback(&callback);
	system->SetFilesToOpen(args->GetBuffer() + 1, args->Length() - 1);
	system->RunMainLoop();
	return 0;
}