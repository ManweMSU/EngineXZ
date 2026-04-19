#include "xe_engine_ui.h"
#include "xe_wndapi.h"
#include "xe_imgapi.h"

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
		constexpr const uint io_mode_read	= 0;
		constexpr const uint io_mode_write	= 1;
		constexpr const uint io_mode_add	= 2;
		constexpr const uint io_mode_remove	= 3;

		class IEngineUIMarshaller
		{
		public:
			const void * int_type, * double_type, * color_type, * string_type, * texture_type, * font_type, * internal_type;
			void * window_system;
			StandardLoader & loader;
		public:
			IEngineUIMarshaller(StandardLoader & ldr) : loader(ldr), int_type(0), double_type(0), color_type(0), string_type(0), texture_type(0), font_type(0), internal_type(0), window_system(0) {}
			virtual uintptr api_level_0(uint32 sel, uintptr a0, uintptr a1, uintptr a2, uintptr a3, uintptr a4, ErrorContext & ectx) noexcept = 0;
			virtual uintptr api_level_1(Object * obj, uint32 sel, uintptr a0, uintptr a1, uintptr a2, uintptr a3, uintptr a4, ErrorContext & ectx) noexcept = 0;
			virtual uintptr api_level_2(Object * obj, uint32 sel, uintptr a0, uintptr a1, uintptr a2, uintptr a3, uintptr a4, ErrorContext & ectx) noexcept = 0;
		};
		struct XSPosition
		{
			double relative;
			double scalable;
			int absolute;
			int unused;
			XSPosition(void) {}
			~XSPosition(void) {}
		};
		struct XSRectangle
		{
			XSPosition left, top, right, bottom;
			XSRectangle(void) {}
			~XSRectangle(void) {}
		};
		struct XControlWrapper
		{
			SafePointer<Object> inner;
			XControlWrapper(void) {}
			XControlWrapper(Object * value) { inner.SetRetain(value); }
			~XControlWrapper(void) {}
		};
		struct XAccelerator
		{
			int command_id;
			uint vkc;
			bool shift;
			bool control;
			bool alternative;
			bool is_system_command;
		};

		XSPosition WrapWObject(const UI::Coordinate & object)
		{
			XSPosition result;
			result.absolute = object.Absolute;
			result.scalable = object.Zoom;
			result.relative = object.Anchor;
			return result;
		}
		XSRectangle WrapWObject(const UI::Rectangle & object)
		{
			XSRectangle result;
			result.left = WrapWObject(object.Left);
			result.top = WrapWObject(object.Top);
			result.right = WrapWObject(object.Right);
			result.bottom = WrapWObject(object.Bottom);
			return result;
		}
		XAccelerator WrapWObject(const UI::Accelerators::AcceleratorCommand & object)
		{
			XAccelerator result;
			if (object.SystemCommand) {
				result.is_system_command = true;
				auto sc = static_cast<UI::Accelerators::AcceleratorSystemCommand>(object.CommandID);
				if (sc == UI::Accelerators::AcceleratorSystemCommand::WindowClose) result.command_id = 0;
				else if (sc == UI::Accelerators::AcceleratorSystemCommand::WindowInvokeHelp) result.command_id = 1;
				else if (sc == UI::Accelerators::AcceleratorSystemCommand::WindowNextControl) result.command_id = 2;
				else if (sc == UI::Accelerators::AcceleratorSystemCommand::WindowPreviousControl) result.command_id = 3;
				else result.command_id = 0;
			} else {
				result.is_system_command = false;
				result.command_id = object.CommandID;
			}
			result.vkc = object.KeyCode;
			result.shift = object.Shift;
			result.control = object.Control;
			result.alternative = object.Alternative;
			return result;
		}
		UI::Coordinate WrapWObject(const XSPosition & object)
		{
			UI::Coordinate result;
			result.Absolute = object.absolute;
			result.Zoom = object.scalable;
			result.Anchor = object.relative;
			return result;
		}
		UI::Rectangle WrapWObject(const XSRectangle & object)
		{
			UI::Rectangle result;
			result.Left = WrapWObject(object.left);
			result.Top = WrapWObject(object.top);
			result.Right = WrapWObject(object.right);
			result.Bottom = WrapWObject(object.bottom);
			return result;
		}
		UI::Accelerators::AcceleratorCommand WrapWObject(const XAccelerator & object)
		{
			UI::Accelerators::AcceleratorCommand result;
			if (object.is_system_command) {
				result.SystemCommand = true;
				if (object.command_id == 0) result.CommandID = int(UI::Accelerators::AcceleratorSystemCommand::WindowClose);
				else if (object.command_id == 1) result.CommandID = int(UI::Accelerators::AcceleratorSystemCommand::WindowInvokeHelp);
				else if (object.command_id == 2) result.CommandID = int(UI::Accelerators::AcceleratorSystemCommand::WindowNextControl);
				else result.CommandID = int(UI::Accelerators::AcceleratorSystemCommand::WindowPreviousControl);
			} else {
				result.SystemCommand = false;
				result.CommandID = object.command_id;
			}
			result.KeyCode = object.vkc;
			result.Shift = object.shift;
			result.Control = object.control;
			result.Alternative = object.alternative;
			return result;
		}
		Windows::DeviceClass WrapDeviceClass(uintptr value) noexcept
		{
			if (value == 0) return Windows::DeviceClass::DontCare;
			else if (value == 1) return Windows::DeviceClass::Hardware;
			else if (value == 2) return Windows::DeviceClass::Basic;
			else return Windows::DeviceClass::Null;
		}
		uintptr WrapWObject(Windows::DeviceClass devcls) noexcept
		{
			if (devcls == Windows::DeviceClass::DontCare) return 0;
			else if (devcls == Windows::DeviceClass::Hardware) return 1;
			else if (devcls == Windows::DeviceClass::Basic) return 2;
			else return 3;
		}
		UI::Animation::AnimationClass WrapAnimationClass(uintptr value) noexcept
		{
			if (value) return UI::Animation::AnimationClass::Hard;
			else return UI::Animation::AnimationClass::Smooth;
		}
		UI::Animation::SlideSide WrapAnimationDirection(uintptr value) noexcept
		{
			if (value == 0) return UI::Animation::SlideSide::Left;
			else if (value == 1) return UI::Animation::SlideSide::Top;
			else if (value == 2) return UI::Animation::SlideSide::Right;
			else return UI::Animation::SlideSide::Bottom;
		}
		UI::Animation::AnimationAction WrapAnimationAction(uintptr value) noexcept
		{
			if (value == 0) return UI::Animation::AnimationAction::None;
			else if (value == 1) return UI::Animation::AnimationAction::ShowWindow;
			else if (value == 2) return UI::Animation::AnimationAction::HideWindow;
			else return UI::Animation::AnimationAction::HideWindowKeepPosition;
		}

		class XVisual : public Object
		{
			SafePointer<UI::Shape> _shape;
		public:
			XVisual(UI::Shape * shape) { _shape.SetRetain(shape); }
			virtual ~XVisual(void) override {}
			virtual string ToString(void) const override { try { return L"XVisual"; } catch (...) { return L""; } }
			virtual void Render(DynamicObject * context, const XRectangle & at) noexcept { try { _shape->Render(GetWrappedDeviceContext(context), WrapWObject(at)); } catch (...) {} }
			virtual void ClearCache(void) noexcept { _shape->ClearCache(); }
			virtual SafePointer<XVisual> Clone(ErrorContext & ectx) noexcept { XE_TRY_INTRO SafePointer<UI::Shape> copy = _shape->Clone(); return new XVisual(copy); XE_TRY_OUTRO(0) }
			UI::Shape * Expose(void) noexcept { return _shape; }
		};
		class XArgumentProvider
		{
		public:
			virtual void ProvideValue(const string & argname, const void * fortype, void * pvalue, ErrorContext & ectx) noexcept = 0;	
		};
		class XArgumentMarshaller : public UI::IArgumentProvider
		{
			XArgumentProvider * _args;
			IEngineUIMarshaller * _eui;
		public:
			XArgumentMarshaller(XArgumentProvider * args, IEngineUIMarshaller * eui) : _args(args), _eui(eui) {}
			virtual void GetArgument(const string & name, int * value) override
			{
				ErrorContext ectx;
				ectx.error_code = ectx.error_subcode = 0;
				_args->ProvideValue(name, _eui->int_type, value, ectx);
				if (ectx.error_code) *value = 0;
			}
			virtual void GetArgument(const string & name, double * value) override
			{
				ErrorContext ectx;
				ectx.error_code = ectx.error_subcode = 0;
				_args->ProvideValue(name, _eui->double_type, value, ectx);
				if (ectx.error_code) *value = 0.0;
			}
			virtual void GetArgument(const string & name, Color * value) override
			{
				XColor color;
				ErrorContext ectx;
				ectx.error_code = ectx.error_subcode = 0;
				_args->ProvideValue(name, _eui->color_type, &color, ectx);
				if (ectx.error_code) color.value = 0;
				value->Value = color.value;
			}
			virtual void GetArgument(const string & name, string * value) override
			{
				ErrorContext ectx;
				ectx.error_code = ectx.error_subcode = 0;
				_args->ProvideValue(name, _eui->string_type, value, ectx);
				if (ectx.error_code) *value = L"";
			}
			virtual void GetArgument(const string & name, Graphics::IBitmap ** value) override
			{
				SafePointer<Object> object;
				ErrorContext ectx;
				ectx.error_code = ectx.error_subcode = 0;
				_args->ProvideValue(name, _eui->texture_type, &object, ectx);
				if (ectx.error_code) object.SetReference(0);
				auto u = object ? GetWrappedBitmap(object) : 0;
				if (u) u->Retain();
				*value = u;
			}
			virtual void GetArgument(const string & name, Graphics::IFont ** value) override
			{
				SafePointer<Object> object;
				ErrorContext ectx;
				ectx.error_code = ectx.error_subcode = 0;
				_args->ProvideValue(name, _eui->font_type, &object, ectx);
				if (ectx.error_code) object.SetReference(0);
				auto u = object ? GetWrappedFont(object) : 0;
				if (u) u->Retain();
				*value = u;
			}
		};
		class XVisualTemplate : public Object
		{
			SafePointer<UI::Template::Shape> _shape;
			IEngineUIMarshaller * _eui;
		public:
			XVisualTemplate(UI::Template::Shape * shape, IEngineUIMarshaller * eui) : _eui(eui) { _shape.SetRetain(shape); }
			virtual ~XVisualTemplate(void) override {}
			virtual string ToString(void) const override { try { return L"XVisualTemplate"; } catch (...) { return L""; } }
			virtual SafePointer<XVisual> CreateInstance(XArgumentProvider * args, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (args) {
					XArgumentMarshaller provider(args, _eui);
					SafePointer<UI::Shape> shape = _shape->Initialize(&provider);
					return new XVisual(shape);
				} else {
					UI::ZeroArgumentProvider provider;
					SafePointer<UI::Shape> shape = _shape->Initialize(&provider);
					return new XVisual(shape);
				}
				XE_TRY_OUTRO(0)
			}
			UI::Template::Shape * Expose(void) noexcept { return _shape; }
		};

		class XControlTemplate : public DynamicObject
		{
		public:
			virtual XControlWrapper CreateInstance(const XSRectangle & position, ObjectArray<DynamicObject> & children, ErrorContext & ectx) noexcept = 0;
		};
		class XControlFactory
		{
		public:
			virtual SafePointer<XControlTemplate> CreateTemplate(const string & classname, ErrorContext & ectx) noexcept = 0;
		};
		namespace ControlMarshalling
		{
			// Directly marshalled: bool, int, double, string, Color
			// Convertion required: IBitmap *, IFont *, Shape *, ControlTemplate *, Rectangle
			struct MirrorRecord
			{
				Reflection::PropertyType type;
				SafePointer<Object> ertobj;
				UI::Rectangle ertrect;
			};
			typedef Volumes::Dictionary<void *, MirrorRecord> MirrorRegistry;
		}
		class XCustomControlFrame : public UI::Template::ControlReflectedBase
		{
			const ClassSymbol * _classinfo;
			SafePointer<XControlTemplate> _template;
			ControlMarshalling::MirrorRegistry * _mirror;
		public:
			XCustomControlFrame(XControlTemplate * base, ControlMarshalling::MirrorRegistry * mirror) : _mirror(mirror) { _template.SetRetain(base); _classinfo = reinterpret_cast<const ClassSymbol *>(_template->GetType()); }
			virtual ~XCustomControlFrame(void) override {}
			virtual void EnumerateProperties(Reflection::IPropertyEnumerator & enumerator) override
			{
				SafePointer< Array<string> > fields = _classinfo->ListFields();
				for (auto & f : *fields) {
					auto prop = GetProperty(f);
					if (prop.Address) enumerator.EnumerateProperty(prop.Name, prop.Address, prop.Type, prop.InnerType, prop.Volume, prop.ElementSize);
				}
			}
			virtual Reflection::PropertyInfo GetProperty(const string & name) override
			{
				Reflection::PropertyInfo prop;
				prop.Address = 0;
				prop.Type = prop.InnerType = Reflection::PropertyType::Unknown;
				prop.ElementSize = prop.Volume = 0;
				auto prop_tcn = _classinfo->GetFieldType(name);
				if (!prop_tcn) return prop;
				prop.Name = name;
				prop.InnerType = Reflection::PropertyType::Unknown;
				prop.Volume = 1;
				if (*prop_tcn == L"Clogicum") {
					prop.Address = _classinfo->GetFieldAddress(name, _template.Inner());
					prop.ElementSize = _classinfo->GetFieldSize(name);
					prop.Type = Reflection::PropertyType::Boolean;
				} else if (*prop_tcn == L"Cint32") {
					prop.Address = _classinfo->GetFieldAddress(name, _template.Inner());
					prop.ElementSize = _classinfo->GetFieldSize(name);
					prop.Type = Reflection::PropertyType::Integer;
				} else if (*prop_tcn == L"Cdfrac") {
					prop.Address = _classinfo->GetFieldAddress(name, _template.Inner());
					prop.ElementSize = _classinfo->GetFieldSize(name);
					prop.Type = Reflection::PropertyType::Double;
				} else if (*prop_tcn == L"Clinea") {
					prop.Address = _classinfo->GetFieldAddress(name, _template.Inner());
					prop.ElementSize = _classinfo->GetFieldSize(name);
					prop.Type = Reflection::PropertyType::String;
				} else if (*prop_tcn == L"Cimago.color") {
					prop.Address = _classinfo->GetFieldAddress(name, _template.Inner());
					prop.ElementSize = _classinfo->GetFieldSize(name);
					prop.Type = Reflection::PropertyType::Color;
				} else if (*prop_tcn == L"Cingenium.iu.quadrum") {
					auto fxa = _classinfo->GetFieldAddress(name, _template.Inner());
					auto mirror = _mirror->GetElementByKey(fxa);
					if (!mirror) {
						_mirror->Append(fxa, ControlMarshalling::MirrorRecord());
						mirror = _mirror->GetElementByKey(fxa);
						if (!mirror) throw InvalidStateException();
					}
					prop.Address = &mirror->ertrect;
					prop.ElementSize = sizeof(mirror->ertrect);
					prop.Type = mirror->type = Reflection::PropertyType::Rectangle;
				} else if (*prop_tcn == L"C@praeformae.I0(Cadl)(Cgraphicum.typographica)") {
					auto fxa = _classinfo->GetFieldAddress(name, _template.Inner());
					auto mirror = _mirror->GetElementByKey(fxa);
					if (!mirror) {
						_mirror->Append(fxa, ControlMarshalling::MirrorRecord());
						mirror = _mirror->GetElementByKey(fxa);
						if (!mirror) throw InvalidStateException();
					}
					prop.Address = &mirror->ertobj;
					prop.ElementSize = sizeof(mirror->ertobj);
					prop.Type = mirror->type = Reflection::PropertyType::Font;
				} else if (*prop_tcn == L"C@praeformae.I0(Cadl)(Cgraphicum.pictura)") {
					auto fxa = _classinfo->GetFieldAddress(name, _template.Inner());
					auto mirror = _mirror->GetElementByKey(fxa);
					if (!mirror) {
						_mirror->Append(fxa, ControlMarshalling::MirrorRecord());
						mirror = _mirror->GetElementByKey(fxa);
						if (!mirror) throw InvalidStateException();
					}
					prop.Address = &mirror->ertobj;
					prop.ElementSize = sizeof(mirror->ertobj);
					prop.Type = mirror->type = Reflection::PropertyType::Texture;
				} else if (*prop_tcn == L"C@praeformae.I0(Cadl)(Cingenium.iu.visualis_incompletus)") {
					auto fxa = _classinfo->GetFieldAddress(name, _template.Inner());
					auto mirror = _mirror->GetElementByKey(fxa);
					if (!mirror) {
						_mirror->Append(fxa, ControlMarshalling::MirrorRecord());
						mirror = _mirror->GetElementByKey(fxa);
						if (!mirror) throw InvalidStateException();
					}
					prop.Address = &mirror->ertobj;
					prop.ElementSize = sizeof(mirror->ertobj);
					prop.Type = mirror->type = Reflection::PropertyType::Application;
				} else if (*prop_tcn == L"C@praeformae.I0(Cadl)(Cobjectum_dynamicum)") {
					auto fxa = _classinfo->GetFieldAddress(name, _template.Inner());
					auto mirror = _mirror->GetElementByKey(fxa);
					if (!mirror) {
						_mirror->Append(fxa, ControlMarshalling::MirrorRecord());
						mirror = _mirror->GetElementByKey(fxa);
						if (!mirror) throw InvalidStateException();
					}
					prop.Address = &mirror->ertobj;
					prop.ElementSize = sizeof(mirror->ertobj);
					prop.Type = mirror->type = Reflection::PropertyType::Dialog;
				}
				return prop;
			}
			virtual string GetTemplateClass(void) const override { return L"XE_CustomControlFrame"; }
			XControlTemplate * Expose(void) noexcept { return _template; }
		};
		class XCustomControlFactory : public UI::IResourceResolver
		{
			XControlFactory * _factory;
			ControlMarshalling::MirrorRegistry * _mirror;
		public:
			XCustomControlFactory(XControlFactory * factory, ControlMarshalling::MirrorRegistry * mirror) : _factory(factory), _mirror(mirror) {}
			virtual ~XCustomControlFactory(void) override {}
			virtual string ToString(void) const override { try { return L"XCustomControlFactory"; } catch (...) { return L""; } }
			virtual Graphics::IBitmap * GetTexture(const string & Name) override { return 0; }
			virtual Graphics::IFont * GetFont(const string & Name) override { return 0; }
			virtual UI::Template::Shape * GetApplication(const string & Name) override { return 0; }
			virtual UI::Template::ControlTemplate * GetDialog(const string & Name) override { return 0; }
			virtual UI::Template::ControlReflectedBase * CreateCustomTemplate(const string & Class) override
			{
				if (_factory) {
					ErrorContext ectx;
					ectx.error_code = ectx.error_subcode = 0;
					auto base = _factory->CreateTemplate(Class, ectx);
					if (ectx.error_code == 0 && base) return new (std::nothrow) XCustomControlFrame(base, _mirror); else return 0;
				} else return 0;
			}
		};
		class XControlTemplateMarshaller : public DynamicObject
		{
			SafePointer<UI::Template::ControlTemplate> _base;
			XSRectangle _rect;
		public:
			XControlTemplateMarshaller(UI::Template::ControlTemplate * base) { _base.SetRetain(base); }
			virtual ~XControlTemplateMarshaller(void) override {}
			virtual string ToString(void) const override { try { return L"XControlTemplateMarshaller (" + _base->Properties->GetTemplateClass() + L")"; } catch (...) { return L""; } }
			XSRectangle GetRectangle(void) noexcept { return WrapWObject(_base->Properties->ControlPosition); }
			ObjectArray<DynamicObject> * GetChildren(void)
			{
				SafePointer< ObjectArray<DynamicObject> > children = new ObjectArray<DynamicObject>(_base->Children.Length());
				for (auto & c : _base->Children) {
					SafePointer<XControlTemplateMarshaller> m = new XControlTemplateMarshaller(&c);
					children->Append(m);
				}
				children->Retain();
				return children;
			}
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept override
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				if (cls->GetClassName() == L"objectum") {
					Retain(); return static_cast<Object *>(this);
				} else if (cls->GetClassName() == L"objectum_dynamicum") {
					Retain(); return static_cast<DynamicObject *>(this);
				} else if (cls->GetClassName() == L"ingenium.iu.quadrum") {
					_rect = GetRectangle();
					return &_rect;
				} else if (cls->GetClassName() == L"@praeformae.I0(Cdordo)(C@praeformae.I0(Cadl)(Cobjectum_dynamicum))") {
					XE_TRY_INTRO
					return GetChildren();
					XE_TRY_OUTRO(0)
				} else if (cls->GetClassName() == L"ingenium.iu._pons_iu") {
					return this;
				} else {
					if (_base->Properties->GetTemplateClass() == L"XE_CustomControlFrame") {
						return static_cast<XCustomControlFrame *>(_base->Properties)->Expose()->DynamicCast(cls, ectx);
					} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
				}
			}
			virtual void * GetType(void) noexcept override { return 0; }
			UI::Template::ControlTemplate * Expose(void) noexcept { return _base; }
			static XControlTemplateMarshaller * Extract(DynamicObject * object, IEngineUIMarshaller * marshaller) noexcept
			{
				ErrorContext ectx;
				ectx.error_code = ectx.error_subcode = 0;
				auto result = reinterpret_cast<XControlTemplateMarshaller *>(object->DynamicCast(static_cast<const XE::ClassSymbol *>(marshaller->internal_type), ectx));
				if (ectx.error_code) return 0;
				return result;
			}
		};
		class XTemplates : public Object
		{
			UI::InterfaceTemplate _ui;
			IEngineUIMarshaller * _marshaller;
		public:
			XTemplates(IEngineUIMarshaller * marshaller) : _marshaller(marshaller) {}
			virtual ~XTemplates(void) override {}
			virtual string ToString(void) const override { try { return L"XTemplates"; } catch (...) { return L""; } }
			virtual string * GetString(const string & key, ErrorContext & ectx) noexcept
			{
				auto r = _ui.Strings.GetElementByKey(key);
				if (r) return r;
				ectx.error_code = 3; ectx.error_subcode = 0; return 0;
			}
			virtual XColor GetColor(const string & key, ErrorContext & ectx) noexcept
			{
				auto r = _ui.Colors.GetElementByKey(key);
				if (r) return XColor(r->Value);
				ectx.error_code = 3; ectx.error_subcode = 0; return 0;
			}
			virtual SafePointer<Object> GetTexture(const string & key, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto r = _ui.Texture.GetObjectByKey(key);
				if (r) return CreateXBitmap(r);
				ectx.error_code = 3; ectx.error_subcode = 0; return 0;
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<Object> GetFont(const string & key, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto r = _ui.Font.GetObjectByKey(key);
				if (r) return CreateXFont(r);
				ectx.error_code = 3; ectx.error_subcode = 0; return 0;
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<XVisualTemplate> GetVisual(const string & key, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto r = _ui.Application.GetObjectByKey(key);
				if (r) return new XVisualTemplate(r, _marshaller);
				else { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<DynamicObject> GetControl(const string & key, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				auto r = _ui.Dialog.GetObjectByKey(key);
				if (r) return new XControlTemplateMarshaller(r);
				else { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				XE_TRY_OUTRO(0)
			}
			UI::InterfaceTemplate & Unwrap(void) noexcept { return _ui; }
		};

		class IXCustomControlCallback
		{
		public:
			virtual void ControlWasCreated(XControlWrapper & control, ErrorContext & ectx) noexcept = 0;
			virtual void ControlWasDestroyed(ErrorContext & ectx) noexcept = 0;
			virtual void RenderControl(XControlWrapper & control, DynamicObject * context, const XRectangle & rect, ErrorContext & ectx) noexcept = 0;
			virtual void ClearRenderingCache(XControlWrapper & control, ErrorContext & ectx) noexcept = 0;
			virtual void RealignChildren(XControlWrapper & control, ErrorContext & ectx) noexcept = 0;
			virtual void ControlWasMoved(XControlWrapper & control, const XRectangle & rect, ErrorContext & ectx) noexcept = 0;
			virtual bool ControlIsEnabled(XControlWrapper & control, ErrorContext & ectx) noexcept = 0;
			virtual bool ControlIsVisible(XControlWrapper & control, ErrorContext & ectx) noexcept = 0;
			virtual int ControlGetID(XControlWrapper & control, ErrorContext & ectx) noexcept = 0;
			virtual XSRectangle ControlGetPosition(XControlWrapper & control, ErrorContext & ectx) noexcept = 0;
			virtual string ControlGetText(XControlWrapper & control, ErrorContext & ectx) noexcept = 0;
			virtual void ControlSetEnabled(XControlWrapper & control, bool value, ErrorContext & ectx) noexcept = 0;
			virtual void ControlSetVisible(XControlWrapper & control, bool value, ErrorContext & ectx) noexcept = 0;
			virtual void ControlSetID(XControlWrapper & control, int id, ErrorContext & ectx) noexcept = 0;
			virtual void ControlSetPosition(XControlWrapper & control, const XSRectangle & rect, ErrorContext & ectx) noexcept = 0;
			virtual void ControlSetText(XControlWrapper & control, const string & text, ErrorContext & ectx) noexcept = 0;
			virtual bool ControlIsTabStop(XControlWrapper & control, ErrorContext & ectx) noexcept = 0;
			virtual XControlWrapper ControlFindChild(XControlWrapper & control, int id, ErrorContext & ectx) noexcept = 0;
			virtual void ControlProcessEvent(XControlWrapper & control, uint id, int origin, XControlWrapper & sender, ErrorContext & ectx) noexcept = 0;
			virtual void ControlFocusChanged(XControlWrapper & control, bool focused, ErrorContext & ectx) noexcept = 0;
			virtual bool ControlKeyPressed(XControlWrapper & control, uint vkc, ErrorContext & ectx) noexcept = 0;
			virtual void ControlKeyReleased(XControlWrapper & control, uint vkc, ErrorContext & ectx) noexcept = 0;
			virtual void ControlCharacterInput(XControlWrapper & control, uint ucs, ErrorContext & ectx) noexcept = 0;
			virtual void ControlCaptureChanged(XControlWrapper & control, bool captured, ErrorContext & ectx) noexcept = 0;
			virtual void ControlMouseMoved(XControlWrapper & control, const XPosition & pos, ErrorContext & ectx) noexcept = 0;
			virtual void ControlLeftButtonPressed(XControlWrapper & control, const XPosition & pos, ErrorContext & ectx) noexcept = 0;
			virtual void ControlLeftButtonReleased(XControlWrapper & control, const XPosition & pos, ErrorContext & ectx) noexcept = 0;
			virtual void ControlLeftButtonDoubleClicked(XControlWrapper & control, const XPosition & pos, ErrorContext & ectx) noexcept = 0;
			virtual void ControlRightButtonPressed(XControlWrapper & control, const XPosition & pos, ErrorContext & ectx) noexcept = 0;
			virtual void ControlRightButtonReleased(XControlWrapper & control, const XPosition & pos, ErrorContext & ectx) noexcept = 0;
			virtual void ControlRightButtonDoubleClicked(XControlWrapper & control, const XPosition & pos, ErrorContext & ectx) noexcept = 0;
			virtual void ControlRotateVertically(XControlWrapper & control, double value, ErrorContext & ectx) noexcept = 0;
			virtual void ControlRotateHorizontally(XControlWrapper & control, double value, ErrorContext & ectx) noexcept = 0;
			virtual void ControlLostExclusiveState(XControlWrapper & control, ErrorContext & ectx) noexcept = 0;
			virtual void ControlMenuWasCancelled(XControlWrapper & control, ErrorContext & ectx) noexcept = 0;
			virtual void ControlTimerFired(XControlWrapper & control, ErrorContext & ectx) noexcept = 0;
			virtual bool ControlGlobalEventProcessible(XControlWrapper & control, uint event, ErrorContext & ectx) noexcept = 0;
			virtual void ControlHandleGlobalEvent(XControlWrapper & control, uint event, ErrorContext & ectx) noexcept = 0;
			virtual XControlWrapper ControlHitTest(XControlWrapper & control, const XPosition & pos, ErrorContext & ectx) noexcept = 0;
			virtual SafePointer<Object> ControlGetCursor(XControlWrapper & control, const XPosition & pos, ErrorContext & ectx) noexcept = 0;
			virtual int ControlGetAnimationMode(XControlWrapper & control, ErrorContext & ectx) noexcept = 0;
			virtual void ControlConfigure(XControlWrapper & control, uint selector, uint io_mode, sintptr index, void * value_ptr, const XE::ClassSymbol * value_cls, ErrorContext & ectx) noexcept = 0;
		};
		class XCustomControl : public UI::Control
		{
			IXCustomControlCallback * _callback;
		public:
			XCustomControl(IXCustomControlCallback * callback) : _callback(callback) { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlWasCreated(w, ectx); }
			virtual ~XCustomControl(void) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; _callback->ControlWasDestroyed(ectx); }
			virtual string ToString(void) const override { try { return L"XCustomControl"; } catch (...) { return L""; } }
			virtual void Render(Graphics::I2DDeviceContext * device, const Box & at) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
				IDeviceControl * devctl;
				GetControlSystemWindow(GetControlSystem())->ExposeInterface(VisualObjectInterfaceDevCtl, &devctl, ectx);
				if (ectx.error_code) return;
				XControlWrapper w(this); _callback->RenderControl(w, devctl->ControlSystemGetDevice(), WrapWObject(at), ectx);
			}
			virtual void ResetCache(void) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ClearRenderingCache(w, ectx); }
			virtual void ArrangeChildren(void) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->RealignChildren(w, ectx); }
			virtual void Enable(bool enable) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlSetEnabled(w, enable, ectx); }
			virtual bool IsEnabled(void) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); auto r = _callback->ControlIsEnabled(w, ectx); return ectx.error_code ? false : r; }
			virtual void Show(bool visible) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlSetVisible(w, visible, ectx); }
			virtual bool IsVisible(void) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); auto r = _callback->ControlIsVisible(w, ectx); return ectx.error_code ? false : r; }
			virtual bool IsTabStop(void) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); auto r = _callback->ControlIsTabStop(w, ectx); return ectx.error_code ? false : r; }
			virtual void SetID(int ID) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlSetID(w, ID, ectx); }
			virtual int GetID(void) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); auto r = _callback->ControlGetID(w, ectx); return ectx.error_code ? 0 : r; }
			virtual Control * FindChild(int ID) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); auto r = reinterpret_cast<UI::Control *>(_callback->ControlFindChild(w, ID, ectx).inner.Inner()); return ectx.error_code ? 0 : r; }
			virtual void SetRectangle(const UI::Rectangle & rect) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlSetPosition(w, WrapWObject(rect), ectx); }
			virtual UI::Rectangle GetRectangle(void) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); auto r = WrapWObject(_callback->ControlGetPosition(w, ectx)); return ectx.error_code ? UI::Rectangle::Invalid() : r; }
			virtual void SetPosition(const Box & box) override { ControlBoundaries = box; ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlWasMoved(w, WrapWObject(ControlBoundaries), ectx); }
			virtual Box GetPosition(void) override { return ControlBoundaries; }
			virtual void SetText(const string & text) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlSetText(w, text, ectx); }
			virtual string GetText(void) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); auto r = _callback->ControlGetText(w, ectx); return ectx.error_code ? L"" : r; }
			virtual void RaiseEvent(int ID, UI::ControlEvent event, Control * sender) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); XControlWrapper s(sender); _callback->ControlProcessEvent(w, ID, int(event), s, ectx); }
			virtual void FocusChanged(bool got_focus) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlFocusChanged(w, got_focus, ectx); }
			virtual void CaptureChanged(bool got_capture) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlCaptureChanged(w, got_capture, ectx); }
			virtual void LostExclusiveMode(void) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlLostExclusiveState(w, ectx); }
			virtual void LeftButtonDown(Point at) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlLeftButtonPressed(w, WrapWObject(at), ectx); }
			virtual void LeftButtonUp(Point at) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlLeftButtonReleased(w, WrapWObject(at), ectx); }
			virtual void LeftButtonDoubleClick(Point at) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlLeftButtonDoubleClicked(w, WrapWObject(at), ectx); }
			virtual void RightButtonDown(Point at) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlRightButtonPressed(w, WrapWObject(at), ectx); }
			virtual void RightButtonUp(Point at) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlRightButtonReleased(w, WrapWObject(at), ectx); }
			virtual void RightButtonDoubleClick(Point at) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlRightButtonDoubleClicked(w, WrapWObject(at), ectx); }
			virtual void MouseMove(Point at) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlMouseMoved(w, WrapWObject(at), ectx); }
			virtual void ScrollVertically(double delta) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlRotateVertically(w, delta, ectx); }
			virtual void ScrollHorizontally(double delta) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlRotateHorizontally(w, delta, ectx); }
			virtual bool KeyDown(int key_code) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); auto r = _callback->ControlKeyPressed(w, key_code, ectx); return ectx.error_code ? true : r; }
			virtual void KeyUp(int key_code) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlKeyReleased(w, key_code, ectx); }
			virtual void CharDown(uint32 ucs_code) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlCharacterInput(w, ucs_code, ectx); }
			virtual void PopupMenuCancelled(void) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlMenuWasCancelled(w, ectx); }
			virtual void Timer(void) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlTimerFired(w, ectx); }
			virtual Control * HitTest(Point at) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); auto r = reinterpret_cast<UI::Control *>(_callback->ControlHitTest(w, WrapWObject(at), ectx).inner.Inner()); return ectx.error_code ? 0 : r; }
			virtual void SetCursor(Point at) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); auto cursor = _callback->ControlGetCursor(w, WrapWObject(at), ectx); if (!ectx.error_code) Windows::GetWindowSystem()->SetCursor(static_cast<Windows::ICursor *>(cursor.Inner())); }
			virtual bool IsWindowEventEnabled(Windows::WindowHandler handler) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); auto r = _callback->ControlGlobalEventProcessible(w, WrapWObject(handler), ectx); return ectx.error_code ? false : r; }
			virtual void HandleWindowEvent(Windows::WindowHandler handler) override { ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); _callback->ControlHandleGlobalEvent(w, WrapWObject(handler), ectx); }
			virtual UI::ControlRefreshPeriod GetFocusedRefreshPeriod(void) override
			{
				ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0; XControlWrapper w(this); auto r = _callback->ControlGetAnimationMode(w, ectx);
				if (ectx.error_code || r == 0) return UI::ControlRefreshPeriod::None;
				else if (r == 1) return UI::ControlRefreshPeriod::CaretBlink;
				else return UI::ControlRefreshPeriod::Cinematic;
			}
			virtual string GetControlClass(void) override { return L"XCustomControl"; }
			void ControlConfigure(uint selector, uint io_mode, sintptr index, void * value_ptr, const XE::ClassSymbol * value_cls, ErrorContext & ectx) noexcept { XControlWrapper w(this); _callback->ControlConfigure(w, selector, io_mode, index, value_ptr, value_cls, ectx); }
		};
		class XControlFactory2 : public UI::IControlFactory
		{
		public:
			XControlFactory2(void) {}
			virtual ~XControlFactory2(void) override {}
			virtual string ToString(void) const override { try { return L"XControlFactory2"; } catch (...) { return L""; } }
			static UI::Control * CreateCustomControl(UI::Template::ControlTemplate * base, ErrorContext & ectx)
			{
				XControlTemplateMarshaller m(base);
				XSRectangle rect = m.GetRectangle();
				SafePointer< ObjectArray<DynamicObject> > children = m.GetChildren();
				auto ctl = static_cast<XCustomControlFrame *>(base->Properties)->Expose()->CreateInstance(rect, *children, ectx);
				if (ectx.error_code) return 0;
				ctl.inner->Retain();
				return static_cast<UI::Control *>(ctl.inner.Inner());
			}
			virtual UI::Control * CreateControl(UI::Template::ControlTemplate * base, IControlFactory * factory) override
			{
				if (base->Properties->GetTemplateClass() == L"XE_CustomControlFrame") {
					try {
						ErrorContext ectx;
						ectx.error_code = ectx.error_subcode = 0;
						auto control = CreateCustomControl(base, ectx);
						if (ectx.error_code) return 0;
						return control;
					} catch (...) { return 0; }
				} else return 0;
			}
		};
		struct XControlSystemDecorations
		{
			int MenuBorderWidth, MenuElementHeight, MenuSeparatorHeight, _unused;
			SafePointer<Object> MenuFont;
			SafePointer<XVisualTemplate> MenuShadow, MenuBackground, MenuArrow, MenuSeparator;
			SafePointer<XVisualTemplate> MenuItemNormal, MenuItemDisabled, MenuItemHot;
			SafePointer<XVisualTemplate> MenuItemNormalChecked, MenuItemDisabledChecked, MenuItemHotChecked;
		};
		struct XEmbeddedEditorDesc
		{
			SafePointer<DynamicObject> base;
			handle _padding;
			XSRectangle rect;
		};
		class XMirrorArgumentProvider : public UI::IArgumentProvider
		{
			void * _data_ptr;
			const XE::ClassSymbol * _data_cls;
		public:
			XMirrorArgumentProvider(void * data_ptr, const XE::ClassSymbol * data_cls) : _data_ptr(data_ptr), _data_cls(data_cls) {}
			virtual void GetArgument(const string & name, int * value) override
			{
				auto tcn = _data_cls->GetFieldType(name);
				auto vptr = _data_cls->GetFieldAddress(name, _data_ptr);
				if (tcn && vptr && *tcn == L"Cint32") *value = *reinterpret_cast<int *>(vptr);
				else if (tcn && vptr && *tcn == L"Cdfrac") *value = *reinterpret_cast<double *>(vptr);
				else *value = 0;
			}
			virtual void GetArgument(const string & name, double * value) override
			{
				auto tcn = _data_cls->GetFieldType(name);
				auto vptr = _data_cls->GetFieldAddress(name, _data_ptr);
				if (tcn && vptr && *tcn == L"Cint32") *value = *reinterpret_cast<int *>(vptr);
				else if (tcn && vptr && *tcn == L"Cdfrac") *value = *reinterpret_cast<double *>(vptr);
				else *value = 0.0;
			}
			virtual void GetArgument(const string & name, Color * value) override
			{
				auto tcn = _data_cls->GetFieldType(name);
				auto vptr = _data_cls->GetFieldAddress(name, _data_ptr);
				if (tcn && vptr && *tcn == L"Cimago.color") *value = *reinterpret_cast<Color *>(vptr);
				else value->Value = 0;
			}
			virtual void GetArgument(const string & name, string * value) override
			{
				auto tcn = _data_cls->GetFieldType(name);
				auto vptr = _data_cls->GetFieldAddress(name, _data_ptr);
				if (tcn && vptr && *tcn == L"Clinea") *value = *reinterpret_cast<string *>(vptr);
				else *value = L"";
			}
			virtual void GetArgument(const string & name, Graphics::IBitmap ** value) override
			{
				auto tcn = _data_cls->GetFieldType(name);
				auto vptr = _data_cls->GetFieldAddress(name, _data_ptr);
				if (tcn && vptr && *tcn == L"C@praeformae.I0(Cadl)(Cgraphicum.pictura)") {
					auto object = reinterpret_cast< SafePointer<Object> * >(vptr)->Inner();
					if (object) {
						*value = GetWrappedBitmap(object);
						(*value)->Retain();
					} else *value = 0;
				} else *value = 0;
			}
			virtual void GetArgument(const string & name, Graphics::IFont ** value) override
			{
				auto tcn = _data_cls->GetFieldType(name);
				auto vptr = _data_cls->GetFieldAddress(name, _data_ptr);
				if (tcn && vptr && *tcn == L"C@praeformae.I0(Cadl)(Cgraphicum.typographica)") {
					auto object = reinterpret_cast< SafePointer<Object> * >(vptr)->Inner();
					if (object) {
						*value = GetWrappedFont(object);
						(*value)->Retain();
					} else *value = 0;
				} else *value = 0;
			}
		};

		class EngineUIExtension : public IAPIExtension, IEngineUIMarshaller, UI::Controls::ToolButtonPart::IToolButtonPartCustomDropDown
		{
			struct _edit_hook : UI::Controls::Edit::IEditHook
			{
				virtual void InitializeContextMenu(Windows::IMenu * menu, UI::Controls::Edit * sender) override
				{
					auto callback = sender->GetControlSystem()->GetCallback();
					if (callback) callback->HandleControlEvent(sender->GetWindow(), sender->GetID(), UI::ControlEvent::ContextClick, sender);
				}
			};
			struct _ml_edit_hook : UI::Controls::MultiLineEdit::IMultiLineEditHook
			{
				virtual void InitializeContextMenu(Windows::IMenu * menu, UI::Controls::MultiLineEdit * sender) override
				{
					auto callback = sender->GetControlSystem()->GetCallback();
					if (callback) callback->HandleControlEvent(sender->GetWindow(), sender->GetID(), UI::ControlEvent::ContextClick, sender);
				}
			};
			struct _rich_edit_hook__menu_link_caret : UI::Controls::RichEdit::IRichEditHook
			{
				string & command_store;
				_rich_edit_hook__menu_link_caret(string & cs) : command_store(cs) {}
				virtual void InitializeContextMenu(Windows::IMenu * menu, UI::Controls::RichEdit * sender) override
				{
					auto callback = sender->GetControlSystem()->GetCallback();
					if (callback) callback->HandleControlEvent(sender->GetWindow(), sender->GetID(), UI::ControlEvent::ContextClick, sender);
				}
				virtual void LinkPressed(const string & resource, UI::Controls::RichEdit * sender) override
				{
					command_store = resource;
					auto callback = sender->GetControlSystem()->GetCallback();
					if (callback) callback->HandleControlEvent(sender->GetWindow(), sender->GetID(), UI::ControlEvent::Command, sender);
				}
				virtual void CaretPositionChanged(UI::Controls::RichEdit * sender) override
				{
					auto callback = sender->GetControlSystem()->GetCallback();
					if (callback) callback->HandleControlEvent(sender->GetWindow(), sender->GetID(), UI::ControlEvent::ValueChange, sender);
				}
			};
			struct _rich_edit_hook__link_caret : UI::Controls::RichEdit::IRichEditHook
			{
				string & command_store;
				_rich_edit_hook__link_caret(string & cs) : command_store(cs) {}
				virtual void LinkPressed(const string & resource, UI::Controls::RichEdit * sender) override
				{
					command_store = resource;
					auto callback = sender->GetControlSystem()->GetCallback();
					if (callback) callback->HandleControlEvent(sender->GetWindow(), sender->GetID(), UI::ControlEvent::Command, sender);
				}
				virtual void CaretPositionChanged(UI::Controls::RichEdit * sender) override
				{
					auto callback = sender->GetControlSystem()->GetCallback();
					if (callback) callback->HandleControlEvent(sender->GetWindow(), sender->GetID(), UI::ControlEvent::ValueChange, sender);
				}
			};
			struct _rich_edit_hook__menu_caret : UI::Controls::RichEdit::IRichEditHook
			{
				virtual void InitializeContextMenu(Windows::IMenu * menu, UI::Controls::RichEdit * sender) override
				{
					auto callback = sender->GetControlSystem()->GetCallback();
					if (callback) callback->HandleControlEvent(sender->GetWindow(), sender->GetID(), UI::ControlEvent::ContextClick, sender);
				}
				virtual void CaretPositionChanged(UI::Controls::RichEdit * sender) override
				{
					auto callback = sender->GetControlSystem()->GetCallback();
					if (callback) callback->HandleControlEvent(sender->GetWindow(), sender->GetID(), UI::ControlEvent::ValueChange, sender);
				}
			};
			struct _rich_edit_hook__menu_link : UI::Controls::RichEdit::IRichEditHook
			{
				string & command_store;
				_rich_edit_hook__menu_link(string & cs) : command_store(cs) {}
				virtual void InitializeContextMenu(Windows::IMenu * menu, UI::Controls::RichEdit * sender) override
				{
					auto callback = sender->GetControlSystem()->GetCallback();
					if (callback) callback->HandleControlEvent(sender->GetWindow(), sender->GetID(), UI::ControlEvent::ContextClick, sender);
				}
				virtual void LinkPressed(const string & resource, UI::Controls::RichEdit * sender) override
				{
					command_store = resource;
					auto callback = sender->GetControlSystem()->GetCallback();
					if (callback) callback->HandleControlEvent(sender->GetWindow(), sender->GetID(), UI::ControlEvent::Command, sender);
				}
			};
			struct _rich_edit_hook__link : UI::Controls::RichEdit::IRichEditHook
			{
				string & command_store;
				_rich_edit_hook__link(string & cs) : command_store(cs) {}
				virtual void LinkPressed(const string & resource, UI::Controls::RichEdit * sender) override
				{
					command_store = resource;
					auto callback = sender->GetControlSystem()->GetCallback();
					if (callback) callback->HandleControlEvent(sender->GetWindow(), sender->GetID(), UI::ControlEvent::Command, sender);
				}
			};
			struct _rich_edit_hook__caret : UI::Controls::RichEdit::IRichEditHook
			{
				virtual void CaretPositionChanged(UI::Controls::RichEdit * sender) override
				{
					auto callback = sender->GetControlSystem()->GetCallback();
					if (callback) callback->HandleControlEvent(sender->GetWindow(), sender->GetID(), UI::ControlEvent::ValueChange, sender);
				}
			};
			struct _rich_edit_hook__menu : UI::Controls::RichEdit::IRichEditHook
			{
				virtual void InitializeContextMenu(Windows::IMenu * menu, UI::Controls::RichEdit * sender) override
				{
					auto callback = sender->GetControlSystem()->GetCallback();
					if (callback) callback->HandleControlEvent(sender->GetWindow(), sender->GetID(), UI::ControlEvent::ContextClick, sender);
				}
			};
		private:
			Point _last_toolbutton_dropdown_position;
			string _last_richedit_command;
			_edit_hook _edit_hook_object;
			_ml_edit_hook _ml_edit_hook_object;
			_rich_edit_hook__menu_link_caret _rich_edit_hook__menu_link_caret_object;
			_rich_edit_hook__link_caret _rich_edit_hook__link_caret_object;
			_rich_edit_hook__menu_caret _rich_edit_hook__menu_caret_object;
			_rich_edit_hook__menu_link _rich_edit_hook__menu_link_object;
			_rich_edit_hook__caret _rich_edit_hook__caret_object;
			_rich_edit_hook__link _rich_edit_hook__link_object;
			_rich_edit_hook__menu _rich_edit_hook__menu_object;
		private:
			UI::Control * _create_control(DynamicObject * base, ErrorContext & ectx)
			{
				auto w = XControlTemplateMarshaller::Extract(base, this);
				if (!w) throw InvalidArgumentException();
				if (w->Expose()->Properties->GetTemplateClass() == L"XE_CustomControlFrame") {
					return XControlFactory2::CreateCustomControl(w->Expose(), ectx);
				} else {
					XControlFactory2 cf;
					auto control = UI::CreateStandardControl(w->Expose(), &cf);
					if (!control) throw InvalidArgumentException();
					return control;
				}
			}
			void _ctl_config_0(UI::Control * ctl, uint sel, uint io_mode, sintptr index, void * data_ptr, const XE::ClassSymbol * data_cls)
			{
				auto ss = sel & 0xFF;
				if ((sel & 0xFF00) == 0x0100) {
					auto c = ctl->As<UI::RootControl>();
					if (ss == 0) {
						auto & act = c->GetAcceleratorTable();
						auto & accel = *reinterpret_cast<XAccelerator *>(data_ptr);
						if (io_mode == io_mode_add) {
							if (index < 0) act.Append(WrapWObject(accel));
							else act.Insert(WrapWObject(accel), index);
						} else if (io_mode == io_mode_remove) act.Remove(index);
						else if (io_mode == io_mode_read) accel = WrapWObject(act[index]);
						else if (io_mode == io_mode_write) act[index] = WrapWObject(accel);
						else throw Exception();
					} else if (ss == 1) {
						auto & act = c->GetAcceleratorTable();
						if (io_mode == io_mode_read) *reinterpret_cast<int *>(data_ptr) = act.Length();
						else throw Exception();
					} else if (ss == 2) {
						if (io_mode == io_mode_add) c->AddDialogStandardAccelerators();
						else throw Exception();
					} else if (ss == 3) {
						auto & vis = *reinterpret_cast< SafePointer<XVisualTemplate> * >(data_ptr);
						if (io_mode == io_mode_read) {
							auto shape = c->GetBackground();
							if (shape) vis = new XVisualTemplate(shape, this);
							else vis.SetReference(0);
						} else if (io_mode == io_mode_write) {
							if (vis) c->SetBackground(vis->Expose()); else c->SetBackground(0);
						} else throw Exception();
					} else throw Exception();
				} else if ((sel & 0xFF00) == 0x0200) {
					auto c = ctl->As<UI::Controls::Static>();
					if (ss == 0) {
						auto & tex = *reinterpret_cast< SafePointer<Object> * >(data_ptr);
						if (io_mode == io_mode_read) {
							auto img = c->GetImage();
							if (img) tex = CreateXBitmap(img); else tex.SetReference(0);
						} else if (io_mode == io_mode_write) {
							if (tex) c->SetImage(GetWrappedBitmap(tex)); else c->SetImage(0);
						} else throw Exception();
					} else throw Exception();
				} else if ((sel & 0xFF00) == 0x0300) {
					auto c = ctl->As<UI::Controls::ProgressBar>();
					if (ss == 0) {
						auto & value = *reinterpret_cast<double *>(data_ptr);
						if (io_mode == io_mode_read) value = c->GetValue();
						else if (io_mode == io_mode_write) c->SetValue(value);
						else throw Exception();
					} else throw Exception();
				} else if ((sel & 0xFF00) == 0x0400) {
					auto c = ctl->As<UI::Controls::ColorView>();
					if (ss == 0) {
						auto & value = *reinterpret_cast<Color *>(data_ptr);
						if (io_mode == io_mode_read) value = c->GetColor();
						else if (io_mode == io_mode_write) c->SetColor(value);
						else throw Exception();
					} else throw Exception();
				} else if ((sel & 0xFF00) == 0x0500) {
					auto c = ctl->As<UI::Controls::ComplexView>();
					if (ss == 0) {
						if (index != 0 && index != 1) throw InvalidArgumentException();
						if (!data_cls) throw InvalidArgumentException();
						if (data_cls->GetClassName() == L"int32") {
							auto & value = *reinterpret_cast<int *>(data_ptr);
							if (io_mode == io_mode_read) value = c->GetInteger(index);
							else if (io_mode == io_mode_write) c->SetInteger(value, index);
							else throw Exception();
						} else if (data_cls->GetClassName() == L"dfrac") {
							auto & value = *reinterpret_cast<double *>(data_ptr);
							if (io_mode == io_mode_read) value = c->GetFloat(index);
							else if (io_mode == io_mode_write) c->SetFloat(value, index);
							else throw Exception();
						} else if (data_cls->GetClassName() == L"imago.color") {
							auto & value = *reinterpret_cast<Color *>(data_ptr);
							if (io_mode == io_mode_read) value = c->GetColor(index);
							else if (io_mode == io_mode_write) c->SetColor(value, index);
							else throw Exception();
						} else if (data_cls->GetClassName() == L"linea") {
							auto & value = *reinterpret_cast<string *>(data_ptr);
							if (io_mode == io_mode_read) value = c->GetText(index);
							else if (io_mode == io_mode_write) c->SetText(value, index);
							else throw Exception();
						} else if (data_cls->GetClassName() == L"@praeformae.I0(Cadl)(Cgraphicum.pictura)") {
							auto & tex = *reinterpret_cast< SafePointer<Object> * >(data_ptr);
							if (io_mode == io_mode_read) {
								auto img = c->GetTexture(index);
								if (img) tex = CreateXBitmap(img); else tex.SetReference(0);
							} else if (io_mode == io_mode_write) {
								if (tex) c->SetTexture(GetWrappedBitmap(tex), index); else c->SetTexture(0, index);
							} else throw Exception();
						} else if (data_cls->GetClassName() == L"@praeformae.I0(Cadl)(Cgraphicum.typographica)") {
							auto & fnt = *reinterpret_cast< SafePointer<Object> * >(data_ptr);
							if (io_mode == io_mode_read) {
								auto font = c->GetFont(index);
								if (font) fnt = CreateXFont(font); else fnt.SetReference(0);
							} else if (io_mode == io_mode_write) {
								if (fnt) c->SetFont(GetWrappedFont(fnt), index); else c->SetFont(0, index);
							} else throw Exception();
						} else throw InvalidArgumentException();
					} else throw Exception();
				} else if ((sel & 0xFF00) == 0x0600) {
					auto c = ctl->As<UI::Controls::ControlGroup>();
					if (ss == 0) {
						if (io_mode == io_mode_add) {
							auto & data = *reinterpret_cast< SafePointer<DynamicObject> * >(data_ptr);
							auto base = data ? XControlTemplateMarshaller::Extract(data, this)->Expose() : 0;
							XControlFactory2 factory;
							c->SetInnerControls(base, &factory);
						} else if (io_mode == io_mode_remove) c->SetInnerControls(0); else throw Exception();
					} else throw Exception();
				} else if ((sel & 0xFF00) == 0x0700) {
					auto c = ctl->As<UI::Controls::RadioButtonGroup>();
					if (ss == 0) {
						if (data_cls->GetClassName() == L"int32") {
							auto & data = *reinterpret_cast<int *>(data_ptr);
							if (io_mode == io_mode_write) c->CheckRadioButton(data);
							else if (io_mode == io_mode_read) data = c->GetCheckedButton();
							else throw Exception();
						} else if (data_cls->GetClassName() == L"ingenium.iu.elementum") {
							auto & data = *reinterpret_cast<XControlWrapper *>(data_ptr);
							if (io_mode == io_mode_write) c->CheckRadioButton(static_cast<UI::Control *>(data.inner.Inner()));
							else if (io_mode == io_mode_read) data.inner.SetRetain(c->FindChild(c->GetCheckedButton()));
							else throw Exception();
						} else throw InvalidArgumentException();
					} else throw Exception();
				} else if ((sel & 0xFF00) == 0x0800) {
					auto c = ctl->As<UI::Controls::ScrollBox>();
					if (ss == 0 && io_mode == io_mode_read) {
						auto & data = *reinterpret_cast<XControlWrapper *>(data_ptr);
						data.inner.SetRetain(c->GetVirtualGroup());
					} else throw Exception();
				} else if ((sel & 0xFF00) == 0x0900) {
					auto c = ctl->As<UI::Controls::SplitBoxPart>();
					if (ss == 0) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_write) { c->Size = data; if (c->GetParent()) c->GetParent()->ArrangeChildren(); }
						else if (io_mode == io_mode_read) data = c->Size;
						else throw Exception();
					} else throw Exception();
				} else if ((sel & 0xFF00) == 0x0A00) {
					auto c = ctl->As<UI::Controls::BookmarkView>();
					if (ss == 0) {
						if (io_mode == io_mode_add) {
							if (data_ptr) {
								auto & data = *reinterpret_cast< SafePointer<DynamicObject> * >(data_ptr);
								auto base = data ? XControlTemplateMarshaller::Extract(data, this)->Expose() : 0;
								XControlFactory2 factory;
								c->AddBookmark(1, 0, L"", base, &factory);
							} else c->AddBookmark(1, 0, L"");
						} else if (io_mode == io_mode_remove) {
							c->RemoveBookmark(index);
						} else throw Exception();
					} else if (ss == 1) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetBookmarkCount();
						else throw Exception();
					} else if (ss == 2) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetActiveBookmark();
						else if (io_mode == io_mode_write) c->ActivateBookmark(data);
						else throw Exception();
					} else if (ss == 3) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_write) c->OrderBookmark(index, data);
						else throw Exception();
					} else if (ss == 4) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) data = c->FindBookmark(index);
						else throw Exception();
					} else if (ss == 5) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetBookmarkID(index);
						else if (io_mode == io_mode_write) c->SetBookmarkID(index, data);
						else throw Exception();
					} else if (ss == 6) {
						auto & data = *reinterpret_cast<string *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetBookmarkText(index);
						else if (io_mode == io_mode_write) c->SetBookmarkText(index, data);
						else throw Exception();
					} else if (ss == 7) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetBookmarkWidth(index);
						else if (io_mode == io_mode_write) c->SetBookmarkWidth(index, data);
						else throw Exception();
					} else if (ss == 8) {
						auto & data = *reinterpret_cast<XControlWrapper *>(data_ptr);
						if (io_mode == io_mode_read) data.inner.SetRetain(c->GetBookmarkView(index));
						else throw Exception();
					} else throw Exception();
				} else if ((sel & 0xFF00) == 0x0B00) {
					auto c = ctl->As<UI::Controls::Button>();
					if (ss == 0) {
						auto & data = *reinterpret_cast< SafePointer<Object> * >(data_ptr);
						if (io_mode == io_mode_read) {
							auto image = c->GetNormalImage();
							data = image ? CreateXBitmap(image) : 0;
						} else if (io_mode == io_mode_write) c->SetNormalImage(data ? GetWrappedBitmap(data) : 0);
						else throw Exception();
					} else if (ss == 1) {
						auto & data = *reinterpret_cast< SafePointer<Object> * >(data_ptr);
						if (io_mode == io_mode_read) {
							auto image = c->GetGrayedImage();
							data = image ? CreateXBitmap(image) : 0;
						} else if (io_mode == io_mode_write) c->SetGrayedImage(data ? GetWrappedBitmap(data) : 0);
						else throw Exception();
					} else throw Exception();
				} else if ((sel & 0xFF00) == 0x0C00) {
					auto c = ctl->As<UI::Controls::CheckBox>();
					if (ss == 0) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = c->IsChecked();
						else if (io_mode == io_mode_write) c->Check(data);
						else throw Exception();
					} else throw Exception();
				} else if ((sel & 0xFF00) == 0x0D00) {
					auto c = ctl->As<UI::Controls::RadioButton>();
					if (ss == 0) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = c->IsChecked();
						else if (io_mode == io_mode_write) c->Check(data);
						else throw Exception();
					} else throw Exception();
				} else if ((sel & 0xFF00) == 0x0E00) {
					auto c = ctl->As<UI::Controls::ToolButtonPart>();
					if (ss == 0) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = c->IsChecked();
						else if (io_mode == io_mode_write) c->Check(data);
						else throw Exception();
					} else if (ss == 1) {
						auto & data = *reinterpret_cast< SafePointer<Object> * >(data_ptr);
						if (io_mode == io_mode_read) {
							auto image = c->GetNormalImage();
							data = image ? CreateXBitmap(image) : 0;
						} else if (io_mode == io_mode_write) c->SetNormalImage(data ? GetWrappedBitmap(data) : 0);
						else throw Exception();
					} else if (ss == 2) {
						auto & data = *reinterpret_cast< SafePointer<Object> * >(data_ptr);
						if (io_mode == io_mode_read) {
							auto image = c->GetGrayedImage();
							data = image ? CreateXBitmap(image) : 0;
						} else if (io_mode == io_mode_write) c->SetGrayedImage(data ? GetWrappedBitmap(data) : 0);
						else throw Exception();
					} else if (ss == 3) {
						auto & data = *reinterpret_cast< SafePointer<Object> * >(data_ptr);
						if (io_mode == io_mode_read) {
							auto menu = c->GetDropDownMenu();
							data = menu ? WrapWMenu(menu) : 0;
						} else if (io_mode == io_mode_write) c->SetDropDownMenu(data ? UnwrapWMenu(data) : 0);
						else throw Exception();
					} else if (ss == 4) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetDropDownCallback() != 0;
						else if (io_mode == io_mode_write) c->SetDropDownCallback(data ? this : 0);
						else throw Exception();
					} else if (ss == 5) {
						auto & data = *reinterpret_cast<XPosition *>(data_ptr);
						if (io_mode == io_mode_read) data = WrapWObject(_last_toolbutton_dropdown_position);
						else throw Exception();
					} else if (ss == 6) {
						if (io_mode == io_mode_write) c->CustomDropDownClosed();
						else throw Exception();
					} else throw Exception();
				} else throw Exception();
			}
			void _ctl_config_1(UI::Control * ctl, uint sel, uint io_mode, sintptr index, void * data_ptr, const XE::ClassSymbol * data_cls)
			{
				auto ss = sel & 0xFF;
				if ((sel & 0xFF00) == 0x0F00) {
					auto c = ctl->As<UI::Controls::Edit>();
					if (ss == 0x00) {
						if (io_mode == io_mode_write) c->Undo();
						else throw Exception();
					} else if (ss == 0x01) {
						if (io_mode == io_mode_write) c->Redo();
						else throw Exception();
					} else if (ss == 0x02) {
						if (io_mode == io_mode_write) c->Cut();
						else throw Exception();
					} else if (ss == 0x03) {
						if (io_mode == io_mode_write) c->Copy();
						else throw Exception();
					} else if (ss == 0x04) {
						if (io_mode == io_mode_write) c->Paste();
						else throw Exception();
					} else if (ss == 0x05) {
						if (io_mode == io_mode_write) c->Delete();
						else throw Exception();
					} else if (ss == 0x06) {
						if (io_mode == io_mode_write) c->ScrollToCaret();
						else throw Exception();
					} else if (ss == 0x07) {
						if (io_mode == io_mode_read) *reinterpret_cast<string *>(data_ptr) = c->GetSelection();
						else throw Exception();
					} else if (ss == 0x08) {
						auto data = reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_write) c->SetSelection(data[0], data[1]);
						else throw Exception();
					} else if (ss == 0x09) {
						auto & data = *reinterpret_cast<string *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetPlaceholder();
						else if (io_mode == io_mode_write) c->SetPlaceholder(data);
						else throw Exception();
					} else if (ss == 0x0A) {
						auto & data = *reinterpret_cast< SafePointer<Object> * >(data_ptr);
						if (io_mode == io_mode_read) {
							auto menu = c->GetContextMenu();
							data = menu ? WrapWMenu(menu) : 0;
						} else if (io_mode == io_mode_write) c->SetContextMenu(data ? UnwrapWMenu(data) : 0);
						else throw Exception();
					} else if (ss == 0x0B) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = c->ReadOnly;
						else if (io_mode == io_mode_write) { c->ReadOnly = data; c->Invalidate(); }
						else throw Exception();
					} else if (ss == 0x0C) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = c->UpperCase;
						else if (io_mode == io_mode_write) c->UpperCase = data;
						else throw Exception();
					} else if (ss == 0x0D) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = c->LowerCase;
						else if (io_mode == io_mode_write) c->LowerCase = data;
						else throw Exception();
					} else if (ss == 0x0E) {
						auto & data = *reinterpret_cast<string *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetCharacterFilter();
						else if (io_mode == io_mode_write) c->SetCharacterFilter(data);
						else throw Exception();
					} else if (ss == 0x0F) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetPasswordMode();
						else if (io_mode == io_mode_write) c->SetPasswordMode(data);
						else throw Exception();
					} else if (ss == 0x10) {
						auto & data = *reinterpret_cast<uint *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetPasswordChar();
						else if (io_mode == io_mode_write) c->SetPasswordChar(data);
						else throw Exception();
					} else if (ss == 0x11) {
						auto & data = *reinterpret_cast<string *>(data_ptr);
						if (io_mode == io_mode_write) data = c->FilterInput(data);
						else throw Exception();
					} else if (ss == 0x12) {
						auto & data = *reinterpret_cast<string *>(data_ptr);
						if (io_mode == io_mode_write) c->Print(data);
						else throw Exception();
					} else if (ss == 0x13) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetHook() != 0;
						else if (io_mode == io_mode_write) c->SetHook(data ? &_edit_hook_object : 0);
						else throw Exception();
					} else throw Exception();
				} else if ((sel & 0xFF00) == 0x1000) {
					auto c = ctl->As<UI::Controls::MultiLineEdit>();
					if (ss == 0x00) {
						if (io_mode == io_mode_write) c->Undo();
						else throw Exception();
					} else if (ss == 0x01) {
						if (io_mode == io_mode_write) c->Redo();
						else throw Exception();
					} else if (ss == 0x02) {
						if (io_mode == io_mode_write) c->Cut();
						else throw Exception();
					} else if (ss == 0x03) {
						if (io_mode == io_mode_write) c->Copy();
						else throw Exception();
					} else if (ss == 0x04) {
						if (io_mode == io_mode_write) c->Paste();
						else throw Exception();
					} else if (ss == 0x05) {
						if (io_mode == io_mode_write) c->Delete();
						else throw Exception();
					} else if (ss == 0x06) {
						if (io_mode == io_mode_write) c->ScrollToCaret();
						else throw Exception();
					} else if (ss == 0x07) {
						if (io_mode == io_mode_read) *reinterpret_cast<string *>(data_ptr) = c->GetSelection();
						else throw Exception();
					} else if (ss == 0x08) {
						auto data = reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_write) c->SetSelection(Point(data[0], data[1]), Point(data[2], data[3]));
						else throw Exception();
					} else if (ss == 0x09) {
						auto & data = *reinterpret_cast< SafePointer<Object> * >(data_ptr);
						if (io_mode == io_mode_read) {
							auto menu = c->GetContextMenu();
							data = menu ? WrapWMenu(menu) : 0;
						} else if (io_mode == io_mode_write) c->SetContextMenu(data ? UnwrapWMenu(data) : 0);
						else throw Exception();
					} else if (ss == 0x0A) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = c->ReadOnly;
						else if (io_mode == io_mode_write) { c->ReadOnly = data; c->Invalidate(); }
						else throw Exception();
					} else if (ss == 0x0B) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = c->UpperCase;
						else if (io_mode == io_mode_write) c->UpperCase = data;
						else throw Exception();
					} else if (ss == 0x0C) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = c->LowerCase;
						else if (io_mode == io_mode_write) c->LowerCase = data;
						else throw Exception();
					} else if (ss == 0x0D) {
						auto & data = *reinterpret_cast<string *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetCharacterFilter();
						else if (io_mode == io_mode_write) c->SetCharacterFilter(data);
						else throw Exception();
					} else if (ss == 0x0E) {
						auto & data = *reinterpret_cast<string *>(data_ptr);
						if (io_mode == io_mode_write) data = c->FilterInput(data);
						else throw Exception();
					} else if (ss == 0x0F) {
						auto & data = *reinterpret_cast<string *>(data_ptr);
						if (io_mode == io_mode_write) c->Print(data);
						else throw Exception();
					} else if (ss == 0x10) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetHook() != 0;
						else if (io_mode == io_mode_write) c->SetHook(data ? &_ml_edit_hook_object : 0);
						else throw Exception();
					} else throw Exception();
				} else if ((sel & 0xFF00) == 0x1100) {
					auto c = ctl->As<UI::Controls::RichEdit>();
					if (ss == 0x00) {
						if (io_mode == io_mode_write) c->Undo();
						else throw Exception();
					} else if (ss == 0x01) {
						if (io_mode == io_mode_write) c->Redo();
						else throw Exception();
					} else if (ss == 0x02) {
						if (io_mode == io_mode_write) c->Cut();
						else throw Exception();
					} else if (ss == 0x03) {
						if (io_mode == io_mode_write) c->Copy();
						else throw Exception();
					} else if (ss == 0x04) {
						if (io_mode == io_mode_write) c->Paste();
						else throw Exception();
					} else if (ss == 0x05) {
						if (io_mode == io_mode_write) c->Delete();
						else throw Exception();
					} else if (ss == 0x06) {
						if (io_mode == io_mode_write) c->ScrollToCaret();
						else throw Exception();
					} else if (ss == 0x07) {
						auto & data = *reinterpret_cast<string *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetAttributedText();
						else if (io_mode == io_mode_write) c->SetAttributedText(data);
						else throw Exception();
					} else if (ss == 0x08) {
						if (io_mode == io_mode_read) *reinterpret_cast<string *>(data_ptr) = c->SerializeSelection();
						else throw Exception();
					} else if (ss == 0x09) {
						if (io_mode == io_mode_read) *reinterpret_cast<string *>(data_ptr) = c->SerializeSelectionAttributed();
						else throw Exception();
					} else if (ss == 0x0A) {
						auto & data = *reinterpret_cast< SafePointer<Object> * >(data_ptr);
						if (io_mode == io_mode_read) {
							auto menu = c->GetContextMenu();
							data = menu ? WrapWMenu(menu) : 0;
						} else if (io_mode == io_mode_write) c->SetContextMenu(data ? UnwrapWMenu(data) : 0);
						else throw Exception();
					} else if (ss == 0x0B) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = c->IsReadOnly();
						else if (io_mode == io_mode_write) c->SetReadOnly(data);
						else throw Exception();
					} else if (ss == 0x0C) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = !c->DontCopyAttributes;
						else if (io_mode == io_mode_write) c->DontCopyAttributes = !data;
						else throw Exception();
					} else if (ss == 0x0D) {
						auto & data = *reinterpret_cast<string *>(data_ptr);
						if (io_mode == io_mode_write) c->Print(data);
						else throw Exception();
					} else if (ss == 0x0E) {
						auto & data = *reinterpret_cast<string *>(data_ptr);
						if (io_mode == io_mode_write) c->PrintAttributed(data);
						else throw Exception();
					} else if (ss >= 0x0F && ss <= 0x11) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						auto hook = c->GetHook();
						bool handle_menus = hook == &_rich_edit_hook__menu_object || hook == &_rich_edit_hook__menu_link_object || hook == &_rich_edit_hook__menu_caret_object || hook == &_rich_edit_hook__menu_link_caret_object;
						bool handle_links = hook == &_rich_edit_hook__link_object || hook == &_rich_edit_hook__menu_link_object || hook == &_rich_edit_hook__link_caret_object || hook == &_rich_edit_hook__menu_link_caret_object;
						bool handle_caret = hook == &_rich_edit_hook__caret_object || hook == &_rich_edit_hook__menu_caret_object || hook == &_rich_edit_hook__link_caret_object || hook == &_rich_edit_hook__menu_link_caret_object;
						if (io_mode == io_mode_read) {
							if (ss == 0x0F) data = handle_menus;
							else if (ss == 0x10) data = handle_links;
							else data = handle_caret;
						} else if (io_mode == io_mode_write) {
							if (ss == 0x0F) handle_menus = data;
							else if (ss == 0x10) handle_links = data;
							else handle_caret = data;
							if (handle_menus) {
								if (handle_links) {
									if (handle_caret) c->SetHook(&_rich_edit_hook__menu_link_caret_object);
									else c->SetHook(&_rich_edit_hook__menu_link_object);
								} else {
									if (handle_caret) c->SetHook(&_rich_edit_hook__menu_caret_object);
									else c->SetHook(&_rich_edit_hook__menu_object);
								}
							} else {
								if (handle_links) {
									if (handle_caret) c->SetHook(&_rich_edit_hook__link_caret_object);
									else c->SetHook(&_rich_edit_hook__link_object);
								} else {
									if (handle_caret) c->SetHook(&_rich_edit_hook__caret_object);
									else c->SetHook(0);
								}
							}
							_last_richedit_command = "";
						} else throw Exception();
					} else if (ss == 0x12) {
						auto & data = *reinterpret_cast<string *>(data_ptr);
						if (io_mode == io_mode_read) { data = _last_richedit_command; _last_richedit_command = ""; }
						else throw Exception();
					} else if (ss == 0x13) {
						if (io_mode == io_mode_read) *reinterpret_cast<int *>(data_ptr) = c->GetFontFaceCount();
						else throw Exception();
					} else if (ss == 0x14) {
						auto & data = *reinterpret_cast<string *>(data_ptr);
						if (io_mode == io_mode_add) c->RegisterFontFace(data);
						else if (io_mode == io_mode_read) data = c->GetFontFaceWithIndex(index);
						else throw Exception();
					} else if (ss == 0x15) {
						if (io_mode == io_mode_read) *reinterpret_cast<bool *>(data_ptr) = c->IsSelectionEmpty();
						else throw Exception();
					} else if (ss == 0x16) {
						auto & data = *reinterpret_cast<uint *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetSelectedTextStyle();
						else if (io_mode == io_mode_write) c->SetSelectedTextStyle(data, index);
						else throw Exception();
					} else if (ss == 0x17) {
						auto & data = *reinterpret_cast<string *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetSelectedTextFontFamily();
						else if (io_mode == io_mode_write) c->SetSelectedTextFontFamily(data);
						else throw Exception();
					} else if (ss == 0x18) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetSelectedTextHeight();
						else if (io_mode == io_mode_write) c->SetSelectedTextHeight(data);
						else throw Exception();
					} else if (ss == 0x19) {
						auto & data = *reinterpret_cast<Color *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetSelectedTextColor();
						else if (io_mode == io_mode_write) c->SetSelectedTextColor(data);
						else throw Exception();
					} else if (ss == 0x1A) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) {
							auto a = c->GetSelectedTextAlignment();
							if (a == Engine::UI::Controls::RichEdit::TextAlignment::Left) data = 0;
							else if (a == Engine::UI::Controls::RichEdit::TextAlignment::Center) data = 1;
							else if (a == Engine::UI::Controls::RichEdit::TextAlignment::Right) data = 2;
							else data = 3;
						} else if (io_mode == io_mode_write) {
							if (data == 0) c->SetSelectedTextAlignment(Engine::UI::Controls::RichEdit::TextAlignment::Left);
							else if (data == 1) c->SetSelectedTextAlignment(Engine::UI::Controls::RichEdit::TextAlignment::Center);
							else if (data == 2) c->SetSelectedTextAlignment(Engine::UI::Controls::RichEdit::TextAlignment::Right);
							else c->SetSelectedTextAlignment(Engine::UI::Controls::RichEdit::TextAlignment::Stretch);
						} else throw Exception();
					} else if (ss == 0x1B) {
						if (io_mode == io_mode_read) *reinterpret_cast<bool *>(data_ptr) = c->IsImageSelected();
						else throw Exception();
					} else if (ss == 0x1C) {
						if (io_mode == io_mode_read) *reinterpret_cast<bool *>(data_ptr) = c->IsLinkSelected();
						else throw Exception();
					} else if (ss == 0x1D) {
						if (io_mode == io_mode_read) *reinterpret_cast<bool *>(data_ptr) = c->IsTableSelected();
						else throw Exception();
					} else if (ss == 0x1E) {
						if (io_mode == io_mode_read) *reinterpret_cast<bool *>(data_ptr) = c->CanCreateLink();
						else throw Exception();
					} else if (ss == 0x1F) {
						if (io_mode == io_mode_read) *reinterpret_cast<bool *>(data_ptr) = c->CanInsertTable();
						else throw Exception();
					} else if (ss == 0x20) {
						auto & data = *reinterpret_cast< SafePointer<Object> * >(data_ptr);
						if (io_mode == io_mode_add) {
							if (!data) throw InvalidArgumentException();
							c->InsertImage(ExtractImageFromXImage(data.Inner()));
						} else if (io_mode == io_mode_read) {
							auto image = c->GetSelectedImage();
							data = image ? CreateXImage(image) : 0;
						} else if (io_mode == io_mode_write) {
							if (!data) throw InvalidArgumentException();
							c->SetSelectedImage(ExtractImageFromXImage(data.Inner()));
						} else throw Exception();
					} else if (ss == 0x21) {
						auto & data = *reinterpret_cast<Point *>(data_ptr);
						if (io_mode == io_mode_read) {
							data.x = c->GetSelectedImageWidth();
							data.y = c->GetSelectedImageHeight();
						} else if (io_mode == io_mode_write) {
							c->SetSelectedImageSize(data.x, data.y);
						} else throw Exception();
					} else if (ss == 0x22) {
						auto & data = *reinterpret_cast<string *>(data_ptr);
						if (io_mode == io_mode_add) c->TransformSelectionIntoLink(data);
						else if (io_mode == io_mode_remove) c->DetransformSelectedLink();
						else if (io_mode == io_mode_read) data = c->GetSelectedLinkResource();
						else if (io_mode == io_mode_write) c->SetSelectedLinkResource(data);
						else throw Exception();
					} else if (ss == 0x23) {
						auto & data = *reinterpret_cast<Point *>(data_ptr);
						if (io_mode == io_mode_add) c->InsertTable(data);
						else throw Exception();
					} else if (ss == 0x24) {
						auto & data = *reinterpret_cast<Point *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetSelectedTableCell();
						else throw Exception();
					} else if (ss == 0x25) {
						auto & data = *reinterpret_cast<Point *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetSelectedTableSize();
						else throw Exception();
					} else if (ss == 0x26) {
						if (io_mode == io_mode_add) c->InsertSelectedTableColumn(index);
						else if (io_mode == io_mode_remove) c->RemoveSelectedTableColumn(index);
						else throw Exception();
					} else if (ss == 0x27) {
						if (io_mode == io_mode_add) c->InsertSelectedTableRow(index);
						else if (io_mode == io_mode_remove) c->RemoveSelectedTableRow(index);
						else throw Exception();
					} else if (ss == 0x28) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetSelectedTableColumnWidth(index);
						else if (io_mode == io_mode_write) {
							if (data) c->SetSelectedTableColumnWidth(data);
							else c->AdjustSelectedTableColumnWidth();
						} else throw Exception();
					} else if (ss == 0x29) {
						auto & data = *reinterpret_cast<Color *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetSelectedTableBorderColor();
						else if (io_mode == io_mode_write) c->SetSelectedTableBorderColor(data);
						else throw Exception();
					} else if (ss == 0x2A) {
						auto & data = *reinterpret_cast<Color *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetSelectedTableCellBackground(Point(index & 0xFFFF, index >> 16));
						else if (io_mode == io_mode_write) c->SetSelectedTableCellBackground(data);
						else throw Exception();
					} else if (ss == 0x2B) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) {
							auto a = c->GetSelectedTableCellVerticalAlignment(Point(index & 0xFFFF, index >> 16));
							if (a == Engine::UI::Controls::RichEdit::TextVerticalAlignment::Top) data = 0;
							else if (a == Engine::UI::Controls::RichEdit::TextVerticalAlignment::Center) data = 1;
							else data = 2;
						} else if (io_mode == io_mode_write) {
							if (data == 0) c->SetSelectedTableCellVerticalAlignment(Engine::UI::Controls::RichEdit::TextVerticalAlignment::Top);
							else if (data == 1) c->SetSelectedTableCellVerticalAlignment(Engine::UI::Controls::RichEdit::TextVerticalAlignment::Center);
							else c->SetSelectedTableCellVerticalAlignment(Engine::UI::Controls::RichEdit::TextVerticalAlignment::Bottom);
						} else throw Exception();
					} else if (ss == 0x2C) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) {
							if (index == 0) data = c->GetSelectedTableBorderWidth();
							else if (index == 1) data = c->GetSelectedTableVerticalBorderWidth();
							else if (index == 2) data = c->GetSelectedTableHorizontalBorderWidth();
							else throw Exception();
						} else if (io_mode == io_mode_write) {
							if (index == 0) c->SetSelectedTableBorderWidth(data);
							else if (index == 1) c->SetSelectedTableVerticalBorderWidth(data);
							else if (index == 2) c->SetSelectedTableHorizontalBorderWidth(data);
							else throw Exception();
						} else throw Exception();
					} else throw Exception();
				} else throw Exception();
			}
			void _ctl_config_2(UI::Control * ctl, uint sel, uint io_mode, sintptr index, void * data_ptr, const XE::ClassSymbol * data_cls)
			{
				auto ss = sel & 0xFF;
				if ((sel & 0xFF00) == 0x1200) {
					auto c = ctl->As<UI::Controls::ListBox>();
					if (ss == 0) {
						if (io_mode == io_mode_add) {
							if (data_cls->GetClassName() == L"linea") {
								if (index < 0) c->AddItem(*reinterpret_cast<string *>(data_ptr)); else c->InsertItem(*reinterpret_cast<string *>(data_ptr), index);
							} else if (data_cls->GetClassName() == L"ingenium.iu.argumentor") {
								XArgumentMarshaller am(reinterpret_cast<XArgumentProvider *>(data_ptr), this);
								if (index < 0) c->AddItem(&am); else c->InsertItem(&am, index);
							} else {
								XMirrorArgumentProvider ma(data_ptr, data_cls);
								if (index < 0) c->AddItem(&ma); else c->InsertItem(&ma, index);
							}
						} else if (io_mode == io_mode_write) {
							if (data_cls->GetClassName() == L"linea") {
								c->ResetItem(index, *reinterpret_cast<string *>(data_ptr));
							} else if (data_cls->GetClassName() == L"ingenium.iu.argumentor") {
								XArgumentMarshaller am(reinterpret_cast<XArgumentProvider *>(data_ptr), this);
								c->ResetItem(index, &am);
							} else {
								XMirrorArgumentProvider ma(data_ptr, data_cls);
								c->ResetItem(index, &ma);
							}
						} else if (io_mode == io_mode_remove) c->RemoveItem(index);
						else throw Exception();
					} else if (ss == 1) {
						if (io_mode == io_mode_read) *reinterpret_cast<int *>(data_ptr) = c->ItemCount();
						else throw Exception();
					} else if (ss == 2) {
						if (io_mode == io_mode_write) c->SwapItems(index, *reinterpret_cast<int *>(data_ptr));
						else throw Exception();
					} else if (ss == 3) {
						if (io_mode == io_mode_write) c->ClearItems();
						else throw Exception();
					} else if (ss == 4) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_add) c->SetSelectedIndex(data, true);
						else if (io_mode == io_mode_write) c->SetSelectedIndex(data, false);
						else if (io_mode == io_mode_read) data = c->GetSelectedIndex();
						else throw Exception();
					} else if (ss == 5) {
						auto & data = *reinterpret_cast<handle *>(data_ptr);
						if (io_mode == io_mode_write) c->SetItemUserData(index, data);
						else if (io_mode == io_mode_read) data = c->GetItemUserData(index);
						else throw Exception();
					} else if (ss == 6) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = c->IsItemSelected(index);
						else if (io_mode == io_mode_write) c->SelectItem(index, data);
						else throw Exception();
					} else if (ss == 7) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = c->MultiChoose;
						else if (io_mode == io_mode_write) c->MultiChoose = data;
						else throw Exception();
					} else if (ss == 8) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetScrollerPosition();
						else if (io_mode == io_mode_write) c->SetScrollerPosition(data);
						else throw Exception();
					} else if (ss == 9) {
						if (io_mode == io_mode_add) {
							auto & data = *reinterpret_cast<XEmbeddedEditorDesc *>(data_ptr);
							auto base = data.base ? XControlTemplateMarshaller::Extract(data.base, this)->Expose() : 0;
							XControlFactory2 factory;
							c->CreateEmbeddedEditor(base, WrapWObject(data.rect), &factory);
						} else if (io_mode == io_mode_read) {
							auto & data = *reinterpret_cast<XControlWrapper *>(data_ptr);
							data.inner.SetRetain(c->GetEmbeddedEditor());
						} else if (io_mode == io_mode_remove) c->CloseEmbeddedEditor(); else throw Exception();
					} else throw Exception();
				} else if ((sel & 0xFF00) == 0x1300) {
					auto c = ctl->As<UI::Controls::ComboBox>();
					if (ss == 0) {
						if (io_mode == io_mode_add) {
							if (data_cls->GetClassName() == L"linea") {
								if (index < 0) c->AddItem(*reinterpret_cast<string *>(data_ptr)); else c->InsertItem(*reinterpret_cast<string *>(data_ptr), index);
							} else if (data_cls->GetClassName() == L"ingenium.iu.argumentor") {
								XArgumentMarshaller am(reinterpret_cast<XArgumentProvider *>(data_ptr), this);
								if (index < 0) c->AddItem(&am); else c->InsertItem(&am, index);
							} else {
								XMirrorArgumentProvider ma(data_ptr, data_cls);
								if (index < 0) c->AddItem(&ma); else c->InsertItem(&ma, index);
							}
						} else if (io_mode == io_mode_write) {
							if (data_cls->GetClassName() == L"linea") {
								c->ResetItem(index, *reinterpret_cast<string *>(data_ptr));
							} else if (data_cls->GetClassName() == L"ingenium.iu.argumentor") {
								XArgumentMarshaller am(reinterpret_cast<XArgumentProvider *>(data_ptr), this);
								c->ResetItem(index, &am);
							} else {
								XMirrorArgumentProvider ma(data_ptr, data_cls);
								c->ResetItem(index, &ma);
							}
						} else if (io_mode == io_mode_remove) c->RemoveItem(index);
						else throw Exception();
					} else if (ss == 1) {
						if (io_mode == io_mode_read) *reinterpret_cast<int *>(data_ptr) = c->ItemCount();
						else throw Exception();
					} else if (ss == 2) {
						if (io_mode == io_mode_write) c->SwapItems(index, *reinterpret_cast<int *>(data_ptr));
						else throw Exception();
					} else if (ss == 3) {
						if (io_mode == io_mode_write) c->ClearItems();
						else throw Exception();
					} else if (ss == 4) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_write) c->SetSelectedIndex(data);
						else if (io_mode == io_mode_read) data = c->GetSelectedIndex();
						else throw Exception();
					} else if (ss == 5) {
						auto & data = *reinterpret_cast<handle *>(data_ptr);
						if (io_mode == io_mode_write) c->SetItemUserData(index, data);
						else if (io_mode == io_mode_read) data = c->GetItemUserData(index);
						else throw Exception();
					} else throw Exception();
				} else if ((sel & 0xFF00) == 0x1400) {
					auto c = ctl->As<UI::Controls::TextComboBox>();
					if (ss == 0x00) {
						if (io_mode == io_mode_add) {
							if (index < 0) c->AddItem(*reinterpret_cast<string *>(data_ptr)); else c->InsertItem(*reinterpret_cast<string *>(data_ptr), index);
						} else if (io_mode == io_mode_write) {
							c->SetItemText(index, *reinterpret_cast<string *>(data_ptr));
						} else if (io_mode == io_mode_read) {
							*reinterpret_cast<string *>(data_ptr) = c->GetItemText(index);
						} else if (io_mode == io_mode_remove) c->RemoveItem(index);
						else throw Exception();
					} else if (ss == 0x01) {
						if (io_mode == io_mode_read) *reinterpret_cast<int *>(data_ptr) = c->ItemCount();
						else throw Exception();
					} else if (ss == 0x02) {
						if (io_mode == io_mode_write) c->SwapItems(index, *reinterpret_cast<int *>(data_ptr));
						else throw Exception();
					} else if (ss == 0x03) {
						if (io_mode == io_mode_write) c->ClearItems();
						else throw Exception();
					} else if (ss == 0x04) {
						if (io_mode == io_mode_write) c->Undo();
						else throw Exception();
					} else if (ss == 0x05) {
						if (io_mode == io_mode_write) c->Redo();
						else throw Exception();
					} else if (ss == 0x06) {
						if (io_mode == io_mode_write) c->Cut();
						else throw Exception();
					} else if (ss == 0x07) {
						if (io_mode == io_mode_write) c->Copy();
						else throw Exception();
					} else if (ss == 0x08) {
						if (io_mode == io_mode_write) c->Paste();
						else throw Exception();
					} else if (ss == 0x09) {
						if (io_mode == io_mode_write) c->Delete();
						else throw Exception();
					} else if (ss == 0x0A) {
						if (io_mode == io_mode_read) *reinterpret_cast<string *>(data_ptr) = c->GetSelection();
						else throw Exception();
					} else if (ss == 0x0B) {
						auto data = reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_write) c->SetSelection(data[0], data[1]);
						else throw Exception();
					} else if (ss == 0x0C) {
						auto & data = *reinterpret_cast<string *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetPlaceholder();
						else if (io_mode == io_mode_write) c->SetPlaceholder(data);
						else throw Exception();
					} else if (ss == 0x0D) {
						auto & data = *reinterpret_cast< SafePointer<Object> * >(data_ptr);
						if (io_mode == io_mode_read) {
							auto menu = c->GetContextMenu();
							data = menu ? WrapWMenu(menu) : 0;
						} else if (io_mode == io_mode_write) c->SetContextMenu(data ? UnwrapWMenu(data) : 0);
						else throw Exception();
					} else if (ss == 0x0E) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = c->UpperCase;
						else if (io_mode == io_mode_write) c->UpperCase = data;
						else throw Exception();
					} else if (ss == 0x0F) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = c->LowerCase;
						else if (io_mode == io_mode_write) c->LowerCase = data;
						else throw Exception();
					} else if (ss == 0x10) {
						auto & data = *reinterpret_cast<string *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetCharacterFilter();
						else if (io_mode == io_mode_write) c->SetCharacterFilter(data);
						else throw Exception();
					} else if (ss == 0x11) {
						auto & data = *reinterpret_cast<string *>(data_ptr);
						if (io_mode == io_mode_write) data = c->FilterInput(data);
						else throw Exception();
					} else if (ss == 0x12) {
						auto & data = *reinterpret_cast<string *>(data_ptr);
						if (io_mode == io_mode_write) c->Print(data);
						else throw Exception();
					} else throw Exception();
				} else if ((sel & 0xFF00) == 0x1500) {
					auto c = ctl->As<UI::Controls::TreeView>();
					if (ss == 0x0) {
						if (io_mode == io_mode_read) *reinterpret_cast<UI::Controls::TreeView::TreeViewItem **>(data_ptr) = c->GetRootItem();
						else throw Exception();
					} else if (ss == 0x1) {
						auto & data = *reinterpret_cast<UI::Controls::TreeView::TreeViewItem **>(data_ptr);
						if (io_mode == io_mode_add) c->SetSelectedItem(data, true);
						else if (io_mode == io_mode_write) c->SetSelectedItem(data, false);
						else if (io_mode == io_mode_read) data = c->GetSelectedItem();
						else throw Exception();
					} else if (ss == 0x2) {
						if (io_mode == io_mode_write) c->ClearItems();
						else throw Exception();
					} else if (ss == 0x3) {
						auto root = reinterpret_cast<UI::Controls::TreeView::TreeViewItem *>(index);
						if (io_mode == io_mode_add) {
							if (data_cls->GetClassName() == L"linea") {
								root->AddItem(*reinterpret_cast<string *>(data_ptr));
							} else if (data_cls->GetClassName() == L"@praeformae.I2(I1(I0(Ctriplex)(Clinea))(C@praeformae.I0(Cadl)(Cgraphicum.pictura)))(C@praeformae.I0(Cadl)(Cgraphicum.pictura))") {
								auto data = reinterpret_cast<handle *>(data_ptr);
								auto title = reinterpret_cast<string *>(data)[0];
								auto img0 = reinterpret_cast< SafePointer<Object> * >(data)[1];
								auto img1 = reinterpret_cast< SafePointer<Object> * >(data)[2];
								root->AddItem(title, img0 ? GetWrappedBitmap(img0) : 0, img1 ? GetWrappedBitmap(img1) : 0);
							} else if (data_cls->GetClassName() == L"ingenium.iu.argumentor") {
								XArgumentMarshaller am(reinterpret_cast<XArgumentProvider *>(data_ptr), this);
								root->AddItem(&am);
							} else {
								XMirrorArgumentProvider ma(data_ptr, data_cls);
								root->AddItem(&ma);
							}
						} else if (io_mode == io_mode_write) {
							if (data_cls->GetClassName() == L"linea") {
								root->Reset(*reinterpret_cast<string *>(data_ptr));
							} else if (data_cls->GetClassName() == L"") {
								auto data = reinterpret_cast<handle *>(data_ptr);
								auto title = reinterpret_cast<string *>(data)[0];
								auto img0 = reinterpret_cast< SafePointer<Object> * >(data)[1];
								auto img1 = reinterpret_cast< SafePointer<Object> * >(data)[2];
								root->Reset(title, img0 ? GetWrappedBitmap(img0) : 0, img1 ? GetWrappedBitmap(img1) : 0);
							} else if (data_cls->GetClassName() == L"ingenium.iu.argumentor") {
								XArgumentMarshaller am(reinterpret_cast<XArgumentProvider *>(data_ptr), this);
								root->Reset(&am);
							} else {
								XMirrorArgumentProvider ma(data_ptr, data_cls);
								root->Reset(&ma);
							}
						} else if (io_mode == io_mode_remove) root->Remove();
						else throw Exception();
					} else if (ss == 0x4) {
						auto root = reinterpret_cast<UI::Controls::TreeView::TreeViewItem *>(index);
						if (io_mode == io_mode_read) *reinterpret_cast<int *>(data_ptr) = root->GetChildrenCount();
						else throw Exception();
					} else if (ss == 0x5) {
						auto item1 = reinterpret_cast<UI::Controls::TreeView::TreeViewItem *>(index);
						auto item2 = *reinterpret_cast<UI::Controls::TreeView::TreeViewItem **>(data_ptr);
						if (item1->GetParent() != item2->GetParent()) throw InvalidArgumentException();
						if (io_mode == io_mode_write) item1->GetParent()->SwapItems(item1->GetIndexAtParent(), item2->GetIndexAtParent());
						else throw Exception();
					} else if (ss == 0x6) {
						auto root = reinterpret_cast<UI::Controls::TreeView::TreeViewItem *>(index);
						auto & data_in = *reinterpret_cast<uintptr *>(data_ptr);
						auto & data_ex = *reinterpret_cast<UI::Controls::TreeView::TreeViewItem **>(data_ptr);
						if (io_mode == io_mode_read) data_ex = root->GetChild(data_in);
						else throw Exception();
					} else if (ss == 0x7) {
						auto root = reinterpret_cast<UI::Controls::TreeView::TreeViewItem *>(index);
						auto & data_ex = *reinterpret_cast<UI::Controls::TreeView::TreeViewItem **>(data_ptr);
						if (io_mode == io_mode_read) data_ex = root->GetParent();
						else throw Exception();
					} else if (ss == 0x8) {
						auto root = reinterpret_cast<UI::Controls::TreeView::TreeViewItem *>(index);
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) data = root->GetIndexAtParent();
						else throw Exception();
					} else if (ss == 0x9) {
						auto root = reinterpret_cast<UI::Controls::TreeView::TreeViewItem *>(index);
						auto & data = *reinterpret_cast<handle *>(data_ptr);
						if (io_mode == io_mode_write) root->User = data;
						else if (io_mode == io_mode_read) data = root->User;
						else throw Exception();
					} else if (ss == 0xA) {
						auto root = reinterpret_cast<UI::Controls::TreeView::TreeViewItem *>(index);
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = root->IsExpanded();
						else if (io_mode == io_mode_write) root->Expand(data);
						else throw Exception();
					} else if (ss == 0xB) {
						auto root = reinterpret_cast<UI::Controls::TreeView::TreeViewItem *>(index);
						auto & data = *reinterpret_cast<XRectangle *>(data_ptr);
						if (io_mode == io_mode_read) data = WrapWObject(root->GetBounds());
						else throw Exception();
					} else if (ss == 0xC) {
						auto root = reinterpret_cast<UI::Controls::TreeView::TreeViewItem *>(index);
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = !root->GetParent() || root->IsAccessible();
						else throw Exception();
					} else if (ss == 0xD) {
						auto root = reinterpret_cast<UI::Controls::TreeView::TreeViewItem *>(index);
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = root->IsParent();
						else throw Exception();
					} else if (ss == 0xE) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetScrollerPosition();
						else if (io_mode == io_mode_write) c->SetScrollerPosition(data);
						else throw Exception();
					} else if (ss == 0xF) {
						if (io_mode == io_mode_add) {
							auto & data = *reinterpret_cast<XEmbeddedEditorDesc *>(data_ptr);
							auto base = data.base ? XControlTemplateMarshaller::Extract(data.base, this)->Expose() : 0;
							XControlFactory2 factory;
							c->CreateEmbeddedEditor(base, WrapWObject(data.rect), &factory);
						} else if (io_mode == io_mode_read) {
							auto & data = *reinterpret_cast<XControlWrapper *>(data_ptr);
							data.inner.SetRetain(c->GetEmbeddedEditor());
						} else if (io_mode == io_mode_remove) c->CloseEmbeddedEditor(); else throw Exception();
					} else throw Exception();
				} else if ((sel & 0xFF00) == 0x1600) {
					auto c = ctl->As<UI::Controls::ListView>();
					if (ss == 0x00) {
						if (io_mode == io_mode_add) {
							auto data = reinterpret_cast< SafePointer<XVisualTemplate> * >(data_ptr);
							c->AddColumn(L"", index, 1, 1, data[0] ? data[0]->Expose() : 0, data[1] ? data[1]->Expose() : 0);
						} else if (io_mode == io_mode_remove) c->RemoveColumn(index);
						else throw Exception();
					} else if (ss == 0x01) {
						if (io_mode == io_mode_write) c->ClearListView();
						else throw Exception();
					} else if (ss == 0x02) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetColumnOrder(index);
						else if (io_mode == io_mode_write) c->OrderColumn(index, data);
						else throw Exception();
					} else if (ss == 0x03) {
						if (io_mode == io_mode_read) *reinterpret_cast<int *>(data_ptr) = c->GetColumns().Length();
						else throw Exception();
					} else if (ss == 0x04) {
						if (io_mode == io_mode_read) *reinterpret_cast<int *>(data_ptr) = c->GetColumns()[index];
						else throw Exception();
					} else if (ss == 0x05) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetColumnWidth(index);
						else if (io_mode == io_mode_write) c->SetColumnWidth(index, data);
						else throw Exception();
					} else if (ss == 0x06) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetColumnMinimalWidth(index);
						else if (io_mode == io_mode_write) c->SetColumnMinimalWidth(index, data);
						else throw Exception();
					} else if (ss == 0x07) {
						auto & data = *reinterpret_cast<string *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetColumnTitle(index);
						else if (io_mode == io_mode_write) c->SetColumnTitle(index, data);
						else throw Exception();
					} else if (ss == 0x08) {
						if (io_mode == io_mode_write) c->SetColumnID(index, *reinterpret_cast<int *>(data_ptr));
						else throw Exception();
					} else if (ss == 0x09) {
						if (io_mode == io_mode_add) {
							if (data_cls->GetClassName() == L"ingenium.iu.argumentor") {
								XArgumentMarshaller am(reinterpret_cast<XArgumentProvider *>(data_ptr), this);
								if (index < 0) c->AddItem(&am); else c->InsertItem(&am, index);
							} else {
								XMirrorArgumentProvider ma(data_ptr, data_cls);
								if (index < 0) c->AddItem(&ma); else c->InsertItem(&ma, index);
							}
						} else if (io_mode == io_mode_write) {
							if (data_cls->GetClassName() == L"ingenium.iu.argumentor") {
								XArgumentMarshaller am(reinterpret_cast<XArgumentProvider *>(data_ptr), this);
								c->ResetItem(index, &am);
							} else {
								XMirrorArgumentProvider ma(data_ptr, data_cls);
								c->ResetItem(index, &ma);
							}
						} else if (io_mode == io_mode_remove) c->RemoveItem(index);
						else throw Exception();
					} else if (ss == 0x0A) {
						if (io_mode == io_mode_read) *reinterpret_cast<int *>(data_ptr) = c->ItemCount();
						else throw Exception();
					} else if (ss == 0x0B) {
						if (io_mode == io_mode_write) c->SwapItems(index, *reinterpret_cast<int *>(data_ptr));
						else throw Exception();
					} else if (ss == 0x0C) {
						if (io_mode == io_mode_write) c->ClearItems();
						else throw Exception();
					} else if (ss == 0x0D) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_add) c->SetSelectedIndex(data, true);
						else if (io_mode == io_mode_write) c->SetSelectedIndex(data, false);
						else if (io_mode == io_mode_read) data = c->GetSelectedIndex();
						else throw Exception();
					} else if (ss == 0x0E) {
						auto & data = *reinterpret_cast<handle *>(data_ptr);
						if (io_mode == io_mode_write) c->SetItemUserData(index, data);
						else if (io_mode == io_mode_read) data = c->GetItemUserData(index);
						else throw Exception();
					} else if (ss == 0x0F) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = c->IsItemSelected(index);
						else if (io_mode == io_mode_write) c->SelectItem(index, data);
						else throw Exception();
					} else if (ss == 0x10) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = c->MultiChoose;
						else if (io_mode == io_mode_write) c->MultiChoose = data;
						else throw Exception();
					} else if (ss == 0x11) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetLastCellID();
						else throw Exception();
					} else if (ss == 0x12) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetScrollerPosition();
						else if (io_mode == io_mode_write) c->SetScrollerPosition(data);
						else throw Exception();
					} else if (ss == 0x13) {
						if (io_mode == io_mode_add) {
							auto & data = *reinterpret_cast<XEmbeddedEditorDesc *>(data_ptr);
							auto base = data.base ? XControlTemplateMarshaller::Extract(data.base, this)->Expose() : 0;
							XControlFactory2 factory;
							c->CreateEmbeddedEditor(base, index, WrapWObject(data.rect), &factory);
						} else if (io_mode == io_mode_read) {
							auto & data = *reinterpret_cast<XControlWrapper *>(data_ptr);
							data.inner.SetRetain(c->GetEmbeddedEditor());
						} else if (io_mode == io_mode_remove) c->CloseEmbeddedEditor(); else throw Exception();
					} else throw Exception();
				} else throw Exception();
			}
			void _ctl_config_3(UI::Control * ctl, uint sel, uint io_mode, sintptr index, void * data_ptr, const XE::ClassSymbol * data_cls)
			{
				auto ss = sel & 0xFF;
				if ((sel & 0xFF00) == 0x1700) {
					auto c = ctl->As<UI::Controls::MenuBar>();
					if (ss == 0x0) {
						if (io_mode == io_mode_add) {
							if (index < 0) c->AppendMenuElement(0, true, L"", 0);
							else c->InsertMenuElement(index, 0, true, L"", 0);
						} else if (io_mode == io_mode_remove) c->RemoveMenuElement(index);
						else throw Exception();
					} else if (ss == 0x1) {
						if (io_mode == io_mode_read) *reinterpret_cast<int *>(data_ptr) = c->GetMenuElementCount();
						else throw Exception();
					} else if (ss == 0x2) {
						if (io_mode == io_mode_write) c->SwapMenuElements(index, *reinterpret_cast<int *>(data_ptr));
						else throw Exception();
					} else if (ss == 0x3) {
						if (io_mode == io_mode_write) c->ClearMenuElements();
						else throw Exception();
					} else if (ss == 0x4) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetMenuElementID(index);
						else if (io_mode == io_mode_write) c->SetMenuElementID(index, data);
						else throw Exception();
					} else if (ss == 0x5) {
						auto & data = *reinterpret_cast< SafePointer<Object> * >(data_ptr);
						if (io_mode == io_mode_read) {
							auto menu = c->GetMenuElementMenu(index);
							data = menu ? WrapWMenu(menu) : 0;
						} else if (io_mode == io_mode_write) c->SetMenuElementMenu(index, data ? UnwrapWMenu(data) : 0);
						else throw Exception();
					} else if (ss == 0x6) {
						auto & data = *reinterpret_cast<string *>(data_ptr);
						if (io_mode == io_mode_read) data = c->GetMenuElementText(index);
						else if (io_mode == io_mode_write) c->SetMenuElementText(index, data);
						else throw Exception();
					} else if (ss == 0x7) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_read) data = c->IsMenuElementEnabled(index);
						else if (io_mode == io_mode_write) c->EnableMenuElement(index, data);
						else throw Exception();
					} else if (ss == 0x8) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) data = c->FindMenuElement(index);
						else throw Exception();
					} else if (ss == 0x9) {
						auto & data = *reinterpret_cast< SafePointer<Object> * >(data_ptr);
						if (io_mode == io_mode_read) {
							auto item = c->FindMenuItem(index);
							data = item ? WrapWMenuItem(item) : 0;
						} else throw Exception();
					} else if (ss == 0xA) {
						auto & data = *reinterpret_cast<bool *>(data_ptr);
						if (io_mode == io_mode_write) c->SetFlushRight(index, data);
						else throw Exception();
					} else throw Exception();
				} else if ((sel & 0xFF00) == 0x1800) {
					auto c = ctl->As<UI::Controls::VerticalScrollBar>();
					if (ss == 0x0) {
						auto data = reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) {
							data[0] = c->RangeMinimal;
							data[1] = c->RangeMaximal;
						} else if (io_mode == io_mode_write) {
							if (index) c->SetRangeSilent(data[0], data[1]);
							else c->SetRange(data[0], data[1]);
						} else throw Exception();
					} else if (ss == 0x1) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) {
							data = c->Position;
						} else if (io_mode == io_mode_write) {
							if (index) c->SetScrollerPositionSilent(data);
							else c->SetScrollerPosition(data);
						} else throw Exception();
					} else if (ss == 0x2) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) {
							data = c->Page;
						} else if (io_mode == io_mode_write) {
							if (index) c->SetPageSilent(data);
							else c->SetPage(data);
						} else throw Exception();
					} else if (ss == 0x3) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) data = c->Line;
						else if (io_mode == io_mode_write) c->Line = data;
						else throw Exception();
					} else throw Exception();
				} else if ((sel & 0xFF00) == 0x1900) {
					auto c = ctl->As<UI::Controls::HorizontalScrollBar>();
					if (ss == 0x0) {
						auto data = reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) {
							data[0] = c->RangeMinimal;
							data[1] = c->RangeMaximal;
						} else if (io_mode == io_mode_write) {
							if (index) c->SetRangeSilent(data[0], data[1]);
							else c->SetRange(data[0], data[1]);
						} else throw Exception();
					} else if (ss == 0x1) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) {
							data = c->Position;
						} else if (io_mode == io_mode_write) {
							if (index) c->SetScrollerPositionSilent(data);
							else c->SetScrollerPosition(data);
						} else throw Exception();
					} else if (ss == 0x2) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) {
							data = c->Page;
						} else if (io_mode == io_mode_write) {
							if (index) c->SetPageSilent(data);
							else c->SetPage(data);
						} else throw Exception();
					} else if (ss == 0x3) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) data = c->Line;
						else if (io_mode == io_mode_write) c->Line = data;
						else throw Exception();
					} else throw Exception();
				} else if ((sel & 0xFF00) == 0x1A00) {
					auto c = ctl->As<UI::Controls::VerticalTrackBar>();
					if (ss == 0x0) {
						auto data = reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) {
							data[0] = c->RangeMinimal;
							data[1] = c->RangeMaximal;
						} else if (io_mode == io_mode_write) {
							if (index) c->SetRangeSilent(data[0], data[1]);
							else c->SetRange(data[0], data[1]);
						} else throw Exception();
					} else if (ss == 0x1) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) {
							data = c->Position;
						} else if (io_mode == io_mode_write) {
							if (index) c->SetTrackerPositionSilent(data);
							else c->SetTrackerPosition(data);
						} else throw Exception();
					} else if (ss == 0x2) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) data = c->Step;
						else if (io_mode == io_mode_write) c->Step = data;
						else throw Exception();
					} else throw Exception();
				} else if ((sel & 0xFF00) == 0x1B00) {
					auto c = ctl->As<UI::Controls::HorizontalTrackBar>();
					if (ss == 0x0) {
						auto data = reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) {
							data[0] = c->RangeMinimal;
							data[1] = c->RangeMaximal;
						} else if (io_mode == io_mode_write) {
							if (index) c->SetRangeSilent(data[0], data[1]);
							else c->SetRange(data[0], data[1]);
						} else throw Exception();
					} else if (ss == 0x1) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) {
							data = c->Position;
						} else if (io_mode == io_mode_write) {
							if (index) c->SetTrackerPositionSilent(data);
							else c->SetTrackerPosition(data);
						} else throw Exception();
					} else if (ss == 0x2) {
						auto & data = *reinterpret_cast<int *>(data_ptr);
						if (io_mode == io_mode_read) data = c->Step;
						else if (io_mode == io_mode_write) c->Step = data;
						else throw Exception();
					} else throw Exception();
				} else throw Exception();
			}
		public:
			EngineUIExtension(StandardLoader & loader) : IEngineUIMarshaller(loader), _rich_edit_hook__menu_link_caret_object(_last_richedit_command), _rich_edit_hook__menu_link_object(_last_richedit_command),
				_rich_edit_hook__link_caret_object(_last_richedit_command), _rich_edit_hook__link_object(_last_richedit_command) {}
			virtual ~EngineUIExtension(void) override {}
			virtual const void * ExposeRoutine(const string & routine_name) noexcept override { return 0; }
			virtual const void * ExposeInterface(const string & interface) noexcept override { if (interface == L"ingenium.pons.iu") return static_cast<IEngineUIMarshaller *>(this); else return 0; }
			virtual uintptr api_level_0(uint32 sel, uintptr a0, uintptr a1, uintptr a2, uintptr a3, uintptr a4, ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				if (sel == 0) {
					auto xstream = reinterpret_cast<XStream *>(a0);
					auto factory = reinterpret_cast<XControlFactory *>(a1);
					SafePointer<Streaming::Stream> stream = WrapFromXStream(xstream);
					SafePointer<XTemplates> result = new XTemplates(this);
					ControlMarshalling::MirrorRegistry mirror;
					XCustomControlFactory ccf(factory, &mirror);
					UI::Loader::LoadUserInterfaceFromBinary(result->Unwrap(), stream, 0, &ccf);
					for (auto & m : mirror) {
						if (m.value.type == Reflection::PropertyType::Rectangle) {
							*reinterpret_cast<XSRectangle *>(m.key) = WrapWObject(m.value.ertrect);
						} else if (m.value.type == Reflection::PropertyType::Font) {
							*reinterpret_cast< SafePointer<Object> *>(m.key) = CreateXFont(static_cast<Graphics::IFont *>(m.value.ertobj.Inner()));
						} else if (m.value.type == Reflection::PropertyType::Texture) {
							*reinterpret_cast< SafePointer<Object> *>(m.key) = CreateXBitmap(static_cast<Graphics::IBitmap *>(m.value.ertobj.Inner()));
						} else if (m.value.type == Reflection::PropertyType::Application) {
							*reinterpret_cast< SafePointer<Object> *>(m.key) = new XVisualTemplate(static_cast<UI::Template::Shape *>(m.value.ertobj.Inner()), this);
						} else if (m.value.type == Reflection::PropertyType::Dialog) {
							*reinterpret_cast< SafePointer<Object> *>(m.key) = new XControlTemplateMarshaller(static_cast<UI::Template::ControlTemplate *>(m.value.ertobj.Inner()));
						}
					}
					result->Retain();
					return reinterpret_cast<uintptr>(result.Inner());
				} else if (sel >= 1 && sel <= 3) {
					if (sel == 1) {
						if (a2) {
							if (!a0) throw InvalidArgumentException();
							auto base = XControlTemplateMarshaller::Extract(reinterpret_cast<DynamicObject *>(a0), this);
							if (!base) throw InvalidArgumentException();
							auto callback = reinterpret_cast<void *>(a1);
							auto rect = WrapWObject(*reinterpret_cast<const XSRectangle *>(a2));
							auto parent = reinterpret_cast<VisualObject *>(a3);
							auto devcls = WrapDeviceClass(a4);
							XControlFactory2 cf;
							return reinterpret_cast<uintptr>(CreateWWindow(window_system, base->Expose(), callback, rect, parent, &cf, devcls, ectx));
						} else {
							auto pdesc = reinterpret_cast<const void *>(a0);
							auto devcls = WrapDeviceClass(a4);
							return reinterpret_cast<uintptr>(CreateWWindow(window_system, pdesc, devcls, ectx));
						}
					} else if (sel == 2) {
						if (a2) {
							if (!a0) throw InvalidArgumentException();
							auto base = XControlTemplateMarshaller::Extract(reinterpret_cast<DynamicObject *>(a0), this);
							if (!base) throw InvalidArgumentException();
							auto callback = reinterpret_cast<void *>(a1);
							auto rect = WrapWObject(*reinterpret_cast<const XSRectangle *>(a2));
							auto parent = reinterpret_cast<VisualObject *>(a3);
							auto devcls = WrapDeviceClass(a4);
							XControlFactory2 cf;
							return reinterpret_cast<uintptr>(CreateModalWWindow(window_system, base->Expose(), callback, rect, parent, &cf, devcls, ectx));
						} else {
							auto pdesc = reinterpret_cast<const void *>(a0);
							auto devcls = WrapDeviceClass(a4);
							return reinterpret_cast<uintptr>(CreateModalWWindow(window_system, pdesc, devcls, ectx));
						}
					} else {
						if (!a0) throw InvalidArgumentException();
						auto base = XControlTemplateMarshaller::Extract(reinterpret_cast<DynamicObject *>(a0), this);
						if (!base) throw InvalidArgumentException();
						return reinterpret_cast<uintptr>(CreateWMenu(base->Expose(), ectx));
					}
				} else if (sel >= 4 && sel <= 6) {
					if (sel == 4) {
						auto callback = reinterpret_cast<IXCustomControlCallback *>(a0);
						if (!callback) throw InvalidArgumentException();
						return reinterpret_cast<uintptr>(new XCustomControl(callback));
					} else if (sel == 5) {
						return reinterpret_cast<uintptr>(_create_control(reinterpret_cast<DynamicObject *>(a0), ectx));
					} else {
						auto control_on = reinterpret_cast<UI::Control *>(a0);
						auto children = reinterpret_cast< ObjectArray<DynamicObject> * >(a1);
						for (auto & c : *children) {
							SafePointer<UI::Control> control_new = _create_control(&c, ectx);
							if (ectx.error_code) return 0;
							if (control_new) control_on->AddChild(control_new);
						}
						return 0;
					}
				} else if (sel >= 7 && sel <= 14) {
					if (sel == 7) {
						auto menu = UnwrapWMenu(reinterpret_cast<Object *>(a0));
						auto control = reinterpret_cast<UI::Control *>(a1);
						auto pos = WrapWObject(*reinterpret_cast<XPosition *>(a2));
						if (!menu || !control) throw InvalidArgumentException();
						UI::RunMenu(menu, control, pos);
						return 0;
					} else if (sel == 12) {
						auto window = reinterpret_cast<VisualObject *>(a0);
						if (!window) throw InvalidArgumentException();
						UI::ControlSystem * cs;
						window->ExposeInterface(VisualObjectInterfaceConSys, &cs, ectx);
						if (ectx.error_code) return 0;
						cs->Retain();
						return reinterpret_cast<uintptr>(cs);
					} else if (sel == 13) {
						auto window = reinterpret_cast<VisualObject *>(a0);
						if (!window) throw InvalidArgumentException();
						int id = a1;
						UI::ControlSystem * cs;
						window->ExposeInterface(VisualObjectInterfaceConSys, &cs, ectx);
						if (ectx.error_code) return 0;
						auto control = cs->GetRootControl()->FindChild(id);
						if (!control) throw InvalidArgumentException();
						control->Retain();
						return reinterpret_cast<uintptr>(control);
					} else if (sel == 14) {
						auto window = reinterpret_cast<VisualObject *>(a0);
						if (!window) throw InvalidArgumentException();
						UI::ControlSystem * cs;
						window->ExposeInterface(VisualObjectInterfaceConSys, &cs, ectx);
						if (ectx.error_code) return 0;
						auto root = cs->GetRootControl();
						root->Retain();
						return reinterpret_cast<uintptr>(root);
					} else throw Exception();
				} else if (sel >= 15 && sel <= 16) {
					auto scale_ptr = reinterpret_cast<double *>(a0);
					auto scale_array_ptr = reinterpret_cast<double *>(a1);
					auto num = a2;
					if (sel == 16) {
						double select = 0.0;
						if (scale_array_ptr) {
							double diff = -1.0;
							for (int i = 0; i < num; i++) {
								auto d = Math::abs(scale_array_ptr[i] - *scale_ptr);
								if (diff < 0.0 || d < diff) { select = scale_array_ptr[i]; diff = d; }
							}
						} else select = *scale_ptr;
						if (isnan(select) || isinf(select) || select <= 0.0) throw InvalidArgumentException();
						UI::CurrentScaleFactor = select;
					} else *scale_ptr = UI::CurrentScaleFactor;
					return 0;
				} else if (sel >= 17) {
					if (sel == 17) {
						if (!int_type) int_type = reinterpret_cast<const void *>(a0);
						if (!double_type) double_type = reinterpret_cast<const void *>(a1);
						if (!color_type) color_type = reinterpret_cast<const void *>(a2);
						if (!string_type) string_type = reinterpret_cast<const void *>(a3);
					} else if (sel == 18) {
						if (!texture_type) texture_type = reinterpret_cast<const void *>(a0);
						if (!font_type) font_type = reinterpret_cast<const void *>(a1);
						if (!internal_type) internal_type = reinterpret_cast<const void *>(a4);
					} else if (sel == 19) {
						window_system = reinterpret_cast<void *>(a0);
					} else throw Exception();
				} else throw Exception();
				XE_TRY_OUTRO(0)
			}
			virtual uintptr api_level_1(Object * obj, uint32 sel, uintptr a0, uintptr a1, uintptr a2, uintptr a3, uintptr a4, ErrorContext & ectx) noexcept override
			{
				auto cs = static_cast<UI::ControlSystem *>(obj);
				uint ss = sel & 0xFF;
				XE_TRY_INTRO
				if (!cs) throw InvalidStateException();
				if ((sel & 0xF00) == 0x000) {
					if (ss == 0x0) cs->SetCaretWidth(a0);
					else if (ss == 0x1) {
						if (a0 == 1) cs->SetRefreshPeriod(UI::ControlRefreshPeriod::CaretBlink);
						else if (a0 == 2) cs->SetRefreshPeriod(UI::ControlRefreshPeriod::Cinematic);
						else cs->SetRefreshPeriod(UI::ControlRefreshPeriod::None);
					} else if (ss == 0x2) {
						auto context = reinterpret_cast<DynamicObject *>(a0);
						IDeviceControl * devctl;
						GetControlSystemWindow(cs)->ExposeInterface(VisualObjectInterfaceDevCtl, &devctl, ectx);
						if (ectx.error_code) return 0;
						devctl->ControlSystemSetDevice(context);
					} else if (ss == 0x3) {
						auto devcls = WrapDeviceClass(a0);
						IDeviceControl * devctl;
						GetControlSystemWindow(cs)->ExposeInterface(VisualObjectInterfaceDevCtl, &devctl, ectx);
						if (ectx.error_code) return 0;
						devctl->ControlSystemSetDeviceClass(devcls);
					} else if (ss == 0x4) cs->SetVirtualClientSize(WrapWObject(*reinterpret_cast<XPosition *>(a0)));
					else if (ss == 0x5) cs->SetVirtualClientAutoresize(a0 != 0);
					else if (ss == 0x6) cs->EnableBeep(a0 != 0);
					else if (ss == 0x7) cs->SetVirtualCursorPosition(WrapWObject(*reinterpret_cast<XPosition *>(a0)));
					else if (ss == 0x8) {
						IDeviceControl * devctl;
						GetControlSystemWindow(cs)->ExposeInterface(VisualObjectInterfaceDevCtl, &devctl, ectx);
						if (ectx.error_code) return 0;
						devctl->ControlSystemSetCallback(reinterpret_cast<void *>(a0));
					} else if (ss == 0xB) cs->SetFocus(reinterpret_cast<UI::Control *>(a0));
					else if (ss == 0xC) {
						if (a0) cs->SetCapture(reinterpret_cast<UI::Control *>(a0));
						else cs->ReleaseCapture();
					} else if (ss == 0xD) cs->SetExclusiveControl(reinterpret_cast<UI::Control *>(a0));
					else throw Exception();
					return 0;
				} else if ((sel & 0xF00) == 0x100) {
					if (ss == 0x0) return cs->GetCaretWidth();
					else if (ss == 0x1) {
						auto a = cs->GetRefreshPeriod();
						if (a == UI::ControlRefreshPeriod::CaretBlink) return 1;
						else if (a == UI::ControlRefreshPeriod::Cinematic) return 2;
						else return 0;
					} else if (ss == 0x2) {
						IDeviceControl * devctl;
						GetControlSystemWindow(cs)->ExposeInterface(VisualObjectInterfaceDevCtl, &devctl, ectx);
						if (ectx.error_code) return 0;
						return reinterpret_cast<uintptr>(devctl->ControlSystemGetDevice());
					} else if (ss == 0x3) {
						IDeviceControl * devctl;
						GetControlSystemWindow(cs)->ExposeInterface(VisualObjectInterfaceDevCtl, &devctl, ectx);
						if (ectx.error_code) return 0;
						return WrapWObject(devctl->ControlSystemGetDeviceClass());
					} else if (ss == 0x4) {
						*reinterpret_cast<XPosition *>(a0) = WrapWObject(cs->GetVirtualClientSize());
						return 0;
					} else if (ss == 0x5) return cs->IsVirtualClientAutoresize();
					else if (ss == 0x6) return cs->IsBeepEnabled();
					else if (ss == 0x7) {
						*reinterpret_cast<XPosition *>(a0) = WrapWObject(cs->GetVirtualCursorPosition());
						return 0;
					} else if (ss == 0x8) {
						IDeviceControl * devctl;
						GetControlSystemWindow(cs)->ExposeInterface(VisualObjectInterfaceDevCtl, &devctl, ectx);
						if (ectx.error_code) return 0;
						return reinterpret_cast<uintptr>(devctl->ControlSystemGetCallback());
					} else if (ss == 0x9) {
						auto root = cs->GetRootControl();
						root->Retain();
						return reinterpret_cast<uintptr>(root);
					} else if (ss == 0xA) return reinterpret_cast<uintptr>(GetControlSystemWindow(cs));
					else if (ss == 0xB) {
						auto ctl = cs->GetFocus();
						if (ctl) ctl->Retain();
						return reinterpret_cast<uintptr>(ctl);
					} else if (ss == 0xC) {
						auto ctl = cs->GetCapture();
						if (ctl) ctl->Retain();
						return reinterpret_cast<uintptr>(ctl);
					} else if (ss == 0xD) {
						auto ctl = cs->GetExclusiveControl();
						if (ctl) ctl->Retain();
						return reinterpret_cast<uintptr>(ctl);
					} else throw Exception();
				} else if ((sel & 0xF00) == 0x200) {
					if (ss == 0x0) {
						auto & dec = *reinterpret_cast<XControlSystemDecorations *>(a0);
						SafePointer<UI::VirtualPopupStyles> pstyles = new UI::VirtualPopupStyles;
						UI::ZeroArgumentProvider zero;
						UI::VirtualPopupStyles & styles = *pstyles;
						styles.MenuBorderWidth = dec.MenuBorderWidth;
						styles.MenuElementDefaultSize = dec.MenuElementHeight;
						styles.MenuSeparatorDefaultSize = dec.MenuSeparatorHeight;
						if (dec.MenuShadow) styles.PopupShadow = dec.MenuShadow->Expose()->Initialize(&zero);
						if (dec.MenuBackground) styles.MenuFrame = dec.MenuBackground->Expose()->Initialize(&zero);
						if (dec.MenuArrow) styles.SubmenuArrow = dec.MenuArrow->Expose()->Initialize(&zero);
						if (dec.MenuSeparator) styles.MenuSeparator = dec.MenuSeparator->Expose()->Initialize(&zero);
						if (dec.MenuFont) styles.MenuItemReferenceFont.SetRetain(GetWrappedFont(dec.MenuFont));
						if (dec.MenuItemNormal) styles.MenuItemNormal.SetRetain(dec.MenuItemNormal->Expose());
						if (dec.MenuItemDisabled) styles.MenuItemGrayed.SetRetain(dec.MenuItemDisabled->Expose());
						if (dec.MenuItemHot) styles.MenuItemHot.SetRetain(dec.MenuItemHot->Expose());
						if (dec.MenuItemNormalChecked) styles.MenuItemNormalChecked.SetRetain(dec.MenuItemNormalChecked->Expose());
						if (dec.MenuItemDisabledChecked) styles.MenuItemGrayedChecked.SetRetain(dec.MenuItemDisabledChecked->Expose());
						if (dec.MenuItemHotChecked) styles.MenuItemHotChecked.SetRetain(dec.MenuItemHotChecked->Expose());
						cs->SetVirtualPopupStyles(pstyles);
						return 0;
					} else if (ss == 0x1) {
						auto cursor = cs->GetSystemCursor(static_cast<Windows::SystemCursorClass>(a0));
						if (cursor) cursor->Retain();
						return reinterpret_cast<uintptr>(cursor);
					} else if (ss == 0x2) {
						cs->OverrideSystemCursor(static_cast<Windows::SystemCursorClass>(a0), reinterpret_cast<Windows::ICursor *>(a1));
						return 0;
					} else if (ss == 0x3 || ss == 0x4) {
						UI::Control * control = 0;
						auto vpos = WrapWObject(*reinterpret_cast<XPosition *>(a0));
						auto lpos = reinterpret_cast<XPosition *>(a1);
						if (ss == 0x3) {
							Point lpos_ert;
							cs->EvaluateControlAt(vpos, &control, &lpos_ert);
							if (lpos) *lpos = WrapWObject(lpos_ert);
						} else control = cs->AccessibleHitTest(vpos);
						if (control) control->Retain();
						return reinterpret_cast<uintptr>(control);
					} else if (ss >= 0x5 && ss <= 0x8) {
						auto p_result = reinterpret_cast<XPosition *>(a0);
						auto relative = reinterpret_cast<UI::Control *>(a1);
						auto input = WrapWObject(*reinterpret_cast<XPosition *>(a2));
						if (!relative && (ss == 5 || ss == 6)) throw InvalidArgumentException();
						Point result;
						if (ss == 5) result = cs->ConvertControlToClient(relative, input);
						else if (ss == 6) result = cs->ConvertClientToControl(relative, input);
						else if (ss == 7) result = cs->ConvertClientVirtualToScreen(input);
						else result = cs->ConvertClientScreenToVirtual(input);
						*p_result = WrapWObject(result);
						return 0;
					} else if (ss >= 0x9 && ss <= 0xB) {
						auto p_rect = reinterpret_cast<XRectangle *>(a0);
						auto time = a1;
						auto initial_ac = WrapAnimationClass(a2);
						auto final_ac = WrapAnimationClass(a3);
						if (ss == 0x9) cs->MoveWindowAnimated(WrapWObject(*p_rect), time, initial_ac, final_ac);
						else if (ss == 0xA) cs->ShowWindowAnimated(time);
						else cs->HideWindowAnimated(time);
						return 0;
					} else if (ss == 0xC) {
						auto target = reinterpret_cast<UI::Control *>(a0);
						auto time = a1;
						if (!target) throw InvalidArgumentException();
						cs->SetTimer(target, time);
						return 0;
					} else if (ss == 0xD) {
						auto dev = cs->GetRenderingDevice();
						if (dev) cs->Render(dev);
						return 0;
					} else if (ss == 0xE) {
						cs->Invalidate();
						return 0;
					} else if (ss == 0xF) {
						cs->Beep();
						return 0;
					} else throw Exception();
				} else throw Exception();
				XE_TRY_OUTRO(0)
			}
			virtual uintptr api_level_2(Object * obj, uint32 sel, uintptr a0, uintptr a1, uintptr a2, uintptr a3, uintptr a4, ErrorContext & ectx) noexcept override
			{
				auto ctl = static_cast<UI::Control *>(obj);
				uint ss = sel & 0xFF;
				XE_TRY_INTRO
				if (!ctl) throw InvalidStateException();
				if ((sel & 0xF00) == 0x000) {
					if (ss == 0x00) ctl->Enable(a0);
					else if (ss == 0x01) ctl->Show(a0);
					else if (ss == 0x02) ctl->SetID(a0);
					else if (ss == 0x03) ctl->SetPosition(WrapWObject(*reinterpret_cast<XRectangle *>(a0)));
					else if (ss == 0x04) ctl->SetRectangle(WrapWObject(*reinterpret_cast<XSRectangle *>(a0)));
					else if (ss == 0x05) ctl->SetText(*reinterpret_cast<string *>(a0));
					else throw Exception();
					return 0;
				} else if ((sel & 0xF00) == 0x100) {
					if (ss >= 0x00 && ss <= 0x05) {
						if (ss == 0x00) return ctl->IsEnabled();
						else if (ss == 0x01) return ctl->IsVisible();
						else if (ss == 0x02) return ctl->GetID();
						else if (ss == 0x03) *reinterpret_cast<XRectangle *>(a0) = WrapWObject(ctl->GetPosition());
						else if (ss == 0x04) *reinterpret_cast<XSRectangle *>(a0) = WrapWObject(ctl->GetRectangle());
						else *reinterpret_cast<string *>(a0) = ctl->GetText();
						return 0;
					} else if (ss >= 0x06 && ss <= 0x0B) {
						if (ss == 0x06) {
							auto parent = ctl->GetParent();
							if (!parent) throw InvalidStateException();
							parent->Retain();
							return reinterpret_cast<uintptr>(parent);
						} else if (ss == 0x07) {
							auto cs = ctl->GetControlSystem();
							if (!cs) throw InvalidStateException();
							cs->Retain();
							return reinterpret_cast<uintptr>(cs);
						} else if (ss == 0x08) {
							auto cs = ctl->GetControlSystem();
							if (!cs) throw InvalidStateException();
							return reinterpret_cast<uintptr>(GetControlSystemWindow(cs));
						} else if (ss == 0x09) {
							auto & array = *reinterpret_cast< Array<XControlWrapper> * >(a0);
							for (int i = 0; i < ctl->ChildrenCount(); i++) {
								XControlWrapper w;
								w.inner.SetRetain(ctl->Child(i));
								array.Append(w);
							}
							return 0;
						} else if (ss == 0x0A) {
							return ctl->GetIndexAtParent();
						} else {
							auto & rect = *reinterpret_cast< XRectangle * >(a0);
							rect = WrapWObject(ctl->GetAbsolutePosition());
							return 0;
						}
					} else if (ss >= 0x0C && ss <= 0x11) {
						if (ss == 0x0C) {
							return ctl->IsAccessible();
						} else if (ss == 0x0D) {
							return ctl->IsHovered();
						} else if (ss == 0x0E) {
							return ctl->IsTabStop();
						} else if (ss == 0x0F) {
							*reinterpret_cast<string *>(a0) = ctl->GetControlClass();
							return 0;
						} else if (ss == 0x10) {
							auto next = ctl->GetNextTabStopControl();
							if (!next) throw InvalidStateException();
							next->Retain();
							return reinterpret_cast<uintptr>(next);
						} else {
							auto prev = ctl->GetPreviousTabStopControl();
							if (!prev) throw InvalidStateException();
							prev->Retain();
							return reinterpret_cast<uintptr>(prev);
						}
					} else throw Exception();
				} else if ((sel & 0xF00) == 0x200) {
					if (ss >= 0x00 && ss <= 0x07) {
						if (ss == 0x00) {
							if (a0 == 0) ctl->SetOrder(UI::ControlDepthOrder::SetFirst);
							else if (a0 == 1) ctl->SetOrder(UI::ControlDepthOrder::SetLast);
							else if (a0 == 2) ctl->SetOrder(UI::ControlDepthOrder::MoveUp);
							else if (a0 == 3) ctl->SetOrder(UI::ControlDepthOrder::MoveDown);
							return 0;
						} else if (ss == 0x01) {
							auto ctl2 = reinterpret_cast<UI::Control *>(a0);
							if (!ctl2) throw InvalidArgumentException();
							ctl->AddChild(ctl2);
							return 0;
						} else if (ss == 0x02) {
							auto ctl2 = reinterpret_cast<UI::Control *>(a0);
							if (!ctl2) throw InvalidArgumentException();
							ctl->RemoveChild(ctl2);
							return 0;
						} else if (ss == 0x03) {
							uint index = a0;
							if (index >= ctl->ChildrenCount()) throw InvalidArgumentException();
							ctl->RemoveChildAt(a0);
							return 0;
						} else if (ss == 0x04 || ss == 0x05) {
							if (ss == 0x04) ctl->RemoveFromParent(); else ctl->DeferredRemoveFromParent();
							return 0;
						} else if (ss == 0x06) {
							auto ctl2 = reinterpret_cast<UI::Control *>(a0);
							if (!ctl2) throw InvalidArgumentException();
							return ctl->GetChildIndex(ctl2);
						} else {
							auto ctl2 = reinterpret_cast<UI::Control *>(a0);
							if (!ctl2) throw InvalidArgumentException();
							return ctl->IsGeneralizedParent(ctl2);
						}
					} else if (ss >= 0x08 && ss <= 0x0A) {
						if (ss == 0x08) {
							auto cs = ctl->GetControlSystem();
							if (!cs) throw InvalidStateException();
							auto rect = WrapWObject(*reinterpret_cast<XRectangle *>(a0));
							ctl->Render(cs->GetRenderingDevice(), rect);
						} else if (ss == 0x09) ctl->ResetCache(); else ctl->ArrangeChildren();
						return 0;
					} else if (ss >= 0x0B && ss <= 0x0D) {
						if (ss == 0x0B) {
							auto ctl2 = ctl->FindChild(a0);
							if (!ctl2) throw InvalidArgumentException();
							ctl2->Retain();
							return reinterpret_cast<uintptr>(ctl2);
						} else if (ss == 0x0C) {
							ctl->RaiseEvent(a0, static_cast<UI::ControlEvent>(a1), reinterpret_cast<UI::Control *>(a2));
							return 0;
						} else {
							ctl->SubmitEvent(a0);
							return 0;
						}
					} else if (ss >= 0x0E && ss <= 0x11) {
						auto time = a1;
						if (ss == 0x0E || ss == 0x0F) {
							auto aci = WrapAnimationClass(a2), acf = WrapAnimationClass(a3);
							auto aact = WrapAnimationAction(a4);
							if (ss == 0x0E) ctl->MoveAnimated(WrapWObject(*reinterpret_cast<XSRectangle *>(a0)), time, aci, acf, aact);
							else ctl->MoveAnimated(WrapWObject(*reinterpret_cast<XRectangle *>(a0)), time, aci, acf, aact);
						} else {
							auto adir = WrapAnimationDirection(a0);
							auto aci = WrapAnimationClass(a2), acf = WrapAnimationClass(a3);
							if (ss == 0x10) ctl->ShowAnimated(adir, time, aci, acf);
							else ctl->HideAnimated(adir, time, aci, acf);
						}
						return 0;
					} else throw Exception();
				} else if (sel == 0x300) {
					uint subsel = a0;
					uint io_mode = a1;
					sintptr index = a2;
					void * data_ptr = reinterpret_cast<void *>(a3);
					auto data_cls = reinterpret_cast<const XE::ClassSymbol *>(a4);
					if (subsel < 0x10000) {
						if (subsel < 0x00F00) _ctl_config_0(ctl, subsel, io_mode, index, data_ptr, data_cls);
						else if (subsel < 0x01200) _ctl_config_1(ctl, subsel, io_mode, index, data_ptr, data_cls);
						else if (subsel < 0x01700) _ctl_config_2(ctl, subsel, io_mode, index, data_ptr, data_cls);
						else _ctl_config_3(ctl, subsel, io_mode, index, data_ptr, data_cls);
					} else ctl->As<XCustomControl>()->ControlConfigure(subsel, io_mode, index, data_ptr, data_cls, ectx);
					return 0;
				} else throw Exception();
				XE_TRY_OUTRO(0)
			}
			virtual bool RunDropDown(UI::Controls::ToolButtonPart * sender, Point top_left) override
			{
				_last_toolbutton_dropdown_position = top_left;
				auto callback = sender->GetControlSystem()->GetCallback();
				if (callback) callback->HandleControlEvent(sender->GetWindow(), sender->GetID(), UI::ControlEvent::ContextClick, sender);
				return true;
			}
		};

		void ActivateEngineUI(StandardLoader & ldr)
		{
			SafePointer<IAPIExtension> ext = new EngineUIExtension(ldr);
			if (!ldr.RegisterAPIExtension(ext)) throw Exception();
		}
	}
}