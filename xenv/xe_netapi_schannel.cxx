#include "xe_netapi_schannel.h"
#include "xe_netapi_windows.h"

#ifdef ENGINE_WINDOWS

#define SECURITY_WIN32
#define SCHANNEL_USE_BLACKLISTS

#include <windows.h>

typedef struct _UNICODE_STRING {
	USHORT Length;
	USHORT MaximumLength;
	PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;

#include <sspi.h>
#include <schannel.h>
#include <Network/Punycode.h>

#pragma comment(lib, "Secur32.lib")
#pragma comment(lib, "Crypt32.lib")

#undef ZeroMemory
#undef CreateSemaphore

#define XE_TRY_INTRO try {
#define XE_TRY_OUTRO(DRV) } catch (Engine::InvalidArgumentException &) { ectx.error_code = 3; ectx.error_subcode = 0; return DRV; } \
catch (Engine::InvalidFormatException &) { ectx.error_code = 4; ectx.error_subcode = 0; return DRV; } \
catch (Engine::InvalidStateException &) { ectx.error_code = 5; ectx.error_subcode = 0; return DRV; } \
catch (Engine::OutOfMemoryException &) { ectx.error_code = 2; ectx.error_subcode = 0; return DRV; } \
catch (Engine::IO::FileAccessException & e) { ectx.error_code = 6; ectx.error_subcode = e.code; return DRV; } \
catch (Engine::Exception &) { ectx.error_code = 1; ectx.error_subcode = 0; return DRV; } \
catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return DRV; }

#define CHANNEL_STATE_INITIAL	0
#define CHANNEL_STATE_READY		1
#define CHANNEL_STATE_LAUNCHING	2
#define CHANNEL_STATE_FAILED	3

#define LISTENER_STATE_UNBOUND	0
#define LISTENER_STATE_BOUND	1

namespace Engine
{
	namespace XE
	{
		struct SecurityContextStore : public Object
		{
			SecurityContextStore(const CtxtHandle & sec) : security(sec) {}
			virtual ~SecurityContextStore(void) override { DeleteSecurityContext(&security); }
		public:
			CtxtHandle security;
		};
		struct ClientCredentialsStore : public Object
		{
			ClientCredentialsStore(const CredHandle & cred) : credentials(cred) {}
			virtual ~ClientCredentialsStore(void) override { FreeCredentialsHandle(&credentials); }
		public:
			CredHandle credentials;
		};
		struct ServerCredentialsStore : public Object
		{
			ServerCredentialsStore(PCCERT_CONTEXT cert, const CredHandle & cred) : certificate(cert), credentials(cred) {}
			virtual ~ServerCredentialsStore(void) override { CertFreeCertificateContext(certificate); FreeCredentialsHandle(&credentials); }
		public:
			PCCERT_CONTEXT certificate;
			CredHandle credentials;
		};
		struct SecurityCheckStore : public Object
		{
			PCCERT_CONTEXT _root_cert;
			HCERTSTORE _root_store;
			HCERTCHAINENGINE _root_engine;
			DynamicString _domain_name;
			bool _ignore_security;
		public:
			SecurityCheckStore(NetworkSecurityDescriptor & sec) : _root_cert(0), _root_store(0), _root_engine(0)
			{
				if (sec.IgnoreSecurity) {
					_ignore_security = true;
				} else {
					_ignore_security = false;
					if (sec.Domain.Length()) _domain_name << Network::DomainNameToPunycode(sec.Domain);
					if (sec.Certificate) {
						CERT_BLOB blob;
						blob.pbData = sec.Certificate->GetBuffer();
						blob.cbData = sec.Certificate->Length();
						if (!CryptQueryObject(CERT_QUERY_OBJECT_BLOB, &blob, CERT_QUERY_CONTENT_FLAG_CERT, CERT_QUERY_FORMAT_FLAG_ALL,
							X509_ASN_ENCODING, 0, 0, 0, 0, 0, reinterpret_cast<const void **>(&_root_cert))) throw InvalidFormatException();
						_root_store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, CERT_STORE_CREATE_NEW_FLAG, 0);
						if (!_root_store) {
							CertFreeCertificateContext(_root_cert);
							throw OutOfMemoryException();
						}
						auto status = CertAddCertificateContextToStore(_root_store, _root_cert, CERT_STORE_ADD_ALWAYS, 0);
						if (!status) {
							CertFreeCertificateContext(_root_cert);
							CertCloseStore(_root_store, CERT_CLOSE_STORE_FORCE_FLAG);
							throw OutOfMemoryException();
						}
						CERT_CHAIN_ENGINE_CONFIG config;
						ZeroMemory(&config, sizeof(config));
						config.cbSize = sizeof(config);
						config.hExclusiveRoot = _root_store;
						status = CertCreateCertificateChainEngine(&config, &_root_engine);
						if (!status) {
							CertFreeCertificateContext(_root_cert);
							CertCloseStore(_root_store, CERT_CLOSE_STORE_FORCE_FLAG);
							throw OutOfMemoryException();
						}
					}
				}
			}
			virtual ~SecurityCheckStore(void) override
			{
				if (_root_cert) CertFreeCertificateContext(_root_cert);
				if (_root_engine) CertFreeCertificateChainEngine(_root_engine);
				if (_root_store) CertCloseStore(_root_store, CERT_CLOSE_STORE_FORCE_FLAG);
			}
			LPWCH GetDomainName(void) noexcept { return _domain_name.Length() ? _domain_name.GetBuffer() : 0; }
			bool Verify(PCCERT_CONTEXT server) noexcept
			{
				if (_ignore_security) return true;
				BOOL status;
				PCCERT_CHAIN_CONTEXT chain;
				CERT_CHAIN_PARA chain_params;
				chain_params.cbSize = sizeof(chain_params);
				chain_params.RequestedUsage.dwType = USAGE_MATCH_TYPE_AND;
				chain_params.RequestedUsage.Usage.cUsageIdentifier = 0;
				chain_params.RequestedUsage.Usage.rgpszUsageIdentifier = 0;
				status = CertGetCertificateChain(_root_engine, server, 0, _root_store, &chain_params, 0, 0, &chain);
				if (!status) return false;
				SSL_EXTRA_CERT_CHAIN_POLICY_PARA ssl_policy_params;
				ssl_policy_params.cbSize = sizeof(ssl_policy_params);
				ssl_policy_params.dwAuthType = AUTHTYPE_SERVER;
				ssl_policy_params.fdwChecks = 0;
				ssl_policy_params.pwszServerName = _domain_name;
				CERT_CHAIN_POLICY_PARA policy_params;
				policy_params.cbSize = sizeof(policy_params);
				policy_params.dwFlags = 0;
				policy_params.pvExtraPolicyPara = _domain_name.Length() ? &ssl_policy_params : 0;
				CERT_CHAIN_POLICY_STATUS policy_status;
				policy_status.cbSize = sizeof(policy_status);
				policy_status.pvExtraPolicyStatus = 0;
				status = CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL, chain, &policy_params, &policy_status);
				CertFreeCertificateChain(chain);
				return status && policy_status.dwError == 0;
			}
		};

		class SSPINetworkChannel : public INetworkChannel
		{
			struct ReceiveTask
			{
				int length;
				ErrorContext * error;
				SafePointer<DataBlock> * data;
				SafePointer<IDispatchTask> handler;
			};
			class DetachedState : public Object
			{
			public:
				uint state;
				// General State
				SafePointer<Semaphore> sync;
				SafePointer<INetworkChannel> wsa_channel;
				SafePointer<ClientCredentialsStore> client_store;
				SafePointer<ServerCredentialsStore> server_store;
				SafePointer<SecurityContextStore> security;
				SecPkgContext_StreamSizes data_sizes;
				PCredHandle credentials;
				// Pending Data State
				bool end_of_stream;
				DataBlock pending_data;
				int pending_data_position;
				// Pending Data Requests State
				Volumes::Queue<ReceiveTask> receive_tasks;
			public:
				DetachedState(INetworkChannel * channel) : end_of_stream(false), pending_data(0x1000), pending_data_position(0)
				{
					sync = CreateSemaphore(1);
					if (!sync) throw OutOfMemoryException();
					wsa_channel.SetRetain(channel);
				}
				virtual ~DetachedState(void) override {}
				void Shutdown(void) noexcept { sync->Wait(); wsa_channel.SetReference(0); sync->Open(); }
			};
			class Connector : public IDispatchTask
			{
				int _last_data_request;
				uint _state; // 0 - connecting, 1 - receiving header, 2 - receiving body, 3 - sending then continue, 4 - sending then success, 5 - sending then fail
				ErrorContext _error;
				SafePointer<SecurityContextStore> _security;
				SafePointer<DataBlock> _data, _header;
			public:
				SafePointer<DetachedState> state;
				SafePointer<SecurityCheckStore> sec_check;
			public:
				ErrorContext * result_error;
				SafePointer<IDispatchTask> handler;
			private:
				void Seal(void) noexcept
				{
					if (_error.error_code) {
						ErrorContext local;
						ClearXEError(local);
						state->state = CHANNEL_STATE_FAILED;
						state->sync->Wait();
						auto channel = state->wsa_channel;
						state->sync->Open();
						if (channel) channel->Close(true, local);
						if (result_error) *result_error = _error;
						if (handler) handler->DoTask(0);
					} else {
						state->state = CHANNEL_STATE_READY;
						if (result_error) ClearXEError(*result_error);
						if (handler) handler->DoTask(0);
					}
				}
				void Seal(const ErrorContext & ectx) noexcept { _error = ectx; Seal(); }
				void VerifySecurity(void) noexcept
				{
					PCERT_CONTEXT cert;
					if (QueryContextAttributesW(&state->security->security, SECPKG_ATTR_STREAM_SIZES, &state->data_sizes) != SEC_E_OK) {
						SetXEError(_error, 5, 0);
						Seal();
						return;
					}
					if (QueryContextAttributesW(&state->security->security, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &cert) != SEC_E_OK) {
						SetXEError(_error, 5, 0);
						Seal();
						return;
					}
					bool trust = sec_check->Verify(cert);
					CertFreeCertificateContext(cert);
					if (trust) {
						ClearXEError(_error);
						Seal();
					} else {
						SetXEError(_error, 0x8, 0xB);
						Seal();
					}
				}
				void InitiateReceivePackage(void) noexcept
				{
					_state = 1;
					ErrorContext local;
					ClearXEError(local);
					state->sync->Wait();
					auto channel = state->wsa_channel;
					state->sync->Open();
					if (channel) channel->Receive(5, &_error, &_header, this, local); else SetXEError(local, 0x8, 0x8);
					if (local.error_code) Seal(local);
				}
				DataBlock * PrepareSendRecords(SecBuffer * buffers, int count, ErrorContext & ectx) noexcept
				{
					XE_TRY_INTRO
					int output_length = 0;
					for (int i = 0; i < count; i++) if (buffers[i].BufferType) output_length += buffers[i].cbBuffer;
					if (output_length) {
						SafePointer<DataBlock> result = new DataBlock(1);
						result->SetLength(output_length);
						output_length = 0;
						for (int i = 0; i < count; i++) if (buffers[i].BufferType) {
							MemoryCopy(result->GetBuffer() + output_length, buffers[i].pvBuffer, buffers[i].cbBuffer);
							output_length += buffers[i].cbBuffer;
						}
						result->Retain();
						return result;
					} else return 0;
					XE_TRY_OUTRO(0)
				}
			public:
				Connector(void) : _state(0) { ClearXEError(_error); }
				virtual ~Connector(void) override {}
				void Initiate(NetworkAddress * address) noexcept
				{
					ErrorContext ectx;
					ClearXEError(ectx);
					state->sync->Wait();
					auto channel = state->wsa_channel;
					state->sync->Open();
					_state = 0;
					if (channel) channel->ConnectA(address, &_error, this, ectx); else SetXEError(ectx, 0x8, 0x8);
					if (ectx.error_code) Seal(ectx);
				}
				virtual void DoTask(IDispatchQueue * queue) noexcept override
				{
					if (_state == 0) {
						if (_error.error_code) {
							Seal();
						} else {
							SecBufferDesc desc;
							SecBuffer buffer;
							ULONG attributes;
							desc.ulVersion = SECBUFFER_VERSION;
							desc.cBuffers = 1;
							desc.pBuffers = &buffer;
							ZeroMemory(&buffer, sizeof(buffer));
							CtxtHandle context;
							context.dwLower = context.dwUpper = 0;
							auto status = InitializeSecurityContextW(state->credentials, 0, sec_check->GetDomainName(),
								ISC_REQ_CONFIDENTIALITY | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_MANUAL_CRED_VALIDATION,
								0, 0, 0, 0, &context, &desc, &attributes, 0);
							if (context.dwLower || context.dwUpper) {
								try { state->security = new SecurityContextStore(context); }
								catch (...) { DeleteSecurityContext(&context); SetXEError(_error, 2, 0); Seal(); return; }
							}
							SafePointer<DataBlock> send = PrepareSendRecords(&buffer, 1, _error);
							if (_error.error_code) { Seal(); return; }
							if (status == SEC_E_OK) {
								if (send) _state = 4;
								else VerifySecurity();
							} else if (status == SEC_I_CONTINUE_NEEDED) {
								if (send) _state = 3;
								else InitiateReceivePackage();
							} else {
								if (send) _state = 5;
								else { SetXEError(_error, 0x8, 0xB); Seal(); return; }
							}
							FreeContextBuffer(buffer.pvBuffer);
							if (send) {
								_header = send;
								state->sync->Wait();
								auto channel = state->wsa_channel;
								state->sync->Open();
								ErrorContext local;
								ClearXEError(local);
								if (channel) channel->Send(send, &_error, &_last_data_request, this, local); else SetXEError(local, 0x8, 0x8);
								if (local.error_code) Seal(local);
							}
						}
					} else if (_state == 1) {
						if (_error.error_code) {
							Seal();
						} else if (_header->Length() != 5) {
							if (_header->Length()) SetXEError(_error, 0x8, 0xB);
							Seal();
						} else {
							_last_data_request = (int(_header->ElementAt(3)) << 8) | int(_header->ElementAt(4));
							_state = 2;
							state->sync->Wait();
							auto channel = state->wsa_channel;
							state->sync->Open();
							ErrorContext local;
							ClearXEError(local);
							if (channel) channel->Receive(_last_data_request, &_error, &_data, this, local); else SetXEError(local, 0x8, 0x8);
							if (local.error_code) Seal(local);
						}
					} else if (_state == 2) {
						if (_error.error_code) {
							Seal();
						} else if (_data->Length() != _last_data_request) {
							SetXEError(_error, 0x8, 0xB);
							Seal();
						} else {
							try {
								_header->SetLength(5 + _last_data_request);
								MemoryCopy(_header->GetBuffer() + 5, _data->GetBuffer(), _last_data_request);
								_data.SetReference(0);
							} catch (...) { SetXEError(_error, 2, 0); Seal(); return; }
							ULONG attributes;
							SecBufferDesc input, output;
							SecBuffer buffers[4];
							ZeroMemory(&buffers, sizeof(buffers));
							input.ulVersion = SECBUFFER_VERSION;
							input.cBuffers = 2;
							input.pBuffers = buffers;
							output.ulVersion = SECBUFFER_VERSION;
							output.cBuffers = 2;
							output.pBuffers = buffers + 2;
							buffers[0].BufferType = SECBUFFER_TOKEN;
							buffers[0].pvBuffer = _header->GetBuffer();
							buffers[0].cbBuffer = _header->Length();
							buffers[3].BufferType = SECBUFFER_ALERT;
							auto status = InitializeSecurityContextW(state->credentials, &state->security->security, sec_check->GetDomainName(),
								ISC_REQ_CONFIDENTIALITY | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_MANUAL_CRED_VALIDATION,
								0, 0, &input, 0, &state->security->security, &output, &attributes, 0);
							_header = PrepareSendRecords(buffers + 2, 2, _error);
							for (int i = 2; i < 4; i++) if (buffers[i].BufferType) FreeContextBuffer(buffers[i].pvBuffer);
							if (_error.error_code) { Seal(); return; }
							if (status == SEC_E_OK) {
								_state = 4;
								if (!_header) VerifySecurity();
							} else if (status == SEC_I_CONTINUE_NEEDED) {
								if (_header) _state = 3;
								else InitiateReceivePackage();
							} else {
								_state = 5;
								if (!_header) { SetXEError(_error, 0x8, 0xB); Seal(); }
							}
							if (_header) {
								state->sync->Wait();
								auto channel = state->wsa_channel;
								state->sync->Open();
								ErrorContext local;
								ClearXEError(local);
								if (channel) channel->Send(_header, &_error, &_last_data_request, this, local); else SetXEError(local, 0x8, 0x8);
								if (local.error_code) Seal(local);
							}
						}
					} else {
						if (_error.error_code) {
							Seal();
						} else if (_header->Length() != _last_data_request) {
							SetXEError(_error, 0x8, 0x8);
							Seal();
						} else {
							if (_state == 3) {
								InitiateReceivePackage();
							} else if (_state == 4) {
								VerifySecurity();
							} else if (_state == 5) {
								SetXEError(_error, 0x8, 0xB);
								Seal();
							}
						}
					}
				}
			};
			class Sender : public IDispatchTask
			{
				int _length, _length_sent, _pure_length;
				ErrorContext _error;
				ErrorContext * _result_error;
				int * _result_sent;
				SafePointer<IDispatchTask> _result_handler;
			public:
				Sender(int pure, ErrorContext * error, int * sent, IDispatchTask * hdlr) : _pure_length(pure), _result_error(error), _result_sent(sent) { _result_handler.SetRetain(hdlr); }
				virtual ~Sender(void) override {}
				virtual void DoTask(IDispatchQueue * queue) noexcept override
				{
					if (_error.error_code) {
						if (_result_error) *_result_error = _error;
						if (_result_sent) *_result_sent = 0;
						if (_result_handler) _result_handler->DoTask(0);
					} else {
						if (_length == _length_sent) {
							if (_result_error) ClearXEError(*_result_error);
							if (_result_sent) *_result_sent = _pure_length;
							if (_result_handler) _result_handler->DoTask(0);
						} else {
							if (_result_error) SetXEError(*_result_error, 0x8, 0x8);
							if (_result_sent) *_result_sent = 0;
							if (_result_handler) _result_handler->DoTask(0);
						}
					}
				}
				void Perform(INetworkChannel * channel, DataBlock * crypt_data, ErrorContext & ectx) noexcept
				{
					_length = crypt_data->Length();
					channel->Send(crypt_data, &_error, &_length_sent, this, ectx);
				}
			};
			class Receptor : public IDispatchTask
			{
				uint _internal_state; // 0 - reading header, 1 - reading body
				uint _data_requested;
				ErrorContext _error;
				SafePointer<DetachedState> _state;
				SafePointer<DataBlock> _header, _data;
			private:
				void ProcessEndOfStream(void) noexcept
				{
					while (true) {
						int control_word; // 0 - exit, 2 - perform more
						SafePointer<IDispatchTask> perform;
						_state->sync->Wait();
						_state->end_of_stream = true;
						auto avail = _state->pending_data.Length() - _state->pending_data_position;
						auto & task = _state->receive_tasks.GetFirst()->GetValue();
						try {
							SafePointer<DataBlock> result = new DataBlock(1);
							if (task.length <= avail) {
								result->SetLength(task.length);
								MemoryCopy(result->GetBuffer(), _state->pending_data.GetBuffer() + _state->pending_data_position, task.length);
								_state->pending_data_position += task.length;
								if (task.length == avail) {
									_state->pending_data.SetLength(0);
									_state->pending_data_position = 0;
								}
							} else {
								result->SetLength(avail);
								MemoryCopy(result->GetBuffer(), _state->pending_data.GetBuffer() + _state->pending_data_position, avail);
								_state->pending_data.SetLength(0);
								_state->pending_data_position = 0;
							}
							if (task.error) ClearXEError(*task.error);
							if (task.data) *task.data = result;
							perform = task.handler;
							_state->receive_tasks.RemoveFirst();
							control_word = _state->receive_tasks.IsEmpty() ? 0 : 2;
						} catch (...) {
							_state->sync->Open();
							ProcessTransportError(2, 0);
							return;
						}
						_state->sync->Open();
						if (perform) perform->DoTask(0);
						if (control_word == 0) {
							break;
						} else if (control_word == 2) {
							continue;
						}
					}
				}
				void ProcessTransportError(uintptr error, uintptr suberror) noexcept
				{
					while (true) {
						int control_word; // 0 - exit, 2 - perform more
						SafePointer<IDispatchTask> perform;
						_state->sync->Wait();
						_state->end_of_stream = true;
						_state->pending_data.SetLength(0);
						_state->pending_data_position = 0;
						auto & task = _state->receive_tasks.GetFirst()->GetValue();
						if (task.error) SetXEError(*task.error, error, suberror);
						if (task.data) *task.data = 0;
						perform = task.handler;
						_state->receive_tasks.RemoveFirst();
						control_word = _state->receive_tasks.IsEmpty() ? 0 : 2;
						_state->sync->Open();
						if (perform) perform->DoTask(0);
						if (control_word == 0) {
							break;
						} else if (control_word == 2) {
							continue;
						}
					}
				}
				void ProcessDataInput(const void * data, int length) noexcept
				{
					while (true) {
						int control_word; // 0 - exit, 1 - read more, 2 - perform more
						SafePointer<IDispatchTask> perform;
						_state->sync->Wait();
						auto avail = _state->pending_data.Length() - _state->pending_data_position;
						auto & task = _state->receive_tasks.GetFirst()->GetValue();
						if (task.length <= avail + length) {
							try {
								SafePointer<DataBlock> result = new DataBlock(1);
								result->SetLength(task.length);
								if (task.length <= avail) {
									MemoryCopy(result->GetBuffer(), _state->pending_data.GetBuffer() + _state->pending_data_position, task.length);
									_state->pending_data_position += task.length;
									if (task.length == avail) {
										_state->pending_data.SetLength(0);
										_state->pending_data_position = 0;
									}
								} else {
									MemoryCopy(result->GetBuffer(), _state->pending_data.GetBuffer() + _state->pending_data_position, avail);
									MemoryCopy(result->GetBuffer() + avail, data, task.length - avail);
									_state->pending_data.SetLength(0);
									_state->pending_data.Append(reinterpret_cast<const uint8 *>(data) + task.length - avail, length - task.length + avail);
									_state->pending_data_position = 0;
									length = 0;
								}
								if (task.error) ClearXEError(*task.error);
								if (task.data) *task.data = result;
								perform = task.handler;
								_state->receive_tasks.RemoveFirst();
								control_word = _state->receive_tasks.IsEmpty() ? 0 : 2;
							} catch (...) {
								_state->sync->Open();
								ProcessTransportError(2, 0);
								return;
							}
						} else {
							try {
								_state->pending_data.Append(reinterpret_cast<const uint8 *>(data), length);
								control_word = 1;
							} catch (...) {
								_state->sync->Open();
								ProcessTransportError(2, 0);
								return;
							}
						}
						_state->sync->Open();
						if (perform) perform->DoTask(0);
						if (control_word == 0) {
							break;
						} else if (control_word == 1) {
							ErrorContext local;
							ClearXEError(local);
							Perform(local);
							if (local.error_code) ProcessTransportError(local.error_code, local.error_subcode);
							return;
						} else if (control_word == 2) {
							continue;
						}
					}
				}
			public:
				Receptor(DetachedState * state) { _state.SetRetain(state); }
				virtual ~Receptor(void) override {}
				virtual void DoTask(IDispatchQueue * queue) noexcept override
				{
					if (_internal_state == 0) {
						if (_error.error_code) {
							ProcessTransportError(_error.error_code, _error.error_subcode);
						} else if (!_header->Length()) {
							ProcessEndOfStream();
						} else if (_header->Length() != _data_requested) {
							ProcessTransportError(0x8, 0xB);
						} else {
							_data_requested = (int(_header->ElementAt(3)) << 8) | int(_header->ElementAt(4));
							_internal_state = 1;
							ErrorContext local;
							ClearXEError(local);
							SafePointer<INetworkChannel> channel;
							_state->sync->Wait();
							channel = _state->wsa_channel;
							_state->sync->Open();
							if (channel) channel->Receive(_data_requested, &_error, &_data, this, local);
							else ProcessTransportError(0x8, 0x8);
							if (local.error_code) ProcessTransportError(local.error_code, local.error_subcode);
						}
					} else if (_internal_state == 1) {
						if (_error.error_code) {
							ProcessTransportError(_error.error_code, _error.error_subcode);
						} else if (_data->Length() != _data_requested) {
							ProcessTransportError(0x8, 0xB);
						} else {
							try {
								_header->SetLength(5 + _data->Length());
								MemoryCopy(_header->GetBuffer() + 5, _data->GetBuffer(), _data->Length());
								_data.SetReference(0);
							} catch (...) {
								ProcessTransportError(2, 0);
								return;
							}
							SecBufferDesc buffer_desc;
							SecBuffer buffers[4];
							ZeroMemory(&buffers, sizeof(buffers));
							buffer_desc.ulVersion = SECBUFFER_VERSION;
							buffer_desc.cBuffers = 4;
							buffer_desc.pBuffers = buffers;
							buffers[0].cbBuffer = _header->Length();
							buffers[0].pvBuffer = _header->GetBuffer();
							buffers[0].BufferType = SECBUFFER_DATA;
							auto status = DecryptMessage(&_state->security->security, &buffer_desc, 0, 0);
							if (status != SEC_E_OK) {
								if (status == SEC_I_CONTEXT_EXPIRED || status == SEC_I_RENEGOTIATE) {
									ProcessEndOfStream();
								} else if (status == SEC_E_UNSUPPORTED_FUNCTION) {
									ErrorContext local;
									ClearXEError(local);
									Perform(local);
									if (local.error_code) ProcessTransportError(local.error_code, local.error_subcode);
								} else {
									ProcessTransportError(0x8, 0xB);
								}
							} else {
								for (uint i = 0; i < buffer_desc.cBuffers; i++) if (buffer_desc.pBuffers[i].BufferType == SECBUFFER_DATA) {
									ProcessDataInput(buffer_desc.pBuffers[i].pvBuffer, buffer_desc.pBuffers[i].cbBuffer);
									return;
								}
								ProcessTransportError(5, 0);
							}
						}
					}
				}
				void Perform(ErrorContext & ectx) noexcept
				{
					_state->sync->Wait();
					auto channel = _state->wsa_channel;
					_state->sync->Open();
					_internal_state = 0;
					_data_requested = 5;
					if (channel) channel->Receive(5, &_error, &_header, this, ectx); else SetXEError(ectx, 5, 0);
				}
			};
		private:
			SafePointer<DetachedState> _ext_state;
		public:
			SSPINetworkChannel(void)
			{
				CredHandle cred;
				SafePointer<INetworkChannel> channel = CreateNetworkChannel(0);
				_ext_state = new DetachedState(channel);
				SCHANNEL_CRED scc;
				ZeroMemory(&scc, sizeof(scc));
				scc.dwVersion = SCHANNEL_CRED_VERSION;
				scc.dwFlags = SCH_CRED_MANUAL_CRED_VALIDATION | SCH_USE_STRONG_CRYPTO;
				auto status = AcquireCredentialsHandleW(0, UNISP_NAME, SECPKG_CRED_OUTBOUND, 0, &scc, 0, 0, &cred, 0);
				if (status == SEC_E_OK) {
					try { _ext_state->client_store = new ClientCredentialsStore(cred); }
					catch (...) { FreeCredentialsHandle(&cred); throw OutOfMemoryException(); }
				} else throw InvalidStateException();
				_ext_state->credentials = &_ext_state->client_store->credentials;
				_ext_state->state = CHANNEL_STATE_INITIAL;
			}
			SSPINetworkChannel(INetworkChannel * accept, ServerCredentialsStore * store, SecurityContextStore * sec)
			{
				_ext_state = new DetachedState(accept);
				_ext_state->server_store.SetRetain(store);
				_ext_state->security.SetRetain(sec);
				_ext_state->credentials = &_ext_state->server_store->credentials;
				_ext_state->state = CHANNEL_STATE_READY;
				if (QueryContextAttributesW(&_ext_state->security->security, SECPKG_ATTR_STREAM_SIZES, &_ext_state->data_sizes) != SEC_E_OK) throw InvalidStateException();
			}
			virtual ~SSPINetworkChannel(void) override { _ext_state->Shutdown(); }
			virtual void ConnectA(NetworkAddress * address, ErrorContext * error, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override { SetXEError(ectx, 1, 0); }
			virtual void ConnectB(NetworkAddress * address, NetworkSecurityDescriptor & sec, ErrorContext * error, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (_ext_state->state != CHANNEL_STATE_INITIAL) { SetXEError(ectx, 5, 0); return; }
				XE_TRY_INTRO
				SafePointer<SecurityCheckStore> check = new SecurityCheckStore(sec);
				SafePointer<Connector> conn = new Connector;
				conn->state = _ext_state;
				conn->sec_check = check;
				conn->result_error = error;
				conn->handler.SetRetain(hdlr);
				_ext_state->state = CHANNEL_STATE_LAUNCHING;
				conn->Initiate(address);
				XE_TRY_OUTRO()
			}
			virtual void Send(DataBlock * data, ErrorContext * error, int * sent, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (_ext_state->state != CHANNEL_STATE_READY) { SetXEError(ectx, 5, 0); return; }
				if (!data || !data->Length()) {
					if (error) ClearXEError(*error);
					if (sent) *sent = 0;
					if (hdlr) hdlr->DoTask(0);
					return;
				}
				XE_TRY_INTRO
				SafePointer<DataBlock> crypt = new DataBlock(1);
				int position = 0;
				while (position < data->Length()) {
					int encrypt = min(data->Length() - position, int(_ext_state->data_sizes.cbMaximumMessage));
					DataBlock im(encrypt + _ext_state->data_sizes.cbHeader + _ext_state->data_sizes.cbTrailer);
					im.SetLength(_ext_state->data_sizes.cbHeader);
					im.Append(data->GetBuffer() + position, encrypt);
					im.SetLength(encrypt + _ext_state->data_sizes.cbHeader + _ext_state->data_sizes.cbTrailer);
					SecBufferDesc desc;
					SecBuffer buffers[4];
					ZeroMemory(&buffers, sizeof(buffers));
					desc.ulVersion = SECBUFFER_VERSION;
					desc.cBuffers = 4;
					desc.pBuffers = buffers;
					buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
					buffers[0].cbBuffer = _ext_state->data_sizes.cbHeader;
					buffers[0].pvBuffer = im.GetBuffer();
					buffers[1].BufferType = SECBUFFER_DATA;
					buffers[1].cbBuffer = encrypt;
					buffers[1].pvBuffer = im.GetBuffer() + _ext_state->data_sizes.cbHeader;
					buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
					buffers[2].cbBuffer = _ext_state->data_sizes.cbTrailer;
					buffers[2].pvBuffer = im.GetBuffer() + _ext_state->data_sizes.cbHeader + encrypt;
					auto status = EncryptMessage(&_ext_state->security->security, 0, &desc, 0);
					if (status != SEC_E_OK) {
						if (error) SetXEError(*error, 0x8, 0xB);
						if (sent) *sent = 0;
						if (hdlr) hdlr->DoTask(0);
						return;
					}
					im.SetLength(buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer);
					crypt->Append(im);
					position += encrypt;
				}
				SafePointer<Sender> sender = new Sender(data->Length(), error, sent, hdlr);
				sender->Perform(_ext_state->wsa_channel, crypt, ectx);
				XE_TRY_OUTRO()
			}
			virtual void Receive(int length, ErrorContext * error, SafePointer<DataBlock> * data, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (_ext_state->state != CHANNEL_STATE_READY) { SetXEError(ectx, 5, 0); return; }
				if (length < 0) { SetXEError(ectx, 3, 0); return; }
				XE_TRY_INTRO
				if (!length) {
					SafePointer<DataBlock> responce = new DataBlock(1);
					if (error) ClearXEError(*error);
					if (data) *data = responce;
					if (hdlr) hdlr->DoTask(0);
					return;
				}
				ReceiveTask task;
				task.length = length;
				task.error = error;
				task.data = data;
				task.handler.SetRetain(hdlr);
				_ext_state->sync->Wait();
				bool run_receptor = false;
				bool run_handler = false;
				auto data_ready = _ext_state->pending_data.Length() - _ext_state->pending_data_position;
				if (data_ready >= length && _ext_state->receive_tasks.IsEmpty()) {
					run_handler = true;
					try {
						SafePointer<DataBlock> result = new DataBlock(1);
						result->SetLength(length);
						MemoryCopy(result->GetBuffer(), _ext_state->pending_data.GetBuffer() + _ext_state->pending_data_position, length);
						_ext_state->pending_data_position += length;
						if (data_ready == length) {
							_ext_state->pending_data.SetLength(0);
							_ext_state->pending_data_position = 0;
						}
						if (task.error) ClearXEError(*task.error);
						if (task.data) *task.data = result;
					} catch (...) {
						if (task.error) SetXEError(*task.error, 2, 0);
						if (task.data) *task.data = 0;
					}
				} else if (_ext_state->end_of_stream && _ext_state->receive_tasks.IsEmpty()) {
					run_handler = true;
					try {
						SafePointer<DataBlock> result = new DataBlock(1);
						result->SetLength(data_ready);
						MemoryCopy(result->GetBuffer(), _ext_state->pending_data.GetBuffer() + _ext_state->pending_data_position, data_ready);
						_ext_state->pending_data.SetLength(0);
						_ext_state->pending_data_position = 0;
						if (task.error) ClearXEError(*task.error);
						if (task.data) *task.data = result;
					} catch (...) {
						if (task.error) SetXEError(*task.error, 2, 0);
						if (task.data) *task.data = 0;
					}
				} else {
					try {
						run_receptor = _ext_state->receive_tasks.IsEmpty();
						_ext_state->receive_tasks.Push(task);
					} catch (...) { _ext_state->sync->Open(); throw; }
				}
				_ext_state->sync->Open();
				if (run_receptor) {
					SafePointer<Receptor> req = new Receptor(_ext_state);
					req->Perform(ectx);
				}
				if (run_handler && task.handler) task.handler->DoTask(0);
				XE_TRY_OUTRO()
			}
			virtual void Close(bool ultimately, ErrorContext & ectx) noexcept override
			{
				if (_ext_state->state != CHANNEL_STATE_READY) { SetXEError(ectx, 5, 0); return; }
				XE_TRY_INTRO
				SafePointer<DataBlock> record = new DataBlock(1);
				record->SetLength(2 + _ext_state->data_sizes.cbHeader + _ext_state->data_sizes.cbTrailer);
				(*record)[_ext_state->data_sizes.cbHeader + 0] = 1;
				(*record)[_ext_state->data_sizes.cbHeader + 1] = 0;
				SecBufferDesc desc;
				SecBuffer buffers[4];
				ZeroMemory(&buffers, sizeof(buffers));
				desc.ulVersion = SECBUFFER_VERSION;
				desc.cBuffers = 4;
				desc.pBuffers = buffers;
				buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
				buffers[0].cbBuffer = _ext_state->data_sizes.cbHeader;
				buffers[0].pvBuffer = record->GetBuffer();
				buffers[1].BufferType = SECBUFFER_DATA;
				buffers[1].cbBuffer = 2;
				buffers[1].pvBuffer = record->GetBuffer() + _ext_state->data_sizes.cbHeader;
				buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
				buffers[2].cbBuffer = _ext_state->data_sizes.cbTrailer;
				buffers[2].pvBuffer = record->GetBuffer() + _ext_state->data_sizes.cbHeader + 2;
				auto status = EncryptMessage(&_ext_state->security->security, SECQOP_WRAP_OOB_DATA, &desc, 0);
				if (status == SEC_E_OK) {
					record->SetLength(buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer);
					_ext_state->wsa_channel->Send(record, 0, 0, CreateFunctionalTask([st = _ext_state, ult = ultimately]() {
						st->sync->Wait();
						auto cn = st->wsa_channel;
						st->sync->Open();
						ErrorContext local;
						ClearXEError(local);
						if (cn) cn->Close(ult, local);
					}), ectx);
				} else _ext_state->wsa_channel->Close(ultimately, ectx);
				XE_TRY_OUTRO();
			}
		};
		class SSPINetworkListener : public INetworkListener
		{
			class Initiator : public IDispatchTask
			{
				int _last_data_request;
				uint _state; // 0 - receiving header, 1 - receiving body, 2 - sending then continue, 3 - sending then success, 4 - sending then fail
				ErrorContext _error;
				SafePointer<SecurityContextStore> _security;
				SafePointer<DataBlock> _data, _header;
			public:
				SafePointer<ServerCredentialsStore> store;
				SafePointer<INetworkChannel> raw_channel;
				SafePointer<NetworkAddress> raw_address;
			public:
				ErrorContext * result_error;
				SafePointer<INetworkChannel> * result_channel;
				SafePointer<NetworkAddress> * result_address;
				SafePointer<IDispatchTask> handler;
			private:
				void Seal(void) noexcept
				{
					if (_error.error_code) {
						if (result_error) *result_error = _error;
						if (result_channel) *result_channel = 0;
						if (result_address) *result_address = 0;
						if (handler) handler->DoTask(0);
					} else {
						SafePointer<INetworkChannel> result;
						try { result = new SSPINetworkChannel(raw_channel, store, _security); } catch (...) {
							if (result_error) SetXEError(*result_error, 2, 0);
							if (result_channel) *result_channel = 0;
							if (result_address) *result_address = 0;
							if (handler) handler->DoTask(0);
							return;
						}
						if (result_error) ClearXEError(*result_error);
						if (result_channel) *result_channel = result;
						if (result_address) *result_address = raw_address;
						if (handler) handler->DoTask(0);
					}
				}
				void Seal(const ErrorContext & ectx) noexcept { _error = ectx; Seal(); }
				void InitiateReceivePackage(void) noexcept
				{
					_state = 0;
					ErrorContext local;
					ClearXEError(local);
					raw_channel->Receive(5, &_error, &_header, this, local);
					if (local.error_code) Seal(local);
				}
			public:
				Initiator(void) : _state(0) { ClearXEError(_error); }
				virtual ~Initiator(void) override {}
				void Initiate(void) noexcept { InitiateReceivePackage(); }
				virtual void DoTask(IDispatchQueue * queue) noexcept override
				{
					if (_state == 0) {
						if (_error.error_code) {
							Seal();
						} else if (_header->Length() != 5) {
							if (_header->Length()) SetXEError(_error, 0x8, 0xB);
							Seal();
						} else {
							_last_data_request = (int(_header->ElementAt(3)) << 8) | int(_header->ElementAt(4));
							_state = 1;
							ErrorContext local;
							ClearXEError(local);
							raw_channel->Receive(_last_data_request, &_error, &_data, this, local);
							if (local.error_code) Seal(local);
						}
					} else if (_state == 1) {
						if (_error.error_code) {
							Seal();
						} else if (_data->Length() != _last_data_request) {
							SetXEError(_error, 0x8, 0xB);
							Seal();
						} else {
							try {
								_header->SetLength(5 + _last_data_request);
								MemoryCopy(_header->GetBuffer() + 5, _data->GetBuffer(), _last_data_request);
								_data.SetReference(0);
							} catch (...) { SetXEError(_error, 2, 0); Seal(); return; }
							SECURITY_STATUS status;
							ULONG attributes;
							SecBufferDesc input, output;
							SecBuffer buffers[4];
							ZeroMemory(&buffers, sizeof(buffers));
							input.ulVersion = SECBUFFER_VERSION;
							input.cBuffers = 2;
							input.pBuffers = buffers;
							output.ulVersion = SECBUFFER_VERSION;
							output.cBuffers = 2;
							output.pBuffers = buffers + 2;
							buffers[0].BufferType = SECBUFFER_TOKEN;
							buffers[0].pvBuffer = _header->GetBuffer();
							buffers[0].cbBuffer = _header->Length();
							buffers[3].BufferType = SECBUFFER_ALERT;
							if (_security) {
								status = AcceptSecurityContext(&store->credentials, &_security->security, &input, ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_CONFIDENTIALITY, 0, &_security->security, &output, &attributes, 0);
							} else {
								CtxtHandle new_sec;
								new_sec.dwLower = new_sec.dwUpper = 0;
								status = AcceptSecurityContext(&store->credentials, 0, &input, ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_CONFIDENTIALITY, 0, &new_sec, &output, &attributes, 0);
								if (new_sec.dwLower || new_sec.dwUpper) try { _security = new SecurityContextStore(new_sec); }
								catch (...) { DeleteSecurityContext(&new_sec); SetXEError(_error, 2, 0); Seal(); return; }
							}
							try {
								int output_length = 0;
								for (int i = 2; i < 4; i++) if (buffers[i].BufferType) output_length += buffers[i].cbBuffer;
								if (output_length) {
									_header->SetLength(output_length);
									output_length = 0;
									for (int i = 2; i < 4; i++) if (buffers[i].BufferType) {
										MemoryCopy(_header->GetBuffer() + output_length, buffers[i].pvBuffer, buffers[i].cbBuffer);
										output_length += buffers[i].cbBuffer;
									}
								} else _header.SetReference(0);
								for (int i = 2; i < 4; i++) if (buffers[i].BufferType) FreeContextBuffer(buffers[i].pvBuffer);
							} catch (...) {
								for (int i = 2; i < 4; i++) if (buffers[i].BufferType) FreeContextBuffer(buffers[i].pvBuffer);
								SetXEError(_error, 2, 0); Seal(); return;
							}
							if (status == SEC_E_OK) {
								_state = 3;
								if (!_header) { ClearXEError(_error); Seal(); }
							} else if (status == SEC_I_CONTINUE_NEEDED) {
								if (_header) _state = 2;
								else InitiateReceivePackage();
							} else {
								_state = 4;
								if (!_header) { SetXEError(_error, 0x8, 0xB); Seal(); }
							}
							if (_header) {
								ErrorContext local;
								ClearXEError(local);
								raw_channel->Send(_header, &_error, &_last_data_request, this, local);
								if (local.error_code) { Seal(local); }
							}
						}
					} else {
						if (_error.error_code) {
							Seal();
						} else if (_header->Length() != _last_data_request) {
							SetXEError(_error, 0x8, 0x8);
							Seal();
						} else {
							_header.SetReference(0);
							if (_state == 2) {
								InitiateReceivePackage();
							} else if (_state == 3) {
								ClearXEError(_error);
								Seal();
							} else if (_state == 4) {
								SetXEError(_error, 0x8, 0xB);
								Seal();
							}
						}
					}
				}
			};
			class Acceptor : public IDispatchTask
			{
			public:
				ErrorContext error;
				SafePointer<ServerCredentialsStore> store;
				SafePointer<INetworkChannel> raw_channel;
				SafePointer<NetworkAddress> raw_address;
			public:
				ErrorContext * result_error;
				SafePointer<INetworkChannel> * result_channel;
				SafePointer<NetworkAddress> * result_address;
				SafePointer<IDispatchTask> handler;
			public:
				Acceptor(void) { ClearXEError(error); }
				virtual ~Acceptor(void) override {}
				virtual void DoTask(IDispatchQueue * queue) noexcept override
				{
					if (error.error_code) {
						raw_channel = 0;
						raw_address = 0;
						if (result_error) *result_error = error;
						if (result_channel) *result_channel = 0;
						if (result_address) *result_address = 0;
						if (handler) handler->DoTask(queue);
					} else {
						SafePointer<Initiator> init;
						try { init = new Initiator; } catch (...) {
							raw_channel = 0;
							raw_address = 0;
							if (result_error) SetXEError(*result_error, 2, 0);
							if (result_channel) *result_channel = 0;
							if (result_address) *result_address = 0;
							if (handler) handler->DoTask(queue);
							return;
						}
						init->store = store;
						init->raw_channel = raw_channel;
						init->raw_address = raw_address;
						init->result_error = result_error;
						init->result_channel = result_channel;
						init->result_address = result_address;
						init->handler = handler;
						init->Initiate();
						raw_channel = 0;
						raw_address = 0;
					}
				}
			};
		private:
			uint _state;
			SafePointer<INetworkListener> _wsa_listener;
			SafePointer<ServerCredentialsStore> _store;
		public:
			SSPINetworkListener(NetworkAddressFactory & factory) : _state(LISTENER_STATE_UNBOUND) { _wsa_listener = CreateNetworkListener(0, factory); }
			virtual ~SSPINetworkListener(void) override {}
			virtual void BindA(NetworkAddress * address, ErrorContext & ectx) noexcept override { SetXEError(ectx, 1, 0); }
			virtual void BindB(NetworkAddress * address, NetworkIdentityDescriptor & idesc, ErrorContext & ectx) noexcept override
			{
				if (_state != LISTENER_STATE_UNBOUND) { SetXEError(ectx, 5, 0); return; }
				if (!idesc.Data) { SetXEError(ectx, 3, 0); return; }
				CRYPT_DATA_BLOB blob;
				blob.pbData = idesc.Data->GetBuffer();
				blob.cbData = idesc.Data->Length();
				HCERTSTORE store = PFXImportCertStore(&blob, idesc.Password, 0);
				if (!store) { SetXEError(ectx, 0x08, 0x0B); return; }
				auto cert = CertFindCertificateInStore(store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_HAS_PRIVATE_KEY, 0, 0);
				CertCloseStore(store, 0);
				if (!cert) { SetXEError(ectx, 3, 0); return; }
				SCHANNEL_CRED scc;
				ZeroMemory(&scc, sizeof(scc));
				scc.dwVersion = SCHANNEL_CRED_VERSION;
				scc.cCreds = 1;
				scc.paCred = &cert;
				CredHandle cred;
				auto status = AcquireCredentialsHandleW(0, UNISP_NAME, SECPKG_CRED_INBOUND, 0, &scc, 0, 0, &cred, 0);
				if (status == SEC_E_OK) {
					try {
						_store = new ServerCredentialsStore(cert, cred);
					} catch (...) {
						FreeCredentialsHandle(&cred);
						CertFreeCertificateContext(cert);
						SetXEError(ectx, 2, 0);
						return;
					}
				} else {
					CertFreeCertificateContext(cert);
					SetXEError(ectx, 3, 0);
					return;
				}
				_wsa_listener->BindA(address, ectx);
				if (ectx.error_code) { _store.SetReference(0); return; }
				_state = LISTENER_STATE_BOUND;
			}
			virtual void Accept(int limit, ErrorContext * error, SafePointer<INetworkChannel> * channel, SafePointer<NetworkAddress> * address, IDispatchTask * hdlr, ErrorContext & ectx) noexcept override
			{
				if (_state != LISTENER_STATE_BOUND) { SetXEError(ectx, 5, 0); return; }
				XE_TRY_INTRO
				SafePointer<Acceptor> acceptor = new Acceptor;
				acceptor->store = _store;
				acceptor->result_error = error;
				acceptor->result_channel = channel;
				acceptor->result_address = address;
				acceptor->handler.SetRetain(hdlr);
				_wsa_listener->Accept(limit, &acceptor->error, &acceptor->raw_channel, &acceptor->raw_address, acceptor, ectx);
				XE_TRY_OUTRO()
			}
			virtual void Close(ErrorContext & ectx) noexcept override { _wsa_listener->Close(ectx); }
		};

		SafePointer<INetworkChannel> CreateNetworkChannelS(NetworkAddress * address) { return new SSPINetworkChannel; }
		SafePointer<INetworkListener> CreateNetworkListenerS(NetworkAddress * address, NetworkAddressFactory & factory) { return new SSPINetworkListener(factory); }
	}
}
#endif