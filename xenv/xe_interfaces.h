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

		XStream * WrapToXStream(Streaming::Stream * stream);
		Streaming::Stream * WrapFromXStream(XStream * stream);
	}
}