#include "xe_crypto.h"

#include "xe_interfaces.h"
#include "xe_tryblock.h"

namespace Engine
{
	namespace XE
	{
		struct XKeyDesc
		{
			const void * key_data;
			int key_data_length;
			int encryption_mode;
			int cipher;
		};
		class XKey : public Object
		{
			SafePointer<Cryptography::Key> _key;
		public:
			XKey(const XKeyDesc & desc)
			{
				SafePointer<Cryptography::Algorithm> alg;
				if (desc.cipher == 0) alg = Cryptography::OpenEncryptionAlgorithm(Cryptography::EncryptionAlgorithm::AES);
				else throw InvalidArgumentException();
				if (!alg) throw OutOfMemoryException();
				if (desc.encryption_mode == 0) alg->SetEncryptionMode(Cryptography::EncryptionMode::ECB);
				else if (desc.encryption_mode == 1) alg->SetEncryptionMode(Cryptography::EncryptionMode::CBC);
				else throw InvalidArgumentException();
				if (desc.key_data_length != 32) throw InvalidArgumentException();
				_key = alg->ImportKey(desc.key_data, desc.key_data_length);
				if (!_key) throw InvalidArgumentException();
			}
			virtual ~XKey(void) override {}
			virtual SafePointer<DataBlock> Encrypt(const void * data, int length, const void * iv, int iv_length, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				uint8 aiv[16];
				ZeroMemory(aiv, sizeof(aiv));
				MemoryCopy(aiv, iv, min(iv_length, 16));
				SafePointer<DataBlock> result = _key->EncryptData(data, length, aiv);
				if (!result) throw OutOfMemoryException();
				return result;
				XE_TRY_OUTRO(0)
			}
			virtual SafePointer<DataBlock> Decrypt(const void * data, int length, const void * iv, int iv_length, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				uint8 aiv[16];
				ZeroMemory(aiv, sizeof(aiv));
				MemoryCopy(aiv, iv, min(iv_length, 16));
				SafePointer<DataBlock> result = _key->DecryptData(data, length, aiv);
				if (!result) throw InvalidFormatException();
				return result;
				XE_TRY_OUTRO(0)
			}
		};
		class CryptoExtension : public IAPIExtension
		{
			static SafePointer<XKey> _create_key(const XKeyDesc & desc, ErrorContext & ectx) noexcept { XE_TRY_INTRO return new XKey(desc); XE_TRY_OUTRO(0) }
			static SafePointer<DataBlock> _create_hash(int hash, const void * data, int length, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<DataBlock> result;
				if (hash == 0) result = Cryptography::CreateHash(Cryptography::HashAlgorithm::MD5, data, length);
				else if (hash == 1) result = Cryptography::CreateHash(Cryptography::HashAlgorithm::SHA1, data, length);
				else if (hash == 2) result = Cryptography::CreateHash(Cryptography::HashAlgorithm::SHA256, data, length);
				else if (hash == 3) result = Cryptography::CreateHash(Cryptography::HashAlgorithm::SHA384, data, length);
				else if (hash == 4) result = Cryptography::CreateHash(Cryptography::HashAlgorithm::SHA512, data, length);
				else throw InvalidArgumentException();
				if (!result) throw OutOfMemoryException();
				return result;
				XE_TRY_OUTRO(0)
			}
			static SafePointer<DataBlock> _create_random(int length, ErrorContext & ectx) noexcept
			{
				XE_TRY_INTRO
				SafePointer<DataBlock> result = Cryptography::CreateSecureRandom(length);
				if (!result) throw OutOfMemoryException();
				return result;
				XE_TRY_OUTRO(0)
			}
		public:
			CryptoExtension(void) {}
			virtual ~CryptoExtension(void) override {}
			virtual const void * ExposeRoutine(const string & routine_name) noexcept override
			{
				if (string::Compare(routine_name, L"cy_crcl") < 0) {
					if (string::Compare(routine_name, L"cy_crcc") == 0) return reinterpret_cast<const void *>(&_create_random);
				} else {
					if (string::Compare(routine_name, L"cy_crdg") < 0) {
						if (string::Compare(routine_name, L"cy_crcl") == 0) return reinterpret_cast<const void *>(&_create_key);
					} else {
						if (string::Compare(routine_name, L"cy_crdg") == 0) return reinterpret_cast<const void *>(&_create_hash);
					}
				}
				return 0;
			}
			virtual const void * ExposeInterface(const string & interface) noexcept override { return 0; }
		};
		void ActivateCryptography(StandardLoader & ldr)
		{
			SafePointer<IAPIExtension> ext = new CryptoExtension;
			if (!ldr.RegisterAPIExtension(ext)) throw Exception();
		}
	}
}