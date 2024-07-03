#include "lex.h"
#include "lex_lisp.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

struct token_name { token_type_t type; const char *name; };
static struct token_name token_names[] = {
    { TOKEN_TYPE_STRING, "string" },
    { TOKEN_TYPE_NUMBER, "number" },
    { TOKEN_TYPE_COMMENT, "comment" },
    { TOKEN_TYPE_METADATA, "metadata" },
    { TOKEN_TYPE_KEYWORD, "keyword" },
    { TOKEN_TYPE_SYMBOL, "symbol" },
    { TOKEN_TYPE_CHAR, "char" },
    { TOKEN_TYPE_SYMBOL_REF, "symbol_ref" },
    { TOKEN_TYPE_LIST_START, "list_start" },
    { TOKEN_TYPE_LIST_END, "list_end" },
    { TOKEN_TYPE_MAP_START, "map_start" },
    { TOKEN_TYPE_MAP_END, "map_end" },
    { TOKEN_TYPE_VEC_START, "vec_start" },
    { TOKEN_TYPE_VEC_END, "vec_end" },
    { TOKEN_TYPE_SET_START, "set_start" },
    { TOKEN_TYPE_SET_END, "set_end" },
    { TOKEN_TYPE_LAMBDA_START, "lambda_start" },
    { TOKEN_TYPE_LAMBDA_END, "lambda_end" },
};

const char *get_token_name(token_type_t tt)
{
    for (unsigned i = 0; i < sizeof(token_names) / sizeof(token_names[0]); i++) {
        if (token_names[i].type == tt)
            return token_names[i].name;
    }
    return "unknown";
}

/* TODO: this can probably be optimised */
static int lex_is_symbol(lexer_t lexer)
{
    if (lex_is_letter(lexer) || lex_is_digit(lexer))
        return 1;

    switch(lex_peek(lexer)) {
        case '~':
        case '@':
        case '-':
        case '+':
        case '\\':
        case '/':
        case '*':
        case '_':
        case '=':
        case '>':
        case '<':
        case '?':
        case '!':
        case '.':
        case '%':
        case '\'':
        case '$':
            return 1;
    }

    return 0;
}

static lexer_t lex_string_quote(lexer_t lexer)
{
    switch(lex_peek(lexer)) {
        case 'n': return lex_token_add_char(lex_skip(lexer), 0x0a);
        case 'r': return lex_token_add_char(lex_skip(lexer), 0x0d);
        case 't': return lex_token_add_char(lex_skip(lexer), 0x09);
        case '"': return lex_token_add_char(lex_skip(lexer), 0x22);
    }
    if (lex_is_eos(lexer))
        return lex_error(lexer, "Unterminated string");
    return lex_read(lexer);
}

static lexer_t lex_string(lexer_t lexer)
{
    switch(lex_peek(lexer)) {
        case '\\': return lex_string_quote(lex_skip(lexer));
        case '"': return lex_return(lex_emit(lex_skip(lexer), TOKEN_TYPE_STRING));
    }
    if (lex_is_eos(lexer))
        return lex_error(lexer, "Unterminated string");
    return lex_read(lexer);
}

static lexer_t lex_hex_number(lexer_t lexer)
{
    char ch = lex_peek(lexer);
    if ((ch >= 0x30 && ch <= 0x39)
     || (ch >= 0x61 && ch <= 0x66)
     || (ch >= 0x41 && ch <= 0x46)) {
        return lex_read(lexer);
    }
    if (lex_peek(lexer) == 'L')
        return lex_return(lex_emit(lex_read(lexer), TOKEN_TYPE_NUMBER));
    if (lex_is_letter(lexer))
        return lex_error(lexer, "Invalid hexadecimal digit");
    return lex_return(lex_emit(lexer, TOKEN_TYPE_NUMBER));
}

static lexer_t lex_oct_number(lexer_t lexer)
{
    char ch = lex_peek(lexer);
    if (ch >= 0x30 && ch <= 0x37)
        return lex_read(lexer);
    if (lex_peek(lexer) == 'L')
        return lex_return(lex_emit(lex_read(lexer), TOKEN_TYPE_NUMBER));
    if (lex_is_digit(lexer) || lex_is_letter(lexer))
        return lex_error(lexer, "Invalid octal digit");
    return lex_return(lex_emit(lexer, TOKEN_TYPE_NUMBER));
}

static lexer_t lex_decimal(lexer_t lexer)
{
    if (lex_is_digit(lexer))
        return lex_read(lexer);
    if (lex_peek(lexer) == '.')
        return lex_error(lexer, "Unexpected decimal point");
    if (lex_is_digit(lexer))
        return lex_error(lexer, "Unexpected letter");
    return lex_return(lex_emit(lexer, TOKEN_TYPE_NUMBER));
}

static lexer_t lex_number(lexer_t lexer)
{
    if (lex_is_digit(lexer))
        return lex_read(lexer);
    if (lex_peek(lexer) == '.')
        return lex_assoc_state(lex_read(lexer), lex_decimal);
    if (lex_peek(lexer) == 'L')
        return lex_return(lex_emit(lex_read(lexer), TOKEN_TYPE_NUMBER));
    if (lex_is_digit(lexer))
        return lex_error(lexer, "Unexpected letter");
    return lex_return(lex_emit(lexer, TOKEN_TYPE_NUMBER));
}

static lexer_t lex_comment(lexer_t lexer)
{
#ifdef KEEP_COMMENTS
    if (lex_is_eos(lexer))
        return lex_return(lex_emit(lexer, TOKEN_TYPE_COMMENT));
    if (lex_peek(lexer) == '\n')
        return lex_return(lex_emit(lex_skip(lexer), TOKEN_TYPE_COMMENT));
    if (lex_coming_up(lexer, "\r\n", 2))
        return lex_return(lex_emit(lex_skipn(lexer, 2), TOKEN_TYPE_COMMENT));
    return lex_read(lexer);
#else
    if (lex_is_eos(lexer))
        return lex_return(lexer);
    if (lex_peek(lexer) == '\n')
        return lex_return(lex_skip(lexer));
    if (lex_coming_up(lexer, "\r\n", 2))
        return lex_return(lex_skipn(lexer, 2));
    return lex_skip(lexer);
#endif
}

static lexer_t lex_comment_start(lexer_t lexer)
{
    return lex_assoc_state(lex_skip(lexer), lex_comment);
}

static lexer_t lex_sequence_inside(lexer_t lexer);

static lexer_t lex_seq_start(
    lexer_t lexer,
    int n,
    token_type_t start_token,
    lexer_fn end_state
) {
    lexer = lex_readn(lexer, n);
    lexer = lex_emit(lexer, start_token);
    lexer = lex_push_return(lexer, end_state);
    lexer = lex_assoc_state(lexer, lex_sequence_inside);
    return lexer;
}

static lexer_t lex_seq_end(
    lexer_t lexer,
    char end_char,
    token_type_t end_token
) {
    if (lex_peek(lexer) == end_char)
        return lex_return(lex_emit(lex_read(lexer), end_token));
    if (lex_is_eos(lexer))
        return lex_error(lexer, "Unterminated seq");
    return lex_error(lexer, "Unexpected character");
}

static lexer_t lex_list_end(lexer_t lexer) {
    return lex_seq_end(lexer, ')', TOKEN_TYPE_LIST_END);
}

static lexer_t lex_list_start(lexer_t lexer) {
    return lex_seq_start(lexer, 1, TOKEN_TYPE_LIST_START, lex_list_end);
}

static lexer_t lex_map_end(lexer_t lexer) {
    return lex_seq_end(lexer, '}', TOKEN_TYPE_MAP_END);
}
static lexer_t lex_map_start(lexer_t lexer) {
    return lex_seq_start(lexer, 1, TOKEN_TYPE_MAP_START, lex_map_end);
}
static lexer_t lex_vector_end(lexer_t lexer) {
    return lex_seq_end(lexer, ']', TOKEN_TYPE_VEC_END);
}
static lexer_t lex_vector_start(lexer_t lexer) {
    return lex_seq_start(lexer, 1, TOKEN_TYPE_VEC_START, lex_vector_end);
}
static lexer_t lex_set_end(lexer_t lexer) {
    return lex_seq_end(lexer, '}', TOKEN_TYPE_SET_END);
}
static lexer_t lex_set_start(lexer_t lexer) {
    return lex_seq_start(lexer, 2, TOKEN_TYPE_SET_START, lex_set_end);
}
static lexer_t lex_lambda_end(lexer_t lexer) {
    return lex_seq_end(lexer, ')', TOKEN_TYPE_LAMBDA_END);
}
static lexer_t lex_lambda_start(lexer_t lexer) {
    return lex_seq_start(lexer, 2, TOKEN_TYPE_LAMBDA_START, lex_lambda_end);
}

static lexer_t lex_symbol_or_keyword(lexer_t lexer, token_type_t token_type)
{
    if (lex_is_symbol(lexer))
        return lex_read(lexer);
    return lex_return(lex_emit(lexer, token_type));
}

static lexer_t lex_keyword(lexer_t lexer) {
    return lex_symbol_or_keyword(lexer, TOKEN_TYPE_KEYWORD);
}
static lexer_t lex_symbol(lexer_t lexer) {
    return lex_symbol_or_keyword(lexer, TOKEN_TYPE_SYMBOL);
}

static lexer_t lex_start_metadata(lexer_t lexer)
{
    char ch = lex_peek(lexer);

    switch(ch) {
    case ':':
        return lex_assoc_state(lex_read(lex_emit(lexer, TOKEN_TYPE_METADATA)), lex_keyword);
    case '{':
        return lex_assoc_state(lex_emit(lexer, TOKEN_TYPE_METADATA), lex_map_start);
    default:
        if (isalpha(ch))
            return lex_assoc_state(lex_emit(lexer, TOKEN_TYPE_METADATA), lex_symbol);
        break;
    }

    return lex_error(lexer, "Unexpected character in metadata");
}

static lexer_t lex_char_octal(lexer_t lexer)
{
    if (lex_is_digit(lexer))
        return lex_read(lexer);
    return lex_return(lex_emit(lexer, TOKEN_TYPE_CHAR));
}

static lexer_t lex_character(lexer_t lexer)
{
    if (lex_is_letter(lexer))
        return lex_read(lexer);
    return lex_return(lex_emit(lexer, TOKEN_TYPE_CHAR));
}

static lexer_t lex_char_start(lexer_t lexer)
{
    const char *s = lex_peekn(lexer, 2);
    if (s[0] == 'o' && isdigit(s[1]))
        return lex_assoc_state(lex_read(lexer), lex_char_octal);

    return lex_assoc_state(lex_read(lexer), lex_character);
}

static lexer_t lex_symbol_slurp(lexer_t lexer)
{
    /* Skip whitespace between ampersand and symbol */
    if (lex_is_whitespace(lexer))
        return lex_skip(lexer);

    if (lex_is_symbol(lexer))
        return lex_read(lexer);

    return lex_return(lex_emit(lexer, TOKEN_TYPE_SYMBOL));
}

static lexer_t lex_quote_end_list(lexer_t lexer)
{
    return lex_emit_token(lex_list_end(lexer), TOKEN_TYPE_LIST_END, ")");
}

static lexer_t lex_quote_end_vec(lexer_t lexer)
{
    return lex_emit_token(lex_vector_end(lexer), TOKEN_TYPE_LIST_END, ")");
}

static lexer_t lex_quote_start(lexer_t lexer)
{
    if (lex_is_whitespace(lexer))
        return lex_skip(lexer);

    char ch = lex_peek(lexer);
    if (ch == '(' || ch == '[') {
        lexer = lex_emit_token(lexer, TOKEN_TYPE_LIST_START, "(");
        lexer = lex_emit_token(lexer, TOKEN_TYPE_SYMBOL, "quote");
        if (ch == '(')
            return lex_seq_start(lexer, 1, TOKEN_TYPE_LIST_START, lex_quote_end_list);
        if (ch == '[')
            return lex_seq_start(lexer, 1, TOKEN_TYPE_VEC_START, lex_quote_end_vec);
    }
    if (lex_is_eos(lexer))
        return lex_error(lexer, "Unexpected end of input");
    return lex_error(lexer, "Unexpected character");
}

static lexer_t lex_deref_end(lexer_t lexer)
{
    lexer = lex_emit_token(lexer, TOKEN_TYPE_LIST_END, ")");
    return lex_return(lexer);
}

static lexer_t lex_deref_start(lexer_t lexer)
{
    lexer = lex_emit_token(lexer, TOKEN_TYPE_LIST_START, "(");
    lexer = lex_emit_token(lexer, TOKEN_TYPE_SYMBOL, "deref");
    lexer = lex_push_return(lexer, lex_deref_end);
    return lex_assoc_state(lexer, lex_symbol);
}

static lexer_t lex_sequence_inside(lexer_t lexer)
{
    if (lex_is_eos(lexer))
        return lex_return(lexer);

    char peek = lex_peek(lexer);

    if (lex_is_whitespace(lexer))
        return lex_skip(lexer);
    if (peek == '0') {
        const char *peeks = lex_peekn(lexer, 2);
        if (peeks[1] == 'x')
            return lex_state_call(lex_readn(lexer, 2), lex_hex_number);
    }
    if (lex_is_digit(lexer))
        return lex_state_call(lex_read(lexer), lex_number);
    if (peek == '-') {
        const char *peeks = lex_peekn(lexer, 2);
        if (peeks[1] >= '0' && peeks[1] <= '9')
            return lex_state_call(lex_read(lexer), lex_number);
    }

    switch(peek) {
        case ',': return lex_skip(lexer);
        case '\"': return lex_state_call(lex_skip(lexer), lex_string);
        case '0': return lex_state_call(lex_read(lexer), lex_oct_number);
        case ';': return lex_state_call(lexer, lex_comment_start);
        case '(': return lex_state_call(lexer, lex_list_start);
        case '{': return lex_state_call(lexer, lex_map_start);
        case '[': return lex_state_call(lexer, lex_vector_start);
        case '&': return lex_state_call(lex_read(lexer), lex_symbol_slurp);
        case ':': return lex_state_call(lex_read(lexer), lex_keyword);
        case '\\': return lex_state_call(lex_skip(lexer), lex_char_start);
        case '`':
        case '\'': return lex_state_call(lex_skip(lexer), lex_quote_start);
        case '@': return lex_state_call(lex_skip(lexer), lex_deref_start);
        case ')':
        case '}':
        case ']': return lex_return(lexer);
        case '#': {
            const char *peeks = lex_peekn(lexer, 2);
            if (peeks[1] == '{')
                return lex_state_call(lexer, lex_set_start);
            if (peeks[1] == '(')
                return lex_state_call(lexer, lex_lambda_start);
            if (peeks[1] == '\'') {
                lexer = lex_readn(lexer, 2);
                lexer = lex_emit(lexer, TOKEN_TYPE_SYMBOL_REF);
                return lex_state_call(lexer, lex_symbol);
            }
            break;
        }
        case '^': {
            const char *peeks = lex_peekn(lexer, 2);
            if (peeks[1] == ':' || peeks[1] == '{' || isalpha(peeks[1]))
                return lex_state_call(lex_skip(lexer), lex_start_metadata);
            break;
        }
    }

    if (lex_is_symbol(lexer))
        return lex_state_call(lexer, lex_symbol);

    return lex_return(lexer);
}

static lexer_t lex_root(lexer_t lexer)
{
    if (lex_is_eos(lexer))
        return lex_null(lex_clear(lexer));
    return lex_sequence_inside(lexer);
}

lex_token_t *parse_lisp(lexer_get_input_t get_input, void *get_input_data)
{
    return lex_process(lex_root, NULL, get_input, get_input_data);
}
