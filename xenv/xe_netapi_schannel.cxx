#include "xe_netapi_schannel.h"

#ifdef ENGINE_WINDOWS

#define SECURITY_WIN32

#include <windows.h>
#include <sspi.h>
#include <schannel.h>

#pragma comment(lib, "Secur32.lib")

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
		SafePointer<INetworkChannel> CreateNetworkChannelS(NetworkAddress * address)
		{
			// TODO: IMPLEMENT
			throw Exception();
		}
		SafePointer<INetworkListener> CreateNetworkListenerS(NetworkAddress * address, NetworkAddressFactory & factory)
		{
			// TODO: IMPLEMENT
			throw Exception();
		}
	}
}
#endif