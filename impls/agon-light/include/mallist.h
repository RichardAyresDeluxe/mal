#ifndef _list_H
#define _list_H

#include "malval.h"

typedef struct MalList {
  struct MalList *next;
  unsigned ref_count;
  struct MalVal *value;
} MalList;

extern MalList *cons(MalVal*, MalList*);
static inline MalList *list_acquire(MalList *l) {
  if (l)
    l->ref_count++;
  return l;
}
extern void list_release(MalList*);
extern void list_foreach(MalList *, MalValProc, void*);
extern unsigned list_count(MalList *list);

#endif /* _list_H */
