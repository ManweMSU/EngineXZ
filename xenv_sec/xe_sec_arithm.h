#pragma once

#include <EngineRuntime.h>

namespace Engine
{
	namespace XE
	{
		namespace Security
		{
			class LargeInteger
			{
				uint32 * _data;
				int _num_dwords;
			public:
				LargeInteger(void);
				explicit LargeInteger(uint bits);
				LargeInteger(const LargeInteger & src);
				LargeInteger(LargeInteger && src);
				LargeInteger(const void * data, uint size);
				LargeInteger(Storage::RegistryNode * reg, const widechar * value);
				~LargeInteger(void);

				bool InitWithRandomPrime(uint num_bits, uint num_tests, uint max_trys);
				void InitWithRandom(uint num_bits);
				void InitWithRandom(const LargeInteger & min, const LargeInteger & max);
				void InitWithZero(void);
				void InitWithString(const string & decimal);

				LargeInteger & operator = (const LargeInteger & src);
				LargeInteger & operator = (LargeInteger && src);
				
				int DWordLength(void) const;
				int BitLength(void) const;
				int EffectiveBitLength(void) const;
				uint32 * Data(void);
				const uint32 * Data(void) const;
				void Store(Storage::RegistryNode * reg, const widechar * value);
				LargeInteger Resize(uint bits) const;
				string ToString(void) const;
			};

			enum class GeneratorTaskStatus { Success, NotFound, Cancelled, Failed, Incomplete };
			class IGeneratorTask : public Object
			{
			public:
				virtual void Cancel(void) noexcept = 0;
				virtual GeneratorTaskStatus Status(void) noexcept = 0;
				virtual const LargeInteger * Number(void) noexcept = 0;
			};

			int LongCompare(const LargeInteger & value, const LargeInteger & to);

			void LongAdd(LargeInteger & to, const LargeInteger & value);
			void LongAdd(LargeInteger & to, uint32 value);
			void LongAdd(LargeInteger & to, const LargeInteger & value, const LargeInteger & modulus);
			void LongAdd(LargeInteger & to, uint32 value, const LargeInteger & modulus);

			void LongSubtract(LargeInteger & to, const LargeInteger & value);
			void LongSubtract(LargeInteger & to, uint32 value);

			void LongShiftLeft(LargeInteger & out, const LargeInteger & value, uint32 by);
			void LongShiftLeft(LargeInteger & out, const LargeInteger & value, const LargeInteger & modulus);
			void LongShiftRight(LargeInteger & out, const LargeInteger & value, uint32 by);

			void LongMultiply(LargeInteger & out, const LargeInteger & value, uint32 by);
			void LongMultiply(LargeInteger & out, const LargeInteger & value, const LargeInteger & by);
			void LongMultiply(LargeInteger & out, const LargeInteger & value, const LargeInteger & by, const LargeInteger & modulus);

			void LongEuclidianAlgorithm(LargeInteger & a, LargeInteger & b, LargeInteger & gcd, const LargeInteger & x, const LargeInteger & y);
			void LongInverse(LargeInteger & out, const LargeInteger & value, const LargeInteger & modulus);
			void LongPower(LargeInteger & out, const LargeInteger & value, const LargeInteger & by, const LargeInteger & modulus);
			void LongDivide(LargeInteger & div, LargeInteger & mod, const LargeInteger & value, const LargeInteger & by);

			bool LongDivides(const LargeInteger & value, uint32 by);
			bool LongMillerRabinTest(const LargeInteger & value, uint num_tests, volatile bool * cancel_flag = 0);

			IGeneratorTask * LongGenerateRandomPrime(uint num_bits, uint num_tests, uint max_trys, IDispatchQueue * task, IDispatchQueue * dispatch, IDispatchTask * completion);
		}
	}
}