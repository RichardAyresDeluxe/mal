assume adl=1
include "common.inc"

section .text

public _main, fputs
extern _mos_editline, _fputc, _fflush, _stdout, _strlen

;; char *READ(char *);
;; char *EVAL(char *);
;; char *PRINT(char *);
EVAL_:
READ:
PRINT:
  LD HL, 3
  ADD HL, SP
  LD HL, (HL)
  RET

rep:
  BEGIN_FRAME

  LD HL, (IX + 6)
  PUSH HL
  CALL READ
  
  PUSH HL
  CALL EVAL_

  PUSH HL
  CALL PRINT

  END_FRAME
  RET
  
;; int main(int argc, char **argv)
_main:
  BEGIN_FRAME 80  ;; char *buf = IX - 80

.loop:
  LD HL, _stdout
  PUSH HL
  LD HL, txt_prompt
  PUSH HL
  CALL fputs
  POP HL
  POP HL

  LD HL, 0
  LD L, 00000101b
  PUSH HL
  LD L, 79
  PUSH HL
  LEA HL, IX - 80
  PUSH HL
  CALL _mos_editline
  RESTORE_STACK 9

  CP A, 27
  JR Z, .got_escape

  ; need to add whitespace to terminate the token
  ; int l = strlen(s);
  ; s[l] = ' ';
  ; s[l+1] = '\0';
  ; rv = s;
  LEA IY, IX - 80
  PUSH IY
  CALL _strlen
  POP IY
  EX DE, HL       ;; DE = length of string

  ADD IY, DE
  LD (IY), ' '
  LD (IY + 1), 0

.done:
  LEA HL, IX - 80

  PUSH HL
  CALL rep
  POP DE

  PUSH HL             ;; HL = result of rep
  CALL fputnewline
  POP HL              ;; restore HL = result of rep

  LD DE, _stdout
  PUSH DE
  PUSH HL
  CALL fputs
  POP HL
  POP DE

  CALL fputnewline

  JR .loop

  END_FRAME
  RET

.got_escape:
  LD (IX - 80), 0
  JR .done

fputnewline:
  LD HL, _stdout
  PUSH HL
  LD HL, 0x0a
  PUSH HL
  CALL _fputc
  POP HL
  POP HL
  RET

; int fputs(const char *s, FILE *stream)
fputs:
  BEGIN_FRAME

  LD DE, 0
  LD BC, (IX + 9)

  LD HL, (IX + 6)
  LD A, (IX + 8)
  OR H
  OR L
  JR NZ, .test
  JR .done

.loop:
  PUSH HL
  PUSH BC
  LD E, A
  PUSH DE
  CALL _fputc
  POP DE
  POP BC
  POP HL
  INC HL
  INC D
.test:
  LD A, (HL)
  AND A
  JR NZ, .loop

.done:
  LD A, D
  END_FRAME
  RET
; {
;   int c = 0;
;   while (s && *s != '\0') {
;     fputc(*s++, stream);
;     c++;
;   }
;   return c;
; }

section .rodata

private txt_prompt

txt_prompt: db "user> ", 0
