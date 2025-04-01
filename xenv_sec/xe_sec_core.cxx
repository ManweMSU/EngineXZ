#include "xe_sec_core.h"
#include "xe_sec_rsa.h"

namespace Engine
{
	namespace XE
	{
		namespace Security
		{
			class XCertificate : public ICertificate
			{
				CertificateDesc _desc;
				SafePointer<DataBlock> _key_data;
				SafePointer<DataBlock> _data;
			public:
				XCertificate(DataBlock * data)
				{
					_data.SetRetain(data);
					Streaming::MemoryStream stream(data->GetBuffer(), data->Length());
					SafePointer<Storage::Registry> reg = Storage::LoadRegistry(&stream);
					if (!reg) throw InvalidFormatException();
					_desc.Organization = reg->GetValueString(L"Organizatio");
					_desc.PersonName = reg->GetValueString(L"Persona");
					_desc.CertificateUsage = reg->GetValueInteger(L"Usus");
					_desc.CertificateDerivation = reg->GetValueInteger(L"UsusDerivatus");
					_desc.IsRootCertificate = reg->GetValueBoolean(L"Radix");
					_desc.ValidSince = reg->GetValueTime(L"ValidusAb");
					_desc.ValidUntil = reg->GetValueTime(L"ValidusAd");
					SafePointer<Storage::RegistryNode> attr = reg->OpenNode(L"Attributa");
					if (attr) for (auto & v : attr->GetValues()) _desc.Attributes.Append(v, attr->GetValueString(v));
					_key_data = new DataBlock(1);
					_key_data->SetLength(reg->GetValueBinarySize(L"Clavis"));
					reg->GetValueBinary(L"Clavis", _key_data->GetBuffer());
				}
				XCertificate(const CertificateDesc & desc, IKey * key) : _desc(desc) { _key_data = key->LoadRepresentation(); if (!_key_data) throw OutOfMemoryException(); }
				virtual ~XCertificate(void) override {}
				virtual const CertificateDesc & GetDescription(void) noexcept override { return _desc; }
				virtual IKey * LoadPublicKey(void) noexcept override { return LoadKeyRSA(_key_data); }
				virtual DataBlock * LoadRepresentation(void) noexcept override
				{
					if (_data) {
						_data->Retain();
						return _data;
					} else try {
						Streaming::MemoryStream stream(0x1000);
						SafePointer<Storage::Registry> reg = Storage::CreateRegistry();
						reg->CreateValue(L"Organizatio", Storage::RegistryValueType::String);
						reg->CreateValue(L"Persona", Storage::RegistryValueType::String);
						reg->CreateValue(L"Usus", Storage::RegistryValueType::Integer);
						reg->CreateValue(L"UsusDerivatus", Storage::RegistryValueType::Integer);
						reg->CreateValue(L"Radix", Storage::RegistryValueType::Boolean);
						reg->CreateValue(L"ValidusAb", Storage::RegistryValueType::Time);
						reg->CreateValue(L"ValidusAd", Storage::RegistryValueType::Time);
						reg->CreateValue(L"Clavis", Storage::RegistryValueType::Binary);
						reg->SetValue(L"Organizatio", _desc.Organization);
						reg->SetValue(L"Persona", _desc.PersonName);
						reg->SetValue(L"Usus", int(_desc.CertificateUsage));
						reg->SetValue(L"UsusDerivatus", int(_desc.CertificateDerivation));
						reg->SetValue(L"Radix", _desc.IsRootCertificate);
						reg->SetValue(L"ValidusAb", _desc.ValidSince);
						reg->SetValue(L"ValidusAd", _desc.ValidUntil);
						reg->SetValue(L"Clavis", _key_data->GetBuffer(), _key_data->Length());
						if (!_desc.Attributes.IsEmpty()) {
							reg->CreateNode(L"Attributa");
							SafePointer<Storage::RegistryNode> attr = reg->OpenNode(L"Attributa");
							if (!attr) throw InvalidStateException();
							for (auto & a : _desc.Attributes) {
								attr->CreateValue(a.key, Storage::RegistryValueType::String);
								attr->SetValue(a.key, a.value);
							}
						}
						reg->Save(&stream);
						stream.Seek(0, Streaming::Begin);
						_data = stream.ReadAll();
						_data->Retain();
						return _data;
					} catch (...) { return 0; }
				}
			};
			class XIdentity : public IIdentity
			{
				SafePointer<IContainer> _cert_src;
				SafePointer<IKey> _private_key;
			public:
				XIdentity(IContainer * input0, IContainer * input1, const string & password)
				{
					if (input0 && (input0->GetContainerClass() == ContainerClass::Certificate || input0->GetContainerClass() == ContainerClass::Identity)) {
						_cert_src.SetRetain(input0);
					} else if (input1 && (input1->GetContainerClass() == ContainerClass::Certificate || input1->GetContainerClass() == ContainerClass::Identity)) {
						_cert_src.SetRetain(input1);
					} else throw InvalidArgumentException();
					if (input0 && (input0->GetContainerClass() == ContainerClass::PrivateKey || input0->GetContainerClass() == ContainerClass::Identity)) {
						_private_key = input0->LoadPrivateKey(password);
					} else if (input1 && (input1->GetContainerClass() == ContainerClass::PrivateKey || input1->GetContainerClass() == ContainerClass::Identity)) {
						_private_key = input1->LoadPrivateKey(password);
					} else throw InvalidArgumentException();
					if (!_private_key) throw InvalidArgumentException();
					SafePointer<ICertificate> main_cert = _cert_src->LoadCertificate(0);
					if (!main_cert) throw InvalidStateException();
					SafePointer<IKey> pub_key_1 = main_cert->LoadPublicKey();
					SafePointer<IKey> pub_key_2 = _private_key->ExtractPublicKey();
					if (!pub_key_1 || !pub_key_2) throw InvalidStateException();
					SafePointer<DataBlock> pub_key_data_1 = pub_key_1->LoadRepresentation();
					SafePointer<DataBlock> pub_key_data_2 = pub_key_2->LoadRepresentation();
					if (!pub_key_data_1 || !pub_key_data_2) throw InvalidStateException();
					if (pub_key_data_1->Length() != pub_key_data_2->Length()) throw InvalidStateException();
					if (MemoryCompare(pub_key_data_1->GetBuffer(), pub_key_data_2->GetBuffer(), pub_key_data_2->Length()) != 0) throw InvalidStateException();
				}
				virtual ~XIdentity(void) override {}
				virtual uint GetCertificateChainLength(void) noexcept override { return _cert_src->GetCertificateChainLength(); }
				virtual ICertificate * LoadCertificate(uint index) noexcept override { return _cert_src->LoadCertificate(index); }
				virtual DataBlock * LoadCertificateSignature(uint index) noexcept override { return _cert_src->LoadCertificateSignature(index); }
				virtual IKey * LoadPrivateKey(void) noexcept override { _private_key->Retain(); return _private_key; }
			};
			class XContainer : public IContainer
			{
				SafePointer<Storage::Registry> _storage;
			public:
				XContainer(ContainerClass cls)
				{
					_storage = Storage::CreateRegistry();
					if (!_storage) throw OutOfMemoryException();
					_storage->CreateValue(L"Classis", Storage::RegistryValueType::Integer);
					_storage->SetValue(L"Classis", int(cls));
				}
				XContainer(Streaming::Stream * stream)
				{
					uint64 org = stream->Seek(0, Streaming::Current);
					uint64 length = stream->Length();
					SafePointer<Streaming::Stream> window = new Streaming::FragmentStream(stream, org, length - org);
					_storage = Storage::LoadRegistry(window);
					if (!_storage) throw OutOfMemoryException();
				}
				virtual ~XContainer(void) override {}
				virtual uint GetCertificateChainLength(void) noexcept override { try { return _storage->GetValueInteger(L"NumerusCertificatorum"); } catch (...) { return 0; } }
				virtual ICertificate * LoadCertificate(uint index) noexcept override
				{
					try {
						string name = string(L"Certificatus") + string(index + 1);
						SafePointer<Storage::RegistryNode> node = _storage->OpenNode(name);
						if (!node) return 0;
						SafePointer<DataBlock> init = new DataBlock(1);
						init->SetLength(node->GetValueBinarySize(L"Data"));
						node->GetValueBinary(L"Data", init->GetBuffer());
						return new XCertificate(init);
					} catch (...) { return 0; }
				}
				virtual DataBlock * LoadCertificateSignature(uint index) noexcept override
				{
					try {
						string name = string(L"Certificatus") + string(index + 1);
						SafePointer<Storage::RegistryNode> node = _storage->OpenNode(name);
						if (!node) return 0;
						SafePointer<DataBlock> result = new DataBlock(1);
						result->SetLength(node->GetValueBinarySize(L"Subscriptio"));
						node->GetValueBinary(L"Subscriptio", result->GetBuffer());
						if (!result->Length()) return 0;
						result->Retain();
						return result;
					} catch (...) { return 0; }
				}
				virtual IKey * LoadPrivateKey(const string & password) noexcept override
				{
					try {
						SafePointer<DataBlock> data = new DataBlock(1);
						data->SetLength(_storage->GetValueBinarySize(L"ClavisInterna"));
						_storage->GetValueBinary(L"ClavisInterna", data->GetBuffer());
						if (_storage->GetValueBoolean(L"ClavisProtecta")) {
							if (!password.Length()) return 0;
							SafePointer<DataBlock> hash = password.EncodeSequence(Encoding::UTF8, false);
							if (!hash) return 0;
							hash = Cryptography::CreateHash(Cryptography::HashAlgorithm::SHA384, hash);
							if (!hash) return 0;
							SafePointer<DataBlock> kdata = new DataBlock(32);
							kdata->SetLength(32);
							MemoryCopy(kdata->GetBuffer(), hash->GetBuffer(), 32);
							SafePointer<Cryptography::Algorithm> aes = Cryptography::OpenEncryptionAlgorithm(Cryptography::EncryptionAlgorithm::AES);
							if (!aes) return 0;
							aes->SetEncryptionMode(Cryptography::EncryptionMode::CBC);
							SafePointer<Cryptography::Key> key_aes = aes->ImportKey(kdata);
							if (!key_aes) return 0;
							SafePointer<DataBlock> data_dec = key_aes->DecryptData(data, hash->GetBuffer() + 32);
							if (!data_dec) return 0;
							data = data_dec;
						}
						SafePointer<IKey> result = LoadKeyRSA(data);
						ZeroMemory(data->GetBuffer(), data->Length());
						if (!result) return 0;
						result->Retain();
						return result;
					} catch (...) { return 0; }
				}
				virtual DataBlock * LoadDocumentHash(void) noexcept override
				{
					try {
						SafePointer<DataBlock> result = new DataBlock(1);
						result->SetLength(_storage->GetValueBinarySize(L"Digestum"));
						_storage->GetValueBinary(L"Digestum", result->GetBuffer());
						if (!result->Length()) return 0;
						result->Retain();
						return result;
					} catch (...) { return 0; }
				}
				virtual DataBlock * LoadDocumentSignature(void) noexcept override
				{
					try {
						SafePointer<DataBlock> result = new DataBlock(1);
						result->SetLength(_storage->GetValueBinarySize(L"Subscriptio"));
						_storage->GetValueBinary(L"Subscriptio", result->GetBuffer());
						if (!result->Length()) return 0;
						result->Retain();
						return result;
					} catch (...) { return 0; }
				}
				virtual ContainerClass GetContainerClass(void) noexcept override { try { return static_cast<ContainerClass>(_storage->GetValueInteger(L"Classis")); } catch (...) { return ContainerClass::Unknown; } }
				virtual DataBlock * LoadContainerRepresentation(void) noexcept override
				{
					try {
						Streaming::MemoryStream stream(0x1000);
						_storage->Save(&stream);
						stream.Seek(0, Streaming::Begin);
						return stream.ReadAll();
					} catch (...) { return 0; }
				}
				void init_add_certificate(ICertificate * certificate)
				{
					SafePointer<DataBlock> cert_data = certificate ? certificate->LoadRepresentation() : 0;
					if (!cert_data) throw OutOfMemoryException();
					try { _storage->CreateValue(L"NumerusCertificatorum", Storage::RegistryValueType::Integer); } catch (...) {}
					int n_cert = _storage->GetValueInteger(L"NumerusCertificatorum") + 1;
					string name = string(L"Certificatus") + string(n_cert);
					_storage->CreateNode(name);
					_storage->CreateValue(name + L"/Data", Storage::RegistryValueType::Binary);
					_storage->SetValue(name + L"/Data", cert_data->GetBuffer(), cert_data->Length());
					_storage->SetValue(L"NumerusCertificatorum", n_cert);
				}
				void init_add_certificate(DataBlock * certificate, DataBlock * signed_hash)
				{
					try { _storage->CreateValue(L"NumerusCertificatorum", Storage::RegistryValueType::Integer); } catch (...) {}
					int n_cert = _storage->GetValueInteger(L"NumerusCertificatorum") + 1;
					string name = string(L"Certificatus") + string(n_cert);
					_storage->CreateNode(name);
					_storage->CreateValue(name + L"/Data", Storage::RegistryValueType::Binary);
					_storage->SetValue(name + L"/Data", certificate->GetBuffer(), certificate->Length());
					_storage->CreateValue(name + L"/Subscriptio", Storage::RegistryValueType::Binary);
					_storage->SetValue(name + L"/Subscriptio", signed_hash->GetBuffer(), signed_hash->Length());
					_storage->SetValue(L"NumerusCertificatorum", n_cert);
				}
				void init_add_private_key(IKey * key, const string & password)
				{
					SafePointer<DataBlock> data = key->LoadRepresentation();
					_storage->CreateValue(L"ClavisInterna", Storage::RegistryValueType::Binary);
					_storage->CreateValue(L"ClavisProtecta", Storage::RegistryValueType::Boolean);
					if (password.Length()) {
						_storage->SetValue(L"ClavisProtecta", true);
						SafePointer<DataBlock> hash = password.EncodeSequence(Encoding::UTF8, false);
						if (!hash) throw OutOfMemoryException();
						hash = Cryptography::CreateHash(Cryptography::HashAlgorithm::SHA384, hash);
						if (!hash) throw OutOfMemoryException();
						SafePointer<DataBlock> kdata = new DataBlock(32);
						kdata->SetLength(32);
						MemoryCopy(kdata->GetBuffer(), hash->GetBuffer(), 32);
						SafePointer<Cryptography::Algorithm> aes = Cryptography::OpenEncryptionAlgorithm(Cryptography::EncryptionAlgorithm::AES);
						if (!aes) throw OutOfMemoryException();
						aes->SetEncryptionMode(Cryptography::EncryptionMode::CBC);
						SafePointer<Cryptography::Key> key_aes = aes->ImportKey(kdata);
						if (!key_aes) throw OutOfMemoryException();
						SafePointer<DataBlock> data_enc = key_aes->EncryptData(data, hash->GetBuffer() + 32);
						if (!data_enc) throw OutOfMemoryException();
						ZeroMemory(data->GetBuffer(), data->Length());
						data = data_enc;
					} else _storage->SetValue(L"ClavisProtecta", false);
					_storage->SetValue(L"ClavisInterna", data->GetBuffer(), data->Length());
				}
				void init_add_hash(DataBlock * data)
				{
					_storage->CreateValue(L"Digestum", Storage::RegistryValueType::Binary);
					_storage->SetValue(L"Digestum", data->GetBuffer(), data->Length());
				}
				void init_add_signed_hash(DataBlock * data)
				{
					_storage->CreateValue(L"Subscriptio", Storage::RegistryValueType::Binary);
					_storage->SetValue(L"Subscriptio", data->GetBuffer(), data->Length());
				}
			};

			ICertificate * LoadCertificate(DataBlock * data) noexcept { try { return new XCertificate(data); } catch (...) { return 0; } }
			IIdentity * LoadIdentity(IContainer * input0, IContainer * input1, const string & password) noexcept { try { return new XIdentity(input0, input1, password); } catch (...) { return 0; } }
			IContainer * LoadContainer(Streaming::Stream * input) noexcept { try { return new XContainer(input); } catch (...) { return 0; } }
			IContainer * CreateCertificate(const CertificateDesc & cert_desc, IKey * public_key) noexcept
			{
				try {
					SafePointer<XContainer> result = new XContainer(ContainerClass::UnsignedCertificate);
					SafePointer<XCertificate> cert = new XCertificate(cert_desc, public_key);
					result->init_add_certificate(cert);
					result->Retain();
					return result;
				} catch (...) { return 0; }
			}
			IContainer * ValidateCertificate(ICertificate * unsigned_cert, IKey * private_key) noexcept
			{
				try {
					if (!unsigned_cert || !unsigned_cert->GetDescription().IsRootCertificate) return 0;
					SafePointer<IKey> pub_key_1 = unsigned_cert->LoadPublicKey();
					SafePointer<IKey> pub_key_2 = private_key->ExtractPublicKey();
					if (!pub_key_1 || !pub_key_2) return 0;
					SafePointer<DataBlock> pub_key_data_1 = pub_key_1->LoadRepresentation();
					SafePointer<DataBlock> pub_key_data_2 = pub_key_2->LoadRepresentation();
					if (!pub_key_data_1 || !pub_key_data_2) return 0;
					if (pub_key_data_1->Length() != pub_key_data_2->Length()) return 0;
					if (MemoryCompare(pub_key_data_1->GetBuffer(), pub_key_data_2->GetBuffer(), pub_key_data_2->Length()) != 0) return 0;
					SafePointer<DataBlock> cert_data = unsigned_cert->LoadRepresentation();
					if (!cert_data) return 0;
					SafePointer<DataBlock> hash = Cryptography::CreateHash(Cryptography::HashAlgorithm::SHA512, cert_data);
					if (!hash) return 0;
					SafePointer<DataBlock> signed_hash = private_key->Sign(hash);
					if (!signed_hash) return 0;
					SafePointer<XContainer> result = new XContainer(ContainerClass::Certificate);
					result->init_add_certificate(cert_data, signed_hash);
					result->Retain();
					return result;
				} catch (...) { return 0; }
			}
			IContainer * ValidateCertificate(ICertificate * unsigned_cert, IIdentity * identity) noexcept
			{
				try {
					if (!unsigned_cert || unsigned_cert->GetDescription().IsRootCertificate) return 0;
					SafePointer<DataBlock> cert_data = unsigned_cert->LoadRepresentation();
					if (!cert_data) return 0;
					SafePointer<DataBlock> hash = Cryptography::CreateHash(Cryptography::HashAlgorithm::SHA512, cert_data);
					if (!hash) return 0;
					SafePointer<IKey> private_key = identity->LoadPrivateKey();
					if (!private_key) return 0;
					SafePointer<DataBlock> signed_hash = private_key->Sign(hash);
					if (!signed_hash) return 0;
					SafePointer<XContainer> result = new XContainer(ContainerClass::Certificate);
					result->init_add_certificate(cert_data, signed_hash);
					for (uint i = 0; i < identity->GetCertificateChainLength(); i++) {
						SafePointer<ICertificate> cert = identity->LoadCertificate(i);
						if (!cert) return 0;
						SafePointer<DataBlock> cert_data = cert->LoadRepresentation();
						SafePointer<DataBlock> cert_sign = identity->LoadCertificateSignature(i);
						if (!cert_data || !cert_sign) return 0;
						result->init_add_certificate(cert_data, cert_sign);
					}
					result->Retain();
					return result;
				} catch (...) { return 0; }
			}
			IContainer * CreatePrivateKeyStorage(IKey * private_key, const string & password) noexcept
			{
				try {
					SafePointer<XContainer> result = new XContainer(ContainerClass::PrivateKey);
					result->init_add_private_key(private_key, password);
					result->Retain();
					return result;
				} catch (...) { return 0; }
			}
			IContainer * CreateIdentityStorage(IIdentity * identity, const string & password) noexcept
			{
				try {
					if (!identity) return 0;
					SafePointer<XContainer> result = new XContainer(ContainerClass::Identity);
					for (uint i = 0; i < identity->GetCertificateChainLength(); i++) {
						SafePointer<ICertificate> cert = identity->LoadCertificate(i);
						if (!cert) return 0;
						SafePointer<DataBlock> cert_data = cert->LoadRepresentation();
						SafePointer<DataBlock> cert_sign = identity->LoadCertificateSignature(i);
						if (!cert_data || !cert_sign) return 0;
						result->init_add_certificate(cert_data, cert_sign);
					}
					SafePointer<IKey> key = identity->LoadPrivateKey();
					if (!key) return 0;
					result->init_add_private_key(key, password);
					result->Retain();
					return result;
				} catch (...) { return 0; }
			}
			IContainer * CreateCertificateStorage(IContainer * source) noexcept
			{
				try {
					if (!source) return 0;
					SafePointer<XContainer> result = new XContainer(ContainerClass::Certificate);
					for (uint i = 0; i < source->GetCertificateChainLength(); i++) {
						SafePointer<ICertificate> cert = source->LoadCertificate(i);
						if (!cert) return 0;
						SafePointer<DataBlock> cert_data = cert->LoadRepresentation();
						SafePointer<DataBlock> cert_sign = source->LoadCertificateSignature(i);
						if (!cert_data || !cert_sign) return 0;
						result->init_add_certificate(cert_data, cert_sign);
					}
					result->Retain();
					return result;
				} catch (...) { return 0; }
			}
			IContainer * CreateIntegrityData(DataBlock * hash) noexcept
			{
				try {
					if (!hash) return 0;
					SafePointer<XContainer> result = new XContainer(ContainerClass::IntegrityData);
					result->init_add_hash(hash);
					result->Retain();
					return result;
				} catch (...) { return 0; }
			}
			IContainer * CreateSignatureData(DataBlock * hash, IIdentity * identity) noexcept
			{
				try {
					if (!hash || !identity) return 0;
					SafePointer<XContainer> result = new XContainer(ContainerClass::Signature);
					SafePointer<IKey> private_key = identity->LoadPrivateKey();
					if (!private_key) return 0;
					SafePointer<DataBlock> signed_hash = private_key->Sign(hash);
					if (!signed_hash) return 0;
					result->init_add_hash(hash);
					result->init_add_signed_hash(signed_hash);
					for (uint i = 0; i < identity->GetCertificateChainLength(); i++) {
						SafePointer<ICertificate> cert = identity->LoadCertificate(i);
						if (!cert) return 0;
						SafePointer<DataBlock> cert_data = cert->LoadRepresentation();
						SafePointer<DataBlock> cert_sign = identity->LoadCertificateSignature(i);
						if (!cert_data || !cert_sign) return 0;
						result->init_add_certificate(cert_data, cert_sign);
					}
					result->Retain();
					return result;
				} catch (...) { return 0; }
			}

			class StandardTrustProvider : public ITrustProvider
			{
				Volumes::Dictionary<string, TrustStatus> _trust;
			public:
				StandardTrustProvider(void) {}
				virtual ~StandardTrustProvider(void) override {}
				virtual bool AddTrust(DataBlock * cert_hash, bool trusted) noexcept override
				{
					try {
						if (!cert_hash) return false;
						return _trust.Append(StringFromDataBlock(cert_hash, 0, false), trusted ? TrustStatus::Trusted : TrustStatus::Untrusted);
					} catch (...) { return false; }
				}
				virtual bool AddTrust(ICertificate * cert, bool trusted) noexcept override
				{
					try {
						if (!cert) return false;
						SafePointer<DataBlock> data = cert->LoadRepresentation();
						if (!data) return false;
						SafePointer<DataBlock> hash = Cryptography::CreateHash(Cryptography::HashAlgorithm::SHA512, data);
						if (!hash) return false;
						return AddTrust(hash, trusted);
					} catch (...) { return false; }
				}
				virtual bool AddTrust(Streaming::Stream * cert_stream, bool trusted) noexcept override
				{
					try {
						if (!cert_stream) return false;
						SafePointer<IContainer> cont = LoadContainer(cert_stream);
						if (!cont || cont->GetContainerClass() != ContainerClass::Certificate) return true;
						SafePointer<ICertificate> cert = cont->LoadCertificate(0);
						return AddTrust(cert, trusted);
					} catch (...) { return false; }
				}
				virtual bool AddTrust(const string & cert_file, bool trusted) noexcept override
				{
					try {
						SafePointer<Streaming::Stream> stream = new Streaming::FileStream(cert_file, Streaming::AccessRead, Streaming::OpenExisting);
						return AddTrust(stream, trusted);
					} catch (...) { return false; }
				}
				virtual bool AddTrustDirectory(const string & folder, bool trusted) noexcept override
				{
					try {
						SafePointer< Array<string> > files = IO::Search::GetFiles(folder + L"/*." + FileExtensions::Certificate);
						if (!files) return false;
						for (auto & f : files->Elements()) if (!AddTrust(folder + L"/" + f, trusted)) return false;
						return true;
					} catch (...) { return false; }
				}
				virtual TrustStatus ProvideTrust(const DataBlock * cert_hash) const noexcept override
				{
					try {
						if (!cert_hash) return TrustStatus::Undefined;
						auto tv = _trust[StringFromDataBlock(cert_hash, 0, false)];
						if (tv) return *tv; else return TrustStatus::Undefined;
					} catch (...) { return TrustStatus::Untrusted; }
				}
			};
			ITrustProvider * CreateTrustProvider(void) noexcept { try { return new StandardTrustProvider(); } catch (...) { return 0; } }
			TrustStatus EvaluateIntegrity(DataBlock * hash, IContainer * idata, const Time & timestamp, const ITrustProvider * trust, IntegrityValidationDesc * desc) noexcept
			{
				if (!hash || !idata) return TrustStatus::Untrusted;
				try {
					if (idata->GetContainerClass() != ContainerClass::IntegrityData && idata->GetContainerClass() != ContainerClass::Signature) return TrustStatus::Untrusted;
					if (desc) {
						desc->object = IntegrityStatus::OK;
						desc->chain = Array<IntegrityStatus>(1);
						desc->chain.SetLength(idata->GetCertificateChainLength());
						for (uint i = 0; i < idata->GetCertificateChainLength(); i++) desc->chain[i] = IntegrityStatus::OK;
					}
					try {
						SafePointer<DataBlock> ref_hash = idata->LoadDocumentHash();
						if (!ref_hash) throw InvalidFormatException();
						if (ref_hash->Length() != hash->Length()) throw InvalidFormatException();
						if (MemoryCompare(ref_hash->GetBuffer(), hash->GetBuffer(), hash->Length()) != 0) throw InvalidFormatException();
					} catch (...) { if (desc) desc->object = IntegrityStatus::DataCorruption; return TrustStatus::Untrusted; }
					if (idata->GetContainerClass() == ContainerClass::Signature && trust) {
						bool trusted_chain = false;
						SafePointer<DataBlock> root_sign = idata->LoadDocumentSignature();
						if (!root_sign) { if (desc) desc->object = IntegrityStatus::InternalError; return TrustStatus::Untrusted; }
						ObjectArray<ICertificate> cert(idata->GetCertificateChainLength());
						ObjectArray<IKey> keys(idata->GetCertificateChainLength());
						ObjectArray<DataBlock> signs(idata->GetCertificateChainLength());
						ObjectArray<DataBlock> hashes(idata->GetCertificateChainLength());
						for (uint i = 0; i < idata->GetCertificateChainLength(); i++) {
							SafePointer<ICertificate> c = idata->LoadCertificate(i);
							SafePointer<DataBlock> s = idata->LoadCertificateSignature(i);
							if (!c || !s) { if (desc) desc->chain[i] = IntegrityStatus::InternalError; return TrustStatus::Untrusted; }
							SafePointer<DataBlock> c_data = c->LoadRepresentation();
							SafePointer<IKey> k = c->LoadPublicKey();
							if (!c_data || !k) { if (desc) desc->chain[i] = IntegrityStatus::InternalError; return TrustStatus::Untrusted; }
							SafePointer<DataBlock> c_hash = Cryptography::CreateHash(Cryptography::HashAlgorithm::SHA512, c_data);
							if (!c_hash) { if (desc) desc->chain[i] = IntegrityStatus::InternalError; return TrustStatus::Untrusted; }
							cert.Append(c);
							keys.Append(k);
							signs.Append(s);
							hashes.Append(c_hash);
						}
						if (!cert.Length()) { if (desc) desc->object = IntegrityStatus::NoTrustInChain; return TrustStatus::Untrusted; }
						if (!keys[0].Validate(hash, root_sign)) { if (desc) desc->object = IntegrityStatus::DataSurrogation; return TrustStatus::Untrusted; }
						for (int i = 0; i < keys.Length(); i++) {
							IKey * key = i < keys.Length() - 1 ? &keys[i + 1] : &keys[i];
							if (!key->Validate(&hashes[i], &signs[i])) { if (desc) desc->chain[i] = IntegrityStatus::DataSurrogation; return TrustStatus::Untrusted; }
						}
						for (int i = 0; i < keys.Length(); i++) {
							auto & cdesc = cert[i].GetDescription();
							if (timestamp > cdesc.ValidUntil) { if (desc) desc->chain[i] = IntegrityStatus::Expired; return TrustStatus::Untrusted; }
							if (timestamp < cdesc.ValidSince) { if (desc) desc->chain[i] = IntegrityStatus::NotIntroduced; return TrustStatus::Untrusted; }
							if (i) { if (!(cdesc.CertificateUsage & CertificateUsageAuthority)) { if (desc) desc->chain[i] = IntegrityStatus::InvalidUsage; return TrustStatus::Untrusted; } }
							else { if (!(cdesc.CertificateUsage & CertificateUsageSignature)) { if (desc) desc->chain[i] = IntegrityStatus::InvalidUsage; return TrustStatus::Untrusted; } }
							auto t = trust->ProvideTrust(&hashes[i]);
							if (t == TrustStatus::Trusted) trusted_chain = true;
							else if (t == TrustStatus::Untrusted) { if (desc) desc->chain[i] = IntegrityStatus::Compromised; return TrustStatus::Untrusted; }
						}
						for (int i = 0; i < keys.Length() - 1; i++) {
							auto & cdesc0 = cert[i].GetDescription();
							auto & cdesc1 = cert[i + 1].GetDescription();
							if ((cdesc0.CertificateUsage & cdesc1.CertificateDerivation) != cdesc0.CertificateUsage) { if (desc) desc->chain[i] = IntegrityStatus::InvalidDerivation; return TrustStatus::Untrusted; }
							if ((cdesc0.CertificateDerivation & cdesc1.CertificateDerivation) != cdesc0.CertificateDerivation) { if (desc) desc->chain[i] = IntegrityStatus::InvalidDerivation; return TrustStatus::Untrusted; }
						}
						if (!trusted_chain) { if (desc) desc->object = IntegrityStatus::NoTrustInChain; return TrustStatus::Untrusted; }
						return TrustStatus::Trusted;
					} else return TrustStatus::Undefined;
				} catch (...) { return TrustStatus::Untrusted; }
			}
		}
	}
}