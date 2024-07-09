#ifndef _list_H
#define _list_H

#include <stddef.h>

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

extern MalVal *list_last(List*);

extern bool list_equals(List*, List*);

typedef bool (*predicate)(MalVal*, void*);
extern bool list_forall(List*, predicate, void*);

/** This is here for type-safety */
static inline bool list_is_empty(List *l) {
  return l == NULL;
}

#endif /* _list_H */
