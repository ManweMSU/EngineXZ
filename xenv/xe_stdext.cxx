#include "xe_stdext.h"

#include "xe_logger.h"
#include "xe_interfaces.h"

namespace Engine
{
	namespace XE
	{
		class FPU : public IAPIExtension
		{
			static float _neg_32(float & self) { return -self; }
			static double _neg_64(double & self) { return -self; }
			static bool _not_32(float & self) { return !self; }
			static bool _not_64(double & self) { return !self; }
			static float & _asgn_add_32(float & self, float value) { self += value; return self; }
			static double & _asgn_add_64(double & self, double value) { self += value; return self; }
			static float & _asgn_sub_32(float & self, float value) { self -= value; return self; }
			static double & _asgn_sub_64(double & self, double value) { self -= value; return self; }
			static float & _asgn_mul_32(float & self, float value) { self *= value; return self; }
			static double & _asgn_mul_64(double & self, double value) { self *= value; return self; }
			static float & _asgn_div_32(float & self, float value) { self /= value; return self; }
			static double & _asgn_div_64(double & self, double value) { self /= value; return self; }
			static float & _inc_32(float & self) { self += 1.0f; return self; }
			static double & _inc_64(double & self) { self += 1.0; return self; }
			static float & _dec_32(float & self) { self -= 1.0f; return self; }
			static double & _dec_64(double & self) { self -= 1.0; return self; }
			static float _add_32(float a, float b) { return a + b; }
			static double _add_64(double a, double b) { return a + b; }
			static float _sub_32(float a, float b) { return a - b; }
			static double _sub_64(double a, double b) { return a - b; }
			static float _mul_32(float a, float b) { return a * b; }
			static double _mul_64(double a, double b) { return a * b; }
			static float _div_32(float a, float b) { return a / b; }
			static double _div_64(double a, double b) { return a / b; }
			static bool _eq_32(float a, float b) { return a == b; }
			static bool _eq_64(double a, double b) { return a == b; }
			static bool _neq_32(float a, float b) { return a != b; }
			static bool _neq_64(double a, double b) { return a != b; }
			static bool _ls_32(float a, float b) { return a < b; }
			static bool _ls_64(double a, double b) { return a < b; }
			static bool _gt_32(float a, float b) { return a > b; }
			static bool _gt_64(double a, double b) { return a > b; }
			static bool _le_32(float a, float b) { return a <= b; }
			static bool _le_64(double a, double b) { return a <= b; }
			static bool _ge_32(float a, float b) { return a >= b; }
			static bool _ge_64(double a, double b) { return a >= b; }
			static float _b_f32(bool & self) { return self ? 1.0f : 0.0f; }
			static double _b_f64(bool & self) { return self ? 1.0 : 0.0; }
			static float _s8_f32(int8 & self) { return self; }
			static float _u8_f32(uint8 & self) { return self; }
			static double _s8_f64(int8 & self) { return self; }
			static double _u8_f64(uint8 & self) { return self; }
			static float _s16_f32(int16 & self) { return self; }
			static float _u16_f32(uint16 & self) { return self; }
			static double _s16_f64(int16 & self) { return self; }
			static double _u16_f64(uint16 & self) { return self; }
			static float _s32_f32(int32 & self) { return self; }
			static float _u32_f32(uint32 & self) { return self; }
			static double _s32_f64(int32 & self) { return self; }
			static double _u32_f64(uint32 & self) { return self; }
			static float _s64_f32(int64 & self) { return self; }
			static float _u64_f32(uint64 & self) { return self; }
			static double _s64_f64(int64 & self) { return self; }
			static double _u64_f64(uint64 & self) { return self; }
			static float _sw_f32(sintptr & self) { return self; }
			static float _uw_f32(uintptr & self) { return self; }
			static double _sw_f64(sintptr & self) { return self; }
			static double _uw_f64(uintptr & self) { return self; }
			static bool _f32_b(float & self) { return self; }
			static int8 _f32_s8(float & self) { return self; }
			static uint8 _f32_u8(float & self) { return self; }
			static int16 _f32_s16(float & self) { return self; }
			static uint16 _f32_u16(float & self) { return self; }
			static int32 _f32_s32(float & self) { return self; }
			static uint32 _f32_u32(float & self) { return self; }
			static int64 _f32_s64(float & self) { return self; }
			static uint64 _f32_u64(float & self) { return self; }
			static sintptr _f32_sw(float & self) { return self; }
			static uintptr _f32_uw(float & self) { return self; }
			static bool _f64_b(double & self) { return self; }
			static int8 _f64_s8(double & self) { return self; }
			static uint8 _f64_u8(double & self) { return self; }
			static int16 _f64_s16(double & self) { return self; }
			static uint16 _f64_u16(double & self) { return self; }
			static int32 _f64_s32(double & self) { return self; }
			static uint32 _f64_u32(double & self) { return self; }
			static int64 _f64_s64(double & self) { return self; }
			static uint64 _f64_u64(double & self) { return self; }
			static sintptr _f64_sw(double & self) { return self; }
			static uintptr _f64_uw(double & self) { return self; }
			static double _f32_f64(float & self) { return self; }
			static float _f64_f32(double & self) { return self; }
			static int8 _abs_i8(int8 value) { return value >= 0 ? value : -value; }
			static int16 _abs_i16(int16 value) { return value >= 0 ? value : -value; }
			static int32 _abs_i32(int32 value) { return value >= 0 ? value : -value; }
			static int64 _abs_i64(int64 value) { return value >= 0 ? value : -value; }
			static sintptr _abs_iw(sintptr value) { return value >= 0 ? value : -value; }
			static float _abs_f32(float value) { return value >= 0.0f ? value : -value; }
			static double _abs_f64(double value) { return value >= 0.0 ? value : -value; }
			static int8 _sgn_i8(int8 value) { return value > 0 ? 1 : value < 0 ? -1 : 0; }
			static int16 _sgn_i16(int16 value) { return value > 0 ? 1 : value < 0 ? -1 : 0; }
			static int32 _sgn_i32(int32 value) { return value > 0 ? 1 : value < 0 ? -1 : 0; }
			static int64 _sgn_i64(int64 value) { return value > 0 ? 1 : value < 0 ? -1 : 0; }
			static sintptr _sgn_iw(sintptr value) { return value > 0 ? 1 : value < 0 ? -1 : 0; }
			static float _sgn_f32(float value) { return value > 0.0f ? 1.0f : value < 0.0f ? -1.0f : 0.0f; }
			static double _sgn_f64(double value) { return value > 0.0 ? 1.0 : value < 0.0 ? -1.0 : 0.0; }
			static float _ctg_f32(float value) { return 1.0f / tanf(value); }
			static double _ctg_f64(double value) { return 1.0 / tan(value); }
			static float _arcctg_f32(float value) { return ENGINE_PI / 2.0 - atanf(value); }
			static double _arcctg_f64(double value) { return ENGINE_PI / 2.0 - atan(value); }
			static float _inf_pos_f32() { return INFINITY; }
			static double _inf_pos_f64() { return INFINITY; }
			static float _inf_neg_f32() { return -INFINITY; }
			static double _inf_neg_f64() { return -INFINITY; }
			static float _inf_nan_f32() { return NAN; }
			static double _inf_nan_f64() { return NAN; }
			static bool _is_inf_f32(float value) { return isinf(value); }
			static bool _is_inf_f64(double value) { return isinf(value); }
			static bool _is_nan_f32(float value) { return isnan(value); }
			static bool _is_nan_f64(double value) { return isnan(value); }
			static void _random_data(uint8 * dest, int length) { for (int i = 0; i < length; i++) dest[i] = Engine::Math::Random::RandomByte(); }
			static int _random_int(int min, int max) { return min + Engine::Math::Random::RandomInteger() % (max + 1 - min); }
			static double _round_d(double value) { return round(value); }
			static double _trunc_d(double value) { return trunc(value); }
			static double _floor_d(double value) { return floor(value); }
			static double _ceil_d(double value) { return ceil(value); }
			static double _sqrt_d(double value) { return sqrt(value); }
			static double _pow_d(double value, double p) { return pow(value, p); }
			static double _exp_d(double value) { return exp(value); }
			static double _log_d(double value) { return log(value); }
			static double _log2_d(double value) { return log2(value); }
			static double _log10_d(double value) { return log10(value); }
			static double _sin_d(double value) { return sin(value); }
			static double _cos_d(double value) { return cos(value); }
			static double _tan_d(double value) { return tan(value); }
			static double _asin_d(double value) { return asin(value); }
			static double _acos_d(double value) { return acos(value); }
			static double _atan_d(double value) { return atan(value); }

			void * _expose_part_1(const string & routine_name) noexcept
			{
				if (routine_name == L"fpu_32_neg") return const_cast<void *>(reinterpret_cast<const void *>(&_neg_32));
				else if (routine_name == L"fpu_64_neg") return const_cast<void *>(reinterpret_cast<const void *>(&_neg_64));
				else if (routine_name == L"fpu_32_non") return const_cast<void *>(reinterpret_cast<const void *>(&_not_32));
				else if (routine_name == L"fpu_64_non") return const_cast<void *>(reinterpret_cast<const void *>(&_not_64));
				else if (routine_name == L"fpu_32_lsm") return const_cast<void *>(reinterpret_cast<const void *>(&_asgn_add_32));
				else if (routine_name == L"fpu_64_lsm") return const_cast<void *>(reinterpret_cast<const void *>(&_asgn_add_64));
				else if (routine_name == L"fpu_32_lsb") return const_cast<void *>(reinterpret_cast<const void *>(&_asgn_sub_32));
				else if (routine_name == L"fpu_64_lsb") return const_cast<void *>(reinterpret_cast<const void *>(&_asgn_sub_64));
				else if (routine_name == L"fpu_32_lml") return const_cast<void *>(reinterpret_cast<const void *>(&_asgn_mul_32));
				else if (routine_name == L"fpu_64_lml") return const_cast<void *>(reinterpret_cast<const void *>(&_asgn_mul_64));
				else if (routine_name == L"fpu_32_ldv") return const_cast<void *>(reinterpret_cast<const void *>(&_asgn_div_32));
				else if (routine_name == L"fpu_64_ldv") return const_cast<void *>(reinterpret_cast<const void *>(&_asgn_div_64));
				else if (routine_name == L"fpu_32_inc") return const_cast<void *>(reinterpret_cast<const void *>(&_inc_32));
				else if (routine_name == L"fpu_64_inc") return const_cast<void *>(reinterpret_cast<const void *>(&_inc_64));
				else if (routine_name == L"fpu_32_dec") return const_cast<void *>(reinterpret_cast<const void *>(&_dec_32));
				else if (routine_name == L"fpu_64_dec") return const_cast<void *>(reinterpret_cast<const void *>(&_dec_64));
				else if (routine_name == L"fpu_32_sum") return const_cast<void *>(reinterpret_cast<const void *>(&_add_32));
				else if (routine_name == L"fpu_64_sum") return const_cast<void *>(reinterpret_cast<const void *>(&_add_64));
				else if (routine_name == L"fpu_32_sub") return const_cast<void *>(reinterpret_cast<const void *>(&_sub_32));
				else if (routine_name == L"fpu_64_sub") return const_cast<void *>(reinterpret_cast<const void *>(&_sub_64));
				else if (routine_name == L"fpu_32_mul") return const_cast<void *>(reinterpret_cast<const void *>(&_mul_32));
				else if (routine_name == L"fpu_64_mul") return const_cast<void *>(reinterpret_cast<const void *>(&_mul_64));
				else if (routine_name == L"fpu_32_div") return const_cast<void *>(reinterpret_cast<const void *>(&_div_32));
				else if (routine_name == L"fpu_64_div") return const_cast<void *>(reinterpret_cast<const void *>(&_div_64));
				else if (routine_name == L"fpu_32_par") return const_cast<void *>(reinterpret_cast<const void *>(&_eq_32));
				else if (routine_name == L"fpu_64_par") return const_cast<void *>(reinterpret_cast<const void *>(&_eq_64));
				else if (routine_name == L"fpu_32_npr") return const_cast<void *>(reinterpret_cast<const void *>(&_neq_32));
				else if (routine_name == L"fpu_64_npr") return const_cast<void *>(reinterpret_cast<const void *>(&_neq_64));
				else if (routine_name == L"fpu_32_min") return const_cast<void *>(reinterpret_cast<const void *>(&_ls_32));
				else if (routine_name == L"fpu_64_min") return const_cast<void *>(reinterpret_cast<const void *>(&_ls_64));
				else if (routine_name == L"fpu_32_maj") return const_cast<void *>(reinterpret_cast<const void *>(&_gt_32));
				else if (routine_name == L"fpu_64_maj") return const_cast<void *>(reinterpret_cast<const void *>(&_gt_64));
				else if (routine_name == L"fpu_32_pmn") return const_cast<void *>(reinterpret_cast<const void *>(&_le_32));
				else if (routine_name == L"fpu_64_pmn") return const_cast<void *>(reinterpret_cast<const void *>(&_le_64));
				else if (routine_name == L"fpu_32_pmj") return const_cast<void *>(reinterpret_cast<const void *>(&_ge_32));
				else if (routine_name == L"fpu_64_pmj") return const_cast<void *>(reinterpret_cast<const void *>(&_ge_64));
				else if (routine_name == L"fpu_l_ad_f32") return const_cast<void *>(reinterpret_cast<const void *>(&_b_f32));
				else if (routine_name == L"fpu_l_ad_f64") return const_cast<void *>(reinterpret_cast<const void *>(&_b_f64));
				else if (routine_name == L"fpu_i8_ad_f32") return const_cast<void *>(reinterpret_cast<const void *>(&_s8_f32));
				else if (routine_name == L"fpu_n8_ad_f32") return const_cast<void *>(reinterpret_cast<const void *>(&_u8_f32));
				else if (routine_name == L"fpu_i8_ad_f64") return const_cast<void *>(reinterpret_cast<const void *>(&_s8_f64));
				else if (routine_name == L"fpu_n8_ad_f64") return const_cast<void *>(reinterpret_cast<const void *>(&_u8_f64));
				else if (routine_name == L"fpu_i16_ad_f32") return const_cast<void *>(reinterpret_cast<const void *>(&_s16_f32));
				else if (routine_name == L"fpu_n16_ad_f32") return const_cast<void *>(reinterpret_cast<const void *>(&_u16_f32));
				else if (routine_name == L"fpu_i16_ad_f64") return const_cast<void *>(reinterpret_cast<const void *>(&_s16_f64));
				else if (routine_name == L"fpu_n16_ad_f64") return const_cast<void *>(reinterpret_cast<const void *>(&_u16_f64));
				else if (routine_name == L"fpu_i32_ad_f32") return const_cast<void *>(reinterpret_cast<const void *>(&_s32_f32));
				else if (routine_name == L"fpu_n32_ad_f32") return const_cast<void *>(reinterpret_cast<const void *>(&_u32_f32));
				else if (routine_name == L"fpu_i32_ad_f64") return const_cast<void *>(reinterpret_cast<const void *>(&_s32_f64));
				else if (routine_name == L"fpu_n32_ad_f64") return const_cast<void *>(reinterpret_cast<const void *>(&_u32_f64));
				else if (routine_name == L"fpu_i64_ad_f32") return const_cast<void *>(reinterpret_cast<const void *>(&_s64_f32));
				else if (routine_name == L"fpu_n64_ad_f32") return const_cast<void *>(reinterpret_cast<const void *>(&_u64_f32));
				else if (routine_name == L"fpu_i64_ad_f64") return const_cast<void *>(reinterpret_cast<const void *>(&_s64_f64));
				else if (routine_name == L"fpu_n64_ad_f64") return const_cast<void *>(reinterpret_cast<const void *>(&_u64_f64));
				else if (routine_name == L"fpu_iadl_ad_f32") return const_cast<void *>(reinterpret_cast<const void *>(&_sw_f32));
				else if (routine_name == L"fpu_nadl_ad_f32") return const_cast<void *>(reinterpret_cast<const void *>(&_uw_f32));
				else if (routine_name == L"fpu_iadl_ad_f64") return const_cast<void *>(reinterpret_cast<const void *>(&_sw_f64));
				else if (routine_name == L"fpu_nadl_ad_f64") return const_cast<void *>(reinterpret_cast<const void *>(&_uw_f64));
				else if (routine_name == L"fpu_f32_ad_l") return const_cast<void *>(reinterpret_cast<const void *>(&_f32_b));
				else if (routine_name == L"fpu_f32_ad_i8") return const_cast<void *>(reinterpret_cast<const void *>(&_f32_s8));
				else if (routine_name == L"fpu_f32_ad_n8") return const_cast<void *>(reinterpret_cast<const void *>(&_f32_u8));
				else if (routine_name == L"fpu_f32_ad_i16") return const_cast<void *>(reinterpret_cast<const void *>(&_f32_s16));
				else if (routine_name == L"fpu_f32_ad_n16") return const_cast<void *>(reinterpret_cast<const void *>(&_f32_u16));
				else if (routine_name == L"fpu_f32_ad_i32") return const_cast<void *>(reinterpret_cast<const void *>(&_f32_s32));
				else if (routine_name == L"fpu_f32_ad_n32") return const_cast<void *>(reinterpret_cast<const void *>(&_f32_u32));
				else if (routine_name == L"fpu_f32_ad_i64") return const_cast<void *>(reinterpret_cast<const void *>(&_f32_s64));
				else if (routine_name == L"fpu_f32_ad_n64") return const_cast<void *>(reinterpret_cast<const void *>(&_f32_u64));
				else if (routine_name == L"fpu_f32_ad_iadl") return const_cast<void *>(reinterpret_cast<const void *>(&_f32_sw));
				else if (routine_name == L"fpu_f32_ad_nadl") return const_cast<void *>(reinterpret_cast<const void *>(&_f32_uw));
				else if (routine_name == L"fpu_f64_ad_l") return const_cast<void *>(reinterpret_cast<const void *>(&_f64_b));
				else if (routine_name == L"fpu_f64_ad_i8") return const_cast<void *>(reinterpret_cast<const void *>(&_f64_s8));
				else if (routine_name == L"fpu_f64_ad_n8") return const_cast<void *>(reinterpret_cast<const void *>(&_f64_u8));
				else if (routine_name == L"fpu_f64_ad_i16") return const_cast<void *>(reinterpret_cast<const void *>(&_f64_s16));
				else if (routine_name == L"fpu_f64_ad_n16") return const_cast<void *>(reinterpret_cast<const void *>(&_f64_u16));
				else if (routine_name == L"fpu_f64_ad_i32") return const_cast<void *>(reinterpret_cast<const void *>(&_f64_s32));
				else if (routine_name == L"fpu_f64_ad_n32") return const_cast<void *>(reinterpret_cast<const void *>(&_f64_u32));
				else if (routine_name == L"fpu_f64_ad_i64") return const_cast<void *>(reinterpret_cast<const void *>(&_f64_s64));
				else if (routine_name == L"fpu_f64_ad_n64") return const_cast<void *>(reinterpret_cast<const void *>(&_f64_u64));
				else if (routine_name == L"fpu_f64_ad_iadl") return const_cast<void *>(reinterpret_cast<const void *>(&_f64_sw));
				else if (routine_name == L"fpu_f64_ad_nadl") return const_cast<void *>(reinterpret_cast<const void *>(&_f64_uw));
				else if (routine_name == L"fpu_f32_ad_f64") return const_cast<void *>(reinterpret_cast<const void *>(&_f32_f64));
				else if (routine_name == L"fpu_f64_ad_f32") return const_cast<void *>(reinterpret_cast<const void *>(&_f64_f32));
				else return 0;
			}
			void * _expose_part_2(const string & routine_name) noexcept
			{
				if (routine_name == L"fpu_abs_i8") return const_cast<void *>(reinterpret_cast<const void *>(&_abs_i8));
				else if (routine_name == L"fpu_abs_i16") return const_cast<void *>(reinterpret_cast<const void *>(&_abs_i16));
				else if (routine_name == L"fpu_abs_i32") return const_cast<void *>(reinterpret_cast<const void *>(&_abs_i32));
				else if (routine_name == L"fpu_abs_i64") return const_cast<void *>(reinterpret_cast<const void *>(&_abs_i64));
				else if (routine_name == L"fpu_abs_iad") return const_cast<void *>(reinterpret_cast<const void *>(&_abs_iw));
				else if (routine_name == L"fpu_abs_f32") return const_cast<void *>(reinterpret_cast<const void *>(&_abs_f32));
				else if (routine_name == L"fpu_abs_f64") return const_cast<void *>(reinterpret_cast<const void *>(&_abs_f64));
				else if (routine_name == L"fpu_sgn_i8") return const_cast<void *>(reinterpret_cast<const void *>(&_sgn_i8));
				else if (routine_name == L"fpu_sgn_i16") return const_cast<void *>(reinterpret_cast<const void *>(&_sgn_i16));
				else if (routine_name == L"fpu_sgn_i32") return const_cast<void *>(reinterpret_cast<const void *>(&_sgn_i32));
				else if (routine_name == L"fpu_sgn_i64") return const_cast<void *>(reinterpret_cast<const void *>(&_sgn_i64));
				else if (routine_name == L"fpu_sgn_iad") return const_cast<void *>(reinterpret_cast<const void *>(&_sgn_iw));
				else if (routine_name == L"fpu_sgn_f32") return const_cast<void *>(reinterpret_cast<const void *>(&_sgn_f32));
				else if (routine_name == L"fpu_sgn_f64") return const_cast<void *>(reinterpret_cast<const void *>(&_sgn_f64));
				else if (routine_name == L"fpu_c1") return const_cast<void *>(reinterpret_cast<const void *>(&roundf));
				else if (routine_name == L"fpu_c1_d") return const_cast<void *>(reinterpret_cast<const void *>(&_round_d));
				else if (routine_name == L"fpu_c2") return const_cast<void *>(reinterpret_cast<const void *>(&truncf));
				else if (routine_name == L"fpu_c2_d") return const_cast<void *>(reinterpret_cast<const void *>(&_trunc_d));
				else if (routine_name == L"fpu_c3") return const_cast<void *>(reinterpret_cast<const void *>(&floorf));
				else if (routine_name == L"fpu_c3_d") return const_cast<void *>(reinterpret_cast<const void *>(&_floor_d));
				else if (routine_name == L"fpu_c4") return const_cast<void *>(reinterpret_cast<const void *>(&ceilf));
				else if (routine_name == L"fpu_c4_d") return const_cast<void *>(reinterpret_cast<const void *>(&_ceil_d));
				else if (routine_name == L"fpu_rdx") return const_cast<void *>(reinterpret_cast<const void *>(&sqrtf));
				else if (routine_name == L"fpu_rdx_d") return const_cast<void *>(reinterpret_cast<const void *>(&_sqrt_d));
				else if (routine_name == L"fpu_pot") return const_cast<void *>(reinterpret_cast<const void *>(&powf));
				else if (routine_name == L"fpu_pot_d") return const_cast<void *>(reinterpret_cast<const void *>(&_pow_d));
				else if (routine_name == L"fpu_exp") return const_cast<void *>(reinterpret_cast<const void *>(&expf));
				else if (routine_name == L"fpu_exp_d") return const_cast<void *>(reinterpret_cast<const void *>(&_exp_d));
				else if (routine_name == L"fpu_ln") return const_cast<void *>(reinterpret_cast<const void *>(&logf));
				else if (routine_name == L"fpu_ln_d") return const_cast<void *>(reinterpret_cast<const void *>(&_log_d));
				else if (routine_name == L"fpu_lb") return const_cast<void *>(reinterpret_cast<const void *>(&log2f));
				else if (routine_name == L"fpu_lb_d") return const_cast<void *>(reinterpret_cast<const void *>(&_log2_d));
				else if (routine_name == L"fpu_lg") return const_cast<void *>(reinterpret_cast<const void *>(&log10f));
				else if (routine_name == L"fpu_lg_d") return const_cast<void *>(reinterpret_cast<const void *>(&_log10_d));
				else if (routine_name == L"fpu_sin") return const_cast<void *>(reinterpret_cast<const void *>(&sinf));
				else if (routine_name == L"fpu_sin_d") return const_cast<void *>(reinterpret_cast<const void *>(&_sin_d));
				else if (routine_name == L"fpu_cos") return const_cast<void *>(reinterpret_cast<const void *>(&cosf));
				else if (routine_name == L"fpu_cos_d") return const_cast<void *>(reinterpret_cast<const void *>(&_cos_d));
				else if (routine_name == L"fpu_tg") return const_cast<void *>(reinterpret_cast<const void *>(&tanf));
				else if (routine_name == L"fpu_tg_d") return const_cast<void *>(reinterpret_cast<const void *>(&_tan_d));
				else if (routine_name == L"fpu_ctg") return const_cast<void *>(reinterpret_cast<const void *>(&_ctg_f32));
				else if (routine_name == L"fpu_ctg_d") return const_cast<void *>(reinterpret_cast<const void *>(&_ctg_f64));
				else if (routine_name == L"fpu_asin") return const_cast<void *>(reinterpret_cast<const void *>(&asinf));
				else if (routine_name == L"fpu_asin_d") return const_cast<void *>(reinterpret_cast<const void *>(&_asin_d));
				else if (routine_name == L"fpu_acos") return const_cast<void *>(reinterpret_cast<const void *>(&acosf));
				else if (routine_name == L"fpu_acos_d") return const_cast<void *>(reinterpret_cast<const void *>(&_acos_d));
				else if (routine_name == L"fpu_atg") return const_cast<void *>(reinterpret_cast<const void *>(&atanf));
				else if (routine_name == L"fpu_atg_d") return const_cast<void *>(reinterpret_cast<const void *>(&_atan_d));
				else if (routine_name == L"fpu_actg") return const_cast<void *>(reinterpret_cast<const void *>(&_arcctg_f32));
				else if (routine_name == L"fpu_actg_d") return const_cast<void *>(reinterpret_cast<const void *>(&_arcctg_f64));
				else if (routine_name == L"fpu_f32_pi") return const_cast<void *>(reinterpret_cast<const void *>(&_inf_pos_f32));
				else if (routine_name == L"fpu_f32_ni") return const_cast<void *>(reinterpret_cast<const void *>(&_inf_neg_f32));
				else if (routine_name == L"fpu_f32_nn") return const_cast<void *>(reinterpret_cast<const void *>(&_inf_nan_f32));
				else if (routine_name == L"fpu_f64_pi") return const_cast<void *>(reinterpret_cast<const void *>(&_inf_pos_f64));
				else if (routine_name == L"fpu_f64_ni") return const_cast<void *>(reinterpret_cast<const void *>(&_inf_neg_f64));
				else if (routine_name == L"fpu_f64_nn") return const_cast<void *>(reinterpret_cast<const void *>(&_inf_nan_f64));
				else if (routine_name == L"fpu_ei_32") return const_cast<void *>(reinterpret_cast<const void *>(&_is_inf_f32));
				else if (routine_name == L"fpu_ei_64") return const_cast<void *>(reinterpret_cast<const void *>(&_is_inf_f64));
				else if (routine_name == L"fpu_enn_32") return const_cast<void *>(reinterpret_cast<const void *>(&_is_nan_f32));
				else if (routine_name == L"fpu_enn_64") return const_cast<void *>(reinterpret_cast<const void *>(&_is_nan_f64));
				else if (routine_name == L"fpu_ncg_1") return const_cast<void *>(reinterpret_cast<const void *>(&_random_data));
				else if (routine_name == L"fpu_ncg_2") return const_cast<void *>(reinterpret_cast<const void *>(&_random_int));
				else if (routine_name == L"fpu_ncg_3") return const_cast<void *>(reinterpret_cast<const void *>(&Engine::Math::Random::RandomDouble));
				else return 0;
			}
		public:
			virtual void * ExposeRoutine(const string & routine_name) noexcept override
			{
				auto result = _expose_part_1(routine_name);
				if (result) return result;
				return _expose_part_2(routine_name);
			}
			virtual void * ExposeInterface(const string & interface) noexcept override { return 0; }
		};
		class MMU : public IAPIExtension
		{
			struct _sys_info {
				int64 freq;
				int64 mem;
				string proc_name;
				int sys_machine;
				int arch_machine;
				int arch_process;
				int cores_phys;
				int cores_virt;
				int ver_major;
				int ver_minor;
			};

			static int _get_arch(Platform platform)
			{
				if (platform == Platform::X86) return 0x01;
				else if (platform == Platform::X64) return 0x11;
				else if (platform == Platform::ARM) return 0x02;
				else if (platform == Platform::ARM64) return 0x12;
				else return 0;
			}
			static void * _mem_alloc(uintptr size) { return malloc(size); }
			static void * _mem_realloc(void * mem, uintptr size) { return realloc(mem, size); }
			static void _mem_release(void * mem) { free(mem); }
			static void _get_system_info(_sys_info & info)
			{
				try {
					SystemDesc desc;
					if (!GetSystemInformation(desc)) return;
					info.freq = desc.ClockFrequency;
					info.mem = desc.PhysicalMemory;
					info.proc_name = desc.ProcessorName;
					info.arch_machine = _get_arch(desc.Architecture);
					info.arch_process = _get_arch(ApplicationPlatform);
					info.cores_phys = desc.PhysicalCores;
					info.cores_virt = desc.VirtualCores;
					info.ver_major = desc.SystemVersionMajor;
					info.ver_minor = desc.SystemVersionMinor;
					info.sys_machine = 0;
					#ifdef ENGINE_WINDOWS
					info.sys_machine = 0x01;
					#endif
					#ifdef ENGINE_MACOSX
					info.sys_machine = 0x02;
					#endif
					#ifdef ENGINE_LINUX
					info.sys_machine = 0x03;
					#endif
				} catch (...) {}
			}
			static bool _check_arch(int arch)
			{
				if (arch == 0x01) return IsPlatformAvailable(Platform::X86);
				else if (arch == 0x11) return IsPlatformAvailable(Platform::X64);
				else if (arch == 0x02) return IsPlatformAvailable(Platform::ARM);
				else if (arch == 0x12) return IsPlatformAvailable(Platform::ARM64);
				else return false;
			}
		public:
			virtual void * ExposeRoutine(const string & routine_name) noexcept override
			{
				if (routine_name == L"alloca_memoriam") return const_cast<void *>(reinterpret_cast<const void *>(&_mem_alloc));
				else if (routine_name == L"realloca_memoriam") return const_cast<void *>(reinterpret_cast<const void *>(&_mem_realloc));
				else if (routine_name == L"dimitte_memoriam") return const_cast<void *>(reinterpret_cast<const void *>(&_mem_release));
				else if (routine_name == L"relabe_memoriam") return const_cast<void *>(reinterpret_cast<const void *>(&Engine::ZeroMemory));
				else if (routine_name == L"exscribe_memoriam") return const_cast<void *>(reinterpret_cast<const void *>(&Engine::MemoryCopy));
				else if (routine_name == L"inc_sec") return const_cast<void *>(reinterpret_cast<const void *>(&Engine::InterlockedIncrement));
				else if (routine_name == L"dec_sec") return const_cast<void *>(reinterpret_cast<const void *>(&Engine::InterlockedDecrement));
				else if (routine_name == L"sys_info") return const_cast<void *>(reinterpret_cast<const void *>(&_get_system_info));
				else if (routine_name == L"sys_temp") return const_cast<void *>(reinterpret_cast<const void *>(&Engine::GetTimerValue));
				else if (routine_name == L"sys_arch") return const_cast<void *>(reinterpret_cast<const void *>(&_check_arch));
				else return 0;
			}
			virtual void * ExposeInterface(const string & interface) noexcept override { return 0; }
		};
		class SPU : public IAPIExtension
		{
			static void _init_string_with_string(string * self, const string & with, ErrorContext & error)
			{ try { new (self) string(with); } catch (...) { error.error_code = 2; error.error_subcode = 0; } }
			static void _init_string_with_string_nothrow(string * self, const string & with)
			{ try { new (self) string(with); } catch (...) { new (self) string; } }
			static void _init_string_with_utf32(string * self, const uint * ucs, ErrorContext & error)
			{ try { new (self) string(ucs, -1, Encoding::UTF32); } catch (...) { error.error_code = 2; error.error_subcode = 0; } }
			static void _init_string_with_int8(string * self, int8 value, ErrorContext & error)
			{ try { new (self) string(value); } catch (...) { error.error_code = 2; error.error_subcode = 0; } }
			static void _init_string_with_uint8(string * self, uint8 value, ErrorContext & error)
			{ try { new (self) string(value); } catch (...) { error.error_code = 2; error.error_subcode = 0; } }
			static void _init_string_with_int16(string * self, int16 value, ErrorContext & error)
			{ try { new (self) string(value); } catch (...) { error.error_code = 2; error.error_subcode = 0; } }
			static void _init_string_with_uint16(string * self, uint16 value, ErrorContext & error)
			{ try { new (self) string(value); } catch (...) { error.error_code = 2; error.error_subcode = 0; } }
			static void _init_string_with_int32(string * self, int32 value, ErrorContext & error)
			{ try { new (self) string(value); } catch (...) { error.error_code = 2; error.error_subcode = 0; } }
			static void _init_string_with_uint32(string * self, uint32 value, ErrorContext & error)
			{ try { new (self) string(value); } catch (...) { error.error_code = 2; error.error_subcode = 0; } }
			static void _init_string_with_int64(string * self, int64 value, ErrorContext & error)
			{ try { new (self) string(value); } catch (...) { error.error_code = 2; error.error_subcode = 0; } }
			static void _init_string_with_uint64(string * self, uint64 value, ErrorContext & error)
			{ try { new (self) string(value); } catch (...) { error.error_code = 2; error.error_subcode = 0; } }
			static void _init_string_with_intptr(string * self, sintptr value, ErrorContext & error)
			{ try { new (self) string(value); } catch (...) { error.error_code = 2; error.error_subcode = 0; } }
			static void _init_string_with_uintptr(string * self, uintptr value, ErrorContext & error)
			{ try { new (self) string(value); } catch (...) { error.error_code = 2; error.error_subcode = 0; } }
			static void _init_string_with_uint32_radix(string * self, uint32 value, const string & radix, int ml, ErrorContext & error)
			{
				try { new (self) string(value, radix, ml); }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; }
			}
			static void _init_string_with_uint64_radix(string * self, uint64 value, const string & radix, int ml, ErrorContext & error)
			{
				try { new (self) string(value, radix, ml); }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; }
			}
			static void _init_string_with_uintptr_radix(string * self, uintptr value, const string & radix, int ml, ErrorContext & error)
			{
				try { new (self) string(value, radix, ml); }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; }
			}
			static void _init_string_with_float(string * self, float value, ErrorContext & error)
			{ try { new (self) string(value); } catch (...) { error.error_code = 2; error.error_subcode = 0; } }
			static void _init_string_with_float_del(string * self, float value, uint32 del, ErrorContext & error)
			{
				try { new (self) string(value, widechar(del)); }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; }
			}
			static void _init_string_with_float_del_len(string * self, float value, uint32 del, int len, ErrorContext & error)
			{
				try { new (self) string(value, widechar(del), len); }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; }
			}
			static void _init_string_with_double(string * self, double value, ErrorContext & error)
			{ try { new (self) string(value); } catch (...) { error.error_code = 2; error.error_subcode = 0; } }
			static void _init_string_with_double_del(string * self, double value, uint32 del, ErrorContext & error)
			{
				try { new (self) string(value, widechar(del)); }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; }
			}
			static void _init_string_with_double_del_len(string * self, double value, uint32 del, int len, ErrorContext & error)
			{
				try { new (self) string(value, widechar(del), len); }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; }
			}
			static void _init_string_with_boolean(string * self, bool value, ErrorContext & error)
			{ try { new (self) string(value ? "sic" : "non"); } catch (...) { error.error_code = 2; error.error_subcode = 0; } }
			static void _init_string_with_char(string * self, uint32 value, int len, ErrorContext & error)
			{
				try {
					Array<uint32> line(len + 1);
					for (int i = 0; i < len; i++) line << value; line << 0;
					new (self) string(line.GetBuffer(), -1, Encoding::UTF32);
				} catch (...) { error.error_code = 2; error.error_subcode = 0; }
			}
			static void _init_string_with_ptr(string * self, void * value, ErrorContext & error)
			{ try { new (self) string(value); } catch (...) { error.error_code = 2; error.error_subcode = 0; } }
			static void _init_string_with_data(string * self, void * data, int length, int encoding, ErrorContext & error)
			{
				try { new (self) string(data, length, static_cast<Encoding>(encoding)); }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; }
			}
			static string & _assign_with_string(string & self, const string & with, ErrorContext & error)
			{ try { return self = with; } catch (...) { error.error_code = 2; error.error_subcode = 0; return self; } }
			static string & _assign_with_utf32(string & self, const uint32 * with, ErrorContext & error)
			{ try { return self = string(with, -1, Encoding::UTF32); } catch (...) { error.error_code = 2; error.error_subcode = 0; return self; } }
			static string & _concat(string & self, const string & with, ErrorContext & error)
			{ try { return self += with; } catch (...) { error.error_code = 2; error.error_subcode = 0; return self; } }
			static string _static_concat(const string & a, const string & b, ErrorContext & error)
			{ try { return a + b; } catch (...) { error.error_code = 2; error.error_subcode = 0; return string(); } }
			static int _static_order(const string & a, const string & b) { return string::Compare(a, b); }
			static int _static_order_nc(const string & a, const string & b) { return string::CompareIgnoreCase(a, b); }
			static int _length(const string & s)
			{
				if (SystemEncoding == Encoding::UTF32) return s.Length(); else {
					int counter = 0, pos = 0;
					while (s[pos]) {
						if ((s[pos] & 0xFC00) != 0xD800) { counter++; pos++; }
						else pos++;
					}
					return counter;
				}
			}
			static int _pos_system_to_ucs(const string & s, int from)
			{
				if (SystemEncoding == Encoding::UTF32) return from; else {
					if (from < 0) return from;
					int result = 0;
					for (int i = 0; i < from; i++) {
						if (!s[i]) return _length(s) + 1;
						if ((s[i] & 0xFC00) != 0xD800) result++;
					}
					return result;
				}
			}
			static int _pos_ucs_to_system(const string & s, int from)
			{
				if (SystemEncoding == Encoding::UTF32) return from; else {
					if (from < 0) return from;
					int ucs = 0, pos = 0;
					while (ucs < from) {
						if (!s[pos]) return _length(s) + 1;
						if ((s[pos] & 0xFC00) != 0xD800) { ucs++; pos++; }
						else pos++;
					}
					return pos;
				}
			}
			static uint32 _subscript(const string & self, int index)
			{
				if (SystemEncoding == Encoding::UTF32) return self[index]; else {
					auto pos = _pos_ucs_to_system(self, index);
					if ((self[pos] & 0xFC00) == 0xD800) {
						return ((uint32(self[pos] & 0x03FF) << 10) | uint32(self[pos + 1] & 0x03FF)) + 0x10000;
					} else return self[pos];
				}
			}
			static int _find_first(const string & self, const string & find)
			{
				if (find.Length() == 1) return _pos_system_to_ucs(self, self.FindFirst(find[0]));
				else return _pos_system_to_ucs(self, self.FindFirst(find));
			}
			static int _find_last(const string & self, const string & find)
			{
				if (find.Length() == 1) return _pos_system_to_ucs(self, self.FindLast(find[0]));
				else return _pos_system_to_ucs(self, self.FindLast(find));
			}
			static string _fragment(const string & self, int from, int length, ErrorContext & error)
			{
				try {
					int after = length >= 0 ? _pos_ucs_to_system(self, from + length) : -1;
					int begin = _pos_ucs_to_system(self, from);
					return self.Fragment(begin, after >= 0 ? after - begin : -1);
				} catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; return string(); }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return string(); }
			}
			static string _replace(const string & self, const string & substr, const string & with, ErrorContext & error)
			{
				try { return self.Replace(substr, with); }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; return string(); }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return string(); }
			}
			static string _lower_case(const string & self, ErrorContext & error)
			{
				try { return self.LowerCase(); }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return string(); }
			}
			static string _upper_case(const string & self, ErrorContext & error)
			{
				try { return self.UpperCase(); }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return string(); }
			}
			static int _encode_length(const string & self, int encoding, ErrorContext & error)
			{
				try { return self.GetEncodedLength(static_cast<Encoding>(encoding)); }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; return 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return 0; }
			}
			static void _encode(const string & self, void * dest, int encoding, bool with_term, ErrorContext & error)
			{
				try { return self.Encode(dest, static_cast<Encoding>(encoding), with_term); }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; }
			}
			static int32 _to_int32(const string & self, ErrorContext & error)
			{
				try { return self.ToInt32(); }
				catch (InvalidFormatException &) { error.error_code = 4; error.error_subcode = 0; return 0; }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; return 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return 0; }
			}
			static int32 _to_int32_2(const string & self, const string & digits, ErrorContext & error)
			{
				try { return self.ToInt32(digits); }
				catch (InvalidFormatException &) { error.error_code = 4; error.error_subcode = 0; return 0; }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; return 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return 0; }
			}
			static int32 _to_int32_3(const string & self, const string & digits, bool case_sensitive, ErrorContext & error)
			{
				try { return self.ToInt32(digits, case_sensitive); }
				catch (InvalidFormatException &) { error.error_code = 4; error.error_subcode = 0; return 0; }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; return 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return 0; }
			}
			static uint32 _to_uint32(const string & self, ErrorContext & error)
			{
				try { return self.ToUInt32(); }
				catch (InvalidFormatException &) { error.error_code = 4; error.error_subcode = 0; return 0; }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; return 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return 0; }
			}
			static uint32 _to_uint32_2(const string & self, const string & digits, ErrorContext & error)
			{
				try { return self.ToUInt32(digits); }
				catch (InvalidFormatException &) { error.error_code = 4; error.error_subcode = 0; return 0; }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; return 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return 0; }
			}
			static uint32 _to_uint32_3(const string & self, const string & digits, bool case_sensitive, ErrorContext & error)
			{
				try { return self.ToUInt32(digits, case_sensitive); }
				catch (InvalidFormatException &) { error.error_code = 4; error.error_subcode = 0; return 0; }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; return 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return 0; }
			}
			static int64 _to_int64(const string & self, ErrorContext & error)
			{
				try { return self.ToInt64(); }
				catch (InvalidFormatException &) { error.error_code = 4; error.error_subcode = 0; return 0; }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; return 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return 0; }
			}
			static int64 _to_int64_2(const string & self, const string & digits, ErrorContext & error)
			{
				try { return self.ToInt64(digits); }
				catch (InvalidFormatException &) { error.error_code = 4; error.error_subcode = 0; return 0; }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; return 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return 0; }
			}
			static int64 _to_int64_3(const string & self, const string & digits, bool case_sensitive, ErrorContext & error)
			{
				try { return self.ToInt64(digits, case_sensitive); }
				catch (InvalidFormatException &) { error.error_code = 4; error.error_subcode = 0; return 0; }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; return 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return 0; }
			}
			static uint64 _to_uint64(const string & self, ErrorContext & error)
			{
				try { return self.ToUInt64(); }
				catch (InvalidFormatException &) { error.error_code = 4; error.error_subcode = 0; return 0; }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; return 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return 0; }
			}
			static uint64 _to_uint64_2(const string & self, const string & digits, ErrorContext & error)
			{
				try { return self.ToUInt64(digits); }
				catch (InvalidFormatException &) { error.error_code = 4; error.error_subcode = 0; return 0; }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; return 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return 0; }
			}
			static uint64 _to_uint64_3(const string & self, const string & digits, bool case_sensitive, ErrorContext & error)
			{
				try { return self.ToUInt64(digits, case_sensitive); }
				catch (InvalidFormatException &) { error.error_code = 4; error.error_subcode = 0; return 0; }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; return 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return 0; }
			}
			static float _to_float(const string & self, ErrorContext & error)
			{
				try { return self.ToFloat(); }
				catch (InvalidFormatException &) { error.error_code = 4; error.error_subcode = 0; return 0; }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; return 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return 0; }
			}
			static float _to_float_2(const string & self, const string & del_list, ErrorContext & error)
			{
				try { return self.ToFloat(del_list); }
				catch (InvalidFormatException &) { error.error_code = 4; error.error_subcode = 0; return 0; }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; return 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return 0; }
			}
			static double _to_double(const string & self, ErrorContext & error)
			{
				try { return self.ToDouble(); }
				catch (InvalidFormatException &) { error.error_code = 4; error.error_subcode = 0; return 0; }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; return 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return 0; }
			}
			static double _to_double_2(const string & self, const string & del_list, ErrorContext & error)
			{
				try { return self.ToDouble(del_list); }
				catch (InvalidFormatException &) { error.error_code = 4; error.error_subcode = 0; return 0; }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; return 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return 0; }
			}
			static string _format_0(const string & form, const string & a0, ErrorContext & error)
			{
				try { return FormatString(form, a0); }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return string(); }
			}
			static string _format_1(const string & form, const string & a0, const string & a1, ErrorContext & error)
			{
				try { return FormatString(form, a0, a1); }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return string(); }
			}
			static string _format_2(const string & form, const string & a0, const string & a1, const string & a2,
				ErrorContext & error)
			{
				try { return FormatString(form, a0, a1, a2); }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return string(); }
			}
			static string _format_3(const string & form, const string & a0, const string & a1, const string & a2,
				const string & a3, ErrorContext & error)
			{
				try { return FormatString(form, a0, a1, a2, a3); }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return string(); }
			}
			static string _format_4(const string & form, const string & a0, const string & a1, const string & a2,
				const string & a3, const string & a4, ErrorContext & error)
			{
				try { return FormatString(form, a0, a1, a2, a3, a4); }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return string(); }
			}
			static string _format_5(const string & form, const string & a0, const string & a1, const string & a2,
				const string & a3, const string & a4, const string & a5, ErrorContext & error)
			{
				try { return FormatString(form, a0, a1, a2, a3, a4, a5); }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return string(); }
			}
			static string _format_6(const string & form, const string & a0, const string & a1, const string & a2,
				const string & a3, const string & a4, const string & a5, const string & a6, ErrorContext & error)
			{
				try { return FormatString(form, a0, a1, a2, a3, a4, a5, a6); }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return string(); }
			}
			static string _format_7(const string & form, const string & a0, const string & a1, const string & a2,
				const string & a3, const string & a4, const string & a5, const string & a6, const string & a7,
				ErrorContext & error)
			{
				try { return FormatString(form, a0, a1, a2, a3, a4, a5, a6, a7); }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return string(); }
			}
			static string _format_8(const string & form, const string & a0, const string & a1, const string & a2,
				const string & a3, const string & a4, const string & a5, const string & a6, const string & a7,
				const string & a8, ErrorContext & error)
			{
				try { return FormatString(form, a0, a1, a2, a3, a4, a5, a6, a7, a8); }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return string(); }
			}
			static string _format_9(const string & form, const string & a0, const string & a1, const string & a2,
				const string & a3, const string & a4, const string & a5, const string & a6, const string & a7,
				const string & a8, const string & a9, ErrorContext & error)
			{
				try { return FormatString(form, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9); }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return string(); }
			}
			static SafePointer< Array<string> > _split(const string & self, uint32 by, ErrorContext & error)
			{
				if (by > 0xFFFF) { error.error_code = 3; error.error_subcode = 0; return 0; }
				try { return new Array<string>(self.Split(widechar(by))); }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return 0; }
			}
			static SafePointer<DataBlock> _encode_block(const string & self, int encoding, bool with_term, ErrorContext & error)
			{
				try { return self.EncodeSequence(static_cast<Encoding>(encoding), with_term); }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; return 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return 0; }
			}
		public:
			virtual void * ExposeRoutine(const string & routine_name) noexcept override
			{
				if (routine_name == L"spu_crea") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_string));
				else if (routine_name == L"spu_crea_sec") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_string_nothrow));
				else if (routine_name == L"spu_crea_utf32") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_utf32));
				else if (routine_name == L"spu_crea_int8") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_int8));
				else if (routine_name == L"spu_crea_nint8") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_uint8));
				else if (routine_name == L"spu_crea_int16") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_int16));
				else if (routine_name == L"spu_crea_nint16") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_uint16));
				else if (routine_name == L"spu_crea_int32") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_int32));
				else if (routine_name == L"spu_crea_nint32") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_uint32));
				else if (routine_name == L"spu_crea_int64") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_int64));
				else if (routine_name == L"spu_crea_nint64") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_uint64));
				else if (routine_name == L"spu_crea_intadl") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_intptr));
				else if (routine_name == L"spu_crea_nintadl") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_uintptr));
				else if (routine_name == L"spu_crea_nint32_radix") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_uint32_radix));
				else if (routine_name == L"spu_crea_nint64_radix") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_uint64_radix));
				else if (routine_name == L"spu_crea_nintadl_radix") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_uintptr_radix));
				else if (routine_name == L"spu_crea_frac32") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_float));
				else if (routine_name == L"spu_crea_frac32_2") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_float_del));
				else if (routine_name == L"spu_crea_frac32_3") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_float_del_len));
				else if (routine_name == L"spu_crea_frac64") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_double));
				else if (routine_name == L"spu_crea_frac64_2") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_double_del));
				else if (routine_name == L"spu_crea_frac64_3") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_double_del_len));
				else if (routine_name == L"spu_crea_logicum") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_boolean));
				else if (routine_name == L"spu_crea_char") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_char));
				else if (routine_name == L"spu_crea_adl") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_ptr));
				else if (routine_name == L"spu_crea_data") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_data));
				else if (routine_name == L"spu_loca") return const_cast<void *>(reinterpret_cast<const void *>(&_assign_with_string));
				else if (routine_name == L"spu_loca_utf32") return const_cast<void *>(reinterpret_cast<const void *>(&_assign_with_utf32));
				else if (routine_name == L"spu_concat") return const_cast<void *>(reinterpret_cast<const void *>(&_concat));
				else if (routine_name == L"spu_concat_classis") return const_cast<void *>(reinterpret_cast<const void *>(&_static_concat));
				else if (routine_name == L"spu_compareo") return const_cast<void *>(reinterpret_cast<const void *>(&_static_order));
				else if (routine_name == L"spu_compareo_2") return const_cast<void *>(reinterpret_cast<const void *>(&_static_order_nc));
				else if (routine_name == L"spu_long") return const_cast<void *>(reinterpret_cast<const void *>(&_length));
				else if (routine_name == L"spu_index") return const_cast<void *>(reinterpret_cast<const void *>(&_subscript));
				else if (routine_name == L"spu_reperi_primus") return const_cast<void *>(reinterpret_cast<const void *>(&_find_first));
				else if (routine_name == L"spu_reperi_ultimus") return const_cast<void *>(reinterpret_cast<const void *>(&_find_last));
				else if (routine_name == L"spu_fragmentum") return const_cast<void *>(reinterpret_cast<const void *>(&_fragment));
				else if (routine_name == L"spu_surroga") return const_cast<void *>(reinterpret_cast<const void *>(&_replace));
				else if (routine_name == L"spu_inferna") return const_cast<void *>(reinterpret_cast<const void *>(&_lower_case));
				else if (routine_name == L"spu_supera") return const_cast<void *>(reinterpret_cast<const void *>(&_upper_case));
				else if (routine_name == L"spu_codex_long") return const_cast<void *>(reinterpret_cast<const void *>(&_encode_length));
				else if (routine_name == L"spu_codifica") return const_cast<void *>(reinterpret_cast<const void *>(&_encode));
				else if (routine_name == L"spu_ad_int32") return const_cast<void *>(reinterpret_cast<const void *>(&_to_int32));
				else if (routine_name == L"spu_ad_int32_2") return const_cast<void *>(reinterpret_cast<const void *>(&_to_int32_2));
				else if (routine_name == L"spu_ad_int32_3") return const_cast<void *>(reinterpret_cast<const void *>(&_to_int32_3));
				else if (routine_name == L"spu_ad_nint32") return const_cast<void *>(reinterpret_cast<const void *>(&_to_uint32));
				else if (routine_name == L"spu_ad_nint32_2") return const_cast<void *>(reinterpret_cast<const void *>(&_to_uint32_2));
				else if (routine_name == L"spu_ad_nint32_3") return const_cast<void *>(reinterpret_cast<const void *>(&_to_uint32_3));
				else if (routine_name == L"spu_ad_int64") return const_cast<void *>(reinterpret_cast<const void *>(&_to_int64));
				else if (routine_name == L"spu_ad_int64_2") return const_cast<void *>(reinterpret_cast<const void *>(&_to_int64_2));
				else if (routine_name == L"spu_ad_int64_3") return const_cast<void *>(reinterpret_cast<const void *>(&_to_int64_3));
				else if (routine_name == L"spu_ad_nint64") return const_cast<void *>(reinterpret_cast<const void *>(&_to_uint64));
				else if (routine_name == L"spu_ad_nint64_2") return const_cast<void *>(reinterpret_cast<const void *>(&_to_uint64_2));
				else if (routine_name == L"spu_ad_nint64_3") return const_cast<void *>(reinterpret_cast<const void *>(&_to_uint64_3));
				else if (routine_name == L"spu_ad_frac32") return const_cast<void *>(reinterpret_cast<const void *>(&_to_float));
				else if (routine_name == L"spu_ad_frac32_2") return const_cast<void *>(reinterpret_cast<const void *>(&_to_float_2));
				else if (routine_name == L"spu_ad_frac64") return const_cast<void *>(reinterpret_cast<const void *>(&_to_double));
				else if (routine_name == L"spu_ad_frac64_2") return const_cast<void *>(reinterpret_cast<const void *>(&_to_double_2));
				else if (routine_name == L"spu_forma_0") return const_cast<void *>(reinterpret_cast<const void *>(&_format_0));
				else if (routine_name == L"spu_forma_1") return const_cast<void *>(reinterpret_cast<const void *>(&_format_1));
				else if (routine_name == L"spu_forma_2") return const_cast<void *>(reinterpret_cast<const void *>(&_format_2));
				else if (routine_name == L"spu_forma_3") return const_cast<void *>(reinterpret_cast<const void *>(&_format_3));
				else if (routine_name == L"spu_forma_4") return const_cast<void *>(reinterpret_cast<const void *>(&_format_4));
				else if (routine_name == L"spu_forma_5") return const_cast<void *>(reinterpret_cast<const void *>(&_format_5));
				else if (routine_name == L"spu_forma_6") return const_cast<void *>(reinterpret_cast<const void *>(&_format_6));
				else if (routine_name == L"spu_forma_7") return const_cast<void *>(reinterpret_cast<const void *>(&_format_7));
				else if (routine_name == L"spu_forma_8") return const_cast<void *>(reinterpret_cast<const void *>(&_format_8));
				else if (routine_name == L"spu_forma_9") return const_cast<void *>(reinterpret_cast<const void *>(&_format_9));
				else if (routine_name == L"spu_codifica_taleam") return const_cast<void *>(reinterpret_cast<const void *>(&_encode_block));
				else if (routine_name == L"spu_scinde") return const_cast<void *>(reinterpret_cast<const void *>(&_split));
				else return 0;
			}
			virtual void * ExposeInterface(const string & interface) noexcept override { return 0; }
		};
		class MiscUnit : public IAPIExtension
		{
			class _resource_stream : public Streaming::Stream
			{
				SafePointer<DataBlock> _data;
				int _pos;
			public:
				_resource_stream(DataBlock * data) : _pos(0) { _data.SetRetain(data); }
				virtual ~_resource_stream(void) override {}
				virtual string ToString(void) const override { return L"XE Resource Stream"; }
				virtual void Read(void * buffer, uint32 length) override
				{
					uint avail = _data->Length() - _pos;
					uint read = min(length, avail);
					MemoryCopy(buffer, _data->GetBuffer() + _pos, read);
					_pos += read;
					if (read != length) throw IO::FileReadEndOfFileException(read);
				}
				virtual void Write(const void * data, uint32 length) override { throw IO::FileAccessException(IO::Error::NotImplemented); }
				virtual int64 Seek(int64 position, Streaming::SeekOrigin origin) override
				{
					if (origin == Streaming::SeekOrigin::Begin) _pos = position;
					else if (origin == Streaming::SeekOrigin::Current) _pos += position;
					else if (origin == Streaming::SeekOrigin::End) _pos = _data->Length() + position;
					if (_pos < 0) _pos = 0;
					if (_pos > _data->Length()) _pos = _data->Length();
					return _pos;
				}
				virtual uint64 Length(void) override { return _data->Length(); }
				virtual void SetLength(uint64 length) override { throw IO::FileAccessException(IO::Error::NotImplemented); }
				virtual void Flush(void) override {}
			};

			static void _time_init_7(Time * self, int year, int month, int day, int hour, int minute, int second, int millisecond) { new (self) Time(year, month, day, hour, minute, second, millisecond); }
			static void _time_init_4(Time * self, int hour, int minute, int second, int millisecond) { new (self) Time(hour, minute, second, millisecond); }
			static int _get_year(Time * self) { return self->GetYear(); }
			static int _get_month(Time * self) { return self->GetMonth(); }
			static int _get_day_of_week(Time * self) { return self->DayOfWeek(); }
			static int _get_day(Time * self) { return self->GetDay(); }
			static int _get_hour(Time * self) { return self->GetHour(); }
			static int _get_minute(Time * self) { return self->GetMinute(); }
			static int _get_second(Time * self) { return self->GetSecond(); }
			static int _get_millisecond(Time * self) { return self->GetMillisecond(); }
			static Time _get_time_universal(Time * self) { return self->ToUniversal(); }
			static Time _get_time_local(Time * self) { return self->ToLocal(); }
			static Time _get_time_current(void) { return Time::GetCurrentTime(); }
			static string _time_to_string(Time * self, ErrorContext & ectx)
			{
				try { return self->ToString(); }
				catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return L""; }
			}
			static string _time_to_string_short(Time * self, ErrorContext & ectx)
			{
				try { return self->ToShortString(); }
				catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return L""; }
			}
			static int _time_days_in_month(Time * self)
			{
				uint year, month, day;
				self->GetDate(year, month, day);
				if (Time::IsYearOdd(year)) return OddMonthLength[month - 1];
				else return RegularMonthLength[month - 1];
			}
			static SafePointer<XStream> _create_memory_stream_0(ErrorContext & ectx)
			{
				try {
					SafePointer<Streaming::Stream> stream = new Streaming::MemoryStream(0x10000);
					return WrapToXStream(stream);
				} catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return 0; }
			}
			static SafePointer<XStream> _create_memory_stream_2(const void * data, intptr size, ErrorContext & ectx)
			{
				if (size >= 0x80000000) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				try {
					SafePointer<Streaming::Stream> stream = new Streaming::MemoryStream(data, size);
					return WrapToXStream(stream);
				} catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return 0; }
			}
			static SafePointer<XStream> _create_resource_stream(const Module * mdl, const string & type, int count, ErrorContext & ectx)
			{
				try {
					auto & rsrc = mdl->GetResources();
					auto res = rsrc.GetElementByKey(type + L":" + string(count));
					if (!res || !res->Inner()) { ectx.error_code = 6; ectx.error_subcode = 2; return 0; }
					SafePointer<_resource_stream> stream = new _resource_stream(res->Inner());
					return WrapToXStream(stream);
				} catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return 0; }
			}
			static void _stream_copy_to_2(XStream * from, XStream * to, int64 size, ErrorContext & ectx)
			{
				try {
					SafePointer<Streaming::Stream> src = WrapFromXStream(from);
					SafePointer<Streaming::Stream> dest = WrapFromXStream(to);
					src->CopyTo(dest, size);
				} catch (IO::FileAccessException & e) { ectx.error_code = 6; ectx.error_subcode = e.code; }
				catch (InvalidStateException & e) { ectx.error_code = 5; ectx.error_subcode = 0; }
				catch (InvalidFormatException & e) { ectx.error_code = 4; ectx.error_subcode = 0; }
				catch (InvalidArgumentException & e) { ectx.error_code = 3; ectx.error_subcode = 0; }
				catch (OutOfMemoryException & e) { ectx.error_code = 2; ectx.error_subcode = 0; }
				catch (...) { ectx.error_code = 6; ectx.error_subcode = 1; }
			}
			static void _stream_copy_to_1(XStream * from, XStream * to, ErrorContext & ectx)
			{
				try {
					SafePointer<Streaming::Stream> src = WrapFromXStream(from);
					SafePointer<Streaming::Stream> dest = WrapFromXStream(to);
					src->CopyToUntilEof(dest);
				} catch (IO::FileAccessException & e) { ectx.error_code = 6; ectx.error_subcode = e.code; }
				catch (InvalidStateException & e) { ectx.error_code = 5; ectx.error_subcode = 0; }
				catch (InvalidFormatException & e) { ectx.error_code = 4; ectx.error_subcode = 0; }
				catch (InvalidArgumentException & e) { ectx.error_code = 3; ectx.error_subcode = 0; }
				catch (OutOfMemoryException & e) { ectx.error_code = 2; ectx.error_subcode = 0; }
				catch (...) { ectx.error_code = 6; ectx.error_subcode = 1; }
			}
			static SafePointer<DataBlock> _stream_read_1(XStream * from, int size, ErrorContext & ectx)
			{
				try {
					SafePointer<Streaming::Stream> src = WrapFromXStream(from);
					return src->ReadBlock(size);
				} catch (IO::FileAccessException & e) { ectx.error_code = 6; ectx.error_subcode = e.code; }
				catch (InvalidStateException & e) { ectx.error_code = 5; ectx.error_subcode = 0; }
				catch (InvalidFormatException & e) { ectx.error_code = 4; ectx.error_subcode = 0; }
				catch (InvalidArgumentException & e) { ectx.error_code = 3; ectx.error_subcode = 0; }
				catch (OutOfMemoryException & e) { ectx.error_code = 2; ectx.error_subcode = 0; }
				catch (...) { ectx.error_code = 6; ectx.error_subcode = 1; }
			}
			static SafePointer<DataBlock> _stream_read_0(XStream * from, ErrorContext & ectx)
			{
				try {
					SafePointer<Streaming::Stream> src = WrapFromXStream(from);
					return src->ReadAll();
				} catch (IO::FileAccessException & e) { ectx.error_code = 6; ectx.error_subcode = e.code; }
				catch (InvalidStateException & e) { ectx.error_code = 5; ectx.error_subcode = 0; }
				catch (InvalidFormatException & e) { ectx.error_code = 4; ectx.error_subcode = 0; }
				catch (InvalidArgumentException & e) { ectx.error_code = 3; ectx.error_subcode = 0; }
				catch (OutOfMemoryException & e) { ectx.error_code = 2; ectx.error_subcode = 0; }
				catch (...) { ectx.error_code = 6; ectx.error_subcode = 1; }
			}
			static void _stream_write(XStream * to, const DataBlock * data, ErrorContext & ectx)
			{
				try {
					SafePointer<Streaming::Stream> dest = WrapFromXStream(to);
					dest->WriteArray(data);
				} catch (IO::FileAccessException & e) { ectx.error_code = 6; ectx.error_subcode = e.code; }
				catch (InvalidStateException & e) { ectx.error_code = 5; ectx.error_subcode = 0; }
				catch (InvalidFormatException & e) { ectx.error_code = 4; ectx.error_subcode = 0; }
				catch (InvalidArgumentException & e) { ectx.error_code = 3; ectx.error_subcode = 0; }
				catch (OutOfMemoryException & e) { ectx.error_code = 2; ectx.error_subcode = 0; }
				catch (...) { ectx.error_code = 6; ectx.error_subcode = 1; }
			}
		
			static SafePointer<XTextEncoder> _create_writer_1(XStream * stream, ErrorContext & ectx)
			{
				try {
					SafePointer<Streaming::Stream> str = WrapFromXStream(stream);
					SafePointer<Streaming::TextWriter> wri = new Streaming::TextWriter(str);
					return WrapToEncoder(wri);
				} catch (InvalidArgumentException &) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return 0; }
			}
			static SafePointer<XTextEncoder> _create_writer_2(XStream * stream, int enc, ErrorContext & ectx)
			{
				try {
					SafePointer<Streaming::Stream> str = WrapFromXStream(stream);
					SafePointer<Streaming::TextWriter> wri = new Streaming::TextWriter(str, static_cast<Encoding>(enc));
					return WrapToEncoder(wri);
				} catch (InvalidArgumentException &) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return 0; }
			}
			static SafePointer<XTextDecoder> _create_reader_1(XStream * stream, ErrorContext & ectx)
			{
				try {
					SafePointer<Streaming::Stream> str = WrapFromXStream(stream);
					SafePointer<Streaming::TextReader> rdr = new Streaming::TextReader(str);
					return WrapToDecoder(rdr);
				} catch (InvalidArgumentException &) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return 0; }
			}
			static SafePointer<XTextDecoder> _create_reader_2(XStream * stream, int enc, ErrorContext & ectx)
			{
				try {
					SafePointer<Streaming::Stream> str = WrapFromXStream(stream);
					SafePointer<Streaming::TextReader> rdr = new Streaming::TextReader(str, static_cast<Encoding>(enc));
					return WrapToDecoder(rdr);
				} catch (InvalidArgumentException &) { ectx.error_code = 3; ectx.error_subcode = 0; return 0; }
				catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return 0; }
			}
		public:
			virtual void * ExposeRoutine(const string & routine_name) noexcept override
			{
				if (routine_name == L"tmp_crea_7") return const_cast<void *>(reinterpret_cast<const void *>(&_time_init_7));
				else if (routine_name == L"tmp_crea_4") return const_cast<void *>(reinterpret_cast<const void *>(&_time_init_4));
				else if (routine_name == L"tmp_a_ans") return const_cast<void *>(reinterpret_cast<const void *>(&_get_year));
				else if (routine_name == L"tmp_a_mns") return const_cast<void *>(reinterpret_cast<const void *>(&_get_month));
				else if (routine_name == L"tmp_a_dis") return const_cast<void *>(reinterpret_cast<const void *>(&_get_day_of_week));
				else if (routine_name == L"tmp_a_die") return const_cast<void *>(reinterpret_cast<const void *>(&_get_day));
				else if (routine_name == L"tmp_a_hor") return const_cast<void *>(reinterpret_cast<const void *>(&_get_hour));
				else if (routine_name == L"tmp_a_min") return const_cast<void *>(reinterpret_cast<const void *>(&_get_minute));
				else if (routine_name == L"tmp_a_sec") return const_cast<void *>(reinterpret_cast<const void *>(&_get_second));
				else if (routine_name == L"tmp_a_msc") return const_cast<void *>(reinterpret_cast<const void *>(&_get_millisecond));
				else if (routine_name == L"tmp_a_uni") return const_cast<void *>(reinterpret_cast<const void *>(&_get_time_universal));
				else if (routine_name == L"tmp_a_loc") return const_cast<void *>(reinterpret_cast<const void *>(&_get_time_local));
				else if (routine_name == L"tmp_cur") return const_cast<void *>(reinterpret_cast<const void *>(&_get_time_current));
				else if (routine_name == L"tmp_adl") return const_cast<void *>(reinterpret_cast<const void *>(&_time_to_string));
				else if (routine_name == L"tmp_alb") return const_cast<void *>(reinterpret_cast<const void *>(&_time_to_string_short));
				else if (routine_name == L"tmp_dim") return const_cast<void *>(reinterpret_cast<const void *>(&_time_days_in_month));
				else if (routine_name == L"flumen_mem_0") return const_cast<void *>(reinterpret_cast<const void *>(&_create_memory_stream_0));
				else if (routine_name == L"flumen_mem_2") return const_cast<void *>(reinterpret_cast<const void *>(&_create_memory_stream_2));
				else if (routine_name == L"flumen_auxil") return const_cast<void *>(reinterpret_cast<const void *>(&_create_resource_stream));
				else if (routine_name == L"flumen_ex_2") return const_cast<void *>(reinterpret_cast<const void *>(&_stream_copy_to_2));
				else if (routine_name == L"flumen_ex_1") return const_cast<void *>(reinterpret_cast<const void *>(&_stream_copy_to_1));
				else if (routine_name == L"flumen_lo_1") return const_cast<void *>(reinterpret_cast<const void *>(&_stream_read_1));
				else if (routine_name == L"flumen_lo_0") return const_cast<void *>(reinterpret_cast<const void *>(&_stream_read_0));
				else if (routine_name == L"flumen_so_1") return const_cast<void *>(reinterpret_cast<const void *>(&_stream_write));
				else if (routine_name == L"scr_cod_1") return const_cast<void *>(reinterpret_cast<const void *>(&_create_writer_1));
				else if (routine_name == L"scr_cod_2") return const_cast<void *>(reinterpret_cast<const void *>(&_create_writer_2));
				else if (routine_name == L"scr_dec_1") return const_cast<void *>(reinterpret_cast<const void *>(&_create_reader_1));
				else if (routine_name == L"scr_dec_2") return const_cast<void *>(reinterpret_cast<const void *>(&_create_reader_2));

				// TODO: IMPLEMENT
				
				else return 0;
			}
			virtual void * ExposeInterface(const string & interface) noexcept override { return 0; }
		};

		IAPIExtension * CreateFPU(void) { return new FPU; }
		IAPIExtension * CreateMMU(void) { return new MMU; }
		IAPIExtension * CreateSPU(void) { return new SPU; }
		IAPIExtension * CreateMiscUnit(void) { return new MiscUnit; }
	}
}