#include <string.h>
#include <stdio.h>

#include "lex_lisp.h"
#include "err.h"
#include "reader.h"
#include "mallist.h"
#include "heap.h"
#include "listsort.h"

extern char *strdup(const char *);

static char *repl_fgets(lexer_t lexer, char *s, int n, void *prompt);
static MalVal *read_form(lex_token_t *tokens, lex_token_t **next);
static MalVal *read_atom(lex_token_t *tokens, lex_token_t **next);
static MalVal *read_list(lex_token_t *tokens, lex_token_t **next);
static MalVal *read_vector(lex_token_t *tokens, lex_token_t **next);

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

MalVal *read_form(lex_token_t *token, lex_token_t **next)
{
  switch(token->type) {
    case TOKEN_TYPE_LIST_START:
      return read_list(token->next, next);
    case TOKEN_TYPE_VEC_START:
      return read_vector(token->next, next);
  }

  return read_atom(token, next);
}

MalVal *read_atom(lex_token_t *token, lex_token_t **next)
{
  MalVal *val;

  switch(token->type) {
    case TOKEN_TYPE_NUMBER:
      val = malval_create(TYPE_NUMBER);
      val->data.number = strtol(token->value, NULL, 10);
      *next = token->next;
      return val;

    case TOKEN_TYPE_SYMBOL:
      if (strcmp(token->value, "nil") == 0) {
        val = malval_create(TYPE_NIL);
        *next = token->next;
        return val;
      }
      else if (strcmp(token->value, "false") == 0) {
        val = malval_create(TYPE_BOOL);
        val->data.number = 0;
        *next = token->next;
        return val;
      }
      else if (strcmp(token->value, "true") == 0) {
        val = malval_create(TYPE_BOOL);
        val->data.number = 1;
        *next = token->next;
        return val;
      }
      /* not nil, false or true: fall through */
      val = malval_create(TYPE_SYMBOL);
      val->data.string = strdup(token->value);
      *next = token->next;
      return val;

    case TOKEN_TYPE_KEYWORD: {
      char *s = heap_malloc(1 + strlen(token->value));
      s[0] = -1;
      strcpy(&s[1], &token->value[1]);
      val = malval_create(TYPE_SYMBOL);
      val->data.string = s;
      *next = token->next;
      return val;
    }
    case TOKEN_TYPE_STRING:
      val = malval_create(TYPE_STRING);
      val->data.string = strdup(token->value);
      *next = token->next;
      return val;

  }
      
  err_fatal(ERR_LEXER_ERROR, "Unknown token");
  return NULL;
}

MalVal *read_list(lex_token_t *token, lex_token_t **next)
{
  MalList *list = NULL;
  lex_token_t *rover = token;

  while (rover && rover->type != TOKEN_TYPE_LIST_END) {
    list = cons(read_form(rover, &rover), list);
  }

  if (rover)
    *next = rover->next;

  linked_list_reverse((void**)&list);

  MalVal *val = malval_create(TYPE_LIST);
  val->data.list = list;
  return val;
}

MalVal *read_vector(lex_token_t *token, lex_token_t **next)
{
  MalList *list = NULL;
  lex_token_t *rover = token;

  while (rover && rover->type != TOKEN_TYPE_VEC_END) {
    list = cons(read_form(rover, &rover), list);
  }

  if (rover)
    *next = rover->next;

  linked_list_reverse((void**)&list);

  MalVal *val = malval_create(TYPE_VECTOR);
  val->data.vec = list;
  return val;
}

static int token_depth(lex_token_t *tok)
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

  if (feof(stdin))
    exit(0);

  char *rv = fgets(s, n, stdin);
  if (!rv)
    return NULL;

  if (rv[0] == '\n' && rv[1] == '\0')
    return NULL;

  return rv;
}
#endif
