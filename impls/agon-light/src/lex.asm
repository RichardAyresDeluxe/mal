assume adl=1
include "common.inc"

LEXER_STACK_SIZE    EQU 32
LEXER_BUFFER_SIZE   EQU 120
TOKEN_INIT_SIZE     EQU 40

struct lexer
    .column         db 0
    .line           dl 0
    .state          dl 0
    .token_size     dl 0
    .build_token    dl 0
    .tokens         dl 0
    .token_handler  dl 0
    .stack          rb LEXER_STACK_SIZE * 3
    .stack_offset   db 0
    .get_input      dl 0
    .get_input_data dl 0
    .bufptr         dl 0
    .buffer         rb LEXER_BUFFER_SIZE
ends

struct token
    .next           dl 0
    .value          dl 0
    .type           db 0
    .column         db 0
    .line           dl 0
    ._garbage       rb 3
ends


section .text

;; CALL (HL)
;; put the target address in HL, call this label
indirect:
    JP (HL)

;; static uint8_t lex_get_input(lexer_t lexer)
lex_get_input:
    BEGIN_FRAME 6   ;; unsigned keep  (IX - 3)
                    ;; char *rv       (IX - 6)

    LD BC, 0
    LD (IX - 3), BC

    LD HL, (IX + 6)
    PUSH HL
    CALL buffer_remaining
    AND A
    JR Z, .skip_move

    LD IY, (IX + 6)
    LD HL, (IY + lexer.bufptr)
    PUSH HL
    CALL _strlen
    LD (IX - 3), HL
    LD A, (IX - 1)
    OR H
    OR L
    JR Z, .skip_move

    PUSH HL
    LD IY, (IX + 6)
    LD HL, (IY + lexer.bufptr)
    PUSH HL
    LEA HL, IY + lexer.buffer
    PUSH HL
    CALL _memmove

.skip_move:
    ;; char *rv = lexer->get_input(lexer, &lexer->buffer[keep], BUFFER_SIZE - keep, lexer->get_input_data)
    LD IY, (IX + 6)
    LD HL, (IY + lexer.get_input_data)
    PUSH HL
    LD HL, LEXER_BUFFER_SIZE
    LD BC, (IX - 3)
    RCF
    SBC HL, BC
    PUSH HL
    LEA HL, IY + lexer.buffer
    ADD HL, BC
    PUSH HL
    PUSH IY
    LD HL, (IY + lexer.get_input)
    CALL indirect
    ;; HL = char *rv
    LD (IX - 6), HL
    LD A, (IX - 4)
    OR H
    OR L
    JR Z, .return   ;; A = 0

    LD IY, (IX + 6)
    ;; lexer->bufptr = &lexer->buffer[0]
    LEA HL, IY + lexer.buffer
    LD (IY + lexer.bufptr), HL

    LD BC, (IX - 3)
    ADD HL, BC
    PUSH HL
    CALL _strlen
    LD A, L

.return:
    END_FRAME
    RET

;;extern void initialise(
;;    struct lexer *lexer,
;;    lexer_fn state,
;;    lexer_token_handler token_handler,
;;    lexer_get_input_t get_input,
;;    void *get_input_data
;;)
lex_initialise:
    BEGIN_FRAME

    ;; memset(lexer, 0, sizeof(struct lexer))
    LD BC, lexer.size
    PUSH BC
    LD C, 0
    PUSH BC
    LD BC, (IX + 6)
    PUSH BC
    CALL _memset

    LD IY, (IX + 6)
    INC (IY + lexer.column)
    INC (IY + lexer.line)

    LD HL, (IX + 9)
    LD (IY + lexer.state), HL

    ; LD (IY + lexer.token_size), TOKEN_INIT_SIZE
    LD HL, TOKEN_INIT_SIZE
    LD (IY + lexer.token_size), HL
    PUSH HL
    CALL _heap_malloc

    LD IY, (IX + 6)
    LD (IY + lexer.build_token), HL
    LD (HL), 0

    LD (IY + lexer.stack_offset), LEXER_STACK_SIZE

    LD HL, (IX + 12)    ;; token_handler
    LD (IY + lexer.token_handler), HL

    LD HL, (IX + 15)    ;; get_input callback
    LD (IY + lexer.get_input), HL

    LD HL, (IX + 18)    ;; get_input data
    LD (IY + lexer.get_input_data), HL

    LEA HL, IY + lexer.buffer
    LD BC, LEXER_BUFFER_SIZE
    ADD HL, BC
    LD (IY + lexer.bufptr), HL

    END_FRAME
    RET

;; const char *lex_peekn(lexer_t lexer, uint8_t n)
public _lex_peekn
_lex_peekn:
    BEGIN_FRAME

    LD HL, (IX + 6)
    PUSH HL
    CALL buffer_remaining

    CP A, (IX + 9)
    JP M, .get_input

    LD IY, (IX + 6)
    LD HL, (IY + lexer.bufptr)
    PUSH HL
    CALL _strlen
    LD BC, 0
    LD C, (IX + 9)
    RCF
    SBC HL, BC
    JP P, .done

.get_input:
    LD HL, (IX + 6)
    PUSH HL
    CALL lex_get_input

    CP A, (IX + 9)
    JP M, .err

.done:
    LD IY, (IX + 6)
    LD HL, (IY + lexer.bufptr)
    END_FRAME
    RET

.err:
    LD HL, msg_unexpected_end
    JP err_fatal

;; lexer_t get_char(lexer_t /* in/out */lexer, char *output)
get_char:
    BEGIN_FRAME

    ;; *output = lex_peek(lexer)
    LD C, 1
    PUSH BC
    LD HL, (IX + 6)     ;; IY = lexer
    PUSH HL
    CALL _lex_peekn
    LD A, (HL)

    LD HL, (IX + 9)     ;; HL = char *output
    LD (HL), A

    ;; if (*output == '\0') return lexer
    AND A
    JR Z, .done

    LD IY, (IX + 6)     ;; IY = lexer

    CP 0x0A 
    JR NZ, .not_newline

    ;; *output == '\n'
    LEA HL, IY + lexer.line
    CALL incr
    LD (IY + lexer.column), 0               ;; lexer->column = 0, will be
                                            ;; incremented in a moment
.not_newline:
    LEA HL, IY + lexer.bufptr
    CALL incr
    INC (IY + lexer.column)

.done:
    LD HL, (IX + 6)
    END_FRAME
    RET

;; lexer_t lex_token_add_char(lexer_t lexer, char ch)
public _lex_token_add_char
_lex_token_add_char:
    BEGIN_FRAME 3   ;; unsigned l

    LD IY, (IX + 6)     ;; IY = lexer

    LD HL, (IY + lexer.build_token)
    PUSH HL
    CALL _strlen
    ;; HL = build token length
    LD (IX - 3), HL     ;; save l

    ;; if (l + 1 - lexer->token_size < 0) then .skip_realloc
    LD IY, (IX + 6)
    INC HL
    LD DE, (IY + lexer.token_size)
    RCF
    SBC HL, DE
    JP M, .skip_realloc

    ;; realloc build_token
    EX DE, HL
    ADD HL, HL      ;; token_size *= 2
    LD (IY + lexer.token_size), HL
    PUSH HL
    LD HL, (IY + lexer.build_token)
    PUSH HL
    CALL _heap_realloc
    LD IY, (IX + 6)
    LD (IY + lexer.build_token), HL

.skip_realloc:
    ;; lexer->build_token[l] = ch
    LD HL, (IY + lexer.build_token)
    LD BC, (IX - 3)
    ADD HL, BC
    LD A, (IX + 9)
    LD (HL), A

    ;; lexer->build_token[l + 1] = '\0'
    INC HL
    XOR A, A
    LD (HL), A

    LD HL, (IX + 6)
    END_FRAME
    RET

;; uint8_t buffer_remaining(lexer_t lexer)
buffer_remaining:
    LD HL, 3
    ADD HL, SP
    LD IY, (HL)     ;; IY = lexer

    LEA HL, IY + lexer.buffer
    LD DE, LEXER_BUFFER_SIZE
    ADD HL, DE
    LD BC, (IY + lexer.bufptr)
    RCF
    SBC HL, BC

    LD A, L
    RET

;; int lex_is_eos(lexer_t lexer)
public _lex_is_eos
_lex_is_eos:
    BEGIN_FRAME

    LD IY, (IX + 6)     ;; IY = lexer

    ;; if (lexer->bufptr[0] == 0)
    LD HL, (IY + lexer.bufptr)
    LD A, (HL)
    AND A, A
    JR Z, .test_get_input

    ;; if (buffer_remaining(lexer) == 0)
    PUSH IY
    CALL buffer_remaining
    POP IY
    AND A, A
    JR NZ, .not_eos

    ;; IY = lexer
.test_get_input:
    ;; if (lex_get_input(lexer) == 0)
    PUSH IY
    CALL lex_get_input
    AND A, A
    JR Z, .is_eos

.not_eos:
    XOR A, A
.done:
    END_FRAME
    RET
.is_eos:
    LD A, 1
    JR .done

public _lex_read
public _lex_readn
_lex_read:
    BEGIN_FRAME 3       ;; 1 char = 3 bytes
    LD B, 1
    JR lex_readn_1

_lex_readn:
    BEGIN_FRAME 3
    LD B, (IX + 9)      ;; A = n
lex_readn_1:

    LD HL, (IX + 6)
.loop:
    PUSH BC         ;; save counter

    ;; lexer = get_char(lexer, &ch)
    LEA DE, IX - 3
    PUSH DE
    PUSH HL
    CALL get_char
    ;; HL = new lexer
    POP BC
    POP BC

    LD C, (IX - 3)     ;; C = char
    PUSH BC
    PUSH HL             ;; HL = lexer
    CALL _lex_token_add_char
    ;; HL = new lexer
    POP BC
    POP BC

    POP BC          ;; restore counter
    DJNZ .loop
    ;; HL = lexer

    END_FRAME
    RET

;;lexer_t lex_skip(lexer_t lexer)
public _lex_skip
public _lex_skipn
_lex_skip:
    BEGIN_FRAME 3   ;; allocate char on stack (3 bytes)
    PEA IX - 3  ;; Push = &ch
    LD HL, (IX + 6) ;; HL = lexer
    PUSH HL         ;; push lexer
    CALL get_char
    POP HL
    END_FRAME
    RET

    ; LD B, 1
    ; JR lex_skipn_1
;;lexer_t lex_skipn(lexer_t lexer, uint8_t n)
_lex_skipn:
    BEGIN_FRAME 3   ;; char
    LD B, (IX + 9)  ;; B = counter
    LEA DE, IX - 3  ;; DE = &ch
    LD HL, (IX + 6) ;; HL = lexer
.loop:
    PUSH BC         ;; save counter
    PUSH DE         ;; push &ch
    PUSH HL         ;; push lexer
    CALL get_char
    ;; HL = new lexer
    POP HL
    POP DE          ;; restore DE = &ch
    POP BC          ;; restore counter
    DJNZ .loop      ;; decrement counter and jump if NZ
    ;; return lexer
    END_FRAME
    RET

;; uint8_t lex_coming_up(lexer_t lexer, const char *s, uint8_t c)
public _lex_coming_up
_lex_coming_up:
    BEGIN_FRAME 3   ;; (IX - 3) = c

    LD HL, (IX + 12) ;; HL = len
    LD A, (IX + 14)
    OR H
    OR L
    JR NZ, .skip_strlen

    LD HL, (IX + 9) ;; HL = s
    PUSH HL
    CALL _strlen

.skip_strlen:
    ;; HL = length
    LD (IX - 3), HL
    PUSH HL
    LD DE, (IX + 6) ;; DE = lexer
    PUSH DE
    CALL _lex_peekn
    ;; HL = peek

    ;; strncmp(peek, s, c) == 0
    LD BC, (IX - 3)
    PUSH BC
    LD DE, (IX + 9)
    PUSH DE
    PUSH HL
    CALL _strncmp
    ;; HL = strncmp(peek, s, c)

    LD A, L
    OR H
    JR NZ, .return_zero
    ;; A = 0
    INC A

.done:
    END_FRAME
    RET

.return_zero:
    XOR A, A
    JR .done

public _lex_free_tokens
_lex_free_tokens:
    BEGIN_FRAME 3  ;; lex_token_t *next;

    LD HL, (IX + 6)     ;; HL = token
    LD (IX - 3), HL         ;; (IX - 3) = next
    JR .next

.loop:
    ;; HL = token
    LD BC, (HL)
    LD (IX - 3), BC       ;; save token->next

    ;; heap_free(token->value)
    PUSH HL     ;; push token - will be parameter to second _heap_free

    INC HL
    INC HL
    INC HL
    LD HL, (HL)     ;; (token + 3) = token->value

    PUSH HL
    CALL _heap_free
    POP HL

    CALL _heap_free
    POP HL

.next:
    LD A, (IX - 1)
    LD HL, (IX - 3)
    OR H
    OR L
    JR NZ, .loop

    END_FRAME
    RET

lex_do_process:
    BEGIN_FRAME 3

    LD HL, (IX + 6)     ;; HL = lexer
    JR .test

.loop:
    LD IY, (IX - 3)         ;; IY = lexer
    PUSH IY
    LD HL, (IY + lexer.state)   ;; HL = lexerfn
    CALL indirect
    ;; HL = new lexer
    POP BC

.test:
    LD (IX - 3), HL
    LD A, (IX - 1)
    OR H
    OR L
    JR NZ, .loop

    END_FRAME
    RET

public _lex_get_tokens
_lex_get_tokens:
    LD HL, 3
    ADD HL, SP
    LD IY, (HL)     ;; IY = lexer
    LD HL, (IY + lexer.tokens)
    RET

public _lex_null
_lex_null:
    LD HL, 3
    ADD HL, SP
    LD IY, (HL)     ;; IY = lexer
    LD HL, .null_state
    LD (IY + lexer.state), HL
    LEA HL, IY
    RET
.null_state:
    LD HL, 0
    RET

;; lexer_t lex_emit_token(lexer_t lexer, token_type_t type, const char *s)
public _lex_emit_token
_lex_emit_token:
    BEGIN_FRAME 3       ;; lex_token_t *token

    LD BC, token.size
    PUSH BC
    CALL _heap_malloc

    LD IY, 0
    EX DE, HL
    ADD IY, DE          ;; IY = token

    LD (IX - 3), IY     ;; save IY = token

    ;; token->value = heap_malloc(strlen(s) + 1)
    LD HL, (IX + 12)
    PUSH HL
    CALL _strlen        ;; HL = len

    INC HL
    PUSH HL
    CALL _heap_malloc

    LD IY, (IX - 3)     ;; restore IY = token
    LD (IY + token.value), HL

    ;; strcpy(token->value, s)
    LD BC, (IX + 12)
    PUSH BC
    ; HL is already token->value
    PUSH HL
    CALL _strcpy

    LD IY, (IX - 3)     ;; restore IY = token

    ;; token->type = type
    LD C, (IX + 9)
    LD (IY + token.type), C

    ;; token->next = 0
    LD BC, 0
    LD (IY + token.next), BC

    ;; get line and column out of lexer
    LD IY, (IX + 6)     ;; IY = lexer
    LD D, (IY + lexer.column)
    LD BC, (IY + lexer.line)
    LD HL, (IY + lexer.token_handler)

    LD IY, (IX - 3)     ;; IY = token
    ;; token->pos = lexer->pos
    LD (IY + token.column), D
    ;; token->line = lexer->line
    LD (IY + token.line), BC

    PUSH IY
    LD DE, (IX + 6)         ;; DE = lexer
    PUSH DE
    CALL indirect

    END_FRAME
    RET

;; lexer_t lex_emit(lexer_t lexer, token_type_t type)
public _lex_emit
_lex_emit:
    BEGIN_FRAME

    LD IY, (IX + 6)     ;; IY = lexer

    LD HL, (IY + lexer.build_token)
    PUSH HL
    LD BC, (IX + 9)     ;; token type
    PUSH BC
    PUSH IY
    CALL _lex_emit_token
    ;; HL = new lexer

    ;; jump into lex_clear()
    EX DE, HL
    JR _lex_clear.skip

;; lexer_t lex_clear(lexer_t lexer)
public _lex_clear
_lex_clear:
    BEGIN_FRAME

    LD DE, (IX + 6)

.skip:
    ;; expects lexer in DE
    LD IY, 0
    ADD IY, DE

    LD HL, (IY + lexer.token_size)
    LD BC, TOKEN_INIT_SIZE
    RCF                                 ;; clear carry flag
    SBC HL, BC
    JR Z, .continue

    ;; must free and re-allocate build token
    PUSH IY                             ;; save lexer
    LD HL, (IY + lexer.build_token)
    PUSH HL
    CALL _heap_free
    POP HL

    LD BC, TOKEN_INIT_SIZE
    PUSH BC
    CALL _heap_malloc
    POP BC                              ;; restore BC = TOKEN_INIT_SIZE
    POP IY                              ;; restore lexer

    LD (IY + lexer.build_token), HL
    LD (IY + lexer.token_size), BC

.continue:
    LD HL, (IY + lexer.build_token)
    LD (HL), 0

    LD HL, (IX + 6)
    END_FRAME
    RET

public _lex_assoc_state
_lex_assoc_state:
    BEGIN_FRAME

    LD IY, (IX + 6)     ;; IY = lexer

    LD DE, (IX + 9)     ;; DE = state
    LD A, (IX + 11)
    OR D
    OR E
    JR NZ, .got_new_state

    LD A, (IY + lexer.stack_offset)
    CP LEXER_STACK_SIZE
    JR Z, .stack_underflow
    LD C, A
    LD B, 3
    MLT BC              ;; BC = A * 3

    LEA HL, IY + lexer.stack
    ADD HL, BC
    LD DE, (HL)

    INC (IY + lexer.stack_offset)

.got_new_state:
    ;; DE = new state
    LD (IY + lexer.state), DE
    
    LD HL, (IX + 6)

    END_FRAME
    RET

.stack_underflow:
    LD HL, msg_stack_underflow
    JP err_fatal

public _lex_push_return
_lex_push_return:
    BEGIN_FRAME

    LD IY, (IX + 6)             ;; IY = lexer

    ;; if (lexer->tsack_ptr == 0)
    ;;      overflow
    DEC (IY + lexer.stack_offset)
    JP Z, .stack_overflow

    ;; if (state == NULL)
    LD DE, (IX + 9)             ;; DE = new state
    LD A, (IX + 11)
    OR D
    OR E
    JR NZ, .got_new_state

    ;; use current state
    LD DE, (IY + lexer.state)
.got_new_state:

    LD C, (IY + lexer.stack_offset) ;; BC = new stack offset
    LD B, 3
    MLT BC                  ;; BC = stack offset * 3

    LEA HL, IY + lexer.stack    ;; HL = lexer's stack
    ADD HL, BC                  ;; HL = current point in stack
    LD (HL), DE                 ;; store saved state

    LD HL, (IX + 6)     ;; return lexer
    END_FRAME
    RET

.stack_overflow:
    LD HL, msg_stack_overflow
    JP err_fatal

default_token_handler:
    BEGIN_FRAME

    LD BC, lexer.tokens
    LD HL, (IX + 6)
    ADD HL, BC
    ;; HL = &lexer->tokens

    LD IY, (IX + 9)             ;; IY = token
    LD BC, (HL)                 ;; BC = lexer->tokens
    LD (IY + token.next), BC    ;; token->next = lexer->tokens
    LD (HL), IY                 ;; lexer->tokens = token

    LD HL, (IX + 6)
    END_FRAME
    RET

; lex_token_t *lex_process(
;     lexer_fn initial_state,
;     lexer_token_handler token_handler,
;     lexer_get_input_t get_input,
;     void *get_input_data
; )
public _lex_process
_lex_process:
    BEGIN_FRAME 6       ;; lexer_t lexer
                        ;; lex_token_t *tokens

    ;; _lexer = heap_malloc()
    LD HL, lexer.size
    PUSH HL
    CALL _heap_malloc
    LD (IX - 3), HL     ;; (IX - 3) = lexer

    LD HL, (IX + 9)
    LD A, (IX + 11)
    OR H
    OR L
    JR NZ, .got_token_handler
    LD HL, default_token_handler

.got_token_handler:
    ;; HL = token_handler

    LD DE, (IX + 15)
    PUSH DE
    LD DE, (IX + 12)
    PUSH DE
    PUSH HL
    LD HL, (IX + 6)
    PUSH HL
    LD HL, (IX - 3)
    PUSH HL
    CALL lex_initialise

    LD HL, (IX - 3)
    PUSH HL
    CALL lex_do_process

    LD IY, (IX - 3)
    LD HL, (IY + lexer.tokens)

    LD (IX - 6), HL                 ;; save tokens

    LD HL, (IY + lexer.build_token)
    PUSH HL
    CALL _heap_free

    LD HL, (IX - 3)
    PUSH HL
    CALL _heap_free

    LD HL, (IX - 6)
    END_FRAME
    RET

;; lexer_t lex_error(lexer_t lexer, const char *msg)
public _lex_error
_lex_error:
    BEGIN_FRAME

    LD IY, (IX + 6)
    LD BC, 0
    LD C, (IY + lexer.column)
    PUSH BC
    LD BC, (IY + lexer.line)
    PUSH BC
    LD HL, (IX + 9)
    PUSH HL
    LD HL, msg_error
    PUSH HL
    LD HL, ERR_LEXER
    PUSH HL
    CALL _err_warning

    LD IY, (IX + 6)
    LD HL, (IY + lexer.tokens)
    PUSH HL
    CALL _lex_free_tokens
    LD IY, (IX + 6)
    LD HL, 0
    LD (IY + lexer.tokens), HL

    PUSH IY
    CALL _lex_null

    END_FRAME
    RET
    

;; expects pointer to message in HL
err_fatal:
    PUSH HL
    LD HL, ERR_LEXER
    PUSH HL
    CALL _err_fatal

extern _strncmp
extern _strlen
extern _strcpy
extern _memset
extern _memmove
extern _heap_free
extern _heap_malloc
extern _heap_realloc
extern _err_fatal
extern _err_warning
extern incr

section .rodata

private msg_unexpected_end
private msg_stack_overflow
private msg_stack_underflow

msg_stack_overflow: db "Stack overflow", 00h
msg_stack_underflow: db "Stack underflow", 00h
msg_unexpected_end: db "Unexpected end of input", 00h
msg_error: db "%s at line %d, column %d", 00h

; END
; vim: set ts=4
