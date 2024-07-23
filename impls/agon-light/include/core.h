#ifndef _CORE_H
#define _CORE_H

#include "ns.h"
#include "function.h"

#define ARGS_MAX MAX_ARITY

extern struct ns core_ns[];
extern struct ns core_mos_ns[];

extern bool builtins_args_check(
  List *args,
  unsigned count_lo,
  unsigned count_hi,
  MalType *types
);

#endif /* _CORE_H */
