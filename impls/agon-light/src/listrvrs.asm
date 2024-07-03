assume adl=1

include "frame.inc"

section .text

;; void linked_list_reverse(void **head_ref)
public _linked_list_reverse
_linked_list_reverse:
  BEGIN_FRAME 6   ;; (IX - 3) = head
                  ;; (IX - 6) = reversed

  ;; reversed = NULL
  LD BC, 0
  LD (IX - 6), BC

  ;; head = *head_ref
  LD HL, (IX + 6)
  LD DE, (HL)         ;; DE = head
  LD (IX - 3), DE

  JR .test

.loop:
                  ;; DE = head
  EX DE, HL       ;; HL = head
  LD BC, (IX - 6) ;; BC = reversed
  LD DE, (HL)     ;; DE = next
  LD (HL), BC     ;; head->next = reversed
  LD (IX - 6), HL ;; reversed = head
  LD (IX - 3), DE ;; head = next

.test:
  ;; DE = (IX - 3) = head
  LD A, (IX - 1)
  OR D
  OR E
  JR NZ, .loop

  LD BC, (IX - 6) ;; BC = reversed
  LD HL, (IX + 6) ;; void **head_ref
  LD (HL), BC     ;; *head_ref = reversed

  END_FRAME
  RET
