#include "xe_stdext.h"

#include "xe_logger.h"

namespace Engine
{
	namespace XE
	{
		class MMU : public IAPIExtension
		{
			static void * _mem_alloc(uintptr size) { return malloc(size); }
			static void * _mem_realloc(void * mem, uintptr size) { return realloc(mem, size); }
			static void _mem_release(void * mem) { free(mem); }
		public:
			virtual void * ExposeRoutine(const string & routine_name) noexcept override
			{
				if (routine_name == L"alloca_memoriam") return const_cast<void *>(reinterpret_cast<const void *>(&_mem_alloc));
				else if (routine_name == L"realloca_memoriam") return const_cast<void *>(reinterpret_cast<const void *>(&_mem_realloc));
				else if (routine_name == L"dimitte_memoriam") return const_cast<void *>(reinterpret_cast<const void *>(&_mem_release));
				else return 0;
			}
			virtual void * ExposeInterface(const string & interface) noexcept override { return 0; }
		};
		class SPU : public IAPIExtension
		{
			static void _init_string_with_string(string * self, const string & with, ErrorContext & error)
			{ try { new (self) string(with); } catch (...) { error.error_code = 2; error.error_subcode = 0; } }
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
			static Array<string> * _split(const string & self, uint32 by, ErrorContext & error)
			{
				if (by > 0xFFFF) { error.error_code = 3; error.error_subcode = 0; return 0; }
				try { return new Array<string>(self.Split(widechar(by))); }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return 0; }
			}
			static DataBlock * _encode_block(const string & self, int encoding, bool with_term, ErrorContext & error)
			{
				try { return self.EncodeSequence(static_cast<Encoding>(encoding), with_term); }
				catch (InvalidArgumentException &) { error.error_code = 3; error.error_subcode = 0; return 0; }
				catch (...) { error.error_code = 2; error.error_subcode = 0; return 0; }
			}
		public:
			virtual void * ExposeRoutine(const string & routine_name) noexcept override
			{
				if (routine_name == L"spu_crea") return const_cast<void *>(reinterpret_cast<const void *>(&_init_string_with_string));
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

		IAPIExtension * CreateFPU(void)
		{
			// TODO: IMPLEMENT
			return 0;
		}
		IAPIExtension * CreateMMU(void) { return new MMU; }
		IAPIExtension * CreateSPU(void) { return new SPU; }
	}
}