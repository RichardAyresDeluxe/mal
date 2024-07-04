#include <string.h>

#include "env.h"
#include "heap.h"
#include "gc.h"
#include "map.h"

struct ENV {
  struct Map *map;
  ENV *parent;
};

ENV *env_create(ENV *parent)
{
  ENV *env = heap_malloc(sizeof(ENV));
  env->map = map_create();
  env->parent = parent;
  return env;
}

void env_destroy(ENV *env)
{
  if (!env)
    return;

  map_destroy(env->map);
  env_destroy(env->parent);
}

void env_add(ENV *env, const char *key, MalVal *val)
{
  map_add(env->map, key, val);
}

MalVal *env_find(ENV *env, const char *key)
{
  if (!env)
    return NULL;

  return map_find(env->map, key);
}

void gc_mark_env(struct ENV *env, void *data)
{
  if (!env)
    return;

  gc_mark_map(env->map, data);
  gc_mark_env(env->parent, data);
}
