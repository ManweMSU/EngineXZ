#pragma once

#include <EngineRuntime.h>

namespace Engine
{
	namespace XC
	{
		enum class CaretStyle : uint { Horizontal = 1, Vertical = 2, Cell = 3, Null = 0 };
		enum CellFlags : uint {
			CellFlagBold		= 0x01,
			CellFlagItalic		= 0x02,
			CellFlagUnderline	= 0x04
		};
		enum WritePaletteFlags : uint {
			WritePaletteFlagForeground	= 0x01,
			WritePaletteFlagBackground	= 0x02,
			WritePaletteFlagBothColors	= WritePaletteFlagForeground | WritePaletteFlagBackground,
			WritePaletteFlagSetDefault	= 0x04,
			WritePaletteFlagOverwrite	= 0x08,
			WritePaletteFlagRevert		= 0x10
		};
		enum CommonEventFlags : uint {
			CommonEventCaretMove		= 0x00000001,
			CommonEventCaretReshape		= 0x00000002,
			CommonEventPictureReset		= 0x00000004,
			CommonEventPictureUpdate	= 0x00000008,
			CommonEventPictureReshape	= 0x00000010,
			CommonEventBufferReset		= 0x00000020,
			CommonEventBufferScroll		= 0x00000040,
			CommonEventWindowSetTitle	= 0x00000080,
			CommonEventWindowSetColor	= 0x00000100,
			CommonEventWindowSetIcon	= 0x00000200,
			CommonEventWindowSetBlur	= 0x00000400,
			CommonEventWindowSetFont	= 0x00000800,
			CommonEventWindowSetMargins	= 0x00001000,
			CommonEventEndOfStream		= 0x00002000,
			CommonEventTerminate		= 0x00004000,
		};
		struct CellAttribute
		{
			uint flags;
			Color foreground, background;
		};
		struct Cell
		{
			uint32 ucs;
			CellAttribute attr;
		};
		struct ConsoleCreateMode
		{
			Point size;
			bool fullscreen;	
		};
		struct ConsoleRevertibleState
		{
			int _default_foreground, _default_background;
			Color _palette_foreground[16], _palette_background[16];
			CellAttribute _attribute;
			bool _caret_enable_blinking;
			CaretStyle _caret_style;
			double _caret_size;
		};

		class IPresentationCallback
		{
		public:
			virtual void CommonEvent(uint flags) noexcept = 0;
			virtual void ScrollToLine(int line) noexcept = 0;
			virtual void InvalidateLines(int line_from, int count) noexcept = 0;
			virtual void RemoveLines(int line_from, int count) noexcept = 0;
			virtual void InsertLines(int line_from, int count) noexcept = 0;
			virtual void MoveLines(int line_from, int count, int dy) noexcept = 0;
		};
		class IOutputCallback
		{
		public:
			virtual bool OutputKey(uint code, uint flags) noexcept = 0;
			virtual void OutputText(uint ucs) noexcept = 0;
			virtual void WindowResize(int width, int height) noexcept = 0;
			virtual void ScreenBufferResize(int width, int height) noexcept = 0;
			virtual void Terminate(void) noexcept = 0;
		};

		class ScreenBuffer;
		class ConsoleState;

		class IPictureProvider : public Object
		{
		public:
			virtual int GetWidth(void) const noexcept = 0;
			virtual int GetHeight(void) const noexcept = 0;
			virtual int GetStride(void) const noexcept = 0;
			virtual const void * GetData(void) const noexcept = 0;
			virtual void * GetData(void) noexcept = 0;
			virtual bool IsShared(void) const noexcept = 0;
		};
		class ScreenBuffer : public Object
		{
			friend class ConsoleState;
		private:
			bool _scrollable;
			Point _size, _caret;
			Array<Cell> _cells;
			Array<Point> _caret_stack;
			Windows::ImageRenderMode _picture_mode;
			SafePointer<IPictureProvider> _picture;
			int _scroll_region_from, _scroll_region_num_lines, _scroll_offset;
		public:
			ScreenBuffer(const CellAttribute & attr);
			virtual ~ScreenBuffer(void) override;
		};
		class ConsoleState : public Object
		{
			string _title, _font_face;
			Color _window_background;
			SafePointer<Codec::Image> _window_icon;
			SafePointer<ScreenBuffer> _screen;
			ObjectArray<ScreenBuffer> _screen_stack;
			int _margins, _font_height, _max_height;
			Point _tab_size, _window_size;
			ConsoleRevertibleState _state_current, _state_default;
			bool _close_on_eos;
			double _window_blur_factor;
			IOutputCallback * _output_callback;
			IPresentationCallback * _presentation_callback;
		public:
			ConsoleState(Storage::Registry * initstate);
			virtual ~ConsoleState(void) override;

			void SetEndOfStream(void);
			void SetTitle(const string & title);
			const string & GetTitle(void) const;
			void SetFont(const string & face, int height);
			void GetFont(string & face, int & height) const;
			void SetMargin(int margin);
			int GetMargin(void) const;
			void SetBlurBehind(double factor);
			double GetBlurBehind(void) const;
			void SetWindowColor(Color color);
			Color GetWindowColor(void) const;
			void SetWindowIcon(Codec::Image * icon);
			Codec::Image * GetWindowIcon(void) const;
			int GetBufferWidth(void) const;
			int GetBufferHeight(void) const;
			int GetBufferLimitHeight(void) const;
			void SetBufferLimitHeight(int limit);
			Point GetWindowSize(void) const;
			Point GetTabulationSize(void) const;
			void SetTabulationSize(const Point & size);
			bool GetCloseOnEndOfStream(void) const;
			void SetCloseOnEndOfStream(bool ceos);
			void ResizeBufferEnforced(int width, int height);
			void ResizeBuffer(int width, int height);
			bool IsBufferScrollable(void) const;
			int GetBufferOffset(void) const;
			void SetBufferOffset(int offset) const;
			void ScrollCurrentRange(int by);
			void LineFeed(void);
			void CarriageReturn(void);
			void Print(const uint32 * codes, int length);
			void RevertCaretVisuals(void);
			void SetCaretStyle(CaretStyle style);
			CaretStyle GetCaretStyle(void) const;
			void SetCaretBlinks(bool blinks);
			bool GetCaretBlinks(void) const;
			void SetCaretWeight(double weight);
			double GetCaretWeight(void) const;
			void GetCaretPosition(Point & caret) const;
			void SetCaretPosition(const Point & caret);
			bool ReadCell(int x, int y, Cell & cell) const;
			void ReadLine(int y, const Cell ** first, int * length) const;
			CellAttribute GetCurrentAttribution(void) const;
			void SetAttributionFlags(uint mask_alternate, uint values);
			void RevertAttributionFlags(uint mask_alternate);
			void SetForegroundColor(Color color);
			void SetForegroundColorIndex(int index);
			void SetBackgroundColor(Color color);
			void SetBackgroundColorIndex(int index);
			void Erase(bool screen, int part);
			void ClearScreen(void);
			void CreateScreenBuffer(const Point & size, bool scrollable);
			void RemoveScreenBuffer(void);
			void SwapScreenBuffers(void);
			void PushCaretPosition(void);
			void PopCaretPosition(void);
			void SetScrollingRectangle(int line_first, int line_count);
			void ResetScrollingRectangle(void);
			void WritePalette(uint flags, int index, Color color);
			void OverrideDefaults(void);
			void UpdateLine(int y, const Cell * cells, int length);
			void UpdateLine(int y, const uint * chars, int length, bool keep_attributes);

			void SetPicture(IPictureProvider * picture);
			void NotifyPictureUpdated(void) const;
			IPictureProvider * GetPicture(void) const;
			void SetPictureMode(Windows::ImageRenderMode mode);
			Windows::ImageRenderMode GetPictureMode(void) const;

			bool HandleKey(uint code, uint flags);
			void HandleChar(uint ucs);
			void HandleWindowResize(int width, int height);
			void SetCallback(IPresentationCallback * callback);
			void SetCallback(IOutputCallback * callback);
			void Terminate(void);
		};
		class WrappedPicture : public IPictureProvider
		{
			SafePointer<Codec::Frame> _frame;
		public:
			WrappedPicture(int width, int height);
			WrappedPicture(Codec::Frame * frame);
			virtual ~WrappedPicture(void) override;
			virtual int GetWidth(void) const noexcept override;
			virtual int GetHeight(void) const noexcept override;
			virtual int GetStride(void) const noexcept override;
			virtual const void * GetData(void) const noexcept override;
			virtual void * GetData(void) noexcept override;
			virtual bool IsShared(void) const noexcept override;
			Codec::Frame * GetFrame(void) const noexcept;
		};
		class SharedMemoryPicture : public IPictureProvider
		{
			int _width, _height;
			int _index;
			SafePointer<IPC::ISharedMemory> _memory;
			void * _mapping;
		public:
			SharedMemoryPicture(int width, int height);
			SharedMemoryPicture(Codec::Frame * frame);
			virtual ~SharedMemoryPicture(void) override;
			virtual int GetWidth(void) const noexcept override;
			virtual int GetHeight(void) const noexcept override;
			virtual int GetStride(void) const noexcept override;
			virtual const void * GetData(void) const noexcept override;
			virtual void * GetData(void) noexcept override;
			virtual bool IsShared(void) const noexcept override;
			int ExposeMemoryIndex(void) const noexcept;
		};

		void LoadConsolePreset(const string & title, const string & preset_path, ConsoleState ** state, ConsoleCreateMode * mode);
		void LoadConsolePreset(const string & title, DataBlock * preset, ConsoleState ** state, ConsoleCreateMode * mode);
	}
}