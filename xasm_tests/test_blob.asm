INTERFACE 8 : INT 12 : OBJECT 1 : INT => 12 : OBJECT

CODE {
	ENTER
		NEW @ 12
		EVAL {
			@BLT:4 4=>4(
				@OFFSET:12 0=>4(L[0], -),
				@ADD:4 4=>4( @OFFSET:12 4=>4(A[1], -), A[0] )
			)
		}
		EVAL {
			@BLT:4 4=>4(
				@OFFSET:12 4=>4(L[0], -),
				@ADD:4 4=>4( @OFFSET:12 0=>4(A[1], -), A[0] )
			)
		}
		EVAL {
			@BLT:4 4=>4(
				@OFFSET:12 8=>4(L[0], -),
				@ADD:4 4=>4( @OFFSET:12 8=>4(A[1], -), @S_RESIZE:1=>4(A[2]) )
			)
		}
		RET {
			@BLT:12 12=>12(
				R,
				L[0]
			)
		}
	LEAVE
}