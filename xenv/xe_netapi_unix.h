#pragma once

#include "xe_netapi.h"

#ifdef ENGINE_UNIX

#define REQUEST_ATTRIBUTE_POLL_READ		0x001
#define REQUEST_ATTRIBUTE_POLL_WRITE	0x002
#define REQUEST_ATTRIBUTE_CONNECT_BIT	0x010
#define REQUEST_ATTRIBUTE_ACCEPT_BIT	0x020
#define REQUEST_ATTRIBUTE_RECEIVE_BIT	0x040
#define REQUEST_ATTRIBUTE_SEND_BIT		0x080
#define REQUEST_ATTRIBUTE_REMOVE		0x100
#ifdef ENGINE_LINUX
#define REQUEST_ATTRIBUTE_OPENSSL		0x200
#define REQUEST_ATTRIBUTE_PRIORITY		0x400
#define REQUEST_ATTRIBUTE_INCOMPLETE	0x800
#endif

#define REQUEST_ATTRIBUTE_POLL_MASK		0x00F
#define REQUEST_ATTRIBUTE_ACTION_MASK	0x0F0

#define REQUEST_ATTRIBUTE_CONNECT	(REQUEST_ATTRIBUTE_CONNECT_BIT	| REQUEST_ATTRIBUTE_POLL_WRITE)
#define REQUEST_ATTRIBUTE_ACCEPT	(REQUEST_ATTRIBUTE_ACCEPT_BIT	| REQUEST_ATTRIBUTE_POLL_READ)
#define REQUEST_ATTRIBUTE_RECEIVE	(REQUEST_ATTRIBUTE_RECEIVE_BIT	| REQUEST_ATTRIBUTE_POLL_READ)
#define REQUEST_ATTRIBUTE_SEND		(REQUEST_ATTRIBUTE_SEND_BIT		| REQUEST_ATTRIBUTE_POLL_WRITE)

namespace Engine
{
	namespace XE
	{
		struct NetworkRequestInput
		{
			uint attributes;
			int length, pointer; // read length, read/write execution pointer
			SafePointer<DataBlock> data; // address structure or data to send
		};
		struct NetworkRequestOutput
		{
			SafePointer<IDispatchTask> handler;
			ErrorContext * status;
			int * length;
			SafePointer<DataBlock> * data;
			SafePointer<NetworkAddress> * address;
			SafePointer<INetworkChannel> * channel;
		};
		struct NetworkRequest
		{
			NetworkRequestInput in;
			NetworkRequestOutput out;
		};
		struct NewNetworkConnection
		{
			NetworkAddressFactory * in_factory;
			int in_socket;
			void * in_address;
			int in_address_length;
			SafePointer<INetworkChannel> out_channel;
			SafePointer<NetworkAddress> out_address;
		};
		#ifdef ENGINE_LINUX
		enum class OpenSSLEventStatus { None = 0, Failed = 1, PollRenew = 2, CompleteReadRequest = 3, CompleteWriteRequest = 4, CloseChannelWrite = 5, CloseChannel = 6 };
		class IOpenSSLEventHandler : public Object
		{
		public:
			virtual OpenSSLEventStatus ProcessConnection(int pollstat, NetworkRequest * req_read, NetworkRequest * req_write, uint pollattr) noexcept = 0; 
		};
		#endif

		void SetPosixError(int error, ErrorContext & ectx) noexcept;
		void SetPosixError(ErrorContext & ectx) noexcept;
		void SetDNSError(int error, ErrorContext & ectx) noexcept;

		void SocketAddressInit(DataBlock & dest, NetworkAddress * address, string * ulnk);
		void SocketAddressRead(void * src, NetworkAddressFactory * factory, NetworkAddress ** address);

		void NetworkEngineInit(void);
		void NetworkEngineStop(void) noexcept;
		SafePointer<INetworkChannel> CreateNetworkChannel(NetworkAddress * address);
		SafePointer<INetworkListener> CreateNetworkListener(NetworkAddress * address, NetworkAddressFactory & factory);
		SafePointer< ObjectArray<NetworkAddress> > GetNetworkAddresses(NetworkAddressFactory & factory, const string & domain, uint16 port, const ClassSymbol * nns, ErrorContext & ectx) noexcept;

		int UnixChannelGetSocket(INetworkChannel * channel) noexcept;
		void UnixChannelEnqueueRequest(INetworkChannel * channel, const NetworkRequest & req);
		void UnixChannelEnterCriticalSection(INetworkChannel * channel) noexcept;
		void UnixChannelLeaveCriticalSection(INetworkChannel * channel) noexcept;
		#ifdef ENGINE_LINUX
		void UnixChannelSetOpenSSLEventHandler(INetworkChannel * channel, IOpenSSLEventHandler * hdlr) noexcept;
		#endif
	}
}
#endif