#include "xe_sec_arithm.h"

#include <new>

namespace Engine
{
	namespace XE
	{
		namespace Security
		{
			LargeInteger::LargeInteger(void) : _data(0), _num_dwords(0) {}
			LargeInteger::LargeInteger(uint bits) : _num_dwords((bits + 31) / 32)
			{
				if (_num_dwords) {
					_data = new (std::nothrow) uint32[_num_dwords];
					if (!_data) throw OutOfMemoryException();
				} else _data = 0;
			}
			LargeInteger::LargeInteger(const LargeInteger & src) : _num_dwords(src._num_dwords)
			{
				if (_num_dwords) {
					_data = new (std::nothrow) uint32[_num_dwords];
					if (!_data) throw OutOfMemoryException();
					MemoryCopy(_data, src._data, _num_dwords * 4);
				} else _data = 0;
			}
			LargeInteger::LargeInteger(LargeInteger && src) : _num_dwords(src._num_dwords), _data(src._data) { src._data = 0; src._num_dwords = 0; }
			LargeInteger::LargeInteger(const void * data, uint size)
			{
				if (size) {
					_num_dwords = size / 4;
					if (size % 4) _num_dwords++;
					_data = new (std::nothrow) uint32[_num_dwords];
					if (!_data) throw OutOfMemoryException();
					ZeroMemory(_data, _num_dwords * 4);
					MemoryCopy(_data, data, size);
				} else { _num_dwords = 0; _data = 0; }
			}
			LargeInteger::LargeInteger(Storage::RegistryNode * reg, const widechar * value)
			{
				uint size = reg->GetValueBinarySize(value);
				if (size) {
					_num_dwords = size / 4;
					if (size % 4) _num_dwords++;
					_data = new (std::nothrow) uint32[_num_dwords];
					if (!_data) throw OutOfMemoryException();
					ZeroMemory(_data, _num_dwords * 4);
					try { reg->GetValueBinary(value, _data); } catch (...) { delete[] _data; throw; }
				} else { _num_dwords = 0; _data = 0; }
			}
			LargeInteger::~LargeInteger(void) { if (_data) delete[] _data; }
			bool LargeInteger::InitWithRandomPrime(uint num_bits, uint num_tests, uint max_trys)
			{
				InitWithRandom(num_bits);
				if (_data) _data[0] |= 1U;
				uint step = 2;
				for (uint t = 0; t < max_trys; t++) {
					if (step != 6 && LongDivides(*this, 3)) { LongAdd(*this, step); step = 6; }
					if (!LongDivides(*this, 5) && !LongDivides(*this, 7) && !LongDivides(*this, 11) && !LongDivides(*this, 13) &&
						!LongDivides(*this, 17) && !LongDivides(*this, 19) && !LongDivides(*this, 23) && !LongDivides(*this, 29) &&
						!LongDivides(*this, 31) && !LongDivides(*this, 37) && !LongDivides(*this, 41) && !LongDivides(*this, 43)) {
						if (LongMillerRabinTest(*this, num_tests)) return true;
					}
					LongAdd(*this, step);
				}
				return false;
			}
			void LargeInteger::InitWithRandom(uint num_bits)
			{
				if (!num_bits) return;
				if (num_bits > uint(BitLength())) throw InvalidArgumentException();
				SafePointer<DataBlock> rnd = Cryptography::CreateSecureRandom((num_bits - 1) / 8 + 4);
				int final_dword = (num_bits - 1) / 32;
				uint final_set = 1 << ((num_bits - 1) % 32);
				uint final_mask = final_set;
				while (true) {
					auto mask_new = final_mask | (final_mask >> 1);
					if (mask_new == final_mask) break;
					final_mask = mask_new;
				}
				MemoryCopy(_data, rnd->GetBuffer(), min(final_dword + 1, _num_dwords) * 4);
				_data[final_dword] &= final_mask;
				_data[final_dword] |= final_set;
				for (int i = final_dword + 1; i < _num_dwords; i++) _data[i] = 0;
			}
			void LargeInteger::InitWithRandom(const LargeInteger & min, const LargeInteger & max)
			{
				InitWithRandom(max.EffectiveBitLength());
				if (LongCompare(*this, min) < 0) LongAdd(*this, min);
				else if (LongCompare(*this, max) >= 0) LongSubtract(*this, max);
			}
			void LargeInteger::InitWithZero(void) { if (_data) ZeroMemory(_data, _num_dwords * 4); }
			void LargeInteger::InitWithString(const string & decimal)
			{
				InitWithZero();
				LargeInteger acc(*this);
				int length = decimal.Length();
				for (int i = 0; i < length; i++) {
					uint d = decimal[i] - L'0';
					if (d >= 10) throw InvalidFormatException();
					LongMultiply(acc, *this, 10);
					LongAdd(acc, d);
					MemoryCopy(_data, acc._data, _num_dwords * 4);
				}
			}
			LargeInteger & LargeInteger::operator = (const LargeInteger & src)
			{
				if (this == &src) return *this;
				if (src._num_dwords) {
					auto data = new (std::nothrow) uint32[src._num_dwords];
					if (!data) throw OutOfMemoryException();
					if (_data) delete[] _data;
					_data = data; _num_dwords = src._num_dwords;
					MemoryCopy(_data, src._data, _num_dwords * 4);
				} else {
					if (_data) delete[] _data;
					_data = 0; _num_dwords = 0;
				}
				return *this;
			}
			LargeInteger & LargeInteger::operator = (LargeInteger && src)
			{
				if (this == &src) return *this;
				if (_data) delete[] _data;
				_data = src._data; _num_dwords = src._num_dwords;
				src._data = 0; src._num_dwords = 0;
				return *this;
			}
			int LargeInteger::DWordLength(void) const { return _num_dwords; }
			int LargeInteger::BitLength(void) const { return _num_dwords * 32; }
			int LargeInteger::EffectiveBitLength(void) const
			{
				uint rv = _num_dwords * 32;
				while (rv) {
					auto rvd = rv - 1;
					auto bit = (_data[rvd / 32] >> (rvd % 32)) & 1;
					if (bit) return rv;
					rv = rvd;
				}
				return rv;
			}
			uint32 * LargeInteger::Data(void) { return _data; }
			const uint32 * LargeInteger::Data(void) const { return _data; }
			void LargeInteger::Store(Storage::RegistryNode * reg, const widechar * value) { reg->SetValue(value, _data, _num_dwords * 4); }
			LargeInteger LargeInteger::Resize(uint bits) const
			{
				LargeInteger result(bits);
				result.InitWithZero();
				MemoryCopy(result._data, _data, min(result._num_dwords, _num_dwords) * 4);
				return result;
			}
			string LargeInteger::ToString(void) const
			{
				DynamicString out;
				char decimal = 10;
				LargeInteger self(*this), den(&decimal, 1), div, mod, zero;
				while (LongCompare(self, zero) > 0) {
					LongDivide(div, mod, self, den);
					self = div;
					out << (L'0' + mod.Data()[0]);
				}
				auto len = out.Length();
				auto hl = len / 2;
				for (int i = 0; i < hl; i++) swap(out[i], out[len - i - 1]);
				return out.ToString();
			}

			int LongCompare(const LargeInteger & value, const LargeInteger & to)
			{
				if (value.DWordLength() > to.DWordLength()) {
					for (int i = value.DWordLength() - 1; i >= to.DWordLength(); i--) if (value.Data()[i]) return 1;
				} else if (value.DWordLength() > to.DWordLength()) {
					for (int i = to.DWordLength() - 1; i >= value.DWordLength(); i--) if (to.Data()[i]) return -1;
				}
				for (int i = min(value.DWordLength(), to.DWordLength()) - 1; i >= 0; i--) {
					if (value.Data()[i] > to.Data()[i]) return 1;
					else if (value.Data()[i] < to.Data()[i]) return -1;
				}
				return 0;
			}
			void LongAdd(LargeInteger & to, const LargeInteger & value)
			{
				auto mn = min(to.DWordLength(), value.DWordLength());
				auto mx = to.DWordLength();
				uint32 cr = 0;
				for (int i = 0; i < mn; i++) {
					uint64 sum = uint64(to.Data()[i]) + uint64(value.Data()[i]) + uint64(cr);
					to.Data()[i] = sum;
					cr = sum >> 32;
				}
				for (int i = mn; i < mx; i++) {
					if (!cr) break;
					uint64 sum = uint64(to.Data()[i]) + uint64(cr);
					to.Data()[i] = sum;
					cr = sum >> 32;
				}
			}
			void LongAdd(LargeInteger & to, uint32 value)
			{
				auto mx = to.DWordLength();
				uint32 cr = value;
				for (int i = 0; i < mx; i++) {
					if (!cr) break;
					uint64 sum = uint64(to.Data()[i]) + uint64(cr);
					to.Data()[i] = sum;
					cr = sum >> 32;
				}
			}
			void LongAdd(LargeInteger & to, const LargeInteger & value, const LargeInteger & modulus)
			{
				LongAdd(to, value);
				if (LongCompare(to, modulus) >= 0) LongSubtract(to, modulus);
			}
			void LongAdd(LargeInteger & to, uint32 value, const LargeInteger & modulus)
			{
				LongAdd(to, value);
				if (LongCompare(to, modulus) >= 0) LongSubtract(to, modulus);
			}
			void LongSubtract(LargeInteger & to, const LargeInteger & value)
			{
				auto mn = min(to.DWordLength(), value.DWordLength());
				auto mx = to.DWordLength();
				uint32 cr = 0;
				for (int i = 0; i < mn; i++) {
					uint64 sum = 0x100000000ULL + uint64(to.Data()[i]) - uint64(value.Data()[i]) - uint64(cr);
					to.Data()[i] = sum;
					cr = (sum & 0x100000000ULL) ? 0 : 1;
				}
				for (int i = mn; i < mx; i++) {
					if (!cr) break;
					uint64 sum = 0x100000000ULL + uint64(to.Data()[i]) - uint64(cr);
					to.Data()[i] = sum;
					cr = (sum & 0x100000000ULL) ? 0 : 1;
				}
			}
			void LongSubtract(LargeInteger & to, uint32 value)
			{
				auto mx = to.DWordLength();
				uint32 cr = value;
				for (int i = 0; i < mx; i++) {
					if (!cr) break;
					uint64 sum = 0x100000000ULL + uint64(to.Data()[i]) - uint64(cr);
					to.Data()[i] = sum;
					cr = (sum & 0x100000000ULL) ? 0 : 1;
				}
			}
			void LongShiftLeft(LargeInteger & out, const LargeInteger & value, uint32 by)
			{
				if (by % 32) {
					int bias = by / 32 + 1;
					int shl = by % 32;
					uint mask_1 = 0xFFFFFFFFU >> uint(shl);
					uint mask_0 = 0xFFFFFFFFU << uint(32 - shl);
					for (int i = 0; i < out.DWordLength(); i++) {
						uint o = 0;
						int j = i - bias;
						if (j >= 0 && j < value.DWordLength()) o |= (value.Data()[j] & mask_0) >> uint(32 - shl);
						if (j + 1 >= 0 && j + 1 < value.DWordLength()) o |= (value.Data()[j + 1] & mask_1) << shl;
						out.Data()[i] = o;
					}
				} else {
					int dword_by = by / 32;
					for (int i = 0; i < min(dword_by, out.DWordLength()); i++) out.Data()[i] = 0;
					for (int i = dword_by; i < out.DWordLength(); i++) {
						int j = i - dword_by;
						if (j < value.DWordLength()) out.Data()[i] = value.Data()[j];
						else out.Data()[i] = 0;
					}
				}
			}
			void LongShiftLeft(LargeInteger & out, const LargeInteger & value, const LargeInteger & modulus)
			{
				LongShiftLeft(out, value, 1U);
				if (LongCompare(out, modulus) >= 0) LongSubtract(out, modulus);
			}
			void LongShiftRight(LargeInteger & out, const LargeInteger & value, uint32 by)
			{
				if (by % 32) {
					int bias = by / 32 + 1;
					int shl = by % 32;
					uint mask_1 = 0xFFFFFFFFU << uint(shl);
					uint mask_0 = 0xFFFFFFFFU >> uint(32 - shl);
					for (int i = 0; i < out.DWordLength(); i++) {
						uint o = 0;
						int j = i + bias;
						if (j >= 0 && j < value.DWordLength()) o |= (value.Data()[j] & mask_0) << uint(32 - shl);
						if (j - 1 >= 0 && j - 1 < value.DWordLength()) o |= (value.Data()[j - 1] & mask_1) >> shl;
						out.Data()[i] = o;
					}
				} else {
					int dword_by = by / 32;
					for (int i = 0; i < out.DWordLength() - dword_by; i++) {
						int j = i + dword_by;
						if (j < value.DWordLength()) out.Data()[i] = value.Data()[j];
						else out.Data()[i] = 0;
					}
					for (int i = out.DWordLength() - dword_by; i < out.DWordLength(); i++) out.Data()[i] = 0;
				}
			}
			void LongMultiply(LargeInteger & out, const LargeInteger & value, uint32 by)
			{
				uint32 cr = 0;
				for (int i = 0; i < out.DWordLength(); i++) {
					uint32 in = i < value.DWordLength() ? value.Data()[i] : 0;
					uint64 sum = uint64(in) * uint64(by) + uint64(cr);
					out.Data()[i] = sum;
					cr = sum >> 32;
				}
			}
			void LongMultiply(LargeInteger & out, const LargeInteger & value, const LargeInteger & by)
			{
				out.InitWithZero();
				LargeInteger vs(out), vsm(out);
				MemoryCopy(vs.Data(), value.Data(), min(out.DWordLength(), value.DWordLength()) * 4);
				for (int i = 0; i < by.DWordLength(); i++) {
					LongMultiply(vsm, vs, by.Data()[i]);
					LongAdd(out, vsm);
					if (i + 1 < by.DWordLength()) { LongShiftLeft(vsm, vs, 32); vs = vsm; }
				}
			}
			void LongMultiply(LargeInteger & out, const LargeInteger & value, const LargeInteger & by, const LargeInteger & modulus)
			{
				out.InitWithZero();
				LargeInteger acc(out);
				for (uint k = by.EffectiveBitLength(); k > 0; k--) {
					uint i = k - 1;
					uint j = i / 32;
					uint bw = j < by.DWordLength() ? by.Data()[j] : 0;
					LargeInteger & swap0 = (i & 1) ? acc : out;
					LargeInteger & swap1 = (i & 1) ? out : acc;
					if (k < by.EffectiveBitLength()) LongShiftLeft(swap0, swap1, modulus);
					if (bw & (1U << (i % 32))) LongAdd(swap0, value, modulus);
				}
			}
			void LongEuclidianAlgorithm(LargeInteger & a, LargeInteger & b, LargeInteger & gcd, const LargeInteger & x, const LargeInteger & y)
			{
				char data[] = { 0, 1 };
				uint bits = max(x.EffectiveBitLength(), y.EffectiveBitLength());
				LargeInteger zerum;
				LargeInteger r0 = x.Resize(bits);
				LargeInteger s0 = LargeInteger(&data[1], 1).Resize(bits);
				LargeInteger t0 = LargeInteger(&data[0], 1).Resize(bits);
				LargeInteger r1 = y.Resize(bits);
				LargeInteger s1 = LargeInteger(&data[0], 1).Resize(bits);
				LargeInteger t1 = LargeInteger(&data[1], 1).Resize(bits);
				LargeInteger * rf = &r0, * sf = &s0, * tf = &t0;
				LargeInteger * r = &r1, * s = &s1, * t = &t1;
				while (true) {
					LargeInteger q, ri;
					LongDivide(q, ri, *rf, *r);
					if (LongCompare(ri, zerum) > 0) {
						*rf = ri;
						LongMultiply(ri, q, *s);
						LongSubtract(*sf, ri);
						LongMultiply(ri, q, *t);
						LongSubtract(*tf, ri);
					} else break;
					swap(r, rf); swap(s, sf); swap(t, tf);
				}
				gcd = *r; a = *s; b = *t;
			}
			void LongInverse(LargeInteger & out, const LargeInteger & value, const LargeInteger & modulus)
			{
				LargeInteger unus, k;
				LongEuclidianAlgorithm(out, k, unus, value, modulus);
				while (LongCompare(out, modulus) >= 0) LongAdd(out, modulus);
			}
			void LongPower(LargeInteger & out, const LargeInteger & value, const LargeInteger & by, const LargeInteger & modulus)
			{
				out.InitWithZero();
				if (out.Data()) out.Data()[0] = 1;
				int length = by.EffectiveBitLength();
				LargeInteger xpw(value);
				LargeInteger acc(out);
				for (int i = 0; i < length; i++) {
					if (by.Data()[i / 32] & (1U << (i % 32))) {
						LongMultiply(acc, out, xpw, modulus);
						out = acc;
					}
					LongMultiply(acc, xpw, xpw, modulus);
					xpw = acc;
				}
			}
			void LongDivide(LargeInteger & div, LargeInteger & mod, const LargeInteger & value, const LargeInteger & by)
			{
				div = mod = value;
				LargeInteger sub(value);
				div.InitWithZero();
				int d = value.EffectiveBitLength() - by.EffectiveBitLength();
				if (d < 0) return;
				for (uint k = d + 1; k > 0; k--) {
					uint i = k - 1;
					LongShiftLeft(sub, by, i);
					if (LongCompare(mod, sub) >= 0) {
						LongSubtract(mod, sub);
						div.Data()[i / 32] |= 1U << (i % 32);
					}
				}
			}
			bool LongDivides(const LargeInteger & value, uint32 by)
			{
				uint32 cr = 0;
				for (int i = value.DWordLength() - 1; i >= 0; i--) {
					uint64 num = (uint64(cr) << 32ULL) | uint64(value.Data()[i]);
					cr = num % by;
				}
				return cr == 0;
			}
			bool LongMillerRabinTest(const LargeInteger & value, uint num_tests, volatile bool * cancel_flag)
			{
				char data[] = { 1, 2 };
				uint s = 0;
				uint bitwidth = value.EffectiveBitLength() + 1;
				LargeInteger one(&data[0], 1);
				LargeInteger two(&data[1], 1);
				LargeInteger n1 = value.Resize(bitwidth);
				LongSubtract(n1, 1);
				LargeInteger d(n1);
				while (!(d.Data()[0] & 1)) { LargeInteger d2(d); LongShiftRight(d2, d, 1); swap(d, d2); s++; }
				LargeInteger a(bitwidth), x(bitwidth), y(bitwidth);
				for (uint t = 0; t < num_tests; t++) {
					if (cancel_flag && *cancel_flag) return false;
					a.InitWithRandom(two, n1);
					LongPower(x, a, d, value);
					for (uint i = 0; i < s; i++) {
						if (cancel_flag && *cancel_flag) return false;
						LongMultiply(y, x, x, value);
						if (LongCompare(y, one) == 0 && LongCompare(x, one) != 0 && LongCompare(x, n1) != 0) return false;
						x = y;
					}
					if (LongCompare(y, one) != 0) return false;
				}
				return true;
			}
			
			class GeneratorTask : public IGeneratorTask
			{
				volatile bool _cancelled;
				GeneratorTaskStatus _status;
				LargeInteger _result;
				SafePointer<Semaphore> _sync;
				uint _num_bits, _num_tests, _num_trys, _num_tasks;
				SafePointer<IDispatchQueue> _test_queue;
				SafePointer<IDispatchQueue> _event_queue;
				SafePointer<IDispatchTask> _on_completion;
			private:
				void _end_generation(GeneratorTaskStatus status) noexcept
				{
					_sync->Wait();
					_cancelled = true;
					if (_status == GeneratorTaskStatus::Incomplete) {
						_status = status;
						_event_queue->SubmitTask(_on_completion);
					}
					_sync->Open();
				}
			public:
				GeneratorTask(uint num_bits, uint num_tests, uint max_trys, IDispatchQueue * task, IDispatchQueue * dispatch, IDispatchTask * completion)
				{
					_cancelled = false; _status = GeneratorTaskStatus::Incomplete; _sync = CreateSemaphore(1);
					if (!_sync) throw OutOfMemoryException();
					_num_bits = num_bits; _num_tests = num_tests; _num_trys = max_trys;
					_test_queue.SetRetain(task); _event_queue.SetRetain(dispatch); _on_completion.SetRetain(completion);
					_num_tasks = _num_trys;
				}
				virtual ~GeneratorTask(void) override {}
				virtual void Cancel(void) noexcept override { _end_generation(GeneratorTaskStatus::Cancelled); }
				virtual GeneratorTaskStatus Status(void) noexcept override
				{
					_sync->Wait();
					auto result = _status;
					_sync->Open();
					return result;
				}
				virtual const LargeInteger * Number(void) noexcept override
				{
					_sync->Wait();
					auto result = _status == GeneratorTaskStatus::Success ? &_result : 0;
					_sync->Open();
					return result;
				}
				void Launch(void)
				{
					SafePointer<GeneratorTask> self;
					self.SetRetain(this);
					_test_queue->SubmitTask(CreateFunctionalTask([self]() {
						try {
							LargeInteger iteration(self->_num_bits);
							iteration.InitWithRandom(self->_num_bits);
							if (self->_num_bits) iteration.Data()[0] |= 1U;
							uint step = 2;
							while (self->_num_trys) {
								if (step != 6 && LongDivides(iteration, 3)) { LongAdd(iteration, step); step = 6; }
								bool small_compound =
									LongDivides(iteration, 5) || LongDivides(iteration, 7) || LongDivides(iteration, 11) || LongDivides(iteration, 13) ||
									LongDivides(iteration, 17) || LongDivides(iteration, 19) || LongDivides(iteration, 23) || LongDivides(iteration, 29) ||
									LongDivides(iteration, 31) || LongDivides(iteration, 37) || LongDivides(iteration, 41) || LongDivides(iteration, 43);
								if (!small_compound) {
									self->_test_queue->SubmitTask(CreateFunctionalTask([self, i = iteration]() {
										try {
											bool prime = LongMillerRabinTest(i, self->_num_tests, &self->_cancelled);
											if (prime) {
												self->_sync->Wait();
												try {
													self->_cancelled = true;
													self->_result = i;
													if (self->_status == GeneratorTaskStatus::Incomplete) {
														self->_status = GeneratorTaskStatus::Success;
														self->_event_queue->SubmitTask(self->_on_completion);
													}
												} catch (...) { self->_sync->Open(); throw; }
												self->_sync->Open();
											} else if (!InterlockedDecrement(self->_num_tasks)) {
												self->_end_generation(GeneratorTaskStatus::NotFound);
											}
										} catch (...) { self->_end_generation(GeneratorTaskStatus::Failed); }
									}));
									self->_num_trys--;
								}
								LongAdd(iteration, step);
							}
						} catch (...) { self->_end_generation(GeneratorTaskStatus::Failed); }
					}));
				}
			};
			IGeneratorTask * LongGenerateRandomPrime(uint num_bits, uint num_tests, uint max_trys, IDispatchQueue * task, IDispatchQueue * dispatch, IDispatchTask * completion)
			{
				SafePointer<GeneratorTask> result = new (std::nothrow) GeneratorTask(num_bits, num_tests, max_trys, task, dispatch, completion);
				if (!result) throw OutOfMemoryException();
				result->Launch();
				result->Retain();
				return result;
			}
		}
	}
}