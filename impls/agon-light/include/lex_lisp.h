#ifndef _LEX_LISP_H
#define _LEX_LISP_H

#include "lex.h"

extern lex_token_t *parse_lisp(lexer_get_input_t, void*);
extern const char *get_token_name(token_type_t);

#define TOKEN_TYPE_STRING       0x01  /* 0b00000001 */
#define TOKEN_TYPE_NUMBER       0x02  /* 0b00000010 */
#define TOKEN_TYPE_COMMENT      0x03  /* 0b00000011 */
#define TOKEN_TYPE_METADATA     0x04  /* 0b00000100 */
#define TOKEN_TYPE_KEYWORD      0x05  /* 0b00000101 */
#define TOKEN_TYPE_SYMBOL       0x06  /* 0b00000110 */
#define TOKEN_TYPE_CHAR         0x07  /* 0b00000111 */
#define TOKEN_TYPE_SYMBOL_REF   0x08  /* 0b00001000 */
#define TOKEN_TYPE_LIST_START   0x80  /* 0b10000000 */
#define TOKEN_TYPE_LIST_END     0xC0  /* 0b11000000 */
#define TOKEN_TYPE_MAP_START    0x81  /* 0b10000001 */
#define TOKEN_TYPE_MAP_END      0xC1  /* 0b11000001 */
#define TOKEN_TYPE_VEC_START    0x82  /* 0b10000010 */
#define TOKEN_TYPE_VEC_END      0xC2  /* 0b11000010 */
#define TOKEN_TYPE_SET_START    0x83  /* 0b10000011 */
#define TOKEN_TYPE_SET_END      0xC3  /* 0b11000011 */
#define TOKEN_TYPE_LAMBDA_START 0x84  /* 0b10000100 */
#define TOKEN_TYPE_LAMBDA_END   0xC4  /* 0b11000100 */

#define META_TOKEN_CONTAINER    0x80  /* 0b10000000 */
#define META_TOKEN_START_END    0xC0  /* 0b11000000 */
#define META_TOKEN_START        0x80  /* 0b10000000 */
#define META_TOKEN_END          0xC0  /* 0b11000000 */

#endif /* _LEX_LISP_H */
