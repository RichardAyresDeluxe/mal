#ifndef _HEAP_H
#define _HEAP_H

#include <stddef.h>

extern void *heap_malloc(size_t sz);
extern void *heap_calloc(size_t nmemb, size_t sz);
extern void *heap_realloc(void *p, size_t sz);
extern void heap_free(void*);
extern void heap_info(unsigned *count, unsigned *size);

#endif /* _HEAP_H */
