#include "function.h"
#include "env.h"
#include "heap.h"
#include "err.h"
#include "listsort.h"
#include "gc.h"
#include "eval.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

extern MalVal *eval_ast(MalVal *ast, ENV *env);

MalVal *function_create_builtin(builtin_fn *fn)
{
  Function *func = heap_malloc(sizeof(Function));
  func->env = NULL;
  func->is_builtin = 1;
  func->is_macro = 0;
  func->fn.builtin = fn;
  return malval_function(func);
}

static struct Body *create_body(List *list)
{
  MalVal *binds = list->head;

  List *lbinds;

  if (VAL_TYPE(binds) == TYPE_VECTOR) {
    lbinds = binds->data.vec;
  }
  else if (VAL_TYPE(binds) == TYPE_LIST) {
    lbinds = binds->data.list;
  }
  else {
    err_warning(ERR_ARGUMENT_MISMATCH, "function bindings must be a vector or list");
    return NULL;
  }

  for (List *bind = lbinds; bind; bind = bind->tail) {
    if (VAL_TYPE(bind->head) != TYPE_SYMBOL) {
      err_warning(ERR_ARGUMENT_MISMATCH, "function bindings must be symbols");
      return NULL;
    }
  }

  unsigned nbinds = list_count(lbinds);
  if (nbinds > MAX_ARITY) {
    err_warning(ERR_ARGUMENT_MISMATCH, "function arity too high");
    return NULL;
  }

  struct Body *body = heap_malloc(sizeof(struct Body));
  body->next = NULL;
  body->binds = list_acquire(lbinds);
  body->arity = nbinds;
  body->body = list->tail->head;
  body->is_variadic = 0;

  MalVal *last = list_last(body->binds);
  if (last && last->data.string[0] == '&') {
    body->is_variadic = 1;
    /* lose the '&' in the symbol name */
    unsigned c = strlen(last->data.string);
    memmove(last->data.string, &last->data.string[1], c);
  }

  return body;
}

static int8_t arity_comp(void *_a, void *_b, void *data)
{
  struct Body *a = _a, *b = _b;
  return a->arity - b->arity;
}

static bool is_symbol(MalVal *val, void *data)
{
  return VAL_TYPE(val) == TYPE_SYMBOL;
}

static bool is_single_body(List *body)
{
  unsigned c = list_count(body);

  if (c == 1)
    return FALSE;
  if (c > 2)
    return FALSE;
  /* so do we have a list of bindings then a body, or
   * a vector of bindings then a body,
   * or two bodies */
  if (VAL_TYPE(body->head) == TYPE_VECTOR)
    return TRUE;
  if (VAL_TYPE(body->head) == TYPE_LIST && list_forall(body->head->data.list, is_symbol, NULL))
    return TRUE;
  return FALSE;
}

MalVal *function_create(List *body, ENV *env)
{
  Function *func = heap_malloc(sizeof(Function));
  func->env = env_acquire(env); //env_create(env, NULL, NULL);
  func->is_builtin = 0;

  if (is_single_body(body)) {
    /* single body */
    func->fn.bodies = create_body(body);
  }
  else {
    /* multi-arity function */
    func->fn.bodies = NULL;
    for (List *rover = body; rover; rover = rover->tail) {
      if (VAL_TYPE(rover->head) != TYPE_LIST) {
        err_warning(ERR_ARGUMENT_MISMATCH, "function body is not list");
        function_destroy(func);
        return NIL;
      }
      struct Body *b = create_body(rover->head->data.list);
      if (!b) {
        function_destroy(func);
        return NIL;
      }
      b->next = func->fn.bodies;
      func->fn.bodies = b;
    }

    /* make sure bodies are sorted by increasing arity */
    linked_list_sort_raw((void**)&func->fn.bodies, arity_comp, NULL);

    /* count number of variadic bodies, should be 0 or 1 */
    unsigned nvariadic = 0;
    struct Body *last = NULL;
    for (struct Body *b = func->fn.bodies; b; b = b->next) {
      if (b->is_variadic)
        nvariadic++;
      if (last && b->arity == last->arity) {
        err_warning(ERR_ARGUMENT_MISMATCH, "every body must have unique arity");
        function_destroy(func);
        return NIL;
      }
      last = b;
    }

    if (nvariadic > 1) {
      err_warning(ERR_ARGUMENT_MISMATCH, "only 0 or 1 variadic bodies allowed");
      function_destroy(func);
      return NIL;
    }
    
    // if (last && nvariadic == 1 && list_last(last->binds)->data.string[0] != '&') {
    //   err_warning(ERR_ARGUMENT_MISMATCH, "variadic body must have highest arity");
    //   function_destroy(func);
    //   return NIL;
    // }
  }

  return malval_function(func);
}

void function_destroy(Function *func)
{
  env_release(func->env);

  if (!func->is_builtin) {
    struct Body *rover = func->fn.bodies;
    while (rover) {
      struct Body *next = rover->next;
      list_release(rover->binds);
      heap_free(rover);
      rover = next;
    }
  }

  heap_free(func);
}

struct Body *function_find_body(Function *func, List *args)
{
  unsigned argc = list_count(args);

  assert(!func->is_builtin);

  for (struct Body *b = func->fn.bodies; b; b = b->next) {
    if (b->arity == argc && !b->is_variadic) {
      return b;
    }
    else if (b->is_variadic) {
      /* If variadic, then we must be at the end, and all lower
       * arity bodies have come before */
      return b;
    }
  }

  return NULL;
}

ENV *function_bind(Function *func, List *args, struct Body **body)
{
  *body = function_find_body(func, args);
  if (!*body) {
    return NULL;
  }

  if (!body[0]->is_variadic) {
    return env_create(func->env, body[0]->binds, args);
  }

  /* variadic */
  unsigned bindc = list_count(body[0]->binds);
  List *p = NULL;
  for (unsigned i = 0; i < (bindc-1); i++) {
    p = cons(args->head, p);
    args = args->tail;
  }

  p = cons(malval_list(args), p);
  linked_list_reverse((void**)&p);
  ENV *env = env_create(func->env, body[0]->binds, p);
  list_release(p);
  return env;
}

MalVal *apply(Function *func, List *args)
{
  if (func->is_builtin)
    return func->fn.builtin(args, func->env);

  struct Body *b = NULL;
  ENV *env = function_bind(func, args, &b);

  if (b == NULL) {
    err_warning(ERR_ARGUMENT_MISMATCH, "function arity mismatch");
    env_release(env);
    return NIL;
  }

  MalVal *rv = EVAL(b->body, env);
  env_release(env);

  return rv ? rv : NIL;
}

void function_gc_mark(Function *fn, void *data)
{
  gc_mark_env(fn->env, data);

  if (fn->is_builtin)
    return;

  for (struct Body *body = fn->fn.bodies; body; body = body->next) {
    gc_mark_list(body->binds, data);
    gc_mark(body->body, data);
  }
}
