#include "ns.h"
#include "core.h"
#include "eval.h"
#include "str.h"
#include "heap.h"

#ifdef AGON_LIGHT
#include <mos_api.h>

static MalType types_vdu[] = {METATYPE_NUMERIC, METATYPE_NUMERIC, METATYPE_NUMERIC, METATYPE_NUMERIC, METATYPE_NUMERIC, METATYPE_NUMERIC, 0};
static MalVal *core_vdu(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, ARGS_MAX, types_vdu))
    return NIL;

  for (List *arg = args; arg; arg = arg->tail) {
    switch (VAL_TYPE(arg->head)) {
      case TYPE_BYTE:
        putch(VAL_BYTE(arg->head));
        break;
      case TYPE_NUMBER: {
        int i = VAL_NUMBER(arg->head);
        putch(i & 0xFF);
        putch((i >> 8) & 0xFF);
        break;
      }
    }
  }

  return NIL;
}

static MalType types_mos_cli[] = {TYPE_STRING, 0};
static MalVal *core_mos_cli(List *args, ENV *env)
{
  if (!builtins_args_check(args, 1, ARGS_MAX, types_mos_cli))
    return NIL;

  char *command = strdup(VAL_STRING(args->head));

  for (List *arg = args->tail; arg; arg = arg->tail) {
    if (VAL_TYPE(arg->head) != TYPE_STRING)
      malthrow("arg must be string");

    catstr(&command, " ");
    catstr(&command, VAL_STRING(arg->head));
  }

  int retval = mos_oscli(command, NULL, 0);

  heap_free(command);

  return malval_number(retval);
}

struct ns core_mos_ns[] = {
  {"vdu", core_vdu},
  {"mos/cli", core_mos_cli},
  {NULL, NULL}
};
#else
struct ns core_mos_ns[] = { {NULL, NULL} };
#endif

