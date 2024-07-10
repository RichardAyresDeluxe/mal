#include "malval.h"
#include "list.h"
#include "env.h"
#include "err.h"
#include "listsort.h"
#include "function.h"
#include "ns.h"
#include "core.h"
#include "heap.h"
#include "printer.h"
#include "str.h"
#include "gc.h"
#include "reader.h"
#include "eval.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

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

static MalVal *lessthan_or_equal(List *args, ENV *env)
{
  assert(args != NULL && args->tail != NULL);

  assert(VAL_TYPE(args->head) == TYPE_NUMBER && VAL_TYPE(args->tail->head) == TYPE_NUMBER);

  if (args->head->data.number <= args->tail->head->data.number)
    return T;
  return F;
}

static MalVal *morethan(List *args, ENV *env)
{
  assert(args != NULL && args->tail != NULL);

  assert(VAL_TYPE(args->head) == TYPE_NUMBER && VAL_TYPE(args->tail->head) == TYPE_NUMBER);

  if (args->head->data.number > args->tail->head->data.number)
    return T;
  return F;
}

static MalVal *morethan_or_equal(List *args, ENV *env)
{
  assert(args != NULL && args->tail != NULL);

  assert(VAL_TYPE(args->head) == TYPE_NUMBER && VAL_TYPE(args->tail->head) == TYPE_NUMBER);

  if (args->head->data.number >= args->tail->head->data.number)
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
  
  List *all = list_acquire(args->head->data.list);

  for (List *rover = args->tail; rover; rover = rover->tail)
    all = cons_weak(rover->head, all);

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

static MalVal *builtin_is_list(List *args, ENV *env)
{
  assert(args != NULL);

  return VAL_TYPE(args->head) == TYPE_LIST ? T : F;
}

static MalVal *builtin_is_empty(List *args, ENV *env)
{
  assert(args != NULL);

  if ((VAL_TYPE(args->head) == TYPE_LIST && list_is_empty(args->head->data.list))
   || (VAL_TYPE(args->head) == TYPE_VECTOR && list_is_empty(args->head->data.vec))
   || (VAL_TYPE(args->head) == TYPE_MAP && list_is_empty(args->head->data.map))
   || VAL_IS_NIL(args->head)
  ) {
    return T;
  }
  return F;
}

static MalVal *builtin_count(List *args, ENV *env)
{
  assert(args != NULL);

  if (VAL_IS_NIL(args->head))
    return malval_number(0);

  switch (VAL_TYPE(args->head)) {
    case TYPE_LIST:
      return malval_number(list_count(args->head->data.list));
    case TYPE_VECTOR:
      return malval_number(list_count(args->head->data.vec));
    case TYPE_MAP:
      return malval_number(list_count(args->head->data.map) / 2);
  }

  err_warning(ERR_ARGUMENT_MISMATCH, "cannot count object");
  return NIL;
}

static MalVal *builtin_equals(List *args, ENV *env)
{
  assert(args != NULL && args->tail != NULL);

  return malval_equals(args->head, args->tail->head) ? T : F;
}

struct pr_struct {
  char *s;
  char *sep;
  bool readable;
};

static void pr_str_helper(MalVal *val, void *_p)
{
  struct pr_struct *pr = _p;
  if (pr->sep && pr->sep[0] != '\0')
    catstr(&pr->s, pr->sep);
  char *s = pr_str(val, pr->readable);
  catstr(&pr->s, s);
  heap_free(s);
}

static MalVal *builtin_pr_str(List *args, ENV *env)
{
  struct pr_struct pr = { NULL, " ", TRUE };

  list_foreach(args, pr_str_helper, &pr);

  MalVal *rv = malval_string(pr.s ? &pr.s[1] : "");
  heap_free(pr.s);
  return rv;
}

static MalVal *builtin_str(List *args, ENV *env)
{
  struct pr_struct pr = { NULL, NULL, FALSE };

  list_foreach(args, pr_str_helper, &pr);

  MalVal *rv = malval_string(pr.s ? pr.s : "");
  heap_free(pr.s);
  return rv;
}

static MalVal *builtin_prn(List *args, ENV *env)
{
  struct pr_struct pr = { NULL, " ", TRUE };

  list_foreach(args, pr_str_helper, &pr);

  puts(pr.s ? &pr.s[1] : "");
  heap_free(pr.s);

  return NIL;
}

static MalVal *builtin_println(List *args, ENV *env)
{
  struct pr_struct pr = { NULL, " ", FALSE };

  list_foreach(args, pr_str_helper, &pr);

  puts(pr.s ? &pr.s[1] : "");
  heap_free(pr.s);

  return NIL;
}

static MalVal *builtin_first(List *args, ENV *env)
{
  assert(args != NULL);
  
  MalVal *val = args->head;

  switch(VAL_TYPE(val)) {
    case TYPE_LIST:
      return list_is_empty(val->data.list) ? NIL : val->data.list->head;
    case TYPE_VECTOR:
      return list_is_empty(val->data.vec) ? NIL : val->data.vec->head;
  }
  err_warning(ERR_ARGUMENT_MISMATCH, "cannot take first of non-container");
  return NIL;
}

static MalVal *builtin_rest(List *args, ENV *env)
{
  assert(args != NULL);

  MalVal *val = args->head;

  switch(VAL_TYPE(val)) {
    case TYPE_LIST:
      return malval_list(list_is_empty(val->data.list) ? NULL : val->data.list->tail);
    case TYPE_VECTOR:
      return malval_vector(list_is_empty(val->data.vec) ? NULL : val->data.vec->tail);
  }
  err_warning(ERR_ARGUMENT_MISMATCH, "cannot take rest of non-container");
  return NIL;
}

static MalVal *builtin_gc(List *args, ENV *env)
{
  gc_mark_env(env, NULL);
  gc_mark_list(args, NULL);
  gc(TRUE, TRUE);
  return NIL;
}

static MalVal *builtin_read_string(List *args, ENV *env)
{
  if (!args || VAL_TYPE(args->head) != TYPE_STRING) {
    err_warning(ERR_ARGUMENT_MISMATCH, "not a string input");
    return NIL;
  }
  return read_string(args->head->data.string);
}

static MalVal *builtin_slurp(List *args, ENV *env)
{
  if (!args || VAL_TYPE(args->head) != TYPE_STRING) {
    err_warning(ERR_ARGUMENT_MISMATCH, "not a file name input");
    return NIL;
  }

  /* TODO: AGON VERSION */
  char *s = NULL;
  FILE *fh = fopen(args->head->data.string, "r");
  if (!fh) {
    err_warning(ERR_FILE_ERROR, "cannot open file");
    return NIL;
  }
  char buf[80];
  while (!feof(fh)) {
    buf[0] = '\0';
    fgets(buf, sizeof(buf)-1, fh);
    catstr(&s, buf);
  }
  fclose(fh);

  MalVal *rv = malval_string(s);
  heap_free(s);
  return rv;
}

struct ns core_ns[] = {
  {"+", plus},
  {"-", minus},
  {"*", multiply},
  {"/", divide},
  {"=", builtin_equals},
  {"<", lessthan},
  {"<=", lessthan_or_equal},
  {">", morethan},
  {">=", morethan_or_equal},
  {"apply", builtin_apply},
  {"cons", builtin_cons},
  {"first", builtin_first},
  {"rest", builtin_rest},
  {"list", builtin_list},
  {"list?", builtin_is_list},
  {"empty?", builtin_is_empty},
  {"count", builtin_count},
  {"pr-str", builtin_pr_str},
  {"str", builtin_str},
  {"prn", builtin_prn},
  {"println", builtin_println},
  {"gc", builtin_gc},
  {"read-string", builtin_read_string},
  {"slurp", builtin_slurp},
  {NULL, NULL},
};

