#if !defined(EZ80)
#include "lex.h"
#include "heap.h"
#include "err.h"

#include <stdlib.h>
#include <string.h>

#define TOKEN_INIT_SIZE 40
#define STACK_SIZE  32      /* must be less than 256/3 */
#define BUFFER_SIZE 120     /* must be less than 256 */

#include <stdint.h>

struct lexer {
    uint8_t pos;
    unsigned line;
    lexer_fn state;
    unsigned token_size;
    char *build_token;
    lex_token_t *tokens;
    lexer_token_handler token_handler;
    lexer_fn stack[STACK_SIZE];
    uint8_t stack_ptr;
    lexer_get_input_t get_input;
    void *get_input_data;
    const char *bufptr;
    char buffer[BUFFER_SIZE];
};

static void lex_initialise(
    struct lexer *lexer,
    lexer_fn state,
    lexer_token_handler token_handler,
    lexer_get_input_t get_input,
    void *get_input_data
) {
    memset(lexer, 0, sizeof(struct lexer));

    lexer->pos = 1;
    lexer->line = 1;
    lexer->state = state;
    lexer->tokens = NULL;
    lexer->token_size = TOKEN_INIT_SIZE;
    lexer->build_token = heap_malloc(lexer->token_size);
    lexer->build_token[0] = '\0';
    lexer->stack_ptr = STACK_SIZE;
    lexer->token_handler = token_handler;
    lexer->get_input = get_input;
    lexer->get_input_data = get_input_data;
    lexer->bufptr = &lexer->buffer[BUFFER_SIZE];
}

static inline unsigned buffer_remaining(lexer_t lexer)
{
    return (&lexer->buffer[BUFFER_SIZE] - lexer->bufptr);
}

static uint8_t lex_get_input(lexer_t lexer)
{
    unsigned keep = 0;
    if (buffer_remaining(lexer) > 0) {
        keep = strlen(lexer->bufptr);
        /* use memmove as it allows overlap */
        if (keep)
            memmove(lexer->buffer, lexer->bufptr, keep);
    }

    char *rv = lexer->get_input(lexer, &lexer->buffer[keep], BUFFER_SIZE - keep, lexer->get_input_data);
    if (!rv)
        return 0;

    lexer->bufptr = &lexer->buffer[0];

    return strlen(&lexer->buffer[keep]);
}

const char *lex_peekn(lexer_t lexer, uint8_t n)
{
    if (n > buffer_remaining(lexer)
     || n > strlen(lexer->bufptr))
    {
        unsigned got = lex_get_input(lexer);
        if (n > got) {
            return lexer->bufptr;
            // err_warning(ERR_LEXER_ERROR, "Unexpected end of input");
            // return "";
        }
    }

    return lexer->bufptr;
}

static lexer_t get_char(lexer_t /* in/out */lexer, char *output)
{
    *output = lex_peek(lexer);

    if (*output == '\n') {
        lexer->line++;
        lexer->pos = 1;
    }
    if (*output == '\0')
        return lexer;

    lexer->bufptr++;
    lexer->pos++;

    return lexer;
}

lexer_t lex_token_add_char(lexer_t lexer, char ch)
{
    unsigned l = strlen(lexer->build_token);

    if (l >= lexer->token_size - 1) {
        lexer->build_token = heap_realloc(lexer->build_token, 2 * lexer->token_size);
        lexer->token_size *= 2;
    }

    lexer->build_token[l] = ch;
    lexer->build_token[l+1] = '\0';

    return lexer;
}

lexer_t lex_read(lexer_t lexer)
{
    char ch;
    lexer = get_char(lexer, &ch);
    return lex_token_add_char(lexer, ch);
}

/* TODO: optimise this */
lexer_t lex_readn(lexer_t lexer, uint8_t n)
{
    while (n --> 0)
        lexer = lex_read(lexer);

    return lexer;
}

lexer_t lex_skip(lexer_t lexer)
{
    char ch;
    return get_char(lexer, &ch);
}

lexer_t lex_skipn(lexer_t lexer, uint8_t n)
{
    char ch;
    while (n-- > 0)
        lexer = get_char(lexer, &ch);
    return lexer;
}

uint8_t lex_coming_up(lexer_t lexer, const char *s, unsigned c)
{
    if (c == 0)
        c = strlen(s);

    const char *peek = lex_peekn(lexer, c);

    return strncmp(peek, s, c) == 0;
}

uint8_t lex_is_eos(lexer_t lexer)
{
    if ((buffer_remaining(lexer) == 0 || lexer->bufptr[0] == '\0')
     && (lex_get_input(lexer) == 0))
    {
        return 1;
    }

    return 0;
}

void lex_free_tokens(lex_token_t *token)
{
    while (token != NULL) {
        lex_token_t *next = token->next;
        heap_free(token->value);
        heap_free(token);
        token = next;
    }
}

lexer_t lex_emit_token(lexer_t lexer, token_type_t type, const char *s)
{
    lex_token_t *token = heap_malloc(sizeof(lex_token_t));

    token->value = heap_malloc(strlen(s) + 1);
    strcpy(token->value, s);

    token->type = type;
    token->pos = lexer->pos;
    token->line = lexer->line;
    token->next = NULL;

    return lexer->token_handler(lexer, token);
}

lexer_t lex_emit(lexer_t lexer, token_type_t type)
{
    lexer = lex_emit_token(lexer, type, lexer->build_token);
    return lex_clear(lexer);
}

lexer_t lex_clear(lexer_t lexer)
{
    if (lexer->token_size != TOKEN_INIT_SIZE) {
        heap_free(lexer->build_token);
        lexer->build_token = heap_malloc(TOKEN_INIT_SIZE);
        lexer->token_size = TOKEN_INIT_SIZE;
    }
    lexer->build_token[0] = '\0';
    return lexer;
}

lexer_t lex_assoc_state(lexer_t lexer, lexer_fn state)
{
    if (state == LEX_STATE_RETURN) {
        if (lexer->stack_ptr == STACK_SIZE) {
            err_fatal(ERR_LEXER_ERROR, "Stack underflow");
        }
        /* Pop state from stack */
        lexer->state = lexer->stack[lexer->stack_ptr++];
    } else {
        lexer->state = state;
    }
    return lexer;
}

lexer_t lex_push_return(lexer_t lexer, lexer_fn state)
{
    if (lexer->stack_ptr == 0) {
        err_fatal(ERR_LEXER_ERROR, "Stack overflow");
    }

    lexer->stack_ptr--;
    if (state == LEX_STATE_CURRENT) {
        lexer->stack[lexer->stack_ptr] = lexer->state;
    } else {
        lexer->stack[lexer->stack_ptr] = state;
    }
    return lexer;
}

static lexer_t null_state(lexer_t lexer)
{
    return NULL;
}

lexer_t lex_null(lexer_t lexer)
{
    lexer->state = null_state;
    return lexer;
}

static lexer_t default_token_handler(lexer_t lexer, lex_token_t *token)
{
    token->next = lexer->tokens;
    lexer->tokens = token;
    return lexer;
}

void lex_destroy(lexer_t lexer)
{
    if (lexer)
        heap_free(lexer->build_token);
    lex_free_tokens(lexer->tokens);
    heap_free(lexer);
}

static void lex_do_process(lexer_t lexer)
{
    while(lexer != NULL) {
        lexer = lexer->state(lexer);
    }
}

lex_token_t *lex_process(
    lexer_fn initial_state,
    lexer_token_handler token_handler,
    lexer_get_input_t get_input,
    void *get_input_data
) {
    lexer_t lexer = heap_malloc(sizeof(struct lexer));

    if (token_handler == NULL)
        token_handler = default_token_handler;

    lex_initialise(lexer,
                   initial_state,
                   token_handler,
                   get_input,
                   get_input_data);

    lex_do_process(lexer);

    lex_token_t *tokens = lexer->tokens;

    lexer->tokens = NULL;
    lex_destroy(lexer);

    return tokens;
}

lexer_t lex_error(lexer_t lexer, const char *msg)
{
    err_warning(
        ERR_LEXER_ERROR,
        "%s at line %u, column %d",
        msg,
        lexer->line,
        (int)lexer->pos
    );

    lex_free_tokens(lexer->tokens);
    lexer->tokens = NULL;
    return lex_null(lexer);
}

lex_token_t *lex_get_tokens(lexer_t lexer)
{
    return lexer->tokens;
}
#endif /* LEXASM */
