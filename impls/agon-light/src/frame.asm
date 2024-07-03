assume adl=1

section .text

public begin_frame0, begin_frame3, begin_frame6, begin_frameN

begin_frame0:
  POP HL        ;; DE = back to function
  PUSH IX
  LD IX, 0
  ADD IX, SP
  JP (HL)

begin_frame3:
  POP HL        ;; HL = back to function
  PUSH IX
  LD IX, 0
  ADD IX, SP
  DEC SP
  DEC SP
  DEC SP
  JP (HL)

begin_frame6:
  ; LEA HL, IX + 0    ;; HL = old IX                            3 / 3
  ; EX (SP), HL       ;; HL = back to function, (SP) = old IX   7 / 2

  POP HL        ;; HL = back to function                      4 / 1
  PUSH IX                                                 ;;  5 / 2

  LD IX, 0
  ADD IX, SP

  DEC SP
  DEC SP
  DEC SP
  DEC SP
  DEC SP
  DEC SP
  JP (HL)

;; A = 0 - localbytes
begin_frameN:
  POP DE        ;; DE = back to function
  PUSH IX
  LD IX, 0
  ADD IX, SP

  LD HL, $FFFFFF
  LD L, A
  ADD HL, SP
  LD SP, HL

  EX DE, HL
  JP (HL)

