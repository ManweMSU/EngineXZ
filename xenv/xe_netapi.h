#pragma once

#include "xe_conapi.h"

namespace Engine
{
	namespace XE
	{
		void ClearXEError(ErrorContext & ectx) noexcept;
		void SetXEError(ErrorContext & ectx, int error, int suberror) noexcept;
		void ActivateNetworkAPI(StandardLoader & ldr);

		class NetworkAddress : public DynamicObject {};
		struct NetworkAddressLocal : public NetworkAddress
		{
			string Name;
		};
		struct NetworkAddressIPv4 : public NetworkAddress
		{
			uint8 IP[4];
			uint16 Port;
		};
		struct NetworkAddressIPv6 : public NetworkAddress
		{
			uint16 IP[8];
			uint16 Port;
		};
		class NetworkAddressFactory
		{
		public:
			virtual SafePointer<NetworkAddressLocal> CreateLocal( ErrorContext & ectx) = 0;
			virtual SafePointer<NetworkAddressIPv4> CreateIPv4(ErrorContext & ectx) = 0;
			virtual SafePointer<NetworkAddressIPv6> CreateIPv6(ErrorContext & ectx) = 0;
		};
		struct NetworkSecurityDescriptor
		{
			SafePointer<DataBlock> Certificate;
			string Domain;
			bool IgnoreSecurity;
		};
		struct NetworkIdentityDescriptor
		{
			SafePointer<DataBlock> Data;
			string Password;
		};

		class INetworkChannel : public Object
		{
		public:
			virtual void ConnectA(NetworkAddress * address, ErrorContext * error, IDispatchTask * hdlr, ErrorContext & ectx) noexcept = 0;
			virtual void ConnectB(NetworkAddress * address, NetworkSecurityDescriptor & sec, ErrorContext * error, IDispatchTask * hdlr, ErrorContext & ectx) noexcept = 0;
			virtual void Send(DataBlock * data, ErrorContext * error, int * sent, IDispatchTask * hdlr, ErrorContext & ectx) noexcept = 0;
			virtual void Receive(int length, ErrorContext * error, SafePointer<DataBlock> * data, IDispatchTask * hdlr, ErrorContext & ectx) noexcept = 0;
			virtual void Close(bool ultimately, ErrorContext & ectx) noexcept = 0;
		};
		class INetworkListener : public Object
		{
		public:
			virtual void BindA(NetworkAddress * address, ErrorContext & ectx) noexcept = 0;
			virtual void BindB(NetworkAddress * address, NetworkIdentityDescriptor & idesc, ErrorContext & ectx) noexcept = 0;
			virtual void Accept(int limit, ErrorContext * error, SafePointer<INetworkChannel> * channel, SafePointer<NetworkAddress> * address, IDispatchTask * hdlr, ErrorContext & ectx) noexcept = 0;
			virtual void Close(ErrorContext & ectx) noexcept = 0;
		};
	}
}