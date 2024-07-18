#pragma once

#include "xe_netapi.h"

#ifdef ENGINE_WINDOWS
namespace Engine
{
	namespace XE
	{
		SafePointer<INetworkChannel> CreateNetworkChannelS(NetworkAddress * address);
		SafePointer<INetworkListener> CreateNetworkListenerS(NetworkAddress * address, NetworkAddressFactory & factory);
	}
}
#endif