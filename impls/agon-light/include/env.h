#ifndef _ENV_H
#define _ENV_H

#include "malval.h"

struct ENV;
typedef struct ENV ENV;

ENV *env_create(ENV *parent);
void env_destroy(ENV*, bool delete_parent);
void env_set(ENV *env, const char *key, MalVal *val);
ENV *env_find(ENV *env, const char *key);
MalVal *env_get(ENV *env, const char *key);

#endif /* _ENV_H */
