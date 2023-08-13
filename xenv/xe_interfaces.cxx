﻿#include "xe_interfaces.h"

namespace Engine
{
	namespace XE
	{
		class WrappedXStream : public XStream
		{
			SafePointer<Streaming::Stream> _stream;
		public:
			WrappedXStream(Streaming::Stream * stream) { _stream.SetRetain(stream); }
			virtual ~WrappedXStream(void) override {}
			virtual string ToString(void) const override { try { return _stream->ToString(); } catch (...) { return L""; } }
			virtual int Read(void * data, int length, ErrorContext & ectx) noexcept override
			{
				try { _stream->Read(data, length); return length; }
				catch (IO::FileReadEndOfFileException & e) { return e.DataRead; }
				catch (IO::FileAccessException & e) { ectx.error_code = 6; ectx.error_subcode = e.code; }
				catch (InvalidStateException & e) { ectx.error_code = 5; ectx.error_subcode = 0; }
				catch (InvalidFormatException & e) { ectx.error_code = 4; ectx.error_subcode = 0; }
				catch (InvalidArgumentException & e) { ectx.error_code = 3; ectx.error_subcode = 0; }
				catch (OutOfMemoryException & e) { ectx.error_code = 2; ectx.error_subcode = 0; }
				catch (...) { ectx.error_code = 6; ectx.error_subcode = 1; }
				return 0;
			}
			virtual void Write(const void * data, int length, ErrorContext & ectx) noexcept override
			{
				try { _stream->Write(data, length); }
				catch (IO::FileAccessException & e) { ectx.error_code = 6; ectx.error_subcode = e.code; }
				catch (InvalidStateException & e) { ectx.error_code = 5; ectx.error_subcode = 0; }
				catch (InvalidFormatException & e) { ectx.error_code = 4; ectx.error_subcode = 0; }
				catch (InvalidArgumentException & e) { ectx.error_code = 3; ectx.error_subcode = 0; }
				catch (OutOfMemoryException & e) { ectx.error_code = 2; ectx.error_subcode = 0; }
				catch (...) { ectx.error_code = 6; ectx.error_subcode = 1; }
			}
			virtual int64 Seek(int64 to, int origin, ErrorContext & ectx) noexcept override
			{
				try {
					if (origin == 0) return _stream->Seek(to, Streaming::Begin);
					else if (origin == 1) return _stream->Seek(to, Streaming::Current);
					else if (origin == 2) return _stream->Seek(to, Streaming::End);
					else throw InvalidArgumentException();
				} catch (IO::FileAccessException & e) { ectx.error_code = 6; ectx.error_subcode = e.code; }
				catch (InvalidStateException & e) { ectx.error_code = 5; ectx.error_subcode = 0; }
				catch (InvalidFormatException & e) { ectx.error_code = 4; ectx.error_subcode = 0; }
				catch (InvalidArgumentException & e) { ectx.error_code = 3; ectx.error_subcode = 0; }
				catch (OutOfMemoryException & e) { ectx.error_code = 2; ectx.error_subcode = 0; }
				catch (...) { ectx.error_code = 6; ectx.error_subcode = 1; }
				return 0;
			}
			virtual int64 GetLength(ErrorContext & ectx) noexcept override
			{
				try { return _stream->Length(); }
				catch (IO::FileAccessException & e) { ectx.error_code = 6; ectx.error_subcode = e.code; }
				catch (InvalidStateException & e) { ectx.error_code = 5; ectx.error_subcode = 0; }
				catch (InvalidFormatException & e) { ectx.error_code = 4; ectx.error_subcode = 0; }
				catch (InvalidArgumentException & e) { ectx.error_code = 3; ectx.error_subcode = 0; }
				catch (OutOfMemoryException & e) { ectx.error_code = 2; ectx.error_subcode = 0; }
				catch (...) { ectx.error_code = 6; ectx.error_subcode = 1; }
				return 0;
			}
			virtual void SetLength(const int64 & length, ErrorContext & ectx) noexcept override
			{
				try { _stream->SetLength(length); }
				catch (IO::FileAccessException & e) { ectx.error_code = 6; ectx.error_subcode = e.code; }
				catch (InvalidStateException & e) { ectx.error_code = 5; ectx.error_subcode = 0; }
				catch (InvalidFormatException & e) { ectx.error_code = 4; ectx.error_subcode = 0; }
				catch (InvalidArgumentException & e) { ectx.error_code = 3; ectx.error_subcode = 0; }
				catch (OutOfMemoryException & e) { ectx.error_code = 2; ectx.error_subcode = 0; }
				catch (...) { ectx.error_code = 6; ectx.error_subcode = 1; }
			}
			virtual void Flush(void) noexcept override { try { _stream->Flush(); } catch (...) {} }
			virtual bool IsXV(void) noexcept override { return false; }
			virtual Streaming::Stream * Unwrap(void) noexcept { _stream->Retain(); return _stream; }
		};
		class WrappedStream : public Streaming::Stream
		{
			SafePointer<XStream> _stream;
		public:
			WrappedStream(XStream * stream) { _stream.SetRetain(stream); }
			virtual ~WrappedStream(void) override {}
			virtual string ToString(void) const override { return L"XStream"; }
			virtual void Read(void * buffer, uint32 length) override
			{
				ErrorContext ectx;
				ectx.error_code = ectx.error_subcode = 0;
				int read = _stream->Read(buffer, length, ectx);
				if (read != length) throw IO::FileReadEndOfFileException(read);
				if (ectx.error_code) {
					if (ectx.error_code == 2) throw OutOfMemoryException();
					else if (ectx.error_code == 3) throw InvalidArgumentException();
					else if (ectx.error_code == 4) throw InvalidFormatException();
					else if (ectx.error_code == 5) throw InvalidStateException();
					else if (ectx.error_code == 6) throw IO::FileAccessException(ectx.error_subcode);
					else throw Exception();
				}
			}
			virtual void Write(const void * data, uint32 length) override
			{
				ErrorContext ectx;
				ectx.error_code = ectx.error_subcode = 0;
				_stream->Write(data, length, ectx);
				if (ectx.error_code) {
					if (ectx.error_code == 2) throw OutOfMemoryException();
					else if (ectx.error_code == 3) throw InvalidArgumentException();
					else if (ectx.error_code == 4) throw InvalidFormatException();
					else if (ectx.error_code == 5) throw InvalidStateException();
					else if (ectx.error_code == 6) throw IO::FileAccessException(ectx.error_subcode);
					else throw Exception();
				}
			}
			virtual int64 Seek(int64 position, Streaming::SeekOrigin origin) override
			{
				ErrorContext ectx;
				ectx.error_code = ectx.error_subcode = 0;
				int64 result;
				if (origin == Streaming::SeekOrigin::Begin) result = _stream->Seek(position, 0, ectx);
				else if (origin == Streaming::SeekOrigin::Current) result = _stream->Seek(position, 1, ectx);
				else if (origin == Streaming::SeekOrigin::End) result = _stream->Seek(position, 2, ectx);
				else throw InvalidArgumentException();
				if (ectx.error_code) {
					if (ectx.error_code == 2) throw OutOfMemoryException();
					else if (ectx.error_code == 3) throw InvalidArgumentException();
					else if (ectx.error_code == 4) throw InvalidFormatException();
					else if (ectx.error_code == 5) throw InvalidStateException();
					else if (ectx.error_code == 6) throw IO::FileAccessException(ectx.error_subcode);
					else throw Exception();
				} else return result;
			}
			virtual uint64 Length(void) override
			{
				ErrorContext ectx;
				ectx.error_code = ectx.error_subcode = 0;
				auto result = _stream->GetLength(ectx);
				if (ectx.error_code) {
					if (ectx.error_code == 2) throw OutOfMemoryException();
					else if (ectx.error_code == 3) throw InvalidArgumentException();
					else if (ectx.error_code == 4) throw InvalidFormatException();
					else if (ectx.error_code == 5) throw InvalidStateException();
					else if (ectx.error_code == 6) throw IO::FileAccessException(ectx.error_subcode);
					else throw Exception();
				} else return result;
			}
			virtual void SetLength(uint64 length) override
			{
				ErrorContext ectx;
				ectx.error_code = ectx.error_subcode = 0;
				_stream->SetLength(length, ectx);
				if (ectx.error_code) {
					if (ectx.error_code == 2) throw OutOfMemoryException();
					else if (ectx.error_code == 3) throw InvalidArgumentException();
					else if (ectx.error_code == 4) throw InvalidFormatException();
					else if (ectx.error_code == 5) throw InvalidStateException();
					else if (ectx.error_code == 6) throw IO::FileAccessException(ectx.error_subcode);
					else throw Exception();
				}
			}
			virtual void Flush(void) override { _stream->Flush(); }
			virtual XStream * Unwrap(void) noexcept { _stream->Retain(); return _stream; }
		};

		XStream * WrapToXStream(Streaming::Stream * stream)
		{
			if (stream->ToString() == L"XStream") return static_cast<WrappedStream *>(stream)->Unwrap();
			return new WrappedXStream(stream);
		}
		Streaming::Stream * WrapFromXStream(XStream * stream)
		{
			if (!stream->IsXV()) return static_cast<WrappedXStream *>(stream)->Unwrap();
			return new WrappedStream(stream);
		}
	}
}