#ifndef _MALLIST_H
#define _MALLIST_H

#include "malval.h"

typedef struct MalList {
  struct MalList *next;
  unsigned ref_count;
  struct MalVal *value;
} MalList;

MalList *cons(MalVal*, MalList*);
static inline MalList *mallist_acquire(MalList *l) {
  if (l)
    l->ref_count++;
  return l;
}
void mallist_release(MalList*);

void mallist_foreach(MalList *, MalValProc, void*);

#endif /* _MALLIST_H */
