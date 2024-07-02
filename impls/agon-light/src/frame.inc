;; function arguments start at (IX + 3)
;; frame pointer is at (IX)
;; function locals start at (IX - 3)
macro BEGIN_FRAME localbytes:0
  if localbytes = 0
    CALL begin_frame0
  else if localbytes = 3
    CALL begin_frame3
  else if localbytes = 6
    CALL begin_frame6
  else if localbytes > 126
    PUSH IX
    LD IX, 0
    ADD IX, SP

    LD HL, 0 - localbytes
    ADD HL, SP
    LD SP, HL
  else
    LD A, 0 - localbytes
    CALL begin_frameN
  end if
end macro

macro END_FRAME
  LD SP, IX
  POP IX
end macro

extern begin_frame0
extern begin_frame3
extern begin_frame6
extern begin_frameN
