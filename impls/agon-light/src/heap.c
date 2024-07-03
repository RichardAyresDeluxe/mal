#include <stdlib.h>
#include "heap.h"
#include "err.h"

static void out_of_memory(void);

void *heap_malloc(size_t sz)
{
  void *p = malloc(sz);
  if (!p)
    out_of_memory();
  return p;
}

void *heap_calloc(size_t nmemb, size_t sz)
{
  void *p = calloc(nmemb, sz);
  if (!p)
    out_of_memory();
  return p;
}
void *heap_realloc(void *p, size_t sz)
{
  p = realloc(p, sz);
  if (!p)
    out_of_memory();
  return p;
}
void heap_free(const void *p)
{
  if (p)
    free((void*)p);
}

void out_of_memory(void)
{
  err_fatal(ERR_OUT_OF_MEMORY, "Out of memory");
}
