#include "list.h"
#include "heap.h"

List *cons(MalVal *val, List *list)
{
  List *c = heap_malloc(sizeof(List));
  c->ref_count = 1;
  c->head = val ? val : NIL;
  c->tail = list_acquire(list);
  return c;
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
