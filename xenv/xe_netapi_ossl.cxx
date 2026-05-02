#include "xe_netapi_ossl.h"
#include "xe_netapi_unix.h"

#ifdef ENGINE_LINUX

#include <Network/Punycode.h>
#include <openssl/ssl.h>
#include <openssl/pkcs12.h>
#include <openssl/err.h>
#include <poll.h>

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
		class OpenSSLContextLite : public Object
		{
			SSL_CTX * _ssl_ctx;
		public:
			OpenSSLContextLite(void) : _ssl_ctx(0) {}
			virtual ~OpenSSLContextLite(void) override { if (_ssl_ctx) SSL_CTX_free(_ssl_ctx); }
			void SetContext(SSL_CTX * ssl_ctx) noexcept { if (_ssl_ctx) SSL_CTX_free(_ssl_ctx); _ssl_ctx = ssl_ctx; if (_ssl_ctx) SSL_CTX_up_ref(_ssl_ctx); }
			SSL_CTX * GetContext(void) noexcept { return _ssl_ctx; }
		};
		class OpenSSLContext : public IOpenSSLEventHandler
		{
			SSL_CTX * _ssl_ctx;
			SSL * _ssl;
			BIO * _bio;
			bool _connection_alive, _eos_sent;
			uint _send_eos_mode;
		public:
			OpenSSLContext(void) : _ssl_ctx(0), _ssl(0), _bio(0), _connection_alive(true), _send_eos_mode(0), _eos_sent(false) {}
			virtual ~OpenSSLContext(void) override { if (_ssl_ctx) SSL_CTX_free(_ssl_ctx); if (_ssl) SSL_free(_ssl); if (_bio) BIO_free(_bio); }
			virtual OpenSSLEventStatus ProcessConnection(int pollstat, NetworkRequest * req_read, NetworkRequest * req_write, uint pollattr) noexcept override
			{
				ERR_clear_error();
				NetworkRequest * req_process = 0;
				OpenSSLEventStatus request_complete_responce;
				if (req_read && (req_read->in.attributes & REQUEST_ATTRIBUTE_PRIORITY)) {
					req_process = req_read;
					request_complete_responce = OpenSSLEventStatus::CompleteReadRequest;
				} else if (req_write && (req_write->in.attributes & REQUEST_ATTRIBUTE_PRIORITY)) {
					req_process = req_write;
					request_complete_responce = OpenSSLEventStatus::CompleteWriteRequest;
				} else if (req_write && (req_write->in.attributes & pollattr)) {
					req_process = req_write;
					request_complete_responce = OpenSSLEventStatus::CompleteWriteRequest;
				} else if (req_read && (req_read->in.attributes & pollattr)) {
					req_process = req_read;
					request_complete_responce = OpenSSLEventStatus::CompleteReadRequest;
				}
				if (!req_process) return OpenSSLEventStatus::None;
				if (!_connection_alive) {
					if (req_process->out.status) SetXEError(*req_process->out.status, 0x08, 0x08);
					if (req_process->in.attributes & REQUEST_ATTRIBUTE_ACCEPT_BIT) {
						if (req_process->out.address) (*req_process->out.address).SetReference(0);
						if (req_process->out.channel) (*req_process->out.channel).SetReference(0);
					}
					return request_complete_responce;
				}
				if (pollstat & POLLHUP) {
					if (req_process->out.status) SetXEError(*req_process->out.status, 0x08, 0x08);
					if (req_process->in.attributes & REQUEST_ATTRIBUTE_ACCEPT_BIT) {
						if (req_process->out.address) (*req_process->out.address).SetReference(0);
						if (req_process->out.channel) (*req_process->out.channel).SetReference(0);
					}
					return request_complete_responce;
				}
				if (req_process->in.attributes & REQUEST_ATTRIBUTE_CONNECT_BIT) {
					auto status = SSL_connect(_ssl);
					if (status <= 0) {
						auto error_status = SSL_get_error(_ssl, status);
						if (error_status == SSL_ERROR_WANT_READ) {
							req_process->in.attributes = REQUEST_ATTRIBUTE_CONNECT_BIT | REQUEST_ATTRIBUTE_POLL_READ | REQUEST_ATTRIBUTE_OPENSSL | REQUEST_ATTRIBUTE_PRIORITY | REQUEST_ATTRIBUTE_INCOMPLETE;
							return OpenSSLEventStatus::PollRenew;
						} else if (error_status == SSL_ERROR_WANT_WRITE) {
							req_process->in.attributes = REQUEST_ATTRIBUTE_CONNECT_BIT | REQUEST_ATTRIBUTE_POLL_WRITE | REQUEST_ATTRIBUTE_OPENSSL | REQUEST_ATTRIBUTE_PRIORITY | REQUEST_ATTRIBUTE_INCOMPLETE;
							return OpenSSLEventStatus::PollRenew;
						} else if (error_status == SSL_ERROR_SYSCALL) {
							if (req_process->out.status) SetPosixError(*req_process->out.status);
							return request_complete_responce;
						} else {
							if (req_process->out.status) SetXEError(*req_process->out.status, 0x08, 0x0B);
							_connection_alive = false;
							return request_complete_responce;
						}
					} else {
						if (req_process->out.status) ClearXEError(*req_process->out.status);
						return request_complete_responce;
					}
				} else if (req_process->in.attributes & REQUEST_ATTRIBUTE_RECEIVE_BIT) {
					size_t bytes_read = 0;
					auto status = SSL_read_ex(_ssl, req_process->in.data->GetBuffer() + req_process->in.pointer, req_process->in.length - req_process->in.pointer, &bytes_read);
					if (!status) {
						auto error_status = SSL_get_error(_ssl, status);
						if (error_status == SSL_ERROR_ZERO_RETURN) {
							try {
								req_process->in.data->SetLength(req_process->in.pointer);
								if (req_process->out.data) *req_process->out.data = req_process->in.data;
							} catch (...) {
								if (req_process->out.status) SetXEError(*req_process->out.status, 2, 0);
								return request_complete_responce;
							}
							if (req_process->out.status) ClearXEError(*req_process->out.status);
							return request_complete_responce;
						} else if (error_status == SSL_ERROR_WANT_READ) {
							req_process->in.attributes = REQUEST_ATTRIBUTE_RECEIVE_BIT | REQUEST_ATTRIBUTE_POLL_READ | REQUEST_ATTRIBUTE_OPENSSL | REQUEST_ATTRIBUTE_INCOMPLETE;
							return OpenSSLEventStatus::PollRenew;
						} else if (error_status == SSL_ERROR_WANT_WRITE) {
							req_process->in.attributes = REQUEST_ATTRIBUTE_RECEIVE_BIT | REQUEST_ATTRIBUTE_POLL_WRITE | REQUEST_ATTRIBUTE_OPENSSL | REQUEST_ATTRIBUTE_INCOMPLETE;
							return OpenSSLEventStatus::PollRenew;
						} else if (error_status == SSL_ERROR_SYSCALL) {
							if (req_process->out.status) SetPosixError(*req_process->out.status);
							return request_complete_responce;
						} else {
							if (req_process->out.status) SetXEError(*req_process->out.status, 0x08, 0x0B);
							_connection_alive = false;
							return request_complete_responce;
						}
					} else {
						req_process->in.pointer += bytes_read;
						if (req_process->in.pointer == req_process->in.length) {
							if (req_process->out.status) ClearXEError(*req_process->out.status);
							if (req_process->out.data) *req_process->out.data = req_process->in.data;
							return request_complete_responce;
						} else {
							req_process->in.attributes = REQUEST_ATTRIBUTE_RECEIVE_BIT | REQUEST_ATTRIBUTE_POLL_READ | REQUEST_ATTRIBUTE_OPENSSL;
							return OpenSSLEventStatus::PollRenew;
						}
					}
				} else if (req_process->in.attributes & REQUEST_ATTRIBUTE_SEND_BIT) {
					if (_eos_sent) {
						if (req_process->out.status) SetXEError(*req_process->out.status, 5, 0);
						return request_complete_responce;
					}
					if (req_process->in.data && (!_send_eos_mode || (req_process->in.attributes & REQUEST_ATTRIBUTE_INCOMPLETE))) {
						size_t written = 0;
						int status;
						if (req_process->in.length == req_process->in.pointer) status = 1;
						else status = SSL_write_ex(_ssl, req_process->in.data->GetBuffer() + req_process->in.pointer, req_process->in.length - req_process->in.pointer, &written);
						if (!status) {
							auto error_status = SSL_get_error(_ssl, status);
							if (error_status == SSL_ERROR_WANT_READ) {
								req_process->in.attributes = REQUEST_ATTRIBUTE_SEND_BIT | REQUEST_ATTRIBUTE_POLL_READ | REQUEST_ATTRIBUTE_OPENSSL | REQUEST_ATTRIBUTE_INCOMPLETE;
								return OpenSSLEventStatus::PollRenew;
							} else if (error_status == SSL_ERROR_WANT_WRITE) {
								req_process->in.attributes = REQUEST_ATTRIBUTE_SEND_BIT | REQUEST_ATTRIBUTE_POLL_WRITE | REQUEST_ATTRIBUTE_OPENSSL | REQUEST_ATTRIBUTE_PRIORITY | REQUEST_ATTRIBUTE_INCOMPLETE;
								return OpenSSLEventStatus::PollRenew;
							} else if (error_status == SSL_ERROR_SYSCALL) {
								if (req_process->out.status) SetPosixError(*req_process->out.status);
								return request_complete_responce;
							} else {
								if (req_process->out.status) SetXEError(*req_process->out.status, 0x08, 0x0B);
								_connection_alive = false;
								return request_complete_responce;
							}
						} else {
							req_process->in.pointer += written;
							if (req_process->in.pointer == req_process->in.length) {
								if (_send_eos_mode) {
									req_process->in.attributes = REQUEST_ATTRIBUTE_SEND_BIT | REQUEST_ATTRIBUTE_POLL_WRITE | REQUEST_ATTRIBUTE_OPENSSL;
									return OpenSSLEventStatus::PollRenew;
								} else {
									if (req_process->out.status) ClearXEError(*req_process->out.status);
									if (req_process->out.length) *req_process->out.length = req_process->in.length;
									return request_complete_responce;
								}
							} else {
								req_process->in.attributes = REQUEST_ATTRIBUTE_SEND_BIT | REQUEST_ATTRIBUTE_POLL_WRITE | REQUEST_ATTRIBUTE_OPENSSL;
								return OpenSSLEventStatus::PollRenew;
							}
						}
					} else if (!_eos_sent) {
						auto status = SSL_shutdown(_ssl);
						if (status < 0) {
							auto error_status = SSL_get_error(_ssl, status);
							if (error_status == SSL_ERROR_WANT_READ) {
								req_process->in.attributes = REQUEST_ATTRIBUTE_SEND_BIT | REQUEST_ATTRIBUTE_POLL_READ | REQUEST_ATTRIBUTE_OPENSSL;
								return OpenSSLEventStatus::PollRenew;
							} else if (error_status == SSL_ERROR_WANT_WRITE) {
								req_process->in.attributes = REQUEST_ATTRIBUTE_SEND_BIT | REQUEST_ATTRIBUTE_POLL_WRITE | REQUEST_ATTRIBUTE_OPENSSL | REQUEST_ATTRIBUTE_PRIORITY;
								return OpenSSLEventStatus::PollRenew;
							} else if (error_status == SSL_ERROR_SYSCALL) {
								if (req_process->out.status) SetPosixError(*req_process->out.status);
								_connection_alive = false;
								return request_complete_responce;
							} else {
								if (req_process->out.status) SetXEError(*req_process->out.status, 0x08, 0x0B);
								_connection_alive = false;
								return request_complete_responce;
							}
						} else {
							if (req_process->out.status) ClearXEError(*req_process->out.status);
							_eos_sent = true;
							return _send_eos_mode == 2 ? OpenSSLEventStatus::CloseChannel : OpenSSLEventStatus::CloseChannelWrite;
						}
					} else {
						if (req_process->out.status) SetXEError(*req_process->out.status, 5, 0);
						return request_complete_responce;
					}
				} else if (req_process->in.attributes & REQUEST_ATTRIBUTE_ACCEPT_BIT) {
					auto status = SSL_accept(_ssl);
					if (status <= 0) {
						auto error_status = SSL_get_error(_ssl, status);
						if (error_status == SSL_ERROR_WANT_READ) {
							req_process->in.attributes = REQUEST_ATTRIBUTE_ACCEPT_BIT | REQUEST_ATTRIBUTE_POLL_READ | REQUEST_ATTRIBUTE_OPENSSL | REQUEST_ATTRIBUTE_PRIORITY | REQUEST_ATTRIBUTE_INCOMPLETE;
							return OpenSSLEventStatus::PollRenew;
						} else if (error_status == SSL_ERROR_WANT_WRITE) {
							req_process->in.attributes = REQUEST_ATTRIBUTE_ACCEPT_BIT | REQUEST_ATTRIBUTE_POLL_WRITE | REQUEST_ATTRIBUTE_OPENSSL | REQUEST_ATTRIBUTE_PRIORITY | REQUEST_ATTRIBUTE_INCOMPLETE;
							return OpenSSLEventStatus::PollRenew;
						} else if (error_status == SSL_ERROR_SYSCALL) {
							if (req_process->out.status) SetPosixError(*req_process->out.status);
							if (req_process->out.address) (*req_process->out.address).SetReference(0);
							if (req_process->out.channel) (*req_process->out.channel).SetReference(0);
							return request_complete_responce;
						} else {
							if (req_process->out.status) SetXEError(*req_process->out.status, 0x08, 0x0B);
							if (req_process->out.address) (*req_process->out.address).SetReference(0);
							if (req_process->out.channel) (*req_process->out.channel).SetReference(0);
							_connection_alive = false;
							return request_complete_responce;
						}
					} else {
						if (req_process->out.status) ClearXEError(*req_process->out.status);
						return request_complete_responce;
					}
				} else return OpenSSLEventStatus::Failed;
			}
			void InitializeSecurity(NetworkSecurityDescriptor & sec, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				_ssl_ctx = SSL_CTX_new(TLS_client_method());
				if (!_ssl_ctx) throw OutOfMemoryException();
				if (sec.IgnoreSecurity) SSL_CTX_set_verify(_ssl_ctx, SSL_VERIFY_NONE, NULL);
				else SSL_CTX_set_verify(_ssl_ctx, SSL_VERIFY_PEER, NULL);
				if (sec.Certificate) {
					auto bio = BIO_new_mem_buf(sec.Certificate->GetBuffer(), sec.Certificate->Length());
					if (!bio) throw OutOfMemoryException();
					auto cert = d2i_X509_bio(bio, 0);
					BIO_free(bio);
					if (!cert) throw InvalidFormatException();
					auto store = X509_STORE_new();
					if (!store) { X509_free(cert); throw OutOfMemoryException(); }
					if (!X509_STORE_add_cert(store, cert)) { X509_free(cert); X509_STORE_free(store); throw OutOfMemoryException(); }
					X509_free(cert);
					auto status1 = SSL_CTX_set1_verify_cert_store(_ssl_ctx, store);
					auto status2 = SSL_CTX_set1_chain_cert_store(_ssl_ctx, store);
					X509_STORE_free(store);
					if (!status1 || !status2) throw InvalidStateException();
				} else {
					if (!SSL_CTX_set_default_verify_paths(_ssl_ctx)) throw InvalidStateException();
				}
				if (!SSL_CTX_set_min_proto_version(_ssl_ctx, TLS1_2_VERSION)) throw InvalidStateException();
				SSL_CTX_set_mode(_ssl_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE);
				_ssl = SSL_new(_ssl_ctx);
				if (!_ssl) throw OutOfMemoryException();
				if (sec.Domain.Length()) {
					auto domain = Network::DomainNameToPunycode(sec.Domain);
					Array<char> utf8(1);
					utf8.SetLength(domain.GetEncodedLength(Encoding::ANSI) + 1);
					domain.Encode(utf8.GetBuffer(), Encoding::ANSI, true);
					if (!SSL_set_tlsext_host_name(_ssl, utf8)) throw OutOfMemoryException();
					if (!SSL_set1_host(_ssl, utf8)) throw OutOfMemoryException();
				}
				XE_TRY_OUTRO()
			}
			void InitializeChannel(INetworkChannel * channel, ErrorContext & ectx) noexcept
			{
				UnixChannelSetOpenSSLEventHandler(channel, this);
				auto sock = UnixChannelGetSocket(channel);
				XE_TRY_INTRO
				if (sock < 0) throw InvalidStateException();
				_bio = BIO_new(BIO_s_fd());
				if (!_bio) throw OutOfMemoryException();
				BIO_set_fd(_bio, sock, BIO_NOCLOSE);
				SSL_set_bio(_ssl, _bio, _bio);
				_bio = 0;
				XE_TRY_OUTRO()
			}
			void Initialize(OpenSSLContextLite * context, INetworkChannel * channel, ErrorContext & ectx) noexcept
			{
				UnixChannelSetOpenSSLEventHandler(channel, this);
				auto sock = UnixChannelGetSocket(channel);
				XE_TRY_INTRO
				if (sock < 0) throw InvalidStateException();
				_ssl_ctx = context->GetContext();
				SSL_CTX_up_ref(_ssl_ctx);
				_ssl = SSL_new(_ssl_ctx);
				if (!_ssl) throw OutOfMemoryException();
				_bio = BIO_new(BIO_s_fd());
				if (!_bio) throw OutOfMemoryException();
				BIO_set_fd(_bio, sock, BIO_NOCLOSE);
				SSL_set_bio(_ssl, _bio, _bio);
				_bio = 0;
				XE_TRY_OUTRO()
			}
			void SendEndOfStream(bool ultimately) noexcept { _send_eos_mode = ultimately ? 2 : 1; }
		};
		class OpenSSLNetworkChannel : public INetworkChannel
		{
			SafePointer<INetworkChannel> _unix;
			SafePointer<OpenSSLContext> _ossl_ctx;
		public:
			OpenSSLNetworkChannel(void) {}
			OpenSSLNetworkChannel(INetworkChannel * base, OpenSSLContext * ossl_ctx) { _unix.SetRetain(base); _ossl_ctx.SetRetain(ossl_ctx); }
			virtual ~OpenSSLNetworkChannel(void) noexcept { if (_unix) { ErrorContext ectx; ClearXEError(ectx); _unix->Close(true, ectx); } }
			virtual string ToString(void) const override { return L"communicatio.canale"; }
			virtual void ConnectA(NetworkAddress * address, ErrorContext * error, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override { SetXEError(ectx, 1, 0); }
			virtual void ConnectB(NetworkAddress * address, NetworkSecurityDescriptor & sec, ErrorContext * error, IDispatchTask * hdlr, ErrorContext & ectx) noexcept 
			{
				XE_TRY_INTRO
				if (_unix || _ossl_ctx) throw InvalidStateException();
				_unix = CreateNetworkChannel(address);
				_ossl_ctx = new OpenSSLContext;
				_ossl_ctx->InitializeSecurity(sec, ectx);
				if (ectx.error_code) return;
				SafePointer<IDispatchTask> handler;
				handler.SetRetain(hdlr);
				auto connection = _unix;
				auto ossl_ctx = _ossl_ctx;
				auto connect_task = CreateStructuredTask<ErrorContext>([error, handler, connection, ossl_ctx](ErrorContext & error_status) {
					if (error_status.error_code) {
						if (error) *error = error_status;
						if (handler) handler->DoTask(0);
					} else {
						ErrorContext ectx;
						ClearXEError(ectx);
						ossl_ctx->InitializeChannel(connection, ectx);
						if (ectx.error_code) {
							if (error) *error = ectx;
							if (handler) handler->DoTask(0);
						} else {
							try {
								NetworkRequest req;
								req.in.attributes = REQUEST_ATTRIBUTE_CONNECT | REQUEST_ATTRIBUTE_OPENSSL | REQUEST_ATTRIBUTE_PRIORITY;
								req.in.length = req.in.pointer = 0;
								req.out.handler = handler;
								req.out.status = error;
								req.out.length = 0;
								req.out.data = 0;
								req.out.address = 0;
								req.out.channel = 0;
								UnixChannelEnqueueRequest(connection, req);
							} catch (...) {
								if (error) SetXEError(*error, 2, 0);
								if (handler) handler->DoTask(0);
							}
						}
					}
				});
				_unix->ConnectA(address, &connect_task->Value1, connect_task, ectx);
				XE_TRY_OUTRO()
			}
			virtual void Send(DataBlock * data, ErrorContext * error, int * sent, IDispatchTask * hdlr, ErrorContext & ectx) noexcept 
			{
				if (!_unix) { SetXEError(ectx, 5, 0); return; }
				if (!data || !data->Length()) {
					if (error) ClearXEError(*error);
					if (sent) *sent = 0;
					if (hdlr) hdlr->DoTask(0);
					return;
				}
				XE_TRY_INTRO
				NetworkRequest req;
				req.in.attributes = REQUEST_ATTRIBUTE_SEND | REQUEST_ATTRIBUTE_OPENSSL;
				req.in.length = data->Length();
				req.in.pointer = 0;
				req.in.data.SetRetain(data);
				req.out.handler.SetRetain(hdlr);
				req.out.status = error;
				req.out.length = sent;
				req.out.data = 0;
				req.out.address = 0;
				req.out.channel = 0;
				UnixChannelEnqueueRequest(_unix, req);
				XE_TRY_OUTRO()
			}
			virtual void Receive(int length, ErrorContext * error, SafePointer<DataBlock> * data, IDispatchTask * hdlr, ErrorContext & ectx) noexcept 
			{
				if (!_unix) { SetXEError(ectx, 5, 0); return; }
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
				req.in.attributes = REQUEST_ATTRIBUTE_RECEIVE | REQUEST_ATTRIBUTE_OPENSSL;
				req.in.length = length;
				req.in.pointer = 0;
				req.in.data = responce;
				req.out.handler.SetRetain(hdlr);
				req.out.status = error;
				req.out.length = 0;
				req.out.data = data;
				req.out.address = 0;
				req.out.channel = 0;
				UnixChannelEnterCriticalSection(_unix);
				auto status = _ossl_ctx->ProcessConnection(0, &req, 0, REQUEST_ATTRIBUTE_POLL_READ);
				UnixChannelLeaveCriticalSection(_unix);
				if (status == OpenSSLEventStatus::CompleteReadRequest || status == OpenSSLEventStatus::Failed) {
					if (req.out.handler) req.out.handler->DoTask(0);
				} else UnixChannelEnqueueRequest(_unix, req);
				XE_TRY_OUTRO()
			}
			virtual void Close(bool ultimately, ErrorContext & ectx) noexcept 
			{
				if (!_unix) { SetXEError(ectx, 5, 0); return; }
				XE_TRY_INTRO
				UnixChannelEnterCriticalSection(_unix);
				_ossl_ctx->SendEndOfStream(ultimately);
				UnixChannelLeaveCriticalSection(_unix);
				NetworkRequest req;
				req.in.attributes = REQUEST_ATTRIBUTE_SEND | REQUEST_ATTRIBUTE_OPENSSL;
				req.in.length = req.in.pointer = 0;
				req.out.status = 0;
				req.out.length = 0;
				req.out.data = 0;
				req.out.address = 0;
				req.out.channel = 0;
				UnixChannelEnqueueRequest(_unix, req);
				XE_TRY_OUTRO()
			}
		};
		class OpenSSLNetworkListener : public INetworkListener
		{
			NetworkAddressFactory & _factory;
			SafePointer<INetworkListener> _unix;
			SafePointer<OpenSSLContextLite> _ossl_ctx;
		public:
			OpenSSLNetworkListener(NetworkAddressFactory & factory) : _factory(factory) {}
			virtual ~OpenSSLNetworkListener(void) noexcept { if (_unix) { ErrorContext ectx; ClearXEError(ectx); _unix->Close(ectx); } }
			virtual string ToString(void) const override { return L"communicatio.attentor"; }
			virtual void BindA(NetworkAddress * address, ErrorContext & ectx) noexcept override { SetXEError(ectx, 1, 0); }
			virtual void BindB(NetworkAddress * address, NetworkIdentityDescriptor & idesc, ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				if (_unix || _ossl_ctx) throw InvalidStateException();
				_unix = CreateNetworkListener(address, _factory);
				_ossl_ctx = new OpenSSLContextLite;
				if (!idesc.Data) { SetXEError(ectx, 3, 0); return; }
				Array<char> password(1);
				password.SetLength(idesc.Password.GetEncodedLength(Encoding::UTF8) + 1);
				idesc.Password.Encode(password.GetBuffer(), Encoding::UTF8, true);
				auto bio = BIO_new_mem_buf(idesc.Data->GetBuffer(), idesc.Data->Length());
				if (!bio) throw OutOfMemoryException();
				auto pkcs12 = d2i_PKCS12_bio(bio, 0);
				BIO_free(bio);
				if (!pkcs12) throw InvalidFormatException();
				EVP_PKEY * key = 0;
				X509 * cert = 0;
				STACK_OF(X509) * chain = 0;
				auto pkcs12_status = PKCS12_parse(pkcs12, password, &key, &cert, &chain);
				PKCS12_free(pkcs12);
				if (!pkcs12_status) { SetXEError(ectx, 0x08, 0x0B); return; }
				auto ssl_ctx = SSL_CTX_new(TLS_server_method());
				if (!ssl_ctx) { EVP_PKEY_free(key); X509_free(cert); sk_X509_pop_free(chain, X509_free); throw OutOfMemoryException(); }
				_ossl_ctx->SetContext(ssl_ctx);
				SSL_CTX_free(ssl_ctx);
				if (!SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_2_VERSION)) { EVP_PKEY_free(key); X509_free(cert); sk_X509_pop_free(chain, X509_free); throw OutOfMemoryException(); }
				SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_RENEGOTIATION);
				SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, 0);
				if (!SSL_CTX_use_cert_and_key(ssl_ctx, cert, key, chain, 0)) { EVP_PKEY_free(key); X509_free(cert); sk_X509_pop_free(chain, X509_free); throw InvalidStateException(); }
				EVP_PKEY_free(key);
				X509_free(cert);
				sk_X509_pop_free(chain, X509_free);
				_unix->BindA(address, ectx);
				XE_TRY_OUTRO()
			}
			virtual void Accept(int limit, ErrorContext * error, SafePointer<INetworkChannel> * channel, SafePointer<NetworkAddress> * address, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				XE_TRY_INTRO
				if (!_unix || !_ossl_ctx) throw InvalidStateException();
				SafePointer<IDispatchTask> handler;
				handler.SetRetain(hdlr);
				auto internal_accept = CreateStructuredTask< ErrorContext, SafePointer<INetworkChannel> >([error, channel, address, handler, ossl = _ossl_ctx](const ErrorContext & status, SafePointer<INetworkChannel> & inner) {
					if (status.error_code) {
						if (error) *error = status;
						if (handler) handler->DoTask(0);
					} else {
						SafePointer<OpenSSLContext> full_context;
						SafePointer<OpenSSLNetworkChannel> new_channel;
						try {
							full_context = new OpenSSLContext;
							new_channel = new OpenSSLNetworkChannel(inner, full_context);
						} catch (...) {
							if (error) SetXEError(*error, 2, 0);
							if (address) (*address).SetReference(0);
							if (handler) handler->DoTask(0);
							return;
						}
						ErrorContext ectx;
						ClearXEError(ectx);
						full_context->Initialize(ossl, inner, ectx);
						if (ectx.error_code) {
							if (error) *error = ectx;
							if (address) (*address).SetReference(0);
							if (handler) handler->DoTask(0);
							return;
						}
						if (channel) (*channel).SetRetain(new_channel);
						try {
							NetworkRequest req;
							req.in.attributes = REQUEST_ATTRIBUTE_ACCEPT | REQUEST_ATTRIBUTE_OPENSSL | REQUEST_ATTRIBUTE_PRIORITY;
							req.in.length = req.in.pointer = 0;
							req.out.handler.SetRetain(CreateFunctionalTask([handler, new_channel]() { if (handler) handler->DoTask(0); }));
							req.out.status = error;
							req.out.length = 0;
							req.out.data = 0;
							req.out.address = address;
							req.out.channel = channel;
							UnixChannelEnqueueRequest(inner, req);
						} catch (...) {
							if (error) SetXEError(*error, 2, 0);
							if (address) (*address).SetReference(0);
							if (channel) (*channel).SetReference(0);
							if (handler) handler->DoTask(0);
						}
					}
				});
				_unix->Accept(limit, &internal_accept->Value1, &internal_accept->Value2, address, internal_accept, ectx);
				XE_TRY_OUTRO()
			}
			virtual void Close(ErrorContext & ectx) noexcept override { if (!_unix) { SetXEError(ectx, 5, 0); return; } _unix->Close(ectx); }
		};

		SafePointer<INetworkChannel> CreateNetworkChannelS(NetworkAddress * address) { return new OpenSSLNetworkChannel; }
		SafePointer<INetworkListener> CreateNetworkListenerS(NetworkAddress * address, NetworkAddressFactory & factory) { return new OpenSSLNetworkListener(factory); }
	}
}
#endif