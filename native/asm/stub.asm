# ACERVUS 0x100

LEA DX, [@@DATA]
MOV DS, DX
LEA DX, [@DATA]
MOV AH, 0x09
INT 0x21
MOV AX, 0x4C00
INT 0x21

# SEGMENTUM DATA: # DATA 1 'Programma executa non est in DOS circumiecto.\r\n$'