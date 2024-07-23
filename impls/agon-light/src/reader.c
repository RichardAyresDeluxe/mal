#include <string.h>
#include <stdio.h>

#include "lex_lisp.h"
#include "err.h"
#include "reader.h"
#include "printer.h"
#include "list.h"
#include "listsort.h"
#include "gc.h"
#include "vec.h"
#include "eval.h"
#include "map.h"

#ifdef AGON_LIGHT
#include <mos_api.h>
#endif


static char *repl_fgets(lexer_t lexer, char *s, int n, void *prompt);
static MalVal *read_form(lex_token_t *token, lex_token_t **next);
static MalVal *read_atom(lex_token_t *token, lex_token_t **next);
static MalVal *read_list(lex_token_t *token, lex_token_t **next);
static MalVal *read_vector(lex_token_t *token, lex_token_t **next);
static MalVal *read_map(lex_token_t *token, lex_token_t **next);
static MalVal *read_set(lex_token_t *token, lex_token_t **next);
static MalVal *reader_macro(const char *name, lex_token_t *token, lex_token_t **next);
static MalVal *reader_withmeta(lex_token_t *token, lex_token_t **next);

MalVal *read_str(void)
{
  lex_token_t *end;
  lex_token_t *tokens = parse_lisp(repl_fgets, "user> ");
  if (!tokens)
    return malval_nil();

  linked_list_reverse((void**)&tokens);

  MalVal *val = read_form(tokens, &end);
  lex_free_tokens(tokens);
  return val;
}

MalVal *read_string(char *s)
{
  lex_token_t *end;
  lex_token_t *tokens = parse_lisp_string(s);
  if (!tokens)
    return malval_nil();

  linked_list_reverse((void**)&tokens);

  MalVal *val = read_form(tokens, &end);
  lex_free_tokens(tokens);
  return val;
}

static MalVal *parse(lexer_get_input_t get_input, void *fh, ENV *env)
{
  MalVal *result = NULL;

  lex_token_t *tokens = parse_lisp(get_input, fh);
  linked_list_reverse((void**)&tokens);

  lex_token_t *token = tokens;
  while(token) {
    result = EVAL(read_form(token, &token), env);
    gc_mark(result, NULL);
    gc(TRUE);
  }

  lex_free_tokens(tokens);

  return result ? result : NIL;
}

#ifdef AGON_LIGHT
#define CHAR_DEL 0x7f
static char *preprocess_text(char *s)
{
  /* Process DEL chars */

  while (*s == CHAR_DEL) {  /* DEL at start of string - ignore it */
    memmove(s, &s[1], strlen(&s[1]) + 1);
  }

  for (char *c = s; *c; c++) {
    if (*c == CHAR_DEL) {   // DEL character
      unsigned l = strlen(c);
      memmove(c - 1, c + 1, l - 1);
      *(c + l - 2) = '\0';
      c -= 2;
    }
  }

  return s;
}

static char *mos_fgets(lexer_t lexer, char *buf, int sz, void *data)
{
  uint8_t *fh = data;

  if (mos_feof(*fh))
    return NULL;

  unsigned rc = mos_fread(*fh, buf, (unsigned)sz-1);
  if (rc == 0)
    return NULL;

  buf[rc] = '\0';

  preprocess_text(buf);

  return buf;
}

MalVal *load_file(const char *fname, ENV *env)
{
  MalVal *result = NULL;

  uint8_t fh = mos_fopen(fname, FA_OPEN_EXISTING|FA_READ);
  if (!fh)
    malthrow("cannot open file %s");

  while (!mos_feof(fh)) {
    result = parse(mos_fgets, &fh, env);
  }

  mos_fclose(fh);

  return result ? result : NIL;
}
#else
static char *file_fgets(lexer_t lexer, char *s, int n, void *_fh)
{
  return fgets(s, n, _fh);
}

MalVal *load_file(const char *fname, ENV *env)
{
  MalVal *result = NULL;

  FILE *fh = fopen(fname, "r");
  if (!fh)
    malthrow("Cannot open file");

  while (!feof(fh)) {
    result = parse(file_fgets, fh, env);
  }

  fclose(fh);

  return result ? result : NIL;
}
#endif

MalVal *read_form(lex_token_t *token, lex_token_t **next)
{
  if (!token) {
    err_fatal(ERR_LEXER_ERROR, "Out of tokens");
  }

  MalVal *rv = NULL;

  switch(token->type) {
    case TOKEN_TYPE_LIST_START:
      rv = read_list(token->next, next);
      break;
    case TOKEN_TYPE_VEC_START:
      rv = read_vector(token->next, next);
      break;
    case TOKEN_TYPE_MAP_START:
      rv = read_map(token->next, next);
      break;
    case TOKEN_TYPE_SET_START:
      rv = read_set(token->next, next);
      break;
    default:
      rv = read_atom(token, next);
      break;
  }

  gc_mark(rv, NULL);
  return rv;
}

MalVal *read_atom(lex_token_t *token, lex_token_t **next)
{
  MalVal *val;

  switch(token->type) {
    case TOKEN_TYPE_NUMBER:
      val = malval_number(strtol(token->value, NULL, 10));
      *next = token->next;
      return val;

    case TOKEN_TYPE_SYMBOL:
      if (strcmp(token->value, "nil") == 0) {
        val = NIL;
        *next = token->next;
        return val;
      }
      else if (strcmp(token->value, "false") == 0) {
        val = F;
        *next = token->next;
        return val;
      }
      else if (strcmp(token->value, "true") == 0) {
        val = T;
        *next = token->next;
        return val;
      }
      /* not nil, false or true: fall through */
      val = malval_symbol(token->value);
      *next = token->next;
      return val;

    case TOKEN_TYPE_KEYWORD: {
      val = malval_keyword(token->value);
      *next = token->next;
      return val;
    }
    case TOKEN_TYPE_STRING:
      val = malval_string(token->value);
      *next = token->next;
      return val;

    case TOKEN_TYPE_QUOTE:
      return reader_macro("quote", token->next, next);
    case TOKEN_TYPE_QUASIQUOTE:
      return reader_macro("quasiquote", token->next, next);
    case TOKEN_TYPE_DEREF:
      return reader_macro("deref", token->next, next);
    case TOKEN_TYPE_UNQUOTE:
      return reader_macro("unquote", token->next, next);
    case TOKEN_TYPE_UNQUOTESPLICE:
      return reader_macro("splice-unquote", token->next, next);
    case TOKEN_TYPE_WITHMETA:
      return reader_withmeta(token->next, next);
  }
      
  err_warning(ERR_LEXER_ERROR, "Unknown token");
  return malval_nil();
}

MalVal *read_list(lex_token_t *token, lex_token_t **next)
{
  List *list = NULL;
  lex_token_t *rover = token;

  while (rover && !TOKEN_IS_END(rover)) {
    list = cons_weak(read_form(rover, &rover), list);
  }

  *next = rover ? rover->next : NULL;

  linked_list_reverse((void**)&list);

  return malval_list_weak(list);
}

MalVal *read_vector(lex_token_t *token, lex_token_t **next)
{
  Vec *vec = vec_create();
  lex_token_t *rover = token;

  while (rover && !TOKEN_IS_END(rover)) {
    vec_append(vec, read_form(rover, &rover));
  }

  *next = rover ? rover->next : NULL;

  return malval_vector(vec);
}

MalVal *read_set(lex_token_t *token, lex_token_t **next)
{
  Map *set = map_create();

  lex_token_t *rover = token;
  while (rover && !TOKEN_IS_END(rover)) {
    MalVal *val = read_form(rover, &rover);

    map_add(set, val, NIL);
  }

  *next = rover ? rover->next : NULL;

  MalVal *val = malval_set(set);
  map_release(set);
  return val;
}

MalVal *read_map(lex_token_t *token, lex_token_t **next)
{
  Map *map = map_create();

  lex_token_t *rover = token;
  while (rover && !TOKEN_IS_END(rover)) {
    MalVal *key = read_form(rover, &rover);
    if (TOKEN_IS_END(rover)) {
      map_release(map);
      malthrow("odd number of tokens in map");
    }

    MalVal *val = read_form(rover, &rover);

    map_add(map, key, val);
  }

  *next = rover ? rover->next : NULL;

  MalVal *val = malval_map(map);
  map_release(map);
  return val;
}

MalVal *reader_macro(const char *name, lex_token_t *token, lex_token_t **next)
{
  if (!token) {
    err_warning(ERR_LEXER_ERROR, "out of tokens in reader macro");
    return NIL;
  }

  List *list = NULL;
  list = cons_weak(read_form(token, next), list);
  list = cons_weak(malval_symbol(name), list);
  return malval_list_weak(list);
}

MalVal *reader_withmeta(lex_token_t *token, lex_token_t **next)
{
  if (!token) {
    err_warning(ERR_LEXER_ERROR, "out of tokens in reader macro");
    return malval_nil();
  }

  List *list = NULL;
  list = cons_weak(read_form(token, &token), list);
  list = cons_weak(read_form(token, next), list);
  list = cons_weak(malval_symbol("with-meta"), list);
  return malval_list_weak(list);
}

int token_depth(lex_token_t *tok)
{
  int depth = 0;
  while (tok) {
    if ((tok->type & META_TOKEN_START_END) == META_TOKEN_START)
      depth++;
    else if ((tok->type & META_TOKEN_START_END) == META_TOKEN_END)
      depth--;

    tok = tok->next;
  }
  return depth;
}

#if defined(EZ80)
extern uint8_t  mos_editline(char *buffer, unsigned bufferlength, uint8_t clearbuffer);
static char *repl_fgets(lexer_t lexer, char *s, int n, void *prompt)
{
  if (n == 0)
    err_fatal(ERR_INVALID_OPERATION, "request for zero characters on input");

  lex_token_t *tok = lex_get_tokens(lexer);
  int depth = token_depth(tok);
  if (tok && depth == 0)
    return NULL;

  fputs(prompt, stdout);

  char c = mos_editline(s, n-1, 0x05); //0b00000101
  char *rv;
  fputs("\n", stdout);
  if (c == 27) {
    rv = "";
  } else {
    /* need to add whitespace to terminate the token */
    int l = strlen(s);
    s[l] = ' ';
    s[l+1] = '\0';
    rv = s;
  }

  if (rv[0] == '\n' && rv[1] == '\0')
    return NULL;

  return rv;
}
#else
static char *repl_fgets(lexer_t lexer, char *s, int n, void *prompt)
{
  if (n == 0)
    err_fatal(ERR_INVALID_OPERATION, "request for zero characters on input");

  lex_token_t *tok = lex_get_tokens(lexer);
  int depth = token_depth(tok);
  if (tok && depth == 0)
    return NULL;

  fputs(prompt, stdout);

  if (feof(stdin)) {
    lex_destroy(lexer);
    exit(0);
  }

  char *rv = fgets(s, n, stdin);
  if (!rv)
    return NULL;

  if (rv[0] == '\n' && rv[1] == '\0')
    return NULL;

  return rv;
}
#endif
