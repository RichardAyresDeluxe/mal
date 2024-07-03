assume adl=1
include "common.inc"

section .text

public increment_short, increment_long, increment_int, incr

;; increment functions increase the value in (HL) by 1, B is clobbered
increment_short:
    LD B, 2
    JR incr.loop
increment_long:
    LD B, 4
    JR incr.loop
increment_int:
incr:
    LD B, 3
.loop:
    INC (HL)
    RET NZ
    INC HL
    DJNZ .loop
    RET


