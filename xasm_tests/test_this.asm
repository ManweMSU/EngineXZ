EXTERNAL "test"

INTERFACE W 1 : THIS 12 : OBJECT => 12 : OBJECT

CODE {
	EVAL {
		@BLT: 12 12 => 12(
			R,
			E[0]: W 1 : THIS 12 : OBJECT => 12 : OBJECT (A[0], A[1])
		)
	}
	RET
}