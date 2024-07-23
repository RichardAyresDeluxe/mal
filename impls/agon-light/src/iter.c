#include "list.h"
#include "vec.h"
#include "iter.h"
#include "heap.h"
#include "eval.h"

struct Iterator {
  MalType type;
  union {
    struct {
      Vec *vec;
      unsigned index;
    } v;
    struct {
      List *list;
      List *rover;
    } l;
    struct {
      char *string;
      unsigned index;
    } s;
  } c;
};

Iterator *iter_create(MalVal *val)
{
  Iterator *it = heap_malloc(sizeof(Iterator));
  it->type = VAL_TYPE(val);

  switch(VAL_TYPE(val)) {
    case TYPE_LIST:
      it->c.l.list = it->c.l.rover = list_acquire(VAL_LIST(val));
      break;
    case TYPE_VECTOR:
      it->c.v.vec = VAL_VEC(val);
      it->c.v.index = 0;
      break;
    case TYPE_STRING:
    case TYPE_SYMBOL:
      it->c.s.string = VAL_STRING(val);
      it->c.s.index = 0;
      break;
    default:
      heap_free(it);
      exception = malval_string("Invalid container for iter()");
      return NULL;
  }

  return it;
}

void iter_reset(Iterator *iter)
{
  switch(iter->type) {
  case TYPE_LIST:
    iter->c.l.rover = iter->c.l.list;
    break;
  case TYPE_VECTOR:
    iter->c.v.index = 0;
    break;
  case TYPE_SYMBOL:
  case TYPE_STRING:
    iter->c.s.index = 0;
    break;
  }
}

void iter_destroy(Iterator *iter)
{
  if (iter->type == TYPE_LIST)
    list_release(iter->c.l.list);
  heap_free(iter);
}

MalVal *iter_next(Iterator *iter)
{
  MalVal *rv = NULL;

  switch(iter->type) {
  case TYPE_LIST:
    if (!iter->c.l.rover)
      return NULL;
    rv = iter->c.l.rover->head;
    iter->c.l.rover = iter->c.l.rover->tail;
    break;
  case TYPE_VECTOR:
    if (iter->c.v.index >= vec_count(iter->c.v.vec))
      return NULL;
    rv = vec_get(iter->c.v.vec, iter->c.v.index);
    iter->c.v.index++;
    break;
  case TYPE_STRING:
  case TYPE_SYMBOL: {
    char s[2] = {0, 0};
    if (iter->c.s.string[iter->c.s.index] == '\0')
        return NULL;
    s[0] = iter->c.s.string[iter->c.s.index++];
    rv = malval_string(s);
    break;
  }
  default:
    exception = malval_string("Invalid iterator");
    break;
  }

  return rv;
}

void iter_foreach(Iterator *iter, MalValProc p, void *data)
{
  MalVal *val;
  while ((val = iter_next(iter)) != NULL) {
    p(val, data);
  }
}
