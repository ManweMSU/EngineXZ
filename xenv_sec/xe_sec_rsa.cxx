#include "xe_sec_rsa.h"
#include "xe_sec_arithm.h"

namespace Engine
{
	namespace XE
	{
		namespace Security
		{
			constexpr const char * RSA_PublicKey_Stamp = "XE-RSA-E";
			constexpr const char * RSA_PrivateKey_Stamp = "XE-RSA-I";

			uint MakeAlignment(uint input, uint mask) noexcept { return (input + mask) & (~mask); }
			DataBlock * MakeExternalRepresentation(const char * signature, int argc, const LargeInteger ** argv) noexcept
			{
				try {
					SafePointer<DataBlock> result = new DataBlock(1);
					Array<uint> sizes(1);
					sizes.SetLength(argc);
					uint size = 8;
					for (int i = 0; i < argc; i++) {
						sizes[i] = MakeAlignment(argv[i]->EffectiveBitLength() + 1, 0x1F) / 8;
						size += sizes[i] + 4;
					}
					result->SetLength(size);
					MemoryCopy(result->GetBuffer(), signature, 8);
					int pos = 8;
					for (int i = 0; i < argc; i++) {
						auto encode_length = sizes[i];
						auto input_length = argv[i]->DWordLength() * 4;
						*reinterpret_cast<uint32 *>(result->GetBuffer() + pos) = encode_length;
						pos += 4;
						if (encode_length > input_length) {
							MemoryCopy(result->GetBuffer() + pos, argv[i]->Data(), input_length);
							ZeroMemory(result->GetBuffer() + pos + input_length, encode_length - input_length);
						} else if (encode_length <= input_length) MemoryCopy(result->GetBuffer() + pos, argv[i]->Data(), encode_length);
						pos += encode_length;
					}
					result->Retain();
					return result;
				} catch (...) { return 0; }
			}
			bool CheckExternalRepresentation(const DataBlock * input, const char * signature) noexcept
			{
				if (input->Length() < 8) return false;
				return MemoryCompare(input->GetBuffer(), signature, 8) == 0;
			}
			bool ReadExternalRepresentation(const DataBlock * input, const char * signature, int argc, LargeInteger ** argv, int * size_ref_alignment)
			{
				if (input->Length() < 8) return false;
				if (MemoryCompare(input->GetBuffer(), signature, 8) != 0) return false;
				int pos = 8;
				for (int i = 0; i < argc; i++) {
					if (pos + 4 > input->Length()) return false;
					uint length = *reinterpret_cast<const uint32 *>(input->GetBuffer() + pos);
					pos += 4;
					uint length_allocate;
					if (size_ref_alignment[i] >= 0) length_allocate = MakeAlignment(length, size_ref_alignment[i]);
					else length_allocate = argv[-size_ref_alignment[i] - 1]->DWordLength() * 4;
					if (length < 0 || length > input->Length() - pos || length_allocate < length) return false;
					*argv[i] = LargeInteger(input->GetBuffer() + pos, length, length_allocate);
					pos += length;
				}
				return true;
			}

			class RSA_PublicKey : public IKey
			{
				SafePointer<ICryptographyAcceleration> accel;
				SafePointer<Object> accel_retainer;
				LargeInteger modulus;
				LargeInteger e;
				RSAAccelerationPowerFunction accel_func;
			public:
				RSA_PublicKey(const LargeInteger & mdl, const LargeInteger & exp, ICryptographyAcceleration * acceleration) : modulus(mdl), e(exp)
				{
					accel.SetRetain(acceleration);
					if (accel) accel->CreateRSAAcceleration(modulus.BitLength(), e.BitLength(), &accel_func, accel_retainer.InnerRef()); else accel_func = 0;
				}
				RSA_PublicKey(const DataBlock * data, ICryptographyAcceleration * acceleration)
				{
					accel.SetRetain(acceleration);
					LargeInteger * argv[2] = { &modulus, &e };
					int size_ref_alignment[2] = { 16, 0 };
					if (!ReadExternalRepresentation(data, RSA_PublicKey_Stamp, 2, argv, size_ref_alignment)) throw InvalidFormatException();
					if (accel) accel->CreateRSAAcceleration(modulus.BitLength(), e.BitLength(), &accel_func, accel_retainer.InnerRef()); else accel_func = 0;
				}
				virtual ~RSA_PublicKey(void) override { modulus.InitWithZero(); e.InitWithZero(); }
				virtual KeyClass GetKeyClass(void) noexcept override { return KeyClass::RSA_Public; }
				virtual IKey * ExtractPublicKey(void) noexcept override { Retain(); return this; }
				virtual DataBlock * Encrypt(DataBlock * in) noexcept override
				{
					try {
						if (in->Length() > modulus.DWordLength() * 4) return 0;
						LargeInteger input(in->GetBuffer(), in->Length(), modulus.DWordLength() * 4), output(modulus.BitLength());
						if (LongCompare(input, modulus) >= 0) return 0;
						if (accel_func) {
							LargeInteger aux(modulus.BitLength());
							accel_func(output.Data(), input.Data(), e.Data(), modulus.Data(), aux.Data());
						} else LongPower(output, input, e, modulus);
						SafePointer<DataBlock> result = new DataBlock(1);
						result->SetLength(4 * output.DWordLength());
						MemoryCopy(result->GetBuffer(), output.Data(), 4 * output.DWordLength());
						result->Retain();
						return result;
					} catch (...) { return 0; }
				}
				virtual DataBlock * Decrypt(DataBlock * in) noexcept override { return Encrypt(in); }
				virtual DataBlock * Sign(DataBlock * hash) noexcept override { return 0; }
				virtual bool Validate(DataBlock * hash, DataBlock * signature) noexcept override
				{
					try {
						if (signature->Length() > modulus.DWordLength() * 4) return 0;
						LargeInteger input(signature->GetBuffer(), signature->Length(), modulus.DWordLength() * 4), output(modulus.BitLength());
						if (LongCompare(input, modulus) >= 0) return 0;
						if (accel_func) {
							LargeInteger aux(modulus.BitLength());
							accel_func(output.Data(), input.Data(), e.Data(), modulus.Data(), aux.Data());
						} else LongPower(output, input, e, modulus);
						uint data_bit_length = output.EffectiveBitLength();
						uint hash_bit_length = hash->Length() * 8;
						if (hash_bit_length != output.Data()[0]) return false;
						if (hash_bit_length + 32 > data_bit_length) return false;
						return MemoryCompare(output.Data() + 1, hash->GetBuffer(), hash->Length()) == 0;
					} catch (...) { return false; }
				}
				virtual uint GetParameterCount(void) noexcept override { return 2; }
				virtual bool LoadParameter(uint index, string & name, DataBlock ** data) noexcept override
				{
					try {
						SafePointer<DataBlock> result = new DataBlock(1);
						if (index == 0) {
							name = L"modulus";
							result->SetLength(modulus.DWordLength() * 4);
							MemoryCopy(result->GetBuffer(), modulus.Data(), modulus.DWordLength() * 4);
						} else if (index == 1) {
							name = L"e";
							result->SetLength(e.DWordLength() * 4);
							MemoryCopy(result->GetBuffer(), e.Data(), e.DWordLength() * 4);
						} else return false;
						result->Retain();
						(*data) = result;
						return true;
					} catch (...) { return false; }
				}
				virtual DataBlock * LoadRepresentation(void) noexcept override
				{
					const LargeInteger * argv[2] = { &modulus, &e };
					return MakeExternalRepresentation(RSA_PublicKey_Stamp, 2, argv);
				}
			};
			class RSA_PrivateKey : public IKey
			{
				SafePointer<ICryptographyAcceleration> accel;
				SafePointer<Object> accel_retainer;
				LargeInteger modulus;
				LargeInteger p, q;
				LargeInteger e, d;
				RSAAccelerationPowerFunction accel_func;
			public:
				RSA_PrivateKey(const void * pdata, int plength, const void * qdata, int qlength, uint64 pexp, ICryptographyAcceleration * acceleration) : p(pdata, plength), q(qdata, qlength), e(&pexp, 8)
				{
					accel.SetRetain(acceleration);
					modulus = LargeInteger(p.EffectiveBitLength() + q.EffectiveBitLength() + 1);
					LongMultiply(modulus, p, q);
					LargeInteger phi(modulus.BitLength());
					LargeInteger p1(p);
					LargeInteger q1(q);
					LongSubtract(p1, 1);
					LongSubtract(q1, 1);
					LongMultiply(phi, p1, q1);
					LongInverse(d, e, phi);
					modulus = modulus.Resize(MakeAlignment(modulus.EffectiveBitLength(), 0x7F));
					if (accel) accel->CreateRSAAcceleration(modulus.BitLength(), d.BitLength(), &accel_func, accel_retainer.InnerRef()); else accel_func = 0;
				}
				RSA_PrivateKey(const DataBlock * data, ICryptographyAcceleration * acceleration)
				{
					accel.SetRetain(acceleration);
					LargeInteger * argv[5] = { &modulus, &p, &q, &e, &d };
					int size_ref_alignment[5] = { 16, 0, 0, 0, 0 };
					if (!ReadExternalRepresentation(data, RSA_PrivateKey_Stamp, 5, argv, size_ref_alignment)) throw InvalidFormatException();
					if (accel) accel->CreateRSAAcceleration(modulus.BitLength(), d.BitLength(), &accel_func, accel_retainer.InnerRef()); else accel_func = 0;
				}
				virtual ~RSA_PrivateKey(void) override { modulus.InitWithZero(); p.InitWithZero(); q.InitWithZero(); e.InitWithZero(); d.InitWithZero(); }
				virtual KeyClass GetKeyClass(void) noexcept override { return KeyClass::RSA_Private; }
				virtual IKey * ExtractPublicKey(void) noexcept override
				{
					try {
						SafePointer<RSA_PublicKey> key = new RSA_PublicKey(modulus, e, accel);
						key->Retain();
						return key;
					} catch (...) { return 0; }
				}
				virtual DataBlock * Encrypt(DataBlock * in) noexcept override
				{
					try {
						if (in->Length() > modulus.DWordLength() * 4) return 0;
						LargeInteger input(in->GetBuffer(), in->Length(), modulus.DWordLength() * 4), output(modulus.BitLength());
						if (LongCompare(input, modulus) >= 0) return 0;
						if (accel_func) {
							LargeInteger aux(modulus.BitLength());
							accel_func(output.Data(), input.Data(), d.Data(), modulus.Data(), aux.Data());
						} else LongPower(output, input, d, modulus);
						SafePointer<DataBlock> result = new DataBlock(1);
						result->SetLength(4 * output.DWordLength());
						MemoryCopy(result->GetBuffer(), output.Data(), 4 * output.DWordLength());
						result->Retain();
						return result;
					} catch (...) { return 0; }
				}
				virtual DataBlock * Decrypt(DataBlock * in) noexcept override { return Encrypt(in); }
				virtual DataBlock * Sign(DataBlock * hash) noexcept override
				{
					try {
						uint modulus_bit_length = modulus.EffectiveBitLength();
						uint hash_bit_length = hash->Length() * 8;
						if (modulus_bit_length < hash_bit_length) return 0;
						if (modulus_bit_length - hash_bit_length < 64) return 0;
						LargeInteger input(modulus.BitLength()), output(modulus.BitLength());
						input.InitWithRandom(modulus_bit_length);
						input.Data()[0] = hash_bit_length;
						MemoryCopy(input.Data() + 1, hash->GetBuffer(), hash->Length());
						for (int j = input.DWordLength() - 1; j >= 0; j--) if (modulus.Data()[j]) {
							if (input.Data()[j] >= modulus.Data()[j]) input.Data()[j] = modulus.Data()[j] - 1;
							if (input.Data()[j - 1] == 0) input.Data()[j - 1] = 1;
							break;
						}
						if (accel_func) {
							LargeInteger aux(modulus.BitLength());
							accel_func(output.Data(), input.Data(), d.Data(), modulus.Data(), aux.Data());
						} else LongPower(output, input, d, modulus);
						SafePointer<DataBlock> result = new DataBlock(1);
						result->SetLength(4 * output.DWordLength());
						MemoryCopy(result->GetBuffer(), output.Data(), 4 * output.DWordLength());
						result->Retain();
						return result;
					} catch (...) { return 0; }
				}
				virtual bool Validate(DataBlock * hash, DataBlock * signature) noexcept override { return 0; }
				virtual uint GetParameterCount(void) noexcept override { return 5; }
				virtual bool LoadParameter(uint index, string & name, DataBlock ** data) noexcept override
				{
					try {
						SafePointer<DataBlock> result = new DataBlock(1);
						if (index == 0) {
							name = L"modulus";
							result->SetLength(modulus.DWordLength() * 4);
							MemoryCopy(result->GetBuffer(), modulus.Data(), modulus.DWordLength() * 4);
						} else if (index == 1) {
							name = L"p";
							result->SetLength(p.DWordLength() * 4);
							MemoryCopy(result->GetBuffer(), p.Data(), p.DWordLength() * 4);
						} else if (index == 2) {
							name = L"q";
							result->SetLength(q.DWordLength() * 4);
							MemoryCopy(result->GetBuffer(), q.Data(), q.DWordLength() * 4);
						} else if (index == 3) {
							name = L"e";
							result->SetLength(e.DWordLength() * 4);
							MemoryCopy(result->GetBuffer(), e.Data(), e.DWordLength() * 4);
						} else if (index == 4) {
							name = L"d";
							result->SetLength(d.DWordLength() * 4);
							MemoryCopy(result->GetBuffer(), d.Data(), d.DWordLength() * 4);
						} else return false;
						result->Retain();
						(*data) = result;
						return true;
					} catch (...) { return false; }
				}
				virtual DataBlock * LoadRepresentation(void) noexcept override
				{
					const LargeInteger * argv[5] = { &modulus, &p, &q, &e, &d };
					return MakeExternalRepresentation(RSA_PrivateKey_Stamp, 5, argv);
				}
			};

			IKey * LoadKeyRSA(const DataBlock * representation, ICryptographyAcceleration * acceleration) noexcept
			{
				try {
					if (CheckExternalRepresentation(representation, RSA_PublicKey_Stamp)) {
						return new RSA_PublicKey(representation, acceleration);
					} else if (CheckExternalRepresentation(representation, RSA_PrivateKey_Stamp)) {
						return new RSA_PrivateKey(representation, acceleration);
					} else return 0;
				} catch (...) { return 0; }
			}
			bool CreateKeyRSA(const void * pdata, int plength, const void * qdata, int qlength, uint64 pexp, IKey ** key_prvt, ICryptographyAcceleration * acceleration) noexcept
			{
				try {
					SafePointer<RSA_PrivateKey> prvt = new (std::nothrow) RSA_PrivateKey(pdata, plength, qdata, qlength, pexp, acceleration);
					if (!prvt) throw OutOfMemoryException();
					prvt->Retain();
					(*key_prvt) = prvt;
					return true;
				} catch (...) { return false; }
			}
		}
	}
}