#ifndef _ENV_H
#define _ENV_H

#include "malval.h"

struct ENV;
typedef struct ENV ENV;

ENV *env_create(ENV *parent);
void env_destroy(ENV*);
void env_add(ENV *env, const char *key, MalVal *val);
MalVal *env_find(ENV *env, const char *key);

#endif /* _ENV_H */
