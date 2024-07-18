#pragma once

#include "xe_netapi.h"

#ifdef ENGINE_WINDOWS
namespace Engine
{
	namespace XE
	{
		void NetworkEngineInit(void);
		void NetworkEngineStop(void) noexcept;
		SafePointer<INetworkChannel> CreateNetworkChannel(NetworkAddress * address);
		SafePointer<INetworkListener> CreateNetworkListener(NetworkAddress * address, NetworkAddressFactory & factory);
		SafePointer< ObjectArray<NetworkAddress> > GetNetworkAddresses(NetworkAddressFactory & factory, const string & domain, uint16 port, const ClassSymbol * nns, ErrorContext & ectx) noexcept;
	}
}
#endif