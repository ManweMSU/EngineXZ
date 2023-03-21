INTERFACE 1 : INT 8 : FLOAT 8 : FLOAT => 8 : FLOAT

CODE {
	JUMPIF {
		@ZERO:1=>1(A[0])
	} @ +1
	RET {
		@BLT:8 8=>8(R, A[1])
	}
	RET {
		@BLT:8 8=>8(R, A[2])
	}
}