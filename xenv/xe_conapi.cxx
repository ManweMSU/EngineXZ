#include "xe_conapi.h"

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
	namespace XE
	{
		ConsoleSize::ConsoleSize(void) noexcept {}
		ConsoleSize::ConsoleSize(int _x, int _y) noexcept : x(_x), y(_y) {}
		ConsoleSize::~ConsoleSize(void) {}

		class ConsoleExtension : public IAPIExtension, public IConsoleExtension
		{
			IConsoleAllocator * _allocator;
			SafePointer<IConsoleDevice> _device;
			SafePointer<Object> _allocator_object;
		public:
			ConsoleExtension(IConsoleDevice * device, IConsoleAllocator * allocator) : _allocator(allocator)
			{
				_device.SetRetain(device);
				if (_allocator) _allocator_object.SetRetain(_allocator->ExposeObject());
			}
			virtual ~ConsoleExtension(void) override {}
			virtual const void * ExposeRoutine(const string & routine_name) noexcept override { return 0; }
			virtual const void * ExposeInterface(const string & interface) noexcept override
			{
				if (interface == L"consolatorium") return static_cast<IConsoleExtension *>(this);
				else return 0;
			}
			virtual IConsoleDevice * GetCurrentDevice(ErrorContext & ectx) noexcept override
			{
				if (_device) return _device;
				ectx.error_code = 5; ectx.error_subcode = 0; return 0;
			}
			virtual IConsoleDevice * AllocateDevice(ErrorContext & ectx) noexcept override
			{
				if (_device) { ectx.error_code = 5; ectx.error_subcode = 0; return 0; }
				if (_allocator) {
					_device = _allocator->AllocateConsole();
					if (!_device) { ectx.error_code = 6; ectx.error_subcode = IO::Error::CreateFailure; return 0; }
					return _device;
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual bool HasDevice(void) noexcept override { return _device; }
		};
		class SystemConsoleDevice : public IConsoleDevice
		{
			SafePointer<IO::Console> _console;
		public:
			SystemConsoleDevice(IO::Console * console)
			{
				if (!console->IsConsoleDevice()) throw InvalidArgumentException();
				_console.SetRetain(console);
			}
			virtual ~SystemConsoleDevice(void) override {}
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept override
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
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual void * GetType(void) noexcept override { return 0; }
			virtual void SetTitle(const string & text, ErrorContext & ectx) noexcept override { XE_TRY_INTRO _console->SetTitle(text); XE_TRY_OUTRO() }
			virtual void SetTextColor(const int & color, ErrorContext & ectx) noexcept override { XE_TRY_INTRO _console->SetTextColor(color); XE_TRY_OUTRO() }
			virtual void SetBackgroundColor(const int & color, ErrorContext & ectx) noexcept override { XE_TRY_INTRO _console->SetBackgroundColor(color); XE_TRY_OUTRO() }
			virtual void SetInputMode(const int & mode, ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				if (mode == 0) _console->SetInputMode(IO::ConsoleInputMode::Raw);
				else if (mode == 1) _console->SetInputMode(IO::ConsoleInputMode::Echo);
				else throw InvalidArgumentException();
				XE_TRY_OUTRO()
			}
			virtual void SetScreenBuffer(const bool & alt, ErrorContext & ectx) noexcept override { XE_TRY_INTRO _console->AlternateScreenBuffer(alt); XE_TRY_OUTRO() }
			virtual ConsoleSize GetCaretPosition(ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				int x, y;
				_console->GetCaretPosition(x, y);
				return ConsoleSize(x, y);
				XE_TRY_OUTRO(ConsoleSize())
			}
			virtual void SetCaretPosition(const ConsoleSize & pos, ErrorContext & ectx) noexcept override { XE_TRY_INTRO _console->MoveCaret(pos.x, pos.y); XE_TRY_OUTRO() }
			virtual ConsoleSize GetScreenSize(ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				int x, y;
				_console->GetScreenBufferDimensions(x, y);
				return ConsoleSize(x, y);
				XE_TRY_OUTRO(ConsoleSize())
			}
			virtual void Write(const string & line, ErrorContext & ectx) noexcept override { XE_TRY_INTRO _console->Write(line); XE_TRY_OUTRO() }
			virtual void WriteLine(const string & line, ErrorContext & ectx) noexcept override { XE_TRY_INTRO _console->WriteLine(line); XE_TRY_OUTRO() }
			virtual void LineFeed(ErrorContext & ectx) noexcept override { XE_TRY_INTRO _console->LineFeed(); XE_TRY_OUTRO() }
			virtual void ClearLine(ErrorContext & ectx) noexcept override { XE_TRY_INTRO _console->ClearLine(); XE_TRY_OUTRO() }
			virtual void ClearScreen(ErrorContext & ectx) noexcept override { XE_TRY_INTRO _console->ClearScreen(); XE_TRY_OUTRO() }
			virtual uint32 ReadChar(ErrorContext & ectx) noexcept override { XE_TRY_INTRO return _console->ReadChar(); XE_TRY_OUTRO(0) }
			virtual string ReadLine(ErrorContext & ectx) noexcept override { XE_TRY_INTRO return _console->ReadLine(); XE_TRY_OUTRO(L"") }
			virtual void ReadEvent(ConsoleEvent & event, ErrorContext & ectx) noexcept override
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
		};
		class ConsoleWriter : public XTextEncoder
		{
			SafePointer<IConsoleDevice> _device;
		public:
			ConsoleWriter(IConsoleDevice * device) { _device.SetRetain(device); }
			virtual ~ConsoleWriter(void) override {}
			virtual void Write(const string & str, ErrorContext & ectx) noexcept override { _device->Write(str, ectx); }
			virtual void WriteLine(const string & str, ErrorContext & ectx) noexcept override { _device->WriteLine(str, ectx); }
			virtual void LineFeed(ErrorContext & ectx) noexcept override { _device->LineFeed(ectx); }
			virtual void WriteSignature(ErrorContext & ectx) noexcept override {}
		};
		class ConsoleReader : public XTextDecoder
		{
			SafePointer<IConsoleDevice> _device;
		public:
			ConsoleReader(IConsoleDevice * device) { _device.SetRetain(device); }
			virtual ~ConsoleReader(void) override {}
			virtual uint32 ReadChar(ErrorContext & ectx) noexcept override { return _device->ReadChar(ectx); }
			virtual string ReadLine(ErrorContext & ectx) noexcept override { return _device->ReadLine(ectx); }
			virtual string ReadAll(ErrorContext & ectx) noexcept override { ectx.error_code = 1; ectx.error_subcode = 0; return L""; }
			virtual bool IsAtEOS(void) noexcept override { return false; }
			virtual int GetEncoding(void) noexcept override { return 0; }
		};

		void ActivateConsoleIO(StandardLoader & ldr, IConsoleDevice * device, IConsoleAllocator * allocator)
		{
			SafePointer<IAPIExtension> ext = new ConsoleExtension(device, allocator);
			if (!ldr.RegisterAPIExtension(ext)) throw Exception();
		}
		IConsoleDevice * CreateSystemConsoleDevice(IO::Console * console) { return new SystemConsoleDevice(console); }
		XTextEncoder * WrapToEncoder(IConsoleDevice * device) { return new ConsoleWriter(device); }
		XTextDecoder * WrapToDecoder(IConsoleDevice * device) { return new ConsoleReader(device); }
	}
}