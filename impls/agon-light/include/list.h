#ifndef _list_H
#define _list_H

#include "malval.h"

typedef struct List {
  struct List *tail;
  unsigned ref_count;
  struct MalVal *head;
} List;

extern List *cons(MalVal*, List*);
static inline List *list_acquire(List *l) {
  if (l)
    l->ref_count++;
  return l;
}
extern void list_release(List*);
extern void list_foreach(List *, MalValProc, void*);
extern unsigned list_count(List *list);

#endif /* _list_H */
