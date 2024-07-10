#ifndef _LEX_H
#define _LEX_H

#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

#define LEX_STATE_RETURN    NULL
#define LEX_STATE_CURRENT   NULL

typedef uint8_t token_type_t;

struct lex_token {
    struct lex_token *next;
    char *value;
    token_type_t type;
    uint8_t pos;
    unsigned line;
    /* FIXME: */
    uint8_t _garbage[3];    // need this otherwise breaks, why?
};

typedef struct lex_token lex_token_t;
typedef struct lexer *lexer_t;

typedef lexer_t (*lexer_fn)(lexer_t);
typedef lexer_t (*lexer_token_handler)(lexer_t, lex_token_t*);
typedef char *(*lexer_get_input_t)(lexer_t, char *buf, int sz, void *data);

/** Peek a string in the input.
 * This just returns a pointer to the input - the result is NOT NULL-terminated
 */
const char *lex_peekn(lexer_t, uint8_t n);

/** Peek the next character in the input */
static inline char lex_peek(lexer_t lexer) {
    return *lex_peekn(lexer, 1);
}

/** Get the next character and add to the end of the current token. */
lexer_t lex_read(lexer_t lexer);
/** Get the next n characters and add to the end of the current token. */
lexer_t lex_readn(lexer_t lexer, uint8_t n);
/** Skip the next character */
lexer_t lex_skip(lexer_t lexer);
/** Skip the next n characters */
lexer_t lex_skipn(lexer_t lexer, uint8_t n);

/** Add a character to the token */
lexer_t lex_token_add_char(lexer_t, char);

/**
 * Is the provided string of len `c` coming next?
 * If `c` is 0, then treat `s` as NULL-terminated string
 */
uint8_t lex_coming_up(lexer_t lexer, const char *s, unsigned c);

/** Is the next character a whitespace? */
static inline int lex_is_whitespace(lexer_t lexer) {
    return isspace(lex_peek(lexer));
}

/** Is the next character a letter? */
static inline int lex_is_letter(lexer_t lexer) {
    return isalpha(lex_peek(lexer));
}

/** Is the next character a digit? */
static inline int lex_is_digit(lexer_t lexer) {
    return isdigit(lex_peek(lexer));
}

/** Have we reached the end of input? */
uint8_t lex_is_eos(lexer_t lexer);

/** An error has been detected. Output it to stderr and finish processing */
lexer_t lex_error(lexer_t lexer, const char *msg);

/** Emit the current token. Calls the provided token handler if present, 
 * otherwise prepends the token to the linked list of tokens. */
lexer_t lex_emit(lexer_t lexer, token_type_t type);
/** Emit the provided token */
lexer_t lex_emit_token(lexer_t lexer, token_type_t type, const char *tokenstring);

/** Clear the current token */
lexer_t lex_clear(lexer_t lexer);

/** Set the current state. If state == LEX_STATE_RETURN, then pops
 * a state from the stack and sets the current state to that. */
lexer_t lex_assoc_state(lexer_t lexer, lexer_fn state);

/** Push a state onto the stack. If state == LEX_STATE_CURRENT, then current
 * state is pushed onto the stack. */
lexer_t lex_push_return(lexer_t lexer, lexer_fn state);

/** "call" the state, i.e. push the current state on the stack and jump
 * to the provided state */
static inline lexer_t lex_state_call(lexer_t lexer, lexer_fn state) {
    lexer = lex_push_return(lexer, LEX_STATE_CURRENT);
    return lex_assoc_state(lexer, state);
}

static inline lexer_t lex_return(lexer_t lexer) {
    return lex_assoc_state(lexer, LEX_STATE_RETURN);
}

/** Start lexing a string.
 * - initial_state is the initial state function
 * - token_handler is called each time a token is emitted. If this is null,
 *   the default token_handler will be used. This creates a linked list of
 *   tokens that will be returned from `lex_process`.
 * - input is a NULL-terminated input string
 */
lex_token_t *lex_process(
        lexer_fn initial_state,
        lexer_token_handler token_handler,
        lexer_get_input_t get_input,
        void *get_input_data
);

/** End lexing */
lexer_t lex_null(lexer_t lexer);

/** Get the current list of tokens */
lex_token_t *lex_get_tokens(lexer_t);

/** Free a token list */
void lex_free_tokens(lex_token_t *);


void lex_destroy(lexer_t lexer);

#endif /* _LEX_H */
