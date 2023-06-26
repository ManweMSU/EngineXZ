#include <EngineRuntime.h>

#include "xasm/xa_trans.h"
#include "xasm/xa_type_helper.h"
#include "xasm/xa_compiler.h"
#include "xasm/xa_dasm.h"
#include "xasm_tests/tests.h"
#include "ximg/xi_module.h"
#include "ximg/xi_function.h"
#include "ximg/xi_resources.h"
#include "xenv/xe_ctx.h"

using namespace Engine;
using namespace Engine::XA;
using namespace Engine::XA::TH;

IO::Console console;

typedef int64 (* func_main) (int64 a);

class __unused_class
{
public:
	void int_dtor(void)
	{
		console.WriteLine(FormatString(L"\"Destructing\" the int = %0", string(*reinterpret_cast<uint *>(this), HexadecimalBase, 8)));
	}
};
void print_integer(int64 p)
{
	console.WriteLine(FormatString(L"Printing an integer: %0", p));
}
bool cond_check(void)
{
	console.Write(L"Press Y or N: ");
	console.SetInputMode(IO::ConsoleInputMode::Raw);
	widechar c;
	while (true) {
		IO::ConsoleEventDesc event;
		console.ReadEvent(event);
		if (event.Event == IO::ConsoleEvent::CharacterInput) {
			if (event.CharacterCode == L'Y' || event.CharacterCode == L'y') { c = L'Y'; break; }
			else if (event.CharacterCode == L'N' || event.CharacterCode == L'n') { c = L'N'; break; }
		}
	}
	console.SetInputMode(IO::ConsoleInputMode::Echo);
	console.WriteLine(string(c));
	return c == L'Y';
}
int64 read_integer(uint64 * error)
{
	try {
		console.Write(L"Enter an integer: ");
		return console.ReadLine().ToInt64();
	} catch (...) {
		if (error) *error = 1;
		return 0;
	}
}
void print(const char * utf8)
{
	console.Write(string(utf8));
}

class my_res_t : public IReferenceResolver
{
public:
	virtual uintptr ResolveReference(const string & to) noexcept override
	{
		if (to == L"int_dtor") {
			auto ptr = &__unused_class::int_dtor;
			uintptr addr;
			MemoryCopy(&addr, &ptr, sizeof(void *));
			return addr;
		} else if (to == L"print_integer") return reinterpret_cast<uintptr>(print_integer);
		else if (to == L"print") return reinterpret_cast<uintptr>(print);
		else if (to == L"read_bool") return reinterpret_cast<uintptr>(cond_check);
		else if (to == L"read_int") return reinterpret_cast<uintptr>(read_integer);
		else return 0;
	}
};

int Main(void)
{
	while (true) {
		try {
		
		} catch (Exception & e) {
			console.SetTextColor(12);
			console.WriteLine(L"EXCEPTION: " + e.ToString());
		}
		console.SetTextColor(-1);
	}
	return 1;

	PerformTests(console);
	SafePointer<Streaming::TextReader> rdr;
	try {
		#ifdef ENGINE_DEBUG
		SafePointer<Streaming::Stream> stream = new Streaming::FileStream(L"../../test.asm", Streaming::AccessRead, Streaming::OpenExisting);
		#else
		SafePointer<Streaming::Stream> stream = new Streaming::FileStream(L"test.asm", Streaming::AccessRead, Streaming::OpenExisting);
		#endif
		rdr = new Streaming::TextReader(stream);
	} catch (...) { return 1; }

	Function fvnctia;
	CompilerStatusDesc desc;
	CompileFunction(rdr->ReadAll(), fvnctia, desc);
	if (desc.status != CompilerStatus::Success) {
		console.SetTextColor(12);
		console.WriteLine(L"XASM COMPILER ERROR: " + string(uint(desc.status), HexadecimalBase, 4));
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
		return 1;
	}

	TranslatedFunction fvnctia2;
	SafePointer<IAssemblyTranslator> trs = CreatePlatformTranslator();
	if (!trs->Translate(fvnctia2, fvnctia)) {
		console.WriteLine(L"TRANSLATOR ERROR");
		return 2;
	}

	SafePointer<IExecutableLinker> linker = CreateLinker();
	Volumes::Dictionary<string, TranslatedFunction *> ft;
	ft.Append(L"main", &fvnctia2);
	my_res_t my_res;
	SafePointer<IExecutable> exec = linker->LinkFunctions(ft, &my_res);
	auto fvnctia3 = exec->GetEntryPoint<func_main>(L"main");
	
	while (true) {
		try {
			uint64 a = console.ReadLine().ToInt64();
			auto c = fvnctia3(a);
			console.WriteLine(FormatString(L"RESULT = %0", string(c)));
		} catch (...) { break; }
	}
	return 0;
}