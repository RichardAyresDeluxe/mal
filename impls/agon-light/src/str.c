#include "str.h"
#include "heap.h"

#include <string.h>

char *strdup(const char *s)
{
  unsigned l = strlen(s);
  char *new = heap_malloc(l + 1);
  memcpy(new, s, l);
  new[l] = '\0';
  return new;
}

char *catstr(char **p, const char *s)
{
  unsigned l1 = *p ? strlen(*p) : 0;
  unsigned l2 = strlen(s);
  char *new = heap_malloc(l1 + l2 + 1);
  if (*p)
    memcpy(new, *p, l1);
  memcpy(&new[l1], s, l2);
  new[l1 + l2] = '\0';
  heap_free(*p);
  *p = new;
  return new;
}
