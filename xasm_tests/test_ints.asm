INTERFACE 4 : INT 4 : INT 4 : INT => 4 : INT

CODE {
	RET {
		@BLT:4 4=>4 (
			R,
			@ADD:4 4=>4(
				A[0],
				@S_MUL:4 4=>4(A[1], A[2])
			)
		)
	}
}