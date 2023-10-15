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
					// TODO: PROCESS EVENT
				} else if (req.verb == 23) {
					// TODO: PROCESS EVENT
				} else if (req.verb == 24) {
					// TODO: PROCESS EVENT
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
	}
}