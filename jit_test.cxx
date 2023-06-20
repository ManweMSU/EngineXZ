#include <EngineRuntime.h>

#include "xasm/xa_trans.h"
#include "xasm/xa_type_helper.h"
#include "xasm/xa_compiler.h"
#include "xasm/xa_dasm.h"
#include "xasm_tests/tests.h"
#include "ximg/xi_module.h"

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
		Streaming::MemoryStream stream(0x10000);
		try {
			XI::Module::Literal l1;
			l1.contents = XI::Module::Literal::Class::FloatingPoint;
			l1.data_double = 666.0;
			l1.length = 8;
			l1.attributes.Append(L"ATTR1", L"value-1");
			l1.attributes.Append(L"ATTR2", L"value-666");
			XI::Module::Variable v1;
			v1.size = TH::MakeSize(1, 2);
			v1.offset = TH::MakeSize(0, 3);
			v1.type_canonical_name = L"PIDOR";
			v1.attributes.Append(L"SUKA", L"PIDOR");
			XI::Module::Function f1;
			f1.code_flags = 666;
			f1.vft_index = Point(5, 4);
			f1.attributes.Append(L"SUKA-2", L"PIDOR-2");
			f1.code = new DataBlock(1);
			f1.code->Append(55);
			XI::Module::Property p1;
			p1.type_canonical_name = L"PIDOR_PROP";
			p1.getter_interface = L"GETTER-INT";
			p1.setter_interface = L"SETTER-INT";
			p1.getter = f1;
			p1.setter = f1;
			p1.attributes.Append(L"SUKA-5", L"PIDOR-5");
			XI::Module::Class c1;
			c1.class_nature = XI::Module::Class::Nature::Interface;
			c1.instance_spec.semantics = XA::ArgumentSemantics::RTTI;
			c1.instance_spec.size = TH::MakeSize(12, 34);
			c1.parent_class.interface_name = L"PARENT";
			c1.parent_class.vft_pointer_offset = TH::MakeSize(0, 1);
			c1.interfaces_implements << XI::Module::Interface();
			c1.interfaces_implements.LastElement().interface_name = L"INTERFACE";
			c1.interfaces_implements.LastElement().vft_pointer_offset = TH::MakeSize(2, 3);
			c1.fields.Append(L"F1", v1);
			c1.methods.Append(L"M1", f1);
			c1.properties.Append(L"P1", p1);
			c1.attributes.Append(L"SUKA-3", L"PIDOR-3");
			c1.attributes.Append(L"SUKA-4", L"PIDOR-4");
			XI::Module m1;
			m1.subsystem = XI::Module::ExecutionSubsystem::GUI;
			m1.module_import_name = L"TM";
			m1.assembler_name = L"TA";
			m1.assembler_version.major = 1;
			m1.assembler_version.minor = 2;
			m1.assembler_version.subver = 3;
			m1.assembler_version.build = 4;
			m1.modules_depends_on << L"M1";
			m1.modules_depends_on << L"M2";
			m1.modules_depends_on << L"M3";
			m1.data = new DataBlock(1);
			m1.data->Append(66);
			m1.resources.Append(L"DTA:55", m1.data);
			m1.resources.Append(L"DTA:77", m1.data);
			m1.literals.Append(L"LIT1", l1);
			m1.variables.Append(L"VAR1", v1);
			m1.functions.Append(L"FUNC1", f1);
			m1.classes.Append(L"CLS1", c1);
			m1.Save(&stream);
			XI::Module m2(&stream);

			console.WriteLine(GetStringRepresentation(m2.resources));

			int p = 55;

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