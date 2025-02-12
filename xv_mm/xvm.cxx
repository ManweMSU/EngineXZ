#include <EngineRuntime.h>

#include "xvm.h"
#include "xvm_browser.h"
#include "xvm_editor.h"

using namespace Engine;
using namespace Engine::UI;
using namespace Engine::Windows;
using namespace Engine::Storage;
using namespace Engine::Streaming;

constexpr const widechar * IPC_Create	= L"crea";
constexpr const widechar * IPC_Open		= L"aperi";
constexpr const widechar * IPC_Browse	= L"monstra";

InterfaceTemplate interface;
Volumes::Dictionary<IWindow *, IWindowCallback *> windows;
void RegisterWindow(Engine::Windows::IWindow * window, Engine::Windows::IWindowCallback * callback) { windows.Append(window, callback); }
void UnregisterWindow(Engine::Windows::IWindow * window) { windows.Remove(window); if (windows.IsEmpty()) GetWindowSystem()->ExitMainLoop(); }

class ApplicationCallback : public IApplicationCallback
{
public:
	virtual bool IsHandlerEnabled(ApplicationHandler event) override
	{
		if (event == ApplicationHandler::CreateFile) return true;
		else if (event == ApplicationHandler::OpenExactFile) return true;
		else if (event == ApplicationHandler::OpenSomeFile) return true;
		else if (event == ApplicationHandler::ShowHelp) return true;
		else if (event == ApplicationHandler::Terminate) return true;
		else return false;
	}
	virtual bool IsWindowEventAccessible(WindowHandler handler) override
	{
		if (handler == WindowHandler::Copy) return true;
		else if (handler == WindowHandler::Cut) return true;
		else if (handler == WindowHandler::Delete) return true;
		else if (handler == WindowHandler::Find) return true;
		else if (handler == WindowHandler::Paste) return true;
		else if (handler == WindowHandler::Redo) return true;
		else if (handler == WindowHandler::Save) return true;
		else if (handler == WindowHandler::SaveAs) return true;
		else if (handler == WindowHandler::SelectAll) return true;
		else if (handler == WindowHandler::Undo) return true;
		else return false;
	}
	virtual void CreateNewFile(void) override { ShowHelp(); }
	virtual void OpenSomeFile(void) override
	{
		auto task = CreateStructuredTask<OpenFileInfo>([this](const OpenFileInfo & value) { for (auto & f : value.Files) OpenExactFile(f); });
		task->Value1.Formats << FileFormat();
		task->Value1.Formats.LastElement().Description = L"Manualis Linguae XV";
		task->Value1.Formats.LastElement().Extensions << L"xvm";
		task->Value1.Formats << FileFormat();
		task->Value1.Formats.LastElement().Description = L"Lingua XV";
		task->Value1.Formats.LastElement().Extensions << L"xv" << L"v" << L"vvv";
		task->Value1.Formats << FileFormat();
		task->Value1.Formats.LastElement().Description = L"Lingua XW";
		task->Value1.Formats.LastElement().Extensions << L"xw" << L"w";
		task->Value1.MultiChoose = true;
		GetWindowSystem()->OpenFileDialog(&task->Value1, 0, task);
	}
	virtual bool OpenExactFile(const string & path) override
	{
		if (path.Fragment(0, 7) == L"xvmm://") {
			CreateBrowser(path.Fragment(7, -1));
			if (windows.IsEmpty()) GetWindowSystem()->ExitMainLoop();
			return true;
		} else {
			auto status = CreateEditor(path);
			if (windows.IsEmpty()) GetWindowSystem()->ExitMainLoop();
			return status;
		}
	}
	virtual void ShowHelp(void) override { ShowHelp(L".primus"); }
	virtual bool DataExchangeReceive(handle client, const string & verb, const DataBlock * data) override
	{
		if (verb == IPC_Create) {
			CreateNewManual();
			return true;
		} else if (verb == IPC_Open) {
			string path(data->GetBuffer(), -1, Encoding::UTF8);
			return OpenExactFile(path);
		} else if (verb == IPC_Browse) {
			if (data && data->Length()) {
				string path(data->GetBuffer(), -1, Encoding::UTF8);
				ShowHelp(path);
			} else ShowHelp();
			return true;
		} else return false;
	}
	virtual DataBlock * DataExchangeRespond(handle client, const string & verb) override { return 0; }
	virtual void DataExchangeDisconnect(handle client) override {}
	virtual bool Terminate(void) override
	{
		if (windows.IsEmpty()) GetWindowSystem()->ExitMainLoop();
		auto wnd = windows;
		for (auto & w : wnd) w.value->WindowClose(w.key);
		return windows.IsEmpty();
	}
	void CreateNewManual(void) { CreateEditor(); }
	void ShowHelp(const string & path) { CreateBrowser(path); }
};

int Main(void)
{
	try {
		SafePointer<IScreen> screen = GetDefaultScreen();
		CurrentScaleFactor = screen->GetDpiScale();
		if (CurrentScaleFactor < 1.25) CurrentScaleFactor = 1.0;
		else if (CurrentScaleFactor < 1.75) CurrentScaleFactor = 1.5;
		else CurrentScaleFactor = 2.0;
		Assembly::CurrentLocale = Assembly::GetCurrentUserLocale();
		SafePointer<Stream> com_stream = Assembly::QueryLocalizedResource(L"COM");
		SafePointer<StringTable> com = new StringTable(com_stream);
		Assembly::SetLocalizedCommonStrings(com);
		SafePointer<Stream> ui_stream = Assembly::QueryResource(L"UI");
		Loader::LoadUserInterfaceFromBinary(interface, ui_stream);
	} catch (...) { return 1; }
	ApplicationCallback callback;
	SafePointer< Array<string> > args = GetCommandLine();
	auto client = GetWindowSystem()->CreateIPCClient(ENGINE_VI_APPIDENT, ENGINE_VI_COMPANYIDENT);
	if (client) {
		if (args->Length() <= 1) {
			client->SendData(IPC_Browse, 0, 0, 0);
		} else {
			for (int i = 1; i < args->Length(); i++) {
				auto & a = args->ElementAt(i);
				if (a == L"--crea") {
					client->SendData(IPC_Create, 0, 0, 0);
				} else if (a == L"--monstra") {
					i++;
					if (i < args->Length()) {
						SafePointer<DataBlock> data = args->ElementAt(i).EncodeSequence(Encoding::UTF8, true);
						client->SendData(IPC_Browse, data, 0, 0);
					}
				} else {
					SafePointer<DataBlock> data = IO::ExpandPath(a).EncodeSequence(Encoding::UTF8, true);
					client->SendData(IPC_Open, data, 0, 0);
				}
			}
		}
		Sleep(500);
		return 0;
	}
	GetWindowSystem()->SetFilesToOpen(args->GetBuffer() + 1, args->Length() - 1);
	GetWindowSystem()->SetCallback(&callback);
	GetWindowSystem()->LaunchIPCServer(ENGINE_VI_APPIDENT, ENGINE_VI_COMPANYIDENT);
	GetWindowSystem()->RunMainLoop();
	GetWindowSystem()->SetCallback(0);
	return 0;
}