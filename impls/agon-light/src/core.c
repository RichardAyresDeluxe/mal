#include "malval.h"
#include "list.h"
#include "map.h"
#include "vec.h"
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
#include "iter.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <alloca.h>
#include <assert.h>

#ifdef AGON_LIGHT
#include <mos_api.h>
#endif

#ifndef AGON_LIGHT
#include <sys/time.h>
#endif

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
      exception = malval_string("not a numeric argument");
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

static float_t number_to_float(MalVal *n)
{
  switch(VAL_TYPE(n)) {
  case TYPE_FLOAT:
    return VAL_FLOAT(n);
  case TYPE_NUMBER:
    return (float_t)VAL_NUMBER(n);
  case TYPE_BYTE:
    return (float_t)VAL_BYTE(n);
  }
  err_fatal(ERR_ARGUMENT_MISMATCH, "invalid number");
  return 0;
}

static int number_to_int(MalVal *n)
{
  switch(VAL_TYPE(n)) {
  case TYPE_FLOAT:
    return (int)VAL_FLOAT(n);
  case TYPE_NUMBER:
    return VAL_NUMBER(n);
  case TYPE_BYTE:
    return (int)VAL_BYTE(n);
  }
  err_fatal(ERR_ARGUMENT_MISMATCH, "invalid number");
  return 0;
}

static MalVal *_plus(MalVal *a, MalVal *b)
{
  if (VAL_TYPE(a) == TYPE_FLOAT || VAL_TYPE(b) == TYPE_FLOAT) {
    float_t result = number_to_float(a) + number_to_float(b);
    a->type = TYPE_FLOAT;
    VAL_FLOAT(a) = result;
    return a;
  }

  int result = number_to_int(a) + number_to_int(b);
  a->type = TYPE_NUMBER;
  VAL_NUMBER(a) = result;
  return a;
}

static MalVal *_minus(MalVal *a, MalVal *b)
{
  if (VAL_TYPE(a) == TYPE_FLOAT || VAL_TYPE(b) == TYPE_FLOAT) {
    float_t result = number_to_float(a) - number_to_float(b);
    a->type = TYPE_FLOAT;
    VAL_FLOAT(a) = result;
    return a;
  }

  int result = number_to_int(a) - number_to_int(b);
  a->type = TYPE_NUMBER;
  VAL_NUMBER(a) = result;
  return a;
}

static MalVal *_multiply(MalVal *a, MalVal *b)
{
  if (VAL_TYPE(a) == TYPE_FLOAT || VAL_TYPE(b) == TYPE_FLOAT) {
    float_t result = number_to_float(a) * number_to_float(b);
    a->type = TYPE_FLOAT;
    VAL_FLOAT(a) = result;
    return a;
  }

  int result = number_to_int(a) * number_to_int(b);
  a->type = TYPE_NUMBER;
  VAL_NUMBER(a) = result;
  return a;
}

static MalVal *_divide(MalVal *a, MalVal *b)
{
  if (VAL_TYPE(a) == TYPE_FLOAT || VAL_TYPE(b) == TYPE_FLOAT) {
    float_t bn = number_to_float(b);
    if (bn == 0.0)
      malthrow("divide by zero");

    float_t result = number_to_float(a) / bn;
    a->type = TYPE_FLOAT;
    VAL_FLOAT(a) = result;
    return a;
  }

  int bn = number_to_int(b);
  if (bn == 0)
      malthrow("divide by zero");
  int result = number_to_int(a) / bn;
  a->type = TYPE_NUMBER;
  VAL_NUMBER(a) = result;
  return a;
}

static MalVal *plus(List *args, ENV *env)
{
  if (!builtins_all_numeric(args))
    return NIL;

  MalVal *result = malval_number(0);

  for (List *rover = args; rover; rover = rover->tail) {
    result = _plus(result, rover->head);
  }

  return result;
}

static MalVal *minus(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, ARGS_MAX, NULL)
   || !builtins_all_numeric(args))
  {
    return NIL;
  }

  MalVal *result = malval_dup(args->head);

  for (List *rover = args->tail; rover; rover = rover->tail) {
    result = _minus(result, rover->head);
  }

  return result;
}

static MalVal *multiply(List *args, ENV *env)
{
  if (!builtins_all_numeric(args))
    return NIL;

  MalVal *result = malval_number(1);

  for (List *rover = args; rover; rover = rover->tail) {
    result = _multiply(result, rover->head);
  }
  return result;
}

static MalVal *divide(List *args, ENV *env)
{
  if (!builtins_args_check(args, 2, ARGS_MAX, NULL)
   || !builtins_all_numeric(args))
  {
    return NIL;
  }

  MalVal *result = malval_dup(args->head);

  for (List *rover = args->tail; rover; rover = rover->tail) {
    result = _divide(result, rover->head);
  }
  return result;
}

static MalVal *lessthan(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, ARGS_MAX, NULL)
   || !builtins_all_numeric(args))
  {
    return NIL;
  }

  if (!args->tail)
    return T;

  do {
    if (VAL_NUMBER(args->head) >= VAL_NUMBER(args->tail->head))
      return F;
    args = args->tail;
  } while (args->tail);

  return T;
}

static MalVal *core_mod(List *args, ENV *env)
{
  if (!builtins_args_check(args, 2, 2, types_numbers))
    return NIL;

  return malval_number(VAL_NUMBER(args->head) % VAL_NUMBER(args->tail->head));
}

static MalType types_apply[] = {TYPE_FUNCTION, 0};
static MalVal *core_apply(List *args, ENV *env)
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

static MalVal *core_list(List *args, ENV *env)
{
  return malval_list(args);
}

static MalVal *core_cons(List *args, ENV *env)
{
  if (!builtins_args_check(args, 2, 2, NULL))
    return NIL;

  List *list = NULL;
  switch(VAL_TYPE(args->tail->head)) {
    case TYPE_NIL:
      list = NULL;
      break;
    case TYPE_LIST:
      list = VAL_LIST(args->tail->head);
      break;
    case TYPE_VECTOR:
      list = list_from_container(args->tail->head);
      break;
    case TYPE_MAP:
      malthrow("cannot cons to maps yet");
    default:
      malthrow("unable to cons to object");
  }

  return malval_list_weak(cons(args->head, list));
}

static MalVal *core_concat(List *args, ENV *env)
{
  if (!builtins_args_check(args, 0, ARGS_MAX, NULL))
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

  return malval_list_weak(result);
}

static MalVal *core_vec(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, types_containers))
    return NIL;

  if (VAL_TYPE(args->head) == TYPE_VECTOR)
    return args->head;

  if (VAL_TYPE(args->head) == TYPE_LIST) {
    Vec *v = vec_create();
    for (List *rover = VAL_LIST(args->head); rover; rover = rover->tail) {
      vec_append(v, rover->head);
    }
    return malval_vector(v);
  }

  malthrow("cannot turn container into a vector");
}

static MalVal *core_is_list(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return VAL_TYPE(args->head) == TYPE_LIST ? T : F;
}

static MalVal *core_is_empty(List *args, ENV *env)
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
   || (VAL_TYPE(args->head) == TYPE_VECTOR && vec_count(VAL_VEC(args->head)) == 0)
   || (VAL_TYPE(args->head) == TYPE_MAP && map_is_empty(VAL_MAP(args->head)))
  ) {
    return T;
  }
  return F;
}

static MalVal *core_count(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  if (VAL_IS_NIL(args->head))
    return malval_number(0);

  switch (VAL_TYPE(args->head)) {
    case TYPE_LIST:
      return malval_number(list_count(VAL_LIST(args->head)));
    case TYPE_VECTOR:
      return malval_number(vec_count(VAL_VEC(args->head)));
    case TYPE_MAP:
      return malval_number(map_count(VAL_MAP(args->head)));
    case TYPE_SET:
      return malval_number(map_count(VAL_SET(args->head)));
    case TYPE_STRING:
    case TYPE_SYMBOL:
      return malval_number(strlen(VAL_STRING(args->head)));
  }

  err_warning(ERR_ARGUMENT_MISMATCH, "cannot count object");
  return NIL;
}

static MalVal *core_equals(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, ARGS_MAX, NULL))
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

static MalVal *core_pr_str(List *args, ENV *env)
{
  struct pr_struct pr = { NULL, " ", TRUE };

  list_foreach(args, pr_str_helper, &pr);

  MalVal *rv = malval_string(pr.s ? &pr.s[1] : "");
  heap_free(pr.s);
  return rv;
}

static MalVal *core_str(List *args, ENV *env)
{
  struct pr_struct pr = { NULL, NULL, FALSE };

  list_foreach(args, pr_str_helper, &pr);

  MalVal *rv = malval_string(pr.s ? pr.s : "");
  heap_free(pr.s);
  return rv;
}

static MalVal *core_prn(List *args, ENV *env)
{
  struct pr_struct pr = { NULL, " ", TRUE };

  list_foreach(args, pr_str_helper, &pr);

  puts(pr.s ? &pr.s[1] : "");
  heap_free(pr.s);

  return NIL;
}

static MalVal *core_println(List *args, ENV *env)
{
  struct pr_struct pr = { NULL, " ", FALSE };

  list_foreach(args, pr_str_helper, &pr);

  puts(pr.s ? &pr.s[1] : "");
  heap_free(pr.s);

  return NIL;
}

static MalVal *core_first(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, ARGS_MAX, types_containers))
    return NIL;

  MalVal *val = args->head;

  switch(VAL_TYPE(val)) {
    case TYPE_LIST:
      return list_is_empty(VAL_LIST(val)) ? NIL : VAL_LIST(val)->head;
    case TYPE_VECTOR:
      return (vec_count(VAL_VEC(val)) == 0) ? NIL : vec_get(VAL_VEC(val), 0);
  }
  err_warning(ERR_ARGUMENT_MISMATCH, "cannot take first of non-container");
  return NIL;
}

static MalVal *core_last(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, ARGS_MAX, types_containers))
    return NIL;

  MalVal *val = args->head;

  List *list;
  switch(VAL_TYPE(val)) {
    case TYPE_LIST:
      list = VAL_LIST(val);
      break;
    case TYPE_VECTOR: {
      Vec *v = VAL_VEC(val);
      unsigned c = vec_count(v);
      if (c == 0)
        return NIL;
      else
        return vec_get(v, c - 1);
    }
    default:
      exception = malval_string("cannot take last of non-container");
      return NIL;
  }

  while (list && list->tail)
    list = list->tail;

  return list ? list->head : NIL;
}

static MalVal *core_butlast(List *args, ENV *env)
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
    case TYPE_VECTOR: {
      Vec *v = VAL_VEC(val);
      unsigned c = vec_count(v);
      if (c <= 1)
        return NIL;
      Vec *rv = vec_slice(v, 0, c - 1);
      list = list_from_vec(rv);
      vec_destroy(rv);
      return malval_list_weak(list);
    }
    default:
      exception = malval_string("cannot take last of non-container");
      return NIL;
  }

  while (list && list->tail) {
    result = cons_weak(list->head, result);
    list = list->tail;
  }

  list_reverse(&result);
  return malval_list_weak(result);
}

static MalVal *core_rest(List *args, ENV *env)
{
  if (args && VAL_IS_NIL(args->head))
    return malval_list(NULL);

  if (!builtins_args_check(args, 1, ARGS_MAX, types_containers))
    return NIL;

  if (VAL_TYPE(args->head) == TYPE_LIST) {
    List *l = VAL_LIST(args->head);
    return malval_list(l ? l->tail : NULL);
  }
  else if (VAL_TYPE(args->head) == TYPE_VECTOR) {
    Vec *v = VAL_VEC(args->head);
    if (vec_count(v) <= 1)
      return malval_list(NULL);

    v = vec_slice(v, 1, vec_count(v)-1);
    MalVal *rv = malval_list_weak(list_from_vec(v));
    vec_destroy(v);
    return rv;
  }

  malthrow("cannot take rest of non-container");
}

static MalVal *core_reverse(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, types_containers))
    return NIL;

  List *result = NULL;
  Iterator *iter = iter_create(args->head);
  MalVal *val;
  while ((val = iter_next(iter)) != NULL) {
    result = cons_weak(val, result);
  }
  iter_destroy(iter);

  return malval_list_weak(result);
}

static MalType types_map[] = {TYPE_FUNCTION, 0};
static MalVal *core_map(List *args, ENV *env)
{
  if (!builtins_args_check(args, 2, 2, types_map))
    return NIL;

  List *result = NULL;
  Function *f = VAL_FUNCTION(args->head);

  Iterator *iter = iter_create(args->tail->head);
  if (!iter)
    return NIL;

  push_temp(malval_list(args));

  MalVal *val;
  while ((val = iter_next(iter)) != NULL) {
    result = cons_weak(apply1(f, val), result);
    if (exception) {
      iter_destroy(iter);
      pop_temp();
      return NIL;
    }
  }
  iter_destroy(iter);
  list_reverse(&result);

  pop_temp();

  return malval_list_weak(result);
}

static MalVal *core_gc(List *args, ENV *env)
{
  if (!builtins_args_check(args, 0, 1, NULL))
    return NIL;

  gc(args ? VAL_TRUE(args->head) : FALSE);

  return NIL;
}

static MalVal *core_read_string(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, types_string))
    return NIL;

  return read_string(args->head->data.string);
}

static MalVal *core_slurp(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, types_string))
    return NIL;

  char buf[80];
  char *s = NULL;

#ifdef AGON_LIGHT
  uint8_t fh = mos_fopen(VAL_STRING(args->head), FA_OPEN_EXISTING|FA_READ);
  if (!fh)
    malthrow("cannot open file %s");

  while (!mos_feof(fh)) {
    buf[0] = '\0';
    unsigned rc = mos_fread(fh, buf, sizeof(buf)-1);
    catstr(&s, buf);
    if (rc == 0)
      break;
  }

  mos_fclose(fh);
#else
  FILE *fh = fopen(VAL_STRING(args->head), "r");
  if (!fh)
    malthrow("cannot open file");

  while (!feof(fh)) {
    buf[0] = '\0';
    fgets(buf, sizeof(buf)-1, fh);
    catstr(&s, buf);
  }
  fclose(fh);
#endif

  return malval_string_own(s);
}

static MalVal *core_atom(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return malval_atom(args->head);
}

static MalVal *core_is_atom(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return VAL_TYPE(args->head) == TYPE_ATOM ? T : F;
}

static MalType types_atom[] = {TYPE_ATOM, 0};
static MalVal *core_deref(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, types_atom))
    return NIL;

  return args->head->data.atom;
}

static MalVal *core_reset(List *args, ENV *env)
{
  if (!builtins_args_check(args, 2, 2, types_atom))
    return NIL;

  args->head->data.atom = args->tail->head;

  return args->tail->head;
}

static MalType types_swap[] = {TYPE_ATOM, TYPE_FUNCTION, 0};
static MalVal *core_swap(List *args, ENV *env)
{
  if (!builtins_args_check(args, 2, ARGS_MAX, types_swap))
    return NIL;

  MalVal *atom = args->head;
  MalVal *func = args->tail->head;
  List *fargs = cons(atom->data.atom, args->tail->tail);

  push_temp(atom);
  push_temp(func);
  push_temp(malval_list(fargs));
  push_temp(malval_list(args));

  atom->data.atom = apply(VAL_FUNCTION(func), fargs);
  list_release(fargs);

  pop_temps(4);

  return atom->data.atom;
}

static MalType types_nth[] = {METATYPE_CONTAINER, TYPE_NUMBER, 0};
static MalVal *core_nth(List *args, ENV *env)
{
  if (!builtins_args_check(args, 2, 2, types_nth))
    return NIL;

  MalVal *rv = NULL;
  int count = VAL_NUMBER(args->tail->head);

  if (VAL_TYPE(args->head) == TYPE_LIST) {
    rv = list_nth(VAL_LIST(args->head), count);
  }
  else if (VAL_TYPE(args->head) == TYPE_VECTOR) {
    if ((unsigned)count >= vec_count(VAL_VEC(args->head)))
      rv = NULL;
    else
      rv = vec_get(VAL_VEC(args->head), count);
  }
  else {
    malthrow("nth only on list and vector");
  }

  if (rv == NULL)
    malthrow("index out of bounds");

  return rv;
}

static MalVal *core_is_nil(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return VAL_IS_NIL(args->head) ? T : F;
}

static MalVal *core_is_true(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return VAL_IS_TRUE(args->head) ? T : F;
}

static MalVal *core_is_false(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return VAL_IS_FALSE(args->head) ? T : F;
}

static MalVal *core_is_symbol(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  if (VAL_TYPE(args->head) == TYPE_SYMBOL) {
    /* is it really a keyword */
    return VAL_IS_KEYWORD(args->head) ? F : T;
  }

  return F;
}

static MalVal *core_is_keyword(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return VAL_IS_KEYWORD(args->head) ? T : F;
}

static MalVal *core_throw(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  exception = args->head;

  return NIL;
}

static MalVal *core_symbol(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, types_string))
    return NIL;

  return malval_symbol(args->head->data.string);
}

static MalVal *core_byte(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  switch (VAL_TYPE(args->head)) {
    case TYPE_BYTE:     return args->head;
    case TYPE_NUMBER:   return malval_byte(VAL_NUMBER(args->head));
    case TYPE_BOOL:     return malval_byte(VAL_BOOL(args->head) ? 1 : 0);
    case TYPE_STRING:   return malval_byte(VAL_STRING(args->head)[0]);
  }

  malthrow("cannot convert object to byte");
}

static MalVal *core_int(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  switch (VAL_TYPE(args->head)) {
    case TYPE_BYTE:     return malval_number(VAL_BYTE(args->head));
    case TYPE_NUMBER:   return args->head;
    case TYPE_BOOL:     return malval_number(VAL_BOOL(args->head) ? 1 : 0);
  }

  malthrow("cannot convert object to integer");
}

static MalVal *core_float(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  switch (VAL_TYPE(args->head)) {
    case TYPE_BYTE:     return malval_float(VAL_BYTE(args->head));
    case TYPE_NUMBER:   return malval_float(VAL_NUMBER(args->head));
    case TYPE_FLOAT:    return args->head;
    case TYPE_BOOL:     return malval_float(VAL_BOOL(args->head) ? 1 : 0);
  }

  malthrow("cannot convert object to float");
}

static MalType types_keyword[] = {METATYPE_STRING, 0};
static MalVal *core_keyword(List *args, ENV *env)
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

static MalVal *core_vector(List *args, ENV *env)
{
  Vec *v = vec_create();
  for (List *arg = args; arg; arg = arg->tail)
    vec_append(v, arg->head);
  return malval_vector(v);
}

static MalVal *core_is_vector(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return VAL_TYPE(args->head) == TYPE_VECTOR ? T : F;
}

static MalVal *core_is_sequential(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return (VAL_TYPE(args->head) == TYPE_VECTOR || VAL_TYPE(args->head) == TYPE_LIST) ? T : F;
}

static MalVal *core_hash_map(List *args, ENV *env)
{
  unsigned count = list_count(args);

  if ((count % 2) != 0)
    malthrow("must be even number of arguments");

  Map *map = map_createN(count / 2);
  for (List *arg = args; arg && arg->tail; arg = arg->tail->tail) {
    map_add(map, arg->head, arg->tail->head);
  }
  MalVal *rv = malval_map(map);
  map_release(map);
  return rv;
}

static MalVal *core_is_map(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return VAL_TYPE(args->head) == TYPE_MAP ? T : F;
}

static MalVal *assoc_map(List *args, ENV *env)
{
  if ((list_count(args->tail) % 2) != 0) {
    err_warning(ERR_ARGUMENT_MISMATCH, "must be even number of arguments");
    return NIL;
  }

  Map *map = map_duplicate(VAL_MAP(args->head));

  for (List *entry = args->tail; entry && entry->tail; entry = entry->tail->tail) {
    map_add(map, entry->head, entry->tail->head);
  }

  MalVal *val = malval_map(map);
  map_release(map);
  return val;
}

static MalVal *assoc_vec(List *args, ENV *env)
{
  MalVal *vec = args->head;

  if ((list_count(args->tail) % 2) != 0)
    malthrow("must be even number of arguments");

  Vec *result = vec_duplicate(VAL_VEC(vec));
  unsigned c = vec_count(result);

  for (List *entry = args->tail; entry; entry = entry->tail->tail) {
    if (VAL_TYPE(entry->head) != TYPE_NUMBER) {
      vec_destroy(result);
      malthrow("require number as index to vec");
    }

    unsigned index = VAL_NUMBER(entry->head);

    if (index == c) {
      /* append */
      vec_append(result, entry->tail->head);
      c++;
    }
    else if (index >= 0 && index < c) {
      vec_update(result, index, entry->tail->head);
    }
    else {
      vec_destroy(result);
      malthrow("Index out of bounds");
    }

  }
  return malval_vector(result);
}

static MalVal *core_assoc(List *args, ENV *env)
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

static MalType types_dissoc[] = {TYPE_MAP, 0};
static MalVal *core_dissoc(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, ARGS_MAX, types_dissoc))
    return NIL;

  MalVal *map = args->head;
  List *keys = args->tail;
  Map *result = map_duplicate(VAL_MAP(map));

  for (List *key = keys; key; key = key->tail) {
    map_remove(result, key->head);
  }

  MalVal *rv = malval_map(result);
  map_release(result);
  return rv;
}

static MalVal *core_get(List *args, ENV *env)
{
  if (!builtins_args_check(args, 2, 3, NULL))
    return NIL;

  MalVal *not_found = args->tail->tail ? args->tail->tail->head : NIL;

  if (VAL_TYPE(args->head) == TYPE_NIL)
    return not_found;

  if (VAL_TYPE(args->head) == TYPE_SET) {
    Map *set = VAL_SET(args->head);
    MalVal *key = args->tail->head;
    MalVal *found = map_find(set, key);
    return found ? key : not_found;
  }

  if (VAL_TYPE(args->head) == TYPE_MAP) {
    Map *map = VAL_MAP(args->head);
    MalVal *key = args->tail->head;
    MalVal *found = map_find(map, key);
    return found ? found : not_found;
  }

  if (VAL_TYPE(args->head) == TYPE_VECTOR) {
    Vec *vec = VAL_VEC(args->head);
    MalVal *index = args->tail->head;
    if (VAL_TYPE(index) != TYPE_NUMBER) {
      exception = malval_string("Invalid index");
      return NIL;
    }
    if ((unsigned)VAL_NUMBER(index) >= vec_count(vec))
      return not_found;
    return vec_get(vec, VAL_NUMBER(index));
  }

  malthrow("get requires vec or map");
}

static MalVal *core_contains(List *args, ENV *env)
{
  if (!builtins_args_check(args, 2, 2, types_container))
    return NIL;

  if (VAL_TYPE(args->head) == TYPE_MAP) {
    return map_contains(VAL_MAP(args->head), args->tail->head) ? T : F;
  }

  if (VAL_TYPE(args->head) == TYPE_SET) {
    return map_contains(VAL_SET(args->head), args->tail->head) ? T : F;
  }

  if (VAL_TYPE(args->head) == TYPE_VECTOR) {
    Vec *vec = VAL_VEC(args->head);
    MalVal *index = args->tail->head;
    if (VAL_TYPE(index) != TYPE_NUMBER) {
      exception = malval_string("Invalid index");
      return NIL;
    }
    return VAL_NUMBER(index) >= 0 && (unsigned)VAL_NUMBER(index) < vec_count(vec) ? T : F;
  }

  malthrow("contains? requires vec or map");
}

static MalType types_hashmap[] = {TYPE_MAP, 0};
static MalVal *core_keys(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, types_hashmap))
    return NIL;

  return malval_list_weak(map_keys(VAL_MAP(args->head)));
}

static MalVal *core_vals(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, types_hashmap))
    return NIL;

  return malval_list_weak(map_values(VAL_MAP(args->head)));
}

static MalVal *core_readline(List *args, ENV *env)
{
  char buf[256];

  if (!builtins_args_check(args, 1, 1, types_string))
    return NIL;

  fputs(VAL_STRING(args->head), stdout);

#ifdef AGON_LIGHT
  char c = mos_editline(buf, sizeof(buf)-1, 0x05);
  fputs("\n", stdout);
  if (c == 27) {
    buf[0] = '\0';
  } else {
    /* need to add whitespace to terminate the token */
    int l = strlen(buf);
    buf[l] = ' ';
    buf[l+1] = '\0';
  }

  if (buf[0] == '\n' && buf[1] == '\0')
    return malval_string("");

  return malval_string(buf);
#else
  char *s;

  if ((s = fgets(buf, 255, stdin)) == NULL)
    return NIL;

  s[strlen(s)-1] = '\0';

  return malval_string(s);
#endif
}

static MalVal *core_time_ms(List *args, ENV *env)
{
  if (!builtins_args_check(args, 0, 0, NULL))
    return NIL;

#ifdef AGON_LIGHT
  return malval_number(10L * getsysvar_time());
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
}

static MalVal *core_is_fn(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return (VAL_TYPE(args->head) == TYPE_FUNCTION && VAL_FUNCTION(args->head)->is_macro == 0) ? T : F;
}

static MalVal *core_is_macro(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return (VAL_TYPE(args->head) == TYPE_FUNCTION && VAL_FUNCTION(args->head)->is_macro == 1) ? T : F;
}

static MalVal *core_is_string(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return VAL_TYPE(args->head) == TYPE_STRING ? T : F;
}

static MalVal *core_is_number(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return VAL_TYPE(args->head) == TYPE_NUMBER ? T : F;
}

static MalVal *core_seq(List *args, ENV *env)
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
      if (vec_count(VAL_VEC(args->head)) == 0)
        return NIL;
      result = list_from_vec(VAL_VEC(args->head));
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

  return malval_list_weak(result);
}

static MalVal *core_conj(List *args, ENV *env)
{
  if (!args)
    return malval_vector(NULL);

  if (VAL_IS_NIL(args->head))
    return malval_list(args->tail);

  if (VAL_TYPE(args->head) == TYPE_VECTOR) {
    Vec *result = vec_duplicate(VAL_VEC(args->head));
    for (List *arg = args->tail; arg; arg = arg->tail)
      vec_append(result, arg->head);
    return malval_vector(result);
  }

  if (VAL_TYPE(args->head) == TYPE_LIST) {
    List *result = list_acquire(VAL_LIST(args->head));

    for (List *arg = args->tail; arg; arg = arg->tail)
      result = cons_weak(arg->head, result);

    return malval_list_weak(result);
  }

  if (VAL_TYPE(args->head) == TYPE_SET) {
    Map *set = map_duplicate(VAL_SET(args->head));
    for (List *arg = args->tail; arg; arg = arg->tail)
      map_add(set, arg->head, NIL);

    MalVal *rv = malval_set(set);
    map_release(set);
    return rv;
  }

  if (VAL_TYPE(args->head) == TYPE_MAP) {
    Map *map = map_duplicate(VAL_MAP(args->head));

    for (List *arg = args->tail; arg; arg = arg->tail) {
      if (VAL_TYPE(arg->head) != TYPE_VECTOR) {
        map_release(map);
        malthrow("conj on map requires vectors as arguments");
      }
      Vec *l = VAL_VEC(arg->head);
      if (vec_count(l) != 2) {
        map_release(map);
        malthrow("vectors must have 2 items");
      }
      map_add(map, vec_get(l, 0), vec_get(l, 1));
    }

    MalVal *rv = malval_map(map);
    map_release(map);
    return rv;
  }

  exception = malval_string("not a container");
  return NIL;
}

static MalType types_disj[] = {TYPE_SET, 0};
static MalVal *core_disj(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, ARGS_MAX, types_disj))
    return NIL;

  Map *set = map_duplicate(VAL_SET(args->head));

  for (List *arg = args->tail; arg; arg = arg->tail)
    map_remove(set, arg->head);

  MalVal *rv = malval_set(set);
  map_release(set);
  return rv;
}

static MalVal *core_meta(List *args, ENV *env)
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

static MalVal *core_with_meta(List *args, ENV *env)
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
    result = malval_vector(vec_duplicate(VAL_VEC(args->head)));
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
    malthrow("Cannot set metadata on object");
  }

  return result;
}

static MalVal *core_hash(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, 1, NULL))
    return NIL;

  return malval_number(malval_hash(args->head));
}

#ifdef AGON_LIGHT
extern int __heaptop, __heapbot;
extern void *_alloc_base[2];
#endif
static MalVal *core_debug_info(List *args, ENV *env)
{
  if (!builtins_args_check(args, 0, 0, NULL))
    return NIL;

  unsigned count, size;
  Map *result = map_createN(2), *values;

  values = map_createN(2);
  value_info(&count, &size);
  map_add(values, malval_keyword(":count"), malval_number(count));
  map_add(values, malval_keyword(":size"), malval_number(size));
  map_add(result, malval_keyword(":values"), malval_map(values));
  map_release(values);

#ifndef NDEBUG
  values = map_createN(2);
  heap_info(&count, &size);
  map_add(values, malval_keyword(":count"), malval_number(count));
  map_add(values, malval_keyword(":used"), malval_number(size));
#ifdef AGON_LIGHT
  map_add(values, malval_keyword(":total"), malval_number((unsigned long)&__heaptop - (unsigned long)&__heapbot));
#endif
  map_add(result, malval_keyword(":heap"), malval_map(values));
  map_release(values);
#endif

#ifdef AGON_LIGHT
  printf("_alloc_base[0]=%p\n", _alloc_base[0]);
  printf("_alloc_base[1]=%p\n", _alloc_base[0]);
#endif

  MalVal *rv = malval_map(result);
  map_release(result);
  return rv;
}

struct ns core_ns[] = {
  {"+", plus},
  {"-", minus},
  {"*", multiply},
  {"/", divide},
  {"mod", core_mod},
  {"=", core_equals},
  {"<", lessthan},
  {"apply", core_apply},
  {"cons", core_cons},
  {"concat", core_concat},
  {"vec", core_vec},
  {"first", core_first},
  {"last", core_last},
  {"butlast", core_butlast},
  {"rest", core_rest},
  {"nth", core_nth},
  {"reverse", core_reverse},
  {"map", core_map},
  {"list", core_list},
  {"list?", core_is_list},
  {"empty?", core_is_empty},
  {"count", core_count},
  {"nil?", core_is_nil},
  {"true?", core_is_true},
  {"false?", core_is_false},
  {"byte", core_byte},
  {"int", core_int},
  {"float", core_float},
  {"symbol", core_symbol},
  {"symbol?", core_is_symbol},
  {"keyword", core_keyword},
  {"keyword?", core_is_keyword},
  {"vector", core_vector},
  {"vector?", core_is_vector},
  {"hash-map", core_hash_map},
  {"map?", core_is_map},
  {"assoc", core_assoc},
  {"dissoc", core_dissoc},
  {"get", core_get},
  {"contains?", core_contains},
  {"keys", core_keys},
  {"vals", core_vals},
  {"sequential?", core_is_sequential},
  {"pr-str", core_pr_str},
  {"str", core_str},
  {"prn", core_prn},
  {"println", core_println},
  {"gc", core_gc},
  {"read-string", core_read_string},
  {"slurp", core_slurp},
  {"throw", core_throw},
  /* atom stuff: */
  {"atom", core_atom},
  {"atom?", core_is_atom},
  {"deref", core_deref},
  {"reset!", core_reset},
  {"swap!", core_swap},

  {"readline", core_readline},

  {"time-ms", core_time_ms},

  {"meta", core_meta},
  {"with-meta", core_with_meta},
  {"conj", core_conj},
  {"disj", core_disj},

  {"fn?", core_is_fn},
  {"macro?", core_is_macro},
  {"string?", core_is_string},
  {"number?", core_is_number},
  {"seq", core_seq},

  {"hash", core_hash},

  {"debug-info", core_debug_info},

  {NULL, NULL},
};

