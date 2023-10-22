#include "xc_pty.h"
#include "xc_vts.h"

#include <Windows.h>

namespace Engine
{
	namespace XC
	{
		class PseudoConsole : public IPseudoTerminal, IOutputCallback
		{
		private:
			SafePointer<ConsoleState> _console;
			HANDLE _in, _out;
			HPCON _pty;
			COORD _last_size;
			bool _alive;
		private:
			static string _escape(const string & arg) { return L"\"" + arg.Replace(L'\"', L"\"\"") + L"\""; }
			static int _pty_thread(void * arg)
			{
				TerminalProcessingState state;
				TerminalStateInit(state);
				int buffer_used = 0;
				int buffer_size = 0x1000;
				char * buffer = (char *) malloc(0x1000);
				auto self = reinterpret_cast<PseudoConsole *>(arg);
				if (buffer) {
					while (true) {
						DWORD used;
						if (!ReadFile(self->_in, buffer + buffer_used, buffer_size - buffer_used, &used, 0)) { if (GetLastError() != ERROR_NO_DATA) break; }
						buffer_used += used;
						int processed;
						ProcessTerminalInput(state, *self, *self->_console, buffer, buffer_used, processed);
						if (processed) {
							for (int i = 0; i < buffer_used - processed; i++) buffer[i] = buffer[i + processed];
							buffer_used -= processed;
						}
					}
					free(buffer);
				}
				Windows::GetWindowSystem()->SubmitTask(CreateFunctionalTask([cns = self->_console]() { cns->SetEndOfStream(); }));
				self->Release();
				return 0;
			}
		public:
			PseudoConsole(ConsoleState * console) : _alive(true)
			{
				_console.SetRetain(console);
				HANDLE con_in, con_out;
				_last_size.X = _console->GetBufferWidth();
				_last_size.Y = _console->GetBufferLimitHeight();
				if (!CreatePipe(&_in, &con_out, 0, 0)) throw Exception();
				if (!CreatePipe(&con_in, &_out, 0, 0)) { CloseHandle(_in); CloseHandle(con_out); throw Exception(); }
				if (CreatePseudoConsole(_last_size, con_in, con_out, 0, &_pty) != S_OK) {
					CloseHandle(_in); CloseHandle(con_out);
					CloseHandle(_out); CloseHandle(con_in);
					throw Exception();
				}
				CloseHandle(con_out); CloseHandle(con_in);
				Retain();
				SafePointer<Thread> thread = CreateThread(_pty_thread, this);
				if (!thread) { Terminate(); Release(); throw Exception(); }
				_console->SetCallback(this);
			}
			virtual ~PseudoConsole(void) override { Terminate(); }
			virtual bool CreateAttachedProcess(const string & exec, const string * argv, int argc) noexcept override
			{
				STARTUPINFOEX si;
				ZeroMemory(&si, sizeof(si));
				si.StartupInfo.cb = sizeof(STARTUPINFOEX);
				SIZE_T tal_size;
				InitializeProcThreadAttributeList(0, 1, 0, &tal_size);
				si.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST) malloc(tal_size);
				if (!si.lpAttributeList) return false;
				bool result = false;
				if (InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &tal_size)) {
					if (UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, _pty, sizeof(_pty), 0, 0)) {
						try {
							DynamicString arg_list;
							arg_list << _escape(exec);
							for (int i = 0; i < argc; i++) arg_list << L" " << _escape(argv[i]);
							PROCESS_INFORMATION pi;
							ZeroMemory(&pi, sizeof(pi));
							if (CreateProcessW(0, arg_list, 0, 0, 0, EXTENDED_STARTUPINFO_PRESENT, 0, 0, &si.StartupInfo, &pi)) {
								result = true;
								CloseHandle(pi.hThread);
								CloseHandle(pi.hProcess);
							}
						} catch (...) {}
					}
					DeleteProcThreadAttributeList(si.lpAttributeList);
				}
				free(si.lpAttributeList);
				return result;
			}
			virtual void WriteOutputStream(const char * data, int length) noexcept override { DWORD written; WriteFile(_out, data, length, &written, 0); }
			virtual bool OutputKey(uint code, uint flags) noexcept override
			{
				return false;
				// TODO: IMPLEMENT
			}
			virtual void OutputText(uint ucs) noexcept override
			{
				DWORD written;
				char utf8[4];
				int len = MeasureSequenceLength(&ucs, 1, Encoding::UTF32, Encoding::UTF8);
				ConvertEncoding(&utf8, &ucs, 1, Encoding::UTF32, Encoding::UTF8);
				WriteFile(_out, &utf8, len, &written, 0);
			}
			virtual void ScreenBufferResize(int width, int height) noexcept override
			{
				if (_alive) {
					COORD size;
					size.X = width;
					size.Y = _console->GetBufferLimitHeight();
					if (size.X != _last_size.X || size.Y != _last_size.Y) {
						_last_size = size;
						ResizePseudoConsole(_pty, size);
					}
				}
			}
			virtual void Terminate(void) noexcept override
			{
				if (_alive) {
					ClosePseudoConsole(_pty); CloseHandle(_in); CloseHandle(_out);
					_alive = false;
				}
			}
		};

		IPseudoTerminal * CreatePseudoTerminal(ConsoleState * console) { return new PseudoConsole(console); }
	}
}