#ifndef _HEAP_H
#define _HEAP_H

#include <stddef.h>

#ifndef NDEBUG
extern unsigned heap_size;
extern void heap_init(void);
#endif
extern void *heap_malloc(size_t sz);
extern void *heap_calloc(size_t nmemb, size_t sz);
extern void *heap_realloc(void *p, size_t sz);
extern void heap_free(void*);
extern void heap_info(unsigned *count, unsigned *size);

#endif /* _HEAP_H */
