#include "xe_sec_ext.h"
#include "xe_sec_arithm.h"
#include "xe_sec_rsa.h"
#include "xe_sec_xaa.h"
#include "xe_sec_quarantine.h"
#include "../xenv/xe_interfaces.h"

namespace Engine
{
	namespace XE
	{
		namespace Security
		{
			struct ECertificateDesc
			{
				string organization;
				string person;
				uint authorities;
				uint inheritable_authorities;
				Time valid_since;
				Time valid_until;
				bool root;
			};
			class EKey : public Object
			{
				SafePointer<IKey> _key;
			public:
				EKey(IKey * key) { _key.SetRetain(key); }
				virtual ~EKey(void) override {}
				virtual int GetType(void) noexcept { return int(_key->GetKeyClass()); }
				virtual SafePointer<EKey> GetPublicKey(void) noexcept
				{
					SafePointer<IKey> key = _key->ExtractPublicKey();
					if (!key) return 0;
					return new (std::nothrow) EKey(key);
				}
				virtual SafePointer<DataBlock> Encrypt(DataBlock * in) noexcept { return _key->Encrypt(in); }
				virtual SafePointer<DataBlock> Decrypt(DataBlock * in) noexcept { return _key->Decrypt(in); }
				virtual SafePointer<DataBlock> Sign(DataBlock * hash) noexcept { return _key->Sign(hash); }
				virtual bool Validate(DataBlock * hash, DataBlock * signature) noexcept { return _key->Validate(hash, signature); }
				IKey * GetObject(void) noexcept { return _key; }
			};
			class ECertificate : public Object
			{
				SafePointer<ICertificate> _cert;
			public:
				ECertificate(ICertificate * cert) { _cert.SetRetain(cert); }
				virtual ~ECertificate(void) override {}
				virtual string GetOrganization(void) noexcept { try { return _cert->GetDescription().Organization; } catch (...) { return L""; } }
				virtual string GetPerson(void) noexcept { try { return _cert->GetDescription().PersonName; } catch (...) { return L""; } }
				virtual uint GetAuthorities(void) noexcept { return _cert->GetDescription().CertificateUsage; }
				virtual uint GetInheritance(void) noexcept { return _cert->GetDescription().CertificateDerivation; }
				virtual bool IsRoot(void) noexcept { return _cert->GetDescription().IsRootCertificate; }
				virtual uint64 GetValidSince(void) noexcept { return _cert->GetDescription().ValidSince.Ticks; }
				virtual uint64 GetValidUntil(void) noexcept { return _cert->GetDescription().ValidUntil.Ticks; }
				virtual SafePointer<Object> GetAttributes(void) noexcept
				{
					try {
						SafePointer< Volumes::Dictionary<string, string> > dict = new Volumes::Dictionary<string, string>(_cert->GetDescription().Attributes);
						return CreateMetadataDictionary(dict);
					} catch (...) { return 0; }
				}
				virtual SafePointer<EKey> GetPublicKey(void) noexcept
				{
					SafePointer<IKey> key = _cert->LoadPublicKey();
					if (!key) return 0;
					return new (std::nothrow) EKey(key);
				}
				ICertificate * GetObject(void) noexcept { return _cert; }
			};
			class EIdentity : public Object
			{
				SafePointer<IIdentity> _ident;
			public:
				EIdentity(IIdentity * ident) { _ident.SetRetain(ident); }
				virtual ~EIdentity(void) override {}
				virtual int GetCertificateNumber(void) noexcept { return _ident->GetCertificateChainLength(); }
				virtual SafePointer<ECertificate> GetCertificate(int index) noexcept
				{
					SafePointer<ICertificate> cert = _ident->LoadCertificate(index);
					if (!cert) return 0;
					return new (std::nothrow) ECertificate(cert);
				}
				virtual SafePointer<EKey> GetPrivateKey(void) noexcept
				{
					SafePointer<IKey> key = _ident->LoadPrivateKey();
					if (!key) return 0;
					return new (std::nothrow) EKey(key);
				}
				IIdentity * GetObject(void) noexcept { return _ident; }
			};
			class EData : public Object
			{
				SafePointer<IContainer> _data;
			public:
				EData(IContainer * data) { _data.SetRetain(data); }
				virtual ~EData(void) override {}
				virtual int GetType(void) noexcept { return int(_data->GetContainerClass()); }
				virtual int GetCertificateNumber(void) noexcept { return _data->GetCertificateChainLength(); }
				virtual SafePointer<ECertificate> GetCertificate(int index) noexcept
				{
					SafePointer<ICertificate> cert = _data->LoadCertificate(index);
					if (!cert) return 0;
					return new (std::nothrow) ECertificate(cert);
				}
				virtual SafePointer<EKey> GetPrivateKey(const string & password) noexcept
				{
					SafePointer<IKey> key = _data->LoadPrivateKey(password);
					if (!key) return 0;
					return new (std::nothrow) EKey(key);
				}
				virtual SafePointer<DataBlock> GetHash(void) noexcept { return _data->LoadDocumentHash(); }
				virtual SafePointer<DataBlock> GetSignature(void) noexcept { return _data->LoadDocumentSignature(); }
				virtual SafePointer<DataBlock> GetRepresentation(void) noexcept { return _data->LoadContainerRepresentation(); }
				IContainer * GetObject(void) noexcept { return _data; }
			};
			class ETrust : public Object
			{
				SafePointer<ITrustProvider> _trust;
			public:
				ETrust(ITrustProvider * trust) { _trust.SetRetain(trust); }
				virtual ~ETrust(void) override {}
				virtual bool AddTrust(ECertificate * cert, bool trusted) noexcept { return _trust->AddTrust(cert->GetObject(), trusted); }
				virtual bool AddTrust(XStream * xstream, bool trusted) noexcept
				{
					try {
						SafePointer<Streaming::Stream> stream = WrapFromXStream(xstream);
						return _trust->AddTrust(stream, trusted);
					} catch (...) { return false; }
				}
				virtual int GetTrust(const DataBlock * hash) noexcept { return int(_trust->ProvideTrust(hash)); }
				virtual bool GetCacheControl(void) noexcept { return _trust->GetCacheControl(); }
				virtual void SetCacheControl(const bool & value) noexcept { _trust->SetCacheControl(value); }
				virtual void ResetCache(void) noexcept { _trust->ResetTrustCache(); }
				ITrustProvider * GetObject(void) noexcept { return _trust; }
			};

			class SecurityExtension : public ISecurityExtension
			{
				SafePointer<ICryptographyAcceleration> _accel;
				SafePointer<ITrustProvider> _trust;
				SecurityLevel _security_level;
			public:
				SecurityExtension(ITrustProvider * trust, SecurityLevel security_level, ICryptographyAcceleration * accel) : _security_level(security_level) { _trust.SetRetain(trust); _accel.SetRetain(accel); }
				virtual ~SecurityExtension(void) override {}
				virtual ModuleLoadError EvaluateTrust(const void * module_data, int module_length, uintptr module_stream_context, Streaming::Stream * residual) noexcept override
				{
					bool needs_trusted_chain = _security_level == SecurityLevel::ValidateAll || (_security_level == SecurityLevel::ValidateQuarantine && module_stream_context);
					SafePointer<IContainer> signature = LoadContainer(residual, _accel);
					if (!signature) {
						if (needs_trusted_chain) return ModuleLoadError::ModuleTrustCompromised;
						else return ModuleLoadError::Success;
					}
					SafePointer<DataBlock> hash;
					try { hash = Cryptography::CreateHash(Cryptography::HashAlgorithm::SHA512, module_data, module_length); } catch (...) { return ModuleLoadError::AllocationFailure; }
					if (!hash) return ModuleLoadError::AllocationFailure;
					IntegrityValidationDesc desc;
					auto trust = EvaluateIntegrity(hash, signature, Time::GetCurrentTime(), _trust, &desc);
					if (trust == TrustStatus::Untrusted || (trust == TrustStatus::Undefined && needs_trusted_chain)) {
						if (desc.object == IntegrityStatus::DataCorruption) return ModuleLoadError::ModuleConsistencyCompromised;
						else return ModuleLoadError::ModuleTrustCompromised;
					} else return ModuleLoadError::Success;
				}
				virtual uintptr EvaluateTrustContext(const string & file) noexcept override
				{
					try {
						if (_security_level == SecurityLevel::ValidateQuarantine) return IsFileOnQuarantine(file) ? 1 : 0;
						else return 0;
					} catch (...) { return 1; }
				}
			};
			class SecurityAPI : public IAPIExtension
			{
				SafePointer<ICryptographyAcceleration> _accel;
				SafePointer<ITrustProvider> _trust;
			private:
				static SafePointer<EKey> _rsa_load(const DataBlock * data, const Module & mdl) noexcept
				{
					auto self = static_cast<SecurityAPI *>(reinterpret_cast<IAPIExtension *>(mdl.GetExecutionContext().GetLoaderCallback()->ExposeInterface(L"xesec")));
					SafePointer<IKey> key = LoadKeyRSA(data, self->_accel);
					if (!key) return 0;
					return new (std::nothrow) EKey(key);
				}
				static SafePointer<EKey> _rsa_create(int modulus_length, int exponent, const Module & mdl) noexcept
				{
					auto self = static_cast<SecurityAPI *>(reinterpret_cast<IAPIExtension *>(mdl.GetExecutionContext().GetLoaderCallback()->ExposeInterface(L"xesec")));
					try {
						int prime_length = modulus_length / 2;
						LargeInteger p(prime_length);
						LargeInteger q(prime_length);
						if (!p.InitWithRandomPrime(prime_length, 128, 2000)) return 0;
						if (!q.InitWithRandomPrime(prime_length, 128, 2000)) return 0;
						SafePointer<IKey> key;
						if (!CreateKeyRSA(p.Data(), p.DWordLength() * 4, q.Data(), q.DWordLength() * 4, exponent, key.InnerRef(), self->_accel)) return 0;
						return new (std::nothrow) EKey(key);
					} catch (...) { return 0; }
				}
				static SafePointer<EData> _data_load(XStream * xstream, const Module & mdl) noexcept
				{
					auto self = static_cast<SecurityAPI *>(reinterpret_cast<IAPIExtension *>(mdl.GetExecutionContext().GetLoaderCallback()->ExposeInterface(L"xesec")));
					try {
						SafePointer<Streaming::Stream> stream = WrapFromXStream(xstream);
						SafePointer<IContainer> data = LoadContainer(stream, self->_accel);
						if (!data) return 0;
						return new (std::nothrow) EData(data);
					} catch (...) { return 0; }
				}
				static SafePointer<EData> _data_create_cert(const ECertificateDesc & desc, const Array< Volumes::KeyValuePair<string, string> > & attr, EKey * pub) noexcept
				{
					try {
						CertificateDesc cd;
						cd.Organization = desc.organization;
						cd.PersonName = desc.person;
						cd.CertificateUsage = desc.authorities;
						cd.CertificateDerivation = desc.inheritable_authorities;
						cd.ValidSince = desc.valid_since;
						cd.ValidUntil = desc.valid_until;
						cd.IsRootCertificate = desc.root;
						for (auto & a : attr) cd.Attributes.Append(a.key, a.value);
						SafePointer<IContainer> data = CreateCertificate(cd, pub->GetObject());
						if (!data) return 0;
						return new (std::nothrow) EData(data);
					} catch (...) { return 0; }
				}
				static SafePointer<EData> _data_extract_cert(EData * source) noexcept
				{
					SafePointer<IContainer> data = CreateCertificateStorage(source->GetObject());
					if (!data) return 0;
					return new (std::nothrow) EData(data);
				}
				static SafePointer<EData> _data_subs_root_cert(ECertificate * cert, EKey * key) noexcept
				{
					SafePointer<IContainer> data = ValidateCertificate(cert->GetObject(), key->GetObject());
					if (!data) return 0;
					return new (std::nothrow) EData(data);
				}
				static SafePointer<EData> _data_subs_cert(ECertificate * cert, EIdentity * ident) noexcept
				{
					SafePointer<IContainer> data = ValidateCertificate(cert->GetObject(), ident->GetObject());
					if (!data) return 0;
					return new (std::nothrow) EData(data);
				}
				static SafePointer<EData> _data_create_key(EKey * key, const string & password) noexcept
				{
					SafePointer<IContainer> data = CreatePrivateKeyStorage(key->GetObject(), password);
					if (!data) return 0;
					return new (std::nothrow) EData(data);
				}
				static SafePointer<EData> _data_create_identity(EIdentity * ident, const string & password) noexcept
				{
					SafePointer<IContainer> data = CreateIdentityStorage(ident->GetObject(), password);
					if (!data) return 0;
					return new (std::nothrow) EData(data);
				}
				static SafePointer<EData> _data_create_hash(DataBlock * hash) noexcept
				{
					SafePointer<IContainer> data = CreateIntegrityData(hash);
					if (!data) return 0;
					return new (std::nothrow) EData(data);
				}
				static SafePointer<EData> _data_create_signature(DataBlock * hash, EIdentity * ident) noexcept
				{
					SafePointer<IContainer> data = CreateSignatureData(hash, ident->GetObject());
					if (!data) return 0;
					return new (std::nothrow) EData(data);
				}
				static SafePointer<EIdentity> _identity_alloc(EData * d0, EData * d1, const string & password) noexcept
				{
					SafePointer<IIdentity> ident = LoadIdentity(d0 ? d0->GetObject() : 0, d1 ? d1->GetObject() : 0, password);
					if (!ident) return 0;
					return new (std::nothrow) EIdentity(ident);
				}
				static SafePointer<ETrust> _trust_alloc(void) noexcept
				{
					SafePointer<ITrustProvider> trust = CreateTrustProvider();
					if (!trust) return 0;
					return new (std::nothrow) ETrust(trust);
				}
				static SafePointer<ETrust> _trust_system(const Module & mdl) noexcept
				{
					auto self = static_cast<SecurityAPI *>(reinterpret_cast<IAPIExtension *>(mdl.GetExecutionContext().GetLoaderCallback()->ExposeInterface(L"xesec")));
					if (self->_trust) return new (std::nothrow) ETrust(self->_trust);
					else return 0;
				}
				static int _evaluate_trust(DataBlock * hash, EData * data, const Time & time, ETrust * trust, Array<int> * status) noexcept
				{
					IntegrityValidationDesc vdesc;
					auto result = EvaluateIntegrity(hash, data->GetObject(), time, trust ? trust->GetObject() : 0, status ? &vdesc : 0);
					if (status) try {
						status->Clear();
						status->Append(int(vdesc.object));
						for (auto & v : vdesc.chain) status->Append(int(v));
					} catch (...) {}
					return int(result);
				}
			public:
				SecurityAPI(ICryptographyAcceleration * accel, ITrustProvider * system_trust) { _accel.SetRetain(accel); _trust.SetRetain(system_trust); }
				virtual const void * ExposeRoutine(const string & routine_name) noexcept override
				{
					if (string::Compare(routine_name, L"xesec_dscc") < 0) {
						if (string::Compare(routine_name, L"xesec_dcin") < 0) {
							if (string::Compare(routine_name, L"xesec_dccd") < 0) {
								if (string::Compare(routine_name, L"xesec_conv") == 0) return reinterpret_cast<const void *>(&_evaluate_trust);
							} else {
								if (string::Compare(routine_name, L"xesec_dccn") < 0) {
									if (string::Compare(routine_name, L"xesec_dccd") == 0) return reinterpret_cast<const void *>(&_data_extract_cert);
								} else {
									if (string::Compare(routine_name, L"xesec_dccn") == 0) return reinterpret_cast<const void *>(&_data_create_cert);
								}
							}
						} else {
							if (string::Compare(routine_name, L"xesec_ddig") < 0) {
								if (string::Compare(routine_name, L"xesec_dclp") < 0) {
									if (string::Compare(routine_name, L"xesec_dcin") == 0) return reinterpret_cast<const void *>(&_data_create_identity);
								} else {
									if (string::Compare(routine_name, L"xesec_dclp") == 0) return reinterpret_cast<const void *>(&_data_create_key);
								}
							} else {
								if (string::Compare(routine_name, L"xesec_done") < 0) {
									if (string::Compare(routine_name, L"xesec_ddig") == 0) return reinterpret_cast<const void *>(&_data_create_hash);
								} else {
									if (string::Compare(routine_name, L"xesec_done") == 0) return reinterpret_cast<const void *>(&_data_load);
								}
							}
						}
					} else {
						if (string::Compare(routine_name, L"xesec_ffss") < 0) {
							if (string::Compare(routine_name, L"xesec_dsub") < 0) {
								if (string::Compare(routine_name, L"xesec_dsci") < 0) {
									if (string::Compare(routine_name, L"xesec_dscc") == 0) return reinterpret_cast<const void *>(&_data_subs_root_cert);
								} else {
									if (string::Compare(routine_name, L"xesec_dsci") == 0) return reinterpret_cast<const void *>(&_data_subs_cert);
								}
							} else {
								if (string::Compare(routine_name, L"xesec_ffal") < 0) {
									if (string::Compare(routine_name, L"xesec_dsub") == 0) return reinterpret_cast<const void *>(&_data_create_signature);
								} else {
									if (string::Compare(routine_name, L"xesec_ffal") == 0) return reinterpret_cast<const void *>(&_trust_alloc);
								}
							}
						} else {
							if (string::Compare(routine_name, L"xesec_rsac") < 0) {
								if (string::Compare(routine_name, L"xesec_inda") < 0) {
									if (string::Compare(routine_name, L"xesec_ffss") == 0) return reinterpret_cast<const void *>(&_trust_system);
								} else {
									if (string::Compare(routine_name, L"xesec_inda") == 0) return reinterpret_cast<const void *>(&_identity_alloc);
								}
							} else {
								if (string::Compare(routine_name, L"xesec_rsao") < 0) {
									if (string::Compare(routine_name, L"xesec_rsac") == 0) return reinterpret_cast<const void *>(&_rsa_create);
								} else {
									if (string::Compare(routine_name, L"xesec_rsao") == 0) return reinterpret_cast<const void *>(&_rsa_load);
								}
							}
						}
					}
					return 0;					
				}
				virtual const void * ExposeInterface(const string & interface) noexcept override { if (interface == L"xesec") return static_cast<IAPIExtension *>(this); return 0; }
			};
			ISecurityExtension * CreateStandardSecurityExtension(ITrustProvider * trust, SecurityLevel security_level, ICryptographyAcceleration * acceleration) noexcept { try { return new SecurityExtension(trust, security_level, acceleration); } catch (...) { return 0; } }
			void ActivateSecurityAPI(StandardLoader & ldr, ICryptographyAcceleration * acceleration, ITrustProvider * system_trust)
			{
				SafePointer<IAPIExtension> ext = new SecurityAPI(acceleration, system_trust);
				if (!ldr.RegisterAPIExtension(ext)) throw InvalidStateException();
			}
		}
	}
}