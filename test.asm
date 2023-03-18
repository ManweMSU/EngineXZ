EXTERNAL "read_bool"
EXTERNAL "read_int"
EXTERNAL "print_integer"
EXTERNAL "int_dtor"

INTERFACE 8 => 8

DATA QWORD 1

CODE {
	ENTER
		JUMPIF { @NOTZERO:8=>8(A[0]) } @+2
		EVAL { @BLT:8 8=>8(A[0], E[1]:=>8()) }
		JUMP @ -3
		JUMPIF { @S_LE:8 8=>8(A[0], D[0]) } @ +1
		RET { @BLT:8 8=>8(R, @S_MUL:8 8=>8(A[0], C[0]:8=>8(@SUB:8 8=>8(A[0], D[0])))) }
		RET { @BLT:8 8=>8(R, D[0]) }
	LEAVE
}