#include "../Interfaces/Base.h"

#include <string.h>

namespace Engine
{
    uint InterlockedIncrement(uint & Value) { return __sync_add_and_fetch(&Value, 1); }
	uint InterlockedDecrement(uint & Value) { return __sync_sub_and_fetch(&Value, 1); }
    void ZeroMemory(void * Memory, intptr Size) { memset(Memory, 0, Size); }
	void * MemoryCopy(void * Dest, const void * Source, intptr Length) { return memcpy(Dest, Source, Length); }
	widechar * StringCopy(widechar * Dest, const widechar * Source)
	{
		int i = -1;
		do { i++; Dest[i] = Source[i]; } while (Source[i]);
		return Dest;
	}
	int StringCompare(const widechar * A, const widechar * B)
	{
		int i = 0;
		while (A[i] == B[i] && A[i]) i++;
		if (A[i] == 0 && B[i] == 0) return 0;
		if (A[i] < B[i]) return -1;
		return 1;
	}
	int SequenceCompare(const widechar * A, const widechar * B, int Length)
	{
		int i = 0;
		while (A[i] == B[i] && i < Length) i++;
		if (i == Length) return 0;
		if (A[i] < B[i]) return -1;
		return 1;
	}
	int MemoryCompare(const void * A, const void * B, intptr Length) { return memcmp(A, B, Length); }
	int StringLength(const widechar * str) { int l = 0; while (str[l]) l++; return l; }
	void StringAppend(widechar * str, widechar letter) { auto len = StringLength(str); str[len + 1] = 0; str[len] = letter; }
}
