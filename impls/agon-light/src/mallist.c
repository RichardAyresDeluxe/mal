#include "mallist.h"
#include "heap.h"

MalList *cons(MalVal *val, MalList *list)
{
  MalList *c = heap_malloc(sizeof(MalList));
  c->next = list;
  c->ref_count = 1;
  c->value = val;
  return c;
}

static void mallist_free(MalList *list)
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

void mallist_release(MalList *list)
{
  if (list && list->ref_count-- == 1)
    mallist_free(list);
}

void mallist_foreach(MalList *list, MalValProc p, void *data)
{
  for (MalList *rover = list; rover; rover = rover->next)
    p(rover->value, data);
}
