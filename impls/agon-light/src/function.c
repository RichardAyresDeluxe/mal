#include "function.h"
#include "env.h"
#include "heap.h"
#include "err.h"
#include "listsort.h"
#include "gc.h"

#include <assert.h>

extern MalVal *eval_ast(MalVal *ast, ENV *env);

struct Body {
  struct Body *next;
  uint8_t arity:7;
  uint8_t is_variadic:1;
  List *binds;
  MalVal *body;
};

struct Function {
  ENV *env;
  uint8_t is_macro:1;
  uint8_t is_builtin:1;
  union {
    builtin_fn *builtin;
    struct Body *bodies;
  } fn;
};

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

  if (VAL_TYPE(binds) != TYPE_VECTOR) {
    err_warning(ERR_ARGUMENT_MISMATCH, "function bindings must be a vector");
    return NULL;
  }
  for (List *bind = binds->data.vec; bind; bind = bind->tail) {
    if (VAL_TYPE(bind->head) != TYPE_SYMBOL) {
      err_warning(ERR_ARGUMENT_MISMATCH, "function bindings must be symbols");
      return NULL;
    }
  }

  struct Body *body = heap_malloc(sizeof(struct Body));
  body->next = NULL;
  body->binds = list_acquire(binds->data.vec);
  body->arity = list_count(body->binds);
  body->body = malval_list(list->tail);

  MalVal *last = list_last(body->binds);
  body->is_variadic = last && last->data.string[0] == '&';

  return body;
}

static int8_t arity_comp(void *_a, void *_b, void *data)
{
  struct Body *a = _a, *b = _b;
  return a->arity - b->arity;
}

MalVal *function_create(List *body, ENV *env)
{
  Function *func = heap_malloc(sizeof(Function));
  func->env = env; //env_create(env, NULL, NULL);
  func->is_builtin = 0;

  if (VAL_TYPE(body->head) == TYPE_VECTOR) {
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
    
    if (last && nvariadic == 1 && list_last(last->binds)->data.string[0] != '&') {
      err_warning(ERR_ARGUMENT_MISMATCH, "variadic body must have highest arity");
      function_destroy(func);
      return NIL;
    }
  }

  return malval_function(func);
}

void function_destroy(Function *func)
{
  // env_destroy(func->env, FALSE);

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

MalVal *apply(Function *func, List *args)
{
  if (func->is_builtin)
    return func->fn.builtin(args, func->env);

  MalVal *rv = NULL;
  unsigned argc = list_count(args);

  struct Body *b = func->fn.bodies;
  while(b) {
    if (b->arity == argc && !b->is_variadic) {
      ENV *env = env_create(func->env, b->binds, args);
      rv = eval_ast(b->body, env);
      assert(VAL_TYPE(rv) == TYPE_LIST);
      rv = rv->data.list->head;
      env_destroy(env, FALSE);
      break;
    }
    else if (b->arity > argc && b->is_variadic) {
      err_fatal(ERR_NOT_IMPLEMENTED, "variadic functions NIY");
    }

    b = b->next;
  }

  if (rv == NULL && b == NULL) {
    err_warning(ERR_ARGUMENT_MISMATCH, "function arity mismatch");
    return NIL;
  }

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
