assume adl=1
include "frame.inc"

section .text

indirect: JP (HL)

;; static void split(
;;    struct linked_list* source,
;;    struct linked_list** front,
;;    struct linked_list** back
;;)
do_split:
  BEGIN_FRAME 6     ;; fast
                    ;; slow

  ;; slow = source
  LD HL, (IX + 6)
  LD (IX - 6), HL

  ;; fast = source->next
  LD DE, (HL)
  LD (IX - 3), DE

  JR .test
.loop:
  ;; DE = fast
  EX DE, HL     ;; HL = fast
  LD DE, (HL)   ;; DE = fast->next
  LD (IX - 3), DE   ;; fast = fast->next
  INC HL
  INC HL
  LD A, (HL)
  OR D
  OR E
  JR Z, .test

  ;; DE = fast
  ;; fast = fast->next
  EX DE, HL
  LD DE, (HL)
  LD (IX - 3), DE

  ;; slow = slow->next
  LD HL, (IX - 6)
  LD BC, (HL)
  LD (IX - 6), BC

.test:
  ;; DE = fast
  LD A, (IX - 1)
  OR D
  OR E
  JR NZ, .loop

  ;; *front = source
  LD BC, (IX + 6)     ;; BC = source
  LD HL, (IX + 9)
  LD (HL), BC

  ;; *back = slow->next
  LD HL, (IX - 6)
  LD DE, (HL)           ;; DE = slow->next
  ;; slow->next = NULL
  LD BC, 0
  LD (HL), BC
  ;; *back = slow->next
  LD HL, (IX + 12)
  LD (HL), DE

  END_FRAME
  RET

;; struct linked_list* merge_raw(
;;    struct linked_list* l1,
;;    struct linked_list* l2,
;;    sort_compare_func compfn,
;;    void *data
;; )
merge_raw:
  BEGIN_FRAME 3   ;; result

  ;; DE = l2
  LD DE, (IX + 9)

  LD A, (IX + 8)
  LD HL, (IX + 6)
  OR H
  OR L
  JR Z, .return_l2

  ;; HL = l1, DE = l2
  LD A, (IX + 11)
  OR D
  OR E
  JR Z, .done

  ;; HL = l1, DE = l2
  LD BC, (IX + 15)    ;; BC = data
  PUSH BC
  PUSH DE
  PUSH HL
  LD HL, (IX + 12)    ;; compfn(l1, l2, data)
  CALL indirect
  ;; A = compare result

  LD HL, 6     ;; restore stack
  ADD HL, SP
  LD SP, HL

  ;; push compfn and data for recursive call
  ; LD BC, (IX + 15)
  ; PUSH BC
  LD BC, (IX + 12)
  PUSH BC

  AND A, A    ;; A = compare result
  JP P, .merge_l2

;; .merge_l1
  LD HL, (IX + 6)   ;; result = l1
  LD (IX - 3), HL   ;; save result

  LD BC, (IX + 9)
  PUSH BC
  LD HL, (HL)
  PUSH HL
  JR .merge_raw
  
.merge_l2:
  LD HL, (IX + 9)   ;; result = l2
  LD (IX - 3), HL   ;; save result

  LD HL, (HL)
  PUSH HL
  LD BC, (IX + 6)
  PUSH BC

.merge_raw:
  ;; result->next = merge_raw(...)
  CALL merge_raw
  LD DE, (IX - 3)  ;; get result back
  EX DE, HL
  LD (HL), DE

.done:
  ;; result in HL
  END_FRAME
  RET

.return_l2:
  ;; DE = l2
  EX DE, HL
  JR .done


;; void linked_list_sort_raw(void** head_ref, sort_compare_func compfn, void *data)
public _linked_list_sort_raw
_linked_list_sort_raw:
  BEGIN_FRAME 9   ;; (IX - 3) = head
                  ;; (IX - 6) = a
                  ;; (IX - 9) = b

  ;; head = *head_ref
  LD HL, (IX + 6)
  LD HL, (HL)
  LD (IX - 3), HL

  ;; if (head == NULL || head->next == NULL)
  ;;    return;
  LD A, (IX - 1)
  OR H
  OR L
  JR Z, .done

  LD DE, (HL)
  INC HL
  INC HL
  LD A, (HL)
  OR D
  OR E
  JR Z, .done


  ;; split(head, &a, &b)
  LEA HL, IX - 9
  PUSH HL
  LEA HL, IX - 6
  PUSH HL
  LD HL, (IX - 3)
  PUSH HL
  CALL do_split

  ;; data and compfn go onto stack for next 3 function calls
  LD HL, (IX + 12)
  PUSH HL
  LD HL, (IX + 9)
  PUSH HL

  ;; linked_list_sort_raw(&a, compfn, data)
  LEA HL, IX - 6
  PUSH HL
  CALL _linked_list_sort_raw
  POP HL

  ;; linked_list_sort_raw(&b, compfn, data)
  LEA HL, IX - 9
  PUSH HL
  CALL _linked_list_sort_raw
  POP HL

  LD HL, (IX - 9)
  PUSH HL
  LD HL, (IX - 6)
  PUSH HL
  CALL merge_raw

  LD DE, (IX + 6)
  EX DE, HL
  LD (HL), DE

.done:
  END_FRAME
  RET

