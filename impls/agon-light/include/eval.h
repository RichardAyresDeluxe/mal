#ifndef _EVAL_H
#define _EVAL_H

#include "malval.h"
#include "env.h"

/** This can return NULL */
extern MalVal *EVAL(MalVal*, ENV*);
extern MalVal *exception;

#define malthrow(msg) do { exception = malval_string(msg); return NIL; } while(0)

#endif /* _EVAL_H */
