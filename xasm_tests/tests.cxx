#include "tests.h"

#include "../xasm/xa_type_helper.h"
#include "../xasm/xa_compiler.h"
#include "../xasm/xa_trans.h"

namespace Engine
{
	namespace XA
	{
		struct TestStruct {
			int a;
			int b;
			int c;

			TestStruct(void) {}
			~TestStruct(void) {}

			TestStruct test(TestStruct & input)
			{
				TestStruct result;
				result.a = input.a + b;
				result.b = input.b + c;
				result.c = input.c + a;
				return result;
			}
		};

		typedef int (* TestIntFunc) (int a, int b, int c);
		typedef double (* TestFloatFunc) (bool a, double b, double c);
		typedef TestStruct (* TestBlobFunc) (int shift_by, TestStruct & input, char i);
		typedef TestStruct (TestStruct:: * TestClassFunc) (TestStruct & input);

		class TestReferenceResolver : public IReferenceResolver
		{
			IO::Console & console;
		public:
			TestReferenceResolver(IO::Console & cns) : console(cns) {}
			virtual uintptr ResolveReference(const string & to) noexcept override
			{
				if (to == L"test") {
					auto test = &TestStruct::test;
					uintptr result;
					MemoryCopy(&result, &test, sizeof(void *));
					return result;
				}
				console.WriteLine(L"Nothing for link named \"" + to + L"\"");
				return 0;
			}
		};
		void PerformTests(IO::Console & console)
		{
			SafePointer<IExecutable> exec;
			{
				auto path = IO::ExpandPath(IO::Path::GetDirectory(IO::GetExecutablePath()) + L"/../../xasm_tests");
				auto files = IO::Search::GetFiles(path + L"/*.asm");
				SafeArray<TranslatedFunction> functions(0x10);
				Volumes::Dictionary<string, TranslatedFunction *> functions_dict;
				SafePointer<IAssemblyTranslator> trs = CreatePlatformTranslator();
				if (!trs) {
					console << L"No platform translator!" << IO::LineFeedSequence;
					return;
				}
				for (auto & f : *files) {
					console << L"Loading: " << f << L"...";
					Streaming::FileStream stream(path + L"/" + f, Streaming::AccessRead, Streaming::OpenExisting);
					Streaming::TextReader rdr(&stream);
					auto code = rdr.ReadAll();
					console << L"OK!" << IO::LineFeedSequence << L"Compiling: " << f << L"...";
					Function abstract;
					CompilerStatusDesc desc;
					CompileFunction(code, abstract, desc);
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
						return;
					}
					TranslatedFunction function;
					console << L"OK!" << IO::LineFeedSequence << L"Translating: " << f << L"...";
					auto status = trs->Translate(function, abstract);
					if (!status) {
						console.SetTextColor(12);
						console.WriteLine(L"XASM TRANSLATOR ERROR.");
						console.SetTextColor(-1);
						return;
					}
					console << L"OK!" << IO::LineFeedSequence;
					functions << function;
					functions_dict.Append(IO::Path::GetFileNameWithoutExtension(f), &functions.LastElement());
				}
				files->Release();
				TestReferenceResolver resolver(console);
				SafePointer<IExecutableLinker> linker = CreateLinker();
				if (!linker) {
					console << L"No platform linker!" << IO::LineFeedSequence;
					return;
				}
				console << L"Linking...";
				exec = linker->LinkFunctions(functions_dict, &resolver);
				if (!exec) {
					console.SetTextColor(12);
					console.WriteLine(L"XASM LINKER ERROR.");
					console.SetTextColor(-1);
					return;
				}
				console << L"OK!" << IO::LineFeedSequence;
			}
			auto test_ints = exec->GetEntryPoint<TestIntFunc>(L"test_ints");
			if (!test_ints) {
				console << L"No test_ints function found." << IO::LineFeedSequence;
				return;
			}
			if (test_ints(1, 2, 3) != 7) {
				console << L"test_ints: test #1 failed" << IO::LineFeedSequence;
				return;
			}
			if (test_ints(5, 10, 15) != 155) {
				console << L"test_ints: test #2 failed" << IO::LineFeedSequence;
				return;
			}
			console << L"test_ints: its OK" << IO::LineFeedSequence;
			auto test_floats = exec->GetEntryPoint<TestFloatFunc>(L"test_floats");
			if (!test_floats) {
				console << L"No test_floats function found." << IO::LineFeedSequence;
				return;
			}
			if (test_floats(true, 66.0, 13.0) != 66.0) {
				console << L"test_floats: test #1 failed" << IO::LineFeedSequence;
				return;
			}
			if (test_floats(false, 66.0, 13.0) != 13.0) {
				console << L"test_floats: test #2 failed" << IO::LineFeedSequence;
				return;
			}
			console << L"test_floats: its OK" << IO::LineFeedSequence;
			auto test_blob = exec->GetEntryPoint<TestBlobFunc>(L"test_blob");
			if (!test_blob) {
				console << L"No test_blob function found." << IO::LineFeedSequence;
				return;
			}
			TestStruct s;
			s.a = 1;
			s.b = 2;
			s.c = 3;
			auto obj = test_blob(10, s, -5);
			if (obj.a != 12) {
				console << L"test_blob: test #1 failed" << IO::LineFeedSequence;
				return;
			}
			if (obj.b != 11) {
				console << L"test_blob: test #2 failed" << IO::LineFeedSequence;
				return;
			}
			if (obj.c != -2) {
				console << L"test_blob: test #3 failed" << IO::LineFeedSequence;
				return;
			}
			console << L"test_blob: its OK" << IO::LineFeedSequence;
			auto test_func = &TestStruct::test;
			auto ptest = &test_func;
			auto test_this = exec->GetEntryPoint(L"test_this");
			MemoryCopy(ptest, &test_this, sizeof(void *));
			if (!test_this) {
				console << L"No test_this function found." << IO::LineFeedSequence;
				return;
			}
			s.a = 10;
			s.b = 20;
			s.c = 30;
			obj.a = 55;
			obj.b = 66;
			obj.c = 77;
			auto obj2 = (s.*test_func)(obj);
			if (obj2.a != 75) {
				console << L"test_this: test #1 failed" << IO::LineFeedSequence;
				return;
			}
			if (obj2.b != 96) {
				console << L"test_this: test #2 failed" << IO::LineFeedSequence;
				return;
			}
			if (obj2.c != 87) {
				console << L"test_this: test #3 failed" << IO::LineFeedSequence;
				return;
			}
			console << L"test_this: its OK" << IO::LineFeedSequence;
		}
	}
}