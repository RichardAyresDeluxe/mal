#include "malval.h"
#include "list.h"
#include "heap.h"
#include "gc.h"
#include "function.h"
#include "err.h"
#include "str.h"
#include "eval.h"

#include <string.h>
#include <alloca.h>


MalVal *_nil, *_true, *_false;

MalVal *malval_create(MalType type)
{
  MalVal *val = heap_malloc(sizeof(MalVal));
  val->mark = 0;
  val->type = type;
  gc_add(val);
  return val;
}

MalVal *malval_symbol(const char *s)
{
  MalVal *val = malval_create(TYPE_SYMBOL);
  val->data.string = strdup(s);
  return val;
}

MalVal *malval_keyword(const char *s)
{
  char *buf;

  if (s[0] == -1)
    return malval_symbol(s);

  if (s[0] == ':') {
    buf = alloca(strlen(s) + 1);
    strcpy(buf, s);
    buf[0] = -1;
  }
  else {
    buf = alloca(strlen(s) + 2);
    buf[0] = -1;
    strcpy(&buf[1], s);
  }

  return malval_symbol(buf);
}

MalVal *malval_string(const char *s)
{
  MalVal *val = malval_create(TYPE_STRING);
  val->data.string = strdup(s);
  return val;
}

MalVal *malval_list_weak(List *list)
{
  MalVal *val = malval_create(TYPE_LIST);
  val->data.list = heap_malloc(sizeof(struct ListWithMeta));
  val->data.list->list = list;
  val->data.list->meta = NIL;
  return val;
}

MalVal *malval_list(List *list)
{
  return malval_list_weak(list_acquire(list));
}

MalVal *malval_vector(List *list)
{
  MalVal *val = malval_create(TYPE_VECTOR);
  val->data.vec = heap_malloc(sizeof(struct VecWithMeta));
  val->data.vec->vec = list_acquire(list);
  val->data.vec->meta = NIL;
  return val;
}

MalVal *malval_map(List *list)
{
  MalVal *val = malval_create(TYPE_MAP);
  val->data.map = heap_malloc(sizeof(struct MapWithMeta));
  val->data.map->map = list_acquire(list);
  val->data.map->meta = NIL;
  return val;
}

MalVal *malval_function(Function *fn)
{
  MalVal *val = malval_create(TYPE_FUNCTION);
  val->data.fn = heap_malloc(sizeof(struct FunctionWithMeta));
  val->data.fn->fn = fn;
  val->data.fn->meta = NIL;
  return val;
}

MalVal *malval_number(int number)
{
  MalVal *val = malval_create(TYPE_NUMBER);
  val->data.number = number;
  return val;
}

MalVal *malval_atom(struct MalVal *v)
{
  MalVal *val = malval_create(TYPE_ATOM);
  val->data.atom = v;
  return val;
}

MalVal *malval_bool(bool b)
{
  MalVal *val = malval_create(TYPE_BOOL);
  val->data.bool = b;
  return val;
}

void malval_free(MalVal *val)
{
  switch(val->type) {
    case TYPE_STRING:
    case TYPE_SYMBOL:
      heap_free(val->data.string);
      break;
    case TYPE_MAP:
      list_release(val->data.map->map);
      heap_free(val->data.map);
      break;
    case TYPE_VECTOR:
      list_release(val->data.vec->vec);
      heap_free(val->data.vec);
      break;
    case TYPE_LIST:
      list_release(val->data.list->list);
      heap_free(val->data.list);
      break;
    case TYPE_FUNCTION:
      function_destroy(val->data.fn->fn);
      heap_free(val->data.fn);
      break;
  }

  heap_free(val);
}

unsigned malval_size(MalVal *val, bool deep)
{
  unsigned sz = sizeof(MalVal);

  if (!deep)
    return sz;

  switch(val->type) {
    case TYPE_LIST:
      for (List *rover = VAL_LIST(val); rover; rover = rover->tail) {
        sz += malval_size(rover->head, deep);
      }
      break;
    case TYPE_MAP:
      for (List *rover = VAL_MAP(val); rover; rover = rover->tail) {
        sz += malval_size(rover->head, deep);
      }
      break;
    case TYPE_VECTOR:
      for (List *rover = VAL_VEC(val); rover; rover = rover->tail) {
        sz += malval_size(rover->head, deep);
      }
      break;
    case TYPE_ATOM:
      sz += malval_size(VAL_ATOM(val), deep);
      break;
  }

  return sz;
}

MalVal *map_get(List *map, MalVal *key)
{
  for (List *entry = map; entry && entry->tail; entry = entry->tail->tail) {
    if (malval_equals(key, entry->head))
      return entry->tail->head;
  }
  return NULL;;
}

List *map_normalise(List *map)
{
  List *result = NULL;

  for (List *entry = map; entry && entry->tail; entry = entry->tail->tail) {
    if (!map_contains(result, entry->head)) {
      result = cons_weak(entry->head, cons_weak(entry->tail->head, result));
    }
  }

  return result;
}

static bool map_equals(List *_a, List *_b)
{
  bool rv = FALSE;
  List *a = map_normalise(_a);
  List *b = map_normalise(_b);

  if (list_count(a) != list_count(b)) {
    goto done;
  }

  for (List *entry = a; entry && entry->tail; entry = entry->tail->tail) {
    if (!malval_equals(entry->tail->head, map_get(b, entry->head))) {
      goto done;
    }
  }
  rv = TRUE;

done:
  list_release(a);
  list_release(b);
  return rv;
}

bool malval_equals(MalVal *a, MalVal *b)
{
  if (VAL_TYPE(a) != VAL_TYPE(b)) {

    if (VAL_TYPE(a) == TYPE_LIST && VAL_TYPE(b) == TYPE_VECTOR)
      return list_equals(VAL_LIST(a), VAL_VEC(b));

    if (VAL_TYPE(b) == TYPE_LIST && VAL_TYPE(a) == TYPE_VECTOR)
      return list_equals(VAL_LIST(b), VAL_VEC(a));

    return FALSE;
  }

  switch (VAL_TYPE(a)) {
    case TYPE_NIL:
      return TRUE;
    case TYPE_NUMBER:
      return a->data.number == b->data.number;
    case TYPE_BOOL:
      return a->data.bool == b->data.bool;
    case TYPE_STRING:
    case TYPE_SYMBOL:
      if (a->data.string[0] == -1)
        return strcmp(&a->data.string[1], &b->data.string[1]) == 0;
      else
        return strcmp(a->data.string, b->data.string) == 0;
    case TYPE_LIST:
      return list_equals(VAL_LIST(a), VAL_LIST(b));
    case TYPE_VECTOR:
      return list_equals(VAL_VEC(a), VAL_VEC(b));
    case TYPE_MAP:
      return map_equals(VAL_MAP(a), VAL_MAP(b));

    case TYPE_ATOM:
      return malval_equals(a->data.atom, b->data.atom);

    default:
      err_warning(ERR_ARGUMENT_MISMATCH, "unknown type");
      break;
  }

  return FALSE;
}

#define HASH_INIT_STRING  53
#define HASH_INIT_SYMBOL  59
#define HASH_INIT_VECTOR  37
#define HASH_INIT_MAP     41
#define HASH_INIT_ATOM    61

static uint16_t string_hash(unsigned p, const char *s)
{
  unsigned m = 65521;  // highest prime inside 16 bits
  unsigned hv = 0;
  unsigned p_pow = 1;
  for (const char *c = s; *c; c++) {
    hv = (hv + (*c - 'a' + 1) * p_pow) % m;
    p_pow = (p_pow * (uint32_t)p) % m;
  }

  return (uint16_t)hv;
}

#define vec_hash(vec) ((HASH_INIT_VECTOR * list_hash(vec)) % 65521)
#define map_hash(map) ((HASH_INIT_MAP * list_hash(map)) % 65521)

uint16_t malval_hash(MalVal *val)
{
  switch(VAL_TYPE(val)) {
    case TYPE_NIL:
      return 0;
    case TYPE_STRING:
      return string_hash(HASH_INIT_STRING, VAL_STRING(val));
    case TYPE_SYMBOL:
      return string_hash(HASH_INIT_SYMBOL, VAL_STRING(val));
    case TYPE_NUMBER:
      return (251L * VAL_NUMBER(val)) % 65521;
    case TYPE_LIST:
      return list_hash(VAL_LIST(val));
    case TYPE_VECTOR:
      return vec_hash(VAL_VEC(val));
    case TYPE_MAP:
      return map_hash(VAL_MAP(val));
    case TYPE_BOOL:
      return (VAL_IS_FALSE(val) ? 7 * 31 : 7 * 47);
    case TYPE_ATOM:
      return (malval_hash(VAL_ATOM(val)) * HASH_INIT_ATOM) % 65521;
    case TYPE_FUNCTION:
      return function_hash(VAL_FUNCTION(val));
    default:
      exception = malval_string("Cannot calculate hash of object");
      return 0;
  }
}
