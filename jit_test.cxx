#include <EngineRuntime.h>

#include "xenv/xe_loader.h"
#include "xenv/xe_logger.h"
#include "xenv/xe_conapi.h"
#include "xlang/xl_lal.h"
#include "ximg/xi_module.h"
#include "ximg/xi_resources.h"
#include "xv/xv_compiler.h"

using namespace Engine;
using namespace Engine::XA;
using namespace Engine::XL;

using namespace Engine::Streaming;

IO::Console console;

class ITestConsole
{
public:
	virtual void PrintInteger(int value)
	{
		console.WriteLine(FormatString(L"Write integer: %0", value));
	}
	virtual void PrintLogicum(bool value)
	{
		console.WriteLine(FormatString(L"Write boolean: %0", value));
	}
};
ITestConsole test_console;

// class ModuleLoader : public Object, public XE::ILoaderCallback
// {
// 	static bool cond_check(void) noexcept
// 	{
// 		console.Write(L"Press Y or N: ");
// 		console.SetInputMode(IO::ConsoleInputMode::Raw);
// 		widechar c;
// 		while (true) {
// 			IO::ConsoleEventDesc event;
// 			console.ReadEvent(event);
// 			if (event.Event == IO::ConsoleEvent::CharacterInput) {
// 				if (event.CharacterCode == L'Y' || event.CharacterCode == L'y') { c = L'Y'; break; }
// 				else if (event.CharacterCode == L'N' || event.CharacterCode == L'n') { c = L'N'; break; }
// 			}
// 		}
// 		console.SetInputMode(IO::ConsoleInputMode::Echo);
// 		console.WriteLine(string(c));
// 		return c == L'Y';
// 	}
// 	static void use_cns(ITestConsole * cns) noexcept
// 	{
// 		cns->PrintInteger(444);
// 	}
// public:
// 	virtual Streaming::Stream * OpenModule(const string & module_name) noexcept override
// 	{
// 		try {
// 			try {
// 				return new FileStream(L"_build/" + module_name + L".xo", AccessRead, OpenExisting);
// 			} catch (...) {}
// 			return new FileStream(L"_build/" + module_name + L".xx", AccessRead, OpenExisting);
// 		} catch (...) { return 0; }
// 	}
// 	virtual void * GetRoutineAddress(const string & routine_name) noexcept override
// 	{
// 		if (routine_name == L"read_bool") return cond_check;
// 		if (routine_name == L"use_cns") return use_cns;
// 		return 0;
// 	}
// 	virtual handle LoadDynamicLibrary(const string & library_name) noexcept override
// 	{
// 		return LoadLibrary(library_name);
// 	}
// 	virtual void HandleModuleLoadError(const string & module_name, const string & subject, XE::ModuleLoadError error) noexcept override
// 	{
// 		console.SetTextColor(12);
// 		console.WriteLine(FormatString(L"Error %0 loading module '%1' with subject '%2'", string(uint(error), HexadecimalBase, 8), module_name, subject));
// 		console.SetTextColor(-1);
// 	}
// 	virtual Object * ExposeObject(void) noexcept override { return this; }
// 	virtual void * ExposeInterface(const string & interface) noexcept override
// 	{
// 		if (interface == L"cns") return &test_console;
// 		return 0;
// 	}
// };

class Logger : public XE::ILoggerSink
{
public:
	virtual void Log(const string & line) noexcept override
	{
		console.SetTextColor(14);
		console.WriteLine(FormatString(L"%0", line));
		console.SetTextColor(-1);
	}
	virtual Object * ExposeObject(void) noexcept override { return 0; }
};

void PrintCompilerError(XV::CompilerStatusDesc & desc)
{
	console.SetTextColor(12);
	console.WriteLine(L"XV COMPILER ERROR: " + string(uint(desc.status), HexadecimalBase, 4));
	console.WriteLine(L"AT LINE NO " + string(desc.error_line_no));
	console.SetTextColor(-1);
	console.WriteLine(desc.error_line);
	console.SetTextColor(4);
	if (desc.error_line_pos >= 0 && desc.error_line_len > 0) {
		console.Write(string(L' ', desc.error_line_pos));
		console.Write(string(L'~', desc.error_line_len));
		console.LineFeed();
	}
	console.SetTextColor(-1);
}

int Main(void)
{
	Codec::InitializeDefaultCodecs();
	IO::SetCurrentDirectory(IO::Path::GetDirectory(IO::GetExecutablePath()) + L"/../..");

	string output = L"_build";
	SafePointer<XV::ICompilerCallback> callback = XV::CreateCompilerCallback(0, 0, &output, 1, 0);
	XV::CompilerStatusDesc desc;
	XV::CompileModule(L"xv_lib/canonicalis.xv", output, callback, desc);
	if (desc.status != XV::CompilerStatus::Success) {
		PrintCompilerError(desc);
		return 1;
	}
	output = L"_build";
	XV::CompileModule(L"xv_lib/consolatorium.xv", output, callback, desc);
	if (desc.status != XV::CompilerStatus::Success) {
		PrintCompilerError(desc);
		return 1;
	}
	output = L"_build";
	XV::CompileModule(L"xv_lib/errores.en.xv", output, callback, desc);
	if (desc.status != XV::CompilerStatus::Success) {
		PrintCompilerError(desc);
		return 1;
	}
	output = L"_build";
	XV::CompileModule(L"xv_lib/errores.ru.xv", output, callback, desc);
	if (desc.status != XV::CompilerStatus::Success) {
		PrintCompilerError(desc);
		return 1;
	}
	output = L"_build";
	XV::CompileModule(L"test.xv", output, callback, desc);
	if (desc.status != XV::CompilerStatus::Success) {
		PrintCompilerError(desc);
		return 1;
	}
	SafePointer<FileStream> stream = new FileStream(output, AccessRead, OpenExisting);
	XI::Module module(stream);
	for (auto & rsrc : module.resources) console.WriteLine(rsrc.key + L" - " + string(rsrc.value->Length()));
	SafePointer< Volumes::Dictionary<string, string> > meta = XI::LoadModuleMetadata(module.resources);
	if (meta) console.WriteLine(meta->ToString());

	Logger logger;
	SafePointer<XE::StandardLoader> ldr = XE::CreateStandardLoader(XE::UseStandard);
	SafePointer<XE::IConsoleDevice> console_device = XE::CreateSystemConsoleDevice(&console);
	XE::ActivateConsoleIO(*ldr, console_device, 0);
	ldr->AddModuleSearchPath(L"_build");
	SafePointer<XE::ExecutionContext> ectx = new XE::ExecutionContext(ldr);
	XE::LoadErrorLocalization(*ectx, L"errores.ru");
	XE::SetLoggerSink(*ectx, &logger);

	ectx->LoadModule(L"test");
	if (!ldr->IsAlive()) {
		console.SetTextColor(12);
		console.WriteLine(FormatString(L"Error %0 loading module '%1' with subject '%2'", string(uint(ldr->GetLastError()), HexadecimalBase, 8),
			ldr->GetLastErrorModule(), ldr->GetLastErrorSubject()));
		console.SetTextColor(-1);
		return 1;
	}

	auto main = ectx->GetEntryPoint();
	if (main) {
		auto routine = reinterpret_cast<XE::StandardRoutine>(main->GetSymbolEntity());
		XE::ErrorContext error;
		error.error_code = error.error_subcode = 0;
		routine(&error);
		if (error.error_code) {
			string e, se;
			XE::GetErrorDescription(error, *ectx, e, se);
			console.SetTextColor(12);
			if (se.Length()) console.WriteLine(FormatString(L"error %0 with subcode %1", e, se));
			else console.WriteLine(FormatString(L"error %0", e));
			console.SetTextColor(-1);
		}
		console.SetTextColor(10);
		console.WriteLine(FormatString(L"main() exited with exit code %0", error.error_code));
		console.SetTextColor(-1);
	}

	return 0;
}