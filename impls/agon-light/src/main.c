#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "malval.h"
#include "list.h"
#include "gc.h"
#include "reader.h"
#include "printer.h"
#include "env.h"
#include "err.h"
#include "listsort.h"
#include "heap.h"
#include "function.h"
#include "core.h"
#include "eval.h"
#include "map.h"


ENV *repl_env = NULL;

static void warn_symbol_not_found(const char *name)
{
  unsigned l = strlen(name);
  l += 7; //strlen("symbol ");
  l += 10; //strlen(" not found");
  char *s = heap_malloc(l + 1);
  strcpy(s, "symbol ");
  strcat(s, name);
  strcat(s, " not found");
  err_warning(ERR_SYMBOL_NOT_FOUND, s);
  heap_free(s);
}

static MalVal *symbol_to_keyword(const char *symbol)
{
  unsigned l = strlen(symbol);
  char *s = heap_malloc(l + 1);

  s[0] = ':';
  memcpy(&s[1], &symbol[1], l - 1);
  s[l] = '\0';

  MalVal *value = malval_symbol(s);
  heap_free(s);

  return value;
}

MalVal *eval_ast(MalVal *ast, ENV *env)
{
  if (ast->type == TYPE_SYMBOL)
  {
    MalVal *value = NULL;
    if (ast->data.string[0] == -1) {
      /* is a keyword */
      value = symbol_to_keyword(ast->data.string);
    }
    else
      value = env_get(env, ast->data.string);

    if (value)
      return value;

    warn_symbol_not_found(ast->data.string);
    return NULL;
  }

  if (ast->type == TYPE_LIST)
  {
    List *evaluated = NULL;
    for (List *rover = ast->data.list; rover; rover = rover->tail) {
      evaluated = cons(EVAL(rover->head, env), evaluated);
    }
    linked_list_reverse((void**)&evaluated);
    MalVal *value = malval_list(evaluated);
    list_release(evaluated);
    return value;
  }

  if (ast->type == TYPE_VECTOR)
  {
    List *evaluated = NULL;
    for (List *rover = ast->data.list; rover; rover = rover->tail) {
      evaluated = cons(EVAL(rover->head, env), evaluated);
    }
    linked_list_reverse((void**)&evaluated);
    MalVal *value = malval_vector(evaluated);
    list_release(evaluated);
    return value;
  }

  if (ast->type == TYPE_MAP)
  {
    List *evaluated = NULL;
    unsigned i = 0;
    for (List *rover = ast->data.list; rover; rover = rover->tail) {
      if ((i++ % 2) == 1)
        evaluated = cons(EVAL(rover->head, env), evaluated);
      else
        evaluated = cons(rover->head, evaluated);
    }
    linked_list_reverse((void**)&evaluated);
    MalVal *value = malval_map(evaluated);
    list_release(evaluated);
    return value;

  }

  return ast;
}

static MalVal *EVAL_def(List *list, ENV *env)
{
  if (list_count(list) != 2) {
    err_warning(ERR_ARGUMENT_MISMATCH, "def! requires two arguments");
    return NIL;
  }

  const char *name = list->head->data.string;

  MalVal *value = EVAL(list->tail->head, env);
  if (!value) {
    warn_symbol_not_found(name);
    return NIL;
  }

  env_set(env, name, value);

  return value;
}

static MalVal *EVAL_fn_star(List *list, ENV *env)
{
  return function_create(list, env);
}

static void EVAL_do(List *list, ENV *env, MalVal **out)
{
  List *rover = list;
  while (rover && rover->tail) {
    MalVal *val = EVAL(rover->head, env);
    malval_reset_temp(val, NULL);
    rover = rover->tail;
  }

  *out = rover ? rover->head : NIL;
}

static void EVAL_if(List *list, ENV *env, MalVal **out)
{
  if (!list || !list->tail) {
    err_warning(ERR_ARGUMENT_MISMATCH, "need at least 2 arguments to if");
    *out = NIL;
    return;
  }

  MalVal *val = EVAL(list->head, env);
  if (VAL_IS_NIL(val) || VAL_IS_FALSE(val)) {
    /* false */
    *out = list->tail->tail ? list->tail->tail->head : NIL;
  }
  else {
    /* true */
    *out = list->tail->head;
  }
}

static void EVAL_let(List *list, ENV *env, MalVal **out, ENV **envout)
{
  if (list_count(list) != 2) {
    err_warning(ERR_ARGUMENT_MISMATCH, "let* requires two arguments");
    *out = NIL;
    return;
  }

  ENV *let = env_create(env, NULL, NULL);

  if (list->head->type != TYPE_LIST && list->head->type != TYPE_VECTOR) {
    err_warning(ERR_ARGUMENT_MISMATCH, "let* bindings argument must be list or vector");
    env_release(let);
    *out = NIL;
    return;
  }

  List *bindings = list->head->data.list;
  if ((list_count(bindings) % 2) != 0) {
    err_warning(ERR_ARGUMENT_MISMATCH, "let* bindings must have even number of entries");
    env_release(let);
    *out = NIL;
    return;
  }

  for (List *rover = bindings;
       rover && rover->tail;
       rover = rover->tail->tail)
  {
    if (rover->head->type != TYPE_SYMBOL) {
      err_warning(ERR_ARGUMENT_MISMATCH, "can only bind to symbols");
      env_release(let);
      *out = NIL;
      return;
    }
    env_set(let, rover->head->data.string, EVAL(rover->tail->head, let));
  }

  env_release(env);

  *out = list->tail->head;
  *envout = let;
}

static void EVAL_call(Function *fn, List *args, ENV *env, MalVal **out, ENV **envout)
{
  /* 
   * TCO mal function call
   */
  struct Body *b = NULL;
  ENV *new_env = function_bind(fn, args, &b);
  if (b == NULL) {
    err_warning(ERR_ARGUMENT_MISMATCH, "function arity mismatch");
    env_release(new_env);
    *out = NIL;
    return;
  }

  *out = b->body;
  *envout = new_env;
}

MalVal *EVAL(MalVal *ast, ENV *env)
{
  while (TRUE)
  {
    if (ast->type != TYPE_LIST)
      return eval_ast(ast, env);
    if (ast->data.list == NULL)
      return ast; /* empty list */

    List *list = ast->data.list;

    MalVal *head = list->head;
    List *tail = list->tail;

    if (head->type == TYPE_SYMBOL
     && strcmp(head->data.string, "def!") == 0
    ) {
      return EVAL_def(tail, env);
    }
    else if (head->type == TYPE_SYMBOL
          && strcmp(head->data.string, "let*") == 0
    ) {
      EVAL_let(tail, env, &ast, &env);
    }
    else if (head->type == TYPE_SYMBOL
          && strcmp(head->data.string, "do") == 0
    ) {
      EVAL_do(tail, env, &ast);
    }
    else if (head->type == TYPE_SYMBOL
          && strcmp(head->data.string, "fn*") == 0
    ) {
      return EVAL_fn_star(tail, env);
    }
    else if (head->type == TYPE_SYMBOL
          && strcmp(head->data.string, "if") == 0
    ) {
      EVAL_if(tail, env, &ast);
    }
    else {
      MalVal *f = eval_ast(ast, env);
      if (!f)
          return NULL;
      if (VAL_IS_NIL(f))
        return NIL;
      assert(f->type == TYPE_LIST);
      assert(f->data.list != NULL);

      if (VAL_IS_NIL(f->data.list->head))
        return NULL;

      if (VAL_TYPE(f->data.list->head) != TYPE_FUNCTION)
        return f->data.list->head;

      Function *fn = f->data.list->head->data.fn;
      List *args = f->data.list->tail;

      if (fn->is_builtin) {
        return fn->fn.builtin(args, env);
      }

      EVAL_call(fn, args, env, &ast, &env);
    }

    // gc_mark(ast, NULL);
    // gc_mark_list(list, NULL);
    // gc_mark_env(env, NULL);
    // gc(FALSE, FALSE);
  }
}

MalVal *READ(char *s)
{
  if (s == NULL) {
    return read_str();
  }
  else {
    return read_string(s);
  }
}

char *PRINT(const MalVal *val)
{
  return pr_str(val ? val : NIL, TRUE);
}

char *rep(ENV *repl_env, char *s)
{
  return PRINT(EVAL(READ(s), repl_env));
}

static void cleanup(void)
{
  unsigned count, size;

  env_release(repl_env);
  repl_env = NULL;
  gc(TRUE, TRUE);
  gc(TRUE, TRUE);
  gc(TRUE, TRUE);

  value_info(&count, &size);
  printf("\nValues remaining: %u (%u bytes)\n", count, size);

  heap_info(&count, &size);
  printf("Heap remainig: %u items (%u bytes)\n", count, size);
}

static void build_env(void)
{
  repl_env = env_create(NULL, NULL, NULL);

  for (struct ns *ns = core_ns; ns->name != NULL; ns++) {
    env_set(repl_env, ns->name, function_create_builtin(ns->fn));
  }

  env_set(repl_env, "nil", NIL);
  env_set(repl_env, "true", T);
  env_set(repl_env, "false", F);
}

/* init mal code, separated by \f */
char init[] = "\
(def! not (fn* (a) (if a false true)))\f\
(def! range\n\
  (fn* ([end] (range 0 end))\n\
       ([start end] (range start end 1))\n\
       ([start end step]\n\
        (if (< start end)\n\
          (cons start (range (+ step start) end step))))))\f\
(def! inc (fn* (a) (+ a 1)))\f\
(def! dec (fn* (a) (- a 1)))\f\
(def! reduce \n\
  (fn* ([f val xs] (if (empty? xs) val (reduce f (f val (first xs)) (rest xs))))\n\
       ([f xs] (reduce f (first xs) (rest xs)))))\f\
(def! map\n\
  (fn* ([f xs]\n\
        (if (not (empty? xs))\n\
          (cons (f (first xs)) (map f (rest xs)))))))";

int main(int argc, char **argv)
{
  atexit(cleanup);

  build_env();

  char *s = strtok(init, "\f");
  do {
    char *out = rep(repl_env, s);
    heap_free(out);
    s = strtok(NULL, "\f");
  } while(s);

  while (1) {
    char *s = rep(repl_env, NULL);
    if (!s) {
      exit(0);
    }
    fputs(s, stdout);
    fputc('\n', stdout);
    heap_free(s);
    // gc(FALSE, TRUE);
  }
}

int fputs(const char *s, FILE *stream)
{
  int c = 0;
  while (s && *s != '\0') {
    fputc(*s++, stream);
    c++;
  }
  return c;
}
