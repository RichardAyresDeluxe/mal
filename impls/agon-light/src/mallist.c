#include "mallist.h"
#include "heap.h"

MalList *cons(MalVal *val, MalList *list)
{
  MalList *c = heap_malloc(sizeof(MalList));
  c->next = list;
  c->ref_count = 1;
  c->value = val ? val : NIL;
  return c;
}

static void list_free(MalList *list)
{
  MalList *rover = list->next;
  heap_free(list);

  while (rover) {
    MalList *next = rover->next;

    if (rover->ref_count-- > 1)
      break;

    heap_free(rover);
    rover = next;
  }
}

void list_release(MalList *list)
{
  if (list && list->ref_count-- == 1)
    list_free(list);
}

void list_foreach(MalList *list, MalValProc p, void *data)
{
  for (MalList *rover = list; rover; rover = rover->next)
    p(rover->value, data);
}

unsigned list_count(MalList *list)
{
  unsigned count = 0;
  while(list) {
    count++;
    list = list->next;
  }
  return count;
}
