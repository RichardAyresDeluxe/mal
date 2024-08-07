#include "list.h"
#include "heap.h"
#include "listsort.h"
#include "vec.h"
#include "hash.h"

#include <string.h>

List *cons_weak(MalVal *val, List *list)
{
  List *c = heap_malloc(sizeof(List));
  c->ref_count = 1;
  c->head = val ? val : NIL;
  c->tail = list;
  return c;
}

List *cons(MalVal *val, List *list)
{
  return cons_weak(val, list_acquire(list));
}

static void list_free(List *list)
{
  List *rover = list->tail;
  heap_free(list);

  while (rover) {
    List *next = rover->tail;

    if (rover->ref_count-- > 1)
      break;

    heap_free(rover);
    rover = next;
  }
}

void list_release(List *list)
{
  if (list && list->ref_count-- == 1)
    list_free(list);
}

void list_foreach(List *list, MalValProc p, void *data)
{
  for (List *rover = list; rover; rover = rover->tail)
    p(rover->head, data);
}

unsigned list_count(List *list)
{
  unsigned count = 0;
  while(list) {
    count++;
    list = list->tail;
  }
  return count;
}

MalVal *list_last(List *list)
{
  List *rover = list, *last = NULL;
  while (rover) {
    last = rover;
    rover = rover->tail;
  }
  return last ? last->head : NULL;
}

bool list_equals(List *_a, List *_b)
{
  List *a = _a, *b = _b;

  while (a && b) {
    if (!malval_equals(a->head, b->head))
      return FALSE;

    a = a->tail;
    b = b->tail;
  }

  if (a || b)
    return FALSE;

  return TRUE;
}

bool list_forall(List *list, predicate p, void *data)
{
  for (List *rover = list; rover; rover = rover->tail) {
    if (!p(rover->head, data))
      return FALSE;
  }
  return TRUE;
}

List *list_from_container(MalVal *val)
{
  switch(val->type) {
    case TYPE_LIST:
      return list_acquire(VAL_LIST(val));
    case TYPE_VECTOR:
      return list_from_vec(VAL_VEC(val));
  }
  return NULL;
}

List *list_from_string(const char *s)
{
  List *result = NULL;

  char c[2] = {0, 0};
  size_t l = strlen(s);

  while (l-- > 0) {
    c[0] = s[l];
    result = cons_weak(malval_string(c), result);
  }
  return result;
}

/* returns a new list */
List *list_concat(List *a, List *b)
{
  List *result = NULL;

  while (a) {
    result = cons_weak(a->head, result);
    a = a->tail;
  }

  while (b) {
    result = cons_weak(b->head, result);
    b = b->tail;
  }

  linked_list_reverse((void**)&result);

  return result;
}

List *list_duplicate(List *list)
{
  List *result = NULL;
  while(list) {
    result = cons_weak(list->head, result);
    list = list->tail;
  }
  linked_list_reverse((void**)&result);
  return result;
}

MalVal *list_nth(List *list, unsigned idx)
{
  while (list && idx-- > 0)
    list = list->tail;
  return list ? list->head : NULL;
}

uint16_t list_hash(List *list)
{
  uint16_t hv = HASH_INIT_LIST;
  list_foreach(list, hash_continue, &hv);
  return hv;
}

struct double_list {
  struct double_list *prev, *next;
};

void dlist_add(void *_list, void *_item)
{
  struct double_list *list = _list, *item = _item;
  item->prev = list->next->prev;
  item->next = list->next;
  list->next->prev = item;
  list->next = item;
}

void dlist_remove(void *_item)
{
  struct double_list *item = _item;
  item->prev->next = item->next;
  item->next->prev = item->prev;
}

void dlist_forall(void *_list, void (*f)(void *, void*), void *data)
{
  struct double_list *list = _list;
  for (struct double_list *rover = list->next; rover != list; rover = rover->next)
    f(rover, data);
}
