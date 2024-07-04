#include <string.h>

#include "malval.h"
#include "mallist.h"
#include "heap.h"
#include "gc.h"

/* not in C99 */
extern char *strdup(const char*);

MalVal *malval_create(uint8_t type)
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

MalVal *malval_list(MalList *list)
{
  MalVal *val = malval_create(TYPE_LIST);
  val->data.list = mallist_acquire(list);
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
      mallist_release(val->data.list);
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
      for (MalList *rover = val->data.list; rover; rover = rover->next) {
        sz += malval_size(rover->value, deep);
      }
      break;
  }

  return sz;
}
