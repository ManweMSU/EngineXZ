#pragma once

#include "xe_loader.h"
#include "xe_imgapi.h"

namespace Engine
{
	namespace XE
	{
		struct XPosition
		{
			int x, y;
			XPosition(void);
			~XPosition(void);
		};
		struct XRectangle
		{
			int left, top, right, bottom;
			XRectangle(void);
			~XRectangle(void);
		};

		XPosition WrapWObject(const Point & object);
		XRectangle WrapWObject(const Box & object);
		Point WrapWObject(const XPosition & object);
		Box WrapWObject(const XRectangle & object);
		uint WrapWObject(Windows::WindowHandler hdlr);
		uint WrapWObject(Windows::ApplicationHandler hdlr);
		uint WrapWObject(Windows::IPCStatus status);

		void ActivateWindowsIO(StandardLoader & ldr);
		void ControlWindowsIO(StandardLoader & ldr, const XE::Module * module);

		// Extensions for Engine UI
		class IDeviceControl
		{
		public:
			virtual DynamicObject * ControlSystemGetDevice(void) noexcept = 0;
			virtual void ControlSystemSetDevice(DynamicObject * dev) noexcept = 0;
			virtual Windows::DeviceClass ControlSystemGetDeviceClass(void) noexcept = 0;
			virtual void ControlSystemSetDeviceClass(Windows::DeviceClass devcls) noexcept = 0;
			virtual void * ControlSystemGetCallback(void) noexcept = 0;
			virtual void ControlSystemSetCallback(void * callback) noexcept = 0;
		};
		VisualObject * GetControlSystemWindow(UI::ControlSystem * cs) noexcept;
		VisualObject * CreateWWindow(handle winsys, const void * desc, Windows::DeviceClass device, ErrorContext & ectx) noexcept;
		VisualObject * CreateWWindow(handle winsys, UI::Template::ControlTemplate * base, void * callback, const UI::Rectangle & outer, VisualObject * parent, UI::IControlFactory * factory, Windows::DeviceClass device, ErrorContext & ectx) noexcept;
		VisualObject * CreateModalWWindow(handle winsys, const void * desc, Windows::DeviceClass device, ErrorContext & ectx) noexcept;
		VisualObject * CreateModalWWindow(handle winsys, UI::Template::ControlTemplate * base, void * callback, const UI::Rectangle & outer, VisualObject * parent, UI::IControlFactory * factory, Windows::DeviceClass device, ErrorContext & ectx) noexcept;
		Object * WrapWMenu(Windows::IMenu * menu);
		Object * WrapWMenuItem(Windows::IMenuItem * item);
		Object * CreateWMenu(UI::Template::ControlTemplate * base, ErrorContext & ectx) noexcept;
		Windows::IMenu * UnwrapWMenu(Object * menu) noexcept;
		Windows::IMenuItem * UnwrapWMenuItem(Object * item) noexcept;
	}
}