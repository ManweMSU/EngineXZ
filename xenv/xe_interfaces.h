#pragma once

#include "xe_ctx.h"

namespace Engine
{
	namespace XE
	{
		class XStream : public Object
		{
		public:
			virtual int Read(void * data, int length, ErrorContext & ectx) noexcept = 0;
			virtual void Write(const void * data, int length, ErrorContext & ectx) noexcept = 0;
			virtual int64 Seek(int64 to, int origin, ErrorContext & ectx) noexcept = 0;
			virtual int64 GetLength(ErrorContext & ectx) noexcept = 0;
			virtual void SetLength(const int64 & length, ErrorContext & ectx) noexcept = 0;
			virtual void Flush(void) noexcept = 0;
			virtual bool IsXV(void) noexcept = 0;
		};
		class XTextEncoder : public Object
		{
		public:
			virtual void Write(const string & str, ErrorContext & ectx) noexcept = 0;
			virtual void WriteLine(const string & str, ErrorContext & ectx) noexcept = 0;
			virtual void LineFeed(ErrorContext & ectx) noexcept = 0;
			virtual void WriteSignature(ErrorContext & ectx) noexcept = 0;
		};
		class XTextDecoder : public Object
		{
		public:
			virtual uint32 ReadChar(ErrorContext & ectx) noexcept = 0;
			virtual string ReadLine(ErrorContext & ectx) noexcept = 0;
			virtual string ReadAll(ErrorContext & ectx) noexcept = 0;
			virtual bool IsAtEOS(void) noexcept = 0;
			virtual int GetEncoding(void) noexcept = 0;
		};
		class XDispatchContext : public Object
		{
			SafePointer<IDispatchQueue> _queue;
		public:
			XDispatchContext(IDispatchQueue * queue);
			virtual ~XDispatchContext(void) override;
			virtual string ToString(void) const override;
			virtual bool AddTask(IDispatchTask * task) noexcept;
			virtual bool AddTasks(IDispatchTask ** tasks, int count) noexcept;
		};

		XStream * WrapToXStream(Streaming::Stream * stream);
		Streaming::Stream * WrapFromXStream(XStream * stream);
		XTextEncoder * WrapToEncoder(Streaming::TextWriter * writer);
		XTextDecoder * WrapToDecoder(Streaming::TextReader * reader);
	}
}