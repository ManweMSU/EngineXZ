#include "xe_imgapi.h"

#include "xe_interfaces.h"
#include "xe_conapi.h"
#include "../ximg/xi_resources.h"

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
		void * CreateX2DFactory(void);

		struct XColor
		{
			uint32 value;
			XColor(void) {}
			~XColor(void) {}
		};
		class XFrame : public Object
		{
			SafePointer<Codec::Frame> _frame;
		public:
			XFrame(Codec::Frame * frame) { _frame.SetRetain(frame); }
			virtual ~XFrame(void) override {}
			virtual SafePointer<XFrame> Copy(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Codec::Frame> frame = new Codec::Frame(_frame);
				return new XFrame(frame);
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<XFrame> ConvertFormat(Codec::PixelFormat format, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Codec::Frame> frame = _frame->ConvertFormat(format);
				return new XFrame(frame);
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<XFrame> ConvertAlpha(Codec::AlphaMode alpha, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Codec::Frame> frame = _frame->ConvertFormat(alpha);
				return new XFrame(frame);
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<XFrame> ConvertOrigin(Codec::ScanOrigin origin, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Codec::Frame> frame = _frame->ConvertFormat(origin);
				return new XFrame(frame);
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<XFrame> ConvertScanLine(int length, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Codec::Frame> frame = _frame->ConvertFormat(_frame->GetPixelFormat(), length);
				return new XFrame(frame);
				XE_TRY_OUTRO(0)
			}
			virtual int GetWidth(void) noexcept { return _frame->GetWidth(); }
			virtual int GetHeight(void) noexcept { return _frame->GetHeight(); }
			virtual void * GetData(void) noexcept { return _frame->GetData(); }
			virtual XColor ReadPixel(int x, int y) noexcept
			{
				XColor result;
				result.value = _frame->ReadPixel(x, y);
				return result;
			}
			virtual void WritePixel(int x, int y, const XColor & color) noexcept
			{
				_frame->WritePixel(x, y, color.value);
			}
			virtual XColor ReadPalette(int i) noexcept
			{
				XColor result;
				result.value = _frame->ReadPalette(i);
				return result;
			}
			virtual void WritePalette(int i, const XColor & color) noexcept
			{
				if (i >= _frame->GetPaletteVolume()) _frame->SetPaletteVolume(i + 1);
				_frame->WritePalette(i, color.value);
			}
			virtual int GetScanLine(void) noexcept { return _frame->GetScanLineLength(); }
			virtual Codec::PixelFormat GetPixelFormat(void) noexcept { return _frame->GetPixelFormat(); }
			virtual Codec::AlphaMode GetAlphaMode(void) noexcept { return _frame->GetAlphaMode(); }
			virtual Codec::ScanOrigin GetScanOrigin(void) noexcept { return _frame->GetScanOrigin(); }
			virtual int GetHotPointX(void) noexcept { return _frame->HotPointX; }
			virtual void SetHotPointX(const int & value) noexcept { _frame->HotPointX = value; }
			virtual int GetHotPointY(void) noexcept { return _frame->HotPointY; }
			virtual void SetHotPointY(const int & value) noexcept { _frame->HotPointY = value; }
			virtual int GetDuration(void) noexcept { return _frame->Duration; }
			virtual void SetDuration(const int & value) noexcept { _frame->Duration = value; }
			virtual Codec::FrameUsage GetUsage(void) noexcept { return _frame->Usage; }
			virtual void SetUsage(const Codec::FrameUsage & value) noexcept { _frame->Usage = value; }
			virtual double GetScale(void) noexcept { return _frame->DpiUsage; }
			virtual void SetScale(const double & value) noexcept { _frame->DpiUsage = value; }
			Codec::Frame * ExposeFrame(void) noexcept { _frame->Retain(); return _frame; }
		};
		class XImage : public Object
		{
		public:
			ObjectArray<XFrame> Frames;
		public:
			XImage(void) : Frames(0x10) {}
			XImage(Codec::Image * image) : Frames(0x10) { for (auto & f : image->Frames) { SafePointer<XFrame> frame = new XFrame(&f); Frames.Append(frame); } }
			virtual ~XImage(void) override {}
			Codec::Image * ExposeImage(void) noexcept
			{
				SafePointer<Codec::Image> result = new Codec::Image;
				for (auto & f : Frames) {
					SafePointer<Codec::Frame> frame = f.ExposeFrame();
					result->Frames.Append(frame);
				}
				result->Retain();
				return result;
			}
		};

		class XBitmap : public Object
		{
			SafePointer<Graphics::IBitmap> _bitmap;
		public:
			XBitmap(Graphics::IBitmap * bitmap) { _bitmap.SetRetain(bitmap); }
			virtual ~XBitmap(void) override {}
			virtual int GetWidth(void) noexcept { return _bitmap->GetWidth(); }
			virtual int GetHeight(void) noexcept { return _bitmap->GetHeight(); }
			virtual SafePointer<XFrame> GetFrame(void) noexcept
			{
				try {
					SafePointer<Codec::Frame> frame = _bitmap->QueryFrame();
					if (!frame) return 0;
					return new XFrame(frame);
				} catch (...) { return 0; }
			}
			virtual bool SetFrame(XFrame * value) noexcept
			{
				if (!value) return false;
				SafePointer<Codec::Frame> frame = value->ExposeFrame();
				return _bitmap->Reload(frame);
			}
			Graphics::IBitmap * ExposeBitmap(void) noexcept { return _bitmap; }
		};
		class X2DContext : public DynamicObject
		{
			SafePointer<Graphics::I2DDeviceContext> _context;
		protected:
			X2DContext(void) {}
			void SetContext(Graphics::I2DDeviceContext * context) { _context.SetRetain(context); }
		public:
			X2DContext(Graphics::I2DDeviceContext * context) { _context.SetRetain(context); }
			virtual ~X2DContext(void) override {}
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept override
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				if (cls->GetClassName() == L"objectum") {
					Retain(); return static_cast<Object *>(this);
				} else if (cls->GetClassName() == L"objectum_dynamicum") {
					Retain(); return static_cast<DynamicObject *>(this);
				} else if (cls->GetClassName() == L"graphicum.contextus_machinae") {
					Retain(); return static_cast<X2DContext *>(this);
				} else if (cls->GetClassName() == L"graphicum.fabricatio_contextus") {
					try { return CreateX2DFactory(); }
					catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return 0; }
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual void * GetType(void) noexcept override { return 0; }
			virtual void GetImplementation(string * tech, uint * version, uint * feature) noexcept
			{
				string t;
				uint v;
				_context->GetImplementationInfo(t, v);
				if (tech) *tech = t;
				if (version) *version = v;
				if (feature) *feature = _context->GetFeatureList();
			}
			virtual SafePointer<Graphics::IColorBrush> CreateSolidColorBrush(const XColor & color) noexcept { return _context->CreateSolidColorBrush(color.value); }
			virtual SafePointer<Graphics::IColorBrush> CreateGradientBrush(int from_x, int from_y, int to_x, int to_y, const XColor * colors, const double * positions, int length) noexcept
			{
				try {
					Array<GradientPoint> grad(length);
					for (int i = 0; i < length; i++) grad << GradientPoint(colors[i].value, positions[i]);
					return _context->CreateGradientBrush(Point(from_x, from_y), Point(to_x, to_y), grad, length);
				} catch (...) { return 0; }
			}
			virtual SafePointer<Graphics::IBlurEffectBrush> CreateBlurBrush(double value) noexcept { return _context->CreateBlurEffectBrush(value); }
			virtual SafePointer<Graphics::IInversionEffectBrush> CreateInversionBrush(void) noexcept { return _context->CreateInversionEffectBrush(); }
			virtual SafePointer<Graphics::IBitmapBrush> CreateBitmapBrush(XBitmap * bitmap, int left, int top, int right, int bottom, bool tile) noexcept { return _context->CreateBitmapBrush(bitmap->ExposeBitmap(), Box(left, top, right, bottom), tile); }
			virtual SafePointer<Graphics::IBitmapBrush> CreateTextureBrush(VisualObject * texture, uint alpha_mode) noexcept
			{
				Graphics::TextureAlphaMode mode;
				SafePointer<Graphics::ITexture> surface;
				ErrorContext ectx;
				ectx.error_code = ectx.error_subcode = 0;
				if (alpha_mode == 0) mode = Graphics::TextureAlphaMode::Ignore;
				else if (alpha_mode == 1) mode = Graphics::TextureAlphaMode::Premultiplied;
				else return 0;
				texture->ExposeInterface(VisualObjectInterfaceTexture, surface.InnerRef(), ectx);
				if (ectx.error_code) return 0;
				return _context->CreateTextureBrush(surface, mode);
			}
			virtual SafePointer<Graphics::ITextBrush> CreateTextBrush(Graphics::IFont * font, const string & text, int horz, int vert, const XColor & color) noexcept { return _context->CreateTextBrush(font, text, horz, vert, color.value); }
			virtual void ClearCache(void) noexcept { _context->ClearInternalCache(); }
			virtual void PushClip(int left, int top, int right, int bottom) noexcept { _context->PushClip(Box(left, top, right, bottom)); }
			virtual void PopClip(void) noexcept { _context->PopClip(); }
			virtual void PushLayer(int left, int top, int right, int bottom, double opacity) noexcept { _context->BeginLayer(Box(left, top, right, bottom), opacity); }
			virtual void PopLayer(void) noexcept { _context->EndLayer(); }
			virtual void RenderColor(Graphics::IColorBrush * brush, int left, int top, int right, int bottom) noexcept { _context->Render(brush, Box(left, top, right, bottom)); }
			virtual void RenderBitmap(Graphics::IBitmapBrush * brush, int left, int top, int right, int bottom) noexcept { _context->Render(brush, Box(left, top, right, bottom)); }
			virtual void RenderText(Graphics::ITextBrush * brush, int left, int top, int right, int bottom, bool clip) noexcept { _context->Render(brush, Box(left, top, right, bottom), clip); }
			virtual void RenderBlur(Graphics::IBlurEffectBrush * brush, int left, int top, int right, int bottom) noexcept { _context->Render(brush, Box(left, top, right, bottom)); }
			virtual void RenderInversion(Graphics::IInversionEffectBrush * brush, int left, int top, int right, int bottom, bool blink) noexcept { _context->Render(brush, Box(left, top, right, bottom), blink); }
			virtual void RenderPolygon(const double * points, int length, const XColor & color) noexcept { _context->RenderPolygon(reinterpret_cast<const Math::Vector2 *>(points), length, color.value); }
			virtual void RenderPolyline(const double * points, int length, const XColor & color, double weight) noexcept { _context->RenderPolyline(reinterpret_cast<const Math::Vector2 *>(points), length, color.value, weight); }
			virtual uint GetCurrentTime(void) noexcept { return _context->GetAnimationTime(); }
			virtual void SetCurrentTime(const uint & value) noexcept { _context->SetAnimationTime(value); }
			virtual uint GetCaretBaseTime(void) noexcept { return _context->GetCaretReferenceTime(); }
			virtual void SetCaretBaseTime(const uint & value) noexcept { _context->SetCaretReferenceTime(value); }
			virtual uint GetCaretBlinkPeriod(void) noexcept { return _context->GetCaretBlinkPeriod(); }
			virtual void SetCaretBlinkPeriod(const uint & value) noexcept { _context->SetCaretBlinkPeriod(value); }
			virtual bool IsCaretVisible(void) noexcept { return _context->IsCaretVisible(); }
		};
		class XBitmapContext : public X2DContext
		{
			Graphics::IBitmapContext * _dc;
		public:
			XBitmapContext(Graphics::IBitmapContext * context) : X2DContext(context), _dc(context) {}
			virtual ~XBitmapContext(void) override {}
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept override
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				if (cls->GetClassName() == L"objectum") {
					Retain(); return static_cast<Object *>(this);
				} else if (cls->GetClassName() == L"objectum_dynamicum") {
					Retain(); return static_cast<DynamicObject *>(this);
				} else if (cls->GetClassName() == L"graphicum.contextus_machinae") {
					Retain(); return static_cast<X2DContext *>(this);
				} else if (cls->GetClassName() == L"graphicum.contextus_picturae") {
					Retain(); return static_cast<XBitmapContext *>(this);
				} else if (cls->GetClassName() == L"graphicum.fabricatio_contextus") {
					try { return CreateX2DFactory(); }
					catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return 0; }
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual bool BeginRendering(XBitmap * dest) noexcept { return _dc->BeginRendering(dest->ExposeBitmap()); }
			virtual bool BeginRenderingAndClear(XBitmap * dest, const XColor & color) noexcept { return _dc->BeginRendering(dest->ExposeBitmap(), color.value); }
			virtual bool EndRendering(void) noexcept { return _dc->EndRendering(); }
		};
		class XDirectContext : public X2DContext
		{
			SafePointer<Graphics::IDevice> _device;
			SafePointer<Graphics::ITexture> _backbuffer;
			SafePointer<Object> _owner;
			void * _data;
			int _width, _height, _stride;
			SynchronizeRoutine _sync;
		public:
			XDirectContext(void * data, int width, int height, int stride, Object * owner, SynchronizeRoutine sync)
			{
				_owner.SetRetain(owner);
				_data = data;
				_width = width;
				_height = height;
				_stride = stride;
				_sync = sync;
				SafePointer<Graphics::IDeviceFactory> factory = Graphics::CreateDeviceFactory();
				if (!factory) throw OutOfMemoryException();
				_device = factory->CreateDefaultDevice();
				if (!_device) throw OutOfMemoryException();
				Graphics::TextureDesc desc;
				Graphics::ResourceInitDesc init;
				desc.Type = Graphics::TextureType::Type2D;
				desc.Format = Graphics::PixelFormat::B8G8R8A8_unorm;
				desc.Width = _width;
				desc.Height = _height;
				desc.MipmapCount = 1;
				desc.Usage = Graphics::ResourceUsageRenderTarget | Graphics::ResourceUsageShaderRead | Graphics::ResourceUsageCPURead;
				desc.MemoryPool = Graphics::ResourceMemoryPool::Default;
				init.Data = _data;
				init.DataPitch = _stride;
				init.DataSlicePitch = 0;
				_backbuffer = _device->CreateTexture(desc, &init);
				if (!_backbuffer) throw OutOfMemoryException();
				auto context = _device->GetDeviceContext();
				if (context->Begin2DRenderingPass(_backbuffer)) {
					auto ctx_2d = context->Get2DContext();
					SetContext(ctx_2d);
					if (!context->EndCurrentPass()) throw InvalidStateException();
				} else throw InvalidStateException();
			}
			virtual ~XDirectContext(void) override { SetContext(0); }
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept override
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				if (cls->GetClassName() == L"objectum") {
					Retain(); return static_cast<Object *>(this);
				} else if (cls->GetClassName() == L"objectum_dynamicum") {
					Retain(); return static_cast<DynamicObject *>(this);
				} else if (cls->GetClassName() == L"graphicum.contextus_machinae") {
					Retain(); return static_cast<X2DContext *>(this);
				} else if (cls->GetClassName() == L"xx.contextus_consolatorii") {
					Retain(); return static_cast<XDirectContext *>(this);
				} else if (cls->GetClassName() == L"graphicum.fabricatio_contextus") {
					try { return CreateX2DFactory(); }
					catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return 0; }
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual int GetWidth(void) noexcept { return _width; }
			virtual int GetHeight(void) noexcept { return _height; }
			virtual bool BeginRendering(void) noexcept
			{
				auto context = _device->GetDeviceContext();
				return context->Begin2DRenderingPass(_backbuffer);
			}
			virtual bool BeginRenderingAndClear(const XColor & color) noexcept
			{
				auto context = _device->GetDeviceContext();
				Color clr(color.value);
				Graphics::RenderTargetViewDesc rtvd;
				rtvd.Texture = _backbuffer;
				rtvd.LoadAction = Graphics::TextureLoadAction::Clear;
				rtvd.ClearValue[3] = float(clr.a) / 255.0f;
				rtvd.ClearValue[0] = float(clr.r) / 255.0f * rtvd.ClearValue[3];
				rtvd.ClearValue[1] = float(clr.g) / 255.0f * rtvd.ClearValue[3];
				rtvd.ClearValue[2] = float(clr.b) / 255.0f * rtvd.ClearValue[3];
				if (!context->BeginRenderingPass(1, &rtvd, 0)) return false;
				if (!context->EndCurrentPass()) return false;
				return BeginRendering();
			}
			virtual bool EndRendering(void) noexcept
			{
				auto context = _device->GetDeviceContext();
				if (!context->EndCurrentPass()) return false;
				if (!context->BeginMemoryManagementPass()) return false;
				Graphics::ResourceDataDesc desc;
				desc.Data = _data;
				desc.DataPitch = _stride;
				desc.DataSlicePitch = 0;
				context->QueryResourceData(desc, _backbuffer, Graphics::SubresourceIndex(0, 0),
					Graphics::VolumeIndex(0, 0, 0), Graphics::VolumeIndex(_width, _height, 1));
				if (!context->EndCurrentPass()) return false;
				return _sync(_owner);
			}
		};
		class XDeviceContextFactory : public DynamicObject
		{
			SafePointer<Graphics::I2DDeviceContextFactory> _factory;
		public:
			XDeviceContextFactory(void) { _factory = Graphics::CreateDeviceContextFactory(); if (!_factory) throw Exception(); }
			virtual ~XDeviceContextFactory(void) override {}
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept override
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				if (cls->GetClassName() == L"objectum") {
					Retain(); return static_cast<Object *>(this);
				} else if (cls->GetClassName() == L"objectum_dynamicum") {
					Retain(); return static_cast<DynamicObject *>(this);
				} else if (cls->GetClassName() == L"graphicum.fabricatio_contextus") {
					Retain(); return static_cast<XDeviceContextFactory *>(this);
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual void * GetType(void) noexcept override { return 0; }
			virtual SafePointer<XBitmap> CreateBitmap(int w, int h, const XColor & color) noexcept
			{
				try {
					SafePointer<Graphics::IBitmap> bitmap = _factory->CreateBitmap(w, h, color.value);
					if (!bitmap) return 0;
					return new XBitmap(bitmap);
				} catch (...) { return 0; }
			}
			virtual SafePointer<XBitmap> LoadBitmap(XFrame * frame) noexcept
			{
				try {
					if (!frame) return 0;
					SafePointer<Codec::Frame> base = frame->ExposeFrame();
					SafePointer<Graphics::IBitmap> bitmap = _factory->LoadBitmap(base);
					if (!bitmap) return 0;
					return new XBitmap(bitmap);
				} catch (...) { return 0; }
			}
			virtual SafePointer<Graphics::IFont> LoadFont(const string & face, int h, int w, bool i, bool u, bool s) noexcept { return _factory->LoadFont(face, h, w, i, u, s); }
			virtual SafePointer< Array<string> > GetFonts(void) noexcept { return _factory->GetFontFamilies(); }
			virtual SafePointer<XBitmapContext> CreateBitmapContext(void) noexcept
			{
				try {
					SafePointer<Graphics::IBitmapContext> context = _factory->CreateBitmapContext();
					if (!context) return 0;
					return new XBitmapContext(context);
				} catch (...) { return 0; }
			}
		};

		class ImageExtension : public IAPIExtension
		{
			static SafePointer<XFrame> _allocate_frame_0(int w, int h, Codec::PixelFormat format, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Codec::Frame> frame = new Codec::Frame(w, h, format); return new XFrame(frame);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XFrame> _allocate_frame_1(int w, int h, Codec::PixelFormat format, Codec::AlphaMode alpha, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Codec::Frame> frame = new Codec::Frame(w, h, format, alpha); return new XFrame(frame);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XFrame> _allocate_frame_2(int w, int h, Codec::PixelFormat format, Codec::ScanOrigin org, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Codec::Frame> frame = new Codec::Frame(w, h, format, org); return new XFrame(frame);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XFrame> _allocate_frame_3(int w, int h, Codec::PixelFormat format, Codec::AlphaMode alpha, Codec::ScanOrigin org, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Codec::Frame> frame = new Codec::Frame(w, h, format, alpha, org); return new XFrame(frame);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XFrame> _allocate_frame_4(int w, int h, int sl, Codec::PixelFormat format, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Codec::Frame> frame = new Codec::Frame(w, h, format); return new XFrame(frame);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XFrame> _allocate_frame_5(int w, int h, int sl, Codec::PixelFormat format, Codec::AlphaMode alpha, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Codec::Frame> frame = new Codec::Frame(w, h, format, alpha); return new XFrame(frame);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XFrame> _allocate_frame_6(int w, int h, int sl, Codec::PixelFormat format, Codec::ScanOrigin org, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Codec::Frame> frame = new Codec::Frame(w, h, format, org); return new XFrame(frame);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XFrame> _allocate_frame_7(int w, int h, int sl, Codec::PixelFormat format, Codec::AlphaMode alpha, Codec::ScanOrigin org, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Codec::Frame> frame = new Codec::Frame(w, h, format, alpha, org); return new XFrame(frame);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XImage> _query_of_module_0(const Module * mdl, int id, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!mdl) throw InvalidArgumentException();
				SafePointer<Codec::Image> image = XI::LoadModuleIcon(mdl->GetResources(), id);
				if (!image || !image->Frames.Length()) throw IO::FileAccessException(IO::Error::FileNotFound);
				return new XImage(image);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XFrame> _query_of_module_1(const Module * mdl, int id, double scale, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!mdl) throw InvalidArgumentException();
				SafePointer<Codec::Frame> frame = XI::LoadModuleIcon(mdl->GetResources(), id, scale);
				if (!frame) throw IO::FileAccessException(IO::Error::FileNotFound);
				return new XFrame(frame);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XFrame> _query_of_module_2(const Module * mdl, int id, int w, int h, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!mdl) throw InvalidArgumentException();
				SafePointer<Codec::Frame> frame = XI::LoadModuleIcon(mdl->GetResources(), id, Point(w, h));
				if (!frame) throw IO::FileAccessException(IO::Error::FileNotFound);
				return new XFrame(frame);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XFrame> _decode_frame_1(XStream * stream, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!stream) throw InvalidArgumentException();
				SafePointer<Streaming::Stream> unwrapped = WrapFromXStream(stream);
				SafePointer<Codec::Frame> frame = Codec::DecodeFrame(unwrapped);
				if (!frame) throw InvalidFormatException();
				return new XFrame(frame);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XFrame> _decode_frame_2(XStream * stream, string & codec, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!stream) throw InvalidArgumentException();
				SafePointer<Streaming::Stream> unwrapped = WrapFromXStream(stream);
				SafePointer<Codec::Frame> frame = Codec::DecodeFrame(unwrapped, &codec, 0);
				if (!frame) throw InvalidFormatException();
				return new XFrame(frame);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XImage> _decode_image_1(XStream * stream, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!stream) throw InvalidArgumentException();
				SafePointer<Streaming::Stream> unwrapped = WrapFromXStream(stream);
				SafePointer<Codec::Image> image = Codec::DecodeImage(unwrapped);
				if (!image) throw InvalidFormatException();
				return new XImage(image);
				XE_TRY_OUTRO(0)
			}
			static SafePointer<XImage> _decode_image_2(XStream * stream, string & codec, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!stream) throw InvalidArgumentException();
				SafePointer<Streaming::Stream> unwrapped = WrapFromXStream(stream);
				SafePointer<Codec::Image> image = Codec::DecodeImage(unwrapped, &codec, 0);
				if (!image) throw InvalidFormatException();
				return new XImage(image);
				XE_TRY_OUTRO(0)
			}
			static void _encode_frame(XStream * stream, XFrame * frame, const string & codec, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!stream || !frame) throw InvalidArgumentException();
				SafePointer<Streaming::Stream> unwrapped = WrapFromXStream(stream);
				SafePointer<Codec::Frame> picture = frame->ExposeFrame();
				Codec::EncodeFrame(unwrapped, picture, codec);
				XE_TRY_OUTRO()
			}
			static void _encode_image(XStream * stream, XImage * image, const string & codec, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!stream || !image) throw InvalidArgumentException();
				SafePointer<Streaming::Stream> unwrapped = WrapFromXStream(stream);
				SafePointer<Codec::Image> picture = image->ExposeImage();
				Codec::EncodeImage(unwrapped, picture, codec);
				XE_TRY_OUTRO()
			}
			static SafePointer<XDeviceContextFactory> _create_dc_factory(void) noexcept { try { return new XDeviceContextFactory; } catch (...) { return 0; } }
		public:
			ImageExtension(void) {}
			virtual ~ImageExtension(void) override {}
			virtual const void * ExposeRoutine(const string & routine_name) noexcept override
			{
				if (string::Compare(routine_name, L"im_crep") < 0) {
					if (string::Compare(routine_name, L"im_al_4") < 0) {
						if (string::Compare(routine_name, L"im_al_2") < 0) {
							if (string::Compare(routine_name, L"im_al_1") < 0) {
								if (string::Compare(routine_name, L"im_al_0") == 0) return reinterpret_cast<const void *>(_allocate_frame_0);
							} else {
								if (string::Compare(routine_name, L"im_al_1") == 0) return reinterpret_cast<const void *>(_allocate_frame_1);
							}
						} else {
							if (string::Compare(routine_name, L"im_al_3") < 0) {
								if (string::Compare(routine_name, L"im_al_2") == 0) return reinterpret_cast<const void *>(_allocate_frame_2);
							} else {
								if (string::Compare(routine_name, L"im_al_3") == 0) return reinterpret_cast<const void *>(_allocate_frame_3);
							}
						}
					} else {
						if (string::Compare(routine_name, L"im_al_6") < 0) {
							if (string::Compare(routine_name, L"im_al_5") < 0) {
								if (string::Compare(routine_name, L"im_al_4") == 0) return reinterpret_cast<const void *>(_allocate_frame_4);
							} else {
								if (string::Compare(routine_name, L"im_al_5") == 0) return reinterpret_cast<const void *>(_allocate_frame_5);
							}
						} else {
							if (string::Compare(routine_name, L"im_al_7") < 0) {
								if (string::Compare(routine_name, L"im_al_6") == 0) return reinterpret_cast<const void *>(_allocate_frame_6);
							} else {
								if (string::Compare(routine_name, L"im_ccol") < 0) {
									if (string::Compare(routine_name, L"im_al_7") == 0) return reinterpret_cast<const void *>(_allocate_frame_7);
								} else {
									if (string::Compare(routine_name, L"im_ccol") == 0) return reinterpret_cast<const void *>(_encode_image);
								}
							}
						}
					}
				} else {
					if (string::Compare(routine_name, L"im_dr_1") < 0) {
						if (string::Compare(routine_name, L"im_dc_1") < 0) {
							if (string::Compare(routine_name, L"im_crfc") < 0) {
								if (string::Compare(routine_name, L"im_crep") == 0) return reinterpret_cast<const void *>(_encode_frame);
							} else {
								if (string::Compare(routine_name, L"im_crfc") == 0) return reinterpret_cast<const void *>(_create_dc_factory);
							}
						} else {
							if (string::Compare(routine_name, L"im_dc_2") < 0) {
								if (string::Compare(routine_name, L"im_dc_1") == 0) return reinterpret_cast<const void *>(_decode_image_1);
							} else {
								if (string::Compare(routine_name, L"im_dc_2") == 0) return reinterpret_cast<const void *>(_decode_image_2);
							}
						}
					} else {
						if (string::Compare(routine_name, L"im_md_0") < 0) {
							if (string::Compare(routine_name, L"im_dr_2") < 0) {
								if (string::Compare(routine_name, L"im_dr_1") == 0) return reinterpret_cast<const void *>(_decode_frame_1);
							} else {
								if (string::Compare(routine_name, L"im_dr_2") == 0) return reinterpret_cast<const void *>(_decode_frame_2);
							}
						} else {
							if (string::Compare(routine_name, L"im_md_1") < 0) {
								if (string::Compare(routine_name, L"im_md_0") == 0) return reinterpret_cast<const void *>(_query_of_module_0);
							} else {
								if (string::Compare(routine_name, L"im_md_2") < 0) {
									if (string::Compare(routine_name, L"im_md_1") == 0) return reinterpret_cast<const void *>(_query_of_module_1);
								} else {
									if (string::Compare(routine_name, L"im_md_2") == 0) return reinterpret_cast<const void *>(_query_of_module_2);
								}
							}
						}
					}
				}
				return 0;
			}
			virtual const void * ExposeInterface(const string & interface) noexcept override { return 0; }
		};

		void * CreateX2DFactory(void) { return new XDeviceContextFactory; }
		void ActivateImageIO(StandardLoader & ldr)
		{
			SafePointer<IAPIExtension> ext = new ImageExtension;
			if (!ldr.RegisterAPIExtension(ext)) throw Exception();
		}
		Codec::Frame * ExtractFrameFromXFrame(handle xframe) { return reinterpret_cast<XFrame *>(xframe)->ExposeFrame(); }
		Codec::Image * ExtractImageFromXImage(handle ximage) { return reinterpret_cast<XImage *>(ximage)->ExposeImage(); }
		Object * CreateXFrame(Codec::Frame * frame) { return new XFrame(frame); }
		Object * CreateDirectContext(void * data, int width, int height, int stride, Object * owner, SynchronizeRoutine sync) { return new XDirectContext(data, width, height, stride, owner, sync); }
	}
}