#include "xe_tpgrph.h"

#include <Typography.h>

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
		struct PrinterMode
		{
			uint width;
			uint length;
			uint dpi;
			uint orientation;
			double scale;
			uint copies;
			uint duplex_mode;
			bool collate;
			PrinterMode(void) {}
			~PrinterMode(void) {}
		};
		struct DeviceSize
		{
			int width;
			int height;
			DeviceSize(void) {}
			DeviceSize(int w, int h) : width(w), height(h) {}
			~DeviceSize(void) {}
		};
		
		void Fill(Graphics::PrinterModeDesc & dest, const PrinterMode & src, ErrorContext & ectx)
		{
			if (src.width == 0) { ectx.error_code = 3; ectx.error_subcode = 0; return; }
			dest.PaperWidth = src.width;
			if (src.length == 0) { ectx.error_code = 3; ectx.error_subcode = 0; return; }
			dest.PaperLength = src.length;
			if (src.dpi == 0) { ectx.error_code = 3; ectx.error_subcode = 0; return; }
			dest.DPI = src.dpi;
			if (src.orientation == 1) dest.Orientation = Graphics::PaperOrientation::Portrait;
			else if (src.orientation == 2) dest.Orientation = Graphics::PaperOrientation::Landscape;
			else { ectx.error_code = 3; ectx.error_subcode = 0; return; }
			if (src.scale <= 0.0 || src.scale > 1.0) { ectx.error_code = 3; ectx.error_subcode = 0; return; }
			dest.ScaleFactor = src.scale;
			if (src.copies == 0) { ectx.error_code = 3; ectx.error_subcode = 0; return; }
			dest.Copies = src.copies;
			if (src.duplex_mode == 1) dest.DuplexMode = Graphics::PrinterDuplexMode::Simplex;
			else if (src.duplex_mode == 2) dest.DuplexMode = Graphics::PrinterDuplexMode::Duplex;
			else if (src.duplex_mode == 3) dest.DuplexMode = Graphics::PrinterDuplexMode::DuplexTumble;
			else { ectx.error_code = 3; ectx.error_subcode = 0; return; }
			dest.Collate = src.collate;
		}
		void Fill(PrinterMode & dest, const Graphics::PrinterModeDesc & src)
		{
			dest.width = src.PaperWidth;
			dest.length = src.PaperLength;
			dest.dpi = src.DPI;
			dest.orientation = src.Orientation == Graphics::PaperOrientation::Landscape ? 2 : 1;
			dest.scale = src.ScaleFactor;
			dest.copies = src.Copies;
			if (src.DuplexMode == Graphics::PrinterDuplexMode::Duplex) dest.duplex_mode = 2;
			else if (src.DuplexMode == Graphics::PrinterDuplexMode::DuplexTumble) dest.duplex_mode = 3;
			else dest.duplex_mode = 1;
			dest.collate = src.Collate;
		}

		class PrintingContext : public Object
		{
			SafePointer<Graphics::IPrintingContext> _ctx;
			SafePointer<DynamicObject> _device;
			bool _done;
		public:
			PrintingContext(Graphics::IPrintingContext * ctx) : _done(false) { _ctx.SetRetain(ctx); }
			virtual ~PrintingContext(void) override {}
			virtual string ToString(void) const override { try { return L"contextus_imprimendi"; } catch (...) { return L""; } }
			virtual DynamicObject * GetContext(void) noexcept { return _device; }
			virtual DeviceSize GetDimensions(void) noexcept { auto s = _ctx->GetEffectiveResolution(); return DeviceSize(s.x, s.y); }
			virtual bool BeginPage(void) noexcept
			{
				if (_device || _done) return false;
				if (!_ctx->BeginPage()) return false;
				try { _device = WrapContext(_ctx->GetPageDeviceContext()); }
				catch (...) { _ctx->EndPage(); return false; }
				return true;
			}
			virtual bool EndPage(void) noexcept
			{
				if (!_device || _done) return false;
				auto status = _ctx->EndPage();
				_device.SetReference(0);
				return status;
			}
			virtual bool EndDocument(void) noexcept
			{
				if (_done) return false;
				if (_device) EndPage();
				auto status = _ctx->FinalizeDocument();
				_done = true;
				return status;
			}
		};
		class Printer : public Object
		{
			SafePointer<Graphics::IPrinter> _printer;
		public:
			Printer(Graphics::IPrinter * printer) { _printer.SetRetain(printer); }
			virtual ~Printer(void) override {}
			virtual string ToString(void) const override { try { return _printer->GetName(); } catch (...) { return L""; } }
			virtual string GetName(ErrorContext & ectx) noexcept { XE_TRY_INTRO return _printer->GetName(); XE_TRY_OUTRO(L"") }
			virtual PrinterMode GetMode(void) noexcept
			{
				PrinterMode result;
				Graphics::PrinterModeDesc desc;
				_printer->GetDefaultMode(desc);
				Fill(result, desc);
				return result;
			}
			virtual void SetMode(const PrinterMode & mode, ErrorContext & ectx) noexcept
			{
				Graphics::PrinterModeDesc desc;
				Fill(desc, mode, ectx);
				if (ectx.error_code) return;
				if (!_printer->SetDefaultMode(desc)) { ectx.error_code = 1; ectx.error_subcode = 0; }
			}
			virtual bool TestMode(const PrinterMode & mode) noexcept
			{
				ErrorContext ectx;
				ectx.error_code = ectx.error_subcode = 0;
				Graphics::PrinterModeDesc desc;
				Fill(desc, mode, ectx);
				if (ectx.error_code) return false;
				return _printer->CheckMode(desc);
			}
			virtual SafePointer<PrintingContext> PrintA(const string & document, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Graphics::IPrintingContext> ctx = _printer->StartPrinting(document);
				if (!ctx) throw InvalidStateException();
				return new PrintingContext(ctx);
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<PrintingContext> PrintB(const string & document, const PrinterMode & mode, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				Graphics::PrinterModeDesc desc;
				Fill(desc, mode, ectx);
				if (ectx.error_code) return 0;
				if (!_printer->CheckMode(desc)) throw Exception();
				SafePointer<Graphics::IPrintingContext> ctx = _printer->StartPrinting(document, desc);
				if (!ctx) throw InvalidStateException();
				return new PrintingContext(ctx);
				XE_TRY_OUTRO(0)
			}
		};
		class PrinterFactory : public Object
		{
			SafePointer<Graphics::IPrinterFactory> _factory;
		public:
			PrinterFactory(Graphics::IPrinterFactory * factory) { _factory.SetRetain(factory); }
			virtual ~PrinterFactory(void) override {}
			virtual string ToString(void) const override { try { return L"fabricatio_imprimitoris"; } catch (...) { return L""; } }
			virtual string GetDefaultPrinter(ErrorContext & ectx) noexcept { XE_TRY_INTRO return _factory->GetDefaultPrinter(); XE_TRY_OUTRO(L"") }
			virtual SafePointer< Array<string> > GetPrinters(ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer< Array<string> > result = _factory->EnumeratePrinters();
				if (!result) throw OutOfMemoryException();
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<Printer> Open(const string & name, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<Graphics::IPrinter> prnt = _factory->OpenPrinter(name);
				if (!prnt) throw IO::FileAccessException(IO::Error::FileNotFound);
				return new Printer(prnt);
				XE_TRY_OUTRO(0)
			}
		};
		class DocumentContext : public Object
		{
			SafePointer<Portable::IEncoderContext> _context;
			bool _done;
		public:
			DocumentContext(Portable::IEncoderContext * context) : _done(false) { _context.SetRetain(context); }
			virtual ~DocumentContext(void) override {}
			virtual string ToString(void) const override { try { return L"documentum_portabile"; } catch (...) { return L""; } }
			Portable::IEncoderContext * GetContext(void) const noexcept { return _context; }
			virtual void AddPageA(uint width, uint height, ErrorContext & ectx) noexcept
			{
				if (_done) { ectx.error_code = 5; ectx.error_subcode = 0; return; }
				if (!width || !height) { ectx.error_code = 3; ectx.error_subcode = 0; return; }
				if (!_context->AddPage(width, height, 0, 0)) { ectx.error_code = 2; ectx.error_subcode = 0; return; }
			}
			virtual void AddPageB(uint width, uint height, handle xframe, ErrorContext & ectx) noexcept
			{
				if (_done) { ectx.error_code = 5; ectx.error_subcode = 0; return; }
				if (!width || !height || !xframe) { ectx.error_code = 3; ectx.error_subcode = 0; return; }
				SafePointer<Codec::Frame> frame = ExtractFrameFromXFrame(xframe);
				if (!_context->AddPage(width, height, frame, 0)) { ectx.error_code = 2; ectx.error_subcode = 0; return; }
			}
			virtual void AddPageC(uint width, uint height, handle xframe, uint flags, ErrorContext & ectx) noexcept
			{
				if (_done) { ectx.error_code = 5; ectx.error_subcode = 0; return; }
				if (!width || !height || !xframe) { ectx.error_code = 3; ectx.error_subcode = 0; return; }
				SafePointer<Codec::Frame> frame = ExtractFrameFromXFrame(xframe);
				if (!_context->AddPage(width, height, frame, flags)) { ectx.error_code = 2; ectx.error_subcode = 0; return; }
			}
			virtual void SetAttributeA(uint attr, const string & value, ErrorContext & ectx) noexcept
			{
				if (_done) { ectx.error_code = 5; ectx.error_subcode = 0; return; }
				if (!_context->SetMetadata(static_cast<Portable::MetadataKey>(attr), value)) { ectx.error_code = 3; ectx.error_subcode = 0; return; }
			}
			virtual void SetAttributeB(uint attr, const Time & value, ErrorContext & ectx) noexcept
			{
				if (_done) { ectx.error_code = 5; ectx.error_subcode = 0; return; }
				if (!_context->SetMetadata(static_cast<Portable::MetadataKey>(attr), value)) { ectx.error_code = 3; ectx.error_subcode = 0; return; }
			}
			virtual void EndDocument(ErrorContext & ectx) noexcept
			{
				if (_done) return;
				if (!_context->FinalizeDocument()) { ectx.error_code = 6; ectx.error_subcode = 1; return; }
				_done = true;
			}
		};
		class TypographyInterface
		{
		public:
			virtual SafePointer<PrinterFactory> CreatePrinterFactory(ErrorContext & ectx) noexcept = 0;
			virtual SafePointer<DocumentContext> CreateDocumentContext(XStream * stream, ErrorContext & ectx) noexcept = 0;
			virtual SafePointer<PrintingContext> CreatePrintingContext(DocumentContext * ctx, const PrinterMode & mode, uint flags, ErrorContext & ectx) noexcept = 0;
		};
		class TypographyExtension : public IAPIExtension, public TypographyInterface
		{
		public:
			TypographyExtension(void) {}
			virtual const void * ExposeRoutine(const string & routine_name) noexcept override { return 0; }
			virtual const void * ExposeInterface(const string & interface) noexcept override { if (interface == L"typographica") return static_cast<TypographyInterface *>(this); else return 0; }
			virtual SafePointer<PrinterFactory> CreatePrinterFactory(ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				SafePointer<Graphics::IPrinterFactory> factory = Graphics::CreatePrinterFactory();
				if (!factory) throw OutOfMemoryException();
				return new PrinterFactory(factory);
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<DocumentContext> CreateDocumentContext(XStream * stream, ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				if (!stream) throw InvalidArgumentException();
				SafePointer<Streaming::Stream> inner = WrapFromXStream(stream);
				SafePointer<Portable::IEncoderContext> context = Portable::CreateEncoder(inner);
				if (!context) throw OutOfMemoryException();
				return new DocumentContext(context);
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<PrintingContext> CreatePrintingContext(DocumentContext * ctx, const PrinterMode & mode, uint flags, ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				if (!ctx) throw InvalidArgumentException();
				Graphics::PrinterModeDesc desc;
				Fill(desc, mode, ectx);
				if (ectx.error_code) return 0;
				SafePointer<Graphics::IPrintingContext> context = Portable::CreateStreamPrinter(ctx->GetContext(), desc, flags);
				if (!context) throw OutOfMemoryException();
				return new PrintingContext(context);
				XE_TRY_OUTRO(0)
			}
		};

		void ActivateTypographyIO(StandardLoader & ldr)
		{
			SafePointer<IAPIExtension> api = new TypographyExtension;
			if (!ldr.RegisterAPIExtension(api)) throw Exception();
		}
	}
}