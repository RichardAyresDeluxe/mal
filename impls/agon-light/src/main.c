#include "malval.h"
#include "list.h"
#include "vec.h"
#include "gc.h"
#include "reader.h"
#include "printer.h"
#include "env.h"
#include "err.h"
#include "listsort.h"
#include "heap.h"
#include "function.h"
#include "core.h"
#include "eval.h"
#include "function.h"
#include "str.h"
#include "itoa.h"
#include "map.h"
#include "iter.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <alloca.h>

/* REPL */
ENV *repl_env = NULL;

/* Global exception value */
MalVal *exception = NULL;


static void warn_symbol_not_found(const char *name)
{
  unsigned l = strlen(name);
  l += 7; //strlen("symbol ");
  l += 10; //strlen(" not found");
  char *s = alloca(l + 1);
  strcpy(s, "symbol ");
  strcat(s, name);
  strcat(s, " not found");
  exception = malval_string(s);
}

struct map_eval_t {
  ENV *env;
  Map *out;
  int i;
};

static void _evaluate_map(MalVal *key, MalVal *val, void *_data)
{
  struct map_eval_t *ev = _data;
  if (exception) {
    return;
  }
  val = EVAL(val, ev->env);
  char buf[24] = "__val_";
  itoa(ev->i++, &buf[6], 10);
  env_set(ev->env, malval_symbol(buf), val);

  map_add(ev->out, EVAL(key, ev->env), val);
}

static Map *evaluate_map(Map *ast, ENV *env)
{
  Map *out = map_createN(map_count(ast));

  struct map_eval_t ev = {env, out, 0};

  map_foreach(ast, _evaluate_map, &ev);

  return out;
}

MalVal *eval_ast(MalVal *ast, ENV *env)
{
  if (ast->type == TYPE_SYMBOL)
  {
    MalVal *value = NULL;
    if (ast->data.string[0] == -1) {
      /* is a keyword */
      value = ast;
    }
    else
      value = env_get(env, ast);

    if (value)
      return value;

    unsigned l = 1 +  // "'"
                 strlen(ast->data.string) +
                 11 + // "' not found"
                 1;
    char *s = alloca(l);
    strcpy(s, "'");
    strcat(s, ast->data.string);
    strcat(s, "' not found");
    exception = malval_string(s);

    return ast;
  }

  if (ast->type == TYPE_LIST)
  {
    List *evaluated = NULL;
    int i = 0;
    ENV *tmp = env_create(env, NULL, NULL);
    env_set(tmp, malval_symbol("__list"), ast);
    char buf[24];
    for (List *rover = VAL_LIST(ast); rover; rover = rover->tail) {
      MalVal *val = EVAL(rover->head, env);
      if (exception) {
        env_release(tmp);
        list_release(evaluated);
        return ast;
      }
      evaluated = cons_weak(val, evaluated);
      env_set(tmp, malval_symbol(itoa(i++, buf, 10)), val);
    }
    env_release(tmp);
    linked_list_reverse((void**)&evaluated);
    return malval_list_weak(evaluated);
  }

  if (ast->type == TYPE_VECTOR)
  {
    Vec *evaluated = vec_create();
    ENV *tmp = env_create(env, NULL, NULL);
    char buf[24];
    env_set(tmp, malval_symbol("__vec"), ast);
    for (int i = 0; i < vec_count(VAL_VEC(ast)); i++) {
      MalVal *val = EVAL(vec_get(VAL_VEC(ast), i), env);
      if (exception) {
        env_release(tmp);
        vec_destroy(evaluated);
        return ast;
      }
      vec_append(evaluated, val);
      env_set(tmp, malval_symbol(itoa(i, buf, 10)), val);
    }
    env_release(tmp);
    return malval_vector(evaluated);
  }

  if (ast->type == TYPE_MAP)
  {
    Map *evaluated = evaluate_map(VAL_MAP(ast), env);
    MalVal *value = malval_map(evaluated);
    map_release(evaluated);
    return value;
  }

  return ast;
}

static MalVal *EVAL_def(List *list, ENV *env)
{
  if (list_count(list) != 2) {
    err_warning(ERR_ARGUMENT_MISMATCH, "def! requires two arguments");
    return NIL;
  }

  char *name = alloca(strlen(VAL_STRING(list->head)) + 1);
  strcpy(name, VAL_STRING(list->head));

  ENV *tmp = env_create(env, NULL, NULL);
  env_set(tmp, malval_symbol("__body"), malval_list(list));

  MalVal *value = EVAL(list->tail->head, env);
  env_release(tmp);
  if (exception) {
    return NIL;
  }
  if (!value) {
    // warn_symbol_not_found(name);
    return NIL;
  }

  env_set(env, malval_symbol(name), value);

  return value;
}

static MalVal *EVAL_defmacro(List *list, ENV *env)
{
  if (list_count(list) != 2) {
    err_warning(ERR_ARGUMENT_MISMATCH, "defmacro! requires two arguments");
    return NIL;
  }

  char *name = alloca(strlen(VAL_STRING(list->head)) + 1);
  strcpy(name, VAL_STRING(list->head));

  ENV *tmp = env_create(env, NULL, NULL);
  env_set(tmp, malval_symbol("__body"), malval_list(list));

  MalVal *value = EVAL(list->tail->head, env);
  env_release(tmp);
  if (exception) {
    return NIL;
  }
  if (!value) {
    warn_symbol_not_found(name);
    return NIL;
  }

  if (VAL_TYPE(value) != TYPE_FUNCTION) {
    malthrow("not a function in defmacro!");
  }

  MalVal *copy = malval_function(function_duplicate(VAL_FUNCTION(value)));
  VAL_FUNCTION(copy)->is_macro = 1;
  env_set(env, malval_symbol(name), copy);
  return copy;
}

/* Returns the function if this is a macro call, otherwise NULL */
static Function *is_macro_call(MalVal *ast, ENV *env)
{
  if (VAL_TYPE(ast) != TYPE_LIST)
    return NULL;

  if (VAL_LIST(ast) == NULL || VAL_TYPE(VAL_LIST(ast)->head) != TYPE_SYMBOL)
    return NULL;
  
  MalVal *val = env_get(env, VAL_LIST(ast)->head);

  if (!val)
    return NULL;

  if (VAL_TYPE(val) != TYPE_FUNCTION)
    return NULL;

  if (!VAL_FUNCTION(val)->is_macro)
    return NULL;

  return VAL_FUNCTION(val);
}

static MalVal *macroexpand(MalVal *ast, ENV *env)
{
  Function *macro = NULL;
  while  ((macro = is_macro_call(ast, env)) != NULL) {
    ast = apply(macro, VAL_LIST(ast)->tail);
  }
  return ast;
}

static MalVal *EVAL_fn_star(List *list, ENV *env)
{
  return function_create(list, env);
}

static void EVAL_do(List *list, ENV *env, MalVal **out)
{
  ENV *tmp = env_create(env, NULL, NULL);
  List *rover = list;
  while (rover && rover->tail) {
    env_set(tmp, malval_symbol("__body"), rover->head);
    EVAL(rover->head, env);
    if (exception) {
      env_release(tmp);
      return;
    }
    rover = rover->tail;
  }
  env_release(tmp);

  *out = rover ? rover->head : NIL;
}

static void EVAL_if(List *list, ENV *env, MalVal **out)
{
  if (!list || !list->tail) {
    err_warning(ERR_ARGUMENT_MISMATCH, "need at least 2 arguments to if");
    *out = NIL;
    return;
  }

  ENV *tmp = env_create(env, NULL, NULL);
  env_set(tmp, malval_symbol("__body"), malval_list(list));

  MalVal *val = EVAL(list->head, env);
  env_release(tmp);
  if (exception) {
    return;
  }
  if (VAL_IS_NIL(val) || VAL_IS_FALSE(val)) {
    /* false */
    *out = list->tail->tail ? list->tail->tail->head : NIL;
  }
  else {
    /* true */
    *out = list->tail->head;
  }
}

static void EVAL_let(List *list, ENV *env, MalVal **out, ENV **envout)
{
  if (list_count(list) != 2) {
    err_warning(ERR_ARGUMENT_MISMATCH, "let* requires two arguments");
    *out = NIL;
    return;
  }

  ENV *let = env_create(env, NULL, NULL);

  Iterator *iter = iter_create(list->head);
  if (!iter)
    return;

  /* We will put all our un-evaluated bodies in here 
   * so that they do not get garbage collected during evaluation
   * of bindings */
  env_set(let, malval_symbol("__list"), malval_list(list));

  MalVal *sym;
  while ((sym = iter_next(iter)) != NULL) {
    MalVal *val = iter_next(iter);
    assert(val != NULL);

    if (VAL_TYPE(sym) != TYPE_SYMBOL) {
      err_warning(ERR_ARGUMENT_MISMATCH, "can only bind to symbols");
      env_release(let);
      iter_destroy(iter);
      *out = NIL;
      return;
    }

    env_set(let, sym, val);
  }

  iter_reset(iter);
  while ((sym = iter_next(iter)) != NULL) {
    MalVal *val = EVAL(iter_next(iter), let);
    if (exception) {
      env_release(let);
      *out = NIL;
      return;
    }
    env_set(let, sym, val);
  }
  iter_destroy(iter);

  assert(list->tail != NULL);
  *out = list->tail->head;
  env_release(*envout);
  *envout = let;
}

static void EVAL_call(Function *fn, List *args, ENV *env, MalVal **out, ENV **envout)
{
  /*
   * TCO mal function call
   */
  struct Body *b = NULL;
  ENV *new_env = function_bind(fn, args, &b);
  if (b == NULL) {
    err_warning(ERR_ARGUMENT_MISMATCH, "function arity mismatch");
    env_release(new_env);
    *out = NIL;
    return;
  }

  *out = b->body;
  env_release(*envout);
  *envout = new_env;
}

static MalVal *EVAL_quasiquote(MalVal *ast, ENV* env);

static MalVal *EVAL_quasiquote_list(List *elt, ENV *env)
{
  List *result = NULL;

  ENV *tmp = env_create(env, NULL, NULL);
  env_set(tmp, malval_symbol("__elt"), malval_list(elt));

  for (; elt; elt = elt->tail) {
    if (VAL_TYPE(elt->head) == TYPE_LIST
     && !list_is_empty(VAL_LIST(elt->head))
     && VAL_TYPE(VAL_LIST(elt->head)->head) == TYPE_SYMBOL
     && strcmp(VAL_LIST(elt->head)->head->data.string, "splice-unquote") == 0
    ) {
      result = cons_weak(malval_symbol("concat"),
                         cons_weak(VAL_LIST(elt->head)->tail->head,
                                   cons_weak(malval_list_weak(result), NULL)));
    }
    else {
      MalVal *val = EVAL_quasiquote(elt->head, env);
      if (exception) {
        list_release(result);
        return NIL;
      }
      result = cons_weak(malval_symbol("cons"),
                         cons_weak(val,
                                   cons_weak(malval_list_weak(result), NULL)));
    }
  }
  env_release(tmp);

  return malval_list_weak(result);
}

static MalVal *EVAL_quasiquote(MalVal *ast, ENV *env)
{
  if (VAL_TYPE(ast) == TYPE_LIST
   && !list_is_empty(VAL_LIST(ast))
   && VAL_TYPE(VAL_LIST(ast)->head) == TYPE_SYMBOL
   && strcmp(VAL_LIST(ast)->head->data.string, "unquote") == 0)
  {
    /*   - If `ast` is a list starting with the "unquote" symbol, return its
     *       second element. */
    return VAL_LIST(ast)->tail->head;
  }

  if (VAL_TYPE(ast) == TYPE_LIST) {
    List *list = list_duplicate(VAL_LIST(ast));
    linked_list_reverse((void**)&list);

    ENV *tmp = env_create(env, NULL, NULL);
    env_set(tmp, malval_symbol("__list"), malval_list(list));
    MalVal *rv = EVAL_quasiquote_list(list, env);
    env_release(tmp);

    if (exception) {
      list_release(list);
      return NIL;
    }
    list_release(list);
    return rv;
  }

  if (VAL_TYPE(ast) == TYPE_VECTOR) {
    List *list = list_from_container(ast);
    linked_list_reverse((void**)&list);

    ENV *tmp = env_create(env, NULL, NULL);
    env_set(tmp, malval_symbol("__list"), malval_list(list));
    MalVal *val = EVAL_quasiquote_list(list, env);
    env_release(tmp);

    if (exception) {
      list_release(list);
      return NIL;
    }
    return malval_list_weak(cons_weak(malval_symbol("vec"),
                            cons_weak(val, NULL)));
  }

  if (VAL_TYPE(ast) == TYPE_MAP || VAL_TYPE(ast) == TYPE_SYMBOL) {
    return malval_list_weak(cons_weak(malval_symbol("quote"),
                            cons_weak(ast, NULL)));
  }

  return ast;
}

static MalVal *EVAL_try(List *body, ENV *env)
{
  ENV *tmp = env_create(env, NULL, NULL);
  env_set(tmp, malval_symbol("__try"), malval_list(body));
  MalVal *result = EVAL(body->head, env);
  env_release(tmp);
  if (!exception) /* no exception */
    return result;

  /* we have an exception */

  if (!body->tail)
    return NIL; /* but no catch */

  MalVal *catch = body->tail->head;

  if (VAL_TYPE(catch) != TYPE_LIST
   || list_count(VAL_LIST(catch)) != 3
   || VAL_TYPE(VAL_LIST(catch)->head) != TYPE_SYMBOL
   || strcmp(VAL_LIST(catch)->head->data.string, "catch*") != 0
   || VAL_TYPE(VAL_LIST(catch)->tail->head) != TYPE_SYMBOL
  ) {
    err_warning(ERR_ARGUMENT_MISMATCH, "invalid catch block");
    return NIL;
  }

  /* we have an exception */
  List binds = {NULL, 1, VAL_LIST(catch)->tail->head};
  List exc = {NULL, 1, exception};
  ENV *env2 = env_create(env, &binds, &exc);
  exception = NULL;
  env_set(env2, malval_symbol("__catch"), catch);
  result = EVAL(VAL_LIST(catch)->tail->tail->head, env2);
  env_release(env2);

  return result;
}

static void EVAL_kw_function(MalVal *kw, List *args, MalVal** ast)
{
  List *kwl = cons_weak(kw, NULL);
  List *result = cons_weak(malval_symbol("get"),
                   list_concat(args, kwl));

  *ast = malval_list(result);

  list_release(kwl);
  list_release(result);
}

static void EVAL_recur(List *args, ENV *env, MalVal **ast, ENV **envout)
{
  MalVal *fn = env_get(env, malval_symbol("__recur_target"));
  if (!fn) {
    exception = malval_string("recur not inside loop");
    return;
  }

  if (VAL_TYPE(fn) == TYPE_SYMBOL)
    fn = env_get(env, fn);

  if (!fn || VAL_TYPE(fn) != TYPE_FUNCTION) {
    exception = malval_string("cannot find function to recur");
    return;
  }

  MalVal *evaluated = eval_ast(malval_list(args), env);
  assert(VAL_TYPE(evaluated) == TYPE_LIST);

  EVAL_call(VAL_FUNCTION(fn), VAL_LIST(evaluated), env, ast, envout);
}

MalVal *EVAL(MalVal *ast, ENV *env)
{
  if (exception) {
    return ast;
  }

  env_acquire(env);

  while (TRUE)
  {
    if (VAL_TYPE(ast) != TYPE_LIST) {
      MalVal *rv = eval_ast(ast, env);
      env_release(env);
      return rv;
    }
    if (VAL_LIST(ast) == NULL) {
      env_release(env);
      return ast; /* empty list */
    }

    ast = macroexpand(ast, env);

    if (VAL_TYPE(ast) != TYPE_LIST) {
      MalVal *rv = eval_ast(ast, env);
      env_release(env);
      return rv;
    }

    List *list = VAL_LIST(ast);

    MalVal *head = list->head;
    List *tail = list->tail;

    if (head->type == TYPE_SYMBOL) {

      if (VAL_IS_KEYWORD(head)) {
        /* We have a keyword as the first item - rearrange so it's a get */
        EVAL_kw_function(head, tail, &ast);
        if (exception) {
          env_release(env);
          return NIL;
        }
        continue;
      }

      /* check for special forms */
      const char *symbol = head->data.string;
      if (symbol[0] == 'd') {
        if (strcmp(symbol, "def!") == 0) {
          MalVal *rv = EVAL_def(tail, env);
          env_release(env);
          return rv;
        }
        if (strcmp(symbol, "defmacro!") == 0) {
          MalVal *rv = EVAL_defmacro(tail, env);
          env_release(env);
          return rv;
        }
        if (strcmp(symbol, "do") == 0) {
          EVAL_do(tail, env, &ast);
          if (exception) {
            env_release(env);
            return NIL;
          }
          continue;
        }
      }
      if (symbol[0] == 'q') {
        if (strcmp(symbol, "quote") == 0) {
          env_release(env);
          return tail->head;
        }
        if (strcmp(symbol, "quasiquote") == 0) {
          ast = EVAL_quasiquote(tail->head, env);
          if (exception) {
            env_release(env);
            return NIL;
          }
          continue;
        }
        if (strcmp(symbol, "quasiquoteexpand") == 0) {
          return EVAL_quasiquote(tail->head, env);
        }
      }
      if (strcmp(symbol, "let*") == 0) {
        EVAL_let(tail, env, &ast, &env);
        if (exception) {
          env_release(env);
          return NIL;
        }
        continue;
      }
      if (strcmp(symbol, "fn*") == 0) {
        return EVAL_fn_star(tail, env);
      }
      if (strcmp(symbol, "if") == 0) {
        EVAL_if(tail, env, &ast);
        if (exception) {
          env_release(env);
          return NIL;
        }
        continue;
      }
      if (strcmp(symbol, "recur") == 0) {
        EVAL_recur(tail, env, &ast, &env);
        if (exception) {
          env_release(env);
          return NIL;
        }
        continue;
      }
      if (strcmp(symbol, "macroexpand") == 0) {
        MalVal *rv = macroexpand(tail->head, env);
        env_release(env);
        return rv;
      }

      if (strcmp(symbol, "try*") == 0) {
        MalVal *rv = EVAL_try(tail, env);
        env_release(env);
        return rv;
      }

      if (strcmp(symbol, "comment") == 0) {
        env_release(env);
        return NIL;
      }
    }

    /* otherwise treat as if it's a function call */

    MalVal *f = eval_ast(ast, env);
    if (!f)
        return NULL;

    if (exception) {
      env_release(env);
      return NIL;
    }

    if (VAL_IS_NIL(f))
      return NIL;

    assert(f->type == TYPE_LIST);
    assert(f->data.list != NULL);

    if (VAL_IS_NIL(VAL_LIST(f)->head))
      return NULL;

    if (VAL_TYPE(VAL_LIST(f)->head) != TYPE_FUNCTION)
      return VAL_LIST(f)->head;

    Function *fn = VAL_FUNCTION(VAL_LIST(f)->head);
    List *args = VAL_LIST(f)->tail;

    if (fn->is_builtin) {
      MalVal *rv = fn->fn.builtin(args, env);
      env_release(env);
      return rv;
    }

    EVAL_call(fn, args, env, &ast, &env);

    if (exception) {
      env_release(env);
      return NIL;
    }

    gc(FALSE);
  }
}

MalVal *READ(char *s)
{
  if (s == NULL) {
    return read_str();
  }
  else {
    return read_string(s);
  }
}

char *PRINT(MalVal *val)
{
  if (!val)
    return strdup("null");

  return pr_str(val, TRUE);
}

char *rep(ENV *repl_env, char *s)
{
  MalVal *val = EVAL(READ(s), repl_env);
  if (exception) {
    fputs("Exception: ", stdout);
    char *x = pr_str(exception, TRUE);
    fputs(x, stdout);
    fputs("\n", stdout);
    heap_free(x);
    exception = NULL;
    return PRINT(NULL);
  }
  return PRINT(val);
}

static void cleanup(void)
{
  env_for_each(env_flush);
  env_for_each(env_flush);

  gc_add(_nil);
  gc_add(_true);
  gc_add(_false);

  gc(TRUE);
  gc(TRUE);

  env_release(repl_env);
  repl_env = NULL;

  gc(TRUE);
  gc(TRUE);


#ifndef NDEBUG
  unsigned count, size;
  value_info(&count, &size);
  fprintf(stderr, "Values remaining: %u (%u bytes)\n", count, size);

  heap_info(&count, &size);
  fprintf(stderr, "Heap remainig: %u items (%u bytes)\n", count, size);
#endif
}

static MalVal *builtin_eval(List *args, ENV *ignored)
{
  if (!args) {
    err_warning(ERR_ARGUMENT_MISMATCH, "need one argument");
    return NIL;
  }

  return EVAL(args->head, repl_env);
}


static MalVal *builtin_load_file(List *args, ENV *env)
{
  if (!args || args->tail)
    malthrow("load-file requires one argument");

  load_file(VAL_STRING(args->head), repl_env);

  return NIL;
}

static void build_env(void)
{
  repl_env = env_create(NULL, NULL, NULL);

  for (struct ns *ns = core_ns; ns->name != NULL; ns++) {
    env_set(repl_env, malval_symbol(ns->name), function_create_builtin(ns->fn));
  }

  env_set(repl_env, malval_symbol("nil"), _nil);
  env_set(repl_env, malval_symbol("true"), _true);
  env_set(repl_env, malval_symbol("false"), _false);
  env_set(repl_env, malval_symbol("eval"), function_create_builtin(builtin_eval));
  env_set(repl_env, malval_symbol("load-file"), function_create_builtin(builtin_load_file));
  env_set(repl_env, malval_symbol("*ARGV*"), malval_list(NULL));
  env_set(repl_env, malval_symbol("*host-language*"), malval_string("agon-light"));
}

extern void __fpurge(FILE*);

int main(int argc, char **argv)
{
  _nil = malval_nil();
  _true = malval_bool(TRUE);
  _false = malval_bool(FALSE);
  gc_pop();
  gc_pop();
  gc_pop();

  /* Largely to keep valgrind happy */
  atexit(cleanup);

  build_env();

  load_file("init.mal", repl_env);

  int arg = 1;

  while (arg < argc) {
    if (argv[arg][0] == '\0') {
      arg++;
      continue;
    }

    List *args = NULL;

    char *input = strdup("(load-file \"");
    catstr(&input, argv[arg++]);
    catstr(&input, "\")");

    for (; arg < argc; arg++)
      args = cons_weak(malval_string(argv[arg]), args);

    linked_list_reverse((void**)&args);
    env_set(repl_env, malval_symbol("*ARGV*"), malval_list_weak(args));

    char *s = rep(repl_env, input);
    heap_free(input);
    heap_free(s);
    exit(0);
  }

  heap_free(rep(repl_env, "(println (str \"Mal [\" *host-language* \"]\"))"));

  while (1) {
    char *s = rep(repl_env, NULL);
    if (!s) {
      exit(0);
    }
    __fpurge(stdout);
    fputs(s, stdout);
    fflush(stdout);
    fputs("\n", stdout);
    fflush(stdout);
    heap_free(s);
    gc(TRUE);
#ifndef NDEBUG
    s = rep(repl_env, "(debug-info)");
    puts(s);
    heap_free(s);
#endif
  }
}

int fputs(const char *s, FILE *stream)
{
  int c = 0;
  while (s && *s != '\0') {
    fputc(*s++, stream);
    c++;
  }
  return c;
}
