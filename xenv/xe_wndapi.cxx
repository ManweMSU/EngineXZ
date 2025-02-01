#include "xe_wndapi.h"

#include "xe_interfaces.h"
#include "xe_filesys.h"
#include "xe_imgapi.h"
#include "../ximg/xi_resources.h"
#include "../xexec/xx_app_activate.h"

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
		struct XPosition
		{
			int x, y;
			XPosition(void) {}
			~XPosition(void) {}
		};
		struct XRectangle
		{
			int left, top, right, bottom;
			XRectangle(void) {}
			~XRectangle(void) {}
		};

		class IVisualTheme : public Object
		{
			SafePointer<Windows::ITheme> _theme;
		public:
			IVisualTheme(Windows::ITheme * theme) { _theme.SetRetain(theme); }
			virtual ~IVisualTheme(void) override {}
			virtual bool IsLight(void) noexcept { return _theme->GetClass() == Windows::ThemeClass::Light; }
			virtual bool IsDark(void) noexcept { return _theme->GetClass() == Windows::ThemeClass::Dark; }
			virtual XColor GetColorA(void) noexcept { return _theme->GetColor(Windows::ThemeColor::Accent).Value; }
			virtual XColor GetColorB(void) noexcept { return _theme->GetColor(Windows::ThemeColor::WindowBackgroup).Value; }
			virtual XColor GetColorC(void) noexcept { return _theme->GetColor(Windows::ThemeColor::WindowText).Value; }
			virtual XColor GetColorD(void) noexcept { return _theme->GetColor(Windows::ThemeColor::SelectedBackground).Value; }
			virtual XColor GetColorE(void) noexcept { return _theme->GetColor(Windows::ThemeColor::SelectedText).Value; }
			virtual XColor GetColorF(void) noexcept { return _theme->GetColor(Windows::ThemeColor::MenuBackground).Value; }
			virtual XColor GetColorG(void) noexcept { return _theme->GetColor(Windows::ThemeColor::MenuText).Value; }
			virtual XColor GetColorH(void) noexcept { return _theme->GetColor(Windows::ThemeColor::MenuHotBackground).Value; }
			virtual XColor GetColorI(void) noexcept { return _theme->GetColor(Windows::ThemeColor::MenuHotText).Value; }
			virtual XColor GetColorJ(void) noexcept { return _theme->GetColor(Windows::ThemeColor::GrayedText).Value; }
			virtual XColor GetColorK(void) noexcept { return _theme->GetColor(Windows::ThemeColor::Hyperlink).Value; }
		};
		class IScreen : public VisualObject
		{
			SafePointer<Windows::IScreen> _screen;
		public:
			IScreen(Windows::IScreen * screen) { _screen.SetRetain(screen); }
			virtual ~IScreen(void) override {}
			virtual string ToString(void) const override { return _screen->GetName(); }
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept override
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				if (cls->GetClassName() == L"objectum") {
					Retain(); return static_cast<Object *>(this);
				} else if (cls->GetClassName() == L"objectum_dynamicum") {
					Retain(); return static_cast<DynamicObject *>(this);
				} else if (cls->GetClassName() == L"graphicum.objectum_visificum") {
					Retain(); return static_cast<VisualObject *>(this);
				} else if (cls->GetClassName() == L"fenestrae.culicare") {
					Retain(); return static_cast<IScreen *>(this);
				} else if (cls->GetClassName() == L"imago.replum") {
					XE_TRY_INTRO
					SafePointer<Codec::Frame> frame = _screen->Capture();
					if (!frame) throw OutOfMemoryException();
					return CreateXFrame(frame);
					XE_TRY_OUTRO(0)
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual void * GetType(void) noexcept override { return 0; }
			virtual void ExposeInterface(uint intid, void * data, ErrorContext & ectx) noexcept override
			{
				if (!data) { ectx.error_code = 3; return; }
				if (intid == VisualObjectInterfaceScreen) {
					*reinterpret_cast<Windows::IScreen **>(data) = _screen.Inner();
					_screen->Retain();
				} else ectx.error_code = 1;
			}
			virtual string GetName(ErrorContext & ectx) noexcept { XE_TRY_INTRO return _screen->GetName(); XE_TRY_OUTRO(L"") }
			virtual XRectangle GetFullRectangle(void) noexcept
			{
				XRectangle result;
				auto data = _screen->GetScreenRectangle();
				result.left = data.Left;
				result.top = data.Top;
				result.right = data.Right;
				result.bottom = data.Bottom;
				return result;
			}
			virtual XRectangle GetUserRectangle(void) noexcept
			{
				XRectangle result;
				auto data = _screen->GetUserRectangle();
				result.left = data.Left;
				result.top = data.Top;
				result.right = data.Right;
				result.bottom = data.Bottom;
				return result;
			}
			virtual XPosition GetResolution(void) noexcept
			{
				XPosition result;
				auto data = _screen->GetResolution();
				result.x = data.x;
				result.y = data.y;
				return result;
			}
			virtual double GetScale(void) noexcept { return _screen->GetDpiScale(); }
			virtual SafePointer<Object> GetContents(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Codec::Frame> frame = _screen->Capture();
				if (!frame) throw OutOfMemoryException();
				return CreateXFrame(frame);
				XE_TRY_OUTRO(0)
			}
			Windows::IScreen * Expose(void) const noexcept { return _screen; }
		};
		class IClipboard : public Object
		{
		public:
			IClipboard(void) {}
			virtual ~IClipboard(void) override {}
			virtual bool CheckFormat(uint format) noexcept
			{
				if (format == 0) return Clipboard::IsFormatAvailable(Clipboard::Format::Text);
				else if (format == 1) return Clipboard::IsFormatAvailable(Clipboard::Format::Image);
				else if (format == 2) return Clipboard::IsFormatAvailable(Clipboard::Format::RichText);
				else if (format == 3) return Clipboard::IsFormatAvailable(Clipboard::Format::Custom);
				else return false;
			}
			virtual bool ReadAsText(string & dest) noexcept { try { return Clipboard::GetData(dest); } catch (...) { return false; } }
			virtual bool ReadAsImage(SafePointer<Object> & dest) noexcept
			{
				try {
					SafePointer<Codec::Frame> frame;
					if (!Clipboard::GetData(frame.InnerRef())) return false;
					dest = CreateXFrame(frame);
					return true;
				} catch (...) { return false; }
			}
			virtual bool ReadAsRichText(string & dest) noexcept { try { return Clipboard::GetData(dest, true); } catch (...) { return false; } }
			virtual bool ReadAsData(const string & format, SafePointer<DataBlock> & dest) noexcept { try { return Clipboard::GetData(format, dest.InnerRef()); } catch (...) { return false; } }
			virtual string GetDataFormat(ErrorContext & ectx) noexcept { XE_TRY_INTRO return Clipboard::GetCustomSubclass(); XE_TRY_OUTRO(L""); }
			virtual bool SetText(const string & text) noexcept { try { return Clipboard::SetData(text); } catch (...) { return false; } }
			virtual bool SetImage(handle image) noexcept
			{
				try {
					SafePointer<Codec::Frame> frame = ExtractFrameFromXFrame(image);
					return Clipboard::SetData(frame);
				} catch (...) { return false; }
			}
			virtual bool SetRichText(const string & regular, const string & rich) noexcept { try { return Clipboard::SetData(regular, rich); } catch (...) { return false; } }
			virtual bool SetData(const string & format, const void * data, int length) noexcept { try { return Clipboard::SetData(format, data, length); } catch (...) { return false; } }
		};

		XPosition WrapWObject(const Point & object)
		{
			XPosition result;
			result.x = object.x;
			result.y = object.y;
			return result;
		}
		XRectangle WrapWObject(const Box & object)
		{
			XRectangle result;
			result.left = object.Left;
			result.top = object.Top;
			result.right = object.Right;
			result.bottom = object.Bottom;
			return result;
		}
		Point WrapWObject(const XPosition & object)
		{
			Point result;
			result.x = object.x;
			result.y = object.y;
			return result;
		}
		Box WrapWObject(const XRectangle & object)
		{
			Box result;
			result.Left = object.left;
			result.Top = object.top;
			result.Right = object.right;
			result.Bottom = object.bottom;
			return result;
		}
		uint WrapWObject(Windows::WindowHandler hdlr)
		{
			if (hdlr == Windows::WindowHandler::Save) return 0;
			else if (hdlr == Windows::WindowHandler::SaveAs) return 1;
			else if (hdlr == Windows::WindowHandler::Export) return 2;
			else if (hdlr == Windows::WindowHandler::Print) return 3;
			else if (hdlr == Windows::WindowHandler::Undo) return 4;
			else if (hdlr == Windows::WindowHandler::Redo) return 5;
			else if (hdlr == Windows::WindowHandler::Cut) return 6;
			else if (hdlr == Windows::WindowHandler::Copy) return 7;
			else if (hdlr == Windows::WindowHandler::Paste) return 8;
			else if (hdlr == Windows::WindowHandler::Duplicate) return 9;
			else if (hdlr == Windows::WindowHandler::Delete) return 10;
			else if (hdlr == Windows::WindowHandler::Find) return 11;
			else if (hdlr == Windows::WindowHandler::Replace) return 12;
			else if (hdlr == Windows::WindowHandler::SelectAll) return 13;
			else return -1;
		}
		uint WrapWObject(Windows::ApplicationHandler hdlr)
		{
			if (hdlr == Windows::ApplicationHandler::CreateFile) return 0;
			else if (hdlr == Windows::ApplicationHandler::OpenSomeFile) return 1;
			else if (hdlr == Windows::ApplicationHandler::OpenExactFile) return 2;
			else if (hdlr == Windows::ApplicationHandler::ShowHelp) return 3;
			else if (hdlr == Windows::ApplicationHandler::ShowAbout) return 4;
			else if (hdlr == Windows::ApplicationHandler::ShowProperties) return 5;
			else if (hdlr == Windows::ApplicationHandler::Terminate) return 6;
			else return -1;
		}
		uint WrapWObject(Windows::IPCStatus status)
		{
			if (status == Windows::IPCStatus::Unknown) return 0;
			else if (status == Windows::IPCStatus::Accepted) return 1;
			else if (status == Windows::IPCStatus::Discarded) return 2;
			else if (status == Windows::IPCStatus::ServerClosed) return 3;
			else if (status == Windows::IPCStatus::InternalError) return 4;
			else return -1;
		}

		struct WindowDesc;
		class IWindow;
		class IWindowSystem;
		class IWindowCallback
		{
		public:
			virtual void WindowCreated(IWindow * window, ErrorContext & ectx) noexcept = 0;
			virtual void WindowDestroyed(IWindow * window, ErrorContext & ectx) noexcept = 0;
			virtual void WindowVisibilityChanged(IWindow * window, bool visible, ErrorContext & ectx) noexcept = 0;
			virtual void RenderRequest(IWindow * window, ErrorContext & ectx) noexcept = 0;
			virtual void WindowClosed(IWindow * window, ErrorContext & ectx) noexcept = 0;
			virtual void WindowMinimized(IWindow * window, ErrorContext & ectx) noexcept = 0;
			virtual void WindowMaximized(IWindow * window, ErrorContext & ectx) noexcept = 0;
			virtual void WindowRestored(IWindow * window, ErrorContext & ectx) noexcept = 0;
			virtual void WindowHelpRequested(IWindow * window, ErrorContext & ectx) noexcept = 0;
			virtual void WindowActivated(IWindow * window, ErrorContext & ectx) noexcept = 0;
			virtual void WindowDeactivated(IWindow * window, ErrorContext & ectx) noexcept = 0;
			virtual void WindowMoved(IWindow * window, ErrorContext & ectx) noexcept = 0;
			virtual void WindowResized(IWindow * window, ErrorContext & ectx) noexcept = 0;
			virtual void WindowFocusChanged(IWindow * window, bool set, ErrorContext & ectx) noexcept = 0;
			virtual bool WindowKeyDown(IWindow * window, int code, ErrorContext & ectx) noexcept = 0;
			virtual void WindowKeyUp(IWindow * window, int code, ErrorContext & ectx) noexcept = 0;
			virtual void WindowCharDown(IWindow * window, uint chr, ErrorContext & ectx) noexcept = 0;
			virtual void WindowCaptureChanged(IWindow * window, bool set, ErrorContext & ectx) noexcept = 0;
			virtual void WindowMouseMoved(IWindow * window, const XPosition & pos, ErrorContext & ectx) noexcept = 0;
			virtual void WindowLeftButtonDown(IWindow * window, const XPosition & pos, ErrorContext & ectx) noexcept = 0;
			virtual void WindowLeftButtonUp(IWindow * window, const XPosition & pos, ErrorContext & ectx) noexcept = 0;
			virtual void WindowLeftButtonDouble(IWindow * window, const XPosition & pos, ErrorContext & ectx) noexcept = 0;
			virtual void WindowRightButtonDown(IWindow * window, const XPosition & pos, ErrorContext & ectx) noexcept = 0;
			virtual void WindowRightButtonUp(IWindow * window, const XPosition & pos, ErrorContext & ectx) noexcept = 0;
			virtual void WindowRightButtonDouble(IWindow * window, const XPosition & pos, ErrorContext & ectx) noexcept = 0;
			virtual void WindowScrollVertically(IWindow * window, double value, ErrorContext & ectx) noexcept = 0;
			virtual void WindowScrollHorizontally(IWindow * window, double value, ErrorContext & ectx) noexcept = 0;
			virtual void WindowTimerEvent(IWindow * window, uint id, ErrorContext & ectx) noexcept = 0;
			virtual void WindowThemeChanged(IWindow * window, ErrorContext & ectx) noexcept = 0;
			virtual bool WindowEventAvailable(IWindow * window, uint event, ErrorContext & ectx) noexcept = 0;
			virtual void WindowHandleEvent(IWindow * window, uint event, ErrorContext & ectx) noexcept = 0;
		};
		class IWindow : public VisualObject
		{
		public:
			IWindowCallback * _callback;
			Windows::IWindow * _window;
			SafePointer<Windows::ICursor> _cursor;
			SafePointer<DynamicObject> _context;
			Array<IWindow *> _children;
			IWindow * _parent;
			IWindowSystem * _system;
			bool _top_level, _cursor_reset, _modal;
		public:
			IWindow(void) : _children(0x10) {}
			virtual ~IWindow(void) override {}
			virtual string ToString(void) const override { return _window->GetText(); }
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept override
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				if (cls->GetClassName() == L"objectum") {
					Retain(); return static_cast<Object *>(this);
				} else if (cls->GetClassName() == L"objectum_dynamicum") {
					Retain(); return static_cast<DynamicObject *>(this);
				} else if (cls->GetClassName() == L"graphicum.objectum_visificum") {
					Retain(); return static_cast<VisualObject *>(this);
				} else if (cls->GetClassName() == L"fenestrae.fenestra") {
					Retain(); return static_cast<IWindow *>(this);
				} else if (cls->GetClassName() == L"fenestrae.contextus_fenestrae" || cls->GetClassName() == L"graphicum.contextus_machinae") {
					XE_TRY_INTRO
					if (_context) return _context->DynamicCast(cls, ectx); else throw InvalidStateException();
					XE_TRY_OUTRO(0)
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual void * GetType(void) noexcept override { return 0; }
			virtual void ExposeInterface(uint intid, void * data, ErrorContext & ectx) noexcept override
			{
				if (!data) { ectx.error_code = 3; return; }
				if (intid == VisualObjectInterfaceWindow) *reinterpret_cast<Windows::IWindow **>(data) = _window;
				else ectx.error_code = 1;
			}
			virtual bool IsVisible(void) noexcept { return _window->IsVisible(); }
			virtual void Show(const bool & show) noexcept { _window->Show(show); }
			virtual string GetTitle(ErrorContext & ectx) noexcept { XE_TRY_INTRO return _window->GetText(); XE_TRY_OUTRO(L""); }
			virtual void SetTitle(const string & title, ErrorContext & ectx) noexcept { XE_TRY_INTRO _window->SetText(title); XE_TRY_OUTRO() }
			virtual XRectangle GetPosition(void) noexcept { return WrapWObject(_window->GetPosition()); }
			virtual void SetPosition(const XRectangle & rect) noexcept { _window->SetPosition(WrapWObject(rect)); }
			virtual XPosition GetClientArea(void) noexcept { return WrapWObject(_window->GetClientSize()); }
			virtual XPosition GetMinimalSize(void) noexcept { return WrapWObject(_window->GetMinimalConstraints()); }
			virtual void SetMinimalSize(const XPosition & size) noexcept { _window->SetMinimalConstraints(WrapWObject(size)); }
			virtual XPosition GetMaximalSize(void) noexcept { return WrapWObject(_window->GetMaximalConstraints()); }
			virtual void SetMaximalSize(const XPosition & size) noexcept { _window->SetMaximalConstraints(WrapWObject(size)); }
			virtual bool IsActive(void) noexcept { return _window->IsActive(); }
			virtual bool IsMinimized(void) noexcept { return _window->IsMinimized(); }
			virtual bool IsMaximized(void) noexcept { return _window->IsMaximized(); }
			virtual void Activate(void) noexcept { _window->Activate(); }
			virtual void Minimize(void) noexcept { _window->Minimize(); }
			virtual void Maximize(void) noexcept { _window->Maximize(); }
			virtual void Normalize(void) noexcept { _window->Restore(); }
			virtual void RequestAttention(void) noexcept { _window->RequireAttention(); }
			virtual void SetOpacity(const double & value) noexcept { _window->SetOpacity(value); }
			virtual void SetCloseButton(const uint & value) noexcept
			{
				if (value == 0) _window->SetCloseButtonState(Windows::CloseButtonState::Disabled);
				else if (value == 1) _window->SetCloseButtonState(Windows::CloseButtonState::Enabled);
				else if (value == 2) _window->SetCloseButtonState(Windows::CloseButtonState::Alert);
			}
			virtual IWindow * GetParentWindow(void) noexcept { return _parent; }
			virtual int GetChildrenCount(void) noexcept { return _children.Length(); }
			virtual IWindow * GetChildWindow(int index) noexcept { if (index >= 0 && index < _children.Length()) return _children[index]; else return 0; }
			virtual void SetProgressMode(const uint & mode) noexcept
			{
				if (mode == 0) _window->SetProgressMode(Windows::ProgressDisplayMode::Hide);
				else if (mode == 1) _window->SetProgressMode(Windows::ProgressDisplayMode::Normal);
				else if (mode == 2) _window->SetProgressMode(Windows::ProgressDisplayMode::Paused);
				else if (mode == 3) _window->SetProgressMode(Windows::ProgressDisplayMode::Error);
				else if (mode == 4) _window->SetProgressMode(Windows::ProgressDisplayMode::Indeterminated);
			}
			virtual void SetProgressValue(const double & value) noexcept { _window->SetProgressValue(value); }
			virtual void SetBackgroundMaterial(const uint & material) noexcept
			{
				if (material == 0) _window->SetCocoaEffectMaterial(Windows::CocoaEffectMaterial::Titlebar);
				else if (material == 1) _window->SetCocoaEffectMaterial(Windows::CocoaEffectMaterial::Selection);
				else if (material == 2) _window->SetCocoaEffectMaterial(Windows::CocoaEffectMaterial::Menu);
				else if (material == 3) _window->SetCocoaEffectMaterial(Windows::CocoaEffectMaterial::Popover);
				else if (material == 4) _window->SetCocoaEffectMaterial(Windows::CocoaEffectMaterial::Sidebar);
				else if (material == 5) _window->SetCocoaEffectMaterial(Windows::CocoaEffectMaterial::HeaderView);
				else if (material == 6) _window->SetCocoaEffectMaterial(Windows::CocoaEffectMaterial::Sheet);
				else if (material == 7) _window->SetCocoaEffectMaterial(Windows::CocoaEffectMaterial::WindowBackground);
				else if (material == 8) _window->SetCocoaEffectMaterial(Windows::CocoaEffectMaterial::HUD);
				else if (material == 9) _window->SetCocoaEffectMaterial(Windows::CocoaEffectMaterial::FullScreenUI);
				else if (material == 10) _window->SetCocoaEffectMaterial(Windows::CocoaEffectMaterial::ToolTip);
			}
			virtual bool HitTest(const XPosition & pos) noexcept { return _window->PointHitTest(WrapWObject(pos)); }
			virtual XPosition ConvertClientToGlobal(const XPosition & pos) noexcept { return WrapWObject(_window->PointClientToGlobal(WrapWObject(pos))); }
			virtual XPosition ConvertGlobalToClient(const XPosition & pos) noexcept { return WrapWObject(_window->PointGlobalToClient(WrapWObject(pos))); }
			virtual SafePointer<Windows::ICursor> GetCursor(void) noexcept { return _cursor; }
			virtual void SetCursor(const SafePointer<Windows::ICursor> & cursor) noexcept { if (cursor) { _cursor = cursor; _cursor_reset = true; } }
			virtual bool IsFocused(void) noexcept { return _window->IsFocused(); }
			virtual bool CapturesMouse(void) noexcept { return _window->IsCaptured(); }
			virtual void SetCapture(const bool & set) noexcept { if (set) _window->SetCapture(); else _window->ReleaseCapture(); }
			virtual void SetFocus(void) noexcept { _window->SetFocus(); }
			virtual void SetTimer(uint id, uint period) noexcept { _window->SetTimer(id, period); }
			virtual void CreateBitmapBackbufferA(handle image) noexcept { CreateBitmapBackbufferC(image, 0, 0xFF000000); }
			virtual void CreateBitmapBackbufferB(handle image, uint mode) noexcept { CreateBitmapBackbufferC(image, mode, 0xFF000000); }
			virtual void CreateBitmapBackbufferC(handle image, uint mode, const XColor & color) noexcept
			{
				if (!image) return;
				SafePointer<Codec::Frame> frame = ExtractFrameFromXFrame(image);
				if (mode == 0) _window->SetBackbufferedRenderingDevice(frame, Windows::ImageRenderMode::Stretch, color.value);
				else if (mode == 1) _window->SetBackbufferedRenderingDevice(frame, Windows::ImageRenderMode::Blit, color.value);
				else if (mode == 2) _window->SetBackbufferedRenderingDevice(frame, Windows::ImageRenderMode::FitKeepAspectRatio, color.value);
				else if (mode == 3) _window->SetBackbufferedRenderingDevice(frame, Windows::ImageRenderMode::CoverKeepAspectRatio, color.value);
			}
			virtual SafePointer<DynamicObject> CreateBackbufferA(void) noexcept { return CreateBackbufferB(0); }
			virtual SafePointer<DynamicObject> CreateBackbufferB(uint devcls) noexcept
			{
				try {
					_context.SetReference(0);
					Windows::I2DPresentationEngine * pres = 0;
					if (devcls == 0) pres = _window->Set2DRenderingDevice(Windows::DeviceClass::DontCare);
					else if (devcls == 1) pres = _window->Set2DRenderingDevice(Windows::DeviceClass::Hardware);
					else if (devcls == 2) pres = _window->Set2DRenderingDevice(Windows::DeviceClass::Basic);
					if (!pres) return 0;
					_context = CreateWindowContext(pres);
					return _context;
				} catch (...) { return 0; }
			}
			virtual void Invalidate(void) noexcept { _window->InvalidateContents(); }
			virtual double GetWindowScale(void) noexcept { return _window->GetDpiScale(); }
			virtual SafePointer<IScreen> GetWindowScreen(void) noexcept
			{
				try {
					SafePointer<Windows::IScreen> screen = _window->GetCurrentScreen();
					return new IScreen(screen);
				} catch (...) { return 0; }
			}
			virtual SafePointer<IVisualTheme> GetWindowTheme(void) noexcept
			{
				try {
					SafePointer<Windows::ITheme> theme = _window->GetCurrentTheme();
					return new IVisualTheme(theme);
				} catch (...) { return 0; }
			}
			virtual uint GetWindowBackgroundFlags(void) noexcept { return _window->GetBackgroundFlags(); }
			virtual bool IsModal(void) noexcept { return _modal; }
			virtual void Destroy(void) noexcept { _window->Destroy(); }
			Windows::IWindow * Expose(void) const noexcept { return _window; }
		};
		class Window : public IWindow, Windows::IWindowCallback
		{
		public:
			Window(const WindowDesc & desc, IWindowSystem * system, bool modal);
			virtual ~Window(void) override;
			virtual void Created(Windows::IWindow * window) override;
			virtual void Destroyed(Windows::IWindow * window) override;
			virtual void Shown(Windows::IWindow * window, bool show) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->WindowVisibilityChanged(this, show, ectx);
			}
			virtual void RenderWindow(Windows::IWindow * window) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->RenderRequest(this, ectx);
			}
			virtual void WindowClose(Windows::IWindow * window) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->WindowClosed(this, ectx);
			}
			virtual void WindowMaximize(Windows::IWindow * window) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->WindowMaximized(this, ectx);
			}
			virtual void WindowMinimize(Windows::IWindow * window) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->WindowMinimized(this, ectx);
			}
			virtual void WindowRestore(Windows::IWindow * window) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->WindowRestored(this, ectx);
			}
			virtual void WindowHelp(Windows::IWindow * window) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->WindowHelpRequested(this, ectx);
			}
			virtual void WindowActivate(Windows::IWindow * window) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->WindowActivated(this, ectx);
			}
			virtual void WindowDeactivate(Windows::IWindow * window) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->WindowDeactivated(this, ectx);
			}
			virtual void WindowMove(Windows::IWindow * window) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->WindowMoved(this, ectx);
			}
			virtual void WindowSize(Windows::IWindow * window) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->WindowResized(this, ectx);
			}
			virtual void FocusChanged(Windows::IWindow * window, bool got) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->WindowFocusChanged(this, got, ectx);
			}
			virtual bool KeyDown(Windows::IWindow * window, int key_code) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) return _callback->WindowKeyDown(this, key_code, ectx);
				return false;
			}
			virtual void KeyUp(Windows::IWindow * window, int key_code) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->WindowKeyUp(this, key_code, ectx);
			}
			virtual void CharDown(Windows::IWindow * window, uint32 ucs_code) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->WindowCharDown(this, ucs_code, ectx);
			}
			virtual void CaptureChanged(Windows::IWindow * window, bool got) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->WindowCaptureChanged(this, got, ectx);
			}
			virtual void SetCursor(Windows::IWindow * window, Point at) override;
			virtual void MouseMove(Windows::IWindow * window, Point at) override;
			virtual void LeftButtonDown(Windows::IWindow * window, Point at) override;
			virtual void LeftButtonUp(Windows::IWindow * window, Point at) override;
			virtual void LeftButtonDoubleClick(Windows::IWindow * window, Point at) override;
			virtual void RightButtonDown(Windows::IWindow * window, Point at) override;
			virtual void RightButtonUp(Windows::IWindow * window, Point at) override;
			virtual void RightButtonDoubleClick(Windows::IWindow * window, Point at) override;
			virtual void ScrollVertically(Windows::IWindow * window, double delta) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->WindowScrollVertically(this, delta, ectx);
			}
			virtual void ScrollHorizontally(Windows::IWindow * window, double delta) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->WindowScrollHorizontally(this, delta, ectx);
			}
			virtual void Timer(Windows::IWindow * window, int timer_id) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->WindowTimerEvent(this, timer_id, ectx);
			}
			virtual void ThemeChanged(Windows::IWindow * window) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->WindowThemeChanged(this, ectx);
			}
			virtual bool IsWindowEventEnabled(Windows::IWindow * window, Windows::WindowHandler handler) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) {
					auto result = _callback->WindowEventAvailable(this, WrapWObject(handler), ectx);
					if (ectx.error_code) return false;
					return result;
				} else return false;
			}
			virtual void HandleWindowEvent(Windows::IWindow * window, Windows::WindowHandler handler) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->WindowHandleEvent(this, WrapWObject(handler), ectx);
			}
		};
		struct WindowDesc
		{
			XRectangle position;
			XPosition minimal_size;
			XPosition maximal_size;
			XRectangle margins;
			double opacity;
			double blur_factor;
			uint flags;
			XColor background;
			string title;
			IWindowCallback * callback;
			IWindow * parent;
			SafePointer<IScreen> screen;
			bool dark_theme;
		};

		class IMenu;
		class IMenuItem;
		class IMenuItemCallback
		{
		public:
			virtual XPosition MenuItemMeasure(IMenuItem * item, DynamicObject * ctx, ErrorContext & ectx) noexcept = 0;
			virtual void MenuItemRender(IMenuItem * item, DynamicObject * ctx, const XRectangle & rect, bool selected, ErrorContext & ectx) noexcept = 0;
			virtual void MenuItemClosed(IMenuItem * item, ErrorContext & ectx) noexcept = 0;
			virtual void MenuItemDestroyed(IMenuItem * item, ErrorContext & ectx) noexcept = 0;
		};
		class IMenuItem : public Object
		{
		protected:
			IMenuItemCallback * _callback;
			SafePointer<Windows::IMenuItem> _item;
			SafePointer<IMenu> _submenu;
		public:
			IMenuItem(Windows::IWindowSystem & system) : _callback(0) { _item = system.CreateMenuItem(); if (!_item) throw OutOfMemoryException(); }
			virtual ~IMenuItem(void) override {}
			virtual IMenuItemCallback * GetCallback(void) noexcept { return _callback; }
			virtual void SetCallback(IMenuItemCallback ** callback) noexcept = 0;
			virtual SafePointer<IMenu> GetSubmenu(void) noexcept { return _submenu; }
			virtual void SetSubmenu(const SafePointer<IMenu> & menu) noexcept = 0;
			virtual handle GetUserData(void) noexcept { return _item->GetUserData(); }
			virtual void SetUserData(const handle & data) noexcept { _item->SetUserData(data); }
			virtual int GetID(void) noexcept { return _item->GetID(); }
			virtual void SetID(const int & id) noexcept { _item->SetID(id); }
			virtual string GetText(ErrorContext & ectx) noexcept { XE_TRY_INTRO return _item->GetText(); XE_TRY_OUTRO(L"") }
			virtual void SetText(const string & text, ErrorContext & ectx) noexcept { XE_TRY_INTRO _item->SetText(text); XE_TRY_OUTRO() }
			virtual string GetRightText(ErrorContext & ectx) noexcept { XE_TRY_INTRO return _item->GetSideText(); XE_TRY_OUTRO(L"") }
			virtual void SetRightText(const string & text, ErrorContext & ectx) noexcept { XE_TRY_INTRO _item->SetSideText(text); XE_TRY_OUTRO() }
			virtual bool GetIsSeparator(void) noexcept { return _item->IsSeparator(); }
			virtual void SetIsSeparator(const bool & value) noexcept { _item->SetIsSeparator(value); }
			virtual bool GetIsEnabled(void) noexcept { return _item->IsEnabled(); }
			virtual void SetIsEnabled(const bool & value) noexcept { _item->Enable(value); }
			virtual bool GetIsChecked(void) noexcept { return _item->IsChecked(); }
			virtual void SetIsChecked(const bool & value) noexcept { _item->Check(value); }
			Windows::IMenuItem * Expose(void) noexcept { return _item; }
		};
		class IMenu : public Object
		{
			SafePointer<Windows::IMenu> _menu;
			ObjectArray<IMenuItem> _items;
		public:
			IMenu(Windows::IWindowSystem & system) : _items(0x10) { _menu = system.CreateMenu(); if (!_menu) throw OutOfMemoryException(); }
			virtual ~IMenu(void) override {}
			virtual void AddItem(IMenuItem * item, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_items.Append(item);
				_menu->AppendMenuItem(item->Expose());
				XE_TRY_OUTRO()
			}
			virtual void InsertItem(IMenuItem * item, int at, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (at < 0 || at > _items.Length()) throw InvalidArgumentException();
				_items.Insert(item, at);
				_menu->InsertMenuItem(item->Expose(), at);
				XE_TRY_OUTRO()
			}
			virtual void RemoveItem(int at) noexcept
			{
				if (at < 0 || at >= _items.Length()) return;
				_items.Remove(at);
				_menu->RemoveMenuItem(at);
			}
			virtual int GetLength(void) noexcept { return _items.Length(); }
			virtual SafePointer<IMenuItem> GetItem(int at) noexcept
			{
				if (at < 0 || at >= _items.Length()) return 0;
				SafePointer<IMenuItem> result;
				result.SetRetain(_items.ElementAt(at));
				return result;
			}
			virtual SafePointer<IMenuItem> FindItem(int id) noexcept
			{
				for (auto & i : _items) {
					SafePointer<IMenuItem> result;
					if (i.GetID() == id) { result.SetRetain(&i); return result; }
					auto submenu = i.GetSubmenu();
					if (submenu) { result = submenu->FindItem(id); if (result) return result; }
				}
				return 0;
			}
			virtual int Execute(IWindow * window, const XPosition & pos) noexcept { return _menu->Run(window->Expose(), WrapWObject(pos)); }
			Windows::IMenu * Expose(void) noexcept { return _menu; }
		};
		class MenuItem : public IMenuItem, public Windows::IMenuItemCallback
		{
			SafePointer<DynamicObject> _context;
		public:
			MenuItem(Windows::IWindowSystem & system) : IMenuItem(system) {}
			virtual ~MenuItem(void) override
			{
				if (_callback) {
					ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
					_item->SetCallback(0);
					_callback->MenuItemDestroyed(this, ectx);
				}
			}
			virtual void SetCallback(XE::IMenuItemCallback ** callback) noexcept override
			{
				_callback = *callback;
				if (_callback) _item->SetCallback(this); else _item->SetCallback(0);
			}
			virtual void SetSubmenu(const SafePointer<IMenu> & menu) noexcept override { _submenu = menu; _item->SetSubmenu(_submenu ? _submenu->Expose() : 0); }
			virtual Point MeasureMenuItem(Windows::IMenuItem * item, Graphics::I2DDeviceContext * device) override
			{
				try {
					if (!_context) _context = WrapContext(device);
					if (_callback) {
						ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
						auto result = WrapWObject(_callback->MenuItemMeasure(this, _context, ectx));
						if (ectx.error_code) return Point(0, 0);
						return result;
					} else return Point(0, 0);
				} catch (...) { return Point(0, 0); }
			}
			virtual void RenderMenuItem(Windows::IMenuItem * item, Graphics::I2DDeviceContext * device, const Box & at, bool hot_state) override
			{
				try {
					if (!_context) _context = WrapContext(device);
					if (_callback) {
						ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
						_callback->MenuItemRender(this, _context, WrapWObject(at), hot_state, ectx);
					}
				} catch (...) {}
			}
			virtual void MenuClosed(Windows::IMenuItem * item) override
			{
				if (_callback) {
					ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
					_callback->MenuItemClosed(this, ectx);
				}
				_context.SetReference(0);
			}
			virtual void MenuItemDisposed(Windows::IMenuItem * item) override {}
		};

		class IStatusIcon;
		class IStatusIconCallback
		{
		public:
			virtual void StatusIconHandleEvent(IStatusIcon * icon, int id, ErrorContext & ectx) noexcept = 0;
		};
		class IStatusIcon : public Object
		{
		protected:
			IStatusIconCallback * _callback;
			SafePointer<Windows::IStatusBarIcon> _icon;
			SafePointer<Object> _image;
			SafePointer<IMenu> _menu;
		public:
			IStatusIcon(Windows::IWindowSystem & system) : _callback(0) { _icon = system.CreateStatusBarIcon(); if (!_icon) throw OutOfMemoryException(); }
			virtual ~IStatusIcon(void) override {}
			virtual IStatusIconCallback * GetCallback(void) noexcept { return _callback; }
			virtual void SetCallback(IStatusIconCallback ** callback) noexcept { _callback = *callback; }
			virtual XPosition GetIconSize(void) noexcept { return WrapWObject(_icon->GetIconSize()); }
			virtual SafePointer<Object> GetIcon(ErrorContext & ectx) noexcept { XE_TRY_INTRO return _image; XE_TRY_OUTRO(0) }
			virtual void SetIcon(const SafePointer<Object> & ximage, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (ximage) {
					SafePointer<Codec::Image> image = ExtractImageFromXImage(ximage.Inner());
					_icon->SetIcon(image);
					_image = ximage;
				} else { _image.SetReference(0); _icon->SetIcon(0); }
				XE_TRY_OUTRO()
			}
			virtual uint GetStyle(void) noexcept
			{
				if (_icon->GetIconColorUsage() == Windows::StatusBarIconColorUsage::Monochromic) return 1;
				else return 0;
			}
			virtual void SetStyle(const uint & style) noexcept
			{
				if (style == 1) _icon->SetIconColorUsage(Windows::StatusBarIconColorUsage::Monochromic);
				else _icon->SetIconColorUsage(Windows::StatusBarIconColorUsage::Colourfull);
			}
			virtual string GetText(ErrorContext & ectx) noexcept { XE_TRY_INTRO return _icon->GetTooltip(); XE_TRY_OUTRO(0) }
			virtual void SetText(const string & text, ErrorContext & ectx) noexcept { XE_TRY_INTRO _icon->SetTooltip(text); XE_TRY_OUTRO() }
			virtual int GetID(void) noexcept { return _icon->GetEventID(); }
			virtual void SetID(const int & id) noexcept { _icon->SetEventID(id); if (id) { _menu.SetReference(0); _icon->SetMenu(0); } }
			virtual SafePointer<IMenu> GetMenu(void) noexcept { return _menu; }
			virtual void SetMenu(const SafePointer<IMenu> & menu) noexcept { _menu = menu; if (_menu) { _icon->SetEventID(0); _icon->SetMenu(_menu->Expose()); } else _icon->SetMenu(0); }
			virtual bool GetIsVisible(void) noexcept { return _icon->IsVisible(); }
			virtual void SetIsVisible(const bool & visible, ErrorContext & ectx) noexcept { XE_TRY_INTRO if (!_icon->PresentIcon(visible)) throw InvalidStateException(); XE_TRY_OUTRO() }
		};
		class StatusIcon : public IStatusIcon, public Windows::IStatusCallback
		{
		public:
			StatusIcon(Windows::IWindowSystem & system) : IStatusIcon(system) { _icon->SetCallback(this); }
			virtual ~StatusIcon(void) override {}
			virtual void StatusIconCommand(Windows::IStatusBarIcon * icon, int id) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->StatusIconHandleEvent(this, id, ectx);
			}
		};

		class ICommunication : public Object
		{
			Windows::IWindowSystem * _system;
			SafePointer<Windows::IIPCClient> _client;
		public:
			ICommunication(Windows::IWindowSystem & system, const string & app, const string & auth) : _system(&system)
			{
				_client = system.CreateIPCClient(app, auth);
				if (!_client) throw IO::FileAccessException(IO::Error::FileNotFound);
			}
			virtual ~ICommunication(void) override {}
			virtual void Send(const string & verb, const DataBlock * data, uint * status, IDispatchTask * task, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<IDispatchTask> hdlr;
				hdlr.SetRetain(task);
				auto tint = CreateStructuredTask<Windows::IPCStatus>([sys = _system, hdlr, status](Windows::IPCStatus resp) {
					if (status) *status = WrapWObject(resp);
					if (hdlr) hdlr->DoTask(sys);
				});
				if (!_client->SendData(verb, data, tint, &tint->Value1)) throw InvalidStateException();
				XE_TRY_OUTRO()
			}
			virtual void Receive(const string & verb, uint * status, SafePointer<DataBlock> * data, IDispatchTask * task, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<IDispatchTask> hdlr;
				hdlr.SetRetain(task);
				auto tint = CreateStructuredTask< SafePointer<DataBlock>, Windows::IPCStatus>([sys = _system, hdlr, status, data](const SafePointer<DataBlock> & block, Windows::IPCStatus resp) {
					if (status) *status = WrapWObject(resp);
					if (data) *data = block;
					if (hdlr) hdlr->DoTask(sys);
				});
				if (!_client->RequestData(verb, tint, &tint->Value2, tint->Value1.InnerRef())) throw InvalidStateException();
				XE_TRY_OUTRO()
			}
			virtual uint GetStatus(void) noexcept { return WrapWObject(_client->GetStatus()); }
		};

		class IWindowSystemCallback
		{
		public:
			virtual bool ApplicationIsEventAvailable(uint event, ErrorContext & ectx) noexcept = 0;
			virtual bool ApplicationIsWindowEventAvailable(uint event, ErrorContext & ectx) noexcept = 0;
			virtual bool ApplicationHandleEvent(uint event, const string & arg, ErrorContext & ectx) noexcept = 0;
			virtual void ApplicationHandleGlobalKeyboard(uint id, ErrorContext & ectx) noexcept = 0;
			virtual void ApplicationDataReceive(handle client, const string & verb, const DataBlock * data, ErrorContext & ectx) noexcept = 0;
			virtual SafePointer<DataBlock> ApplicationDataRespond(handle client, const string & verb, ErrorContext & ectx) noexcept = 0;
			virtual void ApplicationDataDisconnect(handle client, ErrorContext & ectx) noexcept = 0;
		};
		class IWindowSystem : public XDispatchContext
		{
		protected:
			struct _file_format_desc { string desc, exts; };
			struct _open_file_desc
			{
				string title;
				bool multiple;
				int default_format;
				Array<_file_format_desc> formats;
			};
			struct _open_file_result { Array<string> files; };
			struct _save_file_desc
			{
				string title;
				bool append_extension;
				int default_format;
				Array<_file_format_desc> formats;
			};
			struct _save_file_result { string file; int format; };
		protected:
			const XE::Module * _main;
			IWindowSystemCallback * _callback;
			Windows::IWindowSystem * _system;
			Volumes::Set<IWindow *> _windows;
			uint _dock_tile_mode, _dock_tile_state;
			bool _exit_on_last_window;
		protected:
			void _update_icon(void) noexcept
			{
				try {
					Codec::Image icon;
					SafePointer< Array<Point> > sizes = _system->GetApplicationIconSizes();
					for (auto & s : *sizes) {
						SafePointer<Codec::Frame> frame = XI::LoadModuleIcon(_main->GetResources(), 1, s);
						if (frame) icon.Frames.Append(frame);
					}
					if (icon.Frames.Length()) _system->SetApplicationIcon(&icon);
				} catch (...) {}
			}
			void _update_tile(void) noexcept
			{
				#ifdef ENGINE_MACOSX
				auto new_state = _dock_tile_mode;
				if (new_state == 2) new_state = _windows.IsEmpty() ? 0 : 1;
				if (new_state != _dock_tile_state) {
					_dock_tile_state = new_state;
					_system->SetApplicationIconVisibility(_dock_tile_state ? 1 : 0);
					if (_dock_tile_state) _update_icon();
				}
				#endif
			}
		public:
			IWindowSystem(const Array<string> * files) : XDispatchContext(Windows::GetWindowSystem()), _system(Windows::GetWindowSystem()), _exit_on_last_window(false)
			{
				XX::EnforceApplicationActivation();
				if (files && files->Length()) _system->SetFilesToOpen(files->GetBuffer() + 1, files->Length() - 1);
			}
			virtual SafePointer<IVisualTheme> GetSystemTheme(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Windows::ITheme> theme = Windows::GetCurrentTheme();
				return new IVisualTheme(theme);
				XE_TRY_OUTRO(0);
			}
			virtual SafePointer<IScreen> GetDefaultScreen(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Windows::IScreen> screen = Windows::GetDefaultScreen();
				return new IScreen(screen);
				XE_TRY_OUTRO(0);
			}
			virtual SafePointer< ObjectArray<IScreen> > GetScreens(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< ObjectArray<Windows::IScreen> > screens = Windows::GetActiveScreens();
				SafePointer< ObjectArray<IScreen> > result = new ObjectArray<IScreen>(screens->Length());
				for (auto & s : *screens) {
					SafePointer<IScreen> screen = new IScreen(&s);
					result->Append(screen);
				}
				return result;
				XE_TRY_OUTRO(0);
			}
			virtual SafePointer<IClipboard> GetClipboard(ErrorContext & ectx) noexcept { XE_TRY_INTRO return new IClipboard; XE_TRY_OUTRO(0) }
			virtual IWindow * CreateWindow(const WindowDesc & desc, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return new Window(desc, this, false);
				XE_TRY_OUTRO(0);
			}
			virtual IWindow * CreateModalWindow(const WindowDesc & desc, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return new Window(desc, this, true);
				XE_TRY_OUTRO(0);
			}
			virtual XPosition ConvertWindowSize(const XPosition & size, uint flags) noexcept { return WrapWObject(_system->ConvertClientToWindow(WrapWObject(size), flags)); }
			virtual XRectangle ConvertWindowPosition(const XRectangle & pos, uint flags) noexcept { return WrapWObject(_system->ConvertClientToWindow(WrapWObject(pos), flags)); }
			virtual SafePointer< Array<IWindow *> > GetMainWindows(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< Array<IWindow *> > result = new Array<IWindow *>(0x20);
				for (auto & w : _windows) result->Append(w);
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual void Run(void) noexcept
			{
				auto prev = _exit_on_last_window;
				_exit_on_last_window = false;
				_system->RunMainLoop();
				_exit_on_last_window = prev;
			}
			virtual void RunWhileWindows(bool control) noexcept
			{
				auto prev = _exit_on_last_window;
				_exit_on_last_window = control;
				_system->RunMainLoop();
				_exit_on_last_window = prev;
			}
			virtual void Exit(void) noexcept { _system->ExitMainLoop(); }
			virtual void ExitModal(IWindow * window) noexcept { _system->ExitModalSession(window->Expose()); }
			virtual XPosition GetCursorPosition(void) noexcept { return WrapWObject(_system->GetCursorPosition()); }
			virtual void SetCursorPosition(const XPosition & pos) noexcept { _system->SetCursorPosition(WrapWObject(pos)); }
			virtual SafePointer<Windows::ICursor> LoadCursor(handle image, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Codec::Frame> frame = ExtractFrameFromXFrame(image);
				SafePointer<Windows::ICursor> cursor = _system->LoadCursor(frame);
				if (!cursor) throw OutOfMemoryException();
				return cursor;
				XE_TRY_OUTRO(0);
			}
			virtual SafePointer<Windows::ICursor> LoadSystemCursor(uint id, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Windows::ICursor> cursor;
				cursor.SetRetain(_system->GetSystemCursor(static_cast<Windows::SystemCursorClass>(id)));
				if (!cursor) throw InvalidArgumentException();
				return cursor;
				XE_TRY_OUTRO(0);
			}
			virtual handle GetCallback(void) noexcept = 0;
			virtual void SetCallback(const handle & callback) noexcept = 0;
			virtual SafePointer< Array<Point> > GetIconSizes(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< Array<Point> > result = _system->GetApplicationIconSizes();
				if (!result) throw OutOfMemoryException();
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual void SetIcon(const handle & ximage) noexcept
			{
				try {
					if (ximage) {
						SafePointer<Codec::Image> icon = ExtractImageFromXImage(ximage);
						if (icon) _system->SetApplicationIcon(icon);
					} else _update_icon();
				} catch (...) {}
			}
			virtual void SetIconBadge(const string & text) noexcept { _system->SetApplicationBadge(text); }
			virtual void SetIconMode(const uint & mode) noexcept { _dock_tile_mode = mode; _update_tile(); }
			virtual void OpenFileDialog(const _open_file_desc & desc, _open_file_result * out, IWindow * window, IDispatchTask * hdlr, ErrorContext & ectx)
			{
				XE_TRY_INTRO
				SafePointer<IDispatchTask> task;
				task.SetRetain(hdlr);
				auto tint = CreateStructuredTask<Windows::OpenFileInfo>([this, out, task](const Windows::OpenFileInfo & info) {
					try { if (out) out->files = info.Files; } catch (...) {}
					if (task) task->DoTask(_system);
				});
				tint->Value1.MultiChoose = desc.multiple;
				tint->Value1.Title = desc.title;
				tint->Value1.DefaultFormat = desc.default_format;
				for (auto & f : desc.formats) {
					Windows::FileFormat fmt;
					fmt.Description = f.desc;
					fmt.Extensions = f.exts.Split(L'|');
					tint->Value1.Formats << fmt;
				}
				Windows::IWindow * parent = 0;
				if (window) window->ExposeInterface(VisualObjectInterfaceWindow, &parent, ectx);
				if (ectx.error_code) return;
				if (!_system->OpenFileDialog(&tint->Value1, parent, tint)) throw OutOfMemoryException();
				XE_TRY_OUTRO()
			}
			virtual void SaveFileDialog(const _save_file_desc & desc, _save_file_result * out, IWindow * window, IDispatchTask * hdlr, ErrorContext & ectx)
			{
				XE_TRY_INTRO
				SafePointer<IDispatchTask> task;
				task.SetRetain(hdlr);
				auto tint = CreateStructuredTask<Windows::SaveFileInfo>([this, out, task](const Windows::SaveFileInfo & info) {
					try { if (out) { out->file = info.File; out->format = info.Format; } } catch (...) {}
					if (task) task->DoTask(_system);
				});
				tint->Value1.AppendExtension = desc.append_extension;
				tint->Value1.Title = desc.title;
				tint->Value1.Format = desc.default_format;
				for (auto & f : desc.formats) {
					Windows::FileFormat fmt;
					fmt.Description = f.desc;
					fmt.Extensions = f.exts.Split(L'|');
					tint->Value1.Formats << fmt;
				}
				Windows::IWindow * parent = 0;
				if (window) window->ExposeInterface(VisualObjectInterfaceWindow, &parent, ectx);
				if (ectx.error_code) return;
				if (!_system->SaveFileDialog(&tint->Value1, parent, tint)) throw OutOfMemoryException();
				XE_TRY_OUTRO()
			}
			virtual void ChooseDirectoryDialog(const string & title, string * out, IWindow * window, IDispatchTask * hdlr, ErrorContext & ectx)
			{
				XE_TRY_INTRO
				SafePointer<IDispatchTask> task;
				task.SetRetain(hdlr);
				auto tint = CreateStructuredTask<Windows::ChooseDirectoryInfo>([this, out, task](const Windows::ChooseDirectoryInfo & info) {
					try { if (out) *out = info.Directory; } catch (...) {}
					if (task) task->DoTask(_system);
				});
				tint->Value1.Title = title;
				Windows::IWindow * parent = 0;
				if (window) window->ExposeInterface(VisualObjectInterfaceWindow, &parent, ectx);
				if (ectx.error_code) return;
				if (!_system->ChooseDirectoryDialog(&tint->Value1, parent, tint)) throw OutOfMemoryException();
				XE_TRY_OUTRO()
			}
			virtual void CommonDialog(const string & text, const string & title, uint mode, uint style, uint * out, IWindow * window, IDispatchTask * hdlr, ErrorContext & ectx)
			{
				XE_TRY_INTRO
				SafePointer<IDispatchTask> task;
				task.SetRetain(hdlr);
				auto tint = CreateStructuredTask<Windows::MessageBoxResult>([this, out, task](Windows::MessageBoxResult result) {
					if (out) {
						if (result == Windows::MessageBoxResult::Cancel) *out = 0;
						else if (result == Windows::MessageBoxResult::Ok) *out = 1;
						else if (result == Windows::MessageBoxResult::Yes) *out = 2;
						else if (result == Windows::MessageBoxResult::No) *out = 3;
					}
					if (task) task->DoTask(_system);
				});
				Windows::MessageBoxButtonSet set;
				Windows::MessageBoxStyle stl;
				if (mode == 0) set = Windows::MessageBoxButtonSet::Ok;
				else if (mode == 1) set = Windows::MessageBoxButtonSet::OkCancel;
				else if (mode == 2) set = Windows::MessageBoxButtonSet::YesNo;
				else if (mode == 3) set = Windows::MessageBoxButtonSet::YesNoCancel;
				else throw InvalidArgumentException();
				if (style == 0) stl = Windows::MessageBoxStyle::Information;
				else if (style == 1) stl = Windows::MessageBoxStyle::Warning;
				else if (style == 2) stl = Windows::MessageBoxStyle::Error;
				else throw InvalidArgumentException();
				Windows::IWindow * parent = 0;
				if (window) window->ExposeInterface(VisualObjectInterfaceWindow, &parent, ectx);
				if (ectx.error_code) return;
				if (!_system->MessageBox(&tint->Value1, text, title, parent, set, stl, tint)) throw OutOfMemoryException();
				XE_TRY_OUTRO()
			}
			virtual SafePointer<IMenu> CreateMenu(ErrorContext & ectx) noexcept { XE_TRY_INTRO return new IMenu(*_system); XE_TRY_OUTRO(0) }
			virtual SafePointer<IMenuItem> CreateMenuItem(ErrorContext & ectx) noexcept { XE_TRY_INTRO return new MenuItem(*_system); XE_TRY_OUTRO(0) }
			virtual XPosition GetNotificationIconSize(void) noexcept { return WrapWObject(_system->GetUserNotificationIconSize()); }
			virtual void Notify(const string & title, const string & text, handle ximage) noexcept
			{
				try {
					SafePointer<Codec::Image> icon;
					if (ximage) icon = ExtractImageFromXImage(ximage); else {
						auto size = _system->GetUserNotificationIconSize();
						if (size.x && size.y) {
							icon = new Codec::Image;
							SafePointer<Codec::Frame> frame = XI::LoadModuleIcon(_main->GetResources(), 1, size);
							if (!frame) icon.SetReference(0); else icon->Frames.Append(frame);
						}
					}
					_system->PushUserNotification(title, text, icon);
				} catch (...) {}
			}
			virtual SafePointer<IStatusIcon> CreateStatusIcon(ErrorContext & ectx) noexcept { XE_TRY_INTRO return new StatusIcon(*_system); XE_TRY_OUTRO(0) }
			virtual void CreateKeyboardEvent(uint id, uint key_code, uint flags, ErrorContext & ectx) noexcept { if (!_system->CreateHotKey(id, key_code, flags)) { ectx.error_code = 5; ectx.error_subcode = 0; } }
			virtual void RemoveKeyboardEvent(uint id) noexcept { _system->RemoveHotKey(id); }
			virtual void LaunchCommunicationService(const string & app, const string & auth, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!_system->LaunchIPCServer(app, auth)) throw InvalidStateException();
				XE_TRY_OUTRO()
			}
			virtual SafePointer<ICommunication> JoinCommunicationService(const string & app, const string & auth, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				return new ICommunication(*_system, app, auth);
				XE_TRY_OUTRO(0)
			}
			void RegisterWindow(IWindow * window) noexcept { try { _windows.AddElement(window); _update_tile(); } catch (...) {} }
			void UnregisterWindow(IWindow * window) noexcept
			{
				_windows.RemoveElement(window);
				_update_tile();
				if (_windows.IsEmpty() && _exit_on_last_window) _system->ExitMainLoop();
			}
			Windows::IWindowSystem * Expose(void) const noexcept { return _system; }
		};
		class WindowSystem : public IWindowSystem, Windows::IApplicationCallback
		{
		public:
			WindowSystem(const XE::Module * main, IWindowSystemCallback * callback, const Array<string> * files) : IWindowSystem(files)
			{
				_main = main;
				_callback = callback;
				_system->SetCallback(this);
				_dock_tile_mode = 2;
				_dock_tile_state = 0;
				#ifdef ENGINE_WINDOWS
				_update_icon();
				#endif
			}
			virtual ~WindowSystem(void) override {}
			virtual handle GetCallback(void) noexcept override { return _callback; }
			virtual void SetCallback(const handle & callback) noexcept override { _callback = reinterpret_cast<IWindowSystemCallback *>(callback); _system->SetCallback(this); }
			virtual bool IsHandlerEnabled(Windows::ApplicationHandler event) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				auto result = _callback ? _callback->ApplicationIsEventAvailable(WrapWObject(event), ectx) : false;
				if (ectx.error_code) return false; else return result;
			}
			virtual bool IsWindowEventAccessible(Windows::WindowHandler handler) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				auto result = _callback ? _callback->ApplicationIsWindowEventAvailable(WrapWObject(handler), ectx) : false;
				if (ectx.error_code) return false; else return result;
			}
			virtual void CreateNewFile(void) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->ApplicationHandleEvent(WrapWObject(Windows::ApplicationHandler::CreateFile), L"", ectx);
			}
			virtual void OpenSomeFile(void) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->ApplicationHandleEvent(WrapWObject(Windows::ApplicationHandler::OpenSomeFile), L"", ectx);
			}
			virtual bool OpenExactFile(const string & path) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				auto result = _callback ? _callback->ApplicationHandleEvent(WrapWObject(Windows::ApplicationHandler::OpenExactFile), path, ectx) : false;
				if (ectx.error_code) return false; else return result;
			}
			virtual void ShowHelp(void) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->ApplicationHandleEvent(WrapWObject(Windows::ApplicationHandler::ShowHelp), L"", ectx);
			}
			virtual void ShowAbout(void) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->ApplicationHandleEvent(WrapWObject(Windows::ApplicationHandler::ShowAbout), L"", ectx);
			}
			virtual void ShowProperties(void) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->ApplicationHandleEvent(WrapWObject(Windows::ApplicationHandler::ShowProperties), L"", ectx);
			}
			virtual void HotKeyEvent(int event_id) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->ApplicationHandleGlobalKeyboard(event_id, ectx);
			}
			virtual bool DataExchangeReceive(handle client, const string & verb, const DataBlock * data) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->ApplicationDataReceive(client, verb, data, ectx); else return false;
				return ectx.error_code == 0;
			}
			virtual DataBlock * DataExchangeRespond(handle client, const string & verb) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				SafePointer<DataBlock> result;
				if (_callback) result = _callback->ApplicationDataRespond(client, verb, ectx);
				if (ectx.error_code || !result) return 0;
				result->Retain();
				return result;
			}
			virtual void DataExchangeDisconnect(handle client) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				if (_callback) _callback->ApplicationDataDisconnect(client, ectx);
			}
			virtual bool Terminate(void) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				auto result = _callback ? _callback->ApplicationHandleEvent(WrapWObject(Windows::ApplicationHandler::Terminate), L"", ectx) : true;
				if (ectx.error_code) return true; else return result;
			}
		};
		class IWindowSystemFactory
		{
		public:
			virtual IWindowSystem * CreateA(ErrorContext & ectx) noexcept = 0;
			virtual IWindowSystem * CreateB(IWindowSystemCallback * callback, ErrorContext & ectx) noexcept = 0;
			virtual IWindowSystem * CreateC(const DataBlock * locale, ErrorContext & ectx) noexcept = 0;
			virtual IWindowSystem * CreateD(IWindowSystemCallback * callback, const DataBlock * locale, ErrorContext & ectx) noexcept = 0;
			virtual IWindowSystem * Current(ErrorContext & ectx) noexcept = 0;
			virtual void Initialize(const XE::Module * main) noexcept = 0;
		};
		class WindowsExtension : public IAPIExtension, IWindowSystemFactory
		{
			const XE::Module * _main;
			StandardLoader & _loader;
			SafePointer<IWindowSystem> _system;
		public:
			WindowsExtension(StandardLoader & loader) : _main(0), _loader(loader) {}
			virtual ~WindowsExtension(void) override {}
			virtual const void * ExposeRoutine(const string & routine_name) noexcept override { return 0; }
			virtual const void * ExposeInterface(const string & interface) noexcept override { if (interface == L"sysfen") return static_cast<IWindowSystemFactory *>(this); else return 0; }
			virtual IWindowSystem * CreateA(ErrorContext & ectx) noexcept override
			{
				if (!_main) { ectx.error_code = 1; return 0; }
				if (_system) { ectx.error_code = 5; return 0; }
				XE_TRY_INTRO
				SafePointer< Array<string> > args;
				try { args = GetCommandLineFileIO(_loader); } catch (...) {}
				_system = new WindowSystem(_main, 0, args);
				return _system;
				XE_TRY_OUTRO(0)
			}
			virtual IWindowSystem * CreateB(IWindowSystemCallback * callback, ErrorContext & ectx) noexcept override
			{
				if (!_main) { ectx.error_code = 1; return 0; }
				if (_system) { ectx.error_code = 5; return 0; }
				XE_TRY_INTRO
				SafePointer< Array<string> > args;
				try { args = GetCommandLineFileIO(_loader); } catch (...) {}
				_system = new WindowSystem(_main, callback, args);
				return _system;
				XE_TRY_OUTRO(0)
			}
			virtual IWindowSystem * CreateC(const DataBlock * locale, ErrorContext & ectx) noexcept override
			{
				if (!_main) { ectx.error_code = 1; return 0; }
				if (_system) { ectx.error_code = 5; return 0; }
				XE_TRY_INTRO
				SafePointer< Array<string> > args;
				try { args = GetCommandLineFileIO(_loader); } catch (...) {}
				if (!locale) throw InvalidArgumentException();
				SafePointer<Streaming::Stream> stream = new Streaming::MemoryStream(locale->GetBuffer(), locale->Length());
				SafePointer<Storage::StringTable> table = new Storage::StringTable(stream);
				Assembly::SetLocalizedCommonStrings(table);
				_system = new WindowSystem(_main, 0, args);
				return _system;
				XE_TRY_OUTRO(0)
			}
			virtual IWindowSystem * CreateD(IWindowSystemCallback * callback, const DataBlock * locale, ErrorContext & ectx) noexcept override
			{
				if (!_main) { ectx.error_code = 1; return 0; }
				if (_system) { ectx.error_code = 5; return 0; }
				XE_TRY_INTRO
				SafePointer< Array<string> > args;
				try { args = GetCommandLineFileIO(_loader); } catch (...) {}
				if (!locale) throw InvalidArgumentException();
				SafePointer<Streaming::Stream> stream = new Streaming::MemoryStream(locale->GetBuffer(), locale->Length());
				SafePointer<Storage::StringTable> table = new Storage::StringTable(stream);
				Assembly::SetLocalizedCommonStrings(table);
				_system = new WindowSystem(_main, callback, args);
				return _system;
				XE_TRY_OUTRO(0)
			}
			virtual IWindowSystem * Current(ErrorContext & ectx) noexcept override { if (!_system) ectx.error_code = 5; return _system; }
			virtual void Initialize(const XE::Module * main) noexcept override { _main = main; }
		};

		Window::Window(const WindowDesc & desc, IWindowSystem * system, bool modal)
		{
			ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
			Windows::CreateWindowDesc desc2;
			_modal = modal;
			_callback = desc.callback;
			_window = 0;
			_system = system;
			_parent = desc.parent;
			_top_level = _parent == 0;
			_cursor_reset = false;
			_cursor = _system->LoadSystemCursor(1, ectx);
			if (ectx.error_code) throw OutOfMemoryException();
			desc2.Flags = desc.flags;
			desc2.Title = desc.title;
			desc2.Position = WrapWObject(desc.position);
			desc2.MinimalConstraints = WrapWObject(desc.minimal_size);
			desc2.MaximalConstraints = WrapWObject(desc.maximal_size);
			desc2.FrameMargins = WrapWObject(desc.margins);
			desc2.Opacity = desc.opacity;
			desc2.BlurFactor = desc.blur_factor;
			desc2.Theme = desc.dark_theme ? Windows::ThemeClass::Dark : Windows::ThemeClass::Light;
			desc2.BackgroundColor.Value = desc.background.value;
			desc2.Callback = this;
			desc2.ParentWindow = _parent ? _parent->Expose() : 0;
			desc2.Screen = desc.screen ? desc.screen->Expose() : 0;
			if (modal) _system->Expose()->CreateModalWindow(desc2);
			else _system->Expose()->CreateWindow(desc2);
		}
		Window::~Window(void) {}
		void Window::Created(Windows::IWindow * window)
		{
			_window = window;
			if (_parent) _parent->_children.Append(this); else _system->RegisterWindow(this);
			ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
			if (_callback) _callback->WindowCreated(this, ectx);
		}
		void Window::Destroyed(Windows::IWindow * window)
		{
			ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
			if (_callback) _callback->WindowDestroyed(this, ectx);
			for (auto & c : _children) c->_parent = 0;
			if (_parent) {
				for (int i = 0; i < _parent->_children.Length(); i++) if (_parent->_children[i] == static_cast<IWindow *>(this)) {
					_parent->_children.Remove(i);
					break;
				}
			} else if (_top_level) _system->UnregisterWindow(this);
			Release();
		}
		void Window::SetCursor(Windows::IWindow * window, Point at) { _system->Expose()->SetCursor(_cursor); }
		void Window::MouseMove(Windows::IWindow * window, Point at)
		{
			ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
			if (_callback) _callback->WindowMouseMoved(this, WrapWObject(at), ectx);
			if (_cursor_reset) { _system->Expose()->SetCursor(_cursor); _cursor_reset = false; }
		}
		void Window::LeftButtonDown(Windows::IWindow * window, Point at)
		{
			ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
			if (_callback) _callback->WindowLeftButtonDown(this, WrapWObject(at), ectx);
			if (_cursor_reset) { _system->Expose()->SetCursor(_cursor); _cursor_reset = false; }
		}
		void Window::LeftButtonUp(Windows::IWindow * window, Point at)
		{
			ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
			if (_callback) _callback->WindowLeftButtonUp(this, WrapWObject(at), ectx);
			if (_cursor_reset) { _system->Expose()->SetCursor(_cursor); _cursor_reset = false; }
		}
		void Window::LeftButtonDoubleClick(Windows::IWindow * window, Point at)
		{
			ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
			if (_callback) _callback->WindowLeftButtonDouble(this, WrapWObject(at), ectx);
			if (_cursor_reset) { _system->Expose()->SetCursor(_cursor); _cursor_reset = false; }
		}
		void Window::RightButtonDown(Windows::IWindow * window, Point at)
		{
			ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
			if (_callback) _callback->WindowRightButtonDown(this, WrapWObject(at), ectx);
			if (_cursor_reset) { _system->Expose()->SetCursor(_cursor); _cursor_reset = false; }
		}
		void Window::RightButtonUp(Windows::IWindow * window, Point at)
		{
			ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
			if (_callback) _callback->WindowRightButtonUp(this, WrapWObject(at), ectx);
			if (_cursor_reset) { _system->Expose()->SetCursor(_cursor); _cursor_reset = false; }
		}
		void Window::RightButtonDoubleClick(Windows::IWindow * window, Point at)
		{
			ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
			if (_callback) _callback->WindowRightButtonDouble(this, WrapWObject(at), ectx);
			if (_cursor_reset) { _system->Expose()->SetCursor(_cursor); _cursor_reset = false; }
		}

		void ActivateWindowsIO(StandardLoader & ldr)
		{
			SafePointer<IAPIExtension> ext = new WindowsExtension(ldr);
			if (!ldr.RegisterAPIExtension(ext)) throw Exception();
		}
		void ControlWindowsIO(StandardLoader & ldr, const XE::Module * module)
		{
			auto factory = reinterpret_cast<IWindowSystemFactory *>(ldr.ExposeInterface(L"sysfen"));
			if (factory) factory->Initialize(module);
		}
	}
}