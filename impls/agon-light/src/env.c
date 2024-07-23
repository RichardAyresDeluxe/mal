#include <string.h>

#include "env.h"
#include "heap.h"
#include "gc.h"
#include "map.h"
#include "err.h"
#include "vec.h"
#include "eval.h"
#include "iter.h"
#include "printer.h"

#include <stdio.h>
#include <assert.h>

struct ENV {
  struct ENV *prev, *next;
  struct Map *map;
  unsigned ref_count;
  struct ENV *parent;
};

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
    if (VAL_TYPE(bind->head) == TYPE_SYMBOL && VAL_STRING(bind->head)[0] == '&') {
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

  map_release(env->map);
  env->map = map_create();

  env_flush(env->parent);
}

void env_destroy(ENV *env)
{
  if (!env)
    return;

  map_release(env->map);
  env_release(env->parent);
  dlist_remove(env);
  heap_free(env);
}

static void destruct_bind_vec(ENV *env, MalVal *binds, MalVal *value)
{
  puts("destruct_bind_vec");
  Iterator *biter = iter_create(binds);
  if (!biter)
    return;

  Iterator *viter = iter_create(value);
  if (!viter) {
    iter_destroy(biter);
    return;
  }

  char kw_as[] = {-1, 'a', 's', '\0'};

  MalVal *b;
  while ((b = iter_next(biter)) != NULL) {
    if (VAL_TYPE(b) != TYPE_SYMBOL)
      continue;

    if (strcmp(VAL_STRING(b), kw_as) == 0) {
      MalVal *as = iter_next(biter);
      env_set(env, as, value);
    }
    else if (VAL_STRING(b)[0] == '&') {
      List *r = NULL;
      MalVal *v;
      while ((v = iter_next(viter)) != NULL)
        r = cons_weak(v, r);
      list_reverse(&r);
      env_set(env, malval_symbol(&VAL_STRING(b)[1]), malval_list_weak(r));
      break;
    }
    else {
      env_set(env, b, iter_next(viter));
    }
  }

  iter_destroy(viter);
  iter_destroy(biter);
}

struct map_destruct {
  ENV *env;
  MalVal *value;
  MalVal *defaults;
};

static char kw_as[] = {-1, 'a', 's', '\0'};
static char kw_or[] = {-1, 'o', 'r', '\0'};
static char kw_keys[] = {-1, 'k', 'e', 'y', 's', '\0'};
static void destruct_map_entry(MalVal *key, MalVal *val, void *_data)
{
  struct map_destruct *params = _data;

  if (VAL_IS_KEYWORD(key) && strcmp(VAL_STRING(key), kw_as) == 0) {
    /* set the symbol associated with ":as" to the whole value */
    if (VAL_TYPE(val) != TYPE_SYMBOL) {
      err_warning(ERR_ARGUMENT_MISMATCH, ":as must be followed by symbol");
      return;
    }
    env_set(params->env, val, params->value);
    return;
  }

  if (VAL_IS_KEYWORD(key) && strcmp(VAL_STRING(key), kw_keys) == 0) {
    /* process :keys, val contains a vector of symbols that 
     * will be set to entries in the map in params->value */
    if (VAL_TYPE(params->value) != TYPE_MAP) {
      exception = malval_string("parameter must be a map to use :keys");
      return;
    }

    Iterator *iter = iter_create(val);
    if (!iter) {
      exception = malval_string(":keys must associate with a vector or list of symbols");
      return;
    }

    MalVal *v;
    while ((v = iter_next(iter)) != NULL) {
      if (VAL_TYPE(v) != TYPE_SYMBOL) {
        iter_destroy(iter);
        exception = malval_string(":keys must associate with a vector or list of symbols");
        return;
      }

      MalVal *value = map_find(VAL_MAP(params->value), malval_keyword(VAL_STRING(v)));
      if (!value && params->defaults && VAL_TYPE(params->defaults) == TYPE_MAP) {
        /* Try to find in defaults */
        value = map_find(VAL_MAP(params->defaults), v);
      }
      if (!value)
        value = NIL;

      env_set(params->env, v, value);
    }

    iter_destroy(iter);
    return;
  }

  if (VAL_TYPE(key) == TYPE_SYMBOL && VAL_TYPE(params->value) == TYPE_MAP) {
    /* set symbol to named entry in map */
    MalVal *value = map_find(VAL_MAP(params->value), val);
    if (!value && params->defaults && VAL_TYPE(params->defaults) == TYPE_MAP) {
      value = map_find(VAL_MAP(params->defaults), key);
    }
    if (!value)
      value = NIL;
    env_set(params->env, key, value);
    return;
  }

  if (VAL_TYPE(key) == TYPE_SYMBOL && VAL_TYPE(params->value) == TYPE_VECTOR && VAL_TYPE(val) == TYPE_NUMBER) {
    /* set symbol to value at index in vector */
    MalVal *value = vec_get(VAL_VEC(params->value), VAL_NUMBER(val));
    env_set(params->env, key, value);
    return;
  }

  if (VAL_IS_KEYWORD(key) && strcmp(VAL_STRING(key), kw_or) == 0) {
    /* ignore :or, already handled */
    return;
  }

  exception = malval_string("Unexpected type destructuring map");
}

static void find_defaults(MalVal *key, MalVal *val, void *_defaults)
{
  MalVal **defaults = _defaults;

  if (VAL_IS_KEYWORD(key) && strcmp(VAL_STRING(key), kw_or) == 0)
    *defaults = val;
}

static void destruct_bind_map(ENV *env, MalVal *binds, MalVal *value)
{
  assert(VAL_TYPE(binds) == TYPE_MAP);

  struct map_destruct map_params = {env, value, NULL};

  map_foreach(VAL_MAP(binds), find_defaults, &map_params.defaults);
  map_foreach(VAL_MAP(binds), destruct_map_entry, &map_params);
}

void env_set(ENV *env, MalVal *key, MalVal *val)
{
  switch(VAL_TYPE(key)) {
    case TYPE_SYMBOL:
      map_add(env->map, key, val);
      break;

    case TYPE_VECTOR:
      destruct_bind_vec(env, key, val);
      break;

    case TYPE_MAP:
      destruct_bind_map(env, key, val);
      break;

    default:
      exception = malval_string("Cannot bind non-symbol");
      break;
  }
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
