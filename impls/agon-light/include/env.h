#ifndef _ENV_H
#define _ENV_H

#include "malval.h"
#include "list.h"

struct ENV;
typedef struct ENV ENV;

ENV *env_create(ENV *parent, List *binds, List *exprs);
ENV *env_acquire(ENV*);
void env_release(ENV*);
void env_set(ENV *env, const char *key, MalVal *val);
ENV *env_find(ENV *env, const char *key);
MalVal *env_get(ENV *env, const char *key);

#endif /* _ENV_H */
