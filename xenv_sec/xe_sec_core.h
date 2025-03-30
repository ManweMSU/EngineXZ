#pragma once

#include <EngineRuntime.h>

namespace Engine
{
	namespace XE
	{
		namespace Security
		{
			namespace FileExtensions {
				constexpr const widechar * UnsignedCertificate	= L"xepraecert";
				constexpr const widechar * Certificate			= L"xecert";
				constexpr const widechar * PrivateKey			= L"xeclavis";
				constexpr const widechar * Identity				= L"xeindentitas";
			}
			enum class KeyClass {
				Unknown		= 0x000,
				RSA_Public	= 0x001,
				RSA_Private	= 0x101,
			};
			enum class ContainerClass {
				Unknown				= 0x000,
				UnsignedCertificate	= 0x001,
				Certificate			= 0x002,
				PrivateKey			= 0x003,
				Identity			= 0x004,
				IntegrityData		= 0x101,
				Signature			= 0x102,
			};
			enum CertificateUsageFlags : uint {
				CertificateUsageAuthority	= 0x0001,
				CertificateUsageSignature	= 0x0002,
				CertificateUsageMask		= CertificateUsageAuthority | CertificateUsageSignature,
			};
			struct CertificateDesc
			{
				string Organization;
				string PersonName;
				uint CertificateUsage;
				uint CertificateDerivation;
				bool IsRootCertificate;
				Time ValidSince, ValidUntil;
				Volumes::Dictionary<string, string> Attributes;
			};

			class IKey : public Object
			{
			public:
				virtual KeyClass GetKeyClass(void) noexcept = 0;
				virtual IKey * ExtractPublicKey(void) noexcept = 0;
				virtual DataBlock * Encrypt(DataBlock * in) noexcept = 0;
				virtual DataBlock * Decrypt(DataBlock * in) noexcept = 0;
				virtual DataBlock * Sign(DataBlock * hash) noexcept = 0;
				virtual bool Validate(DataBlock * hash, DataBlock * signature) noexcept = 0;
				virtual uint GetParameterCount(void) noexcept = 0;
				virtual bool LoadParameter(uint index, string & name, DataBlock ** data) noexcept = 0;
				virtual DataBlock * LoadRepresentation(void) noexcept = 0;
			};
			class ICertificate : public Object
			{
			public:
				virtual const CertificateDesc & GetDescription(void) noexcept = 0;
				virtual IKey * LoadPublicKey(void) noexcept = 0;
				virtual DataBlock * LoadRepresentation(void) noexcept = 0;
			};
			class IIdentity : public Object
			{
			public:
				virtual uint GetCertificateChainLength(void) noexcept = 0;
				virtual ICertificate * LoadCertificate(uint index) noexcept = 0;
				virtual DataBlock * LoadCertificateSignature(uint index) noexcept = 0;
				virtual IKey * LoadPrivateKey(void) noexcept = 0;
			};
			class IContainer : public Object
			{
			public:
				// For Certificate, Identity, Signature	: 1 or greater
				// For UnsignedCertificate				: 1 exactly
				virtual uint GetCertificateChainLength(void) noexcept = 0;
				// For UnsignedCertificate, Certificate, Identity, Signature
				virtual ICertificate * LoadCertificate(uint index) noexcept = 0;
				// For Certificate, Identity, Signature
				virtual DataBlock * LoadCertificateSignature(uint index) noexcept = 0;
				// For PrivateKey, Identity
				virtual IKey * LoadPrivateKey(const string & password) noexcept = 0;
				// For IntegrityData, Signature
				virtual DataBlock * LoadDocumentHash(void) noexcept = 0;
				// For Signature
				virtual DataBlock * LoadDocumentSignature(void) noexcept = 0;
				// General-purpose
				virtual ContainerClass GetContainerClass(void) noexcept = 0;
				// General-purpose
				virtual DataBlock * LoadContainerRepresentation(void) noexcept = 0;
			};

			ICertificate * LoadCertificate(DataBlock * data) noexcept;
			IIdentity * LoadIdentity(IContainer * input0, IContainer * input1, const string & password) noexcept;

			IContainer * LoadContainer(Streaming::Stream * input) noexcept;
			IContainer * CreateCertificate(const CertificateDesc & cert_desc, IKey * public_key) noexcept; // UnsignedCertificate
			IContainer * ValidateCertificate(ICertificate * unsigned_cert, IKey * private_key) noexcept; // Certificate, for root ones
			IContainer * ValidateCertificate(ICertificate * unsigned_cert, IIdentity * identity) noexcept; // Certificate, for non-root ones
			IContainer * CreatePrivateKeyStorage(IKey * private_key, const string & password) noexcept; // PrivateKey
			IContainer * CreateIdentityStorage(IIdentity * identity, const string & password) noexcept; // Identity
			IContainer * CreateCertificateStorage(IContainer * source) noexcept; // Certificate
			IContainer * CreateIntegrityData(DataBlock * hash) noexcept; // IntegrityData
			IContainer * CreateSignatureData(DataBlock * hash, IIdentity * identity) noexcept; // Signature

			enum class TrustStatus { Untrusted, Undefined, Trusted };
			enum class IntegrityStatus {
				OK					= 0x00,
				DataCorruption		= 0x01,
				DataSurrogation		= 0x02,
				Expired				= 0x03,
				NotIntroduced		= 0x04,
				InvalidUsage		= 0x05,
				InvalidDerivation	= 0x06,
				Compromised			= 0x07,
				NoTrustInChain		= 0x08,
				InternalError		= 0xFF,
			};
			class ITrustProvider : public Object
			{
			public:
				virtual bool AddTrust(DataBlock * cert_hash, bool trusted) noexcept = 0;
				virtual bool AddTrust(ICertificate * cert, bool trusted) noexcept = 0;
				virtual bool AddTrust(Streaming::Stream * cert_stream, bool trusted) noexcept = 0;
				virtual bool AddTrust(const string & cert_file, bool trusted) noexcept = 0;
				virtual bool AddTrustDirectory(const string & folder, bool trusted) noexcept = 0;
				virtual TrustStatus ProvideTrust(const DataBlock * cert_hash) const noexcept = 0;
			};
			struct IntegrityValidationDesc {
				IntegrityStatus object;
				Array<IntegrityStatus> chain;
			};
			ITrustProvider * CreateTrustProvider(void) noexcept;
			TrustStatus EvaluateIntegrity(DataBlock * hash, IContainer * idata, const Time & timestamp, const ITrustProvider * trust, IntegrityValidationDesc * desc) noexcept;
		}
	}
}