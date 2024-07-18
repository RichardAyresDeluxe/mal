#ifndef _list_H
#define _list_H

#include <stddef.h>

#include "malval.h"
#include "listsort.h"

typedef struct List {
  struct List *tail;
  unsigned ref_count;
  struct MalVal *head;
} List;

extern List *cons(MalVal*, List*);

/** Like cons, but don't increase the tail's ref_count */
extern List *cons_weak(MalVal*, List*);

static inline List *list_acquire(List *l) {
  if (l)
    l->ref_count++;
  return l;
}
extern void list_release(List*);
extern void list_foreach(List *, MalValProc, void*);
extern unsigned list_count(List *list);
extern List *list_duplicate(List *);

extern MalVal *list_last(List*);

extern bool list_equals(List*, List*);

typedef bool (*predicate)(MalVal*, void*);
extern bool list_forall(List*, predicate, void*);

/** This is here for type-safety */
static inline bool list_is_empty(List *l) {
  return l == NULL;
}

extern List *list_from_container(MalVal*);
extern List *list_from_string(const char *);

/* returns a new list */
extern List *list_concat(List *, List *);

static inline List *list_reverse(List **list) {
  linked_list_reverse((void**)list);
  return *list;
}

extern MalVal *list_nth(List *, unsigned);
extern uint16_t list_hash(List *list);

/** Call the function p on every item in a double-linked list */
extern void dlist_forall(void *list, void (*p)(void *, void*), void*);
extern void dlist_add(void *list, void *item);
extern void dlist_remove(void *item);

#endif /* _list_H */
