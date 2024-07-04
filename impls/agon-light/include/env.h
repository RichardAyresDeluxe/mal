#ifndef _ENV_H
#define _ENV_H

#include "malval.h"

struct ENV;

struct ENV {
  const char *name;
  MalVal *value;
};

typedef struct ENV ENV;

#endif /* _ENV_H */
