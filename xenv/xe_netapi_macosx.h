#pragma once

#include "xe_netapi.h"

#ifdef ENGINE_MACOSX
namespace Engine
{
	namespace XE
	{
		SafePointer<INetworkChannel> CreateNetworkChannelS(void);
		SafePointer<INetworkListener> CreateNetworkListenerS(NetworkAddressFactory & factory);
	}
}
#endif