#include "xx_machine.h"
#include "xx_com_xe.h"

#include "../xcon/client/xc_api.h"

#include "../xenv/xe_logger.h"
#include "../xenv/xe_conapi.h"
#include "../xenv/xe_filesys.h"
#include "../xenv/xe_imgapi.h"
#include "../xenv/xe_rtff.h"
#include "../xenv/xe_powerapi.h"
#include "../xenv/xe_reflapi.h"
#include "../xenv/xe_wndapi.h"
#include "../xenv/xe_tpgrph.h"
#include "../xenv/xe_netapi.h"
#include "../xenv/xe_crypto.h"
#include "../xenv/xe_commem.h"
#include "../xenv/xe_mm.h"

#include "../ximg/xi_module.h"
#include "../ximg/xi_resources.h"

#define XE_TRY_INTRO try {
#define XE_TRY_OUTRO(DRV) } catch (Engine::InvalidArgumentException &) { ectx.error_code = 3; ectx.error_subcode = 0; return DRV; } \
catch (Engine::InvalidFormatException &) { ectx.error_code = 4; ectx.error_subcode = 0; return DRV; } \
catch (Engine::InvalidStateException &) { ectx.error_code = 5; ectx.error_subcode = 0; return DRV; } \
catch (Engine::OutOfMemoryException &) { ectx.error_code = 2; ectx.error_subcode = 0; return DRV; } \
catch (Engine::IO::FileAccessException & e) { ectx.error_code = 6; ectx.error_subcode = e.code; return DRV; } \
catch (Engine::Exception &) { ectx.error_code = 1; ectx.error_subcode = 0; return DRV; } \
catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return DRV; }

namespace Engine
{
	namespace XX
	{
		struct EnvironmentConfiguration
		{
			string store_file;
			string xe_config;
			string xxf_path;
			string xc_path;
			string xx_hello;
			string xx_commands;
			string locale_override;
			string locale_default;
			string xc_channel_name;
			string xi_executable;
			string logger_file;
			uint logger_type;
		};
		struct LaunchConfiguration
		{
			SafePointer<DataBlock> xc_preset;
			SafePointer<DataBlock> xc_icon;
			SafePointer<DataBlock> xc_backbuffer;
			XI::Module::ExecutionSubsystem subsystem;
			string xc_title;
			bool require_xc;
			bool require_native_console;
			XE::ExecutionContext * primary_context;
		};

		class XLoggerSink : public Object, public XE::ILoggerSink
		{
		public:
			virtual Object * ExposeObject(void) noexcept override { return this; }
		};
		class NullLogger : public XLoggerSink
		{
		public:
			NullLogger(void) {}
			virtual ~NullLogger(void) override {}
			virtual void Log(const string & line) noexcept override {}
		};
		class ConsoleLogger : public XLoggerSink
		{
			SafePointer<IO::Console> _output;
		public:
			ConsoleLogger(void) { _output = new IO::Console(IO::GetStandardError(), IO::InvalidHandle); }
			virtual ~ConsoleLogger(void) override {}
			virtual void Log(const string & line) noexcept override { try { _output->WriteLine(line); } catch (...) {} }
		};
		class FileLogger : public XLoggerSink
		{
			SafePointer<Streaming::TextWriter> _output;
		public:
			FileLogger(const string & path)
			{
				SafePointer<Streaming::FileStream> stream = new Streaming::FileStream(path, Streaming::AccessWrite, Streaming::OpenAlways);
				_output = new Streaming::TextWriter(stream, Encoding::UTF8);
				if (stream->Length()) stream->Seek(0, Streaming::End);
				else _output->WriteEncodingSignature();
			}
			virtual ~FileLogger(void) override {}
			virtual void Log(const string & line) noexcept override { try { _output->WriteLine(line); } catch (...) {} }
		};
		class ConsoleBackground : public Object
		{
			SafePointer<XC::Console> _console;
			SafePointer<IPC::ISharedMemory> _memory;
			void * _data;
			int _width, _height;
		private:
			static bool _synchronize(Object * self) noexcept
			{
				try {
					reinterpret_cast<ConsoleBackground *>(self)->_console->SynchronizeBackbuffer();
					return true;
				} catch (...) { return false; }
			}
		public:
			ConsoleBackground(XC::Console * xc, IPC::ISharedMemory * mem, int w, int h) : _width(w), _height(h)
			{
				_console.SetRetain(xc);
				_memory.SetRetain(mem);
				if (!_memory->Map(&_data, IPC::SharedMemoryMapReadWrite)) throw OutOfMemoryException();
			}
			virtual ~ConsoleBackground(void) override { _memory->Unmap(); }
			virtual int GetWidth(void) noexcept { return _width; }
			virtual int GetHeight(void) noexcept { return _height; }
			virtual void * GetData(void) noexcept { return _data; }
			virtual void Synchronize(XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->SynchronizeBackbuffer();
				XE_TRY_OUTRO()
			}
			virtual SafePointer<Object> QueryContext(XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Object> context = XE::CreateDirectContext(_data, _width, _height, 4 * _width, this, _synchronize);
				return context;
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<Object> QueryFrame(int stride, Codec::PixelFormat format, Codec::AlphaMode alpha_mode, Codec::ScanOrigin scan_order, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Codec::Frame> org = new Codec::Frame(_width, _height, _width * 4,
					Codec::PixelFormat::B8G8R8A8, Codec::AlphaMode::Premultiplied, Codec::ScanOrigin::TopDown);
				MemoryCopy(org->GetData(), _data, _width * _height * 4);
				SafePointer<Codec::Frame> conv = org->ConvertFormat(format, alpha_mode, scan_order, stride);
				SafePointer<Object> wrapper = XE::CreateXFrame(conv);
				return wrapper;
				XE_TRY_OUTRO(0)
			}
			virtual void SynchronizeWithFrame(handle xframe, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Codec::Frame> frame = XE::ExtractFrameFromXFrame(xframe);
				if (frame->GetWidth() != _width || frame->GetHeight() != _height) throw InvalidArgumentException();
				SafePointer<Codec::Frame> native = frame->ConvertFormat(Codec::PixelFormat::B8G8R8A8, Codec::AlphaMode::Premultiplied,
					Codec::ScanOrigin::TopDown, _width * 4);
				MemoryCopy(_data, native->GetData(), _width * _height * 4);
				_console->SynchronizeBackbuffer();
				XE_TRY_OUTRO()
			}
		};
		class DeviceXC : public XE::IConsoleDevice
		{
			SafePointer<XC::Console> _console;
		public:
			DeviceXC(const string & path) { _console = new XC::Console(path); }
			DeviceXC(EnvironmentConfiguration & ec, LaunchConfiguration & lc)
			{
				XC::ConsoleDesc desc;
				desc.xc_path = ec.xc_path;
				desc.title = lc.xc_title;
				desc.icon = lc.xc_icon;
				desc.preset = lc.xc_preset;
				desc.flags = XC::CreateConsoleFlagSetTitle;
				if (desc.icon) desc.flags |= XC::CreateConsoleFlagSetIcon;
				if (desc.preset) desc.flags |= XC::CreateConsoleFlagSetPreset;
				_console = new XC::Console(desc);
				if (lc.xc_backbuffer) _console->SetBackbuffer(lc.xc_backbuffer);
				ec.xc_channel_name = _console->GetSharedConsolePath();
			}
			virtual ~DeviceXC(void) override {}
			virtual void * DynamicCast(const XE::ClassSymbol * cls, XE::ErrorContext & ectx) noexcept override
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				if (cls->GetClassName() == L"objectum") {
					Retain(); return static_cast<Object *>(this);
				} else if (cls->GetClassName() == L"objectum_dynamicum") {
					Retain(); return static_cast<DynamicObject *>(this);
				} else if (cls->GetClassName() == L"consolatorium") {
					Retain(); return static_cast<IConsoleDevice *>(this);
				} else if (cls->GetClassName() == L"scriptio.codificator") {
					try { return WrapToEncoder(this); }
					catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return 0; }
				} else if (cls->GetClassName() == L"scriptio.decodificator") {
					try { return WrapToDecoder(this); }
					catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return 0; }
				} else if (cls->GetClassName() == L"xx.xc") {
					Retain(); return static_cast<IConsoleDevice *>(this);
				} else if (cls->GetClassName() == L"xx.fundus_consolatorii") {
					XE_TRY_INTRO
					int width, height;
					SafePointer<IPC::ISharedMemory> memory;
					_console->AccessBackbuffer(memory.InnerRef(), &width, &height);
					if (!memory || !width || !height) throw InvalidStateException();
					return new ConsoleBackground(_console, memory, width, height);
					XE_TRY_OUTRO(0)
				} else if (cls->GetClassName() == L"xx.contextus_consolatorii" || cls->GetClassName() == L"graphicum.contextus_machinae") {
					XE_TRY_INTRO
					int width, height;
					SafePointer<IPC::ISharedMemory> memory;
					_console->AccessBackbuffer(memory.InnerRef(), &width, &height);
					if (!memory || !width || !height) throw InvalidStateException();
					SafePointer<ConsoleBackground> bkg = new ConsoleBackground(_console, memory, width, height);
					SafePointer<Object> context = bkg->QueryContext(ectx);
					if (context) context->Retain();
					return context.Inner();
					XE_TRY_OUTRO(0)
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual void * GetType(void) noexcept override { return 0; }
			virtual void SetTitle(const string & text, XE::ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				_console->SetWindowTitle(text);
				XE_TRY_OUTRO()
			}
			virtual void SetTextColor(const int & color, XE::ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				_console->SetForegroundColorIndex(color);
				XE_TRY_OUTRO()
			}
			virtual void SetBackgroundColor(const int & color, XE::ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				_console->SetBackgroundColorIndex(color);
				XE_TRY_OUTRO()
			}
			virtual void SetInputMode(const int & mode, XE::ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				if (mode == 0) _console->SetIOMode(IO::ConsoleInputMode::Raw);
				else if (mode == 1) _console->SetIOMode(IO::ConsoleInputMode::Echo);
				else throw InvalidArgumentException();
				XE_TRY_OUTRO()
			}
			virtual void SetScreenBuffer(const bool & alt, XE::ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				if (alt) _console->CreateScreenBuffer(false);
				else _console->CancelScreenBuffer();
				XE_TRY_OUTRO()
			}
			virtual XE::ConsoleSize GetCaretPosition(XE::ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				auto p = _console->GetCaretPosition();
				return XE::ConsoleSize(p.x, p.y);
				XE_TRY_OUTRO(XE::ConsoleSize(0, 0))
			}
			virtual void SetCaretPosition(const XE::ConsoleSize & pos, XE::ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				_console->SetCaretPosition(Point(pos.x, pos.y));
				XE_TRY_OUTRO()
			}
			virtual XE::ConsoleSize GetScreenSize(XE::ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				auto p = _console->GetScreenBufferDimensions();
				return XE::ConsoleSize(p.x, p.y);
				XE_TRY_OUTRO(XE::ConsoleSize(0, 0))
			}
			virtual void Write(const string & line, XE::ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				_console->Print(line);
				XE_TRY_OUTRO()
			}
			virtual void WriteLine(const string & line, XE::ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				_console->Print(line + L"\n\r");
				XE_TRY_OUTRO()
			}
			virtual void LineFeed(XE::ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				_console->Print(L"\n\r");
				XE_TRY_OUTRO()
			}
			virtual void ClearLine(XE::ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				_console->ClearLine();
				XE_TRY_OUTRO()
			}
			virtual void ClearScreen(XE::ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				_console->ClearScreen();
				XE_TRY_OUTRO()
			}
			virtual uint32 ReadChar(XE::ErrorContext & ectx) noexcept override
			{
				XE::ConsoleEvent event;
				while (true) {
					ReadEvent(event, ectx);
					if (ectx.error_code) return 0;
					if (event.event_code == 1) return event.char_input;
					else if (event.event_code == 0) return 0xFFFFFFFF;
				}
			}
			virtual string ReadLine(XE::ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				Array<uint> ucs(0x100);
				while (true) {
					auto chr = ReadChar(ectx);
					if (ectx.error_code) return L"";
					if (chr == 0xFFFFFFFF) throw InvalidStateException();
					else if (chr >= 32) ucs << chr;
					else if (chr == L'\n') break;
				}
				ucs << 0;
				return string(ucs.GetBuffer(), -1, Encoding::UTF32);
				XE_TRY_OUTRO(L"")
			}
			virtual void ReadEvent(XE::ConsoleEvent & event, XE::ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				while (true) {
					IO::ConsoleEventDesc desc;
					_console->ReadEvent(desc);
					if (desc.Event == IO::ConsoleEvent::EndOfFile) {
						event.event_code = 0;
						event.char_input = 0;
						event.key_input = 0;
						event.key_flags = 0;
						event.size.x = 0;
						event.size.y = 0;
						return;
					} else if (desc.Event == IO::ConsoleEvent::CharacterInput) {
						event.event_code = 1;
						event.char_input = desc.CharacterCode;
						event.key_input = 0;
						event.key_flags = 0;
						event.size.x = 0;
						event.size.y = 0;
						return;
					} else if (desc.Event == IO::ConsoleEvent::KeyInput) {
						event.event_code = 2;
						event.char_input = 0;
						event.key_input = desc.KeyCode;
						event.key_flags = desc.KeyFlags;
						event.size.x = 0;
						event.size.y = 0;
						return;
					} else if (desc.Event == IO::ConsoleEvent::ConsoleResized) {
						event.event_code = 3;
						event.char_input = 0;
						event.key_input = 0;
						event.key_flags = 0;
						event.size.x = desc.Width;
						event.size.y = desc.Height;
						return;
					}
				}
				XE_TRY_OUTRO()
			}
			virtual void SetWindowIcon(handle ximage, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Streaming::MemoryStream> stream = new Streaming::MemoryStream(0x10000);
				SafePointer<Codec::Image> image = XE::ExtractImageFromXImage(ximage);
				Codec::EncodeImage(stream, image, Codec::ImageFormatEngine);
				stream->Seek(0, Streaming::Begin);
				SafePointer<DataBlock> data = stream->ReadAll();
				_console->SetWindowIcon(data);
				XE_TRY_OUTRO();
			}
			virtual void SetTextColorRGBA(uint & color, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->SetForegroundColor(color);
				XE_TRY_OUTRO();
			}
			virtual void SetBackgroundColorRGBA(uint & color, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->SetBackgroundColor(color);
				XE_TRY_OUTRO();
			}
			virtual void CreateScreenBuffer(bool scrollable, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->CreateScreenBuffer(scrollable);
				XE_TRY_OUTRO();
			}
			virtual void CancelScreenBuffer(XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->CancelScreenBuffer();
				XE_TRY_OUTRO();
			}
			virtual void SwapScreenBuffers(XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->SwapScreenBuffers();
				XE_TRY_OUTRO();
			}
			virtual void ControlCaretA(uint style, double weight, bool blink, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				XC::CaretStateDesc desc;
				desc.flags = XC::CaretControlFlagShape | XC::CaretControlFlagWeight | XC::CaretControlFlagBlinking;
				desc.style = static_cast<XC::CaretStyle>(style);
				desc.weight = weight;
				if (blink) desc.flags |= XC::CaretControlFlagSetBlinking;
				_console->SetCaretState(desc);
				XE_TRY_OUTRO();
			}
			virtual void ControlCaretB(uint style, double weight, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				XC::CaretStateDesc desc;
				desc.flags = XC::CaretControlFlagShape | XC::CaretControlFlagWeight;
				desc.style = static_cast<XC::CaretStyle>(style);
				desc.weight = weight;
				_console->SetCaretState(desc);
				XE_TRY_OUTRO();
			}
			virtual void ControlCaretC(uint style, bool blink, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				XC::CaretStateDesc desc;
				desc.flags = XC::CaretControlFlagShape | XC::CaretControlFlagBlinking;
				desc.style = static_cast<XC::CaretStyle>(style);
				if (blink) desc.flags |= XC::CaretControlFlagSetBlinking;
				_console->SetCaretState(desc);
				XE_TRY_OUTRO();
			}
			virtual void ControlCaretD(double weight, bool blink, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				XC::CaretStateDesc desc;
				desc.flags = XC::CaretControlFlagWeight | XC::CaretControlFlagBlinking;
				desc.weight = weight;
				if (blink) desc.flags |= XC::CaretControlFlagSetBlinking;
				_console->SetCaretState(desc);
				XE_TRY_OUTRO();
			}
			virtual void ControlCaretE(uint style, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				XC::CaretStateDesc desc;
				desc.flags = XC::CaretControlFlagShape;
				desc.style = static_cast<XC::CaretStyle>(style);
				_console->SetCaretState(desc);
				XE_TRY_OUTRO();
			}
			virtual void ControlCaretF(double weight, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				XC::CaretStateDesc desc;
				desc.flags = XC::CaretControlFlagWeight;
				desc.weight = weight;
				_console->SetCaretState(desc);
				XE_TRY_OUTRO();
			}
			virtual void ControlCaretG(bool blink, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				XC::CaretStateDesc desc;
				desc.flags = XC::CaretControlFlagBlinking;
				if (blink) desc.flags |= XC::CaretControlFlagSetBlinking;
				_console->SetCaretState(desc);
				XE_TRY_OUTRO();
			}
			virtual void ControlCaretR(XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->RevertCaretState();
				XE_TRY_OUTRO();
			}
			virtual void SetFontStyle(uint mask, uint value, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->SetFontAttributes(mask, value);
				XE_TRY_OUTRO()
			}
			virtual void RevertFontStyle(uint mask, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->RevertFontAttributes(mask);
				XE_TRY_OUTRO()
			}
			virtual void WritePalettes(int at_index, const Color & color, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->WritePalette(XC::WritePaletteFlagOverwrite | XC::WritePaletteFlagBothColors, at_index, color);
				XE_TRY_OUTRO()
			}
			virtual void RevertPalettes(int at_index, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->WritePalette(XC::WritePaletteFlagRevert | XC::WritePaletteFlagOverwrite | XC::WritePaletteFlagBothColors, at_index, 0);
				XE_TRY_OUTRO()
			}
			virtual void WritePalette(bool back, int at_index, const Color & color, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				uint pi = back ? XC::WritePaletteFlagBackground : XC::WritePaletteFlagForeground;
				_console->WritePalette(XC::WritePaletteFlagOverwrite | pi, at_index, color);
				XE_TRY_OUTRO()
			}
			virtual void RevertPalette(bool back, int at_index, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				uint pi = back ? XC::WritePaletteFlagBackground : XC::WritePaletteFlagForeground;
				_console->WritePalette(XC::WritePaletteFlagRevert | XC::WritePaletteFlagOverwrite | pi, at_index, 0);
				XE_TRY_OUTRO()
			}
			virtual void WritePaletteDefault(bool back, int to_index, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				uint pi = back ? XC::WritePaletteFlagBackground : XC::WritePaletteFlagForeground;
				_console->WritePalette(XC::WritePaletteFlagSetDefault | pi, to_index, 0);
				XE_TRY_OUTRO()
			}
			virtual void RevertPaletteDefault(bool back, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				uint pi = back ? XC::WritePaletteFlagBackground : XC::WritePaletteFlagForeground;
				_console->WritePalette(XC::WritePaletteFlagRevert | XC::WritePaletteFlagSetDefault | pi, 0, 0);
				XE_TRY_OUTRO()
			}
			virtual void RevertPaletteDefault(XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->OverrideDefaults();
				XE_TRY_OUTRO()
			}
			virtual void SetCloseOnEoS(const bool & value, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->SetCloseDetachedConsole(value);
				XE_TRY_OUTRO()
			}
			virtual void SetHTab(const int & value, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->SetHorizontalTabulation(value);
				XE_TRY_OUTRO()
			}
			virtual void SetVTab(const int & value, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->SetVerticalTabulation(value);
				XE_TRY_OUTRO()
			}
			virtual void SetWindowMargins(const int & value, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->SetWindowMargins(value);
				XE_TRY_OUTRO()
			}
			virtual void SetWindowBackgroundColor(const Color & value, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->SetWindowBackground(value);
				XE_TRY_OUTRO()
			}
			virtual void SetWindowBlurFactor(const double & value, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->SetWindowBlurBehind(value);
				XE_TRY_OUTRO()
			}
			virtual void SetWindowFontA(int height, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->SetWindowFontHeight(height);
				XE_TRY_OUTRO()
			}
			virtual void SetWindowFontB(const string & face, int height, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->SetWindowFont(face, height);
				XE_TRY_OUTRO()
			}
			virtual void PushCaret(XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->PushCaretPosition();
				XE_TRY_OUTRO()
			}
			virtual void PopCaret(XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->PopCaretPosition();
				XE_TRY_OUTRO()
			}
			virtual void SetScrollingRange(int from, int length, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->SetScrollingRange(from, length);
				XE_TRY_OUTRO()
			}
			virtual void ResetScrollingRange(XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->ResetScrollingRange();
				XE_TRY_OUTRO()
			}
			virtual void ScrollBy(int count, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->ScrollContent(count);
				XE_TRY_OUTRO()
			}
			virtual void SetBackbufferMode(uint mode, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (mode == 0) _console->SetBackbufferStretchMode(Windows::ImageRenderMode::Stretch);
				else if (mode == 1) _console->SetBackbufferStretchMode(Windows::ImageRenderMode::Blit);
				else if (mode == 2) _console->SetBackbufferStretchMode(Windows::ImageRenderMode::FitKeepAspectRatio);
				else if (mode == 3) _console->SetBackbufferStretchMode(Windows::ImageRenderMode::CoverKeepAspectRatio);
				else throw InvalidArgumentException();
				XE_TRY_OUTRO()
			}
			virtual void SetBackbufferA(int w, int h, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->CreateBackbuffer(w, h);
				XE_TRY_OUTRO()
			}
			virtual void SetBackbufferB(const string & path, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->LoadBackbuffer(path);
				XE_TRY_OUTRO()
			}
			virtual void SetBackbufferC(handle xframe, XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Streaming::MemoryStream> stream = new Streaming::MemoryStream(0x10000);
				SafePointer<Codec::Frame> frame = XE::ExtractFrameFromXFrame(xframe);
				Codec::EncodeFrame(stream, frame, Codec::ImageFormatEngine);
				stream->Seek(0, Streaming::Begin);
				SafePointer<DataBlock> data = stream->ReadAll();
				_console->SetBackbuffer(data);
				XE_TRY_OUTRO()
			}
			virtual void ResetBackbuffer(XE::ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_console->ResetBackbuffer();
				XE_TRY_OUTRO()
			}
		};
		class ConsoleAllocator : public Object, public XE::IConsoleAllocator
		{
			EnvironmentConfiguration & _ec;
			LaunchConfiguration & _lc;
		public:
			ConsoleAllocator(EnvironmentConfiguration & ec, LaunchConfiguration & lc) : _ec(ec), _lc(lc) {}
			virtual ~ConsoleAllocator(void) override {}
			virtual XE::IConsoleDevice * AllocateConsole(void) noexcept override { try { return new DeviceXC(_ec, _lc); } catch (...) { return 0; } }
			virtual Object * ExposeObject(void) noexcept override { return this; }
		};
		class LauncherServices : public XE::IAPIExtension
		{
			class _xx_process : public Object
			{
				SafePointer<Process> _process;
			public:
				_xx_process(Process * inner) { _process.SetRetain(inner); }
				virtual ~_xx_process(void) override {}
				virtual string ToString(void) const override { try { return L"XX Process"; } catch (...) { return L""; } }
				virtual void Wait(void) noexcept { _process->Wait(); }
				virtual void Terminate(void) noexcept { _process->Terminate(); }
				virtual bool IsActive(void) noexcept { return !_process->Exited(); }
				virtual int GetExitCode(void) noexcept { return _process->GetExitCode(); }
			};
			class _xx_library_interface : public Object
			{
			public:
				virtual void * GetSymbol(const string & name) noexcept = 0;
				virtual uint GetTypeOfModule(void) noexcept = 0;
				virtual handle GetHandleOfModule(void) noexcept = 0;
			};
			class _xx_xe_dynamic_library : public _xx_library_interface
			{
				SafePointer<XE::ExecutionContext> _ctx;
				const XE::Module * _module;
				uint _class;
			public:
				_xx_xe_dynamic_library(XE::ExecutionContext * ctx, uint cls, const XE::Module * module) : _class(cls), _module(module) { _ctx.SetRetain(ctx); }
				virtual ~_xx_xe_dynamic_library(void) override {}
				virtual void * GetSymbol(const string & name) noexcept override
				{
					auto smbl = _ctx->GetSymbol(name);
					if (!smbl) {
						smbl = _ctx->GetSymbol(XE::SymbolType::Function, L"exporta", name);
						if (!smbl) return 0;
					}
					return smbl->GetSymbolEntity();
				}
				virtual uint GetTypeOfModule(void) noexcept override { return _class; }
				virtual handle GetHandleOfModule(void) noexcept override { return const_cast<XE::Module *>(_module); }
			};
			class _xx_xe_resource_library : public _xx_library_interface
			{
				SafePointer<XE::Module> _module;
			public:
				_xx_xe_resource_library(XE::Module * module) { _module.SetRetain(module); }
				virtual ~_xx_xe_resource_library(void) override {}
				virtual void * GetSymbol(const string & name) noexcept override { return 0; }
				virtual uint GetTypeOfModule(void) noexcept override { return 8; }
				virtual handle GetHandleOfModule(void) noexcept override { return _module.Inner(); }
			};
			class _xx_native_dynamic_library : public _xx_library_interface
			{
				handle _library;
			public:
				_xx_native_dynamic_library(handle library) : _library(library) {}
				virtual ~_xx_native_dynamic_library(void) override { ReleaseLibrary(_library); }
				virtual void * GetSymbol(const string & name) noexcept override
				{
					try {
						Array<char> cn(1);
						cn.SetLength(name.GetEncodedLength(Encoding::UTF8) + 1);
						name.Encode(cn.GetBuffer(), Encoding::UTF8, true);
						return GetLibraryRoutine(_library, cn);
					} catch (...) { return 0; }
				}
				virtual uint GetTypeOfModule(void) noexcept override { return 4; }
				virtual handle GetHandleOfModule(void) noexcept override { return 0; }
			};
			struct _create_process_desc {
				string image;
				string log_path;
				const string * argv;
				SafePointer<_xx_process> result;
				uint flags;
				int argc;
			};
		private:
			EnvironmentConfiguration & _ec;
			LaunchConfiguration & _lc;
		private:
			void _check_image(string & image, bool command_mode, bool & is_xe, bool & is_command)
			{
				if (command_mode) {
					if (IO::FileExists(_ec.xx_commands + L"/" + image)) {
						image = IO::ExpandPath(_ec.xx_commands + L"/" + image);
						is_xe = true;
						is_command = false;
						return;
					} else if (IO::FileExists(_ec.xx_commands + L"/" + image + L".xx")) {
						image = IO::ExpandPath(_ec.xx_commands + L"/" + image + L".xx");
						is_xe = true;
						is_command = false;
						return;
					} else if (IO::FileExists(_ec.xx_commands + L"/" + image + L".xex")) {
						image = IO::ExpandPath(_ec.xx_commands + L"/" + image + L".xex");
						is_xe = true;
						is_command = false;
						return;
					} else is_command = false;
				} else is_command = false;
				try {
					char sign[8];
					SafePointer<Streaming::FileStream> file = new Streaming::FileStream(image, Streaming::AccessRead, Streaming::OpenExisting);
					file->Read(&sign, 8);
					if (MemoryCompare(sign, "xximago", 8) == 0) is_xe = true; else is_xe = false;
				} catch (...) { is_xe = false; }
				if (!is_xe && command_mode) is_command = true;
			}
			static bool _create_process(LauncherServices & self, _create_process_desc & desc) noexcept
			{
				try {
					string real_image;
					Array<string> args(desc.argc + 6);
					bool is_xe, is_command;
					real_image = desc.image;
					self._check_image(real_image, (desc.flags & 4) != 0, is_xe, is_command);
					if (is_xe && (desc.flags & 2)) return false;
					if (!is_xe && (desc.flags & 1)) return false;
					if (is_xe) {
						args << real_image;
						real_image = IO::GetExecutablePath();
						if ((desc.flags & 0x10) == 0 && self._ec.xc_channel_name.Length()) { args << L"--xx-xc" << self._ec.xc_channel_name; }
						if (desc.flags & 0x3E0) {
							uint act_mode = 0;
							if (desc.flags & 0x020) act_mode = EnvironmentLoggerNull;
							else if (desc.flags & 0x040) act_mode = EnvironmentLoggerConsole;
							else if (desc.flags & 0x080) act_mode = EnvironmentLoggerFile;
							else if (desc.flags & 0x200) act_mode = self._ec.logger_type;
							if (act_mode == EnvironmentLoggerNull) { args << L"--xx-act" << L"nullus"; }
							else if (act_mode == EnvironmentLoggerConsole) { args << L"--xx-act" << L"consolatorium"; }
							else if (act_mode == EnvironmentLoggerFile) { args << L"--xx-act" << L"lima" << desc.log_path; }
						}
					}
					args.Append(desc.argv, desc.argc);
					SafePointer<Process> proc;
					bool success;
					if (is_command) {
						proc = CreateCommandProcess(real_image, &args);
						success = proc.Inner() != 0;
					} else if (desc.flags & 8) {
						if (is_xe) real_image = self._ec.xxf_path;
						success = CreateProcessElevated(real_image, &args);
					} else {
						proc = CreateProcess(real_image, &args);
						success = proc.Inner() != 0;
					}
					if (proc) desc.result = new _xx_process(proc); else desc.result.SetReference(0);
					return success;
				} catch (...) { return false; }
			}
			static SafePointer<_xx_library_interface> _load_library(LauncherServices & self, const string & path, uint flags) noexcept
			{
				try {
					string image = path;
					bool is_xe, is_command;
					self._check_image(image, false, is_xe, is_command);
					if (is_xe && (flags & 0xB)) {
						SafePointer<Streaming::Stream> stream = new Streaming::FileStream(path, Streaming::AccessRead, Streaming::OpenExisting);
						if (flags & 1) {
							SafePointer<XE::ExecutionContext> subctx = new XE::ExecutionContext(self._lc.primary_context->GetLoaderCallback());
							auto mdl = subctx->LoadModule(path, stream);
							return new _xx_xe_dynamic_library(subctx, 1, mdl);
						} else if (flags & 2) {
							auto mdl = self._lc.primary_context->LoadModule(path, stream);
							return new _xx_xe_dynamic_library(self._lc.primary_context, 2, mdl);
						} else {
							SafePointer<XE::Module> mdl = self._lc.primary_context->LoadModuleResources(stream);
							return new _xx_xe_resource_library(mdl);
						}
					}
					if (!is_xe && (flags & 4)) {
						handle library = XE::LoadLibraryXE(IO::ExpandPath(path));
						if (!library) return 0;
						return new _xx_native_dynamic_library(library);
					}
					return 0;
				} catch (...) { return 0; }
			}
			static void _get_xversion(int * xxv, int * ev) noexcept
			{
				if (xxv) {
					xxv[0] = ENGINE_VI_VERSIONMAJOR;
					xxv[1] = ENGINE_VI_VERSIONMINOR;
					xxv[2] = ENGINE_VI_SUBVERSION;
					xxv[3] = ENGINE_VI_BUILD;
				}
				if (ev) {
					ev[0] = ENGINE_RUNTIME_VERSION_MAJOR;
					ev[1] = ENGINE_RUNTIME_VERSION_MINOR;
				}
			}
		public:
			LauncherServices(EnvironmentConfiguration & ec, LaunchConfiguration & lc) : _ec(ec), _lc(lc) {}
			virtual ~LauncherServices(void) override {}
			virtual const void * ExposeRoutine(const string & routine_name) noexcept override
			{
				if (routine_name == L"xx_crpr") return reinterpret_cast<const void *>(_create_process);
				else if (routine_name == L"xx_onmd") return reinterpret_cast<const void *>(_load_library);
				else if (routine_name == L"xx_pxxv") return reinterpret_cast<const void *>(_get_xversion);
				else return 0;
			}
			virtual const void * ExposeInterface(const string & interface) noexcept override { if (interface == L"xx") return this; else return 0; }
		};

		#ifdef ENGINE_SUBSYSTEM_CONSOLE
		void PlatformErrorReport(const string & message) noexcept
		{
			try {
				IO::Console console;
				console.WriteLine(message);
			} catch (...) {}
		}
		#endif
		#ifdef ENGINE_SUBSYSTEM_GUI
		void PlatformErrorReport(const string & message) noexcept
		{
			try {
				Windows::GetWindowSystem()->SetApplicationIconVisibility(true);
				Windows::GetWindowSystem()->MessageBox(0, message, L"XX", 0, Windows::MessageBoxButtonSet::Ok, Windows::MessageBoxStyle::Error, 0);
			} catch (...) {}
		}
		#endif
		void LoadLaunchConfiguration(const string & module, LaunchConfiguration & conf)
		{
			try {
				SafePointer<Streaming::Stream> stream = new Streaming::FileStream(module, Streaming::AccessRead, Streaming::OpenExisting);
				XI::Module mdl(stream, XI::Module::ModuleLoadFlags::LoadResources);
				conf.subsystem = mdl.subsystem;
				SafePointer< Volumes::Dictionary<string, string> > metadata = XI::LoadModuleMetadata(mdl.resources);
				if (conf.subsystem == XI::Module::ExecutionSubsystem::ConsoleUI) {
					auto xcc = metadata->GetElementByKey(L"XC");
					conf.require_xc = xcc && *xcc == L"sic";
					conf.require_native_console = xcc && *xcc == L"non";
				} else conf.require_xc = conf.require_native_console = false;
				auto title = metadata->GetElementByKey(XI::MetadataKeyModuleName);
				conf.xc_title = title ? *title : module;
				conf.xc_preset.SetRetain(mdl.resources.GetObjectByKey(XI::MakeResourceID(L"XC", 1)));
				conf.xc_backbuffer.SetRetain(mdl.resources.GetObjectByKey(XI::MakeResourceID(L"XC", 2)));
				SafePointer<Codec::Image> icon = XI::LoadModuleIcon(mdl.resources, 1);
				if (icon->Frames.Length()) {
					Streaming::MemoryStream ms(0x10000);
					Codec::EncodeImage(&ms, icon, Codec::ImageFormatEngine);
					ms.Seek(0, Streaming::Begin);
					conf.xc_icon = ms.ReadAll();
				}
				conf.primary_context = 0;
			} catch (...) {
				conf.subsystem = XI::Module::ExecutionSubsystem::NoUI;
				conf.require_xc = conf.require_native_console = false;
			}
		}
		void LoadEnvironmentConfiguration(const EnvironmentDesc & desc, EnvironmentConfiguration & conf)
		{
			auto xx = IO::ExpandPath(IO::Path::GetDirectory(IO::GetExecutablePath()) + L"/" + desc.xx_init_path);
			SafePointer<Storage::Registry> xxr = LoadConfiguration(xx);
			auto root = IO::Path::GetDirectory(xx) + L"/";
			conf.store_file = IO::ExpandPath(root + xxr->GetValueString(L"Entheca"));
			conf.xxf_path = IO::ExpandPath(root + xxr->GetValueString(L"XXF"));
			conf.xc_path = IO::ExpandPath(root + xxr->GetValueString(L"XC"));
			conf.xe_config = IO::ExpandPath(root + xxr->GetValueString(L"XE"));
			conf.xx_hello = IO::ExpandPath(root + xxr->GetValueString(L"XX"));
			conf.xx_commands = IO::ExpandPath(root + xxr->GetValueString(L"Imperata"));
			conf.locale_override = xxr->GetValueString(L"Lingua");
			conf.locale_default = xxr->GetValueString(L"LinguaDefalta");
		}
		void CommandLineInterpret(const Array<string> & args, EnvironmentConfiguration & conf, XE::StandardLoader * loader)
		{
			Array<string> direct_args(0x10);
			for (int i = 1; i < args.Length(); i++) {
				if (args[i] == L"--xx-xc") {
					i++;
					if (i < args.Length()) conf.xc_channel_name = args[i];
				} else if (args[i] == L"--xx-act") {
					i++;
					if (i < args.Length()) {
						if (args[i] == L"nullus") conf.logger_type = EnvironmentLoggerNull;
						else if (args[i] == L"consolatorium") conf.logger_type = EnvironmentLoggerConsole;
						else if (args[i] == L"lima") conf.logger_type = EnvironmentLoggerFile;
						else throw Exception();
						if (conf.logger_type == EnvironmentLoggerFile) {
							i++;
							if (i < args.Length()) conf.logger_file = IO::ExpandPath(args[i]);
						}
					}
				} else if (conf.xi_executable.Length()) {
					direct_args << args[i];
				} else {
					conf.xi_executable = IO::ExpandPath(args[i]);
					direct_args << args[i];
				}
			}
			if (!conf.xi_executable.Length()) {
				conf.xi_executable = conf.xx_hello;
				direct_args << conf.xx_hello;
			}
			XE::ActivateFileIO(*loader, conf.xi_executable, direct_args.GetBuffer(), direct_args.Length());
		}
		void InvalidSubsystemErrorReport(void) noexcept { PlatformErrorReport(L"Subsystema moduli executa non est (in circumiecto currenti)."); }
		void NoEntryPointErrorReport(void) noexcept { PlatformErrorReport(L"Introitus nullus."); }
		int Main(const EnvironmentDesc & desc) noexcept
		{
			try {
				EnvironmentConfiguration environment_configuration;
				LaunchConfiguration launch_configuration;
				Engine::Math::Random::Init();
				Engine::Codec::InitializeDefaultCodecs();
				Engine::Media::InitializeDefaultCodecs();
				Engine::Audio::InitializeDefaultCodecs();
				Engine::Video::InitializeDefaultCodecs();
				Engine::Subtitles::InitializeDefaultCodecs();
				Engine::Assembly::CurrentLocale = Assembly::GetCurrentUserLocale();
				#ifdef ENGINE_SUBSYSTEM_GUI
				{
					SafePointer<Streaming::Stream> coms = Assembly::QueryLocalizedResource(L"COM");
					SafePointer<Storage::StringTable> com = new Storage::StringTable(coms);
					Assembly::SetLocalizedCommonStrings(com);
				}
				#endif
				SafePointer< Array<string> > args = GetCommandLine();
				SafePointer<XE::StandardLoader> loader = XE::CreateStandardLoader(XE::UseStandard);
				LoadEnvironmentConfiguration(desc, environment_configuration);
				try { if (environment_configuration.xe_config.Length()) IncludeComponent(*loader, environment_configuration.xe_config); } catch (...) {}
				try { if (environment_configuration.store_file.Length()) IncludeStoreIntegration(*loader, environment_configuration.store_file); } catch (...) {}
				SafePointer<XLoggerSink> logger;
				SafePointer<XE::ExecutionContext> xctx = new XE::ExecutionContext(loader);
				xctx->AllowEmbeddedModules(true);
				environment_configuration.logger_type = desc.flags_default & EnvironmentLoggerMask;
				CommandLineInterpret(*args, environment_configuration, loader);
				environment_configuration.logger_type &= desc.flags_allow & EnvironmentLoggerMask;
				if (environment_configuration.logger_type == EnvironmentLoggerNull) logger = new NullLogger;
				else if (environment_configuration.logger_type == EnvironmentLoggerConsole) logger = new ConsoleLogger;
				else if (environment_configuration.logger_type == EnvironmentLoggerFile) logger = new FileLogger(environment_configuration.logger_file);
				else logger = new NullLogger;
				XE::SetLoggerSink(*xctx, logger);
				LoadLaunchConfiguration(environment_configuration.xi_executable, launch_configuration);
				SafePointer<LauncherServices> launcher_services = new LauncherServices(environment_configuration, launch_configuration);
				SafePointer<ConsoleAllocator> console_allocator = new ConsoleAllocator(environment_configuration, launch_configuration);
				SafePointer<XE::IConsoleDevice> console;
				bool windows_enabled = false;
				if (launch_configuration.subsystem == XI::Module::ExecutionSubsystem::ConsoleUI) {
					uint con_type = desc.flags_default & EnvironmentConsoleMask;
					if (!con_type) con_type = EnvironmentConsoleXC;
					if (launch_configuration.require_xc) con_type = EnvironmentConsoleXC;
					if (launch_configuration.require_native_console) con_type = EnvironmentConsoleSystem;
					if (con_type & desc.flags_allow == 0) {
						InvalidSubsystemErrorReport();
						return 7;
					}
					if (con_type == EnvironmentConsoleSystem) {
						SafePointer<IO::Console> sc = new IO::Console;
						console = XE::CreateSystemConsoleDevice(sc);
					} else if (con_type == EnvironmentConsoleXC) {
						if (environment_configuration.xc_channel_name.Length()) console = new DeviceXC(environment_configuration.xc_channel_name);
						else console = console_allocator->AllocateConsole();
					}
				} else if (launch_configuration.subsystem == XI::Module::ExecutionSubsystem::GUI) {
					if (desc.flags_allow & EnvironmentWindowsDelegate) {
						args->RemoveFirst();
						SafePointer<Process> xxf = CreateProcess(environment_configuration.xxf_path, args);
						if (!xxf) {
							InvalidSubsystemErrorReport();
							return 7;
						}
						xxf->Wait();
						return xxf->GetExitCode();
					} else if (desc.flags_allow & EnvironmentWindowsAllow == 0) {
						InvalidSubsystemErrorReport();
						return 7;
					}
					windows_enabled = true;
				} else if (launch_configuration.subsystem == XI::Module::ExecutionSubsystem::Library) {
					InvalidSubsystemErrorReport();
					return 7;
				}
				XE::ActivateConsoleIO(*loader, console, console_allocator);
				XE::ActivateImageIO(*loader);
				XE::ActivateFileFormatIO(*loader);
				XE::ActivatePowerControl(*loader);
				XE::ActivateReflectionAPI(*loader);
				XE::ActivateWindowsIO(*loader);
				XE::ActivateTypographyIO(*loader);
				XE::ActivateNetworkAPI(*loader);
				XE::ActivateCryptography(*loader);
				XE::ActivateSharedMemory(*loader);
				XE::ActivateMultimedia(*loader);
				loader->RegisterAPIExtension(launcher_services);
				launch_configuration.primary_context = xctx;
				if (environment_configuration.locale_override.Length()) Assembly::CurrentLocale = environment_configuration.locale_override;
				try { XE::LoadErrorLocalization(*xctx, L"errores." + Assembly::CurrentLocale); }
				catch (...) { XE::LoadErrorLocalization(*xctx, L"errores." + environment_configuration.locale_default); }
				loader->AddModuleSearchPath(IO::Path::GetDirectory(environment_configuration.xi_executable));
				loader->AddDynamicLibrarySearchPath(IO::Path::GetDirectory(environment_configuration.xi_executable));
				SafePointer<Streaming::Stream> module_stream;
				try {
					module_stream = new Streaming::FileStream(environment_configuration.xi_executable, Streaming::AccessRead, Streaming::OpenExisting);
				} catch (IO::FileAccessException & e) {
					XE::ErrorContext ectx;
					ectx.error_code = 6;
					ectx.error_subcode = e.code;
					string er, ser;
					XE::GetErrorDescription(ectx, *xctx, er, ser);
					PlatformErrorReport(FormatString(L"Error onerandi: %0.\n%1: %2.\n%3", environment_configuration.xi_executable,
						er, ser, loader->GetLastErrorSubject()));
					return 7;
				}
				auto main_module = xctx->LoadModule(environment_configuration.xi_executable, module_stream);
				module_stream.SetReference(0);
				if (!loader->IsAlive()) {
					XE::ErrorContext ectx;
					ectx.error_code = 7;
					ectx.error_subcode = int(loader->GetLastError());
					string er, ser;
					XE::GetErrorDescription(ectx, *xctx, er, ser);
					PlatformErrorReport(FormatString(L"Error onerandi: %0.\n%1: %2.\n%3", loader->GetLastErrorModule(),
						er, ser, loader->GetLastErrorSubject()));
					return 7;
				}
				if (windows_enabled) XE::ControlWindowsIO(*loader, main_module);
				auto main = xctx->GetEntryPoint();
				if (!main) {
					NoEntryPointErrorReport();
					return 7;
				}
				auto main_routine = reinterpret_cast<XE::StandardRoutine>(main->GetSymbolEntity());
				XE::ErrorContext ectx;
				ectx.error_code = ectx.error_subcode = 0;
				main_routine(&ectx);
				return ectx.error_code;
			} catch (...) {
				PlatformErrorReport(L"Error internus.");
				return 7;
			}
		}
	}
}