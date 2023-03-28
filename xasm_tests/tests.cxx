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
		struct IntrisicTest {
			int64 r1;
			int64 r2;
			int64 r4;
			int64 r8;
		};

		typedef int (* TestIntFunc) (int a, int b, int c);
		typedef double (* TestFloatFunc) (bool a, double b, double c);
		typedef TestStruct (* TestBlobFunc) (int shift_by, TestStruct & input, char i);
		typedef TestStruct (TestStruct:: * TestClassFunc) (TestStruct & input);
		typedef double (* TestComplexFunc) (int ia, double fa, double fb, int ib, int ic, int id, int ie, int if_, int ig, double fc, int * rv);
		typedef int (* TestCallFunc) (void);
		typedef IntrisicTest (* IntrisicTestFunc) (int64 a, int64 b);
		typedef IntrisicTest (* IntrisicTestFuncUnary) (int64 a);

		class TestReferenceResolver : public IReferenceResolver
		{
			IO::Console & console;

			static double _fp_add_mul(double a, double b, double c) noexcept { return a + b * c; }
			static void _complex_call(int * rv, int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8) noexcept
			{
				*rv = i1 + (i2 * 10) + (i3 * 100) + (i4 * 1000) + (i5 * 10000) + (i6 * 100000) + (i7 * 1000000) + (i8 * 10000000);
			}
			static int _return_one(intptr * error) { return 1; }
			static int _return_zero(intptr * error) { return 0; }
			static int _return_error(intptr * error) { *error = 1; return 0; }
			static void * _func_selector(int index)
			{
				if (index == 0) return reinterpret_cast<void *>(_return_zero);
				else if (index == 1) return reinterpret_cast<void *>(_return_one);
				else return reinterpret_cast<void *>(_return_error);
			}
		public:
			TestReferenceResolver(IO::Console & cns) : console(cns) {}
			virtual uintptr ResolveReference(const string & to) noexcept override
			{
				if (to == L"test") {
					auto test = &TestStruct::test;
					uintptr result;
					MemoryCopy(&result, &test, sizeof(void *));
					return result;
				} else if (to == L"fp_add_mul") {
					return reinterpret_cast<uintptr>(_fp_add_mul);
				} else if (to == L"complex_call") {
					return reinterpret_cast<uintptr>(_complex_call);
				} else if (to == L"func_selector") {
					return reinterpret_cast<uintptr>(_func_selector);
				}
				console.WriteLine(L"Nothing for link named \"" + to + L"\"");
				return 0;
			}
		};
		void PerformIntrisicTests(IO::Console & console)
		{
			string code, code_unary;
			SafePointer<Storage::Registry> reg;
			{
				auto path = IO::ExpandPath(IO::Path::GetDirectory(IO::GetExecutablePath()) + L"/../../xasm_tests");
				Streaming::FileStream stream(path + L"/test_ar.tasm", Streaming::AccessRead, Streaming::OpenExisting);
				Streaming::TextReader rdr(&stream);
				code = rdr.ReadAll();
			}
			{
				auto path = IO::ExpandPath(IO::Path::GetDirectory(IO::GetExecutablePath()) + L"/../../xasm_tests");
				Streaming::FileStream stream(path + L"/test_ar_unary.tasm", Streaming::AccessRead, Streaming::OpenExisting);
				Streaming::TextReader rdr(&stream);
				code_unary = rdr.ReadAll();
			}
			{
				auto path = IO::ExpandPath(IO::Path::GetDirectory(IO::GetExecutablePath()) + L"/../../xasm_tests");
				Streaming::FileStream stream(path + L"/test_ar.ini", Streaming::AccessRead, Streaming::OpenExisting);
				reg = Storage::CompileTextRegistry(&stream);
			}
			TestReferenceResolver ref(console);
			SafePointer<IAssemblyTranslator> trs = CreatePlatformTranslator();
			SafePointer<IExecutableLinker> lnk = CreateLinker();
			if (!lnk || !trs) {
				console << L"Failed to create either translator or linker." << IO::LineFeedSequence;
				return;
			}
			for (auto & op : reg->GetSubnodes()) {
				SafePointer<Storage::RegistryNode> sn = reg->OpenNode(op);
				auto sgn = sn->GetValueBoolean(L"S");
				auto unr = sn->GetValueBoolean(L"U");
				auto fal = sn->GetValueString(L"A");
				auto local_code = unr ? code_unary : code;
				if (fal.Length()) {
					auto path = IO::ExpandPath(IO::Path::GetDirectory(IO::GetExecutablePath()) + L"/../../xasm_tests");
					Streaming::FileStream stream(path + L"/" + fal, Streaming::AccessRead, Streaming::OpenExisting);
					Streaming::TextReader rdr(&stream);
					local_code = rdr.ReadAll();
				}
				for (auto & test : sn->GetSubnodes()) {
					SafePointer<Storage::RegistryNode> tn = sn->OpenNode(test);
					int64 a = tn->GetValueLongInteger(L"A");
					int64 b = tn->GetValueLongInteger(L"B");
					console << FormatString(L"Testing: %0(%1, %2)...", op, a, b);
					string local = local_code.Replace(L"XXX", op).Replace(L"X_RESIZE", sgn ? L"S_RESIZE" : L"U_RESIZE");
					Function func;
					CompilerStatusDesc desc;
					CompileFunction(local, func, desc);
					if (desc.status != CompilerStatus::Success) {
						console << L"Failed compilation" << IO::LineFeedSequence;
						return;
					}
					TranslatedFunction tfunc;
					auto tstat = trs->Translate(tfunc, func);
					if (!tstat) {
						console << L"Failed translation" << IO::LineFeedSequence;
						return;
					}
					Volumes::Dictionary<string, TranslatedFunction *> fd;
					fd.Append(L"main", &tfunc);
					SafePointer<IExecutable> exec = lnk->LinkFunctions(fd, &ref);
					if (!exec) {
						console << L"Failed linking" << IO::LineFeedSequence;
						return;
					}
					IntrisicTest result;
					if (unr) {
						auto nfunc = exec->GetEntryPoint<IntrisicTestFuncUnary>(L"main");
						if (!nfunc) {
							console << L"Failed getting function" << IO::LineFeedSequence;
							return;
						}
						result = nfunc(a);
					} else {
						auto nfunc = exec->GetEntryPoint<IntrisicTestFunc>(L"main");
						if (!nfunc) {
							console << L"Failed getting function" << IO::LineFeedSequence;
							return;
						}
						result = nfunc(a, b);
					}
					if (result.r8 != tn->GetValueLongInteger(L"R8")) {
						console << FormatString(L"Failed a R8 test: %0 != %1", result.r8, tn->GetValueLongInteger(L"R8")) << IO::LineFeedSequence;
						return;
					}
					if (result.r4 != tn->GetValueLongInteger(L"R4")) {
						console << FormatString(L"Failed a R4 test: %0 != %1", result.r4, tn->GetValueLongInteger(L"R4")) << IO::LineFeedSequence;
						return;
					}
					if (result.r2 != tn->GetValueLongInteger(L"R2")) {
						console << FormatString(L"Failed a R2 test: %0 != %1", result.r2, tn->GetValueLongInteger(L"R2")) << IO::LineFeedSequence;
						return;
					}
					if (result.r1 != tn->GetValueLongInteger(L"R1")) {
						console << FormatString(L"Failed a R1 test: %0 != %1", result.r1, tn->GetValueLongInteger(L"R1")) << IO::LineFeedSequence;
						return;
					}
					console << L"OK" << IO::LineFeedSequence;
				}
			}
		}
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
			TestStruct s, obj;
			s.a = 1;
			s.b = 2;
			s.c = 3;
			obj = test_blob(10, s, -5);
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
			auto test_complex = exec->GetEntryPoint<TestComplexFunc>(L"test_complex");
			if (!test_complex) {
				console << L"No test_complex function found." << IO::LineFeedSequence;
				return;
			}
			int rvi;
			double rvf = test_complex(1, 22.0, 45.0, 2, 3, 4, 5, 6, 7, 3.0, &rvi);
			if (rvi != 1234567) {
				console << L"test_complex: test #1 failed" << IO::LineFeedSequence;
				return;
			}
			if (rvf != 22.0 + 45.0 * 3.0) {
				console << L"test_complex: test #2 failed" << IO::LineFeedSequence;
				return;
			}
			console << L"test_complex: its OK" << IO::LineFeedSequence;
			auto test_call = exec->GetEntryPoint<TestCallFunc>(L"test_call");
			if (!test_call) {
				console << L"No test_call function found." << IO::LineFeedSequence;
				return;
			}
			auto test_call_result = test_call();
			if (test_call_result != 87654321) {
				console << L"test_call: test #1 failed" << IO::LineFeedSequence;
				return;
			}
			console << L"test_call: its OK" << IO::LineFeedSequence;
			console << L"Starting tests for intrisics." << IO::LineFeedSequence;
			PerformIntrisicTests(console);
			console << L"Intrisic tests are all OK" << IO::LineFeedSequence;
		}
	}
}