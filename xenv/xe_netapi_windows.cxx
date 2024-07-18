#include "xe_netapi_windows.h"

#ifdef ENGINE_WINDOWS

#include <Network/Punycode.h>

#include <WinSock2.h>
#include <Windows.h>
#include <ws2tcpip.h>
#include <MSWSock.h>
#include <atomic>

#pragma comment(lib, "Ws2_32.lib")

#undef ZeroMemory
#undef InterlockedIncrement
#undef InterlockedDecrement
#undef CreateSemaphore
#undef GetClassName
#undef interface
#undef min
#undef max

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
		void SetWinError(int error, ErrorContext & ectx) noexcept
		{
			if (!error) {
				SetXEError(ectx, 0x08, 0x00);
			} else {
				if (error == ERROR_FILE_NOT_FOUND) SetXEError(ectx, 0x06, 0x02);
				else if (error == ERROR_INVALID_HANDLE) SetXEError(ectx, 0x06, 0x06);
				else if (error == ERROR_NOT_ENOUGH_MEMORY) SetXEError(ectx, 0x02, 0);
				else if (error == ERROR_INVALID_PARAMETER) SetXEError(ectx, 0x03, 0);
				else if (error == ERROR_OPERATION_ABORTED) SetXEError(ectx, 0x08, 0x08);
				else if (error == ERROR_BROKEN_PIPE) SetXEError(ectx, 0x08, 0x08);
				else if (error == ERROR_PIPE_BUSY || error == ERROR_PIPE_LOCAL) SetXEError(ectx, 0x08, 0x07);
				else if (error == ERROR_PIPE_CONNECTED || error == ERROR_PIPE_LISTENING || error == ERROR_PIPE_NOT_CONNECTED) SetXEError(ectx, 0x05, 0);
				else SetXEError(ectx, 0x08, 0x01);
			}
		}
		void SetWinError(ErrorContext & ectx) noexcept { SetWinError(GetLastError(), ectx); }
		void SetWSAError(int error, ErrorContext & ectx) noexcept
		{
			if (!error) {
				SetXEError(ectx, 0x08, 0x00);
			} else if (error < 10009) {
				if (error == WSA_INVALID_HANDLE) SetXEError(ectx, 0x06, 0x06);
				else if (error == WSA_NOT_ENOUGH_MEMORY) SetXEError(ectx, 0x02, 0);
				else if (error == WSA_INVALID_PARAMETER) SetXEError(ectx, 0x03, 0);
				else if (error == WSA_OPERATION_ABORTED) SetXEError(ectx, 0x08, 0x08);
				else SetXEError(ectx, 0x08, 0x01);
			} else if (error < 10100) {
				if (error == WSAEBADF || error == WSAESTALE) SetXEError(ectx, 0x06, 0x06);
				else if (error == WSAEACCES) SetXEError(ectx, 0x06, 0x05);
				else if (error == WSAEFAULT) SetXEError(ectx, 0x03, 0);
				else if (error == WSAEINVAL) SetXEError(ectx, 0x06, 0x0C);
				else if (error == WSAEMFILE) SetXEError(ectx, 0x06, 0x04);
				else if (error == WSAENOTSOCK) SetXEError(ectx, 0x06, 0x06);
				else if (error == WSAEPROTOTYPE || error == WSAEPROTONOSUPPORT || error == WSAESOCKTNOSUPPORT) SetXEError(ectx, 0x08, 0x09);
				else if (error == WSAEPFNOSUPPORT || error == WSAEAFNOSUPPORT) SetXEError(ectx, 0x01, 0);
				else if (error == WSAEADDRINUSE) SetXEError(ectx, 0x08, 0x02);
				else if (error == WSAEADDRNOTAVAIL) SetXEError(ectx, 0x08, 0x03);
				else if (error == WSAENETDOWN) SetXEError(ectx, 0x08, 0x04);
				else if (error == WSAENETUNREACH) SetXEError(ectx, 0x08, 0x06);
				else if (error == WSAENETRESET || error == WSAECONNABORTED || error == WSAECONNRESET) SetXEError(ectx, 0x08, 0x08);
				else if (error == WSAENOBUFS) SetXEError(ectx, 0x06, 0x07);
				else if (error == WSAENOTCONN || error == WSAESHUTDOWN) SetXEError(ectx, 0x05, 0);
				else if (error == WSAETOOMANYREFS || error == WSAEPROCLIM) SetXEError(ectx, 0x06, 0x04);
				else if (error == WSAETIMEDOUT) SetXEError(ectx, 0x08, 0x0A);
				else if (error == WSAECONNREFUSED) SetXEError(ectx, 0x08, 0x07);
				else if (error == WSAELOOP) SetXEError(ectx, 0x06, 0x10);
				else if (error == WSAENAMETOOLONG) SetXEError(ectx, 0x06, 0x11);
				else if (error == WSAEHOSTDOWN || error == WSAEHOSTUNREACH) SetXEError(ectx, 0x08, 0x05);
				else if (error == WSAENOTEMPTY) SetXEError(ectx, 0x06, 0x0D);
				else if (error == WSAEUSERS || error == WSAEDQUOT) SetXEError(ectx, 0x06, 0x0A);
				else if (error == WSASYSNOTREADY || error == WSAVERNOTSUPPORTED) SetXEError(ectx, 0x08, 0x04);
				else if (error == WSANOTINITIALISED) SetXEError(ectx, 0x05, 0);
				else if (error == WSAEOPNOTSUPP) SetXEError(ectx, 0x06, 12);
				else SetXEError(ectx, 0x08, 0x01);
			} else {
				if (error == WSAEREFUSED || error == WSAHOST_NOT_FOUND || error == WSANO_DATA) SetXEError(ectx, 0x08, 0x0C);
				else if (error == WSATRY_AGAIN || error == WSANO_RECOVERY) SetXEError(ectx, 5, 0);
				else SetXEError(ectx, 0x08, 0x01);
			}
		}
		void SetWSAError(ErrorContext & ectx) noexcept { SetWSAError(WSAGetLastError(), ectx); }

		void LocalPipeName(string & dest, NetworkAddressLocal & address)
		{
			if (address.Name()[0] == L'\\') dest = address.Name();
			else dest = L"\\\\.\\pipe\\" + address.Name();
		}
		void SocketAddressInit(DataBlock & dest, NetworkAddress * address)
		{
			if (address) {
				if (reinterpret_cast<ClassSymbol *>(address->GetType())->GetClassName() == L"communicatio.adloquium_ipv4") {
					auto & addr = *reinterpret_cast<NetworkAddressIPv4 *>(address);
					dest.SetLength(sizeof(sockaddr_in));
					auto & sa = *reinterpret_cast<sockaddr_in *>(dest.GetBuffer());
					sa.sin_family = AF_INET;
					sa.sin_port = Network::InverseEndianess(addr.Port());
					MemoryCopy(&sa.sin_addr, &addr.IP(), 4);
					ZeroMemory(&sa.sin_zero, sizeof(sa.sin_zero));
				} else if (reinterpret_cast<ClassSymbol *>(address->GetType())->GetClassName() == L"communicatio.adloquium_ipv6") {
					auto & addr = *reinterpret_cast<NetworkAddressIPv6 *>(address);
					dest.SetLength(sizeof(sockaddr_in6));
					auto & sa = *reinterpret_cast<sockaddr_in6 *>(dest.GetBuffer());
					sa.sin6_family = AF_INET6;
					sa.sin6_port = Network::InverseEndianess(addr.Port());
					sa.sin6_flowinfo = 0;
					MemoryCopy(&sa.sin6_addr, &addr.IP(), 16);
					for (int i = 0; i < 16; i += 2) swap(sa.sin6_addr.s6_addr[i], sa.sin6_addr.s6_addr[i + 1]);
					sa.sin6_scope_id = 0;
				} else throw Exception();
			} else throw InvalidArgumentException();
		}
		void SocketAddressRead(void * src, NetworkAddressFactory * factory, NetworkAddress ** dest)
		{
			ErrorContext ectx;
			ClearXEError(ectx);
			auto & sa = *reinterpret_cast<const sockaddr *>(src);
			if (sa.sa_family == PF_INET) {
				auto address = factory->CreateIPv4(ectx);
				if (ectx.error_code) throw OutOfMemoryException();
				auto & sa_ipv4 = *reinterpret_cast<const sockaddr_in *>(src);
				MemoryCopy(&address->IP(), &sa_ipv4.sin_addr, 4);
				address->Port() = Network::InverseEndianess(sa_ipv4.sin_port);
				address->Retain();
				*dest = address;
			} else if (sa.sa_family == PF_INET6) {
				auto address = factory->CreateIPv6(ectx);
				if (ectx.error_code) throw OutOfMemoryException();
				auto & sa_ipv6 = *reinterpret_cast<sockaddr_in6 *>(src);
				for (int i = 0; i < 16; i += 2) swap(sa_ipv6.sin6_addr.s6_addr[i], sa_ipv6.sin6_addr.s6_addr[i + 1]);
				MemoryCopy(&address->IP(), &sa_ipv6.sin6_addr, 16);
				address->Port() = Network::InverseEndianess(sa_ipv6.sin6_port);
				address->Retain();
				*dest = address;
			} else throw InvalidStateException();
		}
		SOCKET CreateSocket(NetworkAddress * address, ErrorContext & ectx, int * protocol = 0) noexcept
		{
			SOCKET result = INVALID_SOCKET;
			if (address) {
				if (reinterpret_cast<ClassSymbol *>(address->GetType())->GetClassName() == L"communicatio.adloquium_ipv4") {
					result = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
					if (protocol) *protocol = PF_INET;
				} else if (reinterpret_cast<ClassSymbol *>(address->GetType())->GetClassName() == L"communicatio.adloquium_ipv6") {
					result = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP);
					if (protocol) *protocol = PF_INET6;
					DWORD value = FALSE;
					if (result != INVALID_SOCKET) setsockopt(result, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<char *>(&value), 4);
				} else SetXEError(ectx, 1, 0);
			} else SetXEError(ectx, 3, 0);
			if (result == INVALID_SOCKET) SetWSAError(ectx);
			return result;
		}
		SOCKET CreateSocket(int protocol, ErrorContext & ectx) noexcept
		{
			SOCKET result = socket(protocol, SOCK_STREAM, IPPROTO_TCP);
			if (result != INVALID_SOCKET && protocol == PF_INET6) {
				DWORD value = FALSE;
				setsockopt(result, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<char *>(&value), 4);
			}
			if (result == INVALID_SOCKET) SetWSAError(ectx);
			return result;
		}

		enum class RequestStatus { Complete, Await };
		enum class RequestServiceClass { Exclusive, Read, Write, Instant };
		class INetworkRequest : public Object
		{
		public:
			virtual RequestStatus InitiateRequest(void) noexcept = 0;
			virtual RequestStatus CompleteRequest(void) noexcept = 0;
			virtual void CancelRequest(void) noexcept = 0;
			virtual HANDLE GetHandle(void) noexcept = 0;
			virtual RequestServiceClass GetServiceClass(void) noexcept = 0;
		};
		class NetworkEventDispatch : public Object
		{
			static std::atomic_flag _general_sync;
			static uint _general_refcnt;
			static SafePointer<NetworkEventDispatch> _general_dispatch;
		private:
			typedef SafePointer<INetworkRequest> NetworkRequestRef;
			struct PendingRequestDesc {
				NetworkRequestRef exclusive;
				NetworkRequestRef read;
				NetworkRequestRef write;
				Volumes::Queue<NetworkRequestRef> pending;
			};
			class LocalRequestQueue {
				Volumes::Dictionary<HANDLE, PendingRequestDesc> state;
			public:
				LocalRequestQueue(void) {}
				void Schedule(NetworkRequestRef req)
				{
					auto hndl = req->GetHandle();
					auto cls = req->GetServiceClass();
					if (cls == RequestServiceClass::Instant) { req->InitiateRequest(); return; }
					bool created;
					auto item = state.FindElement(Volumes::KeyValuePair<HANDLE, PendingRequestDesc>(hndl, PendingRequestDesc()), true, &created);
					auto & desc = item->GetValue().value;
					NetworkRequestRef * launch_as_ptr = 0;
					if (cls == RequestServiceClass::Exclusive) {
						if (!desc.exclusive && !desc.read && !desc.write) launch_as_ptr = &desc.exclusive;
					} else if (cls == RequestServiceClass::Read) {
						if (!desc.exclusive && !desc.read) launch_as_ptr = &desc.read;
					} else if (cls == RequestServiceClass::Write) {
						if (!desc.exclusive && !desc.write) launch_as_ptr = &desc.write;
					}
					try {
						if (launch_as_ptr) {
							if (req->InitiateRequest() == RequestStatus::Await) *launch_as_ptr = req;
						} else desc.pending.Push(req);
					} catch (...) {
						if (created) state.BinaryTree::Remove(item);
						throw;
					}
				}
				void Process(HANDLE hndl)
				{
					auto item = state.GetElementByKey(hndl);
					if (item) {
						if (item->exclusive && item->exclusive->CompleteRequest() == RequestStatus::Complete) item->exclusive = 0;
						if (item->read && item->read->CompleteRequest() == RequestStatus::Complete) item->read = 0;
						if (item->write && item->write->CompleteRequest() == RequestStatus::Complete) item->write = 0;
						bool exclusive_vacant = !item->exclusive && !item->read && !item->write;
						bool read_vacant = !item->exclusive && !item->read;
						bool write_vacant = !item->exclusive && !item->write;
						auto current = item->pending.GetFirst();
						while (exclusive_vacant || read_vacant || write_vacant) {
							if (!current) break;
							auto next = current->GetNext();
							auto cls = current->GetValue()->GetServiceClass();
							if (current->GetValue()->GetHandle() != hndl) {
								try { Schedule(current->GetValue()); }
								catch (...) { current->GetValue()->CancelRequest(); }
								item->pending.Remove(current);
							} else {
								if (cls == RequestServiceClass::Exclusive) {
									if (exclusive_vacant) {
										if (current->GetValue()->InitiateRequest() == RequestStatus::Await) {
											item->exclusive = current->GetValue();
											exclusive_vacant = read_vacant = write_vacant = false;
										}
										item->pending.Remove(current);
									}
								} else if (cls == RequestServiceClass::Read) {
									if (read_vacant) {
										if (current->GetValue()->InitiateRequest() == RequestStatus::Await) {
											item->read = current->GetValue();
											exclusive_vacant = read_vacant = false;
										}
										item->pending.Remove(current);
									}
								} else if (cls == RequestServiceClass::Write) {
									if (write_vacant) {
										if (current->GetValue()->InitiateRequest() == RequestStatus::Await) {
											item->write = current->GetValue();
											exclusive_vacant = write_vacant = false;
										}
										item->pending.Remove(current);
									}
								}
							}
							current = next;
						}
						if (exclusive_vacant) state.Remove(hndl);
					}
				}
				void Cancel(void)
				{
					for (auto & q : state) {
						if (q.value.exclusive) q.value.exclusive->CancelRequest();
						if (q.value.read) q.value.read->CancelRequest();
						if (q.value.write) q.value.write->CancelRequest();
						for (auto & p : q.value.pending) p->CancelRequest();
					}
				}
			};
		private:
			volatile bool _in_critical_section;
			DWORD _dispatch_thread_id;
			HANDLE _io_port;
			HANDLE _queue_sync;
			HANDLE _dispatch_thread;
			Volumes::Queue<NetworkRequestRef> _queue;
		private:
			NetworkEventDispatch(void) : _in_critical_section(false)
			{
				WSADATA data;
				ZeroMemory(&data, sizeof(data));
				if (WSAStartup(MAKEWORD(2, 2), &data)) throw Exception();
				_io_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 1);
				if (!_io_port) { WSACleanup(); throw OutOfMemoryException(); }
				_queue_sync = CreateSemaphoreW(0, 1, 1, 0);
				if (!_queue_sync) { WSACleanup(); CloseHandle(_io_port); throw OutOfMemoryException(); }
				_dispatch_thread = ::CreateThread(0, 0x100000, _dispatch_thread_proc, this, 0, &_dispatch_thread_id);
				if (!_dispatch_thread) { WSACleanup(); CloseHandle(_io_port); CloseHandle(_queue_sync); throw OutOfMemoryException(); }
			}
			static DWORD WINAPI _dispatch_thread_proc(LPVOID argument) noexcept
			{
				CoInitializeEx(0, COINIT_APARTMENTTHREADED);
				auto self = reinterpret_cast<NetworkEventDispatch *>(argument);
				LocalRequestQueue local_queue;
				try {
					while (true) {
						DWORD transferred;
						ULONG_PTR user;
						LPOVERLAPPED overlapped;
						if (!GetQueuedCompletionStatus(self->_io_port, &transferred, &user, &overlapped, INFINITE) && !overlapped) throw Exception();
						auto request_handle = reinterpret_cast<HANDLE>(user);
						if (request_handle == INVALID_HANDLE_VALUE) {
							if (transferred) {
								WaitForSingleObject(self->_queue_sync, INFINITE);
								self->_in_critical_section = true;
								try {
									Volumes::Queue< SafePointer<INetworkRequest> >::Element * item;
									while (item = self->_queue.GetFirst()) {
										auto value = item->GetValue();
										local_queue.Schedule(value);
										self->_queue.RemoveFirst();
									}
								} catch (...) {
									self->_in_critical_section = false;
									ReleaseSemaphore(self->_queue_sync, 1, 0);
									throw;
								}
								self->_in_critical_section = false;
								ReleaseSemaphore(self->_queue_sync, 1, 0);
							} else { local_queue.Cancel(); break; }
						} else local_queue.Process(request_handle);
					}
				} catch (...) { local_queue.Cancel(); return 1; }
				return 0;
			}
			void _schedule_request(INetworkRequest * rqn, ErrorContext & ectx) noexcept
			{
				bool sync = true;
				if (GetCurrentThreadId() == _dispatch_thread_id && _in_critical_section) sync = false;
				XE_TRY_INTRO
				if (sync) WaitForSingleObject(_queue_sync, INFINITE);
				try {
					SafePointer<INetworkRequest> request;
					SafePointer<Object> object;
					request.SetRetain(rqn);
					_queue.Push(request);
					if (sync) ReleaseSemaphore(_queue_sync, 1, 0);
				} catch (...) {
					if (sync) ReleaseSemaphore(_queue_sync, 1, 0);
					throw;
				}
				if (!PostQueuedCompletionStatus(_io_port, 1, reinterpret_cast<ULONG_PTR>(INVALID_HANDLE_VALUE), 0)) throw OutOfMemoryException();
				XE_TRY_OUTRO()
			}
			void _associate_handle(HANDLE file, ErrorContext & ectx) noexcept
			{
				auto result = CreateIoCompletionPort(file, _io_port, reinterpret_cast<ULONG_PTR>(file), 0);
				if (result != _io_port) SetXEError(ectx, 2, 0);
			}
			void _shutdown(void) noexcept { PostQueuedCompletionStatus(_io_port, 0, reinterpret_cast<ULONG_PTR>(INVALID_HANDLE_VALUE), 0); }
		public:
			virtual ~NetworkEventDispatch(void) override
			{
				for (auto & r : _queue) r->CancelRequest();
				CloseHandle(_io_port);
				CloseHandle(_queue_sync);
				CloseHandle(_dispatch_thread);
				WSACleanup();
			}
			static void AssociateHandle(HANDLE file, ErrorContext & ectx) noexcept
			{
				while (_general_sync.test_and_set(std::memory_order_acquire));
				auto dispatch = _general_dispatch;
				_general_sync.clear(std::memory_order_release);
				if (!dispatch) { SetXEError(ectx, 0x8, 0x4); return; }
				dispatch->_associate_handle(file, ectx);
			}
			static void ScheduleRequest(INetworkRequest * rqn, ErrorContext & ectx) noexcept
			{
				while (_general_sync.test_and_set(std::memory_order_acquire));
				auto dispatch = _general_dispatch;
				_general_sync.clear(std::memory_order_release);
				if (!dispatch) { SetXEError(ectx, 0x8, 0x4); return; }
				dispatch->_schedule_request(rqn, ectx);
			}
			static void Initialize(void)
			{
				while (_general_sync.test_and_set(std::memory_order_acquire));
				if (InterlockedIncrement(_general_refcnt) == 1) try {
					_general_dispatch = new NetworkEventDispatch;
				} catch (...) {
					InterlockedDecrement(_general_refcnt);
					_general_sync.clear(std::memory_order_release);
					throw;
				}
				_general_sync.clear(std::memory_order_release);
			}
			static void Shutdown(void) noexcept
			{
				SafePointer<NetworkEventDispatch> dispatch;
				while (_general_sync.test_and_set(std::memory_order_acquire));
				if (InterlockedDecrement(_general_refcnt) == 0) {
					dispatch = _general_dispatch;
					dispatch->_shutdown();
					_general_dispatch.SetReference(0);
				}
				_general_sync.clear(std::memory_order_release);
				if (dispatch) WaitForSingleObject(dispatch->_dispatch_thread, INFINITE);
			}
		};

		std::atomic_flag NetworkEventDispatch::_general_sync = ATOMIC_FLAG_INIT;
		uint NetworkEventDispatch::_general_refcnt = 0;
		SafePointer<NetworkEventDispatch> NetworkEventDispatch::_general_dispatch;
		
		class LocalNetworkChannel : public INetworkChannel
		{
			class InterfaceHolder : public INetworkRequest
			{
				HANDLE _pipe;
				volatile bool _read_closed;
				volatile bool _write_closed;
			public:
				InterfaceHolder(void) : _pipe(INVALID_HANDLE_VALUE), _read_closed(false), _write_closed(false) {}
				virtual ~InterfaceHolder(void) override { if (_pipe != INVALID_HANDLE_VALUE) CloseHandle(_pipe); }
				virtual RequestStatus InitiateRequest(void) noexcept override
				{
					if (_pipe != INVALID_HANDLE_VALUE) CancelIo(_pipe);
					return RequestStatus::Complete;
				}
				virtual RequestStatus CompleteRequest(void) noexcept override { return InitiateRequest(); }
				virtual void CancelRequest(void) noexcept override { InitiateRequest(); }
				virtual HANDLE GetHandle(void) noexcept override { return _pipe; }
				virtual RequestServiceClass GetServiceClass(void) noexcept override { return RequestServiceClass::Instant; }
				HANDLE & Handle(void) noexcept { return _pipe; }
				volatile bool & ReadClosed(void) noexcept { return _read_closed; }
				volatile bool & WriteClosed(void) noexcept { return _write_closed; }
				void Finalize(void) noexcept
				{
					if (_pipe != INVALID_HANDLE_VALUE) {
						ErrorContext ectx;
						ClearXEError(ectx);
						NetworkEventDispatch::ScheduleRequest(this, ectx);
						if (ectx.error_code) { CloseHandle(_pipe); _pipe = INVALID_HANDLE_VALUE; }
					}
				}
			};
			class ConnectRequest : public Object
			{
				volatile bool _connection_cancelled;
				SafePointer<InterfaceHolder> _interface;
				string _system_path;
				ErrorContext * _error;
				SafePointer<IDispatchTask> _handler;
			private:
				bool _try_connect(void) noexcept
				{
					HANDLE pipe = CreateFileW(_system_path, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
					if (pipe == INVALID_HANDLE_VALUE) {
						auto errid = GetLastError();
						if (errid == ERROR_PIPE_BUSY) return false;
						if (_error) SetWinError(errid, *_error);
						if (_handler) _handler->DoTask(0);
					} else {
						ErrorContext ectx;
						ClearXEError(ectx);
						NetworkEventDispatch::AssociateHandle(pipe, ectx);
						if (ectx.error_code) CloseHandle(pipe);
						else _interface->Handle() = pipe;
						if (_error) *_error = ectx;
						if (_handler) _handler->DoTask(0);
					}
					return true;
				}
				static DWORD WINAPI _connect_thread(LPVOID argument) noexcept
				{
					CoInitializeEx(0, COINIT_APARTMENTTHREADED);
					auto req = reinterpret_cast<ConnectRequest *>(argument);
					while (true) {
						if (WaitNamedPipeW(req->_system_path, 1000)) {
							if (req->_try_connect()) break;
						} else {
							if (GetLastError() != ERROR_SEM_TIMEOUT && req->_try_connect()) break;
						}
						if (req->_connection_cancelled) {
							if (req->_error) SetXEError(*req->_error, 0x8, 0x4);
							if (req->_handler) req->_handler->DoTask(0);
							break;
						}
					}
					req->_handler.SetReference(0);
					req->_interface.SetReference(0);
					req->_system_path = L"";
					req->Release();
					return 0;					
				}
			public:
				ConnectRequest(InterfaceHolder * interface, NetworkAddress * address, ErrorContext * error, IDispatchTask * hdlr) : _connection_cancelled(false)
				{
					_interface.SetRetain(interface);
					auto & addr = *static_cast<NetworkAddressLocal *>(address);
					LocalPipeName(_system_path, addr);
					_error = error;
					_handler.SetRetain(hdlr);
				}
				virtual ~ConnectRequest(void) override {}
				void InitiateRequest(ErrorContext & ectx) noexcept
				{
					Retain();
					HANDLE thread = ::CreateThread(0, 0x100000, _connect_thread, this, 0, 0);
					if (!thread) { Release(); SetXEError(ectx, 2, 0); } else CloseHandle(thread);
				}
				void CancelRequest(void) noexcept { _connection_cancelled = true; }
			};
			class SendRequest : public INetworkRequest
			{
				SafePointer<InterfaceHolder> _interface;
				SafePointer<DataBlock> _buffer;
				ErrorContext * _error;
				int * _sent;
				SafePointer<IDispatchTask> _handler;
				OVERLAPPED _overlapped;
			public:
				SendRequest(InterfaceHolder * interface, DataBlock * data, ErrorContext * error, int * sent, IDispatchTask * hdlr)
				{
					_interface.SetRetain(interface);
					_buffer.SetRetain(data);
					_error = error;
					_sent = sent;
					_handler.SetRetain(hdlr);
				}
				virtual ~SendRequest(void) override {}
				virtual RequestStatus InitiateRequest(void) noexcept override
				{
					if (_interface->WriteClosed()) {
						if (_error) SetXEError(*_error, 5, 0);
						if (_sent) *_sent = 0;
						if (_handler) _handler->DoTask(0);
						return RequestStatus::Complete;
					}
					ZeroMemory(&_overlapped, sizeof(_overlapped));
					DWORD num_sent = 0;
					if (WriteFile(_interface->Handle(), _buffer->GetBuffer(), _buffer->Length(), &num_sent, &_overlapped)) {
						if (_error) ClearXEError(*_error);
						if (_sent) *_sent = _buffer->Length();
						if (_handler) _handler->DoTask(0);
						return RequestStatus::Complete;
					} else {
						auto errid = GetLastError();
						if (errid == ERROR_IO_PENDING) {
							return RequestStatus::Await;
						} else {
							if (_error) SetWinError(errid, *_error);
							if (_sent) *_sent = 0;
							if (_handler) _handler->DoTask(0);
							return RequestStatus::Complete;
						}
					}
				}
				virtual RequestStatus CompleteRequest(void) noexcept override
				{
					if (_interface->WriteClosed()) {
						if (_error) SetXEError(*_error, 5, 0);
						if (_sent) *_sent = 0;
						if (_handler) _handler->DoTask(0);
						return RequestStatus::Complete;
					}
					DWORD transferred;
					if (GetOverlappedResult(_interface->Handle(), &_overlapped, &transferred, FALSE)) {
						if (_error) ClearXEError(*_error);
						if (_sent) *_sent = transferred;
						if (_handler) _handler->DoTask(0);
						return RequestStatus::Complete;
					} else {
						auto errid = GetLastError();
						if (errid == ERROR_IO_INCOMPLETE) {
							return RequestStatus::Await;
						} else {
							if (_error) SetWinError(errid, *_error);
							if (_sent) *_sent = transferred;
							if (_handler) _handler->DoTask(0);
							return RequestStatus::Complete;
						}
					}
				}
				virtual void CancelRequest(void) noexcept override
				{
					if (_error) SetXEError(*_error, 0x8, 0x4);
					if (_sent) *_sent = 0;
					if (_handler) _handler->DoTask(0);
				}
				virtual HANDLE GetHandle(void) noexcept override { return _interface->GetHandle(); }
				virtual RequestServiceClass GetServiceClass(void) noexcept override { return RequestServiceClass::Write; }
			};
			class ReceiveRequest : public INetworkRequest
			{
				SafePointer<InterfaceHolder> _interface;
				SafePointer<DataBlock> _buffer;
				ErrorContext * _error;
				SafePointer<DataBlock> * _data;
				SafePointer<IDispatchTask> _handler;
				OVERLAPPED _overlapped;
			public:
				ReceiveRequest(InterfaceHolder * interface, int length, ErrorContext * error, SafePointer<DataBlock> * data, IDispatchTask * hdlr)
				{
					_interface.SetRetain(interface);
					_buffer = new DataBlock(1);
					_buffer->SetLength(length);
					_error = error;
					_data = data;
					_handler.SetRetain(hdlr);
				}
				virtual ~ReceiveRequest(void) override {}
				virtual RequestStatus InitiateRequest(void) noexcept override
				{
					if (_interface->ReadClosed()) {
						_buffer->SetLength(0);
						if (_error) ClearXEError(*_error);
						if (_data) *_data = _buffer;
						if (_handler) _handler->DoTask(0);
						return RequestStatus::Complete;
					}
					ZeroMemory(&_overlapped, sizeof(_overlapped));
					DWORD num_received = 0;
					if (ReadFile(_interface->Handle(), _buffer->GetBuffer(), _buffer->Length(), &num_received, &_overlapped)) {
						if (!num_received) _interface->ReadClosed() = true;
						_buffer->SetLength(num_received);
						if (_error) ClearXEError(*_error);
						if (_data) *_data = _buffer;
						if (_handler) _handler->DoTask(0);
						return RequestStatus::Complete;
					} else {
						auto errid = GetLastError();
						if (errid == ERROR_IO_PENDING) {
							return RequestStatus::Await;
						} else {
							if (_error) SetWinError(errid, *_error);
							if (_data) *_data = 0;
							if (_handler) _handler->DoTask(0);
							return RequestStatus::Complete;
						}
					}
				}
				virtual RequestStatus CompleteRequest(void) noexcept override
				{
					if (_interface->ReadClosed()) {
						_buffer->SetLength(0);
						if (_error) ClearXEError(*_error);
						if (_data) *_data = _buffer;
						if (_handler) _handler->DoTask(0);
						return RequestStatus::Complete;
					}
					DWORD transferred;
					if (GetOverlappedResult(_interface->Handle(), &_overlapped, &transferred, FALSE)) {
						if (!transferred) _interface->ReadClosed() = true;
						_buffer->SetLength(transferred);
						if (_error) ClearXEError(*_error);
						if (_data) *_data = _buffer;
						if (_handler) _handler->DoTask(0);
						return RequestStatus::Complete;
					} else {
						auto errid = GetLastError();
						if (errid == ERROR_IO_INCOMPLETE) {
							return RequestStatus::Await;
						} else {
							if (_error) SetWinError(errid, *_error);
							if (_data) *_data = 0;
							if (_handler) _handler->DoTask(0);
							return RequestStatus::Complete;
						}
					}
				}
				virtual void CancelRequest(void) noexcept override
				{
					if (_error) SetXEError(*_error, 0x8, 0x4);
					if (_data) *_data = 0;
					if (_handler) _handler->DoTask(0);
				}
				virtual HANDLE GetHandle(void) noexcept override { return _interface->GetHandle(); }
				virtual RequestServiceClass GetServiceClass(void) noexcept override { return RequestServiceClass::Read; }
			};
		private:
			SafePointer<InterfaceHolder> _interface;
			SafePointer<ConnectRequest> _last_connect;
		public:
			LocalNetworkChannel(void) { _interface = new InterfaceHolder; }
			LocalNetworkChannel(HANDLE hndl) { _interface = new InterfaceHolder; _interface->Handle() = hndl; }
			virtual ~LocalNetworkChannel(void) override { _interface->Finalize(); if (_last_connect) _last_connect->CancelRequest(); }
			virtual string ToString(void) const override { return L"communicatio.canale"; }
			virtual void ConnectA(NetworkAddress * address, ErrorContext * error, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (!address) { SetXEError(ectx, 3, 0); return; }
				if (reinterpret_cast<ClassSymbol *>(address->GetType())->GetClassName() != L"communicatio.adloquium_localis") { SetXEError(ectx, 1, 0); return; }
				if (_interface->Handle() != INVALID_HANDLE_VALUE || _last_connect) { SetXEError(ectx, 5, 0); return; }
				XE_TRY_INTRO
				_last_connect = new ConnectRequest(_interface, address, error, hdlr);
				_last_connect->InitiateRequest(ectx);
				XE_TRY_OUTRO()
			}
			virtual void ConnectB(NetworkAddress * address, NetworkSecurityDescriptor & sec, ErrorContext * error, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override { SetXEError(ectx, 1, 0); }
			virtual void Send(DataBlock * data, ErrorContext * error, int * sent, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (_interface->Handle() == INVALID_HANDLE_VALUE) { SetXEError(ectx, 5, 0); return; }
				if (!data || !data->Length()) {
					if (error) ClearXEError(*error);
					if (sent) *sent = 0;
					if (hdlr) hdlr->DoTask(0);
					return;
				}
				XE_TRY_INTRO
				SafePointer<SendRequest> request = new SendRequest(_interface, data, error, sent, hdlr);
				NetworkEventDispatch::ScheduleRequest(request, ectx);
				XE_TRY_OUTRO()
			}
			virtual void Receive(int length, ErrorContext * error, SafePointer<DataBlock> * data, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (_interface->Handle() == INVALID_HANDLE_VALUE) { SetXEError(ectx, 5, 0); return; }
				if (length < 0) { SetXEError(ectx, 3, 0); return; }
				XE_TRY_INTRO
				if (!length) {
					SafePointer<DataBlock> responce = new DataBlock(1);
					if (error) ClearXEError(*error);
					if (data) *data = responce;
					if (hdlr) hdlr->DoTask(0);
					return;
				}
				SafePointer<ReceiveRequest> request = new ReceiveRequest(_interface, length, error, data, hdlr);
				NetworkEventDispatch::ScheduleRequest(request, ectx);
				XE_TRY_OUTRO()
			}
			virtual void Close(bool ultimately, ErrorContext & ectx) noexcept override
			{
				if (_interface->Handle() == INVALID_HANDLE_VALUE) { SetXEError(ectx, 5, 0); return; }
				if (!_interface->WriteClosed()) {
					_interface->WriteClosed() = true;
					OVERLAPPED overlapped;
					ZeroMemory(&overlapped, sizeof(overlapped));
					overlapped.hEvent = CreateEventW(0, TRUE, FALSE, 0);
					if (overlapped.hEvent) {
						FlushFileBuffers(_interface->Handle());
						DWORD written;
						if (!WriteFile(_interface->Handle(), 0, 0, &written, &overlapped)) {
							auto errid = GetLastError();
							if (errid == ERROR_IO_PENDING) WaitForSingleObject(overlapped.hEvent, INFINITE);
						}
						CloseHandle(overlapped.hEvent);
					}
				}
				if (ultimately) {
					FlushFileBuffers(_interface->Handle());
					_interface->Finalize();
				}
			}
		};
		class LocalNetworkListener : public INetworkListener
		{
			class InterfaceHolder : public INetworkRequest
			{
				HANDLE _pipe;
			public:
				InterfaceHolder(void) : _pipe(INVALID_HANDLE_VALUE) {}
				virtual ~InterfaceHolder(void) override { if (_pipe != INVALID_HANDLE_VALUE) CloseHandle(_pipe); }
				virtual RequestStatus InitiateRequest(void) noexcept override
				{
					if (_pipe != INVALID_HANDLE_VALUE) CancelIo(_pipe);
					return RequestStatus::Complete;
				}
				virtual RequestStatus CompleteRequest(void) noexcept override { return InitiateRequest(); }
				virtual void CancelRequest(void) noexcept override { InitiateRequest(); }
				virtual HANDLE GetHandle(void) noexcept override { return _pipe; }
				virtual RequestServiceClass GetServiceClass(void) noexcept override { return RequestServiceClass::Instant; }
				HANDLE & Handle(void) noexcept { return _pipe; }
				void Finalize(void) noexcept
				{
					if (_pipe != INVALID_HANDLE_VALUE) {
						ErrorContext ectx;
						ClearXEError(ectx);
						NetworkEventDispatch::ScheduleRequest(this, ectx);
						if (ectx.error_code) { CloseHandle(_pipe); _pipe = INVALID_HANDLE_VALUE; }
					}
				}
			};
			class AcceptRequest : public INetworkRequest
			{
				int _num_uses;
				string _system_path;
				SafePointer<InterfaceHolder> _interface;
				NetworkAddressFactory & _factory;
				ErrorContext * _error;
				SafePointer<INetworkChannel> * _channel;
				SafePointer<NetworkAddress> * _address;
				SafePointer<IDispatchTask> _handler;
				OVERLAPPED _overlapped;
			private:
				bool _allocate_objects(INetworkChannel ** channel, NetworkAddress ** address, ErrorContext & ectx) noexcept
				{
					HANDLE subst = CreateNamedPipeW(_system_path, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
						PIPE_TYPE_MESSAGE | PIPE_REJECT_REMOTE_CLIENTS, PIPE_UNLIMITED_INSTANCES, 0x400, 0x400, NMPWAIT_USE_DEFAULT_WAIT, 0);
					if (subst == INVALID_HANDLE_VALUE) { SetWinError(ectx); return true; }
					NetworkEventDispatch::AssociateHandle(subst, ectx);
					if (ectx.error_code) { CloseHandle(subst); return true; }
					try { *channel = new LocalNetworkChannel(_interface->Handle()); } catch (...) {
						SetXEError(ectx, 2, 0);
						CloseHandle(_interface->Handle());
						_interface->Handle() = subst;
						return false;
					}
					_interface->Handle() = subst;
					auto addr = _factory.CreateLocal(ectx);
					if (!ectx.error_code && addr) {
						*address = addr.Inner();
						addr->Retain();
					}
					return false;
				}
				void _accept_post_process(void) noexcept
				{
					ErrorContext ectx;
					SafePointer<INetworkChannel> channel;
					SafePointer<NetworkAddress> address;
					ClearXEError(ectx);
					bool halt = _allocate_objects(channel.InnerRef(), address.InnerRef(), ectx);
					if (_error) *_error = ectx;
					if (ectx.error_code) {
						if (_channel) *_channel = 0;
						if (_address) *_address = 0;
					} else {
						if (_channel) *_channel = channel;
						if (_address) *_address = address;
					}
					if (_handler) _handler->DoTask(0);
					if (_num_uses > 0) _num_uses--;
					if (halt) _num_uses = 0;
					if (_num_uses) {
						if (_channel) *_channel = 0;
						if (_address) *_address = 0;
						ClearXEError(ectx);
						NetworkEventDispatch::ScheduleRequest(this, ectx);
						if (ectx.error_code) CancelRequest();
					}
				}
			public:
				AcceptRequest(
					const string & system_path, InterfaceHolder * interface, NetworkAddressFactory & factory,
					int limit, ErrorContext * error, SafePointer<INetworkChannel> * channel,
					SafePointer<NetworkAddress> * address, IDispatchTask * hdlr) :
						_num_uses(limit), _system_path(system_path), _factory(factory),
						_error(error), _channel(channel), _address(address)
				{ _interface.SetRetain(interface); _handler.SetRetain(hdlr); }
				virtual ~AcceptRequest(void) override {}
				virtual RequestStatus InitiateRequest(void) noexcept override
				{
					ZeroMemory(&_overlapped, sizeof(_overlapped));
					if (ConnectNamedPipe(_interface->Handle(), &_overlapped)) {
						_accept_post_process();
						return RequestStatus::Complete;
					} else {
						int errid = GetLastError();
						if (errid == ERROR_IO_PENDING) {
							return RequestStatus::Await;
						} else if (errid == ERROR_PIPE_CONNECTED) {
							_accept_post_process();
							return RequestStatus::Complete;
						} else {
							if (_error) SetWinError(errid, *_error);
							if (_channel) *_channel = 0;
							if (_address) *_address = 0;
							if (_handler) _handler->DoTask(0);
							return RequestStatus::Complete;
						}
					}
				}
				virtual RequestStatus CompleteRequest(void) noexcept override
				{
					DWORD transferred;
					if (GetOverlappedResult(_interface->Handle(), &_overlapped, &transferred, FALSE)) {
						_accept_post_process();
						return RequestStatus::Complete;
					} else {
						int errid = GetLastError();
						if (errid == ERROR_IO_INCOMPLETE) {
							return RequestStatus::Await;
						} else if (errid == ERROR_PIPE_CONNECTED) {
							_accept_post_process();
							return RequestStatus::Complete;
						} else {
							if (_error) SetWinError(errid, *_error);
							if (_channel) *_channel = 0;
							if (_address) *_address = 0;
							if (_handler) _handler->DoTask(0);
							return RequestStatus::Complete;
						}
					}
				}
				virtual void CancelRequest(void) noexcept override
				{
					if (_error) SetXEError(*_error, 0x8, 0x4);
					if (_handler) _handler->DoTask(0);
				}
				virtual HANDLE GetHandle(void) noexcept override { return _interface->GetHandle(); }
				virtual RequestServiceClass GetServiceClass(void) noexcept override { return RequestServiceClass::Read; }
			};
		private:
			string _system_path;
			SafePointer<InterfaceHolder> _interface;
			NetworkAddressFactory & _factory;
		public:
			LocalNetworkListener(NetworkAddressFactory & factory) : _factory(factory) { _interface = new InterfaceHolder; }
			virtual ~LocalNetworkListener(void) override { _interface->Finalize(); }
			virtual string ToString(void) const override { return L"communicatio.attentor"; }
			virtual void BindA(NetworkAddress * address, ErrorContext & ectx) noexcept override
			{
				if (!address) { SetXEError(ectx, 3, 0); return; }
				if (reinterpret_cast<ClassSymbol *>(address->GetType())->GetClassName() != L"communicatio.adloquium_localis") { SetXEError(ectx, 1, 0); return; }
				if (_interface->Handle() != INVALID_HANDLE_VALUE) { SetXEError(ectx, 5, 0); return; }
				XE_TRY_INTRO
				auto & addr = *static_cast<NetworkAddressLocal *>(address);
				LocalPipeName(_system_path, addr);
				_interface->Handle() = CreateNamedPipeW(_system_path, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
					PIPE_TYPE_MESSAGE | PIPE_REJECT_REMOTE_CLIENTS, PIPE_UNLIMITED_INSTANCES, 0x400, 0x400, NMPWAIT_USE_DEFAULT_WAIT, 0);
				if (_interface->Handle() == INVALID_HANDLE_VALUE) {
					auto errid = GetLastError();
					if (errid == ERROR_ACCESS_DENIED) SetXEError(ectx, 8, 2);
					else if (errid == ERROR_INVALID_NAME) SetXEError(ectx, 8, 3);
					else if (errid == ERROR_TOO_MANY_OPEN_FILES) SetXEError(ectx, 6, 4);
					else if (errid == ERROR_NOT_ENOUGH_MEMORY || errid == ERROR_OUTOFMEMORY) SetXEError(ectx, 6, 7);
					else SetXEError(ectx, 8, 1);
					_system_path = L"";
					return;
				}
				NetworkEventDispatch::AssociateHandle(_interface->Handle(), ectx);
				XE_TRY_OUTRO()
			}
			virtual void BindB(NetworkAddress * address, NetworkIdentityDescriptor & idesc, ErrorContext & ectx) noexcept override { SetXEError(ectx, 1, 0); }
			virtual void Accept(int limit, ErrorContext * error, SafePointer<INetworkChannel> * channel, SafePointer<NetworkAddress> * address, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (_interface->Handle() == INVALID_HANDLE_VALUE) { SetXEError(ectx, 5, 0); return; }
				if (!limit) return;
				XE_TRY_INTRO
				SafePointer<AcceptRequest> request = new AcceptRequest(_system_path, _interface, _factory, limit > 0 ? limit : -1, error, channel, address, hdlr);
				NetworkEventDispatch::ScheduleRequest(request, ectx);
				XE_TRY_OUTRO()
			}
			virtual void Close(ErrorContext & ectx) noexcept override { if (_interface->Handle() == INVALID_HANDLE_VALUE) { SetXEError(ectx, 5, 0); return; } _interface->Finalize(); }
		};
		class WSANetworkChannel : public INetworkChannel
		{
			class InterfaceHolder : public INetworkRequest
			{
				SOCKET _sock;
			public:
				InterfaceHolder(void) : _sock(INVALID_SOCKET) {}
				virtual ~InterfaceHolder(void) override { if (_sock != INVALID_SOCKET) closesocket(_sock); }
				virtual RequestStatus InitiateRequest(void) noexcept override
				{
					if (_sock != INVALID_SOCKET) CancelIo(reinterpret_cast<HANDLE>(_sock));
					return RequestStatus::Complete;
				}
				virtual RequestStatus CompleteRequest(void) noexcept override { return InitiateRequest(); }
				virtual void CancelRequest(void) noexcept override { InitiateRequest(); }
				virtual HANDLE GetHandle(void) noexcept override { return reinterpret_cast<HANDLE>(_sock); }
				virtual RequestServiceClass GetServiceClass(void) noexcept override { return RequestServiceClass::Instant; }
				SOCKET & Socket(void) noexcept { return _sock; }
				void Finalize(void) noexcept
				{
					if (_sock != INVALID_SOCKET) {
						ErrorContext ectx;
						ClearXEError(ectx);
						NetworkEventDispatch::ScheduleRequest(this, ectx);
						if (ectx.error_code) { closesocket(_sock); _sock = INVALID_SOCKET; }
					}
				}
			};
			class ConnectRequest : public INetworkRequest
			{
				SafePointer<InterfaceHolder> _interface;
				DataBlock _sa;
				ErrorContext * _error;
				SafePointer<IDispatchTask> _handler;
				OVERLAPPED _overlapped;
			public:
				ConnectRequest(InterfaceHolder * interface, NetworkAddress * address, ErrorContext * error, IDispatchTask * hdlr) : _sa(1)
				{
					_interface.SetRetain(interface);
					SocketAddressInit(_sa, address);
					_error = error;
					_handler.SetRetain(hdlr);
				}
				virtual ~ConnectRequest(void) override {}
				virtual RequestStatus InitiateRequest(void) noexcept override
				{
					GUID GUID_ConnectEx = WSAID_CONNECTEX;
					LPFN_CONNECTEX ConnectEx;
					DWORD Size_ConnectEx;
					auto status = WSAIoctl(_interface->Socket(), SIO_GET_EXTENSION_FUNCTION_POINTER, &GUID_ConnectEx, sizeof(GUID_ConnectEx), &ConnectEx, sizeof(ConnectEx), &Size_ConnectEx, 0, 0);
					if (status == SOCKET_ERROR) {
						if (_error) SetWSAError(*_error);
						if (_handler) _handler->DoTask(0);
						return RequestStatus::Complete;
					}
					auto sa = reinterpret_cast<sockaddr *>(_sa.GetBuffer());
					if (sa->sa_family == PF_INET) {
						sockaddr_in anyaddr;
						ZeroMemory(&anyaddr, sizeof(anyaddr));
						anyaddr.sin_family = PF_INET;
						status = bind(_interface->Socket(), reinterpret_cast<sockaddr *>(&anyaddr), sizeof(anyaddr));
					} else if (sa->sa_family == PF_INET6) {
						sockaddr_in6 anyaddr;
						ZeroMemory(&anyaddr, sizeof(anyaddr));
						anyaddr.sin6_family = PF_INET6;
						status = bind(_interface->Socket(), reinterpret_cast<sockaddr *>(&anyaddr), sizeof(anyaddr));
					} else {
						if (_error) SetXEError(*_error, 5, 0);
						if (_handler) _handler->DoTask(0);
						return RequestStatus::Complete;
					}
					if (status == SOCKET_ERROR) {
						if (_error) SetWSAError(*_error);
						if (_handler) _handler->DoTask(0);
						return RequestStatus::Complete;
					}
					ZeroMemory(&_overlapped, sizeof(_overlapped));
					if (ConnectEx(_interface->Socket(), reinterpret_cast<sockaddr *>(_sa.GetBuffer()), _sa.Length(), 0, 0, 0, &_overlapped)) {
						setsockopt(_interface->Socket(), SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0);
						if (_error) ClearXEError(*_error);
						if (_handler) _handler->DoTask(0);
						return RequestStatus::Complete;
					} else {
						auto errid = WSAGetLastError();
						if (errid == WSA_IO_PENDING) {
							return RequestStatus::Await;
						} else {
							if (_error) SetWSAError(errid, *_error);
							if (_handler) _handler->DoTask(0);
							return RequestStatus::Complete;
						}
					}
				}
				virtual RequestStatus CompleteRequest(void) noexcept override
				{
					DWORD transferred, flags;
					if (WSAGetOverlappedResult(_interface->Socket(), &_overlapped, &transferred, FALSE, &flags)) {
						setsockopt(_interface->Socket(), SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, 0, 0);
						if (_error) ClearXEError(*_error);
						if (_handler) _handler->DoTask(0);
						return RequestStatus::Complete;
					} else {
						auto errid = WSAGetLastError();
						if (errid == WSA_IO_INCOMPLETE) {
							return RequestStatus::Await;
						} else {
							if (_error) SetWSAError(errid, *_error);
							if (_handler) _handler->DoTask(0);
							return RequestStatus::Complete;
						}
					}
				}
				virtual void CancelRequest(void) noexcept override
				{
					if (_error) SetXEError(*_error, 0x8, 0x4);
					if (_handler) _handler->DoTask(0);
				}
				virtual HANDLE GetHandle(void) noexcept override { return _interface->GetHandle(); }
				virtual RequestServiceClass GetServiceClass(void) noexcept override { return RequestServiceClass::Exclusive; }
			};
			class SendRequest : public INetworkRequest
			{
				SafePointer<InterfaceHolder> _interface;
				int _pointer;
				SafePointer<DataBlock> _buffer;
				ErrorContext * _error;
				int * _sent;
				SafePointer<IDispatchTask> _handler;
				WSABUF _buffer_desc;
				OVERLAPPED _overlapped;
			public:
				SendRequest(InterfaceHolder * interface, DataBlock * data, ErrorContext * error, int * sent, IDispatchTask * hdlr) : _pointer(0)
				{
					_interface.SetRetain(interface);
					_buffer.SetRetain(data);
					_error = error;
					_sent = sent;
					_handler.SetRetain(hdlr);
				}
				virtual ~SendRequest(void) override {}
				virtual RequestStatus InitiateRequest(void) noexcept override
				{
				InitiateSendRequestRetry:
					ZeroMemory(&_overlapped, sizeof(_overlapped));
					_buffer_desc.buf = reinterpret_cast<char *>(_buffer->GetBuffer()) + _pointer;
					_buffer_desc.len = _buffer->Length() - _pointer;
					DWORD num_sent = 0;
					auto status = WSASend(_interface->Socket(), &_buffer_desc, 1, &num_sent, 0, &_overlapped, 0);
					if (status == SOCKET_ERROR) {
						auto errid = WSAGetLastError();
						if (errid == WSA_IO_PENDING) {
							return RequestStatus::Await;
						} else {
							if (_error) SetWSAError(errid, *_error);
							if (_sent) *_sent = num_sent;
							if (_handler) _handler->DoTask(0);
							return RequestStatus::Complete;
						}
					} else {
						_pointer += num_sent;
						if (!num_sent || _pointer == _buffer->Length()) {
							if (_error) ClearXEError(*_error);
							if (_sent) *_sent = _pointer;
							if (_handler) _handler->DoTask(0);
							return RequestStatus::Complete;
						} else goto InitiateSendRequestRetry;
					}
				}
				virtual RequestStatus CompleteRequest(void) noexcept override
				{
					DWORD transferred, flags;
					if (WSAGetOverlappedResult(_interface->Socket(), &_overlapped, &transferred, FALSE, &flags)) {
						_pointer += transferred;
						if (!transferred || _pointer == _buffer->Length()) {
							if (_error) ClearXEError(*_error);
							if (_sent) *_sent = _pointer;
							if (_handler) _handler->DoTask(0);
							return RequestStatus::Complete;
						} else return InitiateRequest();
					} else {
						auto errid = WSAGetLastError();
						if (errid == WSA_IO_INCOMPLETE) {
							return RequestStatus::Await;
						} else {
							if (_error) SetWSAError(errid, *_error);
							if (_sent) *_sent = transferred;
							if (_handler) _handler->DoTask(0);
							return RequestStatus::Complete;
						}
					}
				}
				virtual void CancelRequest(void) noexcept override
				{
					if (_error) SetXEError(*_error, 0x8, 0x4);
					if (_sent) *_sent = 0;
					if (_handler) _handler->DoTask(0);
				}
				virtual HANDLE GetHandle(void) noexcept override { return _interface->GetHandle(); }
				virtual RequestServiceClass GetServiceClass(void) noexcept override { return RequestServiceClass::Write; }
			};
			class ReceiveRequest : public INetworkRequest
			{
				SafePointer<InterfaceHolder> _interface;
				int _pointer;
				SafePointer<DataBlock> _buffer;
				ErrorContext * _error;
				SafePointer<DataBlock> * _data;
				SafePointer<IDispatchTask> _handler;
				WSABUF _buffer_desc;
				OVERLAPPED _overlapped;
			public:
				ReceiveRequest(InterfaceHolder * interface, int length, ErrorContext * error, SafePointer<DataBlock> * data, IDispatchTask * hdlr) : _pointer(0)
				{
					_interface.SetRetain(interface);
					_buffer = new DataBlock(1);
					_buffer->SetLength(length);
					_error = error;
					_data = data;
					_handler.SetRetain(hdlr);
				}
				virtual ~ReceiveRequest(void) override {}
				virtual RequestStatus InitiateRequest(void) noexcept override
				{
				InitiateReceiveRequestRetry:
					ZeroMemory(&_overlapped, sizeof(_overlapped));
					_buffer_desc.buf = reinterpret_cast<char *>(_buffer->GetBuffer()) + _pointer;
					_buffer_desc.len = _buffer->Length() - _pointer;
					DWORD num_received = 0, flags = 0;
					auto status = WSARecv(_interface->Socket(), &_buffer_desc, 1, &num_received, &flags, &_overlapped, 0);
					if (status == SOCKET_ERROR) {
						auto errid = WSAGetLastError();
						if (errid == WSA_IO_PENDING) {
							return RequestStatus::Await;
						} else {
							if (_error) SetWSAError(errid, *_error);
							if (_data) *_data = 0;
							if (_handler) _handler->DoTask(0);
							return RequestStatus::Complete;
						}
					} else {
						_pointer += num_received;
						if (!num_received || _pointer == _buffer->Length()) {
							_buffer->SetLength(_pointer);
							if (_error) ClearXEError(*_error);
							if (_data) *_data = _buffer;
							if (_handler) _handler->DoTask(0);
							return RequestStatus::Complete;
						} else goto InitiateReceiveRequestRetry;
					}
				}
				virtual RequestStatus CompleteRequest(void) noexcept override
				{
					DWORD transferred, flags;
					if (WSAGetOverlappedResult(_interface->Socket(), &_overlapped, &transferred, FALSE, &flags)) {
						_pointer += transferred;
						if (!transferred || _pointer == _buffer->Length()) {
							_buffer->SetLength(_pointer);
							if (_error) ClearXEError(*_error);
							if (_data) *_data = _buffer;
							if (_handler) _handler->DoTask(0);
							return RequestStatus::Complete;
						} else return InitiateRequest();
					} else {
						auto errid = WSAGetLastError();
						if (errid == WSA_IO_INCOMPLETE) {
							return RequestStatus::Await;
						} else {
							if (_error) SetWSAError(errid, *_error);
							if (_data) *_data = 0;
							if (_handler) _handler->DoTask(0);
							return RequestStatus::Complete;
						}
					}
				}
				virtual void CancelRequest(void) noexcept override
				{
					if (_error) SetXEError(*_error, 0x8, 0x4);
					if (_data) *_data = 0;
					if (_handler) _handler->DoTask(0);
				}
				virtual HANDLE GetHandle(void) noexcept override { return _interface->GetHandle(); }
				virtual RequestServiceClass GetServiceClass(void) noexcept override { return RequestServiceClass::Read; }
			};
		private:
			SafePointer<InterfaceHolder> _interface;
		public:
			WSANetworkChannel(void) { _interface = new InterfaceHolder; }
			WSANetworkChannel(SOCKET sock) { _interface = new InterfaceHolder; _interface->Socket() = sock; }
			virtual ~WSANetworkChannel(void) override { _interface->Finalize(); }
			virtual string ToString(void) const override { return L"communicatio.canale"; }
			virtual void ConnectA(NetworkAddress * address, ErrorContext * error, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (_interface->Socket() == INVALID_SOCKET) {
					_interface->Socket() = CreateSocket(address, ectx);
					if (ectx.error_code) return;
					NetworkEventDispatch::AssociateHandle(reinterpret_cast<HANDLE>(_interface->Socket()), ectx);
					if (ectx.error_code) { closesocket(_interface->Socket()); _interface->Socket() = INVALID_SOCKET; return; }
				} else { SetXEError(ectx, 5, 0); return; }
				XE_TRY_INTRO
				SafePointer<ConnectRequest> request = new ConnectRequest(_interface, address, error, hdlr);
				NetworkEventDispatch::ScheduleRequest(request, ectx);
				XE_TRY_OUTRO()
			}
			virtual void ConnectB(NetworkAddress * address, NetworkSecurityDescriptor & sec, ErrorContext * error, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override { SetXEError(ectx, 1, 0); }
			virtual void Send(DataBlock * data, ErrorContext * error, int * sent, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (_interface->Socket() == INVALID_SOCKET) { SetXEError(ectx, 5, 0); return; }
				if (!data || !data->Length()) {
					if (error) ClearXEError(*error);
					if (sent) *sent = 0;
					if (hdlr) hdlr->DoTask(0);
					return;
				}
				XE_TRY_INTRO
				SafePointer<SendRequest> request = new SendRequest(_interface, data, error, sent, hdlr);
				NetworkEventDispatch::ScheduleRequest(request, ectx);
				XE_TRY_OUTRO()
			}
			virtual void Receive(int length, ErrorContext * error, SafePointer<DataBlock> * data, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (_interface->Socket() == INVALID_SOCKET) { SetXEError(ectx, 5, 0); return; }
				if (length < 0) { SetXEError(ectx, 3, 0); return; }
				XE_TRY_INTRO
				if (!length) {
					SafePointer<DataBlock> responce = new DataBlock(1);
					if (error) ClearXEError(*error);
					if (data) *data = responce;
					if (hdlr) hdlr->DoTask(0);
					return;
				}
				SafePointer<ReceiveRequest> request = new ReceiveRequest(_interface, length, error, data, hdlr);
				NetworkEventDispatch::ScheduleRequest(request, ectx);
				XE_TRY_OUTRO()
			}
			virtual void Close(bool ultimately, ErrorContext & ectx) noexcept override { if (ultimately) shutdown(_interface->Socket(), SD_BOTH); else shutdown(_interface->Socket(), SD_SEND); }
		};
		class WSANetworkListener : public INetworkListener
		{
			class InterfaceHolder : public INetworkRequest
			{
				SOCKET _sock;
			public:
				InterfaceHolder(void) : _sock(INVALID_SOCKET) {}
				virtual ~InterfaceHolder(void) override { if (_sock != INVALID_SOCKET) closesocket(_sock); }
				virtual RequestStatus InitiateRequest(void) noexcept override
				{
					if (_sock != INVALID_SOCKET) CancelIo(reinterpret_cast<HANDLE>(_sock));
					return RequestStatus::Complete;
				}
				virtual RequestStatus CompleteRequest(void) noexcept override { return InitiateRequest(); }
				virtual void CancelRequest(void) noexcept override { InitiateRequest(); }
				virtual HANDLE GetHandle(void) noexcept override { return reinterpret_cast<HANDLE>(_sock); }
				virtual RequestServiceClass GetServiceClass(void) noexcept override { return RequestServiceClass::Instant; }
				SOCKET & Socket(void) noexcept { return _sock; }
				void Finalize(void) noexcept
				{
					if (_sock != INVALID_SOCKET) {
						ErrorContext ectx;
						ClearXEError(ectx);
						NetworkEventDispatch::ScheduleRequest(this, ectx);
						if (ectx.error_code) { closesocket(_sock); _sock = INVALID_SOCKET; }
					}
				}
			};
			class AcceptRequest : public INetworkRequest
			{
				int _protocol, _num_uses;
				SafePointer<InterfaceHolder> _interface;
				SOCKET _new_sock;
				NetworkAddressFactory & _factory;
				ErrorContext * _error;
				SafePointer<INetworkChannel> * _channel;
				SafePointer<NetworkAddress> * _address;
				SafePointer<IDispatchTask> _handler;
				uint8 _accept_buffer[0x100];
				OVERLAPPED _overlapped;
			private:
				LPFN_ACCEPTEX AcceptEx;
				LPFN_GETACCEPTEXSOCKADDRS GetAcceptExSockaddrs;
			private:
				void _allocate_objects(sockaddr * addr, INetworkChannel ** channel, NetworkAddress ** address, ErrorContext & ectx) noexcept
				{
					NetworkEventDispatch::AssociateHandle(reinterpret_cast<HANDLE>(_new_sock), ectx);
					setsockopt(_new_sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, reinterpret_cast<char *>(&_interface->Socket()), sizeof(_interface->Socket()));
					if (ectx.error_code) {
						closesocket(_new_sock);
						_new_sock = INVALID_SOCKET;
						return;
					}
					try { *channel = new WSANetworkChannel(_new_sock); } catch (...) {
						SetXEError(ectx, 2, 0);
						closesocket(_new_sock);
						_new_sock = INVALID_SOCKET;
						return;
					}
					_new_sock = INVALID_SOCKET;
					XE_TRY_INTRO
					SocketAddressRead(addr, &_factory, address);
					XE_TRY_OUTRO()
				}
				bool _accept_post_process(DWORD num_received) noexcept
				{
					sockaddr * local, * remote;
					int local_length, remote_length;
					GetAcceptExSockaddrs(&_accept_buffer, 0, 0x80, 0x80, &local, &local_length, &remote, &remote_length);
					ErrorContext ectx;
					SafePointer<INetworkChannel> channel;
					SafePointer<NetworkAddress> address;
					ClearXEError(ectx);
					_allocate_objects(remote, channel.InnerRef(), address.InnerRef(), ectx);
					if (_error) *_error = ectx;
					if (ectx.error_code) {
						if (_channel) *_channel = 0;
						if (_address) *_address = 0;
					} else {
						if (_channel) *_channel = channel;
						if (_address) *_address = address;
					}
					if (_handler) _handler->DoTask(0);
					if (_num_uses > 0) _num_uses--;
					if (_num_uses) {
						if (_channel) *_channel = 0;
						if (_address) *_address = 0;
					}
					return _num_uses;
				}
			public:
				AcceptRequest(
					int protocol, InterfaceHolder * interface, NetworkAddressFactory & factory,
					int limit, ErrorContext * error, SafePointer<INetworkChannel> * channel,
					SafePointer<NetworkAddress> * address, IDispatchTask * hdlr) :
						_protocol(protocol), _num_uses(limit), _new_sock(INVALID_SOCKET), _factory(factory),
						_error(error), _channel(channel), _address(address)
				{
					_interface.SetRetain(interface);
					_handler.SetRetain(hdlr);
					GUID GUID_AcceptEx = WSAID_ACCEPTEX;
					DWORD Size_AcceptEx;
					auto status = WSAIoctl(_interface->Socket(), SIO_GET_EXTENSION_FUNCTION_POINTER, &GUID_AcceptEx, sizeof(GUID_AcceptEx), &AcceptEx, sizeof(AcceptEx), &Size_AcceptEx, 0, 0);
					if (status == SOCKET_ERROR) throw InvalidStateException();
					GUID_AcceptEx = WSAID_GETACCEPTEXSOCKADDRS;
					status = WSAIoctl(_interface->Socket(), SIO_GET_EXTENSION_FUNCTION_POINTER, &GUID_AcceptEx, sizeof(GUID_AcceptEx), &GetAcceptExSockaddrs, sizeof(GetAcceptExSockaddrs), &Size_AcceptEx, 0, 0);
					if (status == SOCKET_ERROR) throw InvalidStateException();
				}
				virtual ~AcceptRequest(void) override { if (_new_sock != INVALID_SOCKET) closesocket(_new_sock); }
				virtual RequestStatus InitiateRequest(void) noexcept override
				{
				InitiateAcceptRequestRetry:
					if (_new_sock == INVALID_SOCKET) {
						ErrorContext ectx;
						ClearXEError(ectx);
						_new_sock = CreateSocket(_protocol, ectx);
						if (ectx.error_code) {
							if (_error) *_error = ectx;
							if (_channel) *_channel = 0;
							if (_address) *_address = 0;
							if (_handler) _handler->DoTask(0);
							return RequestStatus::Complete;
						}
					}
					ZeroMemory(&_overlapped, sizeof(_overlapped));
					ZeroMemory(&_accept_buffer, sizeof(_accept_buffer));
					DWORD num_received = 0;
					if (AcceptEx(_interface->Socket(), _new_sock, &_accept_buffer, 0, 0x80, 0x80, &num_received, &_overlapped)) {
						if (_accept_post_process(num_received)) goto InitiateAcceptRequestRetry;
						return RequestStatus::Complete;
					} else {
						int errid = WSAGetLastError();
						if (errid == WSA_IO_PENDING) {
							return RequestStatus::Await;
						} else {
							closesocket(_new_sock);
							_new_sock = INVALID_SOCKET;
							if (_error) SetWSAError(errid, *_error);
							if (_channel) *_channel = 0;
							if (_address) *_address = 0;
							if (_handler) _handler->DoTask(0);
							return RequestStatus::Complete;
						}
					}
				}
				virtual RequestStatus CompleteRequest(void) noexcept override
				{
					DWORD transferred, flags;
					if (WSAGetOverlappedResult(_interface->Socket(), &_overlapped, &transferred, FALSE, &flags)) {
						if (_accept_post_process(transferred)) return InitiateRequest();
						return RequestStatus::Complete;
					} else {
						auto errid = WSAGetLastError();
						if (errid == WSA_IO_INCOMPLETE) {
							return RequestStatus::Await;
						} else {
							closesocket(_new_sock);
							_new_sock = INVALID_SOCKET;
							if (_error) SetWSAError(errid, *_error);
							if (_channel) *_channel = 0;
							if (_address) *_address = 0;
							if (_handler) _handler->DoTask(0);
							return RequestStatus::Complete;
						}
					}
				}
				virtual void CancelRequest(void) noexcept override
				{
					if (_error) SetXEError(*_error, 0x8, 0x4);
					if (_handler) _handler->DoTask(0);
				}
				virtual HANDLE GetHandle(void) noexcept override { return _interface->GetHandle(); }
				virtual RequestServiceClass GetServiceClass(void) noexcept override { return RequestServiceClass::Read; }
			};
		private:
			int _protocol;
			SafePointer<InterfaceHolder> _interface;
			NetworkAddressFactory & _factory;
		public:
			WSANetworkListener(NetworkAddressFactory & factory) : _factory(factory) { _interface = new InterfaceHolder; }
			virtual ~WSANetworkListener(void) override { _interface->Finalize(); }
			virtual string ToString(void) const override { return L"communicatio.attentor"; }
			virtual void BindA(NetworkAddress * address, ErrorContext & ectx) noexcept override
			{
				if (_interface->Socket() == INVALID_SOCKET) {
					_interface->Socket() = CreateSocket(address, ectx, &_protocol);
					if (ectx.error_code) return;
					NetworkEventDispatch::AssociateHandle(reinterpret_cast<HANDLE>(_interface->Socket()), ectx);
					if (ectx.error_code) { closesocket(_interface->Socket()); _interface->Socket() = INVALID_SOCKET; return; }
				} else { SetXEError(ectx, 5, 0); return; }
				XE_TRY_INTRO
				DataBlock sa(1);
				SocketAddressInit(sa, address);
				auto status = bind(_interface->Socket(), reinterpret_cast<sockaddr *>(sa.GetBuffer()), sa.Length());
				if (status == SOCKET_ERROR) { SetWSAError(ectx); return; }
				status = listen(_interface->Socket(), SOMAXCONN);
				if (status == SOCKET_ERROR) { SetWSAError(ectx); return; }
				XE_TRY_OUTRO()
			}
			virtual void BindB(NetworkAddress * address, NetworkIdentityDescriptor & idesc, ErrorContext & ectx) noexcept override { SetXEError(ectx, 1, 0); }
			virtual void Accept(int limit, ErrorContext * error, SafePointer<INetworkChannel> * channel, SafePointer<NetworkAddress> * address, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (_interface->Socket() == INVALID_SOCKET) { SetXEError(ectx, 5, 0); return; }
				if (!limit) return;
				XE_TRY_INTRO
				SafePointer<AcceptRequest> request = new AcceptRequest(_protocol, _interface, _factory, limit > 0 ? limit : -1, error, channel, address, hdlr);
				NetworkEventDispatch::ScheduleRequest(request, ectx);
				XE_TRY_OUTRO()
			}
			virtual void Close(ErrorContext & ectx) noexcept override { shutdown(_interface->Socket(), SD_BOTH); }
		};

		void NetworkEngineInit(void) { NetworkEventDispatch::Initialize(); }
		void NetworkEngineStop(void) noexcept { NetworkEventDispatch::Shutdown(); }
		SafePointer<INetworkChannel> CreateNetworkChannel(NetworkAddress * address)
		{
			if (address && reinterpret_cast<ClassSymbol *>(address->GetType())->GetClassName() == L"communicatio.adloquium_localis") {
				return new LocalNetworkChannel;
			} else {
				return new WSANetworkChannel;
			}
		}
		SafePointer<INetworkListener> CreateNetworkListener(NetworkAddress * address, NetworkAddressFactory & factory)
		{
			if (address && reinterpret_cast<ClassSymbol *>(address->GetType())->GetClassName() == L"communicatio.adloquium_localis") {
				return new LocalNetworkListener(factory);
			} else {
				return new WSANetworkListener(factory);
			}
		}
		SafePointer< ObjectArray<NetworkAddress> > GetNetworkAddresses(NetworkAddressFactory & factory, const string & domain, uint16 port, const ClassSymbol * nns, ErrorContext & ectx) noexcept
		{
			XE_TRY_INTRO
			if (!nns) throw InvalidArgumentException();
			addrinfo * info = 0, * current = 0, req;
			ZeroMemory(&req, sizeof(req));
			req.ai_flags = AI_CANONNAME;
			req.ai_protocol = IPPROTO_TCP;
			if (nns->GetClassName() == L"communicatio.adloquium_ipv4") {
				req.ai_family = AF_INET;
			} else if (nns->GetClassName() == L"communicatio.adloquium_ipv6") {
				req.ai_family = PF_INET6;
				req.ai_flags |= AI_ALL | AI_V4MAPPED;
			} else throw Exception();
			SafePointer<DataBlock> ascii_domain = Network::DomainNameToPunycode(domain).EncodeSequence(Encoding::ANSI, true);
			SafePointer<DataBlock> ascii_port = string(port).EncodeSequence(Encoding::ANSI, true);
			auto error = getaddrinfo(reinterpret_cast<char *>(ascii_domain->GetBuffer()), reinterpret_cast<char *>(ascii_port->GetBuffer()), &req, &info);
			if (error) { SetWSAError(error, ectx); return 0; }
			SafePointer< ObjectArray<NetworkAddress> > result;
			try {
				result = new ObjectArray<NetworkAddress>(0x10);
				current = info;
				while (current) {
					if (current->ai_family == AF_INET) {
						auto addr = factory.CreateIPv4(ectx);
						if (ectx.error_code) { freeaddrinfo(info); return 0; }
						sockaddr_in * ipv4 = reinterpret_cast<sockaddr_in *>(current->ai_addr);
						MemoryCopy(&addr->IP(), &ipv4->sin_addr, 4);
						addr->Port() = Network::InverseEndianess(ipv4->sin_port);
						result->Append(addr);
					} else if (current->ai_family == AF_INET6) {
						auto addr = factory.CreateIPv6(ectx);
						if (ectx.error_code) { freeaddrinfo(info); return 0; }
						sockaddr_in6 * ipv6 = reinterpret_cast<sockaddr_in6 *>(current->ai_addr);
						for (int i = 0; i < 16; i += 2) swap(ipv6->sin6_addr.s6_addr[i], ipv6->sin6_addr.s6_addr[i + 1]);
						MemoryCopy(&addr->IP(), &ipv6->sin6_addr, 16);
						addr->Port() = Network::InverseEndianess(ipv6->sin6_port);
						result->Append(addr);
					}
					current = current->ai_next;
				}
			} catch (...) {
				freeaddrinfo(info);
				throw;
			}
			freeaddrinfo(info);
			return result;
			XE_TRY_OUTRO(0)
		}
	}
}
#endif