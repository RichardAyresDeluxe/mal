#include <string.h>

#include "env.h"
#include "heap.h"
#include "gc.h"
#include "map.h"
#include "err.h"

static void env_destroy(ENV *env);

static ENV envs = {&envs, &envs, NULL, 0, NULL};

static void mark_env(ENV *env)
{
  gc_mark_env(env, NULL);
}

typedef void (*env_op)(ENV*);

static void do_env_op(void *env, void *_op)
{
  env_op *op = _op;
  (*op)(env);
}

void env_for_each(void (*op)(ENV*))
{
  dlist_forall(&envs, do_env_op, &op);
}

void env_mark_all(void)
{
  env_for_each(mark_env);
}

ENV *env_create(ENV *parent, List *binds, List *values)
{
  ENV *env = heap_malloc(sizeof(ENV));
  dlist_add(&envs, env);
  env->map = map_createN(parent ? list_count(binds) : 251);
  env->ref_count = 1;
  env->parent = env_acquire(parent);
  
  List *bind, *value;
  for (bind = binds, value = values; bind && value; bind = bind->tail, value = value->tail) {
    if (bind->head->type != TYPE_SYMBOL) {
      err_warning(ERR_ARGUMENT_MISMATCH, "Cannot not bind non-symbol");
      continue;
    }

    if (bind->head->data.string[0] == '&') {
      /* variadic, so set to the remaining values and stop binding */
      env_set(env, malval_symbol(&bind->head->data.string[1]), malval_list(value));
      break;
    }

    env_set(env, bind->head, value->head);
  }

  if (bind && !value && bind->head->data.string[0] == '&') {
    /* have a variadic binding, but no more values - make empty list */
    env_set(env, malval_symbol(&bind->head->data.string[1]), malval_list(NULL));
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

void env_flush(ENV *env)
{
  if (!env)
    return;

  map_destroy(env->map);
  env->map = map_create();

  env_flush(env->parent);
}

void env_destroy(ENV *env)
{
  if (!env)
    return;

  map_destroy(env->map);
  env_release(env->parent);
  dlist_remove(env);
  heap_free(env);
}

void env_set(ENV *env, MalVal *key, MalVal *val)
{
  map_add(env->map, key, val);
}

ENV *env_find(ENV *env, MalVal *key)
{
  if (!env)
    return NULL;

  if (map_find(env->map, key))
    return env;

  return env_find(env->parent, key);
}


MalVal *env_get(ENV *env, MalVal *key)
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

  gc_mark_map(env->map, NULL);
}
