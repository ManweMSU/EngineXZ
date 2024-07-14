#include <Network/Punycode.h>

#include "xe_netapi.h"
#include "xe_netapi_unix.h"
#include "xe_netapi_macosx.h"

#define XE_TRY_INTRO try {
#define XE_TRY_OUTRO(DRV) } catch (Engine::InvalidArgumentException &) { ectx.error_code = 3; ectx.error_subcode = 0; return DRV; } \
catch (Engine::InvalidFormatException &) { ectx.error_code = 4; ectx.error_subcode = 0; return DRV; } \
catch (Engine::InvalidStateException &) { ectx.error_code = 5; ectx.error_subcode = 0; return DRV; } \
catch (Engine::OutOfMemoryException &) { ectx.error_code = 2; ectx.error_subcode = 0; return DRV; } \
catch (Engine::IO::FileAccessException & e) { ectx.error_code = 6; ectx.error_subcode = e.code; return DRV; } \
catch (Engine::Exception &) { ectx.error_code = 1; ectx.error_subcode = 0; return DRV; } \
catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return DRV; }

namespace Engine
{
	namespace XE
	{
		class NetworkChannel : public INetworkChannel
		{
			SafePointer<INetworkChannel> _interface;
		public:
			NetworkChannel(void) {}
			virtual ~NetworkChannel(void) noexcept {}
			virtual string ToString(void) const override { return L"communicatio.canale"; }
			virtual void ConnectA(NetworkAddress * address, ErrorContext * error, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (_interface) _interface->ConnectA(address, error, hdlr, ectx); else {
					XE_TRY_INTRO
					_interface = CreateNetworkChannel();
					XE_TRY_OUTRO()
					_interface->ConnectA(address, error, hdlr, ectx);
				}
			}
			virtual void ConnectB(NetworkAddress * address, NetworkSecurityDescriptor & sec, ErrorContext * error, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (_interface) _interface->ConnectB(address, sec, error, hdlr, ectx); else {
					XE_TRY_INTRO
					_interface = CreateNetworkChannelS();
					XE_TRY_OUTRO()
					_interface->ConnectB(address, sec, error, hdlr, ectx);
				}
			}
			virtual void Send(DataBlock * data, ErrorContext * error, int * sent, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (_interface) _interface->Send(data, error, sent, hdlr, ectx);
				else SetXEError(ectx, 5, 0);
			}
			virtual void Receive(int length, ErrorContext * error, SafePointer<DataBlock> * data, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (_interface) _interface->Receive(length, error, data, hdlr, ectx);
				else SetXEError(ectx, 5, 0);
			}
			virtual void Close(bool ultimately, ErrorContext & ectx) noexcept override
			{
				if (_interface) _interface->Close(ultimately, ectx);
				else SetXEError(ectx, 5, 0);
			}
		};
		class NetworkListener : public INetworkListener
		{
			NetworkAddressFactory & _factory;
			SafePointer<INetworkListener> _interface;
		public:
			NetworkListener(NetworkAddressFactory & factory) : _factory(factory) {}
			virtual ~NetworkListener(void) noexcept {}
			virtual string ToString(void) const override { return L"communicatio.attentor"; }
			virtual void BindA(NetworkAddress * address, ErrorContext & ectx) noexcept override
			{
				if (_interface) _interface->BindA(address, ectx); else {
					XE_TRY_INTRO
					_interface = CreateNetworkListener(_factory);
					XE_TRY_OUTRO()
					_interface->BindA(address, ectx);
				}
			}
			virtual void BindB(NetworkAddress * address, NetworkIdentityDescriptor & idesc, ErrorContext & ectx) noexcept override
			{
				if (_interface) _interface->BindB(address, idesc, ectx); else {
					XE_TRY_INTRO
					_interface = CreateNetworkListenerS(_factory);
					XE_TRY_OUTRO()
					_interface->BindB(address, idesc, ectx);
				}
			}
			virtual void Accept(int limit, ErrorContext * error, SafePointer<INetworkChannel> * channel, SafePointer<NetworkAddress> * address, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (_interface) _interface->Accept(limit, error, channel, address, hdlr, ectx);
				else SetXEError(ectx, 5, 0);
			}
			virtual void Close(ErrorContext & ectx) noexcept override
			{
				if (_interface) _interface->Close(ectx);
				else SetXEError(ectx, 5, 0);
			}
		};

		class NetworkAPI : public IAPIExtension
		{
			static SafePointer<INetworkChannel> _create_channel(ErrorContext & ectx) noexcept { XE_TRY_INTRO return new NetworkChannel; XE_TRY_OUTRO(0) }
			static SafePointer<INetworkListener> _create_listener(NetworkAddressFactory & factory, ErrorContext & ectx) noexcept { XE_TRY_INTRO return new NetworkListener(factory); XE_TRY_OUTRO(0) }
			static string _convert_domain_to_punycode(const string & data, ErrorContext & ectx) noexcept { XE_TRY_INTRO return Network::DomainNameToPunycode(data); XE_TRY_OUTRO(L"") }
			static string _convert_punycode_to_domain(const string & data, ErrorContext & ectx) noexcept { XE_TRY_INTRO return Network::DomainNameToUnicode(data); XE_TRY_OUTRO(L"") }
			static string _convert_text_to_punycode(const string & data, ErrorContext & ectx) noexcept { XE_TRY_INTRO return Network::UnicodeToPunycode(data); XE_TRY_OUTRO(L"") }
			static string _convert_punycode_to_text(const string & data, ErrorContext & ectx) noexcept { XE_TRY_INTRO return Network::PunycodeToUnicode(data); XE_TRY_OUTRO(L"") }
			static SafePointer<DataBlock> _convert_base64_to_data(const string & data, ErrorContext & ectx) noexcept { XE_TRY_INTRO return ConvertFromBase64(data); XE_TRY_OUTRO(0) }
			static string _convert_data_to_base64(const void * data, int length, ErrorContext & ectx) noexcept { XE_TRY_INTRO return ConvertToBase64(data, length); XE_TRY_OUTRO(L"") }
			static void _network_init(ErrorContext & ectx) noexcept{ XE_TRY_INTRO NetworkEngineInit(); XE_TRY_OUTRO() }
		public:
			NetworkAPI(void) {}
			virtual ~NetworkAPI(void) override {}
			virtual const void * ExposeRoutine(const string & routine_name) noexcept override
			{
				if (string::Compare(routine_name, L"cm_fi") < 0) {
					if (string::Compare(routine_name, L"cm_cc") < 0) {
						if (string::Compare(routine_name, L"cm_ca") < 0) {
							if (string::Compare(routine_name, L"cm_bd") == 0) return reinterpret_cast<const void *>(&_convert_base64_to_data);
						} else {
							if (string::Compare(routine_name, L"cm_ca") == 0) return reinterpret_cast<const void *>(&_create_listener);
						}
					} else {
						if (string::Compare(routine_name, L"cm_db") < 0) {
							if (string::Compare(routine_name, L"cm_cc") == 0) return reinterpret_cast<const void *>(&_create_channel);
						} else {
							if (string::Compare(routine_name, L"cm_dp") < 0) {
								if (string::Compare(routine_name, L"cm_db") == 0) return reinterpret_cast<const void *>(&_convert_data_to_base64);
							} else {
								if (string::Compare(routine_name, L"cm_dp") == 0) return reinterpret_cast<const void *>(&_convert_domain_to_punycode);
							}
						}
					}
				} else {
					if (string::Compare(routine_name, L"cm_lp") < 0) {
						if (string::Compare(routine_name, L"cm_in") < 0) {
							if (string::Compare(routine_name, L"cm_fi") == 0) return reinterpret_cast<const void *>(&NetworkEngineStop);
						} else {
							if (string::Compare(routine_name, L"cm_la") < 0) {
								if (string::Compare(routine_name, L"cm_in") == 0) return reinterpret_cast<const void *>(&_network_init);
							} else {
								if (string::Compare(routine_name, L"cm_la") == 0) return reinterpret_cast<const void *>(&GetNetworkAddresses);
							}
						}
					} else {
						if (string::Compare(routine_name, L"cm_pd") < 0) {
							if (string::Compare(routine_name, L"cm_lp") == 0) return reinterpret_cast<const void *>(&_convert_text_to_punycode);
						} else {
							if (string::Compare(routine_name, L"cm_pl") < 0) {
								if (string::Compare(routine_name, L"cm_pd") == 0) return reinterpret_cast<const void *>(&_convert_punycode_to_domain);
							} else {
								if (string::Compare(routine_name, L"cm_pl") == 0) return reinterpret_cast<const void *>(&_convert_punycode_to_text);
							}
						}
					}
				}
				return 0;
			}
			virtual const void * ExposeInterface(const string & interface) noexcept override { return 0; }
		};

		void ClearXEError(ErrorContext & ectx) noexcept { ectx.error_code = ectx.error_subcode = 0; }
		void SetXEError(ErrorContext & ectx, int error, int suberror) noexcept { ectx.error_code = error; ectx.error_subcode = suberror; }
		void ActivateNetworkAPI(StandardLoader & ldr)
		{
			SafePointer<NetworkAPI> api = new NetworkAPI;
			if (!ldr.RegisterAPIExtension(api)) throw Exception();
		}
	}
}