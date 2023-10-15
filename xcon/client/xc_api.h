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
		struct ConsoleDesc
		{
			uint flags;
			string xc_path;
			string title;
			SafePointer<DataBlock> preset, icon;
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
		public:
			Console(const ConsoleDesc & desc);
			virtual ~Console(void) override;

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
		};
	}
}