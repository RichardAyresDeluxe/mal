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
#include "function.h"
#include "str.h"


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
      evaluated = cons_weak(EVAL(rover->head, env), evaluated);
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
      evaluated = cons_weak(EVAL(rover->head, env), evaluated);
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
      if ((i++ % 2) == 0) {
        /* even - a key, don't evaluate */
        evaluated = cons_weak(rover->head, evaluated);
      }
      else {
        /* odd - a value, evaluate */
        evaluated = cons_weak(EVAL(rover->head, env), evaluated);
      }
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
  env_release(*envout);
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
  env_release(*envout);
  *envout = new_env;
}

static MalVal *EVAL_quasiquote(MalVal *ast);

static MalVal *EVAL_quasiquote_list(List *elt)
{
  List *result = NULL;
  for (; elt; elt = elt->tail) {
    if (VAL_TYPE(elt->head) == TYPE_LIST
     && !list_is_empty(elt->head->data.list)
     && VAL_TYPE(elt->head->data.list->head) == TYPE_SYMBOL
     && strcmp(elt->head->data.list->head->data.string, "splice-unquote") == 0
    ) {
      result = cons_weak(malval_symbol("concat"),
                         cons_weak(elt->head->data.list->tail->head,
                                   cons_weak(malval_list(result), NULL)));
    }
    else {
      result = cons_weak(malval_symbol("cons"),
                         cons_weak(EVAL_quasiquote(elt->head),
                                   cons_weak(malval_list(result), NULL)));
    }
  }

  return malval_list(result);
}

static MalVal *EVAL_quasiquote(MalVal *ast)
{
  if (VAL_TYPE(ast) == TYPE_LIST
   && !list_is_empty(ast->data.list)
   && VAL_TYPE(ast->data.list->head) == TYPE_SYMBOL
   && strcmp(ast->data.list->head->data.string, "unquote") == 0)
  {
    /*   - If `ast` is a list starting with the "unquote" symbol, return its
     *       second element. */
    return ast->data.list->tail->head;
  }

  if (VAL_TYPE(ast) == TYPE_LIST) {
    linked_list_reverse((void**)&ast->data.list);
    return EVAL_quasiquote_list(ast->data.list);
  }

  if (VAL_TYPE(ast) == TYPE_VECTOR) {
    List *list = list_from_container(ast);
    linked_list_reverse((void**)&list);
    List * result = cons_weak(malval_symbol("vec"),
                              cons_weak(EVAL_quasiquote_list(list),
                                        NULL));
    return malval_list(result);
  }

  if (VAL_TYPE(ast) == TYPE_MAP || VAL_TYPE(ast) == TYPE_SYMBOL) {
    return malval_list(cons_weak(malval_symbol("quote"),
                                 cons_weak(ast, NULL)));
  }

  return ast;
}

MalVal *EVAL(MalVal *ast, ENV *env)
{
  env_acquire(env);

  while (TRUE)
  {
    if (ast->type != TYPE_LIST) {
      MalVal *rv = eval_ast(ast, env);
      env_release(env);
      return rv;
    }
    if (ast->data.list == NULL) {
      env_release(env);
      return ast; /* empty list */
    }

    List *list = ast->data.list;

    MalVal *head = list->head;
    List *tail = list->tail;

    if (head->type == TYPE_SYMBOL) {
      /* check for special forms */
      const char *symbol = head->data.string;
      if (symbol[0] == 'd') {
        if (strcmp(symbol, "def!") == 0) {
          MalVal *rv = EVAL_def(tail, env);
          env_release(env);
          return rv;
        }
        if (strcmp(symbol, "do") == 0) {
          EVAL_do(tail, env, &ast);
          continue;
        }
      }
      if (symbol[0] == 'q') {
        if (strcmp(symbol, "quote") == 0) {
          env_release(env);
          return tail->head;
        }
        if (strcmp(symbol, "quasiquote") == 0) {
          ast = EVAL_quasiquote(tail->head);
          continue;
        }
        if (strcmp(symbol, "quasiquoteexpand") == 0) {
          return EVAL_quasiquote(tail->head);
        }
      }
      if (strcmp(symbol, "let*") == 0) {
        EVAL_let(tail, env, &ast, &env);
        continue;
      }
      if (strcmp(symbol, "fn*") == 0) {
        return EVAL_fn_star(tail, env);
      }
      if (strcmp(symbol, "if") == 0) {
        EVAL_if(tail, env, &ast);
        continue;
      }
    }

    /* otherwise treat as if it's a function call */

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
      MalVal *rv = fn->fn.builtin(args, env);
      env_release(env);
      return rv;
    }

    EVAL_call(fn, args, env, &ast, &env);

    gc_mark(ast, NULL);
    gc_mark_list(list, NULL);
    gc_mark_env(env, NULL);
    gc(FALSE, FALSE);
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

char *PRINT(MalVal *val)
{
  return pr_str(val ? val : NIL, TRUE);
}

char *rep(ENV *repl_env, char *s)
{
  return PRINT(EVAL(READ(s), repl_env));
}

static void cleanup(void)
{
  env_flush(repl_env);

  gc(TRUE, TRUE);
  gc(TRUE, TRUE);
  gc(TRUE, TRUE);

  env_release(repl_env);
  repl_env = NULL;

  unsigned count, size;
  value_info(&count, &size);
  fprintf(stderr, "Values remaining: %u (%u bytes)\n", count, size);

  heap_info(&count, &size);
  fprintf(stderr, "Heap remainig: %u items (%u bytes)\n", count, size);
}

static MalVal *builtin_eval(List *args, ENV *ignored)
{
  if (!args) {
    err_warning(ERR_ARGUMENT_MISMATCH, "need one argument");
    return NIL;
  }

  return EVAL(args->head, repl_env);
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
  env_set(repl_env, "eval", function_create_builtin(builtin_eval));
  env_set(repl_env, "*ARGV*", malval_list(NULL));
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
(def! load-file\n\
  (fn* [f] (eval (read-string (str \"(do \" (slurp f) \"\n nil)\")))))";

int main(int argc, char **argv)
{
  /* Largely to keep valgrind happy */
  atexit(cleanup);

  build_env();

  char *s = strtok(init, "\f");
  do {
    char *out = rep(repl_env, s);
    heap_free(out);
    s = strtok(NULL, "\f");
  } while(s);

  int arg = 1;

  while (arg < argc) {
    if (argv[arg][0] == '\0') {
      arg++;
      continue;
    }

    List *args = NULL;

    char *input = strdup("(load-file \"");
    catstr(&input, argv[arg++]);
    catstr(&input, "\")");

    for (; arg < argc; arg++)
      args = cons_weak(malval_string(argv[arg]), args);

    linked_list_reverse((void**)&args);
    env_set(repl_env, "*ARGV*", malval_list(args));
    list_release(args);

    char *s = rep(repl_env, input);
    heap_free(input);
    heap_free(s);
    exit(0);
  }

  while (1) {
    char *s = rep(repl_env, NULL);
    if (!s) {
      exit(0);
    }
    puts(s);
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
