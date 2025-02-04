#pragma once

#include "EngineBase.h"
#include "Interfaces/SystemIO.h"

namespace Engine
{
	namespace Streaming
	{
		class Stream : public Object
		{
		public:
			virtual void Read(void * buffer, uint32 length) = 0;
			virtual void Write(const void * data, uint32 length) = 0;
			virtual int64 Seek(int64 position, SeekOrigin origin) = 0;
			virtual uint64 Length(void) = 0;
			virtual void SetLength(uint64 length) = 0;
			virtual void Flush(void) = 0;

			void CopyTo(Stream * to, uint64 length);
			void CopyTo(Stream * to);
			void CopyToUntilEof(Stream * to);
			Array<uint8> * ReadAll(void);
			Array<uint8> * ReadBlock(uint32 length);
			void WriteArray(const Array<uint8> * data);
			void RelocateData(uint64 offset_from, uint64 offset_to, uint64 length);
		};

		class FileStream final : public Stream
		{
			handle file;
			bool owned;
		public:
			FileStream(const string & path, FileAccess access, FileCreationMode mode);
			FileStream(handle file_handle, bool take_control = false);
			virtual void Read(void * buffer, uint32 length) override;
			virtual void Write(const void * data, uint32 length) override;
			virtual int64 Seek(int64 position, SeekOrigin origin) override;
			virtual uint64 Length(void) override;
			virtual void SetLength(uint64 length) override;
			virtual void Flush(void) override;
			virtual ~FileStream(void) override;
			virtual string ToString(void) const override;

			handle Handle(void) const;
			handle & Handle(void);
		};
		class MemoryStream final : public Stream
		{
			Array<uint8> data;
			int32 pointer;
		public:
			MemoryStream(const void * source, int length);
			MemoryStream(const void * source, int length, int block);
			MemoryStream(Stream * source, int length);
			MemoryStream(Stream * source, int length, int block);
			MemoryStream(int block);
			virtual void Read(void * buffer, uint32 length) override;
			virtual void Write(const void * data, uint32 length) override;
			virtual int64 Seek(int64 position, SeekOrigin origin) override;
			virtual uint64 Length(void) override;
			virtual void SetLength(uint64 length) override;
			virtual void Flush(void) override;
			virtual ~MemoryStream(void) override;
			virtual string ToString(void) const override;

			void * GetBuffer(void);
			const void * GetBuffer(void) const;
		};
		class FragmentStream final : public Stream
		{
			Stream * inner;
			uint64 begin, end;
			int64 pointer;
		public:
			FragmentStream(Stream * Inner, uint64 From, uint64 Length);			
			virtual void Read(void * buffer, uint32 length) override;
			virtual void Write(const void * data, uint32 length) override;
			virtual int64 Seek(int64 position, SeekOrigin origin) override;
			virtual uint64 Length(void) override;
			virtual void SetLength(uint64 length) override;
			virtual void Flush(void) override;
			virtual ~FragmentStream(void) override;
			virtual string ToString(void) const override;
		};
	}
}