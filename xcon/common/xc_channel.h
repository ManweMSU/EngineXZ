#pragma once

#include <EngineRuntime.h>

namespace Engine
{
	namespace XC
	{
		struct Request
		{
			uint verb;
			SafePointer<DataBlock> data;
		};
		
		class IChannel : public Object
		{
		public:
			virtual bool SendRequest(const Request & req) noexcept = 0;
			virtual bool ReadRequest(Request & req) noexcept = 0;
		};
		class IChannelServer : public Object
		{
		public:
			virtual void GetChannelPath(string & path) noexcept = 0;
			virtual IChannel * Accept(void) noexcept = 0;
		};

		IChannelServer * CreateChannelServer(void);
		IChannel * ConnectChannel(const string & at_path);
	}
}