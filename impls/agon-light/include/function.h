#ifndef _FUNCTION_H
#define _FUNCTION_H

#include "malval.h"
#include "list.h"
#include "env.h"

typedef struct MalVal *(builtin_fn)(List *args, ENV *env);

struct Body {
  struct Body *next;
  uint8_t arity;
  List *binds;
  MalVal *body;
};

#define MAX_ARITY ((1 << 8) - 1)

struct Function {
  ENV *env;
  uint8_t is_macro:1;
  uint8_t is_builtin:1;
  union {
    builtin_fn *builtin;
    struct Body *bodies;
  } fn;
};

typedef struct Function Function;

extern MalVal *function_create(List*, ENV*);
extern MalVal *function_create_builtin(builtin_fn*);
extern void function_destroy(Function*);
extern Function *function_duplicate(Function*);

extern uint16_t function_hash(Function*);

extern void function_gc_mark(Function*, void*);

extern MalVal *apply(Function*, List*);
extern struct Body *function_find_body(Function *func, List *args);

/**
 * Binds the given arguments to a function, returning a new environment
 * and setting a body pointer
 */
extern ENV *function_bind(Function *func, List *args, struct Body **);

#endif /* _FUNCTION_H */
