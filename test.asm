EXTERNAL "read_bool"
EXTERNAL "read_int"
EXTERNAL "print_integer"
EXTERNAL "int_dtor"
EXTERNAL "print"

INTERFACE 8 => 8

DATA QWORD 1
DATA QWORD 0
DATA BYTE "Ty pidor.\n"

CODE {
	ENTER
		NEW { @BLT:8 8=>8(I, A[0]) } @ 8 #E[3]
		NEW { @BLT:8 8=>8(I, D[8]) } @ 8 #E[3]
		JUMPIF { @NOTZERO:8=>8(A[0]) } @+2
		EVAL {
			@NEW:0=>8(@FORK:8 0 0=>0(D[0], @BLT:8 8=>8(I, @NEW:0=>8(@BLT:8 8=>8(I,
			@BLT:8 8=>8(A[0],
				@BREAKIF:8 8 +5=>8(
					E[1]:8:ERROR=>8(@PTR_TAKE:8=>W 1(L[1]))#E[3],
					L[1], -
				)
			)
			))#E[3]), NULL))#E[3]
		}
		JUMP @ -3
		JUMPIF { @S_LE:8 8=>8(A[0], D[0]) } @ +1
		RET { @BLT:8 8=>8(R, @S_MUL:8 8=>8(A[0], C[0]:8=>8(@SUB:8 8=>8(A[0], D[0])))) }
		RET { @BLT:8 8=>8(R, D[0]) }
	LEAVE
	EVAL { E[4]:W 1=>0(@PTR_TAKE:1=>W 1(D[16])) }
	RET { @BLT:8 8=>8(R, D[8]) }
}