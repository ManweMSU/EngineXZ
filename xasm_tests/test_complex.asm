EXTERNAL "fp_add_mul"

DATA DWORD 10

INTERFACE 4 : INT 8 : FLOAT 8 : FLOAT 4 : INT 4 : INT 4 : INT 4 : INT 4 : INT 4 : INT 8 : FLOAT W 1 : - => 8 : FLOAT

;typedef double (* TestComplexFunc) (int ia, double fa, double fb, int ib, int ic, int id, int ie, int if, int ig, double fc, int * rv);

CODE {
	EVAL {
		@BLT:4 4=>4 (
			@PTR_FOLLOW:W 1=>4(A[10]),
			@ADD:4 4=>4(
				A[8], @S_MUL:4 4=>4(
					D[0],
					@ADD:4 4=>4(
						A[7], @S_MUL:4 4=>4(
							D[0],
							@ADD:4 4=>4(
								A[6], @S_MUL:4 4=>4(
									D[0],
									@ADD:4 4=>4(
										A[5], @S_MUL:4 4=>4(
											D[0],
											@ADD:4 4=>4(
												A[4], @S_MUL:4 4=>4(
													D[0],
													@ADD:4 4=>4(
														A[3], @S_MUL:4 4=>4(
															D[0],
															A[0]
														)
													)
												)
											)
										)
									)
								)
							)
						)
					)
				)
			)
		)
	}
	RET {
		@BLT:8 8=>8 (
			R,
			E[0]:8:FLOAT 8:FLOAT 8:FLOAT=>8:FLOAT(A[1], A[2], A[9])
		)
	}
}