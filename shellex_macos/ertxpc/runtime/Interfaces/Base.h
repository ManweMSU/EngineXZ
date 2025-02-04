#pragma once

#include "../Miscellaneous/Encoding.h"

#include <stdlib.h>
#include <math.h>

namespace Engine
{
	typedef unsigned int uint;
	typedef signed int int32;
	typedef unsigned int uint32;
	typedef signed long long int64;
	typedef unsigned long long uint64;
	typedef signed short int16;
	typedef unsigned short uint16;
	typedef signed char int8;
	typedef unsigned char uint8;

	typedef wchar_t widechar;

	enum class Platform { Unknown, X86, X64, ARM, ARM64 };

#define ENGINE_PI 3.14159265358979323846

#define ENGINE_PACKED_STRUCTURE(NAME) struct NAME {
#define ENGINE_END_PACKED_STRUCTURE } __attribute__((packed));

	constexpr Encoding SystemEncoding = Encoding::UTF32;
	constexpr const widechar * OperatingSystemName = L"Mac OS";

#ifdef ENGINE_ARM
#ifdef ENGINE_X64
	constexpr Platform ApplicationPlatform = Platform::ARM64;
#else
	constexpr Platform ApplicationPlatform = Platform::ARM;
#endif
#else
#ifdef ENGINE_X64
	constexpr Platform ApplicationPlatform = Platform::X64;
#else
	constexpr Platform ApplicationPlatform = Platform::X86;
#endif
#endif
#ifdef ENGINE_X64
	typedef uint64 intptr;
	typedef uint64 uintptr;
	typedef int64 sintptr;
#else
	typedef uint32 intptr;
	typedef uint32 uintptr;
	typedef int32 sintptr;
#endif

	typedef intptr eint;
	typedef void * handle;

	// Atomic increment and decrement; memory initialization
	uint InterlockedIncrement(uint & Value);
	uint InterlockedDecrement(uint & Value);
	void ZeroMemory(void * Memory, intptr Size);

	// Some C standard library and language dependent case insensitive comparation
	void * MemoryCopy(void * Dest, const void * Source, intptr Length);
	widechar * StringCopy(widechar * Dest, const widechar * Source);
	int StringCompare(const widechar * A, const widechar * B);
	int SequenceCompare(const widechar * A, const widechar * B, int Length);
	int MemoryCompare(const void * A, const void * B, intptr Length);
	int StringLength(const widechar * str);
	void StringAppend(widechar * str, widechar letter);
}