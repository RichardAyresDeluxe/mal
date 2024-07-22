#include "str.h"
#include "heap.h"
#include "hash.h"

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

static uint16_t _string_hash(unsigned p, const char *s)
{
  unsigned m = 65521;  // highest prime inside 16 bits
  unsigned hv = 0;
  unsigned p_pow = 1;
  for (const char *c = s; *c; c++) {
    hv = (hv + (*c - 'a' + 1) * p_pow) % m;
    p_pow = (p_pow * (uint32_t)p) % m;
  }

  return (uint16_t)hv;
}

uint16_t string_hash(const char *s)
{
  return _string_hash(HASH_INIT_STRING, s);
}

uint16_t symbol_hash(const char *s)
{
  return _string_hash(HASH_INIT_SYMBOL, s);
}
