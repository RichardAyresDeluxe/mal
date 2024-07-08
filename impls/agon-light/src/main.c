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

ENV *repl_env = NULL;

/** This can return NULL */
MalVal *EVAL(MalVal *ast, ENV *env);

static MalVal *plus(List *args, ENV *env)
{
  int result = 0;

  for (List *rover = args; rover; rover = rover->tail) {
    assert(rover->head->type == TYPE_NUMBER);
    result += rover->head->data.number;
  }
  return malval_number(result);
}

static MalVal *minus(List *args, ENV *env)
{
  assert(args != NULL);

  int result = args->head->data.number;

  for (List *rover = args->tail; rover; rover = rover->tail) {
    assert(rover->head->type == TYPE_NUMBER);
    result -= rover->head->data.number;
  }
  return malval_number(result);
}

static MalVal *multiply(List *args, ENV *env)
{
  int result = 1;

  for (List *rover = args; rover; rover = rover->tail) {
    assert(rover->head->type == TYPE_NUMBER);
    result *= rover->head->data.number;
  }
  return malval_number(result);
}

static MalVal *divide(List *args, ENV *env)
{
  assert(args != NULL && args->tail != NULL);

  int result = args->head->data.number;

  for (List *rover = args->tail; rover; rover = rover->tail) {
    assert(rover->head->type == TYPE_NUMBER);
    result /= rover->head->data.number;
  }
  return malval_number(result);
}

static MalVal *lessthan(List *args, ENV *env)
{
  assert(args != NULL && args->tail != NULL);

  assert(VAL_TYPE(args->head) == TYPE_NUMBER && VAL_TYPE(args->tail->head) == TYPE_NUMBER);

  if (args->head->data.number < args->tail->head->data.number)
    return T;
  return F;
}

static MalVal *builtin_apply(List *args, ENV *env)
{
  MalVal *f = args->head;

  if (VAL_TYPE(f) != TYPE_FUNCTION) {
    err_warning(ERR_ARGUMENT_MISMATCH, "apply: first argument must be a function");
    return NIL;
  }

  args = args->tail;

  linked_list_reverse((void*)&args);

  if (!args || VAL_TYPE(args->head) != TYPE_LIST) {
    err_warning(ERR_ARGUMENT_MISMATCH, "apply: last argument must be a list");
    return NIL;
  }
  
  List *all = args->head->data.list;

  for (List *rover = args->tail; rover; rover = rover->tail)
    all = cons(rover->head, all);

  MalVal *rv = apply(f->data.fn, all);
  list_release(all);
  return rv;
}

static MalVal *builtin_list(List *args, ENV *env)
{
  return malval_list(list_acquire(args));
}

static MalVal *builtin_cons(List *args, ENV *env)
{
  assert(args != NULL && args->tail != NULL);

  List *list = NULL;
  
  if (!VAL_IS_NIL(args->tail->head))
    list = args->tail->head->data.list;

  list = cons(args->head, list);

  return malval_list(list);
}

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


MalVal *eval_ast(MalVal *ast, ENV *env)
{
  if (ast->type == TYPE_SYMBOL)
  {
    MalVal *value = env_get(env, ast->data.string);
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

static MalVal *EVAL_let(List *list, ENV *env)
{
  if (list_count(list) != 2) {
    err_warning(ERR_ARGUMENT_MISMATCH, "let* requires two arguments");
    return NIL;
  }

  ENV *let = env_create(env, NULL, NULL);

  if (list->head->type != TYPE_LIST && list->head->type != TYPE_VECTOR) {
    err_warning(ERR_ARGUMENT_MISMATCH, "let* bindings argument must be list or vector");
    env_destroy(let, FALSE);
    return NIL;
  }

  List *bindings = list->head->data.list;
  if ((list_count(bindings) % 2) != 0) {
    err_warning(ERR_ARGUMENT_MISMATCH, "let* bindings must have even number of entries");
    env_destroy(let, FALSE);
    return NIL;
  }

  for (List *rover = bindings;
       rover && rover->tail;
       rover = rover->tail->tail)
  {
    if (rover->head->type != TYPE_SYMBOL) {
      err_warning(ERR_ARGUMENT_MISMATCH, "can only bind to symbols");
      env_destroy(let, FALSE);
      return NIL;
    }
    env_set(let, rover->head->data.string, EVAL(rover->tail->head, let));
  }

  MalVal *result = EVAL(list->tail->head, let);

  env_destroy(let, FALSE);

  return result ? result : NIL;
}

static MalVal *EVAL_fn_star(List *list, ENV *env)
{
  return function_create(list, env);
}

MalVal *EVAL(MalVal *ast, ENV *env)
{
  if (ast->type != TYPE_LIST)
    return eval_ast(ast, env);
  if (ast->data.list == NULL)
    return ast; /* empty list */

  MalVal *result = NULL;
  List *list = ast->data.list;

  MalVal *head = list->head;
  List *tail = list->tail;

  if (head->type == TYPE_SYMBOL
   && strcmp(head->data.string, "def!") == 0
  ) {
    result = EVAL_def(tail, env);
  }
  else if (head->type == TYPE_SYMBOL
        && strcmp(head->data.string, "let*") == 0
  ) {
    result = EVAL_let(tail, env);
  }
  else if (head->type == TYPE_SYMBOL
        && strcmp(head->data.string, "do") == 0
  ) {
    MalVal *val = NULL;
    for (List *rover = tail; rover; rover = rover->tail) {
      malval_reset_temp(val, NULL);
      val = eval_ast(rover->head, env);
    }
    return val ? val : NIL;
  }
  else if (head->type == TYPE_SYMBOL
        && strcmp(head->data.string, "fn*") == 0
  ) {
    return EVAL_fn_star(tail, env);
  }
  else if (head->type == TYPE_SYMBOL
        && strcmp(head->data.string, "if") == 0
  ) {
    if (!tail || !tail->tail) {
      err_warning(ERR_ARGUMENT_MISMATCH, "need at least 2 arguments to if");
      return NIL;
    }
    MalVal *val = EVAL(tail->head, env);
    if (VAL_IS_NIL(val) || VAL_IS_FALSE(val)) {
      return tail->tail->tail ? 
        EVAL(tail->tail->tail->head, env) : NIL;
    }
    else {
      return EVAL(tail->tail->head, env);
    }
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

    if (VAL_TYPE(f->data.list->head) != TYPE_FUNCTION) {
      err_warning(ERR_ARGUMENT_MISMATCH, "not a function");
      return NIL;
    }

    result = apply(f->data.list->head->data.fn, f->data.list->tail);
  }

  // gc_mark(ast, NULL);
  // gc_mark_list(list, NULL);
  // gc_mark_env(env, NULL);
  // gc_mark(result, NULL);
  // gc(FALSE, FALSE);

  return result; // ? result : NIL;
}

MalVal *READ(void)
{
  return read_str();
}

const char *PRINT(const MalVal *val)
{
  return pr_str(val ? val : NIL, TRUE);
}

const char *rep(ENV *repl_env)
{
  return PRINT(EVAL(READ(), repl_env));
}

static void cleanup(void)
{
  unsigned count, size;

  env_destroy(repl_env, TRUE);
  repl_env = NULL;
  gc(TRUE, TRUE);
  gc(TRUE, TRUE);
  gc(TRUE, TRUE);

  value_info(&count, &size);

  printf("\nValues remaining: %u (%u bytes)\n", count, size);
}

static void build_env(void)
{
  struct {
    const char *name;
    builtin_fn *fn;
  } fns[] = {
    {"+", plus},
    {"-", minus},
    {"*", multiply},
    {"/", divide},
    {"<", lessthan},
    {"apply", builtin_apply},
    {"cons", builtin_cons},
    {"list", builtin_list},
  };

  repl_env = env_create(NULL, NULL, NULL);

  for (unsigned i=0; i < sizeof(fns)/sizeof(fns[0]); i++) {
    env_set(repl_env, fns[i].name, function_create_builtin(fns[i].fn));
  }

  env_set(repl_env, "nil", NIL);
  env_set(repl_env, "true", T);
  env_set(repl_env, "false", F);
}
  
int main(int argc, char **argv)
{
  atexit(cleanup);

  build_env();

  while (1) {
    const char *s = rep(repl_env);
    if (!s) {
      exit(0);
    }
    fputs(s, stdout);
    fputc('\n', stdout);
    gc(FALSE, TRUE);
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
