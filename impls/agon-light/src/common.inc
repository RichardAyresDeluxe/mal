include "frame.inc"
include "struct.inc"

macro RCF
	;; reset carry flag
	AND A
end macro

macro UPPER reg, out:AF
	DEC SP
	PUSH reg
	INC SP
	POP out
end macro

macro RESTORE_STACK n
	if n = 3
		INC SP
		INC SP
		INC SP
	else if n = 6
		INC SP
		INC SP
		INC SP
		INC SP
		INC SP
		INC SP
	else
		EXX
		LD HL, n
		ADD HL, SP
		LD SP, HL
		EXX
	end if
end macro

