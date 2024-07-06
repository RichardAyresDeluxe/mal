#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "malval.h"
#include "mallist.h"
#include "gc.h"
#include "reader.h"
#include "printer.h"
#include "env.h"
#include "err.h"
#include "listsort.h"
#include "heap.h"

ENV *repl_env = NULL;

/** This can return NULL */
MalVal *EVAL(MalVal *ast, ENV *env);

static MalVal *plus(MalList *args, ENV *env)
{
  int result = 0;

  for (MalList *rover = args; rover; rover = rover->next) {
    assert(rover->value->type == TYPE_NUMBER);
    result += rover->value->data.number;
  }
  return malval_number(result);
}

static MalVal *minus(MalList *args, ENV *env)
{
  assert(args != NULL);

  int result = args->value->data.number;

  for (MalList *rover = args->next; rover; rover = rover->next) {
    assert(rover->value->type == TYPE_NUMBER);
    result -= rover->value->data.number;
  }
  return malval_number(result);
}

static MalVal *multiply(MalList *args, ENV *env)
{
  int result = 1;

  for (MalList *rover = args; rover; rover = rover->next) {
    assert(rover->value->type == TYPE_NUMBER);
    result *= rover->value->data.number;
  }
  return malval_number(result);
}

static MalVal *divide(MalList *args, ENV *env)
{
  assert(args != NULL && args->next != NULL);

  int result = args->value->data.number;

  for (MalList *rover = args->next; rover; rover = rover->next) {
    assert(rover->value->type == TYPE_NUMBER);
    result /= rover->value->data.number;
  }
  return malval_number(result);
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


static MalVal *eval_ast(MalVal *ast, ENV *env)
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
    MalList *evaluated = NULL;
    for (MalList *rover = ast->data.list; rover; rover = rover->next) {
      evaluated = cons(EVAL(rover->value, env), evaluated);
    }
    linked_list_reverse((void**)&evaluated);
    MalVal *value = malval_list(evaluated);
    list_release(evaluated);
    return value;
  }

  if (ast->type == TYPE_VECTOR)
  {
    MalList *evaluated = NULL;
    for (MalList *rover = ast->data.list; rover; rover = rover->next) {
      evaluated = cons(EVAL(rover->value, env), evaluated);
    }
    linked_list_reverse((void**)&evaluated);
    MalVal *value = malval_vector(evaluated);
    list_release(evaluated);
    return value;
  }

  if (ast->type == TYPE_MAP)
  {
    MalList *evaluated = NULL;
    unsigned i = 0;
    for (MalList *rover = ast->data.list; rover; rover = rover->next) {
      if ((i++ % 2) == 1)
        evaluated = cons(EVAL(rover->value, env), evaluated);
      else
        evaluated = cons(rover->value, evaluated);
    }
    linked_list_reverse((void**)&evaluated);
    MalVal *value = malval_map(evaluated);
    list_release(evaluated);
    return value;

  }

  return ast;
}

static MalVal *apply(MalVal *f, MalList *args, ENV *env)
{
  if (f->type != TYPE_FUNCTION)
    return NIL;
  return f->data.fn(args, env);
}

static MalVal *EVAL_def(MalList *list, ENV *env)
{
  if (list_count(list) != 3) {
    err_warning(ERR_ARGUMENT_MISMATCH, "def! requires two arguments");
    return NIL;
  }

  const char *name = list->next->value->data.string;

  MalVal *value = EVAL(list->next->next->value, env);
  if (!value) {
    warn_symbol_not_found(name);
    return NIL;
  }

  env_set(env, name, value);

  return value;
}

static MalVal *EVAL_let(MalList *list, ENV *env)
{
  if (list_count(list) != 3) {
    err_warning(ERR_ARGUMENT_MISMATCH, "let* requires two arguments");
    return NIL;
  }

  ENV *let = env_create(env);

  if (list->next->value->type != TYPE_LIST && list->next->value->type != TYPE_VECTOR) {
    err_warning(ERR_ARGUMENT_MISMATCH, "let* bindings argument must be list or vector");
    env_destroy(let, FALSE);
    return NIL;
  }

  MalList *bindings = list->next->value->data.list;
  if ((list_count(bindings) % 2) != 0) {
    err_warning(ERR_ARGUMENT_MISMATCH, "let* bindings must have even number of entries");
    env_destroy(let, FALSE);
    return NIL;
  }

  for (MalList *rover = bindings;
       rover && rover->next;
       rover = rover->next->next)
  {
    if (rover->value->type != TYPE_SYMBOL) {
      err_warning(ERR_ARGUMENT_MISMATCH, "can only bind to symbols");
      env_destroy(let, FALSE);
      return NIL;
    }
    env_set(let, rover->value->data.string, EVAL(rover->next->value, let));
  }

  MalVal *result = EVAL(list->next->next->value, let);

  env_destroy(let, FALSE);

  return result ? result : NIL;
}

MalVal *EVAL(MalVal *ast, ENV *env)
{
  if (ast->type != TYPE_LIST)
    return eval_ast(ast, env);
  if (ast->data.list == NULL)
    return ast; /* empty list */

  MalVal *result = NULL;
  MalList *list = ast->data.list;

  if (list->value->type == TYPE_SYMBOL
   && strcmp(list->value->data.string, "def!") == 0
  ) {
    result = EVAL_def(list, env);
  }
  else if (list->value->type == TYPE_SYMBOL
        && strcmp(list->value->data.string, "let*") == 0
  ) {
    result = EVAL_let(list, env);
  }
  else {
    MalVal *f = eval_ast(ast, env);
    if (!f)
        return NULL;
    if (VAL_IS_NIL(f))
      return NIL;
    assert(f->type == TYPE_LIST);
    assert(f->data.list != NULL);

    if (VAL_IS_NIL(f->data.list->value))
      return NULL;

    result = apply(f->data.list->value, f->data.list->next, env);
  }

  gc_mark_env(env, NULL);
  gc_mark(result, NULL);
  gc(FALSE);

  return result;
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
  gc(TRUE);
  gc(TRUE);
  gc(TRUE);

  value_info(&count, &size);

  printf("\nValues remaining: %u (%u bytes)\n", count, size);
}

static void build_env(void)
{
  struct {
    const char *name;
    FUNCTION *fn;
  } fns[] = {
    {"+", plus},
    {"-", minus},
    {"*", multiply},
    {"/", divide},
  };

  repl_env = env_create(NULL);

  for (unsigned i=0; i < sizeof(fns)/sizeof(fns[0]); i++) {
    env_set(repl_env, fns[i].name, malval_function(fns[i].fn));
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
    gc(FALSE);
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
