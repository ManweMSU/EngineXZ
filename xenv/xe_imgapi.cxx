#include "xe_imgapi.h"

#include "xe_interfaces.h"
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

		XColor::XColor(void) {}
		XColor::XColor(uint v) : value(v) {}
		XColor::~XColor(void) {}

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

		class XResourceHandle : public Object
		{
			SafePointer<Graphics::IDeviceResourceHandle> _handle;
		public:
			XResourceHandle(Graphics::IDeviceResourceHandle * src) { _handle.SetRetain(src); }
			virtual ~XResourceHandle(void) override {}
			virtual uint64 GetDeviceIdentifier(void) noexcept { return _handle->GetDeviceIdentifier(); }
			virtual SafePointer<DataBlock> GetData(void) noexcept { return _handle->Serialize(); }
			virtual string ToString(void) const override { try { return _handle->ToString(); } catch (...) { return L""; } }
			Graphics::IDeviceResourceHandle * Expose(void) noexcept { return _handle; }
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
			virtual string ToString(void) const override { try { return _bitmap->ToString(); } catch (...) { return L""; } }
			Graphics::IBitmap * ExposeBitmap(void) noexcept { return _bitmap; }
		};
		class XPipelineState : public DynamicObject
		{
			SafePointer<Graphics::IPipelineState> _state;
			Object * _device;
		public:
			XPipelineState(Graphics::IPipelineState * state, Object * device) : _device(device) { _state.SetRetain(state); }
			virtual ~XPipelineState(void) override {}
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept override
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				if (cls->GetClassName() == L"objectum") {
					Retain(); return static_cast<Object *>(this);
				} else if (cls->GetClassName() == L"objectum_dynamicum") {
					Retain(); return static_cast<DynamicObject *>(this);
				} else if (cls->GetClassName() == L"graphicum.machinatio") {
					_device->Retain(); return _device;
				} else if (cls->GetClassName() == L"graphicum.status_oleiductus") {
					Retain(); return this;
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual void * GetType(void) noexcept override { return 0; }
			virtual string ToString(void) const override { try { return _state->ToString(); } catch (...) { return L""; } }
			Graphics::IPipelineState * Expose(void) noexcept { return _state; }
		};
		class XSamplerState : public DynamicObject
		{
			SafePointer<Graphics::ISamplerState> _state;
			Object * _device;
		public:
			XSamplerState(Graphics::ISamplerState * state, Object * device) : _device(device) { _state.SetRetain(state); }
			virtual ~XSamplerState(void) override {}
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept override
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				if (cls->GetClassName() == L"objectum") {
					Retain(); return static_cast<Object *>(this);
				} else if (cls->GetClassName() == L"objectum_dynamicum") {
					Retain(); return static_cast<DynamicObject *>(this);
				} else if (cls->GetClassName() == L"graphicum.machinatio") {
					_device->Retain(); return _device;
				} else if (cls->GetClassName() == L"graphicum.status_exceptandi") {
					Retain(); return this;
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual void * GetType(void) noexcept override { return 0; }
			virtual string ToString(void) const override { try { return _state->ToString(); } catch (...) { return L""; } }
			Graphics::ISamplerState * Expose(void) noexcept { return _state; }
		};
		class XResource : public VisualObject
		{
			SafePointer<Graphics::IDeviceResource> _resource;
			Object * _device;
		protected:
			void UpdateResource(Graphics::IDeviceResource * resource) { _resource.SetRetain(resource); }
		public:
			XResource(Graphics::IDeviceResource * resource, Object * device) : _device(device) { _resource.SetRetain(resource); }
			virtual ~XResource(void) override {}
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept override
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				if (cls->GetClassName() == L"objectum") {
					Retain(); return static_cast<Object *>(this);
				} else if (cls->GetClassName() == L"objectum_dynamicum") {
					Retain(); return static_cast<DynamicObject *>(this);
				} else if (cls->GetClassName() == L"graphicum.machinatio") {
					_device->Retain(); return _device;
				} else if (cls->GetClassName() == L"graphicum.auxilium_machinationis") {
					Retain(); return this;
				} else if (cls->GetClassName() == L"graphicum.series" && _resource->GetResourceType() == Graphics::ResourceType::Buffer) {
					Retain(); return this;
				} else if (cls->GetClassName() == L"graphicum.textura" && _resource->GetResourceType() == Graphics::ResourceType::Texture) {
					Retain(); return this;
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual void * GetType(void) noexcept override { return 0; }
			virtual void ExposeInterface(uint intid, void * data, ErrorContext & ectx) noexcept override
			{
				if (!data) { ectx.error_code = 3; return; }
				if (intid == VisualObjectInterfaceBuffer && _resource->GetResourceType() == Graphics::ResourceType::Buffer) {
					*reinterpret_cast<Graphics::IDeviceResource **>(data) = _resource.Inner();
					_resource->Retain();
				} else if (intid == VisualObjectInterfaceTexture && _resource->GetResourceType() == Graphics::ResourceType::Texture) {
					*reinterpret_cast<Graphics::IDeviceResource **>(data) = _resource.Inner();
					_resource->Retain();
				} else ectx.error_code = 1;
			}
			virtual uint GetResourceClass(void) noexcept { return uint(_resource->GetResourceType()); }
			virtual uint GetResourceMemory(void) noexcept { return uint(_resource->GetMemoryPool()); }
			virtual uint GetResourceFlags(void) noexcept { return uint(_resource->GetResourceUsage()); }
			virtual string ToString(void) const override { try { return _resource->ToString(); } catch (...) { return L""; } }
			Graphics::IDeviceResource * Expose(void) noexcept { return _resource; }
		};
		class XBuffer : public XResource
		{
			Graphics::IBuffer * _buffer;
		public:
			XBuffer(Graphics::IBuffer * resource, Object * device) : XResource(resource, device), _buffer(resource) {}
			virtual ~XBuffer(void) override {}
			virtual int GetLength(void) noexcept { return _buffer->GetLength(); }
			Graphics::IBuffer * Expose(void) noexcept { return _buffer; }
		};
		class XTexture : public XResource
		{
			Graphics::ITexture * _texture;
		public:
			XTexture(Graphics::ITexture * resource, Object * device) : XResource(resource, device), _texture(resource) {}
			virtual ~XTexture(void) override {}
			virtual uint GetTextureClass(void) noexcept { return uint(_texture->GetTextureType()); }
			virtual uint GetTextureFormat(void) noexcept { return uint(_texture->GetPixelFormat()); }
			virtual int GetWidth(void) noexcept { return _texture->GetWidth(); }
			virtual int GetHeight(void) noexcept { return _texture->GetHeight(); }
			virtual int GetDepth(void) noexcept { return _texture->GetDepth(); }
			virtual int GetSubdetailLevel(void) noexcept { return _texture->GetMipmapCount(); }
			virtual int GetVolume(void) noexcept { return _texture->GetArraySize(); }
			Graphics::ITexture * Expose(void) noexcept { return _texture; }
			void UpdateTexture(Graphics::ITexture * resource) { _texture = resource; UpdateResource(resource); }
		};
		class XWindowSurface : public DynamicObject
		{
			SafePointer<Graphics::IWindowLayer> _surface;
			SafePointer<XTexture> _texture;
			Object * _device;
		public:
			XWindowSurface(Graphics::IWindowLayer * surface, Object * device) : _device(device) { _surface.SetRetain(surface); }
			virtual ~XWindowSurface(void) override {}
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept override
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				if (cls->GetClassName() == L"objectum") {
					Retain(); return static_cast<Object *>(this);
				} else if (cls->GetClassName() == L"objectum_dynamicum") {
					Retain(); return static_cast<DynamicObject *>(this);
				} else if (cls->GetClassName() == L"graphicum.machinatio") {
					_device->Retain(); return _device;
				} else if (cls->GetClassName() == L"graphicum.fundus_fenestrae") {
					Retain(); return this;
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual void * GetType(void) noexcept override { return 0; }
			virtual bool Present(void) noexcept
			{
				if (!_texture || _texture->GetReferenceCount() > 1) return false;
				_texture->UpdateTexture(0);
				return _surface->Present();
			}
			virtual bool Resize(int width, int height) noexcept
			{
				if (!_texture || _texture->GetReferenceCount() > 1) return false;
				_texture->UpdateTexture(0);
				return _surface->ResizeSurface(width, height);
			}
			virtual SafePointer<XTexture> GetSurface(void) noexcept
			{
				if (!_texture) {
					SafePointer<Graphics::ITexture> surface = _surface->QuerySurface();
					if (!surface) return 0;
					try { _texture = new XTexture(surface, _device); } catch (...) { return 0; }
					return _texture;
				} else {
					SafePointer<Graphics::ITexture> surface = _surface->QuerySurface();
					_texture->UpdateTexture(surface);
					if (surface) return _texture; else return 0;
				}
			}
			virtual bool IsFullscreen(void) noexcept { return _surface->IsFullscreen(); }
			virtual void SetFullscreen(const bool & set, ErrorContext & ectx) noexcept
			{
				if (set) { if (!_surface->SwitchToFullscreen()) ectx.error_code = 5; }
				else { if (!_surface->SwitchToWindow()) ectx.error_code = 5; }
			}
			virtual uint GetAttributes(void) noexcept { return uint(_surface->GetLayerAttributes()); }
			virtual string ToString(void) const override { try { return _surface->ToString(); } catch (...) { return L""; } }
		};
		class XDeviceContext : public DynamicObject
		{
			SafePointer<DynamicObject> _context_2d;
			Graphics::IDeviceContext * _context;
			Object * _device;
		public:
			XDeviceContext(Object * wrapper, Graphics::IDevice * device) : _device(wrapper), _context(device->GetDeviceContext()) {}
			virtual ~XDeviceContext(void) override {}
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept override
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				if (cls->GetClassName() == L"objectum") {
					Retain(); return static_cast<Object *>(this);
				} else if (cls->GetClassName() == L"objectum_dynamicum") {
					Retain(); return static_cast<DynamicObject *>(this);
				} else if (cls->GetClassName() == L"graphicum.contextus_machinae") {
					if (_context_2d) return _context_2d->DynamicCast(cls, ectx);
					else { ectx.error_code = 5; ectx.error_subcode = 0; return 0; }
				} else if (cls->GetClassName() == L"graphicum.fabricatio_contextus") {
					try { return CreateX2DFactory(); }
					catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return 0; }
				} else if (cls->GetClassName() == L"graphicum.machinatio") {
					_device->Retain(); return _device;
				} else if (cls->GetClassName() == L"graphicum.contextus_machinationis") {
					Retain(); return static_cast<XDeviceContext *>(this);
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual void * GetType(void) noexcept override { return 0; }
			virtual bool BeginRegularPass(int num_dest, const uint8 * color_desc, const Graphics::DepthStencilViewDesc * depth_desc) noexcept
			{
				Graphics::RenderTargetViewDesc rtvd[8];
				Graphics::DepthStencilViewDesc dsvd;
				if (!color_desc) return false;
				uint word_size = sizeof(handle);
				uint desc_size = word_size + 24;
				for (int i = 0; i < num_dest; i++) {
					auto desc = reinterpret_cast<const Graphics::RenderTargetViewDesc *>(color_desc + desc_size * i);
					auto texture = reinterpret_cast<XTexture *>(desc->Texture);
					if (!texture) return false;
					rtvd[i].Texture = texture->Expose();
					rtvd[i].LoadAction = desc->LoadAction;
					MemoryCopy(&rtvd[i].ClearValue, &desc->ClearValue, sizeof(rtvd[i].ClearValue));
				}
				if (depth_desc) {
					auto texture = reinterpret_cast<XTexture *>(depth_desc->Texture);
					if (!texture) return false;
					dsvd.Texture = texture->Expose();
					dsvd.DepthLoadAction = depth_desc->DepthLoadAction;
					dsvd.StencilLoadAction = depth_desc->StencilLoadAction;
					dsvd.DepthClearValue = depth_desc->DepthClearValue;
					dsvd.StencilClearValue = depth_desc->StencilClearValue;
				} else dsvd.Texture = 0;
				return _context->BeginRenderingPass(num_dest, rtvd, dsvd.Texture ? &dsvd : 0);
			}
			virtual bool BeginPlanarPass(const Graphics::RenderTargetViewDesc & desc) noexcept
			{
				Graphics::RenderTargetViewDesc rtvd;
				auto texture = reinterpret_cast<XTexture *>(desc.Texture);
				if (!texture) return false;
				rtvd.Texture = texture->Expose();
				rtvd.LoadAction = desc.LoadAction;
				MemoryCopy(&rtvd.ClearValue, &desc.ClearValue, sizeof(rtvd.ClearValue));
				bool result = _context->Begin2DRenderingPass(rtvd);
				if (result && !_context_2d) try { _context_2d = WrapContext(_context->Get2DContext(), this); } catch (...) { _context->EndCurrentPass(); return false; }
				return result;
			}
			virtual bool BeginMemoryPass(void) noexcept { return _context->BeginMemoryManagementPass(); }
			virtual bool EndPass(void) noexcept { return _context->EndCurrentPass(); }
			virtual void Flush(void) noexcept { _context->Flush(); }
			virtual void SetPipelineState(XPipelineState * state) noexcept { if (state) _context->SetRenderingPipelineState(state->Expose()); }
			virtual void SetViewport(float left, float top, float width, float height, float mindepth, float maxdepth) noexcept { _context->SetViewport(left, top, width, height, mindepth, maxdepth); }
			virtual void SetVResource(int selector, XResource * rsrc) noexcept { _context->SetVertexShaderResource(selector, rsrc ? rsrc->Expose() : 0); }
			virtual void SetVConstantR(int selector, XBuffer * rsrc) noexcept { _context->SetVertexShaderConstant(selector, rsrc ? rsrc->Expose() : 0); }
			virtual void SetVConstantD(int selector, const void * data, int length) noexcept { _context->SetVertexShaderConstant(selector, data, length); }
			virtual void SetVSamplerState(int selector, XSamplerState * state) noexcept { _context->SetVertexShaderSamplerState(selector, state ? state->Expose() : 0); }
			virtual void SetPResource(int selector, XResource * rsrc) noexcept { _context->SetPixelShaderResource(selector, rsrc ? rsrc->Expose() : 0); }
			virtual void SetPConstantR(int selector, XBuffer * rsrc) noexcept { _context->SetPixelShaderConstant(selector, rsrc ? rsrc->Expose() : 0); }
			virtual void SetPConstantD(int selector, const void * data, int length) noexcept { _context->SetPixelShaderConstant(selector, data, length); }
			virtual void SetPSamplerState(int selector, XSamplerState * state) noexcept { _context->SetPixelShaderSamplerState(selector, state ? state->Expose() : 0); }
			virtual void SetIndexBuffer(XBuffer * rsrc, Graphics::IndexBufferFormat format) noexcept { _context->SetIndexBuffer(rsrc ? rsrc->Expose() : 0, format); }
			virtual void SetStencilValue(uint8 value) noexcept { _context->SetStencilReferenceValue(value); }
			virtual void RenderV(int numvert, int first) noexcept { _context->DrawPrimitives(numvert, first); }
			virtual void RenderVI(int numvert, int first, int numinst, int first_inst) noexcept { _context->DrawInstancedPrimitives(numvert, first, numinst, first_inst); }
			virtual void RenderI(int numind, int first, int base) noexcept { _context->DrawIndexedPrimitives(numind, first, base); }
			virtual void RenderII(int numind, int first, int base, int numinst, int first_inst) noexcept { _context->DrawIndexedInstancedPrimitives(numind, first, base, numinst, first_inst); }
			virtual DynamicObject * Get2DContext(void) noexcept { return _context_2d; }
			virtual void GenerateMIPs(XTexture * texture) noexcept { if (texture) _context->GenerateMipmaps(texture->Expose()); }
			virtual void BlockTransferA(XResource * dest, XResource * src) noexcept { if (dest && src) _context->CopyResourceData(dest->Expose(), src->Expose()); }
			virtual void BlockTransferB(XResource * dest, const int * dest_sr, const int * dest_org, XResource * src, const int * src_sr, const int * src_org, const int * size) noexcept
			{
				if (dest && src) _context->CopySubresourceData(
					dest->Expose(), Graphics::SubresourceIndex(dest_sr[0], dest_sr[1]), Graphics::VolumeIndex(dest_org[0], dest_org[1], dest_org[2]),
					src->Expose(), Graphics::SubresourceIndex(src_sr[0], src_sr[1]), Graphics::VolumeIndex(src_org[0], src_org[1], src_org[2]),
					Graphics::VolumeIndex(size[0], size[1], size[2]));
			}
			virtual void MemoryUpdate(XResource * dest, const int * dest_sr, const int * dest_org, const int * size, const Graphics::ResourceInitDesc & src) noexcept
			{
				if (dest) _context->UpdateResourceData(
					dest->Expose(), Graphics::SubresourceIndex(dest_sr[0], dest_sr[1]), Graphics::VolumeIndex(dest_org[0], dest_org[1], dest_org[2]),
					Graphics::VolumeIndex(size[0], size[1], size[2]), src);
			}
			virtual void MemoryQuery(const Graphics::ResourceDataDesc & dest, XResource * src, const int * src_sr, const int * src_org, const int * size) noexcept
			{
				if (src) _context->QueryResourceData(dest,
					src->Expose(), Graphics::SubresourceIndex(src_sr[0], src_sr[1]), Graphics::VolumeIndex(src_org[0], src_org[1], src_org[2]),
					Graphics::VolumeIndex(size[0], size[1], size[2]));
			}
			virtual bool LockSharedResource(XResource * rsrc) noexcept { if (!rsrc) return false; return _context->AcquireSharedResource(rsrc->Expose()); }
			virtual bool LockSharedResourceWithinTimeout(XResource * rsrc, uint32 time) noexcept { if (!rsrc) return false; return _context->AcquireSharedResource(rsrc->Expose(), time); }
			virtual bool UnlockSharedResource(XResource * rsrc) noexcept { if (!rsrc) return false; return _context->ReleaseSharedResource(rsrc->Expose()); }
			virtual string ToString(void) const override { try { return _context->ToString(); } catch (...) { return L""; } }
		};
		class XFunction : public DynamicObject
		{
			SafePointer<Graphics::IShader> _function;
			Object * _parent;
		public:
			XFunction(Object * parent, Graphics::IShader * function) : _parent(parent) { _function.SetRetain(function); }
			virtual ~XFunction(void) override {}
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept override
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				if (cls->GetClassName() == L"objectum") {
					Retain(); return static_cast<Object *>(this);
				} else if (cls->GetClassName() == L"objectum_dynamicum") {
					Retain(); return static_cast<DynamicObject *>(this);
				} else if (cls->GetClassName() == L"graphicum.machinatio") {
					_parent->Retain(); return _parent;
				} else if (cls->GetClassName() == L"graphicum.functio_machinationis") {
					Retain(); return this;
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual void * GetType(void) noexcept override { return 0; }
			virtual string GetName(ErrorContext & ectx) noexcept { XE_TRY_INTRO return _function->GetName(); XE_TRY_OUTRO(L"") }
			virtual uint GetClass(void) noexcept { return uint(_function->GetType()); }
			virtual string ToString(void) const override { try { return _function->ToString(); } catch (...) { return L""; } }
			Graphics::IShader * Expose(void) noexcept { return _function; }
		};
		class XFunctionLibrary : public DynamicObject
		{
			SafePointer<Graphics::IShaderLibrary> _library;
			Object * _parent;
		public:
			XFunctionLibrary(Object * parent, Graphics::IShaderLibrary * library) : _parent(parent) { _library.SetRetain(library); }
			virtual ~XFunctionLibrary(void) override {}
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept override
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				if (cls->GetClassName() == L"objectum") {
					Retain(); return static_cast<Object *>(this);
				} else if (cls->GetClassName() == L"objectum_dynamicum") {
					Retain(); return static_cast<DynamicObject *>(this);
				} else if (cls->GetClassName() == L"graphicum.machinatio") {
					_parent->Retain(); return _parent;
				} else if (cls->GetClassName() == L"graphicum.liber_functionum") {
					Retain(); return this;
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual void * GetType(void) noexcept override { return 0; }
			virtual SafePointer< Array<string> > GetFunctions(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< Array<string> > result = _library->GetShaderNames();
				if (!result) throw OutOfMemoryException();
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<XFunction> GetFunction(const string & name, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Graphics::IShader> func = _library->CreateShader(name);
				if (!func) throw InvalidStateException();
				return new XFunction(_parent, func);
				XE_TRY_OUTRO(0)
			}
			virtual string ToString(void) const override { try { return _library->ToString(); } catch (...) { return L""; } }
		};
		class XDevice : public Object
		{
			SafePointer<Graphics::IDevice> _device;
			SafePointer<XDeviceContext> _context;
		public:
			XDevice(Graphics::IDevice * device) { _device.SetRetain(device); _context = new XDeviceContext(this, _device); }
			virtual ~XDevice(void) override {}
			virtual string GetName(ErrorContext & ectx) noexcept { XE_TRY_INTRO return _device->GetDeviceName(); XE_TRY_OUTRO(0) }
			virtual uint64 GetID(void) noexcept { return _device->GetDeviceIdentifier(); }
			virtual bool IsValid(void) noexcept { return _device->DeviceIsValid(); }
			virtual uint32 GetDeviceClass(void) noexcept { return uint(_device->GetDeviceClass()); }
			virtual uint64 GetDeviceMemory(void) noexcept { return _device->GetDeviceMemory(); }
			virtual XDeviceContext * GetContext(void) noexcept { return _context; }
			virtual void GetImplementation(string * tech, uint * version) noexcept
			{
				try {
					string t; uint v[2];
					_device->GetImplementationInfo(t, v[0], v[1]);
					if (version) { version[0] = v[0]; version[1] = v[1]; }
					if (tech) *tech = t;
				} catch (...) {}
			}
			virtual bool GetPixelFormatSupport(uint32 format, uint32 mode) noexcept { return _device->GetDevicePixelFormatSupport(static_cast<Graphics::PixelFormat>(format), static_cast<Graphics::PixelFormatUsage>(mode)); }
			virtual SafePointer<XFunctionLibrary> LoadFunctions(XStream * xstream) noexcept
			{
				try {
					if (!xstream) return 0;
					SafePointer<Streaming::Stream> stream = WrapFromXStream(xstream);
					SafePointer<Graphics::IShaderLibrary> library = _device->LoadShaderLibrary(stream);
					if (!library) return 0;
					return new XFunctionLibrary(this, library);
				} catch (...) { return 0; }
			}
			virtual SafePointer<XFunctionLibrary> CompileFunctions(XStream * xstream, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!xstream) throw InvalidArgumentException();
				Graphics::ShaderError error;
				SafePointer<Streaming::Stream> stream = WrapFromXStream(xstream);
				SafePointer<Graphics::IShaderLibrary> library = _device->CompileShaderLibrary(stream, &error);
				if (error == Graphics::ShaderError::Success) {
					if (!library) throw InvalidStateException();
					return new XFunctionLibrary(this, library);
				} else if (error == Graphics::ShaderError::IO) {
					throw IO::FileAccessException(IO::Error::Unknown);
				} else if (error == Graphics::ShaderError::InvalidContainerData) {
					throw InvalidFormatException();
				} else if (error == Graphics::ShaderError::NoCompiler) {
					ectx.error_code = 7; ectx.error_subcode = 10; return 0;
				} else if (error == Graphics::ShaderError::NoPlatformVersion) {
					ectx.error_code = 7; ectx.error_subcode = 4; return 0;
				} else if (error == Graphics::ShaderError::Compilation) {
					ectx.error_code = 7; ectx.error_subcode = 3; return 0;
				} else throw InvalidStateException();
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<XPipelineState> CreatePipelineState(const uint8 * src) noexcept
			{
				try {
					XFunction * vertex = reinterpret_cast<XFunction * const *>(src)[0];
					XFunction * pixel = reinterpret_cast<XFunction * const *>(src)[1];
					if (!vertex || !pixel) return 0;
					Graphics::PipelineStateDesc desc;
					desc.VertexShader = vertex->Expose();
					desc.PixelShader = pixel->Expose();
					desc.RenderTargetCount = *reinterpret_cast<const uint *>(src + 2 * sizeof(handle));
					MemoryCopy(&desc.RenderTarget, src + 2 * sizeof(handle) + 4, sizeof(desc.RenderTarget));
					MemoryCopy(&desc.DepthStencil, src + 2 * sizeof(handle) + 4 + sizeof(desc.RenderTarget), sizeof(desc.DepthStencil));
					MemoryCopy(&desc.Rasterization, src + 2 * sizeof(handle) + 4 + sizeof(desc.RenderTarget) + sizeof(desc.DepthStencil), sizeof(desc.Rasterization));
					MemoryCopy(&desc.Topology, src + 2 * sizeof(handle) + 4 + sizeof(desc.RenderTarget) + sizeof(desc.DepthStencil) + sizeof(desc.Rasterization), sizeof(desc.Topology));
					SafePointer<Graphics::IPipelineState> state = _device->CreateRenderingPipelineState(desc);
					if (!state) return 0;
					return new XPipelineState(state, this);
				} catch (...) { return 0; }
			}
			virtual SafePointer<XSamplerState> CreateSamplerState(const Graphics::SamplerDesc & desc) noexcept
			{
				try {
					SafePointer<Graphics::ISamplerState> state = _device->CreateSamplerState(desc);
					if (!state) return 0;
					return new XSamplerState(state, this);
				} catch (...) { return 0; }
			}
			virtual SafePointer<XBuffer> CreateBufferA(const Graphics::BufferDesc & desc) noexcept
			{
				try {
					SafePointer<Graphics::IBuffer> rsrc = _device->CreateBuffer(desc);
					if (!rsrc) return 0;
					return new XBuffer(rsrc, this);
				} catch (...) { return 0; }
			}
			virtual SafePointer<XBuffer> CreateBufferB(const Graphics::BufferDesc & desc, const Graphics::ResourceInitDesc & init) noexcept
			{
				try {
					SafePointer<Graphics::IBuffer> rsrc = _device->CreateBuffer(desc, init);
					if (!rsrc) return 0;
					return new XBuffer(rsrc, this);
				} catch (...) { return 0; }
			}
			virtual SafePointer<XTexture> CreateTextureA(const Graphics::TextureDesc & desc) noexcept
			{
				try {
					#ifdef ENGINE_MACOSX
					if (desc.Format == Graphics::PixelFormat::D24S8_unorm) return 0;
					#endif
					SafePointer<Graphics::ITexture> rsrc = _device->CreateTexture(desc);
					if (!rsrc) return 0;
					return new XTexture(rsrc, this);
				} catch (...) { return 0; }
			}
			virtual SafePointer<XTexture> CreateTextureB(const Graphics::TextureDesc & desc, const Graphics::ResourceInitDesc * init) noexcept
			{
				try {
					#ifdef ENGINE_MACOSX
					if (desc.Format == Graphics::PixelFormat::D24S8_unorm) return 0;
					#endif
					SafePointer<Graphics::ITexture> rsrc = _device->CreateTexture(desc, init);
					if (!rsrc) return 0;
					return new XTexture(rsrc, this);
				} catch (...) { return 0; }
			}
			virtual SafePointer<XTexture> CreateTextureView(XTexture * texture, uint mip, uint depth) noexcept
			{
				try {
					if (!texture) return 0;
					SafePointer<Graphics::ITexture> rsrc = _device->CreateRenderTargetView(texture->Expose(), mip, depth);
					if (!rsrc) return 0;
					return new XTexture(rsrc, this);
				} catch (...) { return 0; }
			}
			virtual SafePointer<XResource> ImportResource(XResourceHandle * handle) noexcept
			{
				try {
					if (!handle) return 0;
					SafePointer<Graphics::IDeviceResource> rsrc = _device->OpenResource(handle->Expose());
					if (!rsrc) return 0;
					if (rsrc->GetResourceType() == Graphics::ResourceType::Buffer) return new XBuffer(static_cast<Graphics::IBuffer *>(rsrc.Inner()), this);
					else if (rsrc->GetResourceType() == Graphics::ResourceType::Texture) return new XTexture(static_cast<Graphics::ITexture *>(rsrc.Inner()), this);
					else return 0;
				} catch (...) { return 0; }
			}
			virtual SafePointer<XWindowSurface> CreateSurface(VisualObject * xwindow, const Graphics::WindowLayerDesc & desc) noexcept
			{
				try {
					ErrorContext ectx; ectx.error_code = ectx.error_subcode = 0;
					Windows::IWindow * window;
					if (!xwindow) return 0;
					xwindow->ExposeInterface(VisualObjectInterfaceWindow, &window, ectx);
					if (ectx.error_code) return 0;
					SafePointer<Graphics::IWindowLayer> layer = _device->CreateWindowLayer(window, desc);
					if (!layer) return 0;
					return new XWindowSurface(layer, this);
				} catch (...) { return 0; }
			}
			virtual string ToString(void) const override { try { return _device->ToString(); } catch (...) { return L""; } }
			Graphics::IDevice * GetDevice(void) noexcept { return _device; }
		};
		class X2DContext : public DynamicObject
		{
			SafePointer<Graphics::I2DDeviceContext> _context;
			DynamicObject * _supercontext;
		protected:
			X2DContext(void) {}
			void SetContext(Graphics::I2DDeviceContext * context) { _context.SetRetain(context); }
		public:
			X2DContext(Graphics::I2DDeviceContext * context) { _context.SetRetain(context); }
			X2DContext(Graphics::I2DDeviceContext * context, DynamicObject * super) : _supercontext(super) { _context.SetRetain(context); }
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
				} else if (cls->GetClassName() == L"graphicum.machinatio" && _supercontext) {
					return _supercontext->DynamicCast(cls, ectx);
				} else if (cls->GetClassName() == L"graphicum.contextus_machinationis" && _supercontext) {
					return _supercontext->DynamicCast(cls, ectx);
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual void * GetType(void) noexcept override { return 0; }
			virtual void GetImplementation(string * tech, uint * version, uint * feature) noexcept
			{
				string t;
				uint v[2];
				_context->GetImplementationInfo(t, v[0], v[1]);
				if (tech) *tech = t;
				if (version) { version[0] = v[0]; version[1] = v[1]; }
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
			virtual string ToString(void) const override { try { return _context->ToString(); } catch (...) { return L""; } }
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
				Graphics::RenderTargetViewDesc rtvd;
				rtvd.Texture = _backbuffer;
				rtvd.LoadAction = Graphics::TextureLoadAction::DontCare;
				if (context->Begin2DRenderingPass(rtvd)) {
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
				Graphics::RenderTargetViewDesc rtvd;
				rtvd.Texture = _backbuffer;
				rtvd.LoadAction = Graphics::TextureLoadAction::Load;
				return context->Begin2DRenderingPass(rtvd);
			}
			virtual bool BeginRenderingAndClear(const XColor & color) noexcept
			{
				auto context = _device->GetDeviceContext();
				Color clr(color.value);
				Graphics::RenderTargetViewDesc rtvd;
				rtvd.Texture = _backbuffer;
				rtvd.LoadAction = Graphics::TextureLoadAction::Clear;
				rtvd.ClearValue[0] = float(clr.r) / 255.0f;
				rtvd.ClearValue[1] = float(clr.g) / 255.0f;
				rtvd.ClearValue[2] = float(clr.b) / 255.0f;
				rtvd.ClearValue[3] = float(clr.a) / 255.0f;
				return context->Begin2DRenderingPass(rtvd);
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
		class XWindowContext : public X2DContext
		{
			SafePointer<Windows::I2DPresentationEngine> _pres;
			SafePointer<XDevice> _own_device;
		public:
			XWindowContext(Windows::I2DPresentationEngine * pres) { _pres.SetRetain(pres); SetContext(pres->GetContext()); }
			virtual ~XWindowContext(void) override { SetContext(0); }
			virtual void * DynamicCast(const ClassSymbol * cls, ErrorContext & ectx) noexcept override
			{
				if (!cls) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				if (cls->GetClassName() == L"objectum") {
					Retain(); return static_cast<Object *>(this);
				} else if (cls->GetClassName() == L"objectum_dynamicum") {
					Retain(); return static_cast<DynamicObject *>(this);
				} else if (cls->GetClassName() == L"graphicum.contextus_machinae") {
					Retain(); return static_cast<X2DContext *>(this);
				} else if (cls->GetClassName() == L"fenestrae.contextus_fenestrae") {
					Retain(); return static_cast<XWindowContext *>(this);
				} else if (cls->GetClassName() == L"graphicum.fabricatio_contextus") {
					try { return CreateX2DFactory(); }
					catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return 0; }
				} else if (cls->GetClassName() == L"graphicum.machinatio") {
					if (!_own_device) {
						auto device = _pres->GetContext()->GetParentDevice();
						if (!device) { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
						try { _own_device = new XDevice(device); } catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return 0; }
					}
					_own_device->Retain(); return _own_device.Inner();
				} else if (cls->GetClassName() == L"graphicum.contextus_machinationis") {
					if (!_own_device) {
						auto device = _pres->GetContext()->GetParentDevice();
						if (!device) { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
						try { _own_device = new XDevice(device); } catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return 0; }
					}
					auto context = _own_device->GetContext();
					context->Retain(); return context;
				} else { ectx.error_code = 1; ectx.error_subcode = 0; return 0; }
			}
			virtual bool BeginRendering(void) noexcept { return _pres->BeginRenderingPass(); }
			virtual bool EndRendering(void) noexcept { return _pres->EndRenderingPass(); }
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
			virtual string ToString(void) const override { try { return _factory->ToString(); } catch (...) { return L""; } }
		};
		class XDeviceFactory : public Object
		{
		public:
			struct desc { uint64 id; string name; intptr padding; };
		private:
			SafePointer<Graphics::IDeviceFactory> _factory;
		public:
			XDeviceFactory(void) { _factory = Graphics::CreateDeviceFactory(); if (!_factory) throw OutOfMemoryException(); }
			virtual ~XDeviceFactory(void) override {}
			virtual SafePointer< Array<desc> > GetDevices(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< Volumes::Dictionary<uint64, string> > list = _factory->GetAvailableDevices();
				if (!list) throw OutOfMemoryException();
				SafePointer< Array<desc> > result = new Array<desc>(0x10);
				for (auto & d : list->Elements()) {
					desc dev;
					dev.id = d.key;
					dev.name = d.value;
					dev.padding = 0;
					result->Append(dev);
				}
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<XDevice> CreateDeviceA(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Graphics::IDevice> device = _factory->CreateDefaultDevice();
				if (!device) throw InvalidStateException();
				return new XDevice(device);
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<XDevice> CreateDeviceB(uint64 id, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Graphics::IDevice> device = _factory->CreateDevice(id);
				if (!device) throw InvalidArgumentException();
				return new XDevice(device);
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<XResourceHandle> ExtractHandle(XResource * rsrc, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!rsrc) throw InvalidArgumentException();
				SafePointer<Graphics::IDeviceResourceHandle> inner = _factory->QueryResourceHandle(rsrc->Expose());
				if (!inner) throw Exception();
				return new XResourceHandle(inner);
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<XResourceHandle> LoadHandle(const DataBlock * data, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				if (!data) throw InvalidArgumentException();
				SafePointer<Graphics::IDeviceResourceHandle> inner = _factory->OpenResourceHandle(data);
				if (!inner) throw InvalidFormatException();
				return new XResourceHandle(inner);
				XE_TRY_OUTRO(0)
			}
			virtual string ToString(void) const override { try { return _factory->ToString(); } catch (...) { return L""; } }
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
			static SafePointer<XDeviceFactory> _create_factory(void) noexcept { try { return new XDeviceFactory; } catch (...) { return 0; } }
		public:
			ImageExtension(void) {}
			virtual ~ImageExtension(void) override {}
			virtual const void * ExposeRoutine(const string & routine_name) noexcept override
			{
				if (string::Compare(routine_name, L"im_crep") < 0) {
					if (string::Compare(routine_name, L"im_al_4") < 0) {
						if (string::Compare(routine_name, L"im_al_2") < 0) {
							if (string::Compare(routine_name, L"im_al_1") < 0) {
								if (string::Compare(routine_name, L"im_al_0") == 0) return reinterpret_cast<const void *>(&_allocate_frame_0);
							} else {
								if (string::Compare(routine_name, L"im_al_1") == 0) return reinterpret_cast<const void *>(&_allocate_frame_1);
							}
						} else {
							if (string::Compare(routine_name, L"im_al_3") < 0) {
								if (string::Compare(routine_name, L"im_al_2") == 0) return reinterpret_cast<const void *>(&_allocate_frame_2);
							} else {
								if (string::Compare(routine_name, L"im_al_3") == 0) return reinterpret_cast<const void *>(&_allocate_frame_3);
							}
						}
					} else {
						if (string::Compare(routine_name, L"im_al_6") < 0) {
							if (string::Compare(routine_name, L"im_al_5") < 0) {
								if (string::Compare(routine_name, L"im_al_4") == 0) return reinterpret_cast<const void *>(&_allocate_frame_4);
							} else {
								if (string::Compare(routine_name, L"im_al_5") == 0) return reinterpret_cast<const void *>(&_allocate_frame_5);
							}
						} else {
							if (string::Compare(routine_name, L"im_al_7") < 0) {
								if (string::Compare(routine_name, L"im_al_6") == 0) return reinterpret_cast<const void *>(&_allocate_frame_6);
							} else {
								if (string::Compare(routine_name, L"im_ccol") < 0) {
									if (string::Compare(routine_name, L"im_al_7") == 0) return reinterpret_cast<const void *>(&_allocate_frame_7);
								} else {
									if (string::Compare(routine_name, L"im_ccol") == 0) return reinterpret_cast<const void *>(&_encode_image);
								}
							}
						}
					}
				} else {
					if (string::Compare(routine_name, L"im_dr_1") < 0) {
						if (string::Compare(routine_name, L"im_crfm") < 0) {
							if (string::Compare(routine_name, L"im_crfc") < 0) {
								if (string::Compare(routine_name, L"im_crep") == 0) return reinterpret_cast<const void *>(&_encode_frame);
							} else {
								if (string::Compare(routine_name, L"im_crfc") == 0) return reinterpret_cast<const void *>(&_create_dc_factory);
							}
						} else {
							if (string::Compare(routine_name, L"im_dc_1") < 0) {
								if (string::Compare(routine_name, L"im_crfm") == 0) return reinterpret_cast<const void *>(&_create_factory);
							} else {
								if (string::Compare(routine_name, L"im_dc_2") < 0) {
									if (string::Compare(routine_name, L"im_dc_1") == 0) return reinterpret_cast<const void *>(&_decode_image_1);
								} else {
									if (string::Compare(routine_name, L"im_dc_2") == 0) return reinterpret_cast<const void *>(&_decode_image_2);
								}
							}
						}
					} else {
						if (string::Compare(routine_name, L"im_md_0") < 0) {
							if (string::Compare(routine_name, L"im_dr_2") < 0) {
								if (string::Compare(routine_name, L"im_dr_1") == 0) return reinterpret_cast<const void *>(&_decode_frame_1);
							} else {
								if (string::Compare(routine_name, L"im_dr_2") == 0) return reinterpret_cast<const void *>(&_decode_frame_2);
							}
						} else {
							if (string::Compare(routine_name, L"im_md_1") < 0) {
								if (string::Compare(routine_name, L"im_md_0") == 0) return reinterpret_cast<const void *>(&_query_of_module_0);
							} else {
								if (string::Compare(routine_name, L"im_md_2") < 0) {
									if (string::Compare(routine_name, L"im_md_1") == 0) return reinterpret_cast<const void *>(&_query_of_module_1);
								} else {
									if (string::Compare(routine_name, L"im_md_2") == 0) return reinterpret_cast<const void *>(&_query_of_module_2);
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
		DynamicObject * CreateWindowContext(Windows::I2DPresentationEngine * pres) { return new XWindowContext(pres); }
		DynamicObject * WrapContext(Graphics::I2DDeviceContext * context) { return new X2DContext(context); }
		DynamicObject * WrapContext(Graphics::I2DDeviceContext * context, DynamicObject * supercontext) { return new X2DContext(context, supercontext); }
		Graphics::IDevice * GetWrappedDevice(Object * wrapper) { return static_cast<XDevice *>(wrapper)->GetDevice(); }
	}
}