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
#include "itoa.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <alloca.h>

#ifndef AGON_LIGHT
#include <sys/time.h>
#endif

#define ARGS_MAX MAX_ARITY

static MalType types_string[] = {TYPE_STRING, 0};
static MalType types_container[] = {METATYPE_CONTAINER, 0};
static MalType types_containers[] = {
  METATYPE_CONTAINER, METATYPE_CONTAINER, METATYPE_CONTAINER,
  METATYPE_CONTAINER, METATYPE_CONTAINER, METATYPE_CONTAINER,
  METATYPE_CONTAINER, METATYPE_CONTAINER, METATYPE_CONTAINER,
  METATYPE_CONTAINER, METATYPE_CONTAINER, METATYPE_CONTAINER,
  METATYPE_CONTAINER, METATYPE_CONTAINER, METATYPE_CONTAINER, 0};
static MalType types_numbers[] = {TYPE_NUMBER, TYPE_NUMBER, 0};

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
  if (!builtins_args_check(args, 2, ARGS_MAX, NULL)
   || !builtins_all_numeric(args))
  {
    return NIL;
  }

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

static MalVal *builtin_mod(List *args, ENV *env)
{
  if (!builtins_args_check(args, 2, 2, types_numbers))
    return NIL;

  return malval_number(VAL_NUMBER(args->head) % VAL_NUMBER(args->tail->head));
}

static MalType types_apply[] = {TYPE_FUNCTION, 0};
static MalVal *builtin_apply(List *args, ENV *env)
{
  if (!builtins_args_check(args, 2, ARGS_MAX, types_apply))
    return NIL;

  MalVal *f = args->head;

  args = args->tail;

  linked_list_reverse((void*)&args);

  List *all;
  if (args && VAL_TYPE(args->head) == TYPE_LIST) {
    all = list_acquire(VAL_LIST(args->head));
  }
  else if (args && VAL_TYPE(args->head) == TYPE_VECTOR) {
    all = list_from_container(args->head);
  }
  else {
    err_warning(ERR_ARGUMENT_MISMATCH, "apply: last argument must be a list");
    return NIL;
  }

  for (List *rover = args->tail; rover; rover = rover->tail)
    all = cons_weak(rover->head, all);

  MalVal *rv = apply(VAL_FUNCTION(f), all);
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

  if (!VAL_IS_NIL(args->tail->head) && !VAL_IS_CONTAINER(args->tail->head)) {
    err_warning(ERR_ARGUMENT_MISMATCH, "second argument must be a container or nil");
    return NIL;
  }

  List *list = NULL;
  switch(VAL_TYPE(args->tail->head)) {
    case TYPE_LIST:
      list = VAL_LIST(args->tail->head);
      break;
    case TYPE_VECTOR:
      list = VAL_VEC(args->tail->head);
      break;
    case TYPE_MAP:
      err_warning(ERR_ARGUMENT_MISMATCH, "cannot cons to maps yet");
      return NIL;
  }

  list = cons(args->head, list);

  MalVal *rv = malval_list(list);
  list_release(list);
  return rv;
}

static MalVal *builtin_concat(List *args, ENV *env)
{
  if (!builtins_args_check(args, 0, ARGS_MAX, types_containers))
    return NIL;

  List *result = NULL;
  while (args) {
    List *list = list_from_container(args->head);
    List *tmp = list_concat(result, list);
    list_release(result);
    list_release(list);
    result = tmp;

    args = args->tail;
  }

  MalVal *rv = malval_list(result);
  list_release(result);
  return rv;
}

static MalVal *builtin_vec(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, types_containers))
    return NIL;

  if (VAL_TYPE(args->head) == TYPE_VECTOR)
    return args->head;

  if (VAL_TYPE(args->head) == TYPE_LIST)
    return malval_vector(VAL_LIST(args->head));

  err_warning(ERR_NOT_IMPLEMENTED, "cannot turn container into a vector");
  return NIL;
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

  if ((VAL_TYPE(args->head) == TYPE_LIST && list_is_empty(VAL_LIST(args->head)))
   || (VAL_TYPE(args->head) == TYPE_VECTOR && list_is_empty(VAL_VEC(args->head)))
   || (VAL_TYPE(args->head) == TYPE_MAP && list_is_empty(VAL_MAP(args->head)))
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
      return malval_number(list_count(VAL_LIST(args->head)));
    case TYPE_VECTOR:
      return malval_number(list_count(VAL_VEC(args->head)));
    case TYPE_MAP:
      return malval_number(list_count(VAL_MAP(args->head)) / 2);
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

static MalVal *builtin_first(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, ARGS_MAX, types_containers))
    return NIL;

  MalVal *val = args->head;

  switch(VAL_TYPE(val)) {
    case TYPE_LIST:
      return list_is_empty(VAL_LIST(val)) ? NIL : VAL_LIST(val)->head;
    case TYPE_VECTOR:
      return list_is_empty(VAL_VEC(val)) ? NIL : VAL_VEC(val)->head;
  }
  err_warning(ERR_ARGUMENT_MISMATCH, "cannot take first of non-container");
  return NIL;
}

static MalVal *builtin_last(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, ARGS_MAX, types_containers))
    return NIL;

  MalVal *val = args->head;

  List *list;
  switch(VAL_TYPE(val)) {
    case TYPE_LIST:
      list = VAL_LIST(val);
      break;
    case TYPE_VECTOR:
      list = VAL_VEC(val);
      break;
    default:
      exception = malval_string("cannot take last of non-container");
      return NIL;
  }

  while (list && list->tail)
    list = list->tail;

  return list ? list->head : NIL;
}

static MalVal *builtin_butlast(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, ARGS_MAX, types_containers))
    return NIL;

  MalVal *val = args->head;

  List *result = NULL;
  List *list;
  switch(VAL_TYPE(val)) {
    case TYPE_LIST:
      list = VAL_LIST(val);
      break;
    case TYPE_VECTOR:
      list = VAL_VEC(val);
      break;
    default:
      exception = malval_string("cannot take last of non-container");
      return NIL;
  }

  while (list && list->tail) {
    result = cons_weak(list->head, result);
    list = list->tail;
  }

  list_reverse(&result);
  MalVal *rv = malval_list(result);
  list_release(result);
  return rv;
}

static MalVal *builtin_rest(List *args, ENV *env)
{
  if (args && VAL_IS_NIL(args->head))
    return malval_list(NULL);

  if (!builtins_args_check(args, 1, ARGS_MAX, types_containers))
    return NIL;

  MalVal *val = args->head;

  switch(VAL_TYPE(val)) {
    case TYPE_LIST:
      return malval_list(list_is_empty(VAL_LIST(val)) ? NULL : VAL_LIST(val)->tail);
    case TYPE_VECTOR:
      return malval_list(list_is_empty(VAL_VEC(val)) ? NULL : VAL_VEC(val)->tail);
  }
  err_warning(ERR_ARGUMENT_MISMATCH, "cannot take rest of non-container");
  return NIL;
}

static MalVal *builtin_reverse(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, types_containers))
    return NIL;

  List *result = NULL;
  for (List *input = list_from_container(args->head); input; input = input->tail) {
    result = cons_weak(input->head, result);
  }

  MalVal *rv = malval_list(result);
  list_release(result);
  return rv;
}

static MalType types_map[] = {TYPE_FUNCTION, METATYPE_CONTAINER, 0};
static MalVal *builtin_map(List *args, ENV *env)
{
  if (!builtins_args_check(args, 2, 2, types_map))
    return NIL;

  List *result = NULL;
  Function *f = VAL_FUNCTION(args->head);
  List *input = list_from_container(args->tail->head);
  for (; input; input = input->tail) {
    List a = {NULL, 1, input->head};
    result = cons_weak(apply(f, &a), result);
  }

  linked_list_reverse((void**)&result);
  MalVal *rv = malval_list(result);
  list_release(result);
  return rv;
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

  char *s = NULL;
#ifdef AGON_LIGHT
  /* TODO: AGON VERSION */
#error NIY
#else
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
#endif

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

  atom->data.atom = apply(VAL_FUNCTION(func), fargs);
  list_release(fargs);

  return atom->data.atom;
}

static MalType types_nth[] = {METATYPE_CONTAINER, TYPE_NUMBER, 0};
static MalVal *builtin_nth(List *args, ENV *env)
{
  if (!builtins_args_check(args, 2, 2, types_nth))
    return NIL;

  MalVal *rv = NULL;
  int count = args->tail->head->data.number;

  if (VAL_TYPE(args->head) == TYPE_LIST) {
    rv = list_nth(VAL_LIST(args->head), count);
  }
  else if (VAL_TYPE(args->head) == TYPE_VECTOR) {
    rv = list_nth(VAL_VEC(args->head), count);
  }
  else {
    err_warning(ERR_NOT_IMPLEMENTED, "nth only on list and vector");
    return NIL;
  }

  if (rv == NULL)
    exception = malval_string("bounds exception");

  return rv;
}

static MalVal *builtin_is_nil(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return VAL_IS_NIL(args->head) ? T : F;
}

static MalVal *builtin_is_true(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return VAL_IS_TRUE(args->head) ? T : F;
}

static MalVal *builtin_is_false(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return VAL_IS_FALSE(args->head) ? T : F;
}

static MalVal *builtin_is_symbol(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  if (VAL_TYPE(args->head) == TYPE_SYMBOL) {
    /* is it really a keyword */
    return VAL_IS_KEYWORD(args->head) ? F : T;
  }

  return F;
}

static MalVal *builtin_is_keyword(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return VAL_IS_KEYWORD(args->head) ? T : F;
}

static MalVal *builtin_throw(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  exception = args->head;

  return NIL;
}

static MalVal *builtin_symbol(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, types_string))
    return NIL;

  return malval_symbol(args->head->data.string);
}

static MalType types_keyword[] = {METATYPE_STRING, 0};
static MalVal *builtin_keyword(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, types_keyword))
    return NIL;

  if (VAL_IS_KEYWORD(args->head))
    return args->head;

  char *s = args->head->data.string;
  char *kw  = alloca(2 + strlen(s));
  kw[0] = -1;
  strcpy(&kw[1], s);
  return malval_symbol(kw);
}

static MalVal *builtin_vector(List *args, ENV *env)
{
  return malval_vector(args);
}

static MalVal *builtin_is_vector(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return VAL_TYPE(args->head) == TYPE_VECTOR ? T : F;
}

static MalVal *builtin_is_sequential(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return (VAL_TYPE(args->head) == TYPE_VECTOR || VAL_TYPE(args->head) == TYPE_LIST) ? T : F;
}

static MalVal *builtin_hash_map(List *args, ENV *env)
{
  if ((list_count(args) % 2) != 0) {
    err_warning(ERR_ARGUMENT_MISMATCH, "must be even number of arguments");
    return NIL;
  }
  return malval_map(list_acquire(args));
}

static MalVal *builtin_is_map(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return VAL_TYPE(args->head) == TYPE_MAP ? T : F;
}

static MalVal *assoc_map(List *args, ENV *env)
{
  MalVal *map = args->head;

  if ((list_count(args->tail) % 2) != 0) {
    err_warning(ERR_ARGUMENT_MISMATCH, "must be even number of arguments");
    return NIL;
  }

  List *result = list_duplicate(VAL_MAP(map));
  for (List *entry = args->tail; entry && entry; entry = entry->tail->tail) {
    result = cons_weak(entry->head, cons_weak(entry->tail->head, result));
  }
  List *normalised = map_normalise(result);
  MalVal *val = malval_map(normalised);
  list_release(result);
  list_release(normalised);
  return val;
}

static MalVal *assoc_vec(List *args, ENV *env)
{
  MalVal *vec = args->head;

  if ((list_count(args->tail) % 2) != 0) {
    err_warning(ERR_ARGUMENT_MISMATCH, "must be even number of arguments");
    return NIL;
  }

  List *result = list_duplicate(VAL_VEC(vec));
  list_reverse(&result);
  unsigned c = list_count(result);

  for (List *entry = args->tail; entry; entry = entry->tail->tail) {
    if (VAL_TYPE(entry->head) != TYPE_NUMBER) {
      err_warning(ERR_ARGUMENT_MISMATCH, "require number as index to vec");
      list_release(result);
      return NIL;
    }
    unsigned index = entry->head->data.number;
    if (index != c) {
      list_release(result);
      exception = malval_string("Index out of bounds");
      return NIL;
    }

    result = cons_weak(entry->tail->head, result);
    c++;
  }

  list_reverse(&result);

  MalVal *val = malval_vector(result);
  list_release(result);
  return val;
}

static MalVal *builtin_assoc(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, ARGS_MAX, types_container))
    return NIL;

  if (VAL_TYPE(args->head) == TYPE_MAP) {
    return assoc_map(args, env);
  }
  else if (VAL_TYPE(args->head) == TYPE_VECTOR) {
    return assoc_vec(args, env);
  }

  err_warning(ERR_ARGUMENT_MISMATCH, "assoc requires a map or vector");
  return NIL;
}

static List *dissoc_key(List *map, MalVal *key)
{
  List *result = NULL;
  for (List *entry = map; entry && entry->tail; entry = entry->tail->tail) {
    if (!malval_equals(entry->head, key)) {
      result = cons_weak(entry->tail->head, cons_weak(entry->head, result));
    }
  }
  linked_list_reverse((void**)&result);
  return result;
}

static MalType types_dissoc[] = {TYPE_MAP, 0};
static MalVal *builtin_dissoc(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, ARGS_MAX, types_dissoc))
    return NIL;

  MalVal *map = args->head;
  List *keys = args->tail;
  List *result = list_duplicate(VAL_MAP(map));

  for (List *key = keys; key; key = key->tail) {
    List *l2 = dissoc_key(result, key->head);
    list_release(result);
    result = l2;
  }

  MalVal *rv = malval_map(result);
  list_release(result);
  return rv;
}

static MalVal *builtin_get(List *args, ENV *env)
{
  if (!builtins_args_check(args, 2, 2, types_container))
    return NIL;

  if (VAL_TYPE(args->head) == TYPE_MAP) {
    List *map = VAL_MAP(args->head);
    MalVal *key = args->tail->head;
    for (List *entry = map; entry && entry->tail; entry = entry->tail->tail) {
      if (malval_equals(key, entry->head))
        return entry->tail->head;
    }
    return NIL;
  }

  if (VAL_TYPE(args->head) == TYPE_VECTOR) {
    List *vec = VAL_VEC(args->head);
    MalVal *index = args->tail->head;
    if (VAL_TYPE(index) != TYPE_NUMBER) {
      exception = malval_string("Invalid index");
      return NIL;
    }
    MalVal *rv = list_nth(vec, index->data.number);
    return rv ? rv : NIL;
  }

  exception = malval_string("get requires vec or map");
  return NIL;
}

static MalVal *builtin_contains(List *args, ENV *env)
{
  if (!builtins_args_check(args, 2, 2, types_container))
    return NIL;

  if (VAL_TYPE(args->head) == TYPE_MAP) {
    return map_contains(VAL_MAP(args->head), args->tail->head) ? T : F;
  }

  if (VAL_TYPE(args->head) == TYPE_VECTOR) {
    List *vec = VAL_VEC(args->head);
    MalVal *index = args->tail->head;
    if (VAL_TYPE(index) != TYPE_NUMBER) {
      exception = malval_string("Invalid index");
      return NIL;
    }
    int i = index->data.number;
    while (vec && i-- > 0)
      vec = vec->tail;
    return i == -1 ? T : F;
  }

  exception = malval_string("contains? requires vec or map");
  return NIL;
}

static MalType types_hashmap[] = {TYPE_MAP, 0};
static MalVal *builtin_keys(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, types_hashmap))
    return NIL;

  List *normalised = map_normalise(VAL_MAP(args->head));
  List *result = NULL;
  for (List *entry = normalised; entry && entry->tail; entry = entry->tail->tail) {
    result = cons_weak(entry->head, result);
  }

  MalVal *rv = malval_list(result);
  list_release(normalised);
  list_release(result);
  return rv;
}

static MalVal *builtin_vals(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, types_hashmap))
    return NIL;

  List *normalised = map_normalise(VAL_MAP(args->head));
  List *result = NULL;
  for (List *entry = normalised; entry && entry->tail; entry = entry->tail->tail) {
    result = cons_weak(entry->tail->head, result);
  }

  MalVal *rv = malval_list(result);
  list_release(normalised);
  list_release(result);
  return rv;
}

static MalVal *builtin_readline(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, types_string))
    return NIL;

#ifdef AGON_LIGHT
#error NIY
#else
  char buf[256], *s;
  fputs(args->head->data.string, stdout);

  if ((s = fgets(buf, 255, stdin)) == NULL)
    return NIL;

  s[strlen(s)-1] = '\0';

  return malval_string(s);
#endif
}

static MalVal *builtin_time_ms(List *args, ENV *env)
{
  if (!builtins_args_check(args, 0, 0, NULL))
    return NIL;

#ifdef AGON_LIGHT
#error NIY
#else
  struct timeval tv;
  if (gettimeofday(&tv, NULL) < 0) {
    err_warning(ERR_INVALID_OPERATION, "gettimeofday() failed");
    return NIL;
  }

  long epoch = 1720700000000; /* random epoch keeps ms within integer size */
  long time = (long)tv.tv_sec * 1000L + (long)tv.tv_usec / 1000L;
  return malval_number((int)(time-epoch));
#endif

  return NIL;
}

static MalVal *builtin_is_fn(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return (VAL_TYPE(args->head) == TYPE_FUNCTION && VAL_FUNCTION(args->head)->is_macro == 0) ? T : F;
}

static MalVal *builtin_is_macro(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return (VAL_TYPE(args->head) == TYPE_FUNCTION && VAL_FUNCTION(args->head)->is_macro == 1) ? T : F;
}

static MalVal *builtin_is_string(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return VAL_TYPE(args->head) == TYPE_STRING ? T : F;
}

static MalVal *builtin_is_number(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return VAL_TYPE(args->head) == TYPE_NUMBER ? T : F;
}

static MalVal *builtin_seq(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  List *result = NULL;

  switch(VAL_TYPE(args->head)) {
    case TYPE_NIL:
      return NIL;
    case TYPE_LIST:
      if (list_is_empty(VAL_LIST(args->head)))
        return NIL;
      return args->head;
    case TYPE_VECTOR:
      if (list_is_empty(VAL_VEC(args->head)))
        return NIL;
      result = list_from_container(args->head);
      break;
    case TYPE_STRING:
      if (args->head->data.string[0] == '\0')
        return NIL;
      result = list_from_string(args->head->data.string);
      break;
    default:
      exception = malval_string("not a container");
      return NIL;
  }

  MalVal *rv = malval_list(result);
  list_release(result);
  return rv;
}

static MalType types_gensym[] = {TYPE_STRING, 0};
MalVal *builtin_gensym(List *args, ENV *env)
{
  static long i = 0;
  char *pfx;

  if (!builtins_args_check(args, 0, 1, types_gensym))
    return NIL;

  if (args && VAL_TYPE(args->head) == TYPE_STRING) {
    pfx = VAL_STRING(args->head);
  } else {
    pfx = "gensym_";
  }

  unsigned lpfx = strlen(pfx);
  char *buf = alloca(lpfx + 20);

  strcpy(buf, pfx);

  itoa(i++, &buf[lpfx], 10);

  return malval_symbol(buf);
}

static MalVal *builtin_conj(List *args, ENV *env)
{
  if (!args)
    return malval_vector(NULL);

  if (VAL_IS_NIL(args->head))
    return malval_list(args->tail);

  if (VAL_TYPE(args->head) == TYPE_VECTOR) {
    List *result = list_concat(VAL_VEC(args->head), args->tail);
    MalVal *rv = malval_vector(result);
    list_release(result);
    return rv;
  }

  if (VAL_TYPE(args->head) == TYPE_LIST) {
    List *result = list_acquire(VAL_LIST(args->head));

    for (List *arg = args->tail; arg; arg = arg->tail)
      result = cons_weak(arg->head, result);

    MalVal *rv = malval_list(result);
    list_release(result);
    return rv;
  }

  exception = malval_string("not a container");
  return NIL;
}

static MalVal *builtin_meta(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  switch(VAL_TYPE(args->head)) {
    case TYPE_LIST:
      return args->head->data.list->meta;
    case TYPE_VECTOR:
      return args->head->data.vec->meta;
    case TYPE_MAP:
      return args->head->data.map->meta;
    case TYPE_FUNCTION:
      return args->head->data.fn->meta;
  }

  return NIL;
}

static MalVal *builtin_with_meta(List *args, ENV *env)
{
  if (!builtins_args_check(args, 2, 2, NULL))
    return NIL;

  MalVal *metadata = args->tail->head;
  MalVal *result = NIL;

  if (VAL_TYPE(args->head) == TYPE_LIST) {
    result = malval_list(VAL_LIST(args->head));
    result->data.list->meta = metadata;
  }
  else if (VAL_TYPE(args->head) == TYPE_VECTOR) {
    result = malval_vector(VAL_VEC(args->head));
    result->data.vec->meta = metadata;
  }
  else if (VAL_TYPE(args->head) == TYPE_MAP) {
    result = malval_map(VAL_MAP(args->head));
    result->data.vec->meta = metadata;
  }
  else if (VAL_TYPE(args->head) == TYPE_FUNCTION) {
    Function *copy = function_duplicate(VAL_FUNCTION(args->head));
    result = malval_function(copy);
    result->data.fn->meta = metadata;
  }
  else {
    exception = malval_string("Cannot set metadata on object");
  }

  return result;
}


static MalVal *builtin_debug_info(List *args, ENV *env)
{
  if (!builtins_args_check(args, 0, 0, NULL))
    return NIL;

  unsigned count, size;
  List *result = NULL, *values;

  value_info(&count, &size);
  values = cons_weak(malval_keyword(":count"), 
           cons_weak(malval_number(count),
           cons_weak(malval_keyword(":size"),
           cons_weak(malval_number(size),
           NULL))));

  result = cons_weak(malval_keyword(":values"),
           cons_weak(malval_map(values),
           result));
  list_release(values);

#ifndef NDEBUG
  heap_info(&count, &size);
  values = cons_weak(malval_keyword(":count"), 
           cons_weak(malval_number(count),
           cons_weak(malval_keyword(":size"),
           cons_weak(malval_number(size),
           NULL))));

  result = cons_weak(malval_keyword(":heap"),
           cons_weak(malval_map(values),
           result));
  list_release(values);
#endif

  MalVal *rv = malval_map(result);
  list_release(result);
  return rv;
}

struct ns core_ns[] = {
  {"+", plus},
  {"-", minus},
  {"*", multiply},
  {"/", divide},
  {"mod", builtin_mod},
  {"=", builtin_equals},
  {"<", lessthan},
  {"<=", lessthan_or_equal},
  {">", morethan},
  {">=", morethan_or_equal},
  {"apply", builtin_apply},
  {"cons", builtin_cons},
  {"concat", builtin_concat},
  {"vec", builtin_vec},
  {"first", builtin_first},
  {"last", builtin_last},
  {"butlast", builtin_butlast},
  {"rest", builtin_rest},
  {"nth", builtin_nth},
  {"reverse", builtin_reverse},
  {"map", builtin_map},
  {"list", builtin_list},
  {"list?", builtin_is_list},
  {"empty?", builtin_is_empty},
  {"count", builtin_count},
  {"nil?", builtin_is_nil},
  {"true?", builtin_is_true},
  {"false?", builtin_is_false},
  {"symbol", builtin_symbol},
  {"symbol?", builtin_is_symbol},
  {"keyword", builtin_keyword},
  {"keyword?", builtin_is_keyword},
  {"vector", builtin_vector},
  {"vector?", builtin_is_vector},
  {"hash-map", builtin_hash_map},
  {"map?", builtin_is_map},
  {"assoc", builtin_assoc},
  {"dissoc", builtin_dissoc},
  {"get", builtin_get},
  {"contains?", builtin_contains},
  {"keys", builtin_keys},
  {"vals", builtin_vals},
  {"sequential?", builtin_is_sequential},
  {"pr-str", builtin_pr_str},
  {"str", builtin_str},
  {"prn", builtin_prn},
  {"println", builtin_println},
  {"gc", builtin_gc},
  {"read-string", builtin_read_string},
  {"slurp", builtin_slurp},
  {"throw", builtin_throw},
  /* atom stuff: */
  {"atom", builtin_atom},
  {"atom?", builtin_is_atom},
  {"deref", builtin_deref},
  {"reset!", builtin_reset},
  {"swap!", builtin_swap},

  {"readline", builtin_readline},

  {"time-ms", builtin_time_ms},

  {"meta", builtin_meta},
  {"with-meta", builtin_with_meta},
  {"conj", builtin_conj},

  {"fn?", builtin_is_fn},
  {"macro?", builtin_is_macro},
  {"string?", builtin_is_string},
  {"number?", builtin_is_number},
  {"seq", builtin_seq},

  {"gensym", builtin_gensym},

  {"debug-info", builtin_debug_info},

  {NULL, NULL},
};

