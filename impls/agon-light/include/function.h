#ifndef _FUNCTION_H
#define _FUNCTION_H

#include "malval.h"
#include "list.h"
#include "env.h"

struct Function;
typedef struct Function Function;

typedef struct MalVal *(builtin_fn)(List *args, ENV *env);

extern MalVal *function_create(List*, ENV*);
extern MalVal *function_create_builtin(builtin_fn*);
extern void function_destroy(Function*);

extern void function_gc_mark(Function*, void*);

extern MalVal *apply(Function*, List*);

#endif /* _FUNCTION_H */
