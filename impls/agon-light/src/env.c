#include <string.h>

#include "env.h"
#include "heap.h"
#include "gc.h"
#include "map.h"
#include "err.h"

struct ENV {
  struct Map *map;
  unsigned ref_count;
  ENV *parent;
};

static void env_destroy(ENV *env);

ENV *env_create(ENV *parent, List *binds, List *values)
{
  ENV *env = heap_malloc(sizeof(ENV));
  env->map = map_create();
  env->ref_count = 1;
  env->parent = env_acquire(parent);
  
  List *bind, *value;
  for (bind = binds, value = values; bind && value; bind = bind->tail, value = value->tail) {
    if (bind->head->type != TYPE_SYMBOL) {
      err_warning(ERR_ARGUMENT_MISMATCH, "Cannot not bind non-symbol");
      continue;
    }
    env_set(env, bind->head->data.string, value->head);
  }

  return env;
}

ENV *env_acquire(ENV *env)
{
  if (env)
    env->ref_count++;
  return env;
}

void env_release(ENV *env)
{
  if (env && env->ref_count-- == 1) {
    env_destroy(env);
  }
}

void env_destroy(ENV *env)
{
  if (!env)
    return;

  map_destroy(env->map);
  env_release(env->parent);
}

void env_set(ENV *env, const char *key, MalVal *val)
{
  malval_reset_temp(val, NULL);
  map_add(env->map, key, val);
}

ENV *env_find(ENV *env, const char *key)
{
  if (!env)
    return NULL;

  if (map_find(env->map, key))
    return env;

  return env_find(env->parent, key);
}


MalVal *env_get(ENV *env, const char *key)
{
  if (!env)
    return NULL;

  MalVal *value = map_find(env->map, key);
  if (value)
    return value;

  return env_get(env->parent, key);
}


void gc_mark_env(struct ENV *env, void *data)
{
  if (!env)
    return;

  gc_mark_map(env->map, data);
  gc_mark_env(env->parent, data);
}
