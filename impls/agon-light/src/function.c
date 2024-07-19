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

Function *function_duplicate(Function *in)
{
  Function *out = heap_malloc(sizeof(Function));
  out->env = env_acquire(in->env);
  out->is_builtin = in->is_builtin;
  out->is_macro = in->is_macro;
  if (in->is_builtin) {
    out->fn.builtin = in->fn.builtin;
    return out;
  }

  struct Body *bodies = NULL;
  for (struct Body *bin = in->fn.bodies; bin; bin = bin->next) {
    struct Body *bout = heap_malloc(sizeof(struct Body));
    bout->arity = bin->arity;
    bout->binds = list_duplicate(bin->binds);
    bout->body = bin->body;
    bout->next = bodies;
    bodies = bout;
  }
  linked_list_reverse((void**)&bodies);
  out->fn.bodies = bodies;

  return out;
}

static struct Body *create_body(List *list);
static bool is_single_body(List *body);
static int8_t arity_comp(void *_a, void *_b, void *data);

MalVal *function_create(List *body, ENV *env)
{
  MalVal *doc = NULL;
  Function *func = heap_malloc(sizeof(Function));
  func->env = env_acquire(env); //env_create(env, NULL, NULL);
  func->is_builtin = 0;
  func->is_macro = 0;

  if (body && VAL_TYPE(body->head) == TYPE_STRING) {
    doc = body->head;
    body = body->tail;
  }

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
      struct Body *b = create_body(VAL_LIST(rover->head));
      if (!b) {
        function_destroy(func);
        return NIL;
      }
      b->next = func->fn.bodies;
      func->fn.bodies = b;
    }

    /* make sure bodies are sorted by increasing arity */
    linked_list_sort_raw((void**)&func->fn.bodies, arity_comp, NULL);
  }

  MalVal *rv = malval_function(func);
  if (doc) {
    List *doclist = cons_weak(malval_keyword(":doc"), cons_weak(doc, NULL));
    rv->data.fn->meta = malval_map(doclist);
    list_release(doclist);
  }
  return rv;
}

static struct Body *create_body(List *list)
{
  MalVal *binds = list->head;

  List *lbinds;

  if (VAL_TYPE(binds) == TYPE_VECTOR) {
    lbinds = VAL_VEC(binds);
  }
  else if (VAL_TYPE(binds) == TYPE_LIST) {
    lbinds = VAL_LIST(binds);
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
  if (VAL_TYPE(body->head) == TYPE_LIST && list_forall(VAL_LIST(body->head), is_symbol, NULL))
    return TRUE;
  return FALSE;
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

  struct Body *b;
  for (b = func->fn.bodies; b && b->next; b = b->next) {
    if (b->arity == argc)
      break;
  }

  if (b->arity > argc) {
    MalVal *last = list_last(b->binds);
    assert(VAL_TYPE(last) == TYPE_SYMBOL);
    if (last->data.string[0] != '&')
      return NULL;
  }

  return b;
}

ENV *function_bind(Function *func, List *args, struct Body **body)
{
  *body = function_find_body(func, args);
  if (!*body) {
    return NULL;
  }

  return env_create(func->env, body[0]->binds, args);
}

MalVal *apply(Function *func, List *args)
{
  if (func->is_builtin)
    return func->fn.builtin(args, func->env);

  struct Body *b = NULL;
  ENV *env = function_bind(func, args, &b);
  env_set(env, malval_symbol("__args"), malval_list(args));

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
  if (fn->is_builtin)
    return;

  for (struct Body *body = fn->fn.bodies; body; body = body->next) {
    gc_mark_list(body->binds, data);
    gc_mark(body->body, data);
  }
}

uint16_t function_hash(Function *f)
{
  unsigned hv = 97;

  if (f->is_builtin)
    return (hv * (unsigned long)(f->fn.builtin)) % 65521;

  for (struct Body *body = f->fn.bodies; body; body = body->next) {
    hv = (hv * 29 + list_hash(body->binds)) % 65521;
    hv = (hv * 23 + malval_hash(body->body)) % 65521;
  }

  return hv;
}
