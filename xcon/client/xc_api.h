#pragma once

#include "../common/xc_channel.h"

namespace Engine
{
	namespace XC
	{
		enum CreateConsoleFlags {
			CreateConsoleFlagSetTitle	= 0x01,
			CreateConsoleFlagSetIcon	= 0x02,
			CreateConsoleFlagSetPreset	= 0x04
		};
		enum CaretControlFlags {
			CaretControlFlagBlinking	= 0x01,
			CaretControlFlagShape		= 0x02,
			CaretControlFlagWeight		= 0x04,
			CaretControlFlagSetBlinking	= 0x08
		};
		enum FontAttributes : uint {
			FontAttributeBold		= 0x01,
			FontAttributeItalic		= 0x02,
			FontAttributeUnderline	= 0x04
		};
		enum WritePaletteFlags : uint {
			WritePaletteFlagForeground	= 0x01,
			WritePaletteFlagBackground	= 0x02,
			WritePaletteFlagBothColors	= WritePaletteFlagForeground | WritePaletteFlagBackground,
			WritePaletteFlagSetDefault	= 0x04,
			WritePaletteFlagOverwrite	= 0x08,
			WritePaletteFlagRevert		= 0x10
		};
		enum class CaretStyle : uint { Horizontal = 1, Vertical = 2, Cell = 3, Null = 0 };
		struct ConsoleDesc
		{
			uint flags;
			string xc_path;
			string title;
			SafePointer<DataBlock> preset, icon;
		};
		struct CaretStateDesc
		{
			uint flags;
			CaretStyle style;
			double weight;
		};

		class Console : public Object
		{
			struct _system_input_struct
			{
				int designation;
				int data[3];
			};
			static int _input_thread(void * arg);
		private:
			SafePointer<Semaphore> _sync, _counter, _sys_counter;
			SafePointer<IChannel> _channel;
			SafePointer<Thread> _thread;
			Volumes::Queue<IO::ConsoleEventDesc> _inputs;
			Volumes::Queue<_system_input_struct> _sys_inputs;
			string _server_ipc;
		public:
			Console(const string & channel_path);
			Console(const ConsoleDesc & desc);
			virtual ~Console(void) override;

			string GetSharedConsolePath(void);
			void SetWindowTitle(const string & text);
			void SetWindowIcon(DataBlock * icon);
			void Print(const string & text);

			void SetForegroundColor(Color color);
			void SetForegroundColorIndex(int index);
			void SetBackgroundColor(Color color);
			void SetBackgroundColorIndex(int index);

			void ClearScreen(void);
			void ClearLine(void);

			void SetCaretPosition(const Point & pos);
			Point GetCaretPosition(void);
			Point GetScreenBufferDimensions(void);

			void CreateScreenBuffer(bool scrollable);
			void CancelScreenBuffer(void);
			void SwapScreenBuffers(void);

			void SetIOMode(IO::ConsoleInputMode mode);
			void ReadEvent(IO::ConsoleEventDesc & event);

			void SetCaretState(const CaretStateDesc & desc);
			void RevertCaretState(void);
			void SetFontAttributes(uint mask, uint set);
			void RevertFontAttributes(uint mask);
			void WritePalette(uint flags, int index, Color color);
			void OverrideDefaults(void);
			
			void SetCloseDetachedConsole(bool close);
			void SetHorizontalTabulation(int size);
			void SetVerticalTabulation(int size);
			void SetWindowMargins(int size);
			void SetWindowBackground(Color color);
			void SetWindowBlurBehind(double power);
			void SetWindowFontHeight(int height);
			void SetWindowFont(const string & font_face, int height);

			void PushCaretPosition(void);
			void PopCaretPosition(void);
			void SetScrollingRange(int from_line, int num_lines);
			void ResetScrollingRange(void);
			void ScrollContent(int lines);
			void SetBackbufferStretchMode(Windows::ImageRenderMode mode);

			void CreateBackbuffer(int width, int height);
			void LoadBackbuffer(const string & path);
			void SetBackbuffer(DataBlock * image);
			void ResetBackbuffer(void);
			void AccessBackbuffer(IPC::ISharedMemory ** memory, int * width, int * height);
			void SynchronizeBackbuffer(void);
		};
	}
}