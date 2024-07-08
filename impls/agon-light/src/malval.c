#include <string.h>

#include "malval.h"
#include "list.h"
#include "heap.h"
#include "gc.h"
#include "map.h"

/* not in C99 */
extern char *strdup(const char*);

MalVal *malval_create(uint8_t type)
{
  MalVal *val = heap_malloc(sizeof(MalVal));
  val->mark = 0;
  val->temp = 1;
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

MalVal *malval_list(List *list)
{
  MalVal *val = malval_create(TYPE_LIST);
  val->data.list = list_acquire(list);
  return val;
}

MalVal *malval_vector(List *list)
{
  MalVal *val = malval_create(TYPE_VECTOR);
  val->data.list = list_acquire(list);
  return val;
}

MalVal *malval_map(List *list)
{
  MalVal *val = malval_create(TYPE_MAP);
  val->data.list = list_acquire(list);
  return val;
}

MalVal *malval_number(int number)
{
  MalVal *val = malval_create(TYPE_NUMBER);
  val->data.number = number;
  return val;
}

MalVal *malval_function(FUNCTION *fn)
{
  MalVal *val = malval_create(TYPE_FUNCTION);
  val->data.fn = fn;
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
    case TYPE_LIST:
      list_release(val->data.list);
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
      for (List *rover = val->data.list; rover; rover = rover->tail) {
        sz += malval_size(rover->head, deep);
      }
      break;
  }

  return sz;
}

void malval_reset_temp(MalVal *val, void *data)
{
  if (!val)
    return;

  val->temp = 0;
  switch (val->type) {
    case TYPE_LIST:
      list_foreach(val->data.list, malval_reset_temp, data);
      break;
    case TYPE_VECTOR:
      list_foreach(val->data.vec, malval_reset_temp, data);
      break;
    case TYPE_MAP:
      list_foreach(val->data.map, malval_reset_temp, data);
      break;
  }
}
