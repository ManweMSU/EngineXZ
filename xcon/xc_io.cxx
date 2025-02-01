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
		// 25 - caret state control
		// 26 - caret state revert
		// 27 - set font attribute
		// 28 - revert font attribute
		// 29 - palette control
		// 30 - set as default
		// 31 - set close on detach
		// 32 - set horizontal tabulation
		// 33 - set vertical tabulation
		// 34 - set window margins
		// 35 - set window background
		// 36 - set window blur behind
		// 37 - set window font height
		// 38 - set window font face and height
		// 39 - push caret position
		// 40 - pop caret position
		// 41 - set scrolling region
		// 42 - scroll region
		// 43 - set backbuffer stretching mode
		// 44 - create backbuffer
		// 45 - load backbuffer
		// 46 - reset backbuffer
		// 47 - give backbuffer access
		// 48 - give backbuffer access responce (shared memory object)
		// 49 - backbuffer synchronize
		// 50 - set backbuffer

		class AttachIO : public IAttachIO, IOutputCallback
		{
			struct _new_io_struct {
				SafePointer<IChannel> channel;
				SafePointer<AttachIO> io;
			};
			struct _input_char
			{
				uint ucs;
				Point pos;
			};
			struct _io_state {
				bool _eos;
				IO::ConsoleInputMode _mode;
				Array<_input_char> _buffer = Array<_input_char>(0x100);
				int _caret_index;
			};
			struct _client_state {
				SafePointer<IChannel> channel;
				uint read_req;
			};
			struct _event {
				uint code;
				uint args[2];
			};
			_io_state _current_state;
			string _title, _preset;
			SafePointer<DataBlock> _override_preset, _override_icon;
			SafePointer<IChannel> _attach_io;
			SafePointer<IChannelServer> _io_server;
			SafePointer<IChannel> _output_sink;
			SafePointer<Semaphore> _sync;
			SafePointer<ConsoleState> _console;
			SafePointer<TaskQueue> _loader_queue;
			SafePointer<Thread> _loader_thread;
			Array<_client_state> _clients;
			Volumes::Queue<_event> _events;
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
			void _send_event(IChannel * channel, const _event & e)
			{
				Request req;
				req.verb = e.code;
				req.data = new DataBlock(1);
				req.data->SetLength(8);
				MemoryCopy(req.data->GetBuffer(), &e.args, 8);
				channel->SendRequest(req);
			}
			void _emit_event(const _event & e)
			{
				IChannel * cs = 0;
				_sync->Wait();
				for (auto & c : _clients) if (c.read_req) { cs = c.channel; c.read_req--; }
				if (!cs) _events.Push(e);
				_sync->Open();
				if (cs) _send_event(cs, e);
			}
			void _set_io_mode(uint mode)
			{
				auto prev = _current_state._mode;
				if (mode == 0) _current_state._mode = IO::ConsoleInputMode::Raw;
				else if (mode == 1) _current_state._mode = IO::ConsoleInputMode::Echo;
				if (prev != _current_state._mode) {
					_current_state._caret_index = 0;
					_current_state._buffer.Clear();
				}
			}
			void _canonical_update_caret(void)
			{
				if (_current_state._caret_index < _current_state._buffer.Length()) {
					_console->SetCaretPosition(_current_state._buffer[_current_state._caret_index].pos);
				} else if (_current_state._buffer.Length()) {
					auto pos = _current_state._buffer.LastElement().pos;
					pos.x++;
					if (pos.x >= _console->GetBufferWidth()) { pos.x = 0; pos.y++; }
					_console->SetCaretPosition(pos);
				}
			}
			void _reprint_chars(_input_char * chars, int count)
			{
				Point pn;
				_console->GetCaretPosition(pn);
				for (int i = 0; i < count; i++) if (chars[i].pos.x >= 0) {
					auto & chr = chars[i];
					uint spc = L' ';
					_console->SetCaretPosition(chr.pos);
					_console->Print(&spc, 1);
				}
				_console->SetCaretPosition(pn);
				for (int i = 0; i < count; i++) {
					auto & chr = chars[i];
					_console->GetCaretPosition(pn);
					if (pn.x >= _console->GetBufferWidth()) {
						uint crlf[] = { L'\n', L'\r' };
						_console->Print(crlf, 2);
						_console->GetCaretPosition(pn);
					}
					chr.pos = pn;
					_console->Print(&chr.ucs, 1);
				}
				_console->GetCaretPosition(pn);
				if (pn.x >= _console->GetBufferWidth()) {
					uint crlf[] = { L'\n', L'\r' };
					_console->Print(crlf, 2);
				}
				_canonical_update_caret();
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
					_console->ClearLine();
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
					_set_io_mode(*reinterpret_cast<uint *>(data->GetBuffer()));
				} else if (verb == 21) {
					_event e;
					_sync->Wait();
					if (_events.IsEmpty()) {
						e.code = 0;
						for (auto & c : _clients) if (c.channel.Inner() == channel) { c.read_req++; break; }
					} else e = _events.Pop();
					_sync->Open();
					if (e.code) _send_event(channel, e);
				} else if (verb == 25 && data && data->Length() == 16) {
					uint flags = reinterpret_cast<uint *>(data->GetBuffer())[0];
					uint shape = reinterpret_cast<uint *>(data->GetBuffer())[1];
					double weight = reinterpret_cast<double *>(data->GetBuffer())[1];
					if (flags & 0x01) {
						if (flags & 0x08) _console->SetCaretBlinks(true);
						else _console->SetCaretBlinks(false);
					}
					if (flags & 0x02) {
						if (shape == 0) _console->SetCaretStyle(CaretStyle::Null);
						else if (shape == 1) _console->SetCaretStyle(CaretStyle::Horizontal);
						else if (shape == 2) _console->SetCaretStyle(CaretStyle::Vertical);
						else if (shape == 3) _console->SetCaretStyle(CaretStyle::Cell);
					}
					if (flags & 0x04) _console->SetCaretWeight(weight);
				} else if (verb == 26) {
					_console->RevertCaretVisuals();
				} else if (verb == 27 && data && data->Length() == 8) {
					uint mask = reinterpret_cast<uint *>(data->GetBuffer())[0];
					uint set = reinterpret_cast<uint *>(data->GetBuffer())[1];
					_console->SetAttributionFlags(mask, set);
				} else if (verb == 28 && data && data->Length() == 4) {
					uint mask = reinterpret_cast<uint *>(data->GetBuffer())[0];
					_console->RevertAttributionFlags(mask);
				} else if (verb == 29 && data && data->Length() == 12) {
					uint flags = reinterpret_cast<uint *>(data->GetBuffer())[0];
					uint index = reinterpret_cast<uint *>(data->GetBuffer())[1];
					uint color = reinterpret_cast<uint *>(data->GetBuffer())[2];
					_console->WritePalette(flags, index, color);
				} else if (verb == 30) {
					_console->OverrideDefaults();
				} else if (verb == 31 && data && data->Length() > 0) {
					_console->SetCloseOnEndOfStream(data->ElementAt(0) != 0);
				} else if (verb == 32 && data && data->Length() == 4) {
					int size = reinterpret_cast<uint *>(data->GetBuffer())[0];
					auto current = _console->GetTabulationSize();
					_console->SetTabulationSize(Point(max(size, 1), current.y));
				} else if (verb == 33 && data && data->Length() == 4) {
					int size = reinterpret_cast<uint *>(data->GetBuffer())[0];
					auto current = _console->GetTabulationSize();
					_console->SetTabulationSize(Point(current.x, max(size, 1)));
				} else if (verb == 34 && data && data->Length() == 4) {
					uint size = reinterpret_cast<uint *>(data->GetBuffer())[0];
					_console->SetMargin(size);
				} else if (verb == 35 && data && data->Length() == 4) {
					uint color = reinterpret_cast<uint *>(data->GetBuffer())[0];
					_console->SetWindowColor(color);
				} else if (verb == 36 && data && data->Length() == 8) {
					double blur = reinterpret_cast<double *>(data->GetBuffer())[0];
					_console->SetBlurBehind(blur);
				} else if (verb == 37 && data && data->Length() == 4) {
					string face;
					int height;
					_console->GetFont(face, height);
					uint new_height = reinterpret_cast<uint *>(data->GetBuffer())[0];
					_console->SetFont(face, new_height);
				} else if (verb == 38 && data && data->Length() >= 4) {
					uint height = reinterpret_cast<uint *>(data->GetBuffer())[0];
					auto face = string(data->GetBuffer() + 4, data->Length() - 4, Encoding::UTF8);
					_console->SetFont(face, height);
				} else if (verb == 39) {
					_console->PushCaretPosition();
				} else if (verb == 40) {
					_console->PopCaretPosition();
				} else if (verb == 41 && data && data->Length() == 8) {
					int from = reinterpret_cast<int *>(data->GetBuffer())[0];
					int lines = reinterpret_cast<int *>(data->GetBuffer())[1];
					_console->SetScrollingRectangle(from, lines);
				} else if (verb == 42 && data && data->Length() == 4) {
					int lines = reinterpret_cast<int *>(data->GetBuffer())[0];
					_console->ScrollCurrentRange(lines);
				} else if (verb == 43 && data && data->Length() == 4) {
					uint mode = reinterpret_cast<uint *>(data->GetBuffer())[0];
					if (mode == 0) _console->SetPictureMode(Windows::ImageRenderMode::Blit);
					else if (mode == 1) _console->SetPictureMode(Windows::ImageRenderMode::Stretch);
					else if (mode == 2) _console->SetPictureMode(Windows::ImageRenderMode::FitKeepAspectRatio);
					else if (mode == 3) _console->SetPictureMode(Windows::ImageRenderMode::CoverKeepAspectRatio);
				} else if (verb == 44 && data && data->Length() == 8) {
					try {
						uint w = reinterpret_cast<uint *>(data->GetBuffer())[0];
						uint h = reinterpret_cast<uint *>(data->GetBuffer())[1];
						SafePointer<WrappedPicture> picture = new WrappedPicture(w, h);
						_console->SetPicture(picture);
					} catch (...) {}
				} else if (verb == 45 && data && data->Length()) {
					try {
						auto path = string(data->GetBuffer(), data->Length(), Encoding::UTF8);
						_loader_queue->SubmitTask(CreateFunctionalTask([path, con_ref = _console]() {
							SafePointer<Streaming::Stream> input = new Streaming::FileStream(path, Streaming::AccessRead, Streaming::OpenExisting);
							SafePointer<Codec::Frame> frame = Codec::DecodeFrame(input);
							if (!frame) throw Exception();
							SafePointer<WrappedPicture> picture = new WrappedPicture(frame);
							Windows::GetWindowSystem()->SubmitTask(CreateFunctionalTask([picture, con_ref]() {
								con_ref->SetPicture(picture);
							}));
						}));
					} catch (...) {}
				} else if (verb == 46) {
					_console->SetPicture(0);
				} else if (verb == 47) {
					int data[3] = { 0, 0, 0 };
					try {
						auto picture = _console->GetPicture();
						if (picture) {
							if (!picture->IsShared()) {
								SafePointer<SharedMemoryPicture> shared = new SharedMemoryPicture(static_cast<WrappedPicture *>(picture)->GetFrame());
								_console->SetPicture(shared);
								picture = shared.Inner();
							}
							data[0] = static_cast<SharedMemoryPicture *>(picture)->ExposeMemoryIndex();
							data[1] = picture->GetWidth();
							data[2] = picture->GetHeight();
						}
					} catch (...) {}
					_respond(channel, 48, &data, sizeof(data));
				} else if (verb == 49) {
					_console->NotifyPictureUpdated();
				} else if (verb == 50 && data) {
					try {
						SafePointer<DataBlock> data_ref;
						data_ref.SetRetain(data);
						_loader_queue->SubmitTask(CreateFunctionalTask([data_ref, con_ref = _console]() {
							SafePointer<Streaming::Stream> input = new Streaming::MemoryStream(data_ref->GetBuffer(), data_ref->Length());
							SafePointer<Codec::Frame> frame = Codec::DecodeFrame(input);
							if (!frame) throw Exception();
							SafePointer<WrappedPicture> picture = new WrappedPicture(frame);
							Windows::GetWindowSystem()->SubmitTask(CreateFunctionalTask([picture, con_ref]() {
								con_ref->SetPicture(picture);
							}));
						}));
					} catch (...) {}
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
					Windows::GetWindowSystem()->SubmitTask(CreateFunctionalTask([v = req.verb, d = req.data, channel, self]() { try { self->_process_request(v, d, channel); } catch (...) {} }));
				}
				self->_sync->Wait();
				for (int i = 0; i < self->_clients.Length(); i++) if (self->_clients[i].channel.Inner() == channel.Inner()) {
					self->_clients.Remove(i);
					break;
				}
				if (!self->_clients.Length()) {
					Windows::GetWindowSystem()->SubmitTask(CreateFunctionalTask([s = self]() {
						s->_current_state._eos = true;
						s->_console->SetEndOfStream();
					}));
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
						_client_state client;
						client.channel = channel;
						client.read_req = 0;
						self->_sync->Wait();
						self->_clients.Append(client);
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
				_current_state._eos = false;
				_current_state._mode = IO::ConsoleInputMode::Echo;
				_current_state._caret_index = 0;
				_attach_io = ConnectChannel(path);
				_sync = CreateSemaphore(1);
			}
			virtual ~AttachIO(void) override
			{
				if (_loader_queue) _loader_queue->Break();
				if (_loader_thread) _loader_thread->Wait();
			}
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
				if (!_loader_queue) try {
					_loader_queue = new TaskQueue;
					if (!_loader_queue->ProcessAsSeparateThread(_loader_thread.InnerRef())) { _loader_queue.SetReference(0); return false; }
				} catch (...) { return false; }
				Retain();
				SafePointer<Thread> listener = CreateThread(_listener_thread, this);
				if (!listener) { Release(); return false; }
				return true;
			}
			virtual void CloseService(void) noexcept override { _io_server->Close(); }
			virtual bool OutputKey(uint code, uint flags) noexcept override
			{
				if (_current_state._eos) return false;
				if (_current_state._mode == IO::ConsoleInputMode::Raw) {
					_event e;
					e.code = 23;
					e.args[0] = code;
					e.args[1] = flags;
					_emit_event(e);
				} else {
					if (code == KeyCodes::Return && (flags & ~IO::ConsoleKeyFlagShift) == 0) {
						for (auto & c : _current_state._buffer) {
							_event e;
							e.code = 22;
							e.args[0] = c.ucs;
							e.args[1] = 0;
							_emit_event(e);
						}
						_event e;
						e.code = 22;
						e.args[0] = L'\n';
						e.args[1] = 0;
						_emit_event(e);
						_current_state._caret_index = _current_state._buffer.Length();
						_canonical_update_caret();
						_current_state._buffer.Clear();
						_current_state._caret_index = 0;
						uint crlf[] = { L'\n', L'\r' };
						_console->Print(crlf, 2);
						return true;
					} else if (code == KeyCodes::Back && flags == 0) {
						if (_current_state._caret_index > 0) {
							uint spc = L' ';
							_console->SetCaretPosition(_current_state._buffer[_current_state._caret_index - 1].pos);
							_console->Print(&spc, 1);
							_console->SetCaretPosition(_current_state._buffer[_current_state._caret_index - 1].pos);
							_current_state._buffer.Remove(_current_state._caret_index - 1);
							_current_state._caret_index--;
							_reprint_chars(_current_state._buffer.GetBuffer() + _current_state._caret_index, _current_state._buffer.Length() - _current_state._caret_index);
						}
						return true;
					} else if (code == KeyCodes::Delete && flags == 0) {
						if (_current_state._caret_index < _current_state._buffer.Length()) {
							uint spc = L' ';
							_console->SetCaretPosition(_current_state._buffer[_current_state._caret_index].pos);
							_console->Print(&spc, 1);
							_console->SetCaretPosition(_current_state._buffer[_current_state._caret_index].pos);
							_current_state._buffer.Remove(_current_state._caret_index);
							_reprint_chars(_current_state._buffer.GetBuffer() + _current_state._caret_index, _current_state._buffer.Length() - _current_state._caret_index);
						}
						return true;
					} else if (code == KeyCodes::Left && flags == 0) {
						_current_state._caret_index = max(_current_state._caret_index - 1, 0);
						_canonical_update_caret();
						return true;
					} else if (code == KeyCodes::Right && flags == 0) {
						_current_state._caret_index = min(_current_state._caret_index + 1, _current_state._buffer.Length());
						_canonical_update_caret();
						return true;
					} else if (code == KeyCodes::Home && flags == 0) {
						_current_state._caret_index = 0;
						_canonical_update_caret();
						return true;
					} else if (code == KeyCodes::End && flags == 0) {
						_current_state._caret_index = _current_state._buffer.Length();
						_canonical_update_caret();
						return true;
					} else if (code == KeyCodes::V && flags == IO::ConsoleKeyFlagControl) {
						if (Clipboard::IsFormatAvailable(Clipboard::Format::Text)) {
							string data;
							if (Clipboard::GetData(data) && data.Length()) {
								Array<uint> ucs(1);
								ucs.SetLength(data.GetEncodedLength(Encoding::UTF32));
								data.Encode(ucs.GetBuffer(), Encoding::UTF32, false);
								for (auto & c : ucs) {
									if (c >= 32) OutputText(c);
									else if (c == L'\n') OutputKey(KeyCodes::Return, 0);
								}
							}
						}
						return true;
					}
				}
				return false;
			}
			virtual void OutputText(uint ucs) noexcept override
			{
				if (_current_state._eos) return;
				if (_current_state._mode == IO::ConsoleInputMode::Raw) {
					_event e;
					e.code = 22;
					e.args[0] = ucs;
					e.args[1] = 0;
					_emit_event(e);
				} else {
					if (ucs >= 32) {
						_input_char in;
						in.ucs = ucs;
						in.pos = Point(-1, -1);
						_current_state._buffer.Insert(in, _current_state._caret_index);
						_current_state._caret_index++;
						_reprint_chars(_current_state._buffer.GetBuffer() + _current_state._caret_index - 1, _current_state._buffer.Length() - _current_state._caret_index + 1);
					}
				}
			}
			virtual void WindowResize(int width, int height) noexcept override {}
			virtual void ScreenBufferResize(int width, int height) noexcept override
			{
				if (_current_state._eos) return;
				if (_current_state._mode == IO::ConsoleInputMode::Raw) {
					if (!_console->IsBufferScrollable()) {
						_event e;
						e.code = 24;
						e.args[0] = width;
						e.args[1] = height;
						_sync->Wait();
						for (auto & c : _clients) _send_event(c.channel, e);
						_sync->Open();
					}
				} else {
					if (_current_state._buffer.Length()) {
						_console->SetCaretPosition(_current_state._buffer[0].pos);
						_reprint_chars(_current_state._buffer.GetBuffer(), _current_state._buffer.Length());
					}
				}
			}
			virtual void Terminate(void) noexcept override
			{
				if (_current_state._eos) return;
				_event e;
				e.code = e.args[0] = e.args[1] = 0;
				_sync->Wait();
				for (auto & c : _clients) _send_event(c.channel, e);
				_sync->Open();
			}
		};

		IAttachIO * CreateAttachIO(const string & io) { return new AttachIO(io); }
	}
}