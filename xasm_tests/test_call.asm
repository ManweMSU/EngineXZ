EXTERNAL "complex_call"

DATA {
	DWORD 1
	DWORD 2
	DWORD 3
	DWORD 4
	DWORD 5
	DWORD 6
	DWORD 7
	DWORD 8
}

INTERFACE => 4:INT

; static void _complex_call(int * rv, int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8) noexcept

CODE {
	RET {
		E[0]:W 1:- 4:INT 4:INT 4:INT 4:INT 4:INT 4:INT 4:INT 4:INT=>0(
			@PTR_TAKE:4=>W 1(R),
			D[0], D[4], D[8], D[12], D[16], D[20], D[24], D[28]
		)
	}
}