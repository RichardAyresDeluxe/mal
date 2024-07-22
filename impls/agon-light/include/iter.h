#ifndef _ITER_H
#define _ITER_H

#include "malval.h"

typedef struct Iterator Iterator;

extern Iterator *iter_create(MalVal *val);
extern void iter_destroy(Iterator *iter);
extern MalVal *iter_next(Iterator *iter);
extern void iter_foreach(Iterator *iter, MalValProc, void *);

#endif /* _ITER_H */
