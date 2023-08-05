#pragma once

#include "xe_loader.h"

namespace Engine
{
	namespace XE
	{
		class ILoggerSink
		{
		public:
			virtual void Log(const string & line) noexcept = 0;
			virtual Object * ExposeObject(void) noexcept = 0;
		};

		void GetErrorDescription(const ErrorContext & ectx, const ExecutionContext & xctx, string & error, string & suberror);

		IAPIExtension * CreateLogger(void);
		void SetLoggerSink(const ExecutionContext & xctx, ILoggerSink * sink);
		void SetErrorLocalization(const ExecutionContext & xctx, Volumes::Dictionary<string, string> * dict);
	}
}