#ifndef _EVAL_H
#define _EVAL_H

#include "malval.h"
#include "env.h"

/** This can return NULL */
extern MalVal *EVAL(MalVal*, ENV*);

#endif /* _EVAL_H */
