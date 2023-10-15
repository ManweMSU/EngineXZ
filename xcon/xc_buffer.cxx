#include "xc_buffer.h"

namespace Engine
{
	namespace XC
	{
		ScreenBuffer::ScreenBuffer(const CellAttribute & attr) : _cells(1), _caret_stack(0x10)
		{
			_scrollable = true;
			_size = Point(1, 1);
			_caret = Point(0, 0);
			_cells << Cell();
			_cells[0].ucs = L' ';
			_cells[0].attr = attr;
			_picture_mode = Windows::ImageRenderMode::Stretch;
			_scroll_region_from = _scroll_region_num_lines = -1;
			_scroll_offset = 0;
		}
		ScreenBuffer::~ScreenBuffer(void) {}

		ConsoleState::ConsoleState(Storage::Registry * initstate) : _screen_stack(1)
		{
			_window_size = Point(1, 1);
			_output_callback = 0;
			_presentation_callback = 0;
			_font_face = initstate->GetValueString(L"Fenestra/Typographica");
			_font_height = initstate->GetValueInteger(L"Fenestra/AltitudoTypographicae");
			_window_background = initstate->GetValueColor(L"Fenestra/Color");
			_margins = initstate->GetValueInteger(L"Fenestra/Margines");
			_close_on_eos = initstate->GetValueBoolean(L"Fenestra/ClodeAputFine");
			_window_blur_factor = initstate->GetValueLongFloat(L"Fenestra/Macula");
			_max_height = initstate->GetValueInteger(L"Fenestra/AltitudoMaxima");
			if (_max_height > 1000) _max_height = 1000;
			else if (_max_height < 1) _max_height = 1;
			for (int i = 0; i < 16; i++) {
				auto word = string(uint(i), HexadecimalBase);
				_state_default._palette_foreground[i] = initstate->GetValueColor(L"Crabattus/P_" + word);
				_state_default._palette_background[i] = initstate->GetValueColor(L"Crabattus/S_" + word);
			}
			_state_default._default_foreground = initstate->GetValueInteger(L"Crabattus/PrimusDefaltus");
			_state_default._default_background = initstate->GetValueInteger(L"Crabattus/SecundusDefaltus");
			if (_state_default._default_foreground < 0 || _state_default._default_foreground >= 0x10) throw InvalidArgumentException();
			if (_state_default._default_background < 0 || _state_default._default_background >= 0x10) throw InvalidArgumentException();
			_state_default._attribute.flags = 0;
			_state_default._attribute.foreground = _state_default._palette_foreground[_state_default._default_foreground];
			_state_default._attribute.background = _state_default._palette_background[_state_default._default_background];
			if (initstate->GetValueBoolean(L"Typographica/Lata")) _state_default._attribute.flags |= CellFlagBold;
			if (initstate->GetValueBoolean(L"Typographica/Cursiva")) _state_default._attribute.flags |= CellFlagItalic;
			if (initstate->GetValueBoolean(L"Typographica/Sublinea")) _state_default._attribute.flags |= CellFlagUnderline;
			_tab_size.x = initstate->GetValueInteger(L"Typographica/TabulatioX");
			_tab_size.y = initstate->GetValueInteger(L"Typographica/TabulatioY");
			if (_tab_size.x <= 0 || _tab_size.y <= 0) throw InvalidArgumentException();
			int cs = initstate->GetValueInteger(L"Cursor/Stylus");
			if (cs == 0) _state_default._caret_style = CaretStyle::Null;
			else if (cs == 1) _state_default._caret_style = CaretStyle::Horizontal;
			else if (cs == 2) _state_default._caret_style = CaretStyle::Vertical;
			else if (cs == 3) _state_default._caret_style = CaretStyle::Cell;
			else throw InvalidArgumentException();
			_state_default._caret_size = initstate->GetValueLongFloat(L"Cursor/Magnitudo");
			if (_state_default._caret_size < 0.0 || _state_default._caret_size > 1.0) throw InvalidArgumentException();
			_state_default._caret_enable_blinking = initstate->GetValueBoolean(L"Cursor/Nictans");
			_state_current = _state_default;
			_screen = new ScreenBuffer(_state_current._attribute);
			int pm = initstate->GetValueInteger(L"Pictura/Modus");
			if (pm == 0) _screen->_picture_mode = Windows::ImageRenderMode::Stretch;
			else if (pm == 1) _screen->_picture_mode = Windows::ImageRenderMode::Blit;
			else if (pm == 2) _screen->_picture_mode = Windows::ImageRenderMode::FitKeepAspectRatio;
			else if (pm == 3) _screen->_picture_mode = Windows::ImageRenderMode::CoverKeepAspectRatio;
			else throw InvalidArgumentException();
		}
		ConsoleState::~ConsoleState(void) {}

		void ConsoleState::SetEndOfStream(void)
		{
			if (_presentation_callback) {
				if (_close_on_eos) _presentation_callback->CommonEvent(CommonEventEndOfStream | CommonEventTerminate);
				else _presentation_callback->CommonEvent(CommonEventEndOfStream);
			}
		}
		void ConsoleState::SetTitle(const string & title)
		{
			_title = title;
			if (_presentation_callback) _presentation_callback->CommonEvent(CommonEventWindowSetTitle);
		}
		const string & ConsoleState::GetTitle(void) const { return _title; }
		void ConsoleState::SetFont(const string & face, int height)
		{
			if (_font_face != face || _font_height != height) {
				_font_face = face;
				_font_height = height;
				if (_presentation_callback) _presentation_callback->CommonEvent(CommonEventWindowSetFont);
			}
		}
		void ConsoleState::GetFont(string & face, int & height) const { face = _font_face; height = _font_height; }
		void ConsoleState::SetMargin(int margin) { if (margin != _margins) { _margins = margin; if (_presentation_callback) _presentation_callback->CommonEvent(CommonEventWindowSetMargins); } }
		int ConsoleState::GetMargin(void) const { return _margins; }
		void ConsoleState::SetBlurBehind(double factor) { _window_blur_factor = factor; if (_presentation_callback) _presentation_callback->CommonEvent(CommonEventWindowSetBlur); }
		double ConsoleState::GetBlurBehind(void) const { return _window_blur_factor; }
		void ConsoleState::SetWindowColor(Color color) { _window_background = color; if (_presentation_callback) _presentation_callback->CommonEvent(CommonEventWindowSetColor); }
		Color ConsoleState::GetWindowColor(void) const { return _window_background; }
		void ConsoleState::SetWindowIcon(Codec::Image * icon) { _window_icon.SetRetain(icon); if (_presentation_callback) _presentation_callback->CommonEvent(CommonEventWindowSetIcon); }
		Codec::Image * ConsoleState::GetWindowIcon(void) const { return _window_icon; }
		int ConsoleState::GetBufferWidth(void) const { return _screen->_size.x; }
		int ConsoleState::GetBufferHeight(void) const { return _screen->_size.y; }
		int ConsoleState::GetBufferLimitHeight(void) const { return _screen->_scrollable ? _max_height : _screen->_size.y; }
		void ConsoleState::SetBufferLimitHeight(int limit) { _max_height = limit; }
		Point ConsoleState::GetWindowSize(void) const { return _window_size; }
		Point ConsoleState::GetTabulationSize(void) const { return _tab_size; }
		void ConsoleState::SetTabulationSize(const Point & size) { _tab_size = size; }
		bool ConsoleState::GetCloseOnEndOfStream(void) const { return _close_on_eos; }
		void ConsoleState::SetCloseOnEndOfStream(bool ceos) { _close_on_eos = ceos; }
		void ConsoleState::ResizeBufferEnforced(int width, int height)
		{
			auto & screen = *_screen;
			if (width == screen._size.x && height == screen._size.y) return;
			if (height < screen._size.y) {
				int dh = screen._size.y - height;
				screen._cells.SetLength(screen._size.x * height);
				screen._size.y = height;
				if (_presentation_callback) _presentation_callback->RemoveLines(height, dh);
			}
			if (width != screen._size.x) {
				if (_presentation_callback) _presentation_callback->InvalidateLines(0, screen._size.y);
				if (width > screen._size.x) {
					screen._cells.SetLength(width * screen._size.y);
					for (int j = screen._size.y - 1; j >= 0; j--) {
						for (int i = width - 1; i >= screen._size.x; i--) {
							screen._cells[i + j * width].ucs = L' ';
							screen._cells[i + j * width].attr = screen._cells[screen._size.x - 1 + j * screen._size.x].attr;
						}
						for (int i = screen._size.x - 1; i >= 0; i--) {
							screen._cells[i + j * width].ucs = screen._cells[i + j * screen._size.x].ucs;
							screen._cells[i + j * width].attr = screen._cells[i + j * screen._size.x].attr;
						}
					}
				} else {
					for (int j = 0; j < screen._size.y; j++) {
						for (int i = 0; i < width; i++) {
							screen._cells[i + j * width].ucs = screen._cells[i + j * screen._size.x].ucs;
							screen._cells[i + j * width].attr = screen._cells[i + j * screen._size.x].attr;
						}
					}
					screen._cells.SetLength(width * screen._size.y);
				}
				screen._size.x = width;
			}
			if (height > screen._size.y) {
				int dh = height - screen._size.y;
				screen._cells.SetLength(screen._size.x * height);
				for (int i = screen._size.x * screen._size.y; i < screen._size.x * height; i++) {
					screen._cells[i].ucs = L' ';
					screen._cells[i].attr = _state_current._attribute;
				}
				screen._size.y = height;
				if (_presentation_callback) _presentation_callback->InsertLines(height - dh, dh);
			}
			if (screen._caret.y >= screen._size.y) {
				screen._caret.y = screen._size.y - 1;
				if (_presentation_callback) _presentation_callback->CommonEvent(CommonEventCaretMove);
			}
			screen._scroll_region_from = screen._scroll_region_num_lines = -1;
			if (_output_callback) _output_callback->ScreenBufferResize(width, height);
		}
		void ConsoleState::ResizeBuffer(int width, int height)
		{
			if (_screen->_scrollable) ResizeBufferEnforced(max(width, 1), _screen->_size.y);
			else ResizeBufferEnforced(max(width, 1), max(height, 1));
		}
		bool ConsoleState::IsBufferScrollable(void) const { return _screen->_scrollable; }
		int ConsoleState::GetBufferOffset(void) const { return _screen->_scroll_offset; }
		void ConsoleState::SetBufferOffset(int offset) const { _screen->_scroll_offset = offset; if (_presentation_callback) _presentation_callback->CommonEvent(CommonEventBufferScroll); }
		void ConsoleState::ScrollCurrentRange(int by)
		{
			if (!by) return;
			auto & screen = *_screen;
			int srm = screen._scroll_region_from >= 0 ? screen._scroll_region_from : 0;
			int srx = screen._scroll_region_from >= 0 ? screen._scroll_region_from + screen._scroll_region_num_lines - 1 : screen._size.y - 1;
			if (screen._caret.y < srm || screen._caret.y > srx) { srm = 0; srx = screen._size.y - 1; }
			int num_lines = srx - srm + 1;
			if (_presentation_callback) _presentation_callback->MoveLines(srm, num_lines, by);
			if (by > 0) {
				int num_scroll = max(num_lines - by, 0);
				int num_rem = num_lines - num_scroll;
				for (int i = screen._size.x * srm; i < screen._size.x * (srx + 1 - num_rem); i++) {
					screen._cells[i] = screen._cells[i + screen._size.x * num_rem];
				}
				for (int i = screen._size.x * (srx + 1 - num_rem); i < screen._size.x * (srx + 1); i++) {
					screen._cells[i].ucs = L' ';
					screen._cells[i].attr = _state_current._attribute;
				}
				screen._caret.y -= by;
				if (screen._caret.y < srm) screen._caret.y = srm;
				else if (screen._caret.y > srx) screen._caret.y = srx;
			} else {
				int num_scroll = max(num_lines + by, 0);
				int num_rem = num_lines - num_scroll;
				for (int i = screen._size.x * (srx + 1 - num_rem) - 1; i >= screen._size.x * srm; i--) {
					screen._cells[i + screen._size.x * num_rem] = screen._cells[i];
				}
				for (int i = screen._size.x * srm; i < screen._size.x * (srm + num_rem); i++) {
					screen._cells[i].ucs = L' ';
					screen._cells[i].attr = _state_current._attribute;
				}
				screen._caret.y -= by;
				if (screen._caret.y < srm) screen._caret.y = srm;
				else if (screen._caret.y > srx) screen._caret.y = srx;
			}
			if (_presentation_callback) _presentation_callback->CommonEvent(CommonEventCaretMove);
		}
		void ConsoleState::LineFeed(void)
		{
			if (_screen->_caret.y == _screen->_scroll_region_from + _screen->_scroll_region_num_lines - 1) {
				ScrollCurrentRange(1);
			} else if (_screen->_caret.y == _screen->_size.y - 1) {
				if (!_screen->_scrollable || _screen->_size.y >= _max_height) ScrollCurrentRange(1);
				else ResizeBufferEnforced(_screen->_size.x, _screen->_size.y + 1);
			}
			_screen->_caret.y++;
			if (_presentation_callback) {
				if (_state_current._caret_style != CaretStyle::Null) _presentation_callback->ScrollToLine(_screen->_caret.y);
				_presentation_callback->CommonEvent(CommonEventCaretMove);
			}
		}
		void ConsoleState::CarriageReturn(void)
		{
			if (_screen->_caret.x) {
				_screen->_caret.x = 0;
				if (_presentation_callback) _presentation_callback->CommonEvent(CommonEventCaretMove);
			}
		}
		void ConsoleState::Print(const uint32 * codes, int length)
		{
			uint com_notify = 0;
			bool cl_inv = false;
			for (int i = 0; i < length; i++) {
				if (codes[i] >= 32) {
					if (_screen->_caret.x >= _screen->_size.x) {
						if (cl_inv) { if (_presentation_callback) _presentation_callback->InvalidateLines(_screen->_caret.y, 1); cl_inv = false; }
						LineFeed();
						CarriageReturn();
					}
					int index = _screen->_caret.x + _screen->_caret.y * _screen->_size.x;
					_screen->_cells[index].ucs = codes[i];
					_screen->_cells[index].attr = _state_current._attribute;
					cl_inv = true;
					_screen->_caret.x++;
					com_notify |= CommonEventCaretMove;
				} else if (codes[i] == L'\a') {
					Audio::Beep();
				} else if (codes[i] == L'\b') {
					if (_screen->_caret.x) {
						_screen->_caret.x--;
						com_notify |= CommonEventCaretMove;
					}
				} else if (codes[i] == L'\t') {
					_screen->_caret.x += _tab_size.x - (_screen->_caret.x % _tab_size.x);
					com_notify |= CommonEventCaretMove;
				} else if (codes[i] == L'\n') {
					if (cl_inv) { if (_presentation_callback) _presentation_callback->InvalidateLines(_screen->_caret.y, 1); cl_inv = false; }
					LineFeed();
				} else if (codes[i] == L'\v') {
					do {
						int y = _screen->_caret.y;
						if (cl_inv) { if (_presentation_callback) _presentation_callback->InvalidateLines(_screen->_caret.y, 1); cl_inv = false; }
						LineFeed();
						if (y == _screen->_caret.y) break;
					} while (_screen->_caret.y % _tab_size.y);
				} else if (codes[i] == L'\r') {
					CarriageReturn();
				}
			}
			if (cl_inv && _presentation_callback) _presentation_callback->InvalidateLines(_screen->_caret.y, 1);
			if (com_notify && _presentation_callback) _presentation_callback->CommonEvent(com_notify);
		}
		void ConsoleState::RevertCaretVisuals(void)
		{
			SetCaretStyle(_state_default._caret_style);
			SetCaretBlinks(_state_default._caret_enable_blinking);
			SetCaretWeight(_state_default._caret_size);
		}
		void ConsoleState::SetCaretStyle(CaretStyle style)
		{
			_state_current._caret_style = style;
			if (_presentation_callback) _presentation_callback->CommonEvent(CommonEventCaretReshape);
		}
		CaretStyle ConsoleState::GetCaretStyle(void) const { return _state_current._caret_style; }
		void ConsoleState::SetCaretBlinks(bool blinks)
		{
			_state_current._caret_enable_blinking = blinks;
			if (_presentation_callback) _presentation_callback->CommonEvent(CommonEventCaretReshape);
		}
		bool ConsoleState::GetCaretBlinks(void) const { return _state_current._caret_enable_blinking; }
		void ConsoleState::SetCaretWeight(double weight)
		{
			_state_current._caret_size = weight;
			if (_presentation_callback) _presentation_callback->CommonEvent(CommonEventCaretReshape);
		}
		double ConsoleState::GetCaretWeight(void) const { return _state_current._caret_size; }
		void ConsoleState::GetCaretPosition(Point & caret) const { caret = _screen->_caret; }
		void ConsoleState::SetCaretPosition(const Point & caret)
		{
			if (_screen->_scrollable && caret.y >= _screen->_size.y) ResizeBufferEnforced(_screen->_size.x, min(caret.y + 1, _max_height));
			_screen->_caret.x = max(min(caret.x, _screen->_size.x - 1), 0);
			_screen->_caret.y = max(min(caret.y, _screen->_size.y - 1), 0);
			if (_presentation_callback) _presentation_callback->CommonEvent(CommonEventCaretMove);
		}
		bool ConsoleState::ReadCell(int x, int y, Cell & cell) const
		{
			auto & screen = *_screen;
			if (x < 0 || y < 0 || x >= screen._size.x || y >= screen._size.y) return false;
			cell = screen._cells[x + y * screen._size.x];
			return true;
		}
		void ConsoleState::ReadLine(int y, const Cell ** first, int * length) const
		{
			*first = _screen->_cells.GetBuffer() + y * _screen->_size.x;
			*length = _screen->_size.x;
		}
		CellAttribute ConsoleState::GetCurrentAttribution(void) const { return _state_current._attribute; }
		void ConsoleState::SetAttributionFlags(uint mask_alternate, uint values)
		{
			_state_current._attribute.flags &= ~mask_alternate;
			_state_current._attribute.flags |= (values & mask_alternate);
		}
		void ConsoleState::RevertAttributionFlags(uint mask_alternate)
		{
			_state_current._attribute.flags &= ~mask_alternate;
			_state_current._attribute.flags |= (_state_default._attribute.flags & mask_alternate);
		}
		void ConsoleState::SetForegroundColor(Color color) { _state_current._attribute.foreground = color; }
		void ConsoleState::SetForegroundColorIndex(int index)
		{
			if (index < 0 || index > 15) {
				index = index == -1 ? _state_current._default_foreground : _state_current._default_background;
			}
			SetForegroundColor(_state_current._palette_foreground[index]);
		}
		void ConsoleState::SetBackgroundColor(Color color) { _state_current._attribute.background = color; }
		void ConsoleState::SetBackgroundColorIndex(int index)
		{
			if (index < 0 || index > 15) {
				index = index == -1 ? _state_current._default_background : _state_current._default_foreground;
			}
			SetBackgroundColor(_state_current._palette_background[index]);
		}
		void ConsoleState::Erase(bool screen, int part)
		{
			auto & scr = *_screen;
			if (screen) {
				if (part == 1) {
					for (int y = 0; y < scr._caret.y; y++) {
						for (int x = 0; x < scr._size.x; x++) {
							int index = x + y * scr._size.x;
							scr._cells[index].ucs = L' ';
							scr._cells[index].attr = _state_current._attribute;
						}
					}
					for (int x = 0; x < min(scr._size.x, scr._caret.x + 1); x++) {
						int index = x + scr._caret.y * scr._size.x;
						scr._cells[index].ucs = L' ';
						scr._cells[index].attr = _state_current._attribute;
					}
					if (_presentation_callback) _presentation_callback->InvalidateLines(0, scr._caret.y + 1);
				} else if (part == 0) {
					for (int x = scr._caret.x; x < scr._size.x; x++) {
						int index = x + scr._caret.y * scr._size.x;
						scr._cells[index].ucs = L' ';
						scr._cells[index].attr = _state_current._attribute;
					}
					for (int y = scr._caret.y + 1; y < scr._size.y; y++) {
						for (int x = 0; x < scr._size.x; x++) {
							int index = x + y * scr._size.x;
							scr._cells[index].ucs = L' ';
							scr._cells[index].attr = _state_current._attribute;
						}
					}
					if (_presentation_callback) _presentation_callback->InvalidateLines(scr._caret.y, scr._size.y - scr._caret.y);
				} else if (part == 2) {
					for (int y = 0; y < scr._size.y; y++) {
						for (int x = 0; x < scr._size.x; x++) {
							int index = x + y * scr._size.x;
							scr._cells[index].ucs = L' ';
							scr._cells[index].attr = _state_current._attribute;
						}
					}
					if (_presentation_callback) _presentation_callback->InvalidateLines(0, scr._size.y);
				}
			} else {
				if (part == 1) {
					for (int x = 0; x < min(scr._size.x, scr._caret.x + 1); x++) {
						int index = x + scr._caret.y * scr._size.x;
						scr._cells[index].ucs = L' ';
						scr._cells[index].attr = _state_current._attribute;
					}
					if (_presentation_callback) _presentation_callback->InvalidateLines(scr._caret.y, 1);
				} else if (part == 0) {
					for (int x = scr._caret.x; x < scr._size.x; x++) {
						int index = x + scr._caret.y * scr._size.x;
						scr._cells[index].ucs = L' ';
						scr._cells[index].attr = _state_current._attribute;
					}
					if (_presentation_callback) _presentation_callback->InvalidateLines(scr._caret.y, 1);
				} else if (part == 2) {
					for (int x = 0; x < scr._size.x; x++) {
						int index = x + scr._caret.y * scr._size.x;
						scr._cells[index].ucs = L' ';
						scr._cells[index].attr = _state_current._attribute;
					}
					if (_presentation_callback) _presentation_callback->InvalidateLines(scr._caret.y, 1);
				}
			}
		}
		void ConsoleState::ClearScreen(void)
		{
			if (_screen->_scrollable) ResizeBufferEnforced(_screen->_size.x, 1);
			_screen->_caret = Point(0, 0);
			for (int i = 0; i < _screen->_size.x * _screen->_size.y; i++) {
				_screen->_cells[i].ucs = L' ';
				_screen->_cells[i].attr = _state_current._attribute;
			}
			if (_presentation_callback) {
				_presentation_callback->InvalidateLines(0, _screen->_size.y);
				_presentation_callback->CommonEvent(CommonEventCaretMove);
			}
		}
		void ConsoleState::CreateScreenBuffer(const Point & size, bool scrollable)
		{
			SafePointer<ScreenBuffer> buffer = new ScreenBuffer(_state_current._attribute);
			_screen_stack.Append(_screen);
			_screen = buffer;
			_screen->_scrollable = scrollable;
			if (_presentation_callback) _presentation_callback->CommonEvent(CommonEventBufferReset);
			ResizeBuffer(size.x, size.y);
		}
		void ConsoleState::RemoveScreenBuffer(void)
		{
			if (!_screen_stack.Length()) return;
			_screen.SetRetain(_screen_stack.LastElement());
			_screen_stack.RemoveLast();
			if (_presentation_callback) _presentation_callback->CommonEvent(CommonEventBufferReset);
		}
		void ConsoleState::SwapScreenBuffers(void)
		{
			if (!_screen_stack.Length()) return;
			auto buffer = _screen;
			_screen.SetRetain(_screen_stack.LastElement());
			_screen_stack.RemoveLast();
			_screen_stack.Append(buffer);
			if (_presentation_callback) _presentation_callback->CommonEvent(CommonEventBufferReset);
		}
		void ConsoleState::PushCaretPosition(void) { _screen->_caret_stack << _screen->_caret; }
		void ConsoleState::PopCaretPosition(void)
		{
			if (_screen->_caret_stack.Length()) {
				_screen->_caret = _screen->_caret_stack.LastElement();
				_screen->_caret_stack.RemoveLast();
				if (_presentation_callback) _presentation_callback->CommonEvent(CommonEventCaretMove);
			}
		}
		void ConsoleState::SetScrollingRectangle(int line_first, int line_count)
		{
			_screen->_scroll_region_from = line_first;
			_screen->_scroll_region_num_lines = line_count;
		}
		void ConsoleState::ResetScrollingRectangle(void) { _screen->_scroll_region_from = _screen->_scroll_region_num_lines = -1; }
		void ConsoleState::WritePalette(uint flags, int index, Color color)
		{
			if (index < 0 || index > 15) return;
			if (flags & WritePaletteFlagOverwrite) {
				if (flags & WritePaletteFlagForeground) {
					if (flags & WritePaletteFlagRevert) _state_current._palette_foreground[index] = _state_default._palette_foreground[index];
					else _state_current._palette_foreground[index] = color;
				}
				if (flags & WritePaletteFlagBackground) {
					if (flags & WritePaletteFlagRevert) _state_current._palette_background[index] = _state_default._palette_background[index];
					else _state_current._palette_background[index] = color;
				}
			}
			if (flags & WritePaletteFlagSetDefault) {
				if (flags & WritePaletteFlagForeground) {
					if (flags & WritePaletteFlagRevert) _state_current._default_foreground = _state_default._default_foreground;
					else _state_current._default_foreground = index;
				}
				if (flags & WritePaletteFlagBackground) {
					if (flags & WritePaletteFlagRevert) _state_current._default_background = _state_default._default_background;
					else _state_current._default_background = index;
				}
			}
		}
		void ConsoleState::OverrideDefaults(void) { _state_default = _state_current; }
		void ConsoleState::UpdateLine(int y, const Cell * cells, int length)
		{
			if (y < 0 || y >= _screen->_size.y) return;
			int w = min(_screen->_size.x, length);
			for (int i = 0; i < w; i++) {
				_screen->_cells[y * _screen->_size.x + i] = cells[i];
			}
			if (_presentation_callback) _presentation_callback->InvalidateLines(y, 1);
		}
		void ConsoleState::UpdateLine(int y, const uint * chars, int length, bool keep_attributes)
		{
			if (y < 0 || y >= _screen->_size.y) return;
			int w = min(_screen->_size.x, length);
			for (int i = 0; i < w; i++) {
				_screen->_cells[y * _screen->_size.x + i].ucs = chars[i];
				if (!keep_attributes) _screen->_cells[y * _screen->_size.x + i].attr = _state_current._attribute;
			}
			if (_presentation_callback) _presentation_callback->InvalidateLines(y, 1);
		}

		void ConsoleState::SetPicture(Codec::Frame * picture)
		{
			if (!picture || (picture->GetPixelFormat() == Codec::PixelFormat::B8G8R8A8 &&
				picture->GetAlphaMode() == Codec::AlphaMode::Premultiplied && picture->GetScanOrigin() == Codec::ScanOrigin::TopDown)) {
				_screen->_picture.SetRetain(picture);
			} else _screen->_picture = picture->ConvertFormat(Codec::PixelFormat::B8G8R8A8, Codec::AlphaMode::Premultiplied, Codec::ScanOrigin::TopDown);
			if (_presentation_callback) _presentation_callback->CommonEvent(CommonEventPictureReset);
		}
		void ConsoleState::NotifyPictureUpdated(void) const { if (_presentation_callback) _presentation_callback->CommonEvent(CommonEventPictureUpdate); }
		Codec::Frame * ConsoleState::GetPicture(void) const { return _screen->_picture; }
		void ConsoleState::SetPictureMode(Windows::ImageRenderMode mode) { _screen->_picture_mode = mode; if (_presentation_callback) _presentation_callback->CommonEvent(CommonEventPictureReshape); }
		Windows::ImageRenderMode ConsoleState::GetPictureMode(void) const { return _screen->_picture_mode; }

		bool ConsoleState::HandleKey(uint code, uint flags) { if (_output_callback) return _output_callback->OutputKey(code, flags); else return false; }
		void ConsoleState::HandleChar(uint ucs) { if (_output_callback) _output_callback->OutputText(ucs); }
		void ConsoleState::HandleWindowResize(int width, int height) { _window_size = Point(width, height); if (_output_callback) _output_callback->WindowResize(width, height); }
		void ConsoleState::SetCallback(IPresentationCallback * callback) { _presentation_callback = callback; }
		void ConsoleState::SetCallback(IOutputCallback * callback) { _output_callback = callback; }
		void ConsoleState::Terminate(void) { if (_output_callback) _output_callback->Terminate(); }

		void LoadConsolePreset(const string & title, const string & preset_path, ConsoleState ** state, ConsoleCreateMode * mode)
		{
			SafePointer<Streaming::Stream> stream = new Streaming::FileStream(preset_path, Streaming::AccessRead, Streaming::OpenExisting);
			SafePointer<Storage::Registry> registry = Storage::LoadRegistry(stream);
			if (!registry) {
				stream->Seek(0, Streaming::Begin);
				registry = Storage::CompileTextRegistry(stream);
			}
			if (!registry) throw InvalidFormatException();
			*state = new ConsoleState(registry);
			(*state)->SetTitle(title);
			auto background = registry->GetValueString(L"Pictura/Semita");
			if (background.Length()) {
				SafePointer<Streaming::Stream> image_stream = new Streaming::FileStream(background, Streaming::AccessRead, Streaming::OpenExisting);
				SafePointer<Codec::Frame> image = Codec::DecodeFrame(image_stream);
				(*state)->SetPicture(image);
			}
			mode->size.x = registry->GetValueInteger(L"Fenestra/Latitudo");
			mode->size.y = registry->GetValueInteger(L"Fenestra/Altitudo");
			mode->fullscreen = registry->GetValueBoolean(L"Fenestra/Ultima");
		}
		void LoadConsolePreset(const string & title, DataBlock * preset, ConsoleState ** state, ConsoleCreateMode * mode)
		{
			SafePointer<Streaming::Stream> stream = new Streaming::MemoryStream(preset->GetBuffer(), preset->Length());
			SafePointer<Storage::Registry> registry = Storage::LoadRegistry(stream);
			if (!registry) {
				stream->Seek(0, Streaming::Begin);
				registry = Storage::CompileTextRegistry(stream);
			}
			if (!registry) throw InvalidFormatException();
			*state = new ConsoleState(registry);
			(*state)->SetTitle(title);
			mode->size.x = registry->GetValueInteger(L"Fenestra/Latitudo");
			mode->size.y = registry->GetValueInteger(L"Fenestra/Altitudo");
			mode->fullscreen = registry->GetValueBoolean(L"Fenestra/Ultima");
		}
	}
}