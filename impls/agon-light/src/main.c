#include "malval.h"
#include "list.h"
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
  char *s = heap_malloc(l + 1);
  strcpy(s, "symbol ");
  strcat(s, name);
  strcat(s, " not found");
  err_warning(ERR_SYMBOL_NOT_FOUND, s);
  heap_free(s);
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
      value = env_get(env, ast->data.string);

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
    for (List *rover = VAL_LIST(ast); rover; rover = rover->tail) {
      MalVal *val = EVAL(rover->head, env);
      if (exception) {
        list_release(evaluated);
        return ast;
      }
      evaluated = cons_weak(val, evaluated);
    }
    linked_list_reverse((void**)&evaluated);
    MalVal *value = malval_list(evaluated);
    list_release(evaluated);
    return value;
  }

  if (ast->type == TYPE_VECTOR)
  {
    List *evaluated = NULL;
    for (List *rover = VAL_LIST(ast); rover; rover = rover->tail) {
      MalVal *val = EVAL(rover->head, env);
      if (exception) {
        list_release(evaluated);
        return ast;
      }
      evaluated = cons_weak(val, evaluated);
    }
    linked_list_reverse((void**)&evaluated);
    MalVal *value = malval_vector(evaluated);
    list_release(evaluated);
    return value;
  }

  if (ast->type == TYPE_MAP)
  {
    List *evaluated = NULL;
    unsigned i = 0;
    for (List *rover = VAL_LIST(ast); rover; rover = rover->tail) {
      if ((i++ % 2) == 0) {
        /* even - a key, don't evaluate */
        evaluated = cons_weak(rover->head, evaluated);
      }
      else {
        /* odd - a value, evaluate */
        MalVal *val = EVAL(rover->head, env);
        if (exception) {
          list_release(evaluated);
          return ast;
        }
        evaluated = cons_weak(val, evaluated);
      }
    }
    linked_list_reverse((void**)&evaluated);
    MalVal *value = malval_map(evaluated);
    list_release(evaluated);
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

  const char *name = list->head->data.string;

  MalVal *value = EVAL(list->tail->head, env);
  if (exception) {
    malval_reset_temp(value, NULL);
    return NIL;
  }
  if (!value) {
    // warn_symbol_not_found(name);
    return NIL;
  }

  env_set(env, name, value);

  return value;
}

static MalVal *EVAL_defmacro(List *list, ENV *env)
{
  if (list_count(list) != 2) {
    err_warning(ERR_ARGUMENT_MISMATCH, "defmacro! requires two arguments");
    return NIL;
  }

  const char *name = VAL_STRING(list->head);

  MalVal *value = EVAL(list->tail->head, env);
  if (exception) {
    malval_reset_temp(value, NULL);
    return NIL;
  }
  if (!value) {
    warn_symbol_not_found(name);
    return NIL;
  }

  if (VAL_TYPE(value) != TYPE_FUNCTION) {
    malval_reset_temp(value, NULL);
    err_warning(ERR_ARGUMENT_MISMATCH, "not a function in defmacro!");
    return NIL;
  }

  MalVal *copy = malval_function(function_duplicate(VAL_FUNCTION(value)));

  VAL_FUNCTION(copy)->is_macro = 1;

  env_set(env, name, copy);

  return copy;
}

/* Returns the function if this is a macro call, otherwise NULL */
static Function *is_macro_call(MalVal *ast, ENV *env)
{
  if (VAL_TYPE(ast) != TYPE_LIST)
    return NULL;

  if (VAL_LIST(ast) == NULL || VAL_TYPE(VAL_LIST(ast)->head) != TYPE_SYMBOL)
    return NULL;
  
  MalVal *val = env_get(env, VAL_LIST(ast)->head->data.string);

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
  List *rover = list;
  while (rover && rover->tail) {
    MalVal *val = EVAL(rover->head, env);
    malval_reset_temp(val, NULL);
    if (exception) {
      return;
    }
    rover = rover->tail;
  }

  *out = rover ? rover->head : NIL;
}

static void EVAL_if(List *list, ENV *env, MalVal **out)
{
  if (!list || !list->tail) {
    err_warning(ERR_ARGUMENT_MISMATCH, "need at least 2 arguments to if");
    *out = NIL;
    return;
  }

  MalVal *val = EVAL(list->head, env);
  if (exception) {
    malval_reset_temp(val, NULL);
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

  if (list->head->type != TYPE_LIST && list->head->type != TYPE_VECTOR) {
    err_warning(ERR_ARGUMENT_MISMATCH, "let* bindings argument must be list or vector");
    env_release(let);
    *out = NIL;
    return;
  }

  List *bindings = VAL_LIST(list->head);
  if ((list_count(bindings) % 2) != 0) {
    err_warning(ERR_ARGUMENT_MISMATCH, "let* bindings must have even number of entries");
    env_release(let);
    *out = NIL;
    return;
  }

  for (List *rover = bindings;
       rover && rover->tail;
       rover = rover->tail->tail)
  {
    if (rover->head->type != TYPE_SYMBOL) {
      err_warning(ERR_ARGUMENT_MISMATCH, "can only bind to symbols");
      env_release(let);
      *out = NIL;
      return;
    }
    MalVal *val = EVAL(rover->tail->head, let);
    if (exception) {
      env_release(let);
      malval_reset_temp(val, NULL);
      *out = NIL;
      return;
    }
    env_set(let, rover->head->data.string, val);
  }

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

static MalVal *EVAL_quasiquote(MalVal *ast);

static MalVal *EVAL_quasiquote_list(List *elt)
{
  List *result = NULL;
  for (; elt; elt = elt->tail) {
    if (VAL_TYPE(elt->head) == TYPE_LIST
     && !list_is_empty(VAL_LIST(elt->head))
     && VAL_TYPE(VAL_LIST(elt->head)->head) == TYPE_SYMBOL
     && strcmp(VAL_LIST(elt->head)->head->data.string, "splice-unquote") == 0
    ) {
      result = cons_weak(malval_symbol("concat"),
                         cons_weak(VAL_LIST(elt->head)->tail->head,
                                   cons_weak(malval_list(result), NULL)));
    }
    else {
      MalVal *val = EVAL_quasiquote(elt->head);
      if (exception) {
        list_release(result);
        malval_reset_temp(val, NULL);
        return NIL;
      }
      result = cons_weak(malval_symbol("cons"),
                         cons_weak(val,
                                   cons_weak(malval_list(result), NULL)));
    }
  }

  return malval_list(result);
}

static MalVal *EVAL_quasiquote(MalVal *ast)
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
    MalVal *rv = EVAL_quasiquote_list(list);
    if (exception) {
      list_release(list);
      malval_reset_temp(rv, NULL);
      return NIL;
    }
    list_release(list);
    return rv;
  }

  if (VAL_TYPE(ast) == TYPE_VECTOR) {
    List *list = list_from_container(ast);
    linked_list_reverse((void**)&list);
    MalVal *val = EVAL_quasiquote_list(list);
    if (exception) {
      list_release(list);
      malval_reset_temp(val, NULL);
      return NIL;
    }
    List * result = cons_weak(malval_symbol("vec"),
                              cons_weak(val, NULL));
    return malval_list(result);
  }

  if (VAL_TYPE(ast) == TYPE_MAP || VAL_TYPE(ast) == TYPE_SYMBOL) {
    return malval_list(cons_weak(malval_symbol("quote"),
                                 cons_weak(ast, NULL)));
  }

  return ast;
}

static MalVal *EVAL_try(List *body, ENV *env)
{
  MalVal *result = EVAL(body->head, env);
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
  malval_reset_temp(result, NULL);
  List binds = {NULL, 1, VAL_LIST(catch)->tail->head};
  List exc = {NULL, 1, exception};
  ENV *env2 = env_create(env, &binds, &exc);
  exception = NULL;
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

MalVal *EVAL(MalVal *ast, ENV *env)
{
  if (exception)
    return ast;

  env_acquire(env);

  while (TRUE)
  {
    if (ast->type != TYPE_LIST) {
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
          ast = EVAL_quasiquote(tail->head);
          if (exception) {
            env_release(env);
            return NIL;
          }
          continue;
        }
        if (strcmp(symbol, "quasiquoteexpand") == 0) {
          return EVAL_quasiquote(tail->head);
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
      if (strcmp(symbol, "macroexpand") == 0) {
        MalVal *rv = macroexpand(tail->head, env);
        env_release(env);
        return rv;
      }

      if (strcmp(symbol, "try*") == 0) {
        return EVAL_try(tail, env);
      }

      if (strcmp(symbol, "comment") == 0) {
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

    gc_mark(ast, NULL);
    gc_mark_list(list, NULL);
    gc_mark_env(env, NULL);
    gc(FALSE, FALSE);
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
    exception = NULL;
    return PRINT(NULL);
  }
  return PRINT(val);
}

static void cleanup(void)
{
  env_flush(repl_env);

  gc(TRUE, TRUE);
  gc(TRUE, TRUE);
  gc(TRUE, TRUE);

  env_release(repl_env);
  repl_env = NULL;

  unsigned count, size;
  value_info(&count, &size);
  fprintf(stderr, "Values remaining: %u (%u bytes)\n", count, size);

  heap_info(&count, &size);
  fprintf(stderr, "Heap remainig: %u items (%u bytes)\n", count, size);
}

static MalVal *builtin_eval(List *args, ENV *ignored)
{
  if (!args) {
    err_warning(ERR_ARGUMENT_MISMATCH, "need one argument");
    return NIL;
  }

  return EVAL(args->head, repl_env);
}

static void build_env(void)
{
  repl_env = env_create(NULL, NULL, NULL);

  for (struct ns *ns = core_ns; ns->name != NULL; ns++) {
    env_set(repl_env, ns->name, function_create_builtin(ns->fn));
  }

  env_set(repl_env, "nil", _nil);
  env_set(repl_env, "true", _true);
  env_set(repl_env, "false", _false);
  env_set(repl_env, "eval", function_create_builtin(builtin_eval));
  env_set(repl_env, "*ARGV*", malval_list(NULL));
  env_set(repl_env, "*host-language*", malval_string("agon-light"));
}

/* init mal code, separated by \f */
char init[] = "\
(defmacro! defn (fn* [name & body] `(def! ~name (fn* ~@body))))\f\
(defmacro! defmacro (fn* [name & body] `(defmacro! ~name (fn* ~@body))))\f\
(defmacro! def (fn* [name & body] `(def! ~name ~@body)))\f\
(defn load-file\n\
       [f] (eval (read-string (str \"(do \" (slurp f) \"\n nil)\"))))\f\
(load-file \"init.mal\")\
";

int main(int argc, char **argv)
{
  /* Largely to keep valgrind happy */
  atexit(cleanup);

  _nil = malval_nil();
  _true = malval_bool(TRUE);
  _false = malval_bool(FALSE);
  gc_pop();
  gc_pop();
  gc_pop();

  build_env();

  char *s = strtok(init, "\f");
  do {
    heap_free(rep(repl_env, s));
    s = strtok(NULL, "\f");
  } while(s);

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
    env_set(repl_env, "*ARGV*", malval_list(args));
    list_release(args);

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
    puts(s);
    heap_free(s);
    // gc(FALSE, TRUE);
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
