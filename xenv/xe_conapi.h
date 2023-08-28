#pragma once

#include "xe_loader.h"
#include "xe_interfaces.h"

namespace Engine
{
	namespace XE
	{
		struct ConsoleSize
		{
			int x, y;
			ConsoleSize(void) noexcept;
			ConsoleSize(int _x, int _y) noexcept;
			~ConsoleSize(void);
		};
		struct ConsoleEvent
		{
			int event_code;
			uint32 char_input;
			int key_input;
			int key_flags;
			ConsoleSize size;
		};
		class DynamicObject : public Object
		{
		public:
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept = 0;
			virtual void * GetType(void) noexcept = 0;
		};
		class IConsoleDevice : public DynamicObject
		{
		public:
			virtual void SetTitle(const string & text, ErrorContext & ectx) noexcept = 0;
			virtual void SetTextColor(const int & color, ErrorContext & ectx) noexcept = 0;
			virtual void SetBackgroundColor(const int & color, ErrorContext & ectx) noexcept = 0;
			virtual void SetInputMode(const int & mode, ErrorContext & ectx) noexcept = 0;
			virtual void SetScreenBuffer(const bool & alt, ErrorContext & ectx) noexcept = 0;
			virtual ConsoleSize GetCaretPosition(ErrorContext & ectx) noexcept = 0;
			virtual void SetCaretPosition(const ConsoleSize & pos, ErrorContext & ectx) noexcept = 0;
			virtual ConsoleSize GetScreenSize(ErrorContext & ectx) noexcept = 0;
			virtual void Write(const string & line, ErrorContext & ectx) noexcept = 0;
			virtual void WriteLine(const string & line, ErrorContext & ectx) noexcept = 0;
			virtual void LineFeed(ErrorContext & ectx) noexcept = 0;
			virtual void ClearLine(ErrorContext & ectx) noexcept = 0;
			virtual void ClearScreen(ErrorContext & ectx) noexcept = 0;
			virtual uint32 ReadChar(ErrorContext & ectx) noexcept = 0;
			virtual string ReadLine(ErrorContext & ectx) noexcept = 0;
			virtual void ReadEvent(ConsoleEvent & event, ErrorContext & ectx) noexcept = 0;
		};
		class IConsoleExtension
		{
		public:
			virtual IConsoleDevice * GetCurrentDevice(ErrorContext & ectx) noexcept = 0;
			virtual IConsoleDevice * AllocateDevice(ErrorContext & ectx) noexcept = 0;
			virtual bool HasDevice(void) noexcept = 0;
		};
		class IConsoleAllocator
		{
		public:
			virtual IConsoleDevice * AllocateConsole(void) noexcept = 0;
			virtual Object * ExposeObject(void) noexcept = 0;
		};

		void ActivateConsoleIO(StandardLoader & ldr, IConsoleDevice * device, IConsoleAllocator * allocator);
		IConsoleDevice * CreateSystemConsoleDevice(IO::Console * console);
		XTextEncoder * WrapToEncoder(IConsoleDevice * device);
		XTextDecoder * WrapToDecoder(IConsoleDevice * device);
	}
}