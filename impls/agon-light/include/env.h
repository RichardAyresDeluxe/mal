#ifndef _ENV_H
#define _ENV_H

#include "malval.h"
#include "list.h"

typedef struct ENV ENV;

ENV *env_create(ENV *parent, List *binds, List *exprs);
ENV *env_acquire(ENV*);
void env_release(ENV*);
void env_flush(ENV*);
void env_set(ENV *env, MalVal *key, MalVal *val);
ENV *env_find(ENV *env, MalVal *key);
MalVal *env_get(ENV *env, MalVal *key);
extern void env_mark_all(void);

/** Call function on all environments */
extern void env_for_each(void (*)(ENV*));

#endif /* _ENV_H */
