#ifndef _HEAP_H
#define _HEAP_H

#include <stdlib.h>

extern void *heap_malloc(size_t sz);
extern void *heap_calloc(size_t nmemb, size_t sz);
extern void *heap_realloc(void *p, size_t sz);
extern void heap_free(const void*);

#endif /* _HEAP_H */
