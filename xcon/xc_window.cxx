#include "xc_window.h"

using namespace Engine::Windows;
using namespace Engine::Graphics;
using namespace Engine::UI;

namespace Engine
{
	namespace XC
	{
		class ApplicationCallback : public IApplicationCallback
		{
			IWindow * _hosted_window;
		public:
			ApplicationCallback(void) : _hosted_window(0) {}
			virtual bool IsHandlerEnabled(ApplicationHandler event) override
			{
				if (event == ApplicationHandler::Terminate) return true;
				else return false;
			}
			virtual bool Terminate(void) override
			{
				if (_hosted_window) {
					auto callback = _hosted_window->GetCallback();
					if (callback) callback->WindowClose(_hosted_window);
				}
				return true;
			}
			void SetWindowBeingHosted(IWindow * window) noexcept { _hosted_window = window; }
		};
		class WindowCallback : public IEventCallback, IPresentationCallback
		{
			SafePointer<ConsoleState> _console;
			SafePointer<I2DDeviceContextFactory> _factory;
			SafePointer<IDevice> _device;
			SafePointer<IWindowLayer> _layer;
			SafePointer<ITexture> _background_texture;
			SafePointer<ITexture> _primary_surface;
			SafePointer<IBitmapBrush> _background_brush;
			SafePointer<IFont> _primary_font;
			Volumes::ObjectDictionary<uint, IFont> _fonts;
			IWindow * _window;
			Point _cell;
			bool _fullscreen, _caret_moved;
		private:
			IFont * _query_font(uint flags)
			{
				if (flags == 0) return _primary_font;
				auto font = _fonts.GetObjectByKey(flags);
				if (font) return font;
				bool bold = (flags & CellFlagBold) != 0;
				bool italic = (flags & CellFlagItalic) != 0;
				bool underline = (flags & CellFlagUnderline) != 0;
				string font_face;
				int font_height;
				_console->GetFont(font_face, font_height);
				SafePointer<IFont> ft = _factory->LoadFont(font_face, int(font_height * CurrentScaleFactor), bold ? 700 : 400, italic, underline, false);
				_fonts.Append(flags, ft);
				return ft;
			}
			void _initialize_fonts(void)
			{
				_fonts.Clear();
				string font_face;
				int font_height;
				_console->GetFont(font_face, font_height);
				_primary_font = _factory->LoadFont(font_face, int(font_height * CurrentScaleFactor), 400, false, false, false);
				_cell.x = _primary_font->GetWidth();
				_cell.y = _primary_font->GetLineSpacing();
			}
			void _update_cursor(void)
			{
				auto cs = GetControlSystem(_window);
				if (_fullscreen) {
					cs->OverrideSystemCursor(SystemCursorClass::Arrow, GetWindowSystem()->GetSystemCursor(SystemCursorClass::Null));
				} else {
					cs->OverrideSystemCursor(SystemCursorClass::Arrow, GetWindowSystem()->GetSystemCursor(SystemCursorClass::Arrow));
				}
			}
			void _recreate(void)
			{
				if (_window) {
					auto window = _window;
					_window = 0;
					ConsoleCreateMode mode;
					auto console = _console;
					mode.fullscreen = _fullscreen;
					mode.size.x = mode.size.y = 1;
					auto rect = window->GetPosition();
					window->Destroy();
					if (rect.Right > rect.Left && rect.Bottom > rect.Top) CreateConsoleWindow(console, mode, rect);
					else CreateConsoleWindow(console, mode);
				}
			}
			void _align(void)
			{
				auto ss = int(CurrentScaleFactor * 8.0);
				auto marg = int(CurrentScaleFactor * _console->GetMargin());
				auto size = _window->GetClientSize();
				auto cell_height = _console->GetBufferHeight();
				auto scrollable = _console->IsBufferScrollable();
				auto visible_height = (size.y - 2 * marg) / _cell.y;
				if (visible_height < cell_height && scrollable) {
					_scroll->Show(true);
					_scroll->SetPosition(Box(size.x - ss, 0, size.x, size.y));
					_scroll->SetRange(0, max(cell_height - 1, 0));
					_scroll->SetPage(visible_height);
					_scroll->Line = 1;
					_view->SetPosition(Box(marg, marg, size.x - ss - marg, size.y - marg));
				} else {
					_scroll->Show(false);
					_scroll->SetScrollerPosition(0);
					_scroll->SetRange(0, 0);
					_view->SetPosition(Box(marg, marg, size.x - marg, size.y - marg));
				}
			}
			static Template::Shape * _color_shape(Color color)
			{
				SafePointer<Template::BarShape> bar = new Template::BarShape;
				bar->Position = Rectangle::Entire();
				bar->Gradient << Template::GradientPoint(color, 0.0);
				bar->Retain();
				return bar;
			}
			friend class _view_control;
			struct _line_info
			{
				bool dirty;
				ObjectArray<ITextBrush> ranges;
				Array<int> offsets;
				_line_info(void) : ranges(8), offsets(8), dirty(true) {}
				void Invalidate(void) noexcept { dirty = true; }
			};
			struct _render_state
			{
				int vp_width, vp_height;
				int rect_left, rect_top, rect_right, rect_bottom;
				int cell_x, cell_y;
			};
			class _view_control : public Control
			{
				friend class WindowCallback;
				WindowCallback & _callback;
				int _id;
				SafePointer<IInversionEffectBrush> _inversion;
				SafePointer<IColorBrush> _color;
				Array<_line_info> _lines;
				SafePointer<IPipelineState> _foreground_state, _background_state;
				SafePointer<ITexture> _preprint, _foreground, _background;
				SafePointer<Codec::Frame> _foreground_frame, _background_frame;
			public:
				_view_control(WindowCallback & callback) : _callback(callback), _id(0), _lines(0x100)
				{
					auto device = _callback._device.Inner();
					SafePointer<Streaming::Stream> stream = Assembly::QueryResource(L"SL");
					SafePointer<IShaderLibrary> library = device->LoadShaderLibrary(stream);
					SafePointer<IShader> common = library->CreateShader(L"vertex_singulus");
					SafePointer<IShader> foreground = library->CreateShader(L"punctum_primum");
					SafePointer<IShader> background = library->CreateShader(L"punctum_secundum");
					PipelineStateDesc desc = DefaultPipelineStateDesc(common, foreground, PixelFormat::B8G8R8A8_unorm);
					desc.RenderTarget[0].Flags = RenderTargetFlagBlendingEnabled;
					desc.RenderTarget[0].BaseFactorAlpha = BlendingFactor::InvertedOverAlpha;
					desc.RenderTarget[0].BaseFactorRGB = BlendingFactor::InvertedOverAlpha;
					desc.RenderTarget[0].OverFactorAlpha = BlendingFactor::One;
					desc.RenderTarget[0].OverFactorRGB = BlendingFactor::One;
					desc.RenderTarget[0].BlendAlpha = BlendingFunction::Add;
					desc.RenderTarget[0].BlendRGB = BlendingFunction::Add;
					_foreground_state = device->CreateRenderingPipelineState(desc);
					desc.PixelShader = background;
					_background_state = device->CreateRenderingPipelineState(desc);
				}
				virtual ~_view_control(void) override {}
				virtual void Render(Graphics::I2DDeviceContext * device, const Box & at) override
				{
					int width = at.Right - at.Left;
					int height = at.Bottom - at.Top;
					int scroll_delta = _callback._console->GetBufferOffset();
					int cw = width / _callback._cell.x;
					int ch = height / _callback._cell.y;
					if (_callback._caret_moved) {
						_callback._caret_moved = false;
						device->SetCaretReferenceTime(device->GetAnimationTime());
					}
					if (cw <= 0 || ch <= 0) return;
					if (!_lines.Length()) _lines.SetLength(_callback._console->GetBufferHeight());
					int line_from = min(max(scroll_delta, 0), _lines.Length() - 1);
					int line_to = min(line_from + ch, _lines.Length());
					for (int i = line_from; i < line_to; i++) if (_lines[i].dirty) {
						auto & line = _lines[i];
						const Cell * line_ucs;
						int length;
						_callback._console->ReadLine(i, &line_ucs, &length);
						Array<uint32> ucs(length);
						Array<double> adv(length);
						line.ranges.Clear();
						line.offsets.Clear();
						int j = 0;
						while (j < length) {
							int k = j;
							while (j < length && line_ucs[j].attr.flags == line_ucs[k].attr.flags) j++;
							ucs.SetLength(j - k);
							adv.SetLength(ucs.Length());
							for (int l = k; l < j; l++) {
								ucs[l - k] = line_ucs[l].ucs;
								adv[l - k] = _callback._cell.x * l;
							}
							int offset = adv[0];
							for (int l = 0; l < j - k; l++) adv[l] = adv[l + 1] - adv[l];
							adv[j - k - 1] = _callback._cell.x;
							auto tf = _callback._query_font(line_ucs[k].attr.flags);
							SafePointer<ITextBrush> tb = device->CreateTextBrush(tf, ucs.GetBuffer(), ucs.Length(), 0, 0, Color(255, 255, 255));
							tb->SetCharAdvances(adv);
							line.ranges.Append(tb);
							line.offsets.Append(offset);
						}
						line.dirty = false;
					}
					auto ctx = _callback._device->GetDeviceContext();
					ctx->EndCurrentPass();
					int ww = cw * _callback._cell.x;
					int wh = ch * _callback._cell.y;
					if (!_foreground || _foreground->GetWidth() != cw || _foreground->GetHeight() != line_to - line_from) {
						TextureDesc desc;
						desc.Type = TextureType::Type2D;
						desc.Format = PixelFormat::R8G8B8A8_unorm;
						desc.Usage = ResourceUsageCPUWrite | ResourceUsageShaderRead;
						desc.Width = cw;
						desc.Height = line_to - line_from;
						desc.MipmapCount = 1;
						desc.MemoryPool = ResourceMemoryPool::Default;
						_foreground = _callback._device->CreateTexture(desc);
						_background = _callback._device->CreateTexture(desc);
						_foreground_frame = new Codec::Frame(cw, line_to - line_from, Codec::PixelFormat::R8G8B8A8, Codec::AlphaMode::Straight, Codec::ScanOrigin::TopDown);
						_background_frame = new Codec::Frame(cw, line_to - line_from, Codec::PixelFormat::R8G8B8A8, Codec::AlphaMode::Straight, Codec::ScanOrigin::TopDown);
					}
					ctx->BeginMemoryManagementPass();
					for (int j = line_from; j < line_to; j++) {
						const Cell * cells;
						int length;
						_callback._console->ReadLine(j, &cells, &length);
						for (int i = 0; i < length; i++) {
							_foreground_frame->SetPixel(i, j - line_from, cells[i].attr.foreground.Value);
							_background_frame->SetPixel(i, j - line_from, cells[i].attr.background.Value);
						}
					}
					ResourceInitDesc init;
					init.Data = _foreground_frame->GetData();
					init.DataPitch = _foreground_frame->GetScanLineLength();
					ctx->UpdateResourceData(_foreground, SubresourceIndex(0, 0), VolumeIndex(0, 0, 0),
						VolumeIndex(_foreground->GetWidth(), _foreground->GetHeight(), 1), init);
					init.Data = _background_frame->GetData();
					init.DataPitch = _background_frame->GetScanLineLength();
					ctx->UpdateResourceData(_background, SubresourceIndex(0, 0), VolumeIndex(0, 0, 0),
						VolumeIndex(_background->GetWidth(), _background->GetHeight(), 1), init);
					ctx->EndCurrentPass();
					if (!_preprint || _preprint->GetWidth() != ww || _preprint->GetHeight() != wh) {
						TextureDesc desc;
						desc.Type = TextureType::Type2D;
						desc.Format = PixelFormat::B8G8R8A8_unorm;
						desc.Usage = ResourceUsageRenderTarget | ResourceUsageShaderRead;
						desc.Width = ww;
						desc.Height = wh;
						desc.MipmapCount = 1;
						desc.MemoryPool = ResourceMemoryPool::Default;
						_preprint = _callback._device->CreateTexture(desc);
					}
					auto rtv = CreateRenderTargetView(_preprint, Math::Vector4f(0.0f, 0.0f, 0.0f, 0.0f));
					ctx->BeginRenderingPass(1, &rtv, 0);
					ctx->EndCurrentPass();
					ctx->Begin2DRenderingPass(_preprint);
					for (int i = line_from; i < line_to; i++) if (!_lines[i].dirty) {
						for (int j = 0; j < _lines[i].ranges.Length(); j++) {
							auto & r = _lines[i].ranges[j];
							auto & o = _lines[i].offsets[j];
							device->Render(&r, Box(o, (i - scroll_delta) * _callback._cell.y, ww, (i - scroll_delta + 1) * _callback._cell.y), false);
						}
					}
					ctx->EndCurrentPass();
					rtv = CreateRenderTargetView(_callback._primary_surface, TextureLoadAction::Load);
					ctx->BeginRenderingPass(1, &rtv, 0);
					_render_state rs;
					rs.vp_width = _callback._primary_surface->GetWidth();
					rs.vp_height = _callback._primary_surface->GetHeight();
					rs.rect_left = at.Left;
					rs.rect_top = at.Top;
					rs.rect_right = rs.rect_left + ww;
					rs.rect_bottom = rs.rect_top + (line_to - line_from) * _callback._cell.y;
					rs.cell_x = _callback._cell.x;
					rs.cell_y = _callback._cell.y;
					ctx->SetViewport(0.0f, 0.0f, _callback._primary_surface->GetWidth(), _callback._primary_surface->GetHeight(), 0.0f, 1.0f);
					ctx->SetVertexShaderConstant(0, &rs, sizeof(rs));
					ctx->SetPixelShaderConstant(0, &rs, sizeof(rs));
					ctx->SetPixelShaderResource(0, _background);
					ctx->SetRenderingPipelineState(_background_state);
					ctx->DrawPrimitives(6, 0);
					ctx->SetPixelShaderResource(0, _foreground);
					ctx->SetPixelShaderResource(1, _preprint);
					ctx->SetRenderingPipelineState(_foreground_state);
					ctx->DrawPrimitives(6, 0);
					ctx->EndCurrentPass();
					ctx->Begin2DRenderingPass(_callback._primary_surface);
					if (GetFocus() != this) return;
					auto style = _callback._console->GetCaretStyle();
					auto blinks = _callback._console->GetCaretBlinks();
					if (style != CaretStyle::Null && (!blinks || device->IsCaretVisible())) {
						auto weight = _callback._console->GetCaretWeight();
						Cell cell;
						Point caret;
						_callback._console->GetCaretPosition(caret);
						if (caret.y >= scroll_delta && caret.y < scroll_delta + ch && _callback._console->ReadCell(caret.x, caret.y, cell)) {
							Box box(at.Left + caret.x * _callback._cell.x, at.Top + (caret.y - scroll_delta) * _callback._cell.y,
								at.Left + (caret.x + 1) * _callback._cell.x, at.Top + (caret.y - scroll_delta + 1) * _callback._cell.y);
							if (cell.attr.background.a > 127) {
								if (!_inversion) _inversion = device->CreateInversionEffectBrush();
								if (style == CaretStyle::Horizontal) {
									int size = max(int(weight * _callback._cell.y), 1);
									device->Render(_inversion, Box(box.Left, box.Bottom - size, box.Right, box.Bottom), false);
								} else if (style == CaretStyle::Vertical) {
									int size = max(int(weight * _callback._cell.x), 1);
									device->Render(_inversion, Box(box.Left, box.Top, box.Left + size, box.Bottom), false);
								} else if (style == CaretStyle::Cell) {
									int size = max(int(CurrentScaleFactor), 1);
									device->Render(_inversion, Box(box.Left, box.Top, box.Left + size, box.Bottom), false);
									device->Render(_inversion, Box(box.Left + size, box.Top, box.Right, box.Top + size), false);
									device->Render(_inversion, Box(box.Left + size, box.Bottom - size, box.Right, box.Bottom), false);
									device->Render(_inversion, Box(box.Right - size, box.Top + size, box.Right, box.Bottom - size), false);
								}
							} else {
								if (!_color) _color = device->CreateSolidColorBrush(Color(255, 255, 255));
								if (style == CaretStyle::Horizontal) {
									int size = max(int(weight * _callback._cell.y), 1);
									device->Render(_color, Box(box.Left, box.Bottom - size, box.Right, box.Bottom));
								} else if (style == CaretStyle::Vertical) {
									int size = max(int(weight * _callback._cell.x), 1);
									device->Render(_color, Box(box.Left, box.Top, box.Left + size, box.Bottom));
								} else if (style == CaretStyle::Cell) {
									int size = max(int(CurrentScaleFactor), 1);
									device->Render(_color, Box(box.Left, box.Top, box.Left + size, box.Bottom));
									device->Render(_color, Box(box.Left + size, box.Top, box.Right, box.Top + size));
									device->Render(_color, Box(box.Left + size, box.Bottom - size, box.Right, box.Bottom));
									device->Render(_color, Box(box.Right - size, box.Top + size, box.Right, box.Bottom - size));
								}
							}
						}
					}
				}
				virtual void ResetCache(void) override
				{
					_inversion.SetReference(0);
					_color.SetReference(0);
					_preprint.SetReference(0);
					_foreground.SetReference(0);
					_foreground_frame.SetReference(0);
					_background.SetReference(0);
					_background_frame.SetReference(0);
					_lines.Clear();
				}
				virtual void SetID(int ID) override { _id = ID; }
				virtual int GetID(void) override { return _id; }
				virtual Control * FindChild(int ID) override { if (ID == _id) return this; else return 0; }
				virtual void SetPosition(const Box & box) override
				{
					Control::SetPosition(box);
					int width = box.Right - box.Left;
					int height = box.Bottom - box.Top;
					int cw = width / _callback._cell.x;
					int ch = height / _callback._cell.y;
					if (cw > 0 && ch > 0) {
						_callback._console->ResizeBuffer(cw, ch);
						_callback._console->HandleWindowResize(cw, ch);
					}
				}
				virtual void FocusChanged(bool got_focus) override { Invalidate(); }
				virtual void ScrollVertically(double delta) override { _callback._scroll->SetScrollerPosition(_callback._scroll->Position + delta); }
				virtual bool KeyDown(int key_code) override
				{
					uint flags = 0;
					if (Keyboard::IsKeyPressed(KeyCodes::Shift)) flags |= IO::ConsoleKeyFlagShift;
					if (Keyboard::IsKeyPressed(KeyCodes::Control)) flags |= IO::ConsoleKeyFlagControl;
					if (Keyboard::IsKeyPressed(KeyCodes::Alternative)) flags |= IO::ConsoleKeyFlagAlternative;
					return _callback._console->HandleKey(key_code, flags);
				}
				virtual void CharDown(uint32 ucs_code) override { _callback._console->HandleChar(ucs_code); }
				virtual ControlRefreshPeriod GetFocusedRefreshPeriod(void) override { return ControlRefreshPeriod::CaretBlink; }
			};
			SafePointer<_view_control> _view;
			SafePointer<Controls::VerticalScrollBar> _scroll;
		public:
			WindowCallback(ConsoleState * state)
			{
				_window = 0;
				_fullscreen = _caret_moved = false;
				_console.SetRetain(state);
				_console->SetCallback(this);
				_factory = CreateDeviceContextFactory();
				_initialize_fonts();
				_device.SetRetain(GetCommonDevice());
				_view = new _view_control(*this);
				_scroll = new Controls::VerticalScrollBar;
				_view->SetID(101);
				_scroll->ID = 102;
				_scroll->ViewBarNormal = _color_shape(Color(0, 0, 0, 128));
				_scroll->ViewScrollerNormal = _color_shape(Color(255, 255, 255, 128));
				_scroll->ViewScrollerHot = _color_shape(Color(255, 255, 255, 192));
				_scroll->ViewScrollerPressed = _color_shape(Color(128, 128, 128, 192));
			}
			void MarkAsFullscreen(void) { _fullscreen = true; }
			void AdviseClientRect(Point cell_size, Point & size)
			{
				auto m = int(CurrentScaleFactor * _console->GetMargin());
				size.x = _cell.x * cell_size.x + 2 * m;
				size.y = _cell.y * cell_size.y + 2 * m;
			}
			virtual void CommonEvent(uint flags) noexcept override
			{
				if (flags & CommonEventPictureReset) {
					_background_texture.SetReference(0);
					_background_brush.SetReference(0);
					_window->InvalidateContents();
				}
				if (flags & CommonEventPictureUpdate) {
					auto picture = _console->GetPicture();
					if (picture && _background_texture) {
						auto context = _device->GetDeviceContext();
						ResourceInitDesc init;
						init.Data = picture->GetData();
						init.DataPitch = picture->GetStride();
						context->BeginMemoryManagementPass();
						context->UpdateResourceData(_background_texture, SubresourceIndex(0, 0), VolumeIndex(0, 0, 0),
							VolumeIndex(picture->GetWidth(), picture->GetHeight(), 1), init);
						context->EndCurrentPass();
					}
					_window->InvalidateContents();
				}
				if (flags & CommonEventPictureReshape) {
					_window->InvalidateContents();
				}
				if (flags & CommonEventWindowSetTitle) {
					if (_window) _window->SetText(_console->GetTitle());
				}
				if (flags & CommonEventWindowSetColor) _window->InvalidateContents();
				if (flags & CommonEventWindowSetIcon) {
					GetWindowSystem()->SetApplicationIcon(_console->GetWindowIcon());
				}
				if (flags & CommonEventTerminate) WindowClose(_window);
				if (flags & CommonEventWindowSetBlur) _recreate();
				if (flags & CommonEventCaretMove) { _caret_moved = true; _window->InvalidateContents(); }
				if (flags & CommonEventCaretReshape) _window->InvalidateContents();
				if (flags & CommonEventBufferReset) { _view->ResetCache(); _align(); }
				if (flags & CommonEventBufferScroll) _window->InvalidateContents();
				if (flags & CommonEventWindowSetFont) _recreate();
				if (flags & CommonEventWindowSetMargins) _align();
			}
			virtual void ScrollToLine(int line) noexcept override
			{
				Point caret;
				_console->GetCaretPosition(caret);
				auto rect = _view->GetPosition();
				auto height = rect.Bottom - rect.Top;
				auto ch = height / _cell.y;
				auto offset = _console->GetBufferOffset();
				if (caret.y < offset) _scroll->SetScrollerPosition(caret.y);
				else if (caret.y >= ch + offset) _scroll->SetScrollerPosition(caret.y - ch + 1);
			}
			virtual void InvalidateLines(int line_from, int count) noexcept override
			{
				if (!_view->_lines.Length()) return;
				for (int i = 0; i < count; i++) _view->_lines[line_from + i].Invalidate();
				_window->InvalidateContents();
			}
			virtual void RemoveLines(int line_from, int count) noexcept override
			{
				if (!_view->_lines.Length()) return;
				for (int i = line_from + count; i < _view->_lines.Length(); i++) {
					_view->_lines[i - count] = _view->_lines[i];
				}
				_view->_lines.SetLength(_view->_lines.Length() - count);
				_align();
				_window->InvalidateContents();
			}
			virtual void InsertLines(int line_from, int count) noexcept override
			{
				if (!_view->_lines.Length()) return;
				_view->_lines.SetLength(_view->_lines.Length() + count);
				for (int i = _view->_lines.Length() - 1; i >= line_from + count; i--) {
					_view->_lines[i] = _view->_lines[i - count];
				}
				for (int i = 0; i < count; i++) _view->_lines[line_from + i].Invalidate();
				_align();
				_window->InvalidateContents();
			}
			virtual void MoveLines(int line_from, int count, int dy) noexcept override
			{
				if (!_view->_lines.Length()) return;
				if (dy > 0) {
					for (int i = line_from; i < line_from + count - dy; i++) _view->_lines[i] = _view->_lines[i + dy];
					for (int i = max(line_from + count - dy, line_from); i < line_from + count; i++) _view->_lines[i].Invalidate();
				} else if (dy < 0) {
					for (int i = line_from + count - 1; i >= line_from - dy; i--) _view->_lines[i] = _view->_lines[i + dy];
					for (int i = 0; i < min(-dy, count); i++) _view->_lines[line_from + i].Invalidate();
				}
				_window->InvalidateContents();
			}
			virtual void Created(IWindow * window) override
			{
				auto root = GetRootControl(window);
				auto & accels = root->GetAcceleratorTable();
				accels << Accelerators::AcceleratorCommand(100, KeyCodes::Return, false, false, true);
				_window = window;
				auto size = window->GetClientSize();
				_update_cursor();
				_layer = _device->CreateWindowLayer(window, CreateWindowLayerDesc(size.x, size.y,
					PixelFormat::B8G8R8A8_unorm, ResourceUsageRenderTarget | ResourceUsageShaderRead));
				root->AddChild(_view);
				root->AddChild(_scroll);
				_align();
			}
			virtual void Destroyed(IWindow * window) override
			{
				_console->SetCallback(reinterpret_cast<IPresentationCallback *>(0));
				delete this;
			}
			virtual void Shown(IWindow * window, bool show) override { if (show && _fullscreen) _layer->SwitchToFullscreen(); }
			virtual void RenderWindow(IWindow * window) override
			{
				auto context = _device->GetDeviceContext();
				_primary_surface = _layer->QuerySurface();
				RenderTargetViewDesc rtvd;
				auto clr = _console->GetWindowColor();
				rtvd.LoadAction = TextureLoadAction::Clear;
				rtvd.ClearValue[3] = float(clr.a) / 255.0f;
				rtvd.ClearValue[0] = float(clr.r) / 255.0f * rtvd.ClearValue[3];
				rtvd.ClearValue[1] = float(clr.g) / 255.0f * rtvd.ClearValue[3];
				rtvd.ClearValue[2] = float(clr.b) / 255.0f * rtvd.ClearValue[3];
				rtvd.Texture = _primary_surface;
				context->BeginRenderingPass(1, &rtvd, 0);
				context->EndCurrentPass();
				context->Begin2DRenderingPass(_primary_surface);
				auto context_2d = context->Get2DContext();
				context_2d->SetAnimationTime(GetTimerValue());
				if (!_background_brush) {
					auto picture = _console->GetPicture();
					if (picture) {
						TextureDesc desc;
						desc.Type = TextureType::Type2D;
						desc.Format = PixelFormat::B8G8R8A8_unorm;
						desc.Width = picture->GetWidth();
						desc.Height = picture->GetHeight();
						desc.MipmapCount = 1;
						desc.MemoryPool = ResourceMemoryPool::Default;
						desc.Usage = ResourceUsageShaderRead | ResourceUsageCPUWrite;
						ResourceInitDesc init;
						init.Data = picture->GetData();
						init.DataPitch = picture->GetStride();
						_background_texture = _device->CreateTexture(desc, &init);
						_background_brush = context_2d->CreateTextureBrush(_background_texture, TextureAlphaMode::Premultiplied);
					}
				}
				if (_background_brush) {
					auto mode = _console->GetPictureMode();
					auto client = window->GetClientSize();
					if (client.x && client.y) {
						Box box;
						if (mode == ImageRenderMode::Stretch) {
							box = Box(0, 0, client.x, client.y);
						} else if (mode == ImageRenderMode::Blit) {
							int w = _background_texture->GetWidth();
							int h = _background_texture->GetHeight();
							box.Left = (client.x - w) / 2;
							box.Top = (client.y - h) / 2;
							box.Right = box.Left + w;
							box.Bottom = box.Top + h;
						} else if (mode == ImageRenderMode::FitKeepAspectRatio) {
							int w = _background_texture->GetWidth();
							int h = _background_texture->GetHeight();
							double sa = double(client.x) / double(client.y);
							double pa = double(w) / double(h);
							if (pa > sa) {
								w = client.x;
								h = w / pa;
							} else {
								h = client.y;
								w = h * pa;
							}
							box.Left = (client.x - w) / 2;
							box.Top = (client.y - h) / 2;
							box.Right = box.Left + w;
							box.Bottom = box.Top + h;
						} else if (mode == ImageRenderMode::CoverKeepAspectRatio) {
							int w = _background_texture->GetWidth();
							int h = _background_texture->GetHeight();
							double sa = double(client.x) / double(client.y);
							double pa = double(w) / double(h);
							if (pa < sa) {
								w = client.x;
								h = w / pa;
							} else {
								h = client.y;
								w = h * pa;
							}
							box.Left = (client.x - w) / 2;
							box.Top = (client.y - h) / 2;
							box.Right = box.Left + w;
							box.Bottom = box.Top + h;
						}
						context_2d->Render(_background_brush, box);
					}
				}
				GetControlSystem(window)->SetRenderingDevice(context_2d);
				GetControlSystem(window)->Render(context_2d);
				context->EndCurrentPass();
				_primary_surface.SetReference(0);
				_layer->Present();
			}
			virtual void WindowClose(IWindow * window) override { _console->Terminate(); _window->Destroy(); GetWindowSystem()->ExitMainLoop(); }
			virtual void WindowActivate(IWindow * window) override { GetWindowSystem()->SubmitTask(CreateFunctionalTask([v = _view]() { v->SetFocus(); })); }
			virtual void WindowDeactivate(IWindow * window) override
			{
				if (_fullscreen) {
					_layer->SwitchToWindow();
					_fullscreen = false;
					#ifdef ENGINE_WINDOWS
					_recreate();
					#endif
					#ifdef ENGINE_MACOSX
					_update_cursor();
					#endif
				}
			}
			virtual void WindowSize(IWindow * window) override
			{
				auto size = window->GetClientSize();
				if (size.x <= 0 || size.y <= 0) return;
				_align();
				_layer->ResizeSurface(size.x, size.y);
			}
			virtual void HandleControlEvent(Windows::IWindow * window, int ID, ControlEvent event, Control * sender) override
			{
				if (ID == 100) {
					_fullscreen = !_fullscreen;
					#ifdef ENGINE_WINDOWS
					_recreate();
					#endif
					#ifdef ENGINE_MACOSX
					if (_fullscreen) _layer->SwitchToFullscreen();
					else _layer->SwitchToWindow();
					_update_cursor();
					#endif
				} else if (ID == 102 && event == ControlEvent::ValueChange) {
					_console->SetBufferOffset(_scroll->Position);
				}
			}
		};
		
		ApplicationCallback _application_callback;

		void WindowSubsystemInitialize(void)
		{
			SafePointer<IScreen> primary = GetDefaultScreen();
			CurrentScaleFactor = primary->GetDpiScale();
			Assembly::CurrentLocale = Assembly::GetCurrentUserLocale();
			SafePointer<Streaming::Stream> com_stream = Assembly::QueryLocalizedResource(L"COM");
			SafePointer<Storage::StringTable> com = new Storage::StringTable(com_stream);
			Assembly::SetLocalizedCommonStrings(com);
			GetWindowSystem()->SetCallback(&_application_callback);
		}
		void CreateConsoleWindow(ConsoleState * state, ConsoleCreateMode & desc, const Box & at)
		{
			GetWindowSystem()->SetApplicationIconVisibility(true);
			SafePointer<IScreen> primary = GetDefaultScreen();
			auto callback = new WindowCallback(state);
			Point min_size;
			callback->AdviseClientRect(Point(30, 1), min_size);
			CreateWindowDesc wd;
			wd.Flags = WindowFlagHasTitle | WindowFlagSizeble | WindowFlagCloseButton | WindowFlagMinimizeButton |
				WindowFlagMaximizeButton | WindowFlagOverrideTheme;
			wd.Title = state->GetTitle();
			wd.MaximalConstraints = Point(0, 0);
			wd.Theme = ThemeClass::Dark;
			wd.Callback = callback;
			wd.Screen = primary;
			wd.ParentWindow = 0;
			#ifdef ENGINE_WINDOWS
			if (!desc.fullscreen) {
			#endif
				auto blur = state->GetBlurBehind();
				wd.Flags |= WindowFlagTransparent;
				if (blur) {
					wd.Flags |= WindowFlagBlurBehind | WindowFlagBlurFactor;
					wd.BlurFactor = blur;
				}
			#ifdef ENGINE_WINDOWS
			}
			#endif
			if (desc.fullscreen) callback->MarkAsFullscreen();
			wd.MinimalConstraints = GetWindowSystem()->ConvertClientToWindow(min_size, wd.Flags);
			wd.Position = at;
			auto window = CreateWindow(wd, DeviceClass::Null);
			if (state->GetWindowIcon()) GetWindowSystem()->SetApplicationIcon(state->GetWindowIcon());
			_application_callback.SetWindowBeingHosted(window);
			window->Show(true);
		}
		void CreateConsoleWindow(ConsoleState * state, ConsoleCreateMode & desc)
		{
			GetWindowSystem()->SetApplicationIconVisibility(true);
			SafePointer<IScreen> primary = GetDefaultScreen();
			auto callback = new WindowCallback(state);
			Point size, min_size;
			callback->AdviseClientRect(Point(desc.size.x, desc.size.y), size);
			callback->AdviseClientRect(Point(30, 1), min_size);
			CreateWindowDesc wd;
			wd.Flags = WindowFlagHasTitle | WindowFlagSizeble | WindowFlagCloseButton | WindowFlagMinimizeButton |
				WindowFlagMaximizeButton | WindowFlagOverrideTheme;
			wd.Title = state->GetTitle();
			wd.MaximalConstraints = Point(0, 0);
			wd.Theme = ThemeClass::Dark;
			wd.Callback = callback;
			wd.Screen = primary;
			wd.ParentWindow = 0;
			#ifdef ENGINE_WINDOWS
			if (!desc.fullscreen) {
			#endif
				auto blur = state->GetBlurBehind();
				wd.Flags |= WindowFlagTransparent;
				if (blur) {
					wd.Flags |= WindowFlagBlurBehind | WindowFlagBlurFactor;
					wd.BlurFactor = blur;
				}
			#ifdef ENGINE_WINDOWS
			}
			#endif
			if (desc.fullscreen) callback->MarkAsFullscreen();
			size = GetWindowSystem()->ConvertClientToWindow(size, wd.Flags);
			wd.MinimalConstraints = GetWindowSystem()->ConvertClientToWindow(min_size, wd.Flags);
			wd.Position = Box(Rectangle(Coordinate(0, 0.0, 0.5), Coordinate(0, 0.0, 0.5), Coordinate(0, 0.0, 0.5), Coordinate(0, 0.0, 0.5)), primary->GetUserRectangle());
			wd.Position.Left -= size.x / 2;
			wd.Position.Top -= size.y / 2;
			wd.Position.Right = wd.Position.Left + size.x;
			wd.Position.Bottom = wd.Position.Top + size.y;
			auto window = CreateWindow(wd, DeviceClass::Null);
			if (state->GetWindowIcon()) GetWindowSystem()->SetApplicationIcon(state->GetWindowIcon());
			_application_callback.SetWindowBeingHosted(window);
			window->Show(true);
		}
		void RunConsoleWindow(void) noexcept { GetWindowSystem()->RunMainLoop(); }
	}
}