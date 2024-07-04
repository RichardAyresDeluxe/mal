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
#include "heap.h"
#include "err.h"
#include "listsort.h"

MalVal *EVAL(MalVal *ast, ENV *env);

ENV *repl_env = NULL;

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

static MalVal *eval_ast(MalVal *ast, ENV *env)
{
  if (ast->type == TYPE_SYMBOL)
  {
    for (ENV *env = repl_env; env && env->name != NULL; env++) {
      if (strcmp(env->name, ast->data.string) == 0)
        return env->value;
    }
    err_warning(ERR_ARGUMENT_MISMATCH, "Unknown symbol", env->name);
    return NIL;
  }

  if (ast->type == TYPE_LIST)
  {
    MalList *evaluated = NULL;
    for (MalList *rover = ast->data.list; rover; rover = rover->next) {
      evaluated = cons(EVAL(rover->value, env), evaluated);
    }
    linked_list_reverse((void**)&evaluated);
    MalVal *value = malval_list(evaluated);
    mallist_release(evaluated);
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
    mallist_release(evaluated);
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
    mallist_release(evaluated);
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

MalVal *EVAL(MalVal *ast, ENV *env)
{
  if (ast->type != TYPE_LIST)
    return eval_ast(ast, env);
  if (ast->data.list == NULL)
    return ast;

  MalVal *f = eval_ast(ast, env);
  assert(f->type == TYPE_LIST);
  assert(f->data.list != NULL);

  return apply(f->data.list->value, f->data.list->next, env);
}

MalVal *READ(void)
{
  return read_str();
}

const char *PRINT(const MalVal *val)
{
  return pr_str(val, TRUE);
}

const char *rep(ENV *repl_env)
{
  return PRINT(EVAL(READ(), repl_env));
}

static void cleanup(void)
{
  unsigned count, size;

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

  int n = sizeof(fns)/sizeof(fns[0]);
  repl_env = heap_malloc(sizeof(ENV) * (n + 1));

  for (int i=0; i < n; i++) {
    repl_env[i].name = fns[i].name;
    repl_env[i].value = malval_function(fns[i].fn);
  }
  repl_env[n].name = NULL;
  repl_env[n].value = NULL;
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
    gc_mark_env(repl_env, NULL);
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
