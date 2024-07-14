#include "xe_netapi_unix.h"

#ifdef ENGINE_UNIX

#include <Network/Punycode.h>

#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <netinet/ip.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <atomic>

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
		void SetPosixError(int error, ErrorContext & ectx) noexcept
		{
			if (error == 0) SetXEError(ectx, 0x08, 0x00);
			else if (error == EADDRINUSE) SetXEError(ectx, 0x08, 0x02);
			else if (error == EADDRNOTAVAIL) SetXEError(ectx, 0x08, 0x03);
			else if (error == ECONNREFUSED) SetXEError(ectx, 0x08, 0x07);
			else if (error == EHOSTUNREACH) SetXEError(ectx, 0x08, 0x05);
			else if (error == ENETDOWN) SetXEError(ectx, 0x08, 0x04);
			else if (error == ENETUNREACH) SetXEError(ectx, 0x08, 0x06);
			else if (error == EPROTOTYPE) SetXEError(ectx, 0x08, 0x09);
			else if (error == ETIMEDOUT) SetXEError(ectx, 0x08, 0x0A);
			else if (error == ECONNRESET) SetXEError(ectx, 0x08, 0x08);
			else if (error == EACCES) SetXEError(ectx, 0x06, 5);
			else if (error == EDQUOT) SetXEError(ectx, 0x06, 10);
			else if (error == EEXIST) SetXEError(ectx, 0x06, 11);
			else if (error == EISDIR) SetXEError(ectx, 0x06, 11);
			else if (error == EMFILE) SetXEError(ectx, 0x06, 4);
			else if (error == ENAMETOOLONG) SetXEError(ectx, 0x06, 17);
			else if (error == ENFILE) SetXEError(ectx, 0x06, 10);
			else if (error == ENOENT) SetXEError(ectx, 0x06, 2);
			else if (error == ENOSPC) SetXEError(ectx, 0x06, 10);
			else if (error == ENOTDIR) SetXEError(ectx, 0x06, 3);
			else if (error == EOPNOTSUPP) SetXEError(ectx, 0x06, 12);
			else if (error == EROFS) SetXEError(ectx, 0x06, 9);
			else if (error == ETXTBSY) SetXEError(ectx, 0x06, 5);
			else if (error == EILSEQ) SetXEError(ectx, 0x06, 16);
			else if (error == EBADF) SetXEError(ectx, 0x06, 6);
			else if (error == EINVAL) SetXEError(ectx, 0x06, 12);
			else if (error == ENOTEMPTY) SetXEError(ectx, 0x06, 13);
			else if (error == ENOTDIR) SetXEError(ectx, 0x06, 3);
			else if (error == ENOTSUP) SetXEError(ectx, 0x06, 12);
			else if (error == EPERM) SetXEError(ectx, 0x06, 5);
			else if (error == EXDEV) SetXEError(ectx, 0x06, 15);
			else if (error == ENOBUFS) SetXEError(ectx, 0x06, 7);
			else if (error == ENOMEM) SetXEError(ectx, 0x06, 7);
			else if (error == ENXIO) SetXEError(ectx, 0x06, 8);
			else if (error == EFBIG) SetXEError(ectx, 0x06, 18);
			else if (error == EBUSY) SetXEError(ectx, 0x06, 5);
			else SetXEError(ectx, 0x08, 0x01);
		}
		void SetPosixError(ErrorContext & ectx) noexcept { SetPosixError(errno, ectx); }
		void SetDNSError(int error, ErrorContext & ectx) noexcept
		{
			if (error) {
				if (error == EAI_ADDRFAMILY || error == EAI_FAMILY || error == EAI_NODATA || error == EAI_NONAME || error == EAI_SERVICE || error == EAI_SOCKTYPE) SetXEError(ectx, 0x08, 0x0C);
				else if (error == EAI_AGAIN || error == EAI_PROTOCOL || error == EAI_FAIL) SetXEError(ectx, 5, 0);
				else if (error == EAI_BADFLAGS || error == EAI_BADHINTS) SetXEError(ectx, 3, 0);
				else if (error == EAI_MEMORY || error == EAI_OVERFLOW) SetXEError(ectx, 2, 0);
				else if (error == EAI_SYSTEM) SetPosixError(ectx);
				else SetXEError(ectx, 0x08, 0x01);
			} else SetXEError(ectx, 0, 0);
		}

		void SocketAddressInit(DataBlock & dest, NetworkAddress * address, string * ulnk)
		{
			if (address) {
				if (reinterpret_cast<ClassSymbol *>(address->GetType())->GetClassName() == L"communicatio.adloquium_localis") {
					auto & addr = *reinterpret_cast<NetworkAddressLocal *>(address);
					dest.SetLength(sizeof(sockaddr_un));
					auto & sa = *reinterpret_cast<sockaddr_un *>(dest.GetBuffer());
					sa.sun_len = sizeof(sockaddr_un);
					sa.sun_family = PF_LOCAL;
					ZeroMemory(&sa.sun_path, sizeof(sa.sun_path));
					string path;
					SafePointer<DataBlock> ascii;
					if (addr.Name[0] == L'/') path = addr.Name; else path = L"/tmp/" + addr.Name;
					ascii = path.EncodeSequence(Encoding::ANSI, true);
					if (ulnk) *ulnk = path;
					MemoryCopy(&sa.sun_path, ascii->GetBuffer(), min(ascii->Length(), int(sizeof(sa.sun_path))));
				} else if (reinterpret_cast<ClassSymbol *>(address->GetType())->GetClassName() == L"communicatio.adloquium_ipv4") {
					auto & addr = *reinterpret_cast<NetworkAddressIPv4 *>(address);
					dest.SetLength(sizeof(sockaddr_in));
					auto & sa = *reinterpret_cast<sockaddr_in *>(dest.GetBuffer());
					sa.sin_len = sizeof(sockaddr_in);
					sa.sin_family = AF_INET;
					sa.sin_port = Network::InverseEndianess(addr.Port);
					MemoryCopy(&sa.sin_addr, &addr.IP, 4);
					ZeroMemory(&sa.sin_zero, sizeof(sa.sin_zero));
				} else if (reinterpret_cast<ClassSymbol *>(address->GetType())->GetClassName() == L"communicatio.adloquium_ipv6") {
					auto & addr = *reinterpret_cast<NetworkAddressIPv6 *>(address);
					dest.SetLength(sizeof(sockaddr_in6));
					auto & sa = *reinterpret_cast<sockaddr_in6 *>(dest.GetBuffer());
					sa.sin6_len = sizeof(sockaddr_in6);
					sa.sin6_family = AF_INET6;
					sa.sin6_port = Network::InverseEndianess(addr.Port);
					sa.sin6_flowinfo = 0;
					MemoryCopy(&sa.sin6_addr, &addr.IP, 16);
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
			if (sa.sa_family == PF_LOCAL) {
				auto address = factory->CreateLocal(ectx);
				if (ectx.error_code) throw OutOfMemoryException();
				auto & sa_local = *reinterpret_cast<const sockaddr_un *>(src);
				address->Name = string(&sa_local.sun_path, sizeof(sa_local.sun_path), Encoding::UTF8);
				address->Retain();
				*dest = address;
			} else if (sa.sa_family == PF_INET) {
				auto address = factory->CreateIPv4(ectx);
				if (ectx.error_code) throw OutOfMemoryException();
				auto & sa_ipv4 = *reinterpret_cast<const sockaddr_in *>(src);
				MemoryCopy(&address->IP, &sa_ipv4.sin_addr, 4);
				address->Port = Network::InverseEndianess(sa_ipv4.sin_port);
				address->Retain();
				*dest = address;
			} else if (sa.sa_family == PF_INET6) {
				auto address = factory->CreateIPv6(ectx);
				if (ectx.error_code) throw OutOfMemoryException();
				auto & sa_ipv6 = *reinterpret_cast<sockaddr_in6 *>(src);
				for (int i = 0; i < 16; i += 2) swap(sa_ipv6.sin6_addr.s6_addr[i], sa_ipv6.sin6_addr.s6_addr[i + 1]);
				MemoryCopy(&address->IP, &sa_ipv6.sin6_addr, 16);
				address->Port = Network::InverseEndianess(sa_ipv6.sin6_port);
				address->Retain();
				*dest = address;
			} else throw InvalidStateException();
		}

		struct NetworkRequestInput
		{
			int request; // 0x1 - connect, 0x9 - read, 0x2 - send, 0xA - accept
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
		class NetworkRequestQueue : public Object
		{
			static std::atomic_flag _dispatch_control;
			static SafePointer<Thread> _dispatch_thread;
			static int _command_in;
			static int _command_out;
			static uint _service_refcnt;
		private:
			int _socket, _address_size;
			SafePointer<DataBlock> _unlink_on_close;
			SafePointer<Semaphore> _local_sync;
			Volumes::Queue<NetworkRequest> _requests;
			NetworkAddressFactory * _factory;
		private:
			static void _allocate_connection(NewNetworkConnection & con, ErrorContext & ectx) noexcept;
			static int _dispatch(void * arg)
			{
				try {
					Volumes::Dictionary<SafePointer<NetworkRequestQueue>, uint> clients; // client : operation class (1 - read, 0 - write)
					Array<pollfd> poll_data(0x100);
					int poll_volume = 1;
					while (true) {
						if (poll_volume > poll_data.Length()) poll_data.SetLength(poll_volume);
						poll_data[0].fd = _command_out;
						poll_data[0].events = POLLIN;
						poll_data[0].revents = 0;
						int i = 1;
						for (auto & c : clients) {
							poll_data[i].fd = c.key->_peek_handle();
							poll_data[i].events = poll_data[i].revents = 0;
							if (c.value) poll_data[i].events |= POLLIN;
							else poll_data[i].events |= POLLOUT;
							i++;
						}
						int status = poll(poll_data, poll_volume, -1);
						if (status > 0) {
							int remove_list = 0;
							i = 1;
							for (auto & c : clients) {
								if (poll_data[i].revents) {
									bool revise = false;
									SafePointer<IDispatchTask> hdlr;
									c.key->_local_sync->Wait();
									auto req_ptr = c.key->_requests.GetFirst();
									if (req_ptr) try {
										auto & req = req_ptr->GetValue();
										if (req.in.request == 0x1) {
											bool finish;
											ErrorContext error;
											status = connect(c.key->_socket, reinterpret_cast<sockaddr *>(req.in.data->GetBuffer()), req.in.data->Length());
											if (status == 0) {
												finish = true;
												ClearXEError(error);
											} else if (status == -1) {
												if (errno == EINTR || errno == EAGAIN || errno == EINPROGRESS) {
													finish = false;
												} else if (errno == EISCONN) {
													finish = true;
													ClearXEError(error);
												} else {
													finish = true;
													SetPosixError(error);
												}
											} else {
												finish = true;
												SetXEError(error, 5, 0);
											}
											if (finish) {
												if (req.out.status) *req.out.status = error;
												hdlr = req.out.handler;
												c.key->_requests.RemoveFirst();
												revise = true;
											}
										} else if (req.in.request == 0x2) {
											bool finish;
											ErrorContext error;
											if (poll_data[i].revents == POLL_HUP) {
												finish = true;
												SetXEError(error, 0x08, 0x08);
											} else {
												int rem = req.in.length - req.in.pointer;
												status = send(c.key->_socket, req.in.data->GetBuffer() + req.in.pointer, rem, 0);
												if (status >= 0) {
													req.in.pointer += status;
													if (req.in.pointer == req.in.length) {
														finish = true;
														ClearXEError(error);
													} else finish = false;
												} else {
													if (errno == EAGAIN || errno == EINTR) {
														finish = false;
													} else {
														finish = true;
														SetPosixError(error);
													}
												}
											}
											if (finish) {
												if (req.out.status) *req.out.status = error;
												if (req.out.length) *req.out.length = req.in.pointer;
												hdlr = req.out.handler;
												c.key->_requests.RemoveFirst();
												revise = true;
											}
										} else if (req.in.request == 0x9) {
											bool finish;
											ErrorContext error;
											int rem = req.in.length - req.in.pointer;
											status = recv(c.key->_socket, req.in.data->GetBuffer() + req.in.pointer, rem, 0);
											if (status > 0) {
												req.in.pointer += status;
												if (req.in.pointer == req.in.length) {
													finish = true;
													ClearXEError(error);
												} else finish = false;
											} else if (status == 0) {
												finish = true;
												ClearXEError(error);
											} else {
												if (errno == EAGAIN || errno == EINTR) {
													finish = false;
												} else {
													finish = true;
													SetPosixError(error);
												}
											}
											if (finish) {
												req.in.data->SetLength(req.in.pointer);
												if (req.out.status) *req.out.status = error;
												if (req.out.data) *req.out.data = req.in.data;
												hdlr = req.out.handler;
												c.key->_requests.RemoveFirst();
												revise = true;
											}
										} else if (req.in.request == 0xA) {
											bool process;
											int socket_accepted = -1;
											uint8 sa[0x100];
											ErrorContext error;
											socklen_t length = sizeof(sa);
											socket_accepted = accept(c.key->_socket, reinterpret_cast<sockaddr *>(&sa), &length);
											if (socket_accepted == -1) {
												if (errno == EINTR || errno == EWOULDBLOCK) {
													process = false;
												} else {
													process = true;
													SetPosixError(error);
												}
											} else {
												process = true;
												ClearXEError(error);
											}
											if (process) {
												NewNetworkConnection con;
												con.in_factory = c.key->_factory;
												con.in_socket = socket_accepted;
												con.in_address = &sa;
												con.in_address_length = length;
												if (socket_accepted >= 0) _allocate_connection(con, error);
												if (req.out.status) *req.out.status = error;
												if (req.out.channel) *req.out.channel = con.out_channel;
												if (req.out.address) *req.out.address = con.out_address;
												hdlr = req.out.handler;
												if (req.in.pointer > 0) {
													req.in.pointer--;
													if (!req.in.pointer) {
														c.key->_requests.RemoveFirst();
														revise = true;
													}
												}
											}
										} else throw InvalidStateException();
									} catch (...) {
										c.key->_local_sync->Open();
										break;
									}
									c.key->_local_sync->Open();
									if (hdlr) hdlr->DoTask(0);
									if (revise) {
										auto next = c.key->_peek_request();
										if (next) {
											if (next->in.request & 0x8) c.value = 1;
											else c.value = 0;
										} else {
											c.value = 0xFF;
											remove_list++;
										}
									}
								}
								i++;
							}
							if (remove_list) {
								auto current = clients.GetFirst();
								while (current) {
									auto next = current->GetNext();
									if (current->GetValue().value == 0xFF) { clients.BinaryTree::Remove(current); poll_volume--; }
									current = next;
								}
							}
							if (poll_data[0].revents) {
								uintptr command[2];
								int rd = 0;
								while (rd < sizeof(command)) {
									int nr = read(_command_out, reinterpret_cast<char *>(&command) + rd, sizeof(command) - rd);
									if (nr > 0) rd += nr;
									else if (nr == 0) { ZeroMemory(&command, sizeof(command)); break; }
									else {
										if (errno == EINTR || errno == EAGAIN) continue;
										else break;
									}
								}
								if (!command[1]) break;
								SafePointer<NetworkRequestQueue> queue = reinterpret_cast<NetworkRequestQueue *>(command[1]);
								if (command[0]) {
									bool created;
									auto request = queue->_peek_request();
									if (request) {
										auto element = clients.FindElement(Volumes::KeyValuePair<SafePointer<NetworkRequestQueue>, uint>(queue, 0), true, &created);
										if (created) poll_volume++;
										if (request->in.request & 0x8) element->GetValue().value = 1;
										else element->GetValue().value = 0;
									} else command[0] = 0;
								}
								if (!command[0]) {
									auto element = clients.FindElementEquivalent(queue);
									if (element) {
										clients.BinaryTree::Remove(element);
										poll_volume--;
									}
								}
							}
						} else if (status == -1) {
							if (errno == EINVAL) break;
						}
					}
				} catch (...) {}
				close(_command_out);
				return 0;
			}
			int _peek_handle(void) noexcept { return _socket; }
			NetworkRequest * _peek_request(void) noexcept
			{
				NetworkRequest * result = 0;
				_local_sync->Wait();
				auto first = _requests.GetFirst();
				if (first) result = &first->GetValue();
				_local_sync->Open();
				return result;
			}
			bool _control_send(uintptr cmd) noexcept
			{
				while (_dispatch_control.test_and_set(std::memory_order_acquire));
				if (_command_in < 0) {
					_dispatch_control.clear(std::memory_order_release);
					return false;
				}
				uintptr command[2];
				command[0] = cmd;
				command[1] = uintptr(this);
				int length = sizeof(command);
				int wp = 0;
				while (wp < length) {
					int wa = write(_command_in, &command, length - wp);
					if (wa == -1) {
						auto errid = errno;
						if (errid == EINTR) continue; else {
							_dispatch_control.clear(std::memory_order_release);
							return false;
						}
					} else wp += wa;
				}
				Retain();
				_dispatch_control.clear(std::memory_order_release);
				return true;
			}
			void _common_init(void) { _local_sync = CreateSemaphore(1); if (!_local_sync) throw OutOfMemoryException(); }
		public:
			NetworkRequestQueue(int sock) : _socket(sock), _address_size(0), _factory(0)
			{
				if (fcntl(_socket, F_SETFL, O_NONBLOCK) == -1) {
					ErrorContext ectx;
					SetPosixError(ectx);
					if (ectx.error_code == 1) throw Exception();
					else if (ectx.error_code == 2) throw OutOfMemoryException();
					else if (ectx.error_code == 3) throw InvalidArgumentException();
					else if (ectx.error_code == 4) throw InvalidFormatException();
					else if (ectx.error_code == 5) throw InvalidStateException();
					else if (ectx.error_code == 6) throw IO::FileAccessException(ectx.error_subcode);
					else throw IO::FileAccessException(IO::Error::Unknown);
				}
				_common_init();
			}
			NetworkRequestQueue(NetworkAddressFactory * factory, NetworkAddress * address) : _address_size(0), _factory(factory)
			{
				if (address) {
					if (reinterpret_cast<ClassSymbol *>(address->GetType())->GetClassName() == L"communicatio.adloquium_localis") {
						_socket = socket(PF_LOCAL, SOCK_STREAM, 0);
					} else if (reinterpret_cast<ClassSymbol *>(address->GetType())->GetClassName() == L"communicatio.adloquium_ipv4") {
						_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
					} else if (reinterpret_cast<ClassSymbol *>(address->GetType())->GetClassName() == L"communicatio.adloquium_ipv6") {
						_socket = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP);
					} else throw Exception();
				} else throw InvalidArgumentException();
				if (_socket == -1) {
					ErrorContext ectx;
					SetPosixError(ectx);
					if (ectx.error_code == 1) throw Exception();
					else if (ectx.error_code == 2) throw OutOfMemoryException();
					else if (ectx.error_code == 3) throw InvalidArgumentException();
					else if (ectx.error_code == 4) throw InvalidFormatException();
					else if (ectx.error_code == 5) throw InvalidStateException();
					else if (ectx.error_code == 6) throw IO::FileAccessException(ectx.error_subcode);
					else throw IO::FileAccessException(IO::Error::Unknown);
				}
				if (fcntl(_socket, F_SETFL, O_NONBLOCK) == -1) {
					ErrorContext ectx;
					SetPosixError(ectx);
					close(_socket);
					if (ectx.error_code == 1) throw Exception();
					else if (ectx.error_code == 2) throw OutOfMemoryException();
					else if (ectx.error_code == 3) throw InvalidArgumentException();
					else if (ectx.error_code == 4) throw InvalidFormatException();
					else if (ectx.error_code == 5) throw InvalidStateException();
					else if (ectx.error_code == 6) throw IO::FileAccessException(ectx.error_subcode);
					else throw IO::FileAccessException(IO::Error::Unknown);
				}
				_common_init();
			}
			virtual ~NetworkRequestQueue(void)
			{
				for (auto & r : _requests) {
					if (r.out.status) SetXEError(*r.out.status, 0x08, 0x08);
					if (r.out.handler) r.out.handler->DoTask(0);
				}
				close(_socket);
				if (_unlink_on_close) unlink(reinterpret_cast<char *>(_unlink_on_close->GetBuffer()));
			}
			friend bool operator > (const SafePointer<NetworkRequestQueue> & a, const SafePointer<NetworkRequestQueue> & b) { return a.Inner() > b.Inner(); }
			friend bool operator >= (const SafePointer<NetworkRequestQueue> & a, const SafePointer<NetworkRequestQueue> & b) { return a.Inner() >= b.Inner(); }
			friend bool operator < (const SafePointer<NetworkRequestQueue> & a, const SafePointer<NetworkRequestQueue> & b) { return a.Inner() < b.Inner(); }
			friend bool operator <= (const SafePointer<NetworkRequestQueue> & a, const SafePointer<NetworkRequestQueue> & b) { return a.Inner() <= b.Inner(); }
			void BindRequest(NetworkAddress * address, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				string ulnk;
				DataBlock sa(1);
				SocketAddressInit(sa, address, &ulnk);
				auto status = bind(_socket, reinterpret_cast<sockaddr *>(sa.GetBuffer()), sa.Length());
				if (status == -1) { SetPosixError(ectx); return; }
				status = listen(_socket, SOMAXCONN);
				if (status == -1) { SetPosixError(ectx); return; }
				_address_size = sa.Length();
				_unlink_on_close.SetReference(0);
				if (ulnk.Length()) try { _unlink_on_close = ulnk.EncodeSequence(Encoding::UTF8, true); } catch (...) {}
				XE_TRY_OUTRO()
			}
			void ShutdownRequest(bool bidirectionally, ErrorContext & ectx) noexcept
			{
				int status;
				if (bidirectionally) status = shutdown(_socket, SHUT_RDWR);
				else status = shutdown(_socket, SHUT_WR);
			}
			void EnqueueConnectRequest(NetworkAddress * address, ErrorContext * error, IDispatchTask * hdlr)
			{
				_local_sync->Wait();
				try {
					NetworkRequest req;
					req.in.request = 0x1;
					req.in.length = req.in.pointer = 0;
					req.in.data = new DataBlock(1);
					req.out.handler.SetRetain(hdlr);
					req.out.status = error;
					req.out.length = 0;
					req.out.data = 0;
					req.out.address = 0;
					req.out.channel = 0;
					SocketAddressInit(*req.in.data, address, 0);
					while (true) {
						if (connect(_socket, reinterpret_cast<sockaddr *>(req.in.data->GetBuffer()), req.in.data->Length()) == -1) {
							auto errid = errno;
							if (errid == EINPROGRESS) {
								_requests.Push(req);
								if (!_control_send(1)) {
									_requests.RemoveLast();
									_local_sync->Open();
									if (req.out.status) SetXEError(*req.out.status, 0x8, 0x4);
									if (req.out.handler) req.out.handler->DoTask(0);
									return;
								}
								break;
							} else if (errid == EINTR) {
								continue;
							} else {
								_local_sync->Open();
								if (req.out.status) SetPosixError(errid, *req.out.status);
								if (req.out.handler) req.out.handler->DoTask(0);
								return;
							}
						} else {
							_local_sync->Open();
							if (req.out.status) ClearXEError(*req.out.status);
							if (req.out.handler) req.out.handler->DoTask(0);
							return;
						}
					}
				} catch (...) {
					_local_sync->Open();
					throw;
				}
				_local_sync->Open();
			}
			void EnqueueRequest(const NetworkRequest & req)
			{
				_local_sync->Wait();
				try {
					_requests.Push(req);
					if (!_control_send(1)) {
						_requests.RemoveLast();
						_local_sync->Open();
						if (req.out.status) SetXEError(*req.out.status, 0x8, 0x4);
						if (req.out.handler) req.out.handler->DoTask(0);
						return;
					}
				} catch (...) {
					_local_sync->Open();
					throw;
				}
				_local_sync->Open();
			}
			void Shutdown(void) noexcept { _control_send(0); }
			static void StopDispatch(void) noexcept
			{
				if (InterlockedDecrement(_service_refcnt) == 0) {
					while (_dispatch_control.test_and_set(std::memory_order_acquire));
					close(_command_in);
					_command_in = -1;
					_dispatch_control.clear(std::memory_order_release);
					_dispatch_thread->Wait();
				}
			}
			static void CreateDispatch(void)
			{
				if (InterlockedIncrement(_service_refcnt) == 1) {
					int p[2];
					if (pipe(p) == -1) throw OutOfMemoryException();
					_command_in = p[1];
					_command_out = p[0];
					if (fcntl(_command_out, F_SETFL, O_NONBLOCK) == -1) { close(_command_in); close(_command_out); throw OutOfMemoryException(); }
					_dispatch_thread = CreateThread(_dispatch);
					if (!_dispatch_thread) { close(_command_in); close(_command_out); throw OutOfMemoryException(); }
				}
			}
		};
		class UnixNetworkChannel : public INetworkChannel
		{
			SafePointer<NetworkRequestQueue> _queue;
		public:
			UnixNetworkChannel(void) {}
			UnixNetworkChannel(NetworkRequestQueue * queue) { _queue.SetRetain(queue); }
			virtual ~UnixNetworkChannel(void) noexcept { if (_queue) _queue->Shutdown(); }
			virtual string ToString(void) const override { return L"communicatio.canale"; }
			virtual void ConnectA(NetworkAddress * address, ErrorContext * error, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (_queue) { SetXEError(ectx, 5, 0); return; }
				XE_TRY_INTRO
				_queue = new NetworkRequestQueue(0, address);
				_queue->EnqueueConnectRequest(address, error, hdlr);
				XE_TRY_OUTRO()
			}
			virtual void ConnectB(NetworkAddress * address, NetworkSecurityDescriptor & sec, ErrorContext * error, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override { SetXEError(ectx, 1, 0); }
			virtual void Send(DataBlock * data, ErrorContext * error, int * sent, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (!_queue) { SetXEError(ectx, 5, 0); return; }
				if (!data || !data->Length()) {
					if (error) ClearXEError(*error);
					if (sent) *sent = 0;
					if (hdlr) hdlr->DoTask(0);
					return;
				}
				XE_TRY_INTRO
				NetworkRequest req;
				req.in.request = 0x2;
				req.in.length = data->Length();
				req.in.pointer = 0;
				req.in.data.SetRetain(data);
				req.out.handler.SetRetain(hdlr);
				req.out.status = error;
				req.out.length = sent;
				req.out.data = 0;
				req.out.address = 0;
				req.out.channel = 0;
				_queue->EnqueueRequest(req);
				XE_TRY_OUTRO()
			}
			virtual void Receive(int length, ErrorContext * error, SafePointer<DataBlock> * data, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (!_queue) { SetXEError(ectx, 5, 0); return; }
				if (length < 0) { SetXEError(ectx, 3, 0); return; }
				XE_TRY_INTRO
				SafePointer<DataBlock> responce = new DataBlock(1);
				if (!length) {
					if (error) ClearXEError(*error);
					if (data) *data = responce;
					if (hdlr) hdlr->DoTask(0);
					return;
				}
				responce->SetLength(length);
				NetworkRequest req;
				req.in.request = 0x9;
				req.in.length = length;
				req.in.pointer = 0;
				req.in.data = responce;
				req.out.handler.SetRetain(hdlr);
				req.out.status = error;
				req.out.length = 0;
				req.out.data = data;
				req.out.address = 0;
				req.out.channel = 0;
				_queue->EnqueueRequest(req);
				XE_TRY_OUTRO()
			}
			virtual void Close(bool ultimately, ErrorContext & ectx) noexcept override
			{
				if (!_queue) { SetXEError(ectx, 5, 0); return; }
				_queue->ShutdownRequest(ultimately, ectx);
			}
		};
		class UnixNetworkListener : public INetworkListener
		{
			NetworkAddressFactory & _factory;
			SafePointer<NetworkRequestQueue> _queue;
		public:
			UnixNetworkListener(NetworkAddressFactory & factory) : _factory(factory) {}
			virtual ~UnixNetworkListener(void) noexcept { if (_queue) _queue->Shutdown(); }
			virtual string ToString(void) const override { return L"communicatio.attentor"; }
			virtual void BindA(NetworkAddress * address, ErrorContext & ectx) noexcept override
			{
				if (_queue) { SetXEError(ectx, 5, 0); return; }
				XE_TRY_INTRO
				_queue = new NetworkRequestQueue(&_factory, address);
				_queue->BindRequest(address, ectx);
				XE_TRY_OUTRO()
			}
			virtual void BindB(NetworkAddress * address, NetworkIdentityDescriptor & idesc, ErrorContext & ectx) noexcept override { SetXEError(ectx, 1, 0); }
			virtual void Accept(int limit, ErrorContext * error, SafePointer<INetworkChannel> * channel, SafePointer<NetworkAddress> * address, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (!_queue) { SetXEError(ectx, 5, 0); return; }
				if (!limit) return;
				XE_TRY_INTRO
				NetworkRequest req;
				req.in.request = 0xA;
				req.in.length = 0;
				req.in.pointer = limit > 0 ? limit : -1;
				req.out.handler.SetRetain(hdlr);
				req.out.status = error;
				req.out.length = 0;
				req.out.data = 0;
				req.out.address = address;
				req.out.channel = channel;
				_queue->EnqueueRequest(req);
				XE_TRY_OUTRO()
			}
			virtual void Close(ErrorContext & ectx) noexcept override
			{
				if (!_queue) { SetXEError(ectx, 5, 0); return; }
				_queue->ShutdownRequest(true, ectx);
			}
		};

		std::atomic_flag NetworkRequestQueue::_dispatch_control = ATOMIC_FLAG_INIT;
		SafePointer<Thread> NetworkRequestQueue::_dispatch_thread;
		int NetworkRequestQueue::_command_in = -1;
		int NetworkRequestQueue::_command_out = -1;
		uint NetworkRequestQueue::_service_refcnt = 0;

		void NetworkRequestQueue::_allocate_connection(NewNetworkConnection & con, ErrorContext & ectx) noexcept
		{
			XE_TRY_INTRO
			SafePointer<NetworkRequestQueue> queue;
			try { queue = new NetworkRequestQueue(con.in_socket); } catch (...) { close(con.in_socket); throw; }
			try {
				con.out_channel = new UnixNetworkChannel(queue);
				SocketAddressRead(con.in_address, con.in_factory, con.out_address.InnerRef());
			} catch (...) {
				con.out_channel.SetReference(0);
				con.out_address.SetReference(0);
				throw;
			}
			XE_TRY_OUTRO()
		}

		void NetworkEngineInit(void) { NetworkRequestQueue::CreateDispatch(); }
		void NetworkEngineStop(void) noexcept { NetworkRequestQueue::StopDispatch(); }
		SafePointer<INetworkChannel> CreateNetworkChannel(void) { return new UnixNetworkChannel; }
		SafePointer<INetworkListener> CreateNetworkListener(NetworkAddressFactory & factory) { return new UnixNetworkListener(factory); }
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
			if (error) { SetDNSError(error, ectx); return 0; }
			SafePointer< ObjectArray<NetworkAddress> > result;
			try {
				result = new ObjectArray<NetworkAddress>(0x10);
				current = info;
				while (current) {
					if (current->ai_family == AF_INET) {
						auto addr = factory.CreateIPv4(ectx);
						if (ectx.error_code) { freeaddrinfo(info); return 0; }
						sockaddr_in * ipv4 = reinterpret_cast<sockaddr_in *>(current->ai_addr);
						MemoryCopy(&addr->IP, &ipv4->sin_addr, 4);
						addr->Port = Network::InverseEndianess(ipv4->sin_port);
						result->Append(addr);
					} else if (current->ai_family == AF_INET6) {
						auto addr = factory.CreateIPv6(ectx);
						if (ectx.error_code) { freeaddrinfo(info); return 0; }
						sockaddr_in6 * ipv6 = reinterpret_cast<sockaddr_in6 *>(current->ai_addr);
						for (int i = 0; i < 16; i += 2) swap(ipv6->sin6_addr.s6_addr[i], ipv6->sin6_addr.s6_addr[i + 1]);
						MemoryCopy(&addr->IP, &ipv6->sin6_addr, 16);
						addr->Port = Network::InverseEndianess(ipv6->sin6_port);
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