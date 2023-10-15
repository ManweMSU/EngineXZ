#include "xc_io.h"

#include "common/xc_channel.h"

namespace Engine
{
	namespace XC
	{
		// XC verbs
		// 0  - close connection
		// 1  - set icon
		// 2  - set title
		// 3  - set preset
		// 4  - init console
		// 5  - init console responce
		// 6  - print
		// 7  - set foreground color (index)
		// 8  - set foreground color (RGBA)
		// 9  - set background color (index)
		// 10 - set background color (RGBA)
		// 11 - clear screen
		// 12 - clear line
		// 13 - set caret position
		// 14 - get caret position - responds with code 13
		// 15 - get screen buffer dimensions
		// 16 - get screen buffer dimensions responce
		// 17 - new screen buffer
		// 18 - cancel screen buffer
		// 19 - swap screen buffer
		// 20 - set IO mode
		// 21 - ready to accept an input
		// 22 - character input
		// 23 - keyboard input
		// 24 - buffer size alternation input

		// TODO: ADD
		// caret shape control API (blinking, weight and shape)
		// window attributes control API (color, margins, blur, font and font height)
		// misc control (tab size, close on end-of-stream)
		// text attribute control (palette, default colors, font attributes, reversion and defaulting)
		// screen buffer extended control (caret position push/pop, scroll region control, background image positioning)
		// background image API (create of size, remove, publish, sync)

		class AttachIO : public IAttachIO, IOutputCallback
		{
			struct _new_io_struct {
				SafePointer<IChannel> channel;
				SafePointer<AttachIO> io;
			};
			struct _io_state {
				IO::ConsoleInputMode _mode;
			};
			_io_state _current_state;
			string _title, _preset;
			SafePointer<DataBlock> _override_preset, _override_icon;
			SafePointer<IChannel> _attach_io;
			SafePointer<IChannelServer> _io_server;
			SafePointer<IChannel> _output_sink;
			ObjectArray<IChannel> _clients;
			SafePointer<Semaphore> _sync;
			SafePointer<ConsoleState> _console;
		public:
			static void _respond(IChannel * channel, uint verb, const void * data, int length)
			{
				Request req;
				req.verb = verb;
				if (length) {
					req.data = new DataBlock(1);
					req.data->SetLength(length);
					MemoryCopy(req.data->GetBuffer(), data, length);
				}
				channel->SendRequest(req);
			}
			void _process_request(uint verb, DataBlock * data, IChannel * channel)
			{
				if (verb == 1 && data) {
					SafePointer<Streaming::Stream> stream = new Streaming::MemoryStream(data->GetBuffer(), data->Length());
					SafePointer<Codec::Image> icon = Codec::DecodeImage(stream);
					_console->SetWindowIcon(icon);
				} else if (verb == 2 && data) {
					_console->SetTitle(string(data->GetBuffer(), data->Length(), Encoding::UTF8));
				} else if (verb == 6 && data) {
					int length = MeasureSequenceLength(data->GetBuffer(), data->Length(), Encoding::UTF8, Encoding::UTF32);
					if (_current_state._mode == IO::ConsoleInputMode::Echo) {
						for (int i = 0; i < data->Length(); i++) if (data->ElementAt(i) == '\n') length++;
					}
					Array<uint> ucs(length);
					ucs.SetLength(length);
					ConvertEncoding(ucs.GetBuffer(), data->GetBuffer(), data->Length(), Encoding::UTF8, Encoding::UTF32);
					if (_current_state._mode == IO::ConsoleInputMode::Echo) {
						for (int i = 0; i < ucs.Length(); i++) if (ucs[i] == L'\n') ucs.Insert(L'\r', i + 1);
					}
					_console->Print(ucs.GetBuffer(), ucs.Length());
				} else if (verb == 7 && data && data->Length() == 4) {
					_console->SetForegroundColorIndex(*reinterpret_cast<int *>(data->GetBuffer()));
				} else if (verb == 8 && data && data->Length() == 4) {
					_console->SetForegroundColor(*reinterpret_cast<Color *>(data->GetBuffer()));
				} else if (verb == 9 && data && data->Length() == 4) {
					_console->SetBackgroundColorIndex(*reinterpret_cast<int *>(data->GetBuffer()));
				} else if (verb == 10 && data && data->Length() == 4) {
					_console->SetBackgroundColor(*reinterpret_cast<Color *>(data->GetBuffer()));
				} else if (verb == 11) {
					_console->ClearScreen();
				} else if (verb == 12) {
					Point pos;
					_console->GetCaretPosition(pos);
					_console->SetCaretPosition(Point(0, pos.y));
					_console->Erase(false, 0);
				} else if (verb == 13 && data && data->Length() == 8) {
					Point pos;
					pos.x = reinterpret_cast<int *>(data->GetBuffer())[0];
					pos.y = reinterpret_cast<int *>(data->GetBuffer())[1];
					_console->SetCaretPosition(pos);
				} else if (verb == 14) {
					Point pos;
					_console->GetCaretPosition(pos);
					_respond(channel, 13, &pos, sizeof(pos));
				} else if (verb == 15) {
					Point pos;
					pos.x = _console->GetBufferWidth();
					pos.y = _console->GetBufferLimitHeight();
					_respond(channel, 16, &pos, sizeof(pos));
				} else if (verb == 17) {
					_console->CreateScreenBuffer(_console->GetWindowSize(), data && data->Length() && data->ElementAt(0));
				} else if (verb == 18) {
					_console->RemoveScreenBuffer();
				} else if (verb == 19) {
					_console->SwapScreenBuffers();
				} else if (verb == 20 && data && data->Length() == 4) {
					// TODO: IMPLEMENT
					// 20 - set IO mode
				} else if (verb == 21) {
					// TODO: IMPLEMENT
					// 21 - ready to accept an input
				}
			}
			static int _io_thread(void * arg)
			{
				auto data = reinterpret_cast<_new_io_struct *>(arg);
				auto self = data->io;
				auto channel = data->channel;
				delete data;
				while (true) {
					Request req;
					if (!channel->ReadRequest(req)) break;
					if (req.verb == 0) break;
					Windows::GetWindowSystem()->SubmitTask(CreateFunctionalTask([v = req.verb, d = req.data, channel, self]() { self->_process_request(v, d, channel); }));
				}
				self->_sync->Wait();
				for (int i = 0; i < self->_clients.Length(); i++) if (self->_clients.ElementAt(i) == channel.Inner()) {
					self->_clients.Remove(i);
					break;
				}
				if (!self->_clients.Length()) {
					Windows::GetWindowSystem()->SubmitTask(CreateFunctionalTask([s = self]() { s->_console->SetEndOfStream(); }));
				}
				self->_sync->Open();
				return 0;
			}
			static int _listener_thread(void * arg)
			{
				auto self = reinterpret_cast<AttachIO *>(arg);
				while (true) {
					SafePointer<IChannel> channel = self->_io_server->Accept();
					if (channel) {
						self->_sync->Wait();
						self->_clients.Append(channel);
						self->_sync->Open();
						auto data = new (std::nothrow) _new_io_struct;
						if (data) {
							data->channel = channel;
							data->io.SetRetain(self);
							SafePointer<Thread> io = CreateThread(_io_thread, data);
							if (!io) delete data;
						}
					} else break;
				}
				self->Release();
				return 0;
			}
		public:
			AttachIO(const string & path) : _clients(0x10)
			{
				_current_state._mode = IO::ConsoleInputMode::Echo;
				_attach_io = ConnectChannel(path);
				_sync = CreateSemaphore(1);
			}
			virtual ~AttachIO(void) override {}
			virtual bool Communicate(const string & init_title, const string & init_preset) noexcept override
			{
				try {
					_title = init_title;
					_preset = init_preset;
					while (true) {
						Request req;
						_attach_io->ReadRequest(req);
						if (req.verb == 0) { _attach_io.SetReference(0); return false; }
						else if (req.verb == 1) _override_icon = req.data;
						else if (req.verb == 2) _title = string(req.data->GetBuffer(), req.data->Length(), Encoding::UTF8);
						else if (req.verb == 3) _override_preset = req.data;
						else if (req.verb == 4) break;
						else { _attach_io.SetReference(0); return false; }
					}
					string path;
					_io_server = CreateChannelServer();
					_io_server->GetChannelPath(path);
					Request req;
					req.verb = 5;
					req.data = path.EncodeSequence(Encoding::UTF8, false);
					_attach_io->SendRequest(req);
					_attach_io.SetReference(0);
					return true;
				} catch (...) { return false; }
			}
			virtual bool CreateConsole(ConsoleState ** state, ConsoleCreateMode * mode) noexcept override
			{
				try {
					if (_override_preset) LoadConsolePreset(_title, _override_preset, _console.InnerRef(), mode);
					else LoadConsolePreset(_title, _preset, _console.InnerRef(), mode);
					*state = _console.Inner();
					_console->Retain();
					if (_override_icon) {
						SafePointer<Codec::Image> icon;
						Streaming::MemoryStream stream(_override_icon->GetBuffer(), _override_icon->Length());
						icon = Codec::DecodeImage(&stream);
						(*state)->SetWindowIcon(icon);
					}
					(*state)->SetCallback(this);
					return true;
				} catch (...) { return false; }
			}
			virtual bool LaunchService(void) noexcept override
			{
				Retain();
				SafePointer<Thread> listener = CreateThread(_listener_thread, this);
				if (!listener) { Release(); return false; }
				return true;
			}
			virtual bool OutputKey(uint code, uint flags) noexcept override
			{
				// TODO: IMPLEMENT
				return false;
			}
			virtual void OutputText(uint ucs) noexcept override
			{
				// TODO: IMPLEMENT
			}
			virtual void WindowResize(int width, int height) noexcept override {}
			virtual void ScreenBufferResize(int width, int height) noexcept override
			{
				// TODO: IMPLEMENT
			}
			virtual void Terminate(void) noexcept override
			{
				// TODO: IMPLEMENT
			}
		};

		IAttachIO * CreateAttachIO(const string & io) { return new AttachIO(io); }
	}
}