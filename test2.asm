EXTERNAL "S:NUMBER"
EXTERNAL "S:PRINT:CPRINT"

INTERFACE W 1 : ERROR => 0

CODE {
    EVAL {
        E[1]:8=>0(@U_RESIZE:1=>8(E[0]))
    }
    EVAL {
        @BLT:1 1=>1(@PTR_FOLLOW:W 1=>1(A[0]), E[0])
    }
    RET
}