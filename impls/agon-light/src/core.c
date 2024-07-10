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

#define ARGS_MAX MAX_ARITY

bool builtins_all_numeric(List *list)
{
  for (List *arg = list; arg; arg = arg->tail) {
    if (!VAL_IS_NUMERIC(arg->head)) {
      err_warning(ERR_ARGUMENT_MISMATCH, "not a numeric argument");
      return FALSE;
    }
  }
  return TRUE;
}

bool builtins_args_check(
  List *args,
  unsigned count_lo,
  unsigned count_hi,
  MalType *types
) {
  unsigned count = list_count(args);

  if (count < count_lo || count > count_hi) {
    if (count_lo == count_hi)
      err_warning(ERR_ARGUMENT_MISMATCH, "function takes %u arguments", count_lo);
    else if (count_hi == ARGS_MAX)
      err_warning(ERR_ARGUMENT_MISMATCH, "function takes at least %u arguments", count_lo);
    else
      err_warning(ERR_ARGUMENT_MISMATCH, "function takes between %u and %u arguments", count_lo, count_hi);
    return FALSE;
  }

  if (!types)
    return TRUE;

  unsigned i = 0;
  List *arg = args;

  while (i < count && types[i] && arg)
  {
    if (((types[i] & METATYPE_MASK) == types[i] && (VAL_METATYPE(arg->head) != types[i]))
     || ((types[i] & METATYPE_MASK) != types[i] && (VAL_TYPE(arg->head) != types[i])))
    {
      err_warning(
        ERR_INVALID_OPERATION,
        "require argument of type 0x%X in position %u",
        types[i],
        (1+i)
      );
      return FALSE;
    }

    i++;
    arg = arg->tail;
  }

  return TRUE;
}

static MalVal *plus(List *args, ENV *env)
{
  if (!builtins_all_numeric(args))
    return NIL;

  int result = 0;

  for (List *rover = args; rover; rover = rover->tail) {
    result += rover->head->data.number;
  }
  return malval_number(result);
}

static MalVal *minus(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, ARGS_MAX, NULL)
   || !builtins_all_numeric(args))
  {
    return NIL;
  }

  int result = args->head->data.number;

  for (List *rover = args->tail; rover; rover = rover->tail) {
    result -= rover->head->data.number;
  }
  return malval_number(result);
}

static MalVal *multiply(List *args, ENV *env)
{
  if (!builtins_all_numeric(args))
    return NIL;

  int result = 1;

  for (List *rover = args; rover; rover = rover->tail) {
    result *= rover->head->data.number;
  }
  return malval_number(result);
}

static MalVal *divide(List *args, ENV *env)
{
  if (!builtins_args_check(args, 2, ARGS_MAX, NULL)
   || !builtins_all_numeric(args))
  {
    return NIL;
  }

  int result = args->head->data.number;

  for (List *rover = args->tail; rover; rover = rover->tail) {
    result /= rover->head->data.number;
  }
  return malval_number(result);
}

static MalVal *lessthan(List *args, ENV *env)
{
  if (!builtins_args_check(args, 2, ARGS_MAX, NULL)
   || !builtins_all_numeric(args))
  {
    return NIL;
  }

  return args->head->data.number < args->tail->head->data.number ? T : F;
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
  if (!builtins_args_check(args, 2, ARGS_MAX, NULL)
   || !builtins_all_numeric(args))
  {
    return NIL;
  }

  return args->head->data.number > args->tail->head->data.number ? T : F;
}

static MalVal *morethan_or_equal(List *args, ENV *env)
{
  if (!builtins_args_check(args, 2, ARGS_MAX, NULL)
   || !builtins_all_numeric(args))
  {
    return NIL;
  }

  return args->head->data.number >= args->tail->head->data.number ? T : F;
}

static MalType types_apply[] = {TYPE_FUNCTION, 0};
static MalVal *builtin_apply(List *args, ENV *env)
{
  if (!builtins_args_check(args, 2, ARGS_MAX, types_apply))
    return NIL;

  MalVal *f = args->head;

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
  if (!builtins_args_check(args, 2, 2, NULL))
    return NIL;

  List *list = NULL;
  
  if (!VAL_IS_NIL(args->tail->head) && !VAL_IS_CONTAINER(args->tail->head)) {
    err_warning(ERR_ARGUMENT_MISMATCH, "second argument must be a container or nil");
    return NIL;
  }

  switch(VAL_TYPE(args->tail->head)) {
    case TYPE_LIST:
      list = args->tail->head->data.list;
      break;
    case TYPE_VECTOR:
      list = args->tail->head->data.vec;
      break;
    case TYPE_MAP:
      err_warning(ERR_ARGUMENT_MISMATCH, "cannot cons to maps yet");
      return NIL;
    case TYPE_NIL:
      list = NULL;
      break;
    default:
      err_warning(ERR_ARGUMENT_MISMATCH, "cannot cons to object");
      break;
  }

  list = cons(args->head, list);

  return malval_list(list);
}

static MalVal *builtin_is_list(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return VAL_TYPE(args->head) == TYPE_LIST ? T : F;
}

static MalVal *builtin_is_empty(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  if (VAL_IS_NIL(args->head))
    return T;

  if (!VAL_IS_CONTAINER(args->head)) {
    err_warning(ERR_ARGUMENT_MISMATCH, "invalid type");
    return NIL;
  }

  if ((VAL_TYPE(args->head) == TYPE_LIST && list_is_empty(args->head->data.list))
   || (VAL_TYPE(args->head) == TYPE_VECTOR && list_is_empty(args->head->data.vec))
   || (VAL_TYPE(args->head) == TYPE_MAP && list_is_empty(args->head->data.map))
  ) {
    return T;
  }
  return F;
}

static MalVal *builtin_count(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

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
  if (!builtins_args_check(args, 2, ARGS_MAX, NULL))
    return NIL;

  MalVal *head = args->head;

  for (List *rover = args->tail; rover; rover = rover->tail) {
    if (!malval_equals(head, rover->head))
      return F;
  }

  return T;
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

static MalType types_container[] = {METATYPE_CONTAINER, 0};
static MalVal *builtin_first(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, ARGS_MAX, types_container))
    return NIL;

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
  if (!builtins_args_check(args, 1, ARGS_MAX, types_container))
    return NIL;

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
  if (!builtins_args_check(args, 0, 0, NULL))
    return NIL;

  gc_mark_env(env, NULL);
  gc_mark_list(args, NULL);
  gc(TRUE, TRUE);

  return NIL;
}

static MalType types_string[] = {TYPE_STRING, 0};
static MalVal *builtin_read_string(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, types_string))
    return NIL;

  return read_string(args->head->data.string);
}

static MalVal *builtin_slurp(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, types_string))
    return NIL;

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

static MalVal *builtin_atom(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return malval_atom(args->head);
}

static MalVal *builtin_is_atom(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return VAL_TYPE(args->head) == TYPE_ATOM ? T : F;
}

static MalType types_atom[] = {TYPE_ATOM, 0};
static MalVal *builtin_deref(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, types_atom))
    return NIL;

  return args->head->data.atom;
}

static MalVal *builtin_reset(List *args, ENV *env)
{
  if (!builtins_args_check(args, 2, 2, types_atom))
    return NIL;

  args->head->data.atom = args->tail->head;

  return args->tail->head;
}

static MalType types_swap[] = {TYPE_ATOM, TYPE_FUNCTION, 0};
static MalVal *builtin_swap(List *args, ENV *env)
{
  if (!builtins_args_check(args, 2, ARGS_MAX, types_swap))
    return NIL;

  MalVal *atom = args->head;
  MalVal *func = args->tail->head;
  List *fargs = cons(atom->data.atom, args->tail->tail);

  atom->data.atom = apply(func->data.fn, fargs);
  list_release(fargs);

  return atom->data.atom;
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
  /* atom stuff: */
  {"atom", builtin_atom},
  {"atom?", builtin_is_atom},
  {"deref", builtin_deref},
  {"reset!", builtin_reset},
  {"swap!", builtin_swap},
  {NULL, NULL},
};

