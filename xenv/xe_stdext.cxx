#include "xe_stdext.h"

#include "xe_logger.h"
#include "xe_interfaces.h"
#include "../ximg/xi_resources.h"

namespace Engine
{
	namespace XE
	{
		class FPU : public IAPIExtension
		{
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
		public:
			virtual const void * ExposeRoutine(const string & routine_name) noexcept override
			{
				if (string::Compare(routine_name, L"fpu_f32_nn") < 0) {
					if (string::Compare(routine_name, L"fpu_c3_d") < 0) {
						if (string::Compare(routine_name, L"fpu_atg") < 0) {
							if (string::Compare(routine_name, L"fpu_actg_d") < 0) {
								if (string::Compare(routine_name, L"fpu_acos_d") < 0) {
									if (string::Compare(routine_name, L"fpu_acos") == 0) return reinterpret_cast<const void *>(&acosf);
								} else {
									if (string::Compare(routine_name, L"fpu_actg") < 0) {
										if (string::Compare(routine_name, L"fpu_acos_d") == 0) return reinterpret_cast<const void *>(&_acos_d);
									} else {
										if (string::Compare(routine_name, L"fpu_actg") == 0) return reinterpret_cast<const void *>(&_arcctg_f32);
									}
								}
							} else {
								if (string::Compare(routine_name, L"fpu_asin") < 0) {
									if (string::Compare(routine_name, L"fpu_actg_d") == 0) return reinterpret_cast<const void *>(&_arcctg_f64);
								} else {
									if (string::Compare(routine_name, L"fpu_asin_d") < 0) {
										if (string::Compare(routine_name, L"fpu_asin") == 0) return reinterpret_cast<const void *>(&asinf);
									} else {
										if (string::Compare(routine_name, L"fpu_asin_d") == 0) return reinterpret_cast<const void *>(&_asin_d);
									}
								}
							}
						} else {
							if (string::Compare(routine_name, L"fpu_c1_d") < 0) {
								if (string::Compare(routine_name, L"fpu_atg_d") < 0) {
									if (string::Compare(routine_name, L"fpu_atg") == 0) return reinterpret_cast<const void *>(&atanf);
								} else {
									if (string::Compare(routine_name, L"fpu_c1") < 0) {
										if (string::Compare(routine_name, L"fpu_atg_d") == 0) return reinterpret_cast<const void *>(&_atan_d);
									} else {
										if (string::Compare(routine_name, L"fpu_c1") == 0) return reinterpret_cast<const void *>(&roundf);
									}
								}
							} else {
								if (string::Compare(routine_name, L"fpu_c2_d") < 0) {
									if (string::Compare(routine_name, L"fpu_c2") < 0) {
										if (string::Compare(routine_name, L"fpu_c1_d") == 0) return reinterpret_cast<const void *>(&_round_d);
									} else {
										if (string::Compare(routine_name, L"fpu_c2") == 0) return reinterpret_cast<const void *>(&truncf);
									}
								} else {
									if (string::Compare(routine_name, L"fpu_c3") < 0) {
										if (string::Compare(routine_name, L"fpu_c2_d") == 0) return reinterpret_cast<const void *>(&_trunc_d);
									} else {
										if (string::Compare(routine_name, L"fpu_c3") == 0) return reinterpret_cast<const void *>(&floorf);
									}
								}
							}
						}
					} else {
						if (string::Compare(routine_name, L"fpu_ei_32") < 0) {
							if (string::Compare(routine_name, L"fpu_cos") < 0) {
								if (string::Compare(routine_name, L"fpu_c4") < 0) {
									if (string::Compare(routine_name, L"fpu_c3_d") == 0) return reinterpret_cast<const void *>(&_floor_d);
								} else {
									if (string::Compare(routine_name, L"fpu_c4_d") < 0) {
										if (string::Compare(routine_name, L"fpu_c4") == 0) return reinterpret_cast<const void *>(&ceilf);
									} else {
										if (string::Compare(routine_name, L"fpu_c4_d") == 0) return reinterpret_cast<const void *>(&_ceil_d);
									}
								}
							} else {
								if (string::Compare(routine_name, L"fpu_ctg") < 0) {
									if (string::Compare(routine_name, L"fpu_cos_d") < 0) {
										if (string::Compare(routine_name, L"fpu_cos") == 0) return reinterpret_cast<const void *>(&cosf);
									} else {
										if (string::Compare(routine_name, L"fpu_cos_d") == 0) return reinterpret_cast<const void *>(&_cos_d);
									}
								} else {
									if (string::Compare(routine_name, L"fpu_ctg_d") < 0) {
										if (string::Compare(routine_name, L"fpu_ctg") == 0) return reinterpret_cast<const void *>(&_ctg_f32);
									} else {
										if (string::Compare(routine_name, L"fpu_ctg_d") == 0) return reinterpret_cast<const void *>(&_ctg_f64);
									}
								}
							}
						} else {
							if (string::Compare(routine_name, L"fpu_enn_64") < 0) {
								if (string::Compare(routine_name, L"fpu_ei_64") < 0) {
									if (string::Compare(routine_name, L"fpu_ei_32") == 0) return reinterpret_cast<const void *>(&_is_inf_f32);
								} else {
									if (string::Compare(routine_name, L"fpu_enn_32") < 0) {
										if (string::Compare(routine_name, L"fpu_ei_64") == 0) return reinterpret_cast<const void *>(&_is_inf_f64);
									} else {
										if (string::Compare(routine_name, L"fpu_enn_32") == 0) return reinterpret_cast<const void *>(&_is_nan_f32);
									}
								}
							} else {
								if (string::Compare(routine_name, L"fpu_exp_d") < 0) {
									if (string::Compare(routine_name, L"fpu_exp") < 0) {
										if (string::Compare(routine_name, L"fpu_enn_64") == 0) return reinterpret_cast<const void *>(&_is_nan_f64);
									} else {
										if (string::Compare(routine_name, L"fpu_exp") == 0) return reinterpret_cast<const void *>(&expf);
									}
								} else {
									if (string::Compare(routine_name, L"fpu_f32_ni") < 0) {
										if (string::Compare(routine_name, L"fpu_exp_d") == 0) return reinterpret_cast<const void *>(&_exp_d);
									} else {
										if (string::Compare(routine_name, L"fpu_f32_ni") == 0) return reinterpret_cast<const void *>(&_inf_neg_f32);
									}
								}
							}
						}
					}
				} else {
					if (string::Compare(routine_name, L"fpu_ncg_3") < 0) {
						if (string::Compare(routine_name, L"fpu_lb_d") < 0) {
							if (string::Compare(routine_name, L"fpu_f64_nn") < 0) {
								if (string::Compare(routine_name, L"fpu_f32_pi") < 0) {
									if (string::Compare(routine_name, L"fpu_f32_nn") == 0) return reinterpret_cast<const void *>(&_inf_nan_f32);
								} else {
									if (string::Compare(routine_name, L"fpu_f64_ni") < 0) {
										if (string::Compare(routine_name, L"fpu_f32_pi") == 0) return reinterpret_cast<const void *>(&_inf_pos_f32);
									} else {
										if (string::Compare(routine_name, L"fpu_f64_ni") == 0) return reinterpret_cast<const void *>(&_inf_neg_f64);
									}
								}
							} else {
								if (string::Compare(routine_name, L"fpu_f64_pi") < 0) {
									if (string::Compare(routine_name, L"fpu_f64_nn") == 0) return reinterpret_cast<const void *>(&_inf_nan_f64);
								} else {
									if (string::Compare(routine_name, L"fpu_lb") < 0) {
										if (string::Compare(routine_name, L"fpu_f64_pi") == 0) return reinterpret_cast<const void *>(&_inf_pos_f64);
									} else {
										if (string::Compare(routine_name, L"fpu_lb") == 0) return reinterpret_cast<const void *>(&log2f);
									}
								}
							}
						} else {
							if (string::Compare(routine_name, L"fpu_ln") < 0) {
								if (string::Compare(routine_name, L"fpu_lg") < 0) {
									if (string::Compare(routine_name, L"fpu_lb_d") == 0) return reinterpret_cast<const void *>(&_log2_d);
								} else {
									if (string::Compare(routine_name, L"fpu_lg_d") < 0) {
										if (string::Compare(routine_name, L"fpu_lg") == 0) return reinterpret_cast<const void *>(&log10f);
									} else {
										if (string::Compare(routine_name, L"fpu_lg_d") == 0) return reinterpret_cast<const void *>(&_log10_d);
									}
								}
							} else {
								if (string::Compare(routine_name, L"fpu_ncg_1") < 0) {
									if (string::Compare(routine_name, L"fpu_ln_d") < 0) {
										if (string::Compare(routine_name, L"fpu_ln") == 0) return reinterpret_cast<const void *>(&logf);
									} else {
										if (string::Compare(routine_name, L"fpu_ln_d") == 0) return reinterpret_cast<const void *>(&_log_d);
									}
								} else {
									if (string::Compare(routine_name, L"fpu_ncg_2") < 0) {
										if (string::Compare(routine_name, L"fpu_ncg_1") == 0) return reinterpret_cast<const void *>(&_random_data);
									} else {
										if (string::Compare(routine_name, L"fpu_ncg_2") == 0) return reinterpret_cast<const void *>(&_random_int);
									}
								}
							}
						}
					} else {
						if (string::Compare(routine_name, L"fpu_sgn_i64") < 0) {
							if (string::Compare(routine_name, L"fpu_sgn_f32") < 0) {
								if (string::Compare(routine_name, L"fpu_pot") < 0) {
									if (string::Compare(routine_name, L"fpu_ncg_3") == 0) return reinterpret_cast<const void *>(&Engine::Math::Random::RandomDouble);
								} else {
									if (string::Compare(routine_name, L"fpu_pot_d") < 0) {
										if (string::Compare(routine_name, L"fpu_pot") == 0) return reinterpret_cast<const void *>(&powf);
									} else {
										if (string::Compare(routine_name, L"fpu_pot_d") == 0) return reinterpret_cast<const void *>(&_pow_d);
									}
								}
							} else {
								if (string::Compare(routine_name, L"fpu_sgn_i16") < 0) {
									if (string::Compare(routine_name, L"fpu_sgn_f64") < 0) {
										if (string::Compare(routine_name, L"fpu_sgn_f32") == 0) return reinterpret_cast<const void *>(&_sgn_f32);
									} else {
										if (string::Compare(routine_name, L"fpu_sgn_f64") == 0) return reinterpret_cast<const void *>(&_sgn_f64);
									}
								} else {
									if (string::Compare(routine_name, L"fpu_sgn_i32") < 0) {
										if (string::Compare(routine_name, L"fpu_sgn_i16") == 0) return reinterpret_cast<const void *>(&_sgn_i16);
									} else {
										if (string::Compare(routine_name, L"fpu_sgn_i32") == 0) return reinterpret_cast<const void *>(&_sgn_i32);
									}
								}
							}
						} else {
							if (string::Compare(routine_name, L"fpu_sin") < 0) {
								if (string::Compare(routine_name, L"fpu_sgn_i8") < 0) {
									if (string::Compare(routine_name, L"fpu_sgn_i64") == 0) return reinterpret_cast<const void *>(&_sgn_i64);
								} else {
									if (string::Compare(routine_name, L"fpu_sgn_iad") < 0) {
										if (string::Compare(routine_name, L"fpu_sgn_i8") == 0) return reinterpret_cast<const void *>(&_sgn_i8);
									} else {
										if (string::Compare(routine_name, L"fpu_sgn_iad") == 0) return reinterpret_cast<const void *>(&_sgn_iw);
									}
								}
							} else {
								if (string::Compare(routine_name, L"fpu_tg") < 0) {
									if (string::Compare(routine_name, L"fpu_sin_d") < 0) {
										if (string::Compare(routine_name, L"fpu_sin") == 0) return reinterpret_cast<const void *>(&sinf);
									} else {
										if (string::Compare(routine_name, L"fpu_sin_d") == 0) return reinterpret_cast<const void *>(&_sin_d);
									}
								} else {
									if (string::Compare(routine_name, L"fpu_tg_d") < 0) {
										if (string::Compare(routine_name, L"fpu_tg") == 0) return reinterpret_cast<const void *>(&tanf);
									} else {
										if (string::Compare(routine_name, L"fpu_tg_d") == 0) return reinterpret_cast<const void *>(&_tan_d);
									}
								}
							}
						}
					}
				}
				return 0;
			}
			virtual const void * ExposeInterface(const string & interface) noexcept override { return 0; }
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
			#ifndef ENGINE_WINDOWS
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
			#else
			;
			#endif
			static bool _check_arch(int arch)
			{
				if (arch == 0x01) return IsPlatformAvailable(Platform::X86);
				else if (arch == 0x11) return IsPlatformAvailable(Platform::X64);
				else if (arch == 0x02) return IsPlatformAvailable(Platform::ARM);
				else if (arch == 0x12) return IsPlatformAvailable(Platform::ARM64);
				else return false;
			}
		public:
			virtual const void * ExposeRoutine(const string & routine_name) noexcept override
			{
				if (string::Compare(routine_name, L"lege_in_filo") < 0) {
					if (string::Compare(routine_name, L"dimitte_in_filo") < 0) {
						if (string::Compare(routine_name, L"alloca_memoriam") < 0) {
							if (string::Compare(routine_name, L"alloca_in_filo") == 0) return reinterpret_cast<const void *>(&Engine::CreateThreadLocal);
						} else {
							if (string::Compare(routine_name, L"dec_sec") < 0) {
								if (string::Compare(routine_name, L"alloca_memoriam") == 0) return reinterpret_cast<const void *>(&_mem_alloc);
							} else {
								if (string::Compare(routine_name, L"dec_sec") == 0) return reinterpret_cast<const void *>(&Engine::InterlockedDecrement);
							}
						}
					} else {
						if (string::Compare(routine_name, L"exscribe_memoriam") < 0) {
							if (string::Compare(routine_name, L"dimitte_memoriam") < 0) {
								if (string::Compare(routine_name, L"dimitte_in_filo") == 0) return reinterpret_cast<const void *>(&Engine::ReleaseThreadLocal);
							} else {
								if (string::Compare(routine_name, L"dimitte_memoriam") == 0) return reinterpret_cast<const void *>(&_mem_release);
							}
						} else {
							if (string::Compare(routine_name, L"inc_sec") < 0) {
								if (string::Compare(routine_name, L"exscribe_memoriam") == 0) return reinterpret_cast<const void *>(&Engine::MemoryCopy);
							} else {
								if (string::Compare(routine_name, L"inc_sec") == 0) return reinterpret_cast<const void *>(&Engine::InterlockedIncrement);
							}
						}
					}
				} else {
					if (string::Compare(routine_name, L"sys_arch") < 0) {
						if (string::Compare(routine_name, L"relabe_memoriam") < 0) {
							if (string::Compare(routine_name, L"realloca_memoriam") < 0) {
								if (string::Compare(routine_name, L"lege_in_filo") == 0) return reinterpret_cast<const void *>(&Engine::GetThreadLocal);
							} else {
								if (string::Compare(routine_name, L"realloca_memoriam") == 0) return reinterpret_cast<const void *>(&_mem_realloc);
							}
						} else {
							if (string::Compare(routine_name, L"scribe_in_filo") < 0) {
								if (string::Compare(routine_name, L"relabe_memoriam") == 0) return reinterpret_cast<const void *>(&Engine::ZeroMemory);
							} else {
								if (string::Compare(routine_name, L"scribe_in_filo") == 0) return reinterpret_cast<const void *>(&Engine::SetThreadLocal);
							}
						}
					} else {
						if (string::Compare(routine_name, L"sys_info") < 0) {
							if (string::Compare(routine_name, L"sys_func") < 0) {
								if (string::Compare(routine_name, L"sys_arch") == 0) return reinterpret_cast<const void *>(&_check_arch);
							} else {
								if (string::Compare(routine_name, L"sys_func") == 0) return reinterpret_cast<const void *>(&Engine::XA::CheckThisMachineCPUFeature);
							}
						} else {
							if (string::Compare(routine_name, L"sys_temp") < 0) {
								if (string::Compare(routine_name, L"sys_info") == 0) return reinterpret_cast<const void *>(&_get_system_info);
							} else {
								if (string::Compare(routine_name, L"sys_temp") == 0) return reinterpret_cast<const void *>(&Engine::GetTimerValue);
							}
						}
					}
				}
				return 0;
			}
			virtual const void * ExposeInterface(const string & interface) noexcept override { return 0; }
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
			virtual const void * ExposeRoutine(const string & routine_name) noexcept override
			{
				if (string::Compare(routine_name, L"spu_crea_int64") < 0) {
					if (string::Compare(routine_name, L"spu_codifica") < 0) {
						if (string::Compare(routine_name, L"spu_ad_int64_2") < 0) {
							if (string::Compare(routine_name, L"spu_ad_int32") < 0) {
								if (string::Compare(routine_name, L"spu_ad_frac64") < 0) {
									if (string::Compare(routine_name, L"spu_ad_frac32_2") < 0) {
										if (string::Compare(routine_name, L"spu_ad_frac32") == 0) return reinterpret_cast<const void *>(_to_float);
									} else {
										if (string::Compare(routine_name, L"spu_ad_frac32_2") == 0) return reinterpret_cast<const void *>(_to_float_2);
									}
								} else {
									if (string::Compare(routine_name, L"spu_ad_frac64_2") < 0) {
										if (string::Compare(routine_name, L"spu_ad_frac64") == 0) return reinterpret_cast<const void *>(_to_double);
									} else {
										if (string::Compare(routine_name, L"spu_ad_frac64_2") == 0) return reinterpret_cast<const void *>(_to_double_2);
									}
								}
							} else {
								if (string::Compare(routine_name, L"spu_ad_int32_3") < 0) {
									if (string::Compare(routine_name, L"spu_ad_int32_2") < 0) {
										if (string::Compare(routine_name, L"spu_ad_int32") == 0) return reinterpret_cast<const void *>(_to_int32);
									} else {
										if (string::Compare(routine_name, L"spu_ad_int32_2") == 0) return reinterpret_cast<const void *>(_to_int32_2);
									}
								} else {
									if (string::Compare(routine_name, L"spu_ad_int64") < 0) {
										if (string::Compare(routine_name, L"spu_ad_int32_3") == 0) return reinterpret_cast<const void *>(_to_int32_3);
									} else {
										if (string::Compare(routine_name, L"spu_ad_int64") == 0) return reinterpret_cast<const void *>(_to_int64);
									}
								}
							}
						} else {
							if (string::Compare(routine_name, L"spu_ad_nint32_3") < 0) {
								if (string::Compare(routine_name, L"spu_ad_nint32") < 0) {
									if (string::Compare(routine_name, L"spu_ad_int64_3") < 0) {
										if (string::Compare(routine_name, L"spu_ad_int64_2") == 0) return reinterpret_cast<const void *>(_to_int64_2);
									} else {
										if (string::Compare(routine_name, L"spu_ad_int64_3") == 0) return reinterpret_cast<const void *>(_to_int64_3);
									}
								} else {
									if (string::Compare(routine_name, L"spu_ad_nint32_2") < 0) {
										if (string::Compare(routine_name, L"spu_ad_nint32") == 0) return reinterpret_cast<const void *>(_to_uint32);
									} else {
										if (string::Compare(routine_name, L"spu_ad_nint32_2") == 0) return reinterpret_cast<const void *>(_to_uint32_2);
									}
								}
							} else {
								if (string::Compare(routine_name, L"spu_ad_nint64_2") < 0) {
									if (string::Compare(routine_name, L"spu_ad_nint64") < 0) {
										if (string::Compare(routine_name, L"spu_ad_nint32_3") == 0) return reinterpret_cast<const void *>(_to_uint32_3);
									} else {
										if (string::Compare(routine_name, L"spu_ad_nint64") == 0) return reinterpret_cast<const void *>(_to_uint64);
									}
								} else {
									if (string::Compare(routine_name, L"spu_ad_nint64_3") < 0) {
										if (string::Compare(routine_name, L"spu_ad_nint64_2") == 0) return reinterpret_cast<const void *>(_to_uint64_2);
									} else {
										if (string::Compare(routine_name, L"spu_codex_long") < 0) {
											if (string::Compare(routine_name, L"spu_ad_nint64_3") == 0) return reinterpret_cast<const void *>(_to_uint64_3);
										} else {
											if (string::Compare(routine_name, L"spu_codex_long") == 0) return reinterpret_cast<const void *>(_encode_length);
										}
									}
								}
							}
						}
					} else {
						if (string::Compare(routine_name, L"spu_crea_data") < 0) {
							if (string::Compare(routine_name, L"spu_concat") < 0) {
								if (string::Compare(routine_name, L"spu_compareo") < 0) {
									if (string::Compare(routine_name, L"spu_codifica_taleam") < 0) {
										if (string::Compare(routine_name, L"spu_codifica") == 0) return reinterpret_cast<const void *>(_encode);
									} else {
										if (string::Compare(routine_name, L"spu_codifica_taleam") == 0) return reinterpret_cast<const void *>(_encode_block);
									}
								} else {
									if (string::Compare(routine_name, L"spu_compareo_2") < 0) {
										if (string::Compare(routine_name, L"spu_compareo") == 0) return reinterpret_cast<const void *>(_static_order);
									} else {
										if (string::Compare(routine_name, L"spu_compareo_2") == 0) return reinterpret_cast<const void *>(_static_order_nc);
									}
								}
							} else {
								if (string::Compare(routine_name, L"spu_crea") < 0) {
									if (string::Compare(routine_name, L"spu_concat_classis") < 0) {
										if (string::Compare(routine_name, L"spu_concat") == 0) return reinterpret_cast<const void *>(_concat);
									} else {
										if (string::Compare(routine_name, L"spu_concat_classis") == 0) return reinterpret_cast<const void *>(_static_concat);
									}
								} else {
									if (string::Compare(routine_name, L"spu_crea_adl") < 0) {
										if (string::Compare(routine_name, L"spu_crea") == 0) return reinterpret_cast<const void *>(_init_string_with_string);
									} else {
										if (string::Compare(routine_name, L"spu_crea_char") < 0) {
											if (string::Compare(routine_name, L"spu_crea_adl") == 0) return reinterpret_cast<const void *>(_init_string_with_ptr);
										} else {
											if (string::Compare(routine_name, L"spu_crea_char") == 0) return reinterpret_cast<const void *>(_init_string_with_char);
										}
									}
								}
							}
						} else {
							if (string::Compare(routine_name, L"spu_crea_frac64") < 0) {
								if (string::Compare(routine_name, L"spu_crea_frac32_2") < 0) {
									if (string::Compare(routine_name, L"spu_crea_frac32") < 0) {
										if (string::Compare(routine_name, L"spu_crea_data") == 0) return reinterpret_cast<const void *>(_init_string_with_data);
									} else {
										if (string::Compare(routine_name, L"spu_crea_frac32") == 0) return reinterpret_cast<const void *>(_init_string_with_float);
									}
								} else {
									if (string::Compare(routine_name, L"spu_crea_frac32_3") < 0) {
										if (string::Compare(routine_name, L"spu_crea_frac32_2") == 0) return reinterpret_cast<const void *>(_init_string_with_float_del);
									} else {
										if (string::Compare(routine_name, L"spu_crea_frac32_3") == 0) return reinterpret_cast<const void *>(_init_string_with_float_del_len);
									}
								}
							} else {
								if (string::Compare(routine_name, L"spu_crea_frac64_3") < 0) {
									if (string::Compare(routine_name, L"spu_crea_frac64_2") < 0) {
										if (string::Compare(routine_name, L"spu_crea_frac64") == 0) return reinterpret_cast<const void *>(_init_string_with_double);
									} else {
										if (string::Compare(routine_name, L"spu_crea_frac64_2") == 0) return reinterpret_cast<const void *>(_init_string_with_double_del);
									}
								} else {
									if (string::Compare(routine_name, L"spu_crea_int16") < 0) {
										if (string::Compare(routine_name, L"spu_crea_frac64_3") == 0) return reinterpret_cast<const void *>(_init_string_with_double_del_len);
									} else {
										if (string::Compare(routine_name, L"spu_crea_int32") < 0) {
											if (string::Compare(routine_name, L"spu_crea_int16") == 0) return reinterpret_cast<const void *>(_init_string_with_int16);
										} else {
											if (string::Compare(routine_name, L"spu_crea_int32") == 0) return reinterpret_cast<const void *>(_init_string_with_int32);
										}
									}
								}
							}
						}
					}
				} else {
					if (string::Compare(routine_name, L"spu_forma_3") < 0) {
						if (string::Compare(routine_name, L"spu_crea_nint64_radix") < 0) {
							if (string::Compare(routine_name, L"spu_crea_nint16") < 0) {
								if (string::Compare(routine_name, L"spu_crea_intadl") < 0) {
									if (string::Compare(routine_name, L"spu_crea_int8") < 0) {
										if (string::Compare(routine_name, L"spu_crea_int64") == 0) return reinterpret_cast<const void *>(_init_string_with_int64);
									} else {
										if (string::Compare(routine_name, L"spu_crea_int8") == 0) return reinterpret_cast<const void *>(_init_string_with_int8);
									}
								} else {
									if (string::Compare(routine_name, L"spu_crea_logicum") < 0) {
										if (string::Compare(routine_name, L"spu_crea_intadl") == 0) return reinterpret_cast<const void *>(_init_string_with_intptr);
									} else {
										if (string::Compare(routine_name, L"spu_crea_logicum") == 0) return reinterpret_cast<const void *>(_init_string_with_boolean);
									}
								}
							} else {
								if (string::Compare(routine_name, L"spu_crea_nint32_radix") < 0) {
									if (string::Compare(routine_name, L"spu_crea_nint32") < 0) {
										if (string::Compare(routine_name, L"spu_crea_nint16") == 0) return reinterpret_cast<const void *>(_init_string_with_uint16);
									} else {
										if (string::Compare(routine_name, L"spu_crea_nint32") == 0) return reinterpret_cast<const void *>(_init_string_with_uint32);
									}
								} else {
									if (string::Compare(routine_name, L"spu_crea_nint64") < 0) {
										if (string::Compare(routine_name, L"spu_crea_nint32_radix") == 0) return reinterpret_cast<const void *>(_init_string_with_uint32_radix);
									} else {
										if (string::Compare(routine_name, L"spu_crea_nint64") == 0) return reinterpret_cast<const void *>(_init_string_with_uint64);
									}
								}
							}
						} else {
							if (string::Compare(routine_name, L"spu_crea_sec") < 0) {
								if (string::Compare(routine_name, L"spu_crea_nintadl") < 0) {
									if (string::Compare(routine_name, L"spu_crea_nint8") < 0) {
										if (string::Compare(routine_name, L"spu_crea_nint64_radix") == 0) return reinterpret_cast<const void *>(_init_string_with_uint64_radix);
									} else {
										if (string::Compare(routine_name, L"spu_crea_nint8") == 0) return reinterpret_cast<const void *>(_init_string_with_uint8);
									}
								} else {
									if (string::Compare(routine_name, L"spu_crea_nintadl_radix") < 0) {
										if (string::Compare(routine_name, L"spu_crea_nintadl") == 0) return reinterpret_cast<const void *>(_init_string_with_uintptr);
									} else {
										if (string::Compare(routine_name, L"spu_crea_nintadl_radix") == 0) return reinterpret_cast<const void *>(_init_string_with_uintptr_radix);
									}
								}
							} else {
								if (string::Compare(routine_name, L"spu_forma_0") < 0) {
									if (string::Compare(routine_name, L"spu_crea_utf32") < 0) {
										if (string::Compare(routine_name, L"spu_crea_sec") == 0) return reinterpret_cast<const void *>(_init_string_with_string_nothrow);
									} else {
										if (string::Compare(routine_name, L"spu_crea_utf32") == 0) return reinterpret_cast<const void *>(_init_string_with_utf32);
									}
								} else {
									if (string::Compare(routine_name, L"spu_forma_1") < 0) {
										if (string::Compare(routine_name, L"spu_forma_0") == 0) return reinterpret_cast<const void *>(_format_0);
									} else {
										if (string::Compare(routine_name, L"spu_forma_2") < 0) {
											if (string::Compare(routine_name, L"spu_forma_1") == 0) return reinterpret_cast<const void *>(_format_1);
										} else {
											if (string::Compare(routine_name, L"spu_forma_2") == 0) return reinterpret_cast<const void *>(_format_2);
										}
									}
								}
							}
						}
					} else {
						if (string::Compare(routine_name, L"spu_inferna") < 0) {
							if (string::Compare(routine_name, L"spu_forma_7") < 0) {
								if (string::Compare(routine_name, L"spu_forma_5") < 0) {
									if (string::Compare(routine_name, L"spu_forma_4") < 0) {
										if (string::Compare(routine_name, L"spu_forma_3") == 0) return reinterpret_cast<const void *>(_format_3);
									} else {
										if (string::Compare(routine_name, L"spu_forma_4") == 0) return reinterpret_cast<const void *>(_format_4);
									}
								} else {
									if (string::Compare(routine_name, L"spu_forma_6") < 0) {
										if (string::Compare(routine_name, L"spu_forma_5") == 0) return reinterpret_cast<const void *>(_format_5);
									} else {
										if (string::Compare(routine_name, L"spu_forma_6") == 0) return reinterpret_cast<const void *>(_format_6);
									}
								}
							} else {
								if (string::Compare(routine_name, L"spu_forma_9") < 0) {
									if (string::Compare(routine_name, L"spu_forma_8") < 0) {
										if (string::Compare(routine_name, L"spu_forma_7") == 0) return reinterpret_cast<const void *>(_format_7);
									} else {
										if (string::Compare(routine_name, L"spu_forma_8") == 0) return reinterpret_cast<const void *>(_format_8);
									}
								} else {
									if (string::Compare(routine_name, L"spu_fragmentum") < 0) {
										if (string::Compare(routine_name, L"spu_forma_9") == 0) return reinterpret_cast<const void *>(_format_9);
									} else {
										if (string::Compare(routine_name, L"spu_index") < 0) {
											if (string::Compare(routine_name, L"spu_fragmentum") == 0) return reinterpret_cast<const void *>(_fragment);
										} else {
											if (string::Compare(routine_name, L"spu_index") == 0) return reinterpret_cast<const void *>(_subscript);
										}
									}
								}
							}
						} else {
							if (string::Compare(routine_name, L"spu_reperi_primus") < 0) {
								if (string::Compare(routine_name, L"spu_loca_utf32") < 0) {
									if (string::Compare(routine_name, L"spu_loca") < 0) {
										if (string::Compare(routine_name, L"spu_inferna") == 0) return reinterpret_cast<const void *>(_lower_case);
									} else {
										if (string::Compare(routine_name, L"spu_loca") == 0) return reinterpret_cast<const void *>(_assign_with_string);
									}
								} else {
									if (string::Compare(routine_name, L"spu_long") < 0) {
										if (string::Compare(routine_name, L"spu_loca_utf32") == 0) return reinterpret_cast<const void *>(_assign_with_utf32);
									} else {
										if (string::Compare(routine_name, L"spu_long") == 0) return reinterpret_cast<const void *>(_length);
									}
								}
							} else {
								if (string::Compare(routine_name, L"spu_scinde") < 0) {
									if (string::Compare(routine_name, L"spu_reperi_ultimus") < 0) {
										if (string::Compare(routine_name, L"spu_reperi_primus") == 0) return reinterpret_cast<const void *>(_find_first);
									} else {
										if (string::Compare(routine_name, L"spu_reperi_ultimus") == 0) return reinterpret_cast<const void *>(_find_last);
									}
								} else {
									if (string::Compare(routine_name, L"spu_supera") < 0) {
										if (string::Compare(routine_name, L"spu_scinde") == 0) return reinterpret_cast<const void *>(_split);
									} else {
										if (string::Compare(routine_name, L"spu_surroga") < 0) {
											if (string::Compare(routine_name, L"spu_supera") == 0) return reinterpret_cast<const void *>(_upper_case);
										} else {
											if (string::Compare(routine_name, L"spu_surroga") == 0) return reinterpret_cast<const void *>(_replace);
										}
									}
								}
							}
						}
					}
				}
				return 0;
			}
			virtual const void * ExposeInterface(const string & interface) noexcept override { return 0; }
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
			class _semaphore : public Object
			{
				SafePointer<Semaphore> _sem;
			public:
				_semaphore(Semaphore * sem) { _sem.SetRetain(sem); if (!_sem) throw OutOfMemoryException(); }
				virtual ~_semaphore(void) override {}
				virtual string ToString(void) const override { try { return L"XE Semaphore"; } catch (...) { return L""; } }
				virtual void _wait(void) noexcept { _sem->Wait(); }
				virtual bool _wait_for(uint quant) noexcept { if (quant) return _sem->WaitFor(quant); else return _sem->TryWait(); }
				virtual void _open(void) noexcept { _sem->Open(); }
			};
			class _signal : public Object
			{
				SafePointer<Signal> _sgn;
			public:
				_signal(void) { _sgn = CreateSignal(false); if (!_sgn) throw OutOfMemoryException(); }
				virtual ~_signal(void) override {}
				virtual string ToString(void) const override { try { return L"XE Signal"; } catch (...) { return L""; } }
				virtual void _wait(void) noexcept { _sgn->Wait(); }
				virtual bool _wait_for(uint quant) noexcept { if (quant) return _sgn->WaitFor(quant); else return _sgn->TryWait(); }
				virtual void _raise(void) noexcept { _sgn->Raise(); }
				virtual void _lower(void) noexcept { _sgn->Reset(); }
			};
			class _thread : public Object
			{
				SafePointer<Thread> _th;
			public:
				_thread(Thread * th) { _th.SetRetain(th); }
				virtual ~_thread(void) override {}
				virtual string ToString(void) const override { try { return L"XE Thread"; } catch (...) { return L""; } }
				virtual void _wait(void) noexcept { _th->Wait(); }
				virtual bool _is_active(void) noexcept { return !_th->Exited(); }
				virtual int _exit_code(void) noexcept { return _th->GetExitCode(); }
			};
			class _task_queue : public XDispatchContext
			{
				TaskQueue * _disp;
			public:
				_task_queue(TaskQueue * queue) : _disp(queue), XDispatchContext(queue) {}
				virtual ~_task_queue() override {}
				virtual void _execute(void) noexcept { try { _disp->Process(); } catch (...) {} }
				virtual bool _execute_once(void) noexcept { try { return _disp->ProcessOnce(); } catch (...) { return false; } }
				virtual SafePointer<_thread> _execute_in_thread(void) noexcept
				{
					try {
						SafePointer<Thread> thread;
						if (!_disp->ProcessAsSeparateThread(thread.InnerRef())) return 0;
						return new _thread(thread);
					} catch (...) { return 0; }
				}
				virtual void _break(void) noexcept { try { _disp->Break(); } catch (...) {} }
				virtual void _quit(void) noexcept { try { _disp->Quit(); } catch (...) {} }
				virtual int _length(void) noexcept { try { return _disp->GetTaskQueueLength(); } catch (...) { return -1; } }
			};
			class _thread_pool : public XDispatchContext
			{
				ThreadPool * _pool;
			public:
				_thread_pool(ThreadPool * pool) : _pool(pool), XDispatchContext(pool) {}
				virtual ~_thread_pool() override {}
				virtual void _get_state(int & num_threads, int & num_active_thread, int & num_tasks) noexcept
				{
					try {
						num_threads = _pool->GetThreadCount();
						num_active_thread = _pool->GetActiveThreads();
						num_tasks = _pool->GetTaskQueueLength();
					} catch (...) {}
				}
				virtual void _wait(void) noexcept { try { _pool->Wait(); } catch (...) {} }
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
			static uint64 _get_time_universal(Time * self) { return self->ToUniversal().Ticks; }
			static uint64 _get_time_local(Time * self) { return self->ToLocal().Ticks; }
			static uint64 _get_time_current(void) { return Time::GetCurrentTime().Ticks; }
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
			static SafePointer<XStream> _create_resource_stream(const Module * mdl, const string & type, int id, ErrorContext & ectx)
			{
				try {
					auto & rsrc = mdl->GetResources();
					auto res = rsrc.GetElementByKey(XI::MakeResourceID(type, id));
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
			static void _sleep(uint time) noexcept { Sleep(time); }
			static void _terminate(uint code) noexcept { ExitProcess(code); }
			static SafePointer<_task_queue> _create_task_queue(void) noexcept
			{
				try {
					SafePointer<TaskQueue> queue = new TaskQueue;
					return new _task_queue(queue);
				} catch (...) { return 0; }
			}
			static SafePointer<_thread_pool> _create_thread_pool_0(void) noexcept
			{
				try {
					SafePointer<ThreadPool> pool = new ThreadPool;
					return new _thread_pool(pool);
				} catch (...) { return 0; }
			}
			static SafePointer<_thread_pool> _create_thread_pool_1(int threads) noexcept
			{
				try {
					SafePointer<ThreadPool> pool = new ThreadPool(threads);
					return new _thread_pool(pool);
				} catch (...) { return 0; }
			}
			static SafePointer<_semaphore> _create_semaphore(int value) noexcept
			{
				try {
					SafePointer<Semaphore> sem = CreateSemaphore(value);
					return new _semaphore(sem);
				} catch (...) { return 0; }
			}
			static SafePointer<_signal> _create_signal(void) noexcept { try { return new _signal; } catch (...) { return 0; } }
			static SafePointer<_thread> _create_thread(IDispatchTask * task) noexcept
			{
				SafePointer<Thread> thread;
				task->Retain();
				try {
					thread = CreateThread(reinterpret_cast<ThreadRoutine>(_do_task_thread), task);
					if (!thread) throw Exception();
				} catch (...) { task->Release(); return 0; }
				try { return new _thread(thread); } catch (...) { return 0; }
			}
			static int _do_task_thread(IDispatchTask * task) noexcept { task->DoTask(0); task->Release(); return 0; }
			static SafePointer<Object> _query_metadata(const Module * mdl, ErrorContext & ectx)
			{
				try {
					SafePointer< Volumes::Dictionary<string, string> > meta = XI::LoadModuleMetadata(mdl->GetResources());
					return CreateMetadataDictionary(meta);
				} catch (...) { ectx.error_code = 2; ectx.error_subcode = 0; return 0; }
			}
		public:
			virtual const void * ExposeRoutine(const string & routine_name) noexcept override
			{
				if (string::Compare(routine_name, L"scr_cod_2") < 0) {
					if (string::Compare(routine_name, L"flumen_ex_1") < 0) {
						if (string::Compare(routine_name, L"ctx_crsem") < 0) {
							if (string::Compare(routine_name, L"ctx_ccsim") < 0) {
								if (string::Compare(routine_name, L"ctx_ccfl1") < 0) {
									if (string::Compare(routine_name, L"ctx_ccfl0") == 0) return reinterpret_cast<const void *>(_create_thread_pool_0);
								} else {
									if (string::Compare(routine_name, L"ctx_ccfl1") == 0) return reinterpret_cast<const void *>(_create_thread_pool_1);
								}
							} else {
								if (string::Compare(routine_name, L"ctx_crfil") < 0) {
									if (string::Compare(routine_name, L"ctx_ccsim") == 0) return reinterpret_cast<const void *>(_create_task_queue);
								} else {
									if (string::Compare(routine_name, L"ctx_crfil") == 0) return reinterpret_cast<const void *>(_create_thread);
								}
							}
						} else {
							if (string::Compare(routine_name, L"ctx_dormi") < 0) {
								if (string::Compare(routine_name, L"ctx_crsgn") < 0) {
									if (string::Compare(routine_name, L"ctx_crsem") == 0) return reinterpret_cast<const void *>(_create_semaphore);
								} else {
									if (string::Compare(routine_name, L"ctx_crsgn") == 0) return reinterpret_cast<const void *>(_create_signal);
								}
							} else {
								if (string::Compare(routine_name, L"ctx_siste") < 0) {
									if (string::Compare(routine_name, L"ctx_dormi") == 0) return reinterpret_cast<const void *>(_sleep);
								} else {
									if (string::Compare(routine_name, L"flumen_auxil") < 0) {
										if (string::Compare(routine_name, L"ctx_siste") == 0) return reinterpret_cast<const void *>(_terminate);
									} else {
										if (string::Compare(routine_name, L"flumen_auxil") == 0) return reinterpret_cast<const void *>(_create_resource_stream);
									}
								}
							}
						}
					} else {
						if (string::Compare(routine_name, L"flumen_mem_0") < 0) {
							if (string::Compare(routine_name, L"flumen_lo_0") < 0) {
								if (string::Compare(routine_name, L"flumen_ex_2") < 0) {
									if (string::Compare(routine_name, L"flumen_ex_1") == 0) return reinterpret_cast<const void *>(_stream_copy_to_1);
								} else {
									if (string::Compare(routine_name, L"flumen_ex_2") == 0) return reinterpret_cast<const void *>(_stream_copy_to_2);
								}
							} else {
								if (string::Compare(routine_name, L"flumen_lo_1") < 0) {
									if (string::Compare(routine_name, L"flumen_lo_0") == 0) return reinterpret_cast<const void *>(_stream_read_0);
								} else {
									if (string::Compare(routine_name, L"flumen_lo_1") == 0) return reinterpret_cast<const void *>(_stream_read_1);
								}
							}
						} else {
							if (string::Compare(routine_name, L"flumen_so_1") < 0) {
								if (string::Compare(routine_name, L"flumen_mem_2") < 0) {
									if (string::Compare(routine_name, L"flumen_mem_0") == 0) return reinterpret_cast<const void *>(_create_memory_stream_0);
								} else {
									if (string::Compare(routine_name, L"flumen_mem_2") == 0) return reinterpret_cast<const void *>(_create_memory_stream_2);
								}
							} else {
								if (string::Compare(routine_name, L"meta_moduli") < 0) {
									if (string::Compare(routine_name, L"flumen_so_1") == 0) return reinterpret_cast<const void *>(_stream_write);
								} else {
									if (string::Compare(routine_name, L"scr_cod_1") < 0) {
										if (string::Compare(routine_name, L"meta_moduli") == 0) return reinterpret_cast<const void *>(_query_metadata);
									} else {
										if (string::Compare(routine_name, L"scr_cod_1") == 0) return reinterpret_cast<const void *>(_create_writer_1);
									}
								}
							}
						}
					}
				} else {
					if (string::Compare(routine_name, L"tmp_a_mns") < 0) {
						if (string::Compare(routine_name, L"tmp_a_die") < 0) {
							if (string::Compare(routine_name, L"scr_dec_2") < 0) {
								if (string::Compare(routine_name, L"scr_dec_1") < 0) {
									if (string::Compare(routine_name, L"scr_cod_2") == 0) return reinterpret_cast<const void *>(_create_writer_2);
								} else {
									if (string::Compare(routine_name, L"scr_dec_1") == 0) return reinterpret_cast<const void *>(_create_reader_1);
								}
							} else {
								if (string::Compare(routine_name, L"tmp_a_ans") < 0) {
									if (string::Compare(routine_name, L"scr_dec_2") == 0) return reinterpret_cast<const void *>(_create_reader_2);
								} else {
									if (string::Compare(routine_name, L"tmp_a_ans") == 0) return reinterpret_cast<const void *>(_get_year);
								}
							}
						} else {
							if (string::Compare(routine_name, L"tmp_a_hor") < 0) {
								if (string::Compare(routine_name, L"tmp_a_dis") < 0) {
									if (string::Compare(routine_name, L"tmp_a_die") == 0) return reinterpret_cast<const void *>(_get_day);
								} else {
									if (string::Compare(routine_name, L"tmp_a_dis") == 0) return reinterpret_cast<const void *>(_get_day_of_week);
								}
							} else {
								if (string::Compare(routine_name, L"tmp_a_loc") < 0) {
									if (string::Compare(routine_name, L"tmp_a_hor") == 0) return reinterpret_cast<const void *>(_get_hour);
								} else {
									if (string::Compare(routine_name, L"tmp_a_min") < 0) {
										if (string::Compare(routine_name, L"tmp_a_loc") == 0) return reinterpret_cast<const void *>(_get_time_local);
									} else {
										if (string::Compare(routine_name, L"tmp_a_min") == 0) return reinterpret_cast<const void *>(_get_minute);
									}
								}
							}
						}
					} else {
						if (string::Compare(routine_name, L"tmp_alb") < 0) {
							if (string::Compare(routine_name, L"tmp_a_sec") < 0) {
								if (string::Compare(routine_name, L"tmp_a_msc") < 0) {
									if (string::Compare(routine_name, L"tmp_a_mns") == 0) return reinterpret_cast<const void *>(_get_month);
								} else {
									if (string::Compare(routine_name, L"tmp_a_msc") == 0) return reinterpret_cast<const void *>(_get_millisecond);
								}
							} else {
								if (string::Compare(routine_name, L"tmp_a_uni") < 0) {
									if (string::Compare(routine_name, L"tmp_a_sec") == 0) return reinterpret_cast<const void *>(_get_second);
								} else {
									if (string::Compare(routine_name, L"tmp_adl") < 0) {
										if (string::Compare(routine_name, L"tmp_a_uni") == 0) return reinterpret_cast<const void *>(_get_time_universal);
									} else {
										if (string::Compare(routine_name, L"tmp_adl") == 0) return reinterpret_cast<const void *>(_time_to_string);
									}
								}
							}
						} else {
							if (string::Compare(routine_name, L"tmp_crea_7") < 0) {
								if (string::Compare(routine_name, L"tmp_crea_4") < 0) {
									if (string::Compare(routine_name, L"tmp_alb") == 0) return reinterpret_cast<const void *>(_time_to_string_short);
								} else {
									if (string::Compare(routine_name, L"tmp_crea_4") == 0) return reinterpret_cast<const void *>(_time_init_4);
								}
							} else {
								if (string::Compare(routine_name, L"tmp_cur") < 0) {
									if (string::Compare(routine_name, L"tmp_crea_7") == 0) return reinterpret_cast<const void *>(_time_init_7);
								} else {
									if (string::Compare(routine_name, L"tmp_dim") < 0) {
										if (string::Compare(routine_name, L"tmp_cur") == 0) return reinterpret_cast<const void *>(_get_time_current);
									} else {
										if (string::Compare(routine_name, L"tmp_dim") == 0) return reinterpret_cast<const void *>(_time_days_in_month);
									}
								}
							}
						}
					}
				}
				return 0;
			}
			virtual const void * ExposeInterface(const string & interface) noexcept override { return 0; }
		};

		IAPIExtension * CreateFPU(void) { return new FPU; }
		IAPIExtension * CreateMMU(void) { return new MMU; }
		IAPIExtension * CreateSPU(void) { return new SPU; }
		IAPIExtension * CreateMiscUnit(void) { return new MiscUnit; }
	}
}

#ifdef ENGINE_WINDOWS
#include <Windows.h>

#undef interface
#undef InterlockedDecrement
#undef InterlockedIncrement
#undef ZeroMemory
#undef GetCurrentTime
#undef CreateSemaphore

namespace Engine
{
	namespace XE
	{
		typedef BOOL (WINAPI * func_GetPhysicallyInstalledSystemMemory) (uint64 * mem);
		typedef BOOL (WINAPI * func_GetLogicalProcessorInformation) (SYSTEM_LOGICAL_PROCESSOR_INFORMATION * inf, DWORD * length);

		void MMU::_get_system_info(_sys_info & info)
		{
			info.arch_machine = info.arch_process = info.freq = info.mem = info.sys_machine = info.ver_minor = info.ver_major = 0;
			info.cores_phys = info.cores_virt = 1;
			HMODULE kernel = LoadLibraryW(L"kernel32.dll");
			if (!kernel) return;
			auto GetPhysicallyInstalledSystemMemory = reinterpret_cast<func_GetPhysicallyInstalledSystemMemory>(GetProcAddress(kernel, "GetPhysicallyInstalledSystemMemory"));
			auto GetLogicalProcessorInformation = reinterpret_cast<func_GetLogicalProcessorInformation>(GetProcAddress(kernel, "GetLogicalProcessorInformation"));
			if (GetPhysicallyInstalledSystemMemory) {
				uint64 mem;
				GetPhysicallyInstalledSystemMemory(&mem);
				info.mem = mem * 0x400;
			} else {
				MEMORYSTATUS stat;
				GlobalMemoryStatus(&stat);
				info.mem = stat.dwTotalPhys;
			}
			info.arch_machine = _get_arch(Engine::GetSystemPlatform());
			info.arch_process = _get_arch(ApplicationPlatform);
			info.sys_machine = 0x01;
			SYSTEM_INFO sysinf;
			GetSystemInfo(&sysinf);
			info.cores_phys = info.cores_virt = sysinf.dwNumberOfProcessors;
			if (GetLogicalProcessorInformation) try {
				Array<SYSTEM_LOGICAL_PROCESSOR_INFORMATION> pi(0x40);
				DWORD length = 0;
				auto status = GetLogicalProcessorInformation(0, &length);
				if (!status && GetLastError() == 122) {
					pi.SetLength(length / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
					if (GetLogicalProcessorInformation(pi.GetBuffer(), &length)) {
						info.cores_phys = 0;
						for (auto & i : pi) if (i.Relationship == 0) info.cores_phys++;
					}
				}
			} catch (...) {}
			FreeLibrary(kernel);
			auto sysver = GetVersion();
			info.ver_major = sysver & uint(0xFF);
			info.ver_minor = (sysver & uint(0xFF00)) >> uint(8);
			HKEY hhw, hdesc, hsys, hcp, h0;
			if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE", 0, 0x20019, &hhw) == 0) {
				if (RegOpenKeyExW(hhw, L"DESCRIPTION", 0, 0x20019, &hdesc) == 0) {
					if (RegOpenKeyExW(hdesc, L"System", 0, 0x20019, &hsys) == 0) {
						if (RegOpenKeyExW(hsys, L"CentralProcessor", 0, 0x20019, &hcp) == 0) {
							if (RegOpenKeyExW(hcp, L"0", 0, 0x20019, &h0) == 0) {
								DWORD length = 4, freq_MHz;
								if (RegQueryValueExW(h0, L"~MHz", 0, 0, reinterpret_cast<LPBYTE>(&freq_MHz), &length) == 0) {
									info.freq = uint64(freq_MHz) * uint64(1000000);
								}
								try {
									DynamicString name;
									name.ReserveLength(0x100);
									length = name.ReservedLength() * 2;
									if (RegQueryValueExW(h0, L"ProcessorNameString", 0, 0, reinterpret_cast<LPBYTE>(name.GetBuffer()), &length) == 0) {
										info.proc_name = string(name.GetBuffer(), length / 2, Encoding::UTF16);
									}
								} catch (...) {}
								RegCloseKey(h0);
							}
							RegCloseKey(hcp);
						}
						RegCloseKey(hsys);
					}
					RegCloseKey(hdesc);
				}
				RegCloseKey(hhw);
			}
		}
	}
}
#endif