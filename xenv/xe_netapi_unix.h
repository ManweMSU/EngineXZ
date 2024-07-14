#pragma once

#include "xe_netapi.h"

#ifdef ENGINE_UNIX
namespace Engine
{
	namespace XE
	{
		void SetPosixError(int error, ErrorContext & ectx) noexcept;
		void SetPosixError(ErrorContext & ectx) noexcept;
		void SetDNSError(int error, ErrorContext & ectx) noexcept;

		void SocketAddressInit(DataBlock & dest, NetworkAddress * address, string * ulnk);
		void SocketAddressRead(void * src, NetworkAddressFactory * factory, NetworkAddress ** address);

		void NetworkEngineInit(void);
		void NetworkEngineStop(void) noexcept;
		SafePointer<INetworkChannel> CreateNetworkChannel(void);
		SafePointer<INetworkListener> CreateNetworkListener(NetworkAddressFactory & factory);
		SafePointer< ObjectArray<NetworkAddress> > GetNetworkAddresses(NetworkAddressFactory & factory, const string & domain, uint16 port, const ClassSymbol * nns, ErrorContext & ectx) noexcept;
	}
}
#endif