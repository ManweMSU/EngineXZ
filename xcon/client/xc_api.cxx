#include "xc_api.h"

namespace Engine
{
	namespace XC
	{
		int Console::_input_thread(void * arg)
		{
			auto self = reinterpret_cast<Console *>(arg);
			while (true) {
				Request req;
				if (!self->_channel->ReadRequest(req) || !req.verb) break;
				if (req.verb == 13) {
					_system_input_struct si;
					si.designation = 13;
					si.data[0] = reinterpret_cast<int *>(req.data->GetBuffer())[0];
					si.data[1] = reinterpret_cast<int *>(req.data->GetBuffer())[1];
					si.data[2] = 0;
					self->_sync->Wait();
					self->_sys_inputs.InsertLast(si);
					self->_sys_counter->Open();
					self->_sync->Open();
				} else if (req.verb == 16) {
					_system_input_struct si;
					si.designation = 16;
					si.data[0] = reinterpret_cast<int *>(req.data->GetBuffer())[0];
					si.data[1] = reinterpret_cast<int *>(req.data->GetBuffer())[1];
					si.data[2] = 0;
					self->_sync->Wait();
					self->_sys_inputs.InsertLast(si);
					self->_sys_counter->Open();
					self->_sync->Open();
				} else if (req.verb == 22) {
					IO::ConsoleEventDesc ce;
					ce.Event = IO::ConsoleEvent::CharacterInput;
					ce.CharacterCode = reinterpret_cast<int *>(req.data->GetBuffer())[0];
					self->_sync->Wait();
					self->_inputs.InsertLast(ce);
					self->_counter->Open();
					self->_sync->Open();
				} else if (req.verb == 23) {
					IO::ConsoleEventDesc ce;
					ce.Event = IO::ConsoleEvent::KeyInput;
					ce.KeyCode = reinterpret_cast<int *>(req.data->GetBuffer())[0];
					ce.KeyFlags = reinterpret_cast<int *>(req.data->GetBuffer())[1];
					self->_sync->Wait();
					self->_inputs.InsertLast(ce);
					self->_counter->Open();
					self->_sync->Open();
				} else if (req.verb == 24) {
					IO::ConsoleEventDesc ce;
					ce.Event = IO::ConsoleEvent::ConsoleResized;
					ce.Width = reinterpret_cast<int *>(req.data->GetBuffer())[0];
					ce.Height = reinterpret_cast<int *>(req.data->GetBuffer())[1];
					self->_sync->Wait();
					self->_inputs.InsertLast(ce);
					self->_counter->Open();
					self->_sync->Open();
				} else if (req.verb == 48) {
					_system_input_struct si;
					si.designation = 48;
					si.data[0] = reinterpret_cast<int *>(req.data->GetBuffer())[0];
					si.data[1] = reinterpret_cast<int *>(req.data->GetBuffer())[1];
					si.data[2] = reinterpret_cast<int *>(req.data->GetBuffer())[2];
					self->_sync->Wait();
					self->_sys_inputs.InsertLast(si);
					self->_sys_counter->Open();
					self->_sync->Open();
				}
			}
			self->_sync->Wait();
			_system_input_struct si;
			IO::ConsoleEventDesc ce;
			si.designation = si.data[0] = si.data[1] = si.data[2] = 0;
			ce.Event = IO::ConsoleEvent::EndOfFile;
			ce.Height = ce.Width = 0;
			self->_inputs.InsertLast(ce);
			self->_sys_inputs.InsertLast(si);
			self->_counter->Open();
			self->_sys_counter->Open();
			self->_sync->Open();
			return 0;
		}
		Console::Console(const ConsoleDesc & desc)
		{
			Array<string> args(4);
			SafePointer<IChannelServer> attach_io = CreateChannelServer();
			args << L"--xx-adnecte" << L"";
			attach_io->GetChannelPath(args[1]);
			if (desc.flags & CreateConsoleFlagSetTitle) args << L"--titulus" << desc.title;
			SafePointer<Process> console_process = CreateProcess(desc.xc_path, &args);
			if (!console_process) throw InvalidArgumentException();
			SafePointer<IChannel> attach_channel = attach_io->Accept();
			if (!attach_channel) throw IO::FileAccessException(IO::Error::CreateFailure);
			if (desc.flags & CreateConsoleFlagSetIcon) {
				Request req;
				req.verb = 1;
				req.data = desc.icon;
				if (!attach_channel->SendRequest(req)) throw IO::FileAccessException(IO::Error::WriteFailure);
			}
			if (desc.flags & CreateConsoleFlagSetPreset) {
				Request req;
				req.verb = 3;
				req.data = desc.preset;
				if (!attach_channel->SendRequest(req)) throw IO::FileAccessException(IO::Error::WriteFailure);
			}
			Request req;
			req.verb = 4;
			if (!attach_channel->SendRequest(req)) throw IO::FileAccessException(IO::Error::WriteFailure);
			if (!attach_channel->ReadRequest(req)) throw IO::FileAccessException(IO::Error::ReadFailure);
			if (req.verb != 5) throw InvalidStateException();
			auto con_path = string(req.data->GetBuffer(), req.data->Length(), Encoding::UTF8);
			_channel = ConnectChannel(con_path);
			attach_io.SetReference(0);
			attach_channel.SetReference(0);
			_sync = CreateSemaphore(1);
			_counter = CreateSemaphore(0);
			_sys_counter = CreateSemaphore(0);
			if (!_sync || !_counter || !_sys_counter) throw OutOfMemoryException();
			_thread = CreateThread(_input_thread, this);
			if (!_thread) throw OutOfMemoryException();
		}
		Console::~Console(void) { Request req; req.verb = 0; _channel->SendRequest(req); _thread->Wait(); }
		void Console::SetWindowTitle(const string & text)
		{
			Request req;
			req.verb = 2;
			req.data = text.NormalizedForm().EncodeSequence(Encoding::UTF8, false);
			_channel->SendRequest(req);
		}
		void Console::SetWindowIcon(DataBlock * icon)
		{
			Request req;
			req.verb = 1;
			req.data.SetRetain(icon);
			_channel->SendRequest(req);
		}
		void Console::Print(const string & text)
		{
			Request req;
			req.verb = 6;
			req.data = text.NormalizedForm().EncodeSequence(Encoding::UTF8, false);
			_channel->SendRequest(req);
		}
		void Console::SetForegroundColor(Color color)
		{
			Request req;
			req.verb = 8;
			req.data = new DataBlock(4);
			req.data->SetLength(4);
			*reinterpret_cast<Color *>(req.data->GetBuffer()) = color;
			_channel->SendRequest(req);
		}
		void Console::SetForegroundColorIndex(int index)
		{
			Request req;
			req.verb = 7;
			req.data = new DataBlock(4);
			req.data->SetLength(4);
			*reinterpret_cast<int *>(req.data->GetBuffer()) = index;
			_channel->SendRequest(req);
		}
		void Console::SetBackgroundColor(Color color)
		{
			Request req;
			req.verb = 10;
			req.data = new DataBlock(4);
			req.data->SetLength(4);
			*reinterpret_cast<Color *>(req.data->GetBuffer()) = color;
			_channel->SendRequest(req);
		}
		void Console::SetBackgroundColorIndex(int index)
		{
			Request req;
			req.verb = 9;
			req.data = new DataBlock(4);
			req.data->SetLength(4);
			*reinterpret_cast<int *>(req.data->GetBuffer()) = index;
			_channel->SendRequest(req);
		}
		void Console::ClearScreen(void)
		{
			Request req;
			req.verb = 11;
			_channel->SendRequest(req);
		}
		void Console::ClearLine(void)
		{
			Request req;
			req.verb = 12;
			_channel->SendRequest(req);
		}
		void Console::SetCaretPosition(const Point & pos)
		{
			Request req;
			req.verb = 13;
			req.data = new DataBlock(sizeof(Point));
			req.data->SetLength(sizeof(Point));
			*reinterpret_cast<Point *>(req.data->GetBuffer()) = pos;
			_channel->SendRequest(req);
		}
		Point Console::GetCaretPosition(void)
		{
			Point result;
			Request req;
			req.verb = 14;
			if (!_channel->SendRequest(req)) throw InvalidStateException();
			_sys_counter->Wait();
			_sync->Wait();
			auto & value = _sys_inputs.GetFirst()->GetValue();
			if (value.designation == 13) {
				result = Point(value.data[0], value.data[1]);
				_sys_inputs.RemoveFirst();
			} else if (value.designation == 0) {
				_sys_counter->Open();
				result = Point(-1, -1);
			} else result = Point(-1, -1);
			_sync->Open();
			if (result.x < 0) throw InvalidStateException();
			return result;
		}
		Point Console::GetScreenBufferDimensions(void)
		{
			Point result;
			Request req;
			req.verb = 15;
			if (!_channel->SendRequest(req)) throw InvalidStateException();
			_sys_counter->Wait();
			_sync->Wait();
			auto & value = _sys_inputs.GetFirst()->GetValue();
			if (value.designation == 16) {
				result = Point(value.data[0], value.data[1]);
				_sys_inputs.RemoveFirst();
			} else if (value.designation == 0) {
				_sys_counter->Open();
				result = Point(-1, -1);
			} else result = Point(-1, -1);
			_sync->Open();
			if (result.x < 0) throw InvalidStateException();
			return result;
		}
		void Console::CreateScreenBuffer(bool scrollable)
		{
			Request req;
			req.verb = 17;
			if (scrollable) {
				req.data = new DataBlock(1);
				req.data->Append(1);
			}
			_channel->SendRequest(req);
		}
		void Console::CancelScreenBuffer(void)
		{
			Request req;
			req.verb = 18;
			_channel->SendRequest(req);
		}
		void Console::SwapScreenBuffers(void)
		{
			Request req;
			req.verb = 19;
			_channel->SendRequest(req);
		}
		void Console::SetIOMode(IO::ConsoleInputMode mode)
		{
			Request req;
			req.verb = 20;
			req.data = new DataBlock(sizeof(uint));
			req.data->SetLength(sizeof(uint));
			if (mode == IO::ConsoleInputMode::Raw) *reinterpret_cast<uint *>(req.data->GetBuffer()) = 0;
			else if (mode == IO::ConsoleInputMode::Echo) *reinterpret_cast<uint *>(req.data->GetBuffer()) = 1;
			else throw InvalidArgumentException();
			_channel->SendRequest(req);
		}
		void Console::ReadEvent(IO::ConsoleEventDesc & event)
		{
			Request req;
			req.verb = 21;
			if (!_channel->SendRequest(req)) throw InvalidStateException();
			_counter->Wait();
			_sync->Wait();
			event = _inputs.GetFirst()->GetValue();
			if (event.Event == IO::ConsoleEvent::EndOfFile) _counter->Open();
			else _inputs.RemoveFirst();
			_sync->Open();
		}
		void Console::SetCaretState(const CaretStateDesc & desc)
		{
			Request req;
			req.verb = 25;
			req.data = new DataBlock(sizeof(desc));
			req.data->SetLength(sizeof(desc));
			*reinterpret_cast<CaretStateDesc *>(req.data->GetBuffer()) = desc;
			_channel->SendRequest(req);
		}
		void Console::RevertCaretState(void)
		{
			Request req;
			req.verb = 26;
			_channel->SendRequest(req);
		}
		void Console::SetFontAttributes(uint mask, uint set)
		{
			Request req;
			req.verb = 27;
			req.data = new DataBlock(1);
			req.data->SetLength(8);
			reinterpret_cast<uint *>(req.data->GetBuffer())[0] = mask;
			reinterpret_cast<uint *>(req.data->GetBuffer())[1] = set;
			_channel->SendRequest(req);
		}
		void Console::RevertFontAttributes(uint mask)
		{
			Request req;
			req.verb = 28;
			req.data = new DataBlock(1);
			req.data->SetLength(4);
			reinterpret_cast<uint *>(req.data->GetBuffer())[0] = mask;
			_channel->SendRequest(req);
		}
		void Console::WritePalette(uint flags, int index, Color color)
		{
			Request req;
			req.verb = 29;
			req.data = new DataBlock(1);
			req.data->SetLength(12);
			reinterpret_cast<uint *>(req.data->GetBuffer())[0] = flags;
			reinterpret_cast<uint *>(req.data->GetBuffer())[1] = index;
			reinterpret_cast<uint *>(req.data->GetBuffer())[2] = color.Value;
			_channel->SendRequest(req);
		}
		void Console::OverrideDefaults(void)
		{
			Request req;
			req.verb = 30;
			_channel->SendRequest(req);
		}
		void Console::SetCloseDetachedConsole(bool close)
		{
			Request req;
			req.verb = 31;
			req.data = new DataBlock(1);
			req.data->SetLength(4);
			*reinterpret_cast<uint *>(req.data->GetBuffer()) = close;
			_channel->SendRequest(req);
		}
		void Console::SetHorizontalTabulation(int size)
		{
			Request req;
			req.verb = 32;
			req.data = new DataBlock(1);
			req.data->SetLength(4);
			*reinterpret_cast<uint *>(req.data->GetBuffer()) = size;
			_channel->SendRequest(req);
		}
		void Console::SetVerticalTabulation(int size)
		{
			Request req;
			req.verb = 33;
			req.data = new DataBlock(1);
			req.data->SetLength(4);
			*reinterpret_cast<uint *>(req.data->GetBuffer()) = size;
			_channel->SendRequest(req);
		}
		void Console::SetWindowMargins(int size)
		{
			Request req;
			req.verb = 34;
			req.data = new DataBlock(1);
			req.data->SetLength(4);
			*reinterpret_cast<uint *>(req.data->GetBuffer()) = size;
			_channel->SendRequest(req);
		}
		void Console::SetWindowBackground(Color color)
		{
			Request req;
			req.verb = 35;
			req.data = new DataBlock(1);
			req.data->SetLength(4);
			*reinterpret_cast<uint *>(req.data->GetBuffer()) = color.Value;
			_channel->SendRequest(req);
		}
		void Console::SetWindowBlurBehind(double power)
		{
			Request req;
			req.verb = 36;
			req.data = new DataBlock(1);
			req.data->SetLength(8);
			*reinterpret_cast<double *>(req.data->GetBuffer()) = power;
			_channel->SendRequest(req);
		}
		void Console::SetWindowFontHeight(int height)
		{
			Request req;
			req.verb = 37;
			req.data = new DataBlock(1);
			req.data->SetLength(4);
			*reinterpret_cast<uint *>(req.data->GetBuffer()) = height;
			_channel->SendRequest(req);
		}
		void Console::SetWindowFont(const string & font_face, int height)
		{
			Request req;
			req.verb = 38;
			req.data = new DataBlock(1);
			req.data->SetLength(4 + font_face.GetEncodedLength(Encoding::UTF8));
			*reinterpret_cast<uint *>(req.data->GetBuffer()) = height;
			font_face.Encode(req.data->GetBuffer() + 4, Encoding::UTF8, false);
			_channel->SendRequest(req);
		}
		void Console::PushCaretPosition(void)
		{
			Request req;
			req.verb = 39;
			_channel->SendRequest(req);
		}
		void Console::PopCaretPosition(void)
		{
			Request req;
			req.verb = 40;
			_channel->SendRequest(req);
		}
		void Console::SetScrollingRange(int from_line, int num_lines)
		{
			Request req;
			req.verb = 41;
			req.data = new DataBlock(1);
			req.data->SetLength(8);
			reinterpret_cast<int *>(req.data->GetBuffer())[0] = from_line;
			reinterpret_cast<int *>(req.data->GetBuffer())[1] = num_lines;
			_channel->SendRequest(req);
		}
		void Console::ResetScrollingRange(void) { SetScrollingRange(-1, -1); }
		void Console::ScrollContent(int lines)
		{
			Request req;
			req.verb = 42;
			req.data = new DataBlock(1);
			req.data->SetLength(4);
			reinterpret_cast<int *>(req.data->GetBuffer())[0] = lines;
			_channel->SendRequest(req);
		}
		void Console::SetBackbufferStretchMode(Windows::ImageRenderMode mode)
		{
			Request req;
			req.verb = 43;
			req.data = new DataBlock(1);
			req.data->SetLength(4);
			if (mode == Windows::ImageRenderMode::Blit) reinterpret_cast<uint *>(req.data->GetBuffer())[0] = 0;
			else if (mode == Windows::ImageRenderMode::Stretch) reinterpret_cast<uint *>(req.data->GetBuffer())[0] = 1;
			else if (mode == Windows::ImageRenderMode::FitKeepAspectRatio) reinterpret_cast<uint *>(req.data->GetBuffer())[0] = 2;
			else if (mode == Windows::ImageRenderMode::CoverKeepAspectRatio) reinterpret_cast<uint *>(req.data->GetBuffer())[0] = 3;
			else throw InvalidArgumentException();
			_channel->SendRequest(req);
		}
		void Console::CreateBackbuffer(int width, int height)
		{
			Request req;
			req.verb = 44;
			req.data = new DataBlock(1);
			req.data->SetLength(8);
			reinterpret_cast<uint *>(req.data->GetBuffer())[0] = width;
			reinterpret_cast<uint *>(req.data->GetBuffer())[1] = height;
			_channel->SendRequest(req);
		}
		void Console::LoadBackbuffer(const string & path)
		{
			Request req;
			req.verb = 45;
			req.data = IO::ExpandPath(path).EncodeSequence(Encoding::UTF8, false);
			_channel->SendRequest(req);
		}
		void Console::ResetBackbuffer(void)
		{
			Request req;
			req.verb = 46;
			_channel->SendRequest(req);
		}
		void Console::AccessBackbuffer(IPC::ISharedMemory ** memory, int * width, int * height)
		{
			Request req;
			req.verb = 47;
			if (!_channel->SendRequest(req)) throw InvalidStateException();
			int result[3];
			_sys_counter->Wait();
			_sync->Wait();
			auto & value = _sys_inputs.GetFirst()->GetValue();
			if (value.designation == 48) {
				result[0] = value.data[0];
				result[1] = value.data[1];
				result[2] = value.data[2];
				_sys_inputs.RemoveFirst();
			} else if (value.designation == 0) {
				_sys_counter->Open();
				result[0] = result[1] = result[2] = 0;
			} else result[0] = result[1] = result[2] = 0;
			_sync->Open();
			if (result[1] && result[2]) {
				if (memory) *memory = IPC::CreateSharedMemory(L"xcmem" + string(result[0]), 4 * result[1] * result[2], IPC::SharedMemoryOpenExisting);
				if (width) *width = result[1];
				if (height) *height = result[2];
			} else {
				if (memory) *memory = 0;
				if (width) *width = 0;
				if (height) *height = 0;
			}
		}
		void Console::SynchronizeBackbuffer(void)
		{
			Request req;
			req.verb = 49;
			_channel->SendRequest(req);
		}
	}
}