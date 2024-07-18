#include "xe_netapi_macosx.h"

#ifdef ENGINE_MACOSX

#include <CoreFoundation/CoreFoundation.h>
#include <Network/Network.h>
#include "xe_netapi_unix.h"

#include <PlatformDependent/CocoaInterop2.h>
#include <Network/Punycode.h>

#define XE_TRY_INTRO try {
#define XE_TRY_OUTRO(DRV) } catch (Engine::InvalidArgumentException &) { ectx.error_code = 3; ectx.error_subcode = 0; return DRV; } \
catch (Engine::InvalidFormatException &) { ectx.error_code = 4; ectx.error_subcode = 0; return DRV; } \
catch (Engine::InvalidStateException &) { ectx.error_code = 5; ectx.error_subcode = 0; return DRV; } \
catch (Engine::OutOfMemoryException &) { ectx.error_code = 2; ectx.error_subcode = 0; return DRV; } \
catch (Engine::IO::FileAccessException & e) { ectx.error_code = 6; ectx.error_subcode = e.code; return DRV; } \
catch (Engine::Exception &) { ectx.error_code = 1; ectx.error_subcode = 0; return DRV; } \
catch (int & e) { ectx.error_code = 8; ectx.error_subcode = e; return DRV; } \
catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return DRV; }

namespace Engine
{
	namespace XE
	{
		void SetNWError(nw_error_t error, ErrorContext & ectx) noexcept
		{
			if (nw_error_get_error_domain(error) == nw_error_domain_tls) SetXEError(ectx, 0x8, 0xB);
			else if (nw_error_get_error_domain(error) == nw_error_domain_dns) SetXEError(ectx, 0x8, 0xC);
			else if (nw_error_get_error_domain(error) == nw_error_domain_posix) SetPosixError(nw_error_get_error_code(error), ectx);
			else SetXEError(ectx, 0x8, 0x1);
		}

		class NWNetworkChannel : public INetworkChannel
		{
			class ConnectDesc : public Object
			{
				nw_connection_t _connection;
				ErrorContext * _error;
				SafePointer<IDispatchTask> _handler;
			public:
				ConnectDesc(nw_connection_t connection, ErrorContext * error, IDispatchTask * hdlr) : _connection(connection), _error(error)
				{
					nw_retain(_connection);
					_handler.SetRetain(hdlr);
				}
				virtual ~ConnectDesc(void) override { nw_release(_connection); }
				void Seal(ErrorContext error) noexcept
				{
					if (_error) *_error = error;
					if (_handler) _handler->DoTask(0);
					_handler.SetReference(0);
					nw_connection_set_state_changed_handler(_connection, 0);
				}
			};
			class VerifyDesc : public Object
			{
				Array<char> _host;
				SecCertificateRef _root;
				CFArrayRef _root_array;
				bool _ignore_security;
			public:
				VerifyDesc(const NetworkSecurityDescriptor & sec) : _host(1), _root(0), _root_array(0)
				{
					if (sec.Domain.Length()) {
						auto punycoded = Network::DomainNameToPunycode(sec.Domain);
						_host.SetLength(punycoded.GetEncodedLength(Encoding::UTF8) + 1);
						punycoded.Encode(_host.GetBuffer(), Encoding::UTF8, true);
					}
					if (sec.Certificate) {
						auto data_ref = CFDataCreateWithBytesNoCopy(0, sec.Certificate->GetBuffer(), sec.Certificate->Length(), kCFAllocatorNull);
						if (!data_ref) throw OutOfMemoryException();
						_root = SecCertificateCreateWithData(0, data_ref);
						CFRelease(data_ref);
						if (!_root) throw InvalidFormatException();
						const void * root = _root;
						_root_array = CFArrayCreate(0, &root, 1, &kCFTypeArrayCallBacks);
						if (!_root_array) {
							CFRelease(_root);
							throw OutOfMemoryException();
						}
					}
					_ignore_security = sec.IgnoreSecurity;
				}
				virtual ~VerifyDesc(void) override
				{
					if (_root) CFRelease(_root);
					if (_root_array) CFRelease(_root_array);
				}
				bool Verify(sec_protocol_metadata_t metadata, sec_trust_t trust_ref) noexcept
				{
					if (_ignore_security) return true;
					auto trust = sec_trust_copy_ref(trust_ref);
					if (_host.Length()) {
						auto host_name = CFStringCreateWithCString(0, _host.GetBuffer(), kCFStringEncodingUTF8);
						auto policy = SecPolicyCreateSSL(true, host_name);
						CFRelease(host_name);
						SecTrustSetPolicies(trust, policy);
						CFRelease(policy);
					} else {
						auto policy = SecPolicyCreateSSL(true, 0);
						SecTrustSetPolicies(trust, policy);
						CFRelease(policy);
					}
					if (_root_array) {
						SecTrustSetAnchorCertificatesOnly(trust, true);
						SecTrustSetAnchorCertificates(trust, _root_array);
					} else {
						SecTrustSetAnchorCertificatesOnly(trust, false);
					}
					auto trusted = SecTrustEvaluateWithError(trust, 0);
					CFRelease(trust);
					return trusted;
				}
			};
			class SendDesc : public Object
			{
				nw_connection_t _connection;
				ErrorContext * _error;
				int * _sent;
				SafePointer<IDispatchTask> _handler;
			public:
				SendDesc(nw_connection_t connection, ErrorContext * error, int * sent, IDispatchTask * hdlr) : _connection(connection), _error(error), _sent(sent)
				{
					nw_retain(_connection);
					_handler.SetRetain(hdlr);
				}
				virtual ~SendDesc(void) override { nw_release(_connection); }
				void Seal(int sent, ErrorContext error) noexcept
				{
					if (_sent) *_sent = sent;
					if (_error) *_error = error;
					if (_handler) _handler->DoTask(0);
					_handler.SetReference(0);
				}
			};
			class ReceiveDesc : public Object
			{
				nw_connection_t _connection;
				ErrorContext * _error;
				SafePointer<DataBlock> * _data;
				SafePointer<IDispatchTask> _handler;
			public:
				ReceiveDesc(nw_connection_t connection, ErrorContext * error, SafePointer<DataBlock> * data, IDispatchTask * hdlr) : _connection(connection), _error(error), _data(data)
				{
					nw_retain(_connection);
					_handler.SetRetain(hdlr);
				}
				virtual ~ReceiveDesc(void) override { nw_release(_connection); }
				void Seal(DataBlock * data, ErrorContext error) noexcept
				{
					if (_data) { *_data = data; if (data) data->Retain(); }
					if (_error) *_error = error;
					if (_handler) _handler->DoTask(0);
					_handler.SetReference(0);
				}
			};
		private:
			nw_connection_t _connection;
		public:
			NWNetworkChannel(void) : _connection(0) {}
			NWNetworkChannel(nw_connection_t connection) : _connection(connection) { nw_retain(_connection); }
			virtual ~NWNetworkChannel(void) noexcept
			{
				if (_connection) {
					nw_connection_force_cancel(_connection);
					nw_release(_connection);
				}
			}
			virtual string ToString(void) const override { return L"communicatio.canale"; }
			virtual void ConnectA(NetworkAddress * address, ErrorContext * error, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override { SetXEError(ectx, 1, 0); }
			virtual void ConnectB(NetworkAddress * address, NetworkSecurityDescriptor & sec, ErrorContext * error, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (_connection) { SetXEError(ectx, 5, 0); return; }
				XE_TRY_INTRO
				string ulnk;
				DataBlock sa(0x100);
				SocketAddressInit(sa, address, &ulnk);
				if (ulnk.Length()) throw Exception();
				SafePointer<VerifyDesc> verify = new VerifyDesc(sec);
				nw_endpoint_t endpoint = 0;
				nw_parameters_t params = 0;
				try {
					endpoint = nw_endpoint_create_address(reinterpret_cast<sockaddr *>(sa.GetBuffer()));
					if (!endpoint) throw OutOfMemoryException();
					params = nw_parameters_create_secure_tcp(^(nw_protocol_options_t options) {
						sec_protocol_options_set_verify_block((sec_protocol_options_t) options, ^(sec_protocol_metadata_t metadata, sec_trust_t trust_ref, sec_protocol_verify_complete_t complete) {
							complete(verify->Verify(metadata, trust_ref));
						}, dispatch_get_global_queue(QOS_CLASS_UTILITY, 0));
					}, NW_PARAMETERS_DEFAULT_CONFIGURATION);
					if (!params) throw OutOfMemoryException();
					_connection = nw_connection_create(endpoint, params);
					if (!_connection) throw OutOfMemoryException();
					nw_release(endpoint);
					endpoint = 0;
					nw_release(params);
					params = 0;
					SafePointer<ConnectDesc> desc = new ConnectDesc(_connection, error, hdlr);
					nw_connection_set_queue(_connection, dispatch_get_global_queue(QOS_CLASS_UTILITY, 0));
					nw_connection_set_state_changed_handler(_connection, ^(nw_connection_state_t state, nw_error_t error) {
						ErrorContext ectx;
						if (state == nw_connection_state_ready) {
							ClearXEError(ectx);
							desc->Seal(ectx);
						} else if (error) {
							SetNWError(error, ectx);
							desc->Seal(ectx);
						} else if (state == nw_connection_state_cancelled) {
							SetXEError(ectx, 0x8, 0x8);
							desc->Seal(ectx);
						} else if (state == nw_connection_state_waiting) {
							SetXEError(ectx, 0x8, 0x4);
							desc->Seal(ectx);
						} else if (state == nw_connection_state_failed || state == nw_connection_state_invalid) {
							SetXEError(ectx, 0x8, 0x1);
							desc->Seal(ectx);
						}
					});
					nw_connection_start(_connection);
				} catch (...) {
					if (endpoint) nw_release(endpoint);
					if (params) nw_release(params);
					if (_connection) nw_release(_connection);
					_connection = 0;
					throw;
				}
				XE_TRY_OUTRO()
			}
			virtual void Send(DataBlock * data, ErrorContext * error, int * sent, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (!_connection) { SetXEError(ectx, 5, 0); return; }
				if (!data || !data->Length()) {
					if (error) ClearXEError(*error);
					if (sent) *sent = 0;
					if (hdlr) hdlr->DoTask(0);
					return;
				}
				XE_TRY_INTRO
				SafePointer<SendDesc> desc = new SendDesc(_connection, error, sent, hdlr);
				int volume = data->Length();
				auto data_ref = dispatch_data_create(data->GetBuffer(), data->Length(), dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), DISPATCH_DATA_DESTRUCTOR_DEFAULT);
				if (!data_ref) throw OutOfMemoryException();
				nw_connection_send(_connection, data_ref, NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, true, ^(nw_error_t error) {
					ErrorContext ectx;
					if (error) {
						SetNWError(error, ectx);
						desc->Seal(0, ectx);
					} else {
						ClearXEError(ectx);
						desc->Seal(volume, ectx);
					}
				});
				dispatch_release(data_ref);
				XE_TRY_OUTRO()
			}
			virtual void Receive(int length, ErrorContext * error, SafePointer<DataBlock> * data, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (!_connection) { SetXEError(ectx, 5, 0); return; }
				if (length < 0) { SetXEError(ectx, 3, 0); return; }
				XE_TRY_INTRO
				if (!length) {
					SafePointer<DataBlock> zero = new DataBlock(1);
					if (error) ClearXEError(*error);
					if (data) *data = zero;
					if (hdlr) hdlr->DoTask(0);
					return;
				}
				SafePointer<ReceiveDesc> desc = new ReceiveDesc(_connection, error, data, hdlr);
				nw_connection_receive(_connection, length, length, ^(dispatch_data_t content, nw_content_context_t context, bool is_complete, nw_error_t error) {
					ErrorContext ectx;
					try {
						if (content) {
							auto volume = dispatch_data_get_size(content);
							SafePointer<DataBlock> data = new DataBlock(volume);
							data->SetLength(volume);
							dispatch_data_apply(content, ^ bool (dispatch_data_t region, size_t offset, const void *buffer, size_t size) {
								MemoryCopy(data->GetBuffer() + offset, buffer, size);
								return true;
							});
							ClearXEError(ectx);
							desc->Seal(data, ectx);
						} else if (is_complete) {
							SafePointer<DataBlock> data = new DataBlock(1);
							ClearXEError(ectx);
							desc->Seal(data, ectx);
						} else {
							SetNWError(error, ectx);
							desc->Seal(0, ectx);
						}
					} catch (...) {
						SetXEError(ectx, 2, 0);
						desc->Seal(0, ectx);
					}
				});
				XE_TRY_OUTRO()
			}
			virtual void Close(bool ultimately, ErrorContext & ectx) noexcept override
			{
				if (!_connection) { SetXEError(ectx, 5, 0); return; }
				if (ultimately) {
					nw_connection_cancel(_connection);
				} else {
					nw_connection_send(_connection, 0, NW_CONNECTION_FINAL_MESSAGE_CONTEXT, true, ^(nw_error_t error) {});
				}
			}
		};
		class NWNetworkListener : public INetworkListener
		{
			class AcceptDesc : public Object
			{
				int _usage_limit;
				ErrorContext * _error;
				SafePointer<INetworkChannel> * _channel;
				SafePointer<NetworkAddress> * _address;
				SafePointer<IDispatchTask> _handler;
			public:
				AcceptDesc(int limit, ErrorContext * error, SafePointer<INetworkChannel> * channel, SafePointer<NetworkAddress> * address, IDispatchTask * handler)
				{
					_usage_limit = limit;
					_error = error;
					_channel = channel;
					_address = address;
					_handler.SetRetain(handler);
				}
				virtual ~AcceptDesc(void) override {}
				int GetCurrentLimit(void) const noexcept { return _usage_limit; }
				void Decrement(void) noexcept { if (_usage_limit > 0) _usage_limit--; }
				void Seal(INetworkChannel * channel, NetworkAddress * address) noexcept
				{
					if (_error) ClearXEError(*_error);
					if (_channel) _channel->SetRetain(channel);
					if (_address) _address->SetRetain(address);
					if (_handler) _handler->DoTask(0);
				}
				void Seal(const ErrorContext & ectx) noexcept
				{
					if (_error) *_error = ectx;
					if (_channel) _channel->SetReference(0);
					if (_address) _address->SetReference(0);
					if (_handler) _handler->DoTask(0);
				}
			};
			class AcceptTask : public Object
			{
				nw_connection_t _connection;
				SafePointer<AcceptDesc> _desc;
				SafePointer<Semaphore> _exec;
				NetworkAddressFactory & _factory;
			private:
				void _create_instances(INetworkChannel ** channel, NetworkAddress ** address, ErrorContext & ectx) noexcept
				{
					XE_TRY_INTRO
					*channel = new NWNetworkChannel(_connection);
					auto endpoint = nw_connection_copy_endpoint(_connection);
					if (!endpoint) throw InvalidStateException();
					auto sa = nw_endpoint_get_address(endpoint);
					uint8 sal[0x100];
					MemoryCopy(&sal, sa, sa->sa_len);
					nw_release(endpoint);
					SocketAddressRead(&sal, &_factory, address);
					XE_TRY_OUTRO()
				}
			public:
				AcceptTask(nw_connection_t connection, AcceptDesc * desc, Semaphore * sync, NetworkAddressFactory & factory) : _connection(connection), _factory(factory)
				{
					nw_retain(_connection);
					_desc.SetRetain(desc);
					_exec.SetRetain(sync);
				}
				virtual ~AcceptTask(void) override { nw_release(_connection); }
				void Seal(void) noexcept
				{
					ErrorContext ectx;
					ClearXEError(ectx);
					nw_connection_set_state_changed_handler(_connection, 0);
					SafePointer<INetworkChannel> channel;
					SafePointer<NetworkAddress> address;
					_create_instances(channel.InnerRef(), address.InnerRef(), ectx);
					if (ectx.error_code) { Seal(ectx); return; }
					_exec->Wait();
					_desc->Seal(channel, address);
					_exec->Open();
				}
				void Seal(const ErrorContext & ectx) noexcept
				{
					nw_connection_force_cancel(_connection);
					nw_connection_set_state_changed_handler(_connection, 0);
					_exec->Wait();
					_desc->Seal(ectx);
					_exec->Open();
				}
			};
			class AcceptQueue : public Object
			{
				nw_listener_t _listener;
				SafePointer<Semaphore> _access_sync;
				SafePointer<Semaphore> _execute_sync;
				NetworkAddressFactory & _factory;
				Volumes::Queue< SafePointer<AcceptDesc> > _queue;
			private:
				void _update_listener(void) noexcept
				{
					if (_listener) {
						uint32_t length;
						auto first = _queue.GetFirst();
						if (first) {
							auto lim = first->GetValue()->GetCurrentLimit();
							if (lim >= 0) length = lim;
							else length = NW_LISTENER_INFINITE_CONNECTION_LIMIT;
						} else length = 0;
						nw_listener_set_new_connection_limit(_listener, length);
					}
				}
			public:
				AcceptQueue(nw_listener_t listener, NetworkAddressFactory & factory) : _listener(listener), _factory(factory)
				{
					_access_sync = CreateSemaphore(1);
					_execute_sync = CreateSemaphore(1);
					if (!_access_sync || !_execute_sync) throw OutOfMemoryException();
				}
				virtual ~AcceptQueue(void) override
				{
					ErrorContext error;
					SetXEError(error, 0x8, 0x8);
					for (auto & ad : _queue) ad->Seal(error);
				}
				void Detach(void) noexcept
				{
					_access_sync->Wait();
					_listener = 0;
					_access_sync->Open();
				}
				void Schedule(int limit, ErrorContext * error, SafePointer<INetworkChannel> * channel, SafePointer<NetworkAddress> * address, IDispatchTask * hdlr)
				{
					SafePointer<AcceptDesc> desc = new AcceptDesc(limit, error, channel, address, hdlr);
					_access_sync->Wait();
					try { _queue.Push(desc); } catch (...) { _access_sync->Open(); throw; }
					_update_listener();
					_access_sync->Open();
				}
				void Perform(nw_connection_t connection) noexcept
				{
					SafePointer<AcceptDesc> desc;
					_access_sync->Wait();
					auto first = _queue.GetFirst();
					if (first) {
						desc = first->GetValue();
						desc->Decrement();
						if (!desc->GetCurrentLimit()) _queue.RemoveFirst();
						_update_listener();
					}
					_access_sync->Open();
					if (desc) {
						SafePointer<AcceptTask> task;
						try { task = new AcceptTask(connection, desc, _execute_sync, _factory); } catch (...) {
							ErrorContext ectx;
							SetXEError(ectx, 2, 0);
							nw_connection_cancel(connection);
							_execute_sync->Wait();
							desc->Seal(ectx);
							_execute_sync->Open();
							return;
						}
						nw_connection_set_queue(connection, dispatch_get_global_queue(QOS_CLASS_UTILITY, 0));
						nw_connection_set_state_changed_handler(connection, ^(nw_connection_state_t state, nw_error_t error) {
							ErrorContext ectx;
							if (state == nw_connection_state_ready) {
								task->Seal();
							} else if (error) {
								SetNWError(error, ectx);
								task->Seal(ectx);
							} else if (state == nw_connection_state_cancelled) {
								SetXEError(ectx, 0x8, 0x8);
								task->Seal(ectx);
							} else if (state == nw_connection_state_waiting) {
								SetXEError(ectx, 0x8, 0x4);
								task->Seal(ectx);
							} else if (state == nw_connection_state_failed || state == nw_connection_state_invalid) {
								SetXEError(ectx, 0x8, 0x1);
								task->Seal(ectx);
							}
						});
						nw_connection_start(connection);
					} else nw_connection_cancel(connection);
				}
				void Cancel(const ErrorContext & error) noexcept
				{
					while (true) {
						SafePointer<AcceptDesc> desc;
						_access_sync->Wait();
						auto first = _queue.GetFirst();
						if (first) {
							desc = first->GetValue();
							_queue.RemoveFirst();
						}
						_update_listener();
						_access_sync->Open();
						if (!first) return;
						if (desc) {
							_execute_sync->Wait();
							desc->Seal(error);
							_execute_sync->Open();
						}
					}
				}
			};
		private:
			nw_listener_t _listener;
			NetworkAddressFactory & _factory;
			SafePointer<AcceptQueue> _queue;
		public:
			NWNetworkListener(NetworkAddressFactory & factory) : _listener(0), _factory(factory) {}
			virtual ~NWNetworkListener(void) noexcept
			{
				if (_queue) _queue->Detach();
				if (_listener) {
					nw_listener_cancel(_listener);
					nw_release(_listener);
				}
			}
			virtual string ToString(void) const override { return L"communicatio.attentor"; }
			virtual void BindA(NetworkAddress * address, ErrorContext & ectx) noexcept override { SetXEError(ectx, 1, 0); }
			virtual void BindB(NetworkAddress * address, NetworkIdentityDescriptor & idesc, ErrorContext & ectx) noexcept override
			{
				if (_listener) { SetXEError(ectx, 5, 0); return; }
				if (!idesc.Data) { SetXEError(ectx, 3, 0); return; }
				XE_TRY_INTRO
				SafePointer<DataBlock> password_utf8 = idesc.Password.EncodeSequence(Encoding::UTF8, true);
				SafePointer<AcceptQueue> queue;
				string ulnk;
				DataBlock sa(0x100);
				SocketAddressInit(sa, address, &ulnk);
				if (ulnk.Length()) throw Exception();
				CFDataRef ind_data_ref = 0;
				CFStringRef password_ref = 0;
				CFDictionaryRef ind_data_props_ref = 0;
				CFArrayRef ind_array_ref = 0;
				sec_identity_t identity_ref = 0;
				nw_endpoint_t endpoint = 0;
				nw_parameters_t params = 0;
				try {
					ind_data_ref = CFDataCreateWithBytesNoCopy(0, idesc.Data->GetBuffer(), idesc.Data->Length(), kCFAllocatorNull);
					if (!ind_data_ref) throw OutOfMemoryException();
					password_ref = CFStringCreateWithCString(0, reinterpret_cast<char *>(password_utf8->GetBuffer()), kCFStringEncodingUTF8);
					if (!password_ref) throw OutOfMemoryException();
					const void * key_0 = kSecImportExportPassphrase;
					const void * value_0 = password_ref;
					ind_data_props_ref = CFDictionaryCreate(0, &key_0, &value_0, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
					if (!ind_data_props_ref) throw OutOfMemoryException();
					auto status = SecPKCS12Import(ind_data_ref, ind_data_props_ref, &ind_array_ref);
					CFRelease(ind_data_ref);
					ind_data_ref = 0;
					CFRelease(password_ref);
					password_ref = 0;
					CFRelease(ind_data_props_ref);
					ind_data_props_ref = 0;
					if (status != errSecSuccess) {
						if (status == errSecDecode) throw InvalidFormatException();
						else if (status == errSecAuthFailed || status == errSecPkcs12VerifyFailure) throw 0x0B;
						else throw 0x01;
					}
					for (int i = 0; i < CFArrayGetCount(ind_array_ref); i++) {
						auto dict_ref = (CFDictionaryRef) CFArrayGetValueAtIndex(ind_array_ref, i);
						auto identity = (SecIdentityRef) CFDictionaryGetValue(dict_ref, kSecImportItemIdentity);
						if (identity) {
							identity_ref = sec_identity_create(identity);
							if (!identity_ref) throw OutOfMemoryException();
							break;
						}
					}
					if (!identity_ref) throw InvalidArgumentException();
					CFRelease(ind_array_ref);
					ind_array_ref = 0;
					endpoint = nw_endpoint_create_address(reinterpret_cast<sockaddr *>(sa.GetBuffer()));
					if (!endpoint) throw OutOfMemoryException();
					params = nw_parameters_create_secure_tcp(^(nw_protocol_options_t options) {
						sec_protocol_options_set_local_identity((sec_protocol_options_t) options, identity_ref);
					}, NW_PARAMETERS_DEFAULT_CONFIGURATION);
					if (!params) throw OutOfMemoryException();
					nw_parameters_set_local_endpoint(params, endpoint);
					sec_release(identity_ref);
					identity_ref = 0;
					nw_release(endpoint);
					endpoint = 0;
					_listener = nw_listener_create(params);
					nw_release(params);
					params = 0;
					if (!_listener) throw OutOfMemoryException();
					try { queue = new AcceptQueue(_listener, _factory); } catch (...) { nw_release(_listener); throw; }
					nw_listener_set_queue(_listener, dispatch_get_global_queue(QOS_CLASS_UTILITY, 0));
					nw_listener_set_new_connection_handler(_listener, ^(nw_connection_t connection) {
						queue->Perform(connection);
					});
					nw_listener_set_state_changed_handler(_listener, ^(nw_listener_state_t state, nw_error_t error) {
						ErrorContext ectx;
						if (error) {
							SetNWError(error, ectx);
							queue->Cancel(ectx);
						} else if (state == nw_listener_state_cancelled) {
							SetXEError(ectx, 0x8, 0x8);
							queue->Cancel(ectx);
						} else if (state == nw_listener_state_waiting) {
							SetXEError(ectx, 0x8, 0x4);
							queue->Cancel(ectx);
						} else if (state == nw_listener_state_failed || state == nw_listener_state_invalid) {
							SetXEError(ectx, 0x8, 0x1);
							queue->Cancel(ectx);
						}
					});
					nw_listener_set_new_connection_limit(_listener, 0);
					_queue = queue;
					nw_listener_start(_listener);
				} catch (...) {
					if (ind_data_ref) CFRelease(ind_data_ref);
					if (password_ref) CFRelease(password_ref);
					if (ind_data_props_ref) CFRelease(ind_data_props_ref);
					if (ind_array_ref) CFRelease(ind_array_ref);
					if (identity_ref) sec_release(identity_ref);
					if (endpoint) nw_release(endpoint);
					if (params) nw_release(params);
					throw;
				}
				XE_TRY_OUTRO()
			}
			virtual void Accept(int limit, ErrorContext * error, SafePointer<INetworkChannel> * channel, SafePointer<NetworkAddress> * address, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (!_listener || !_queue) { SetXEError(ectx, 5, 0); return; }
				if (!limit) return;
				XE_TRY_INTRO
				_queue->Schedule(limit > 0 ? limit : -1, error, channel, address, hdlr);
				XE_TRY_OUTRO()
			}
			virtual void Close(ErrorContext & ectx) noexcept override
			{
				if (!_listener) { SetXEError(ectx, 5, 0); return; }
				nw_listener_cancel(_listener);
			}
		};

		SafePointer<INetworkChannel> CreateNetworkChannelS(NetworkAddress * address) { return new NWNetworkChannel; }
		SafePointer<INetworkListener> CreateNetworkListenerS(NetworkAddress * address, NetworkAddressFactory & factory) { return new NWNetworkListener(factory); }
	}
}
#endif