#include <stdlib.h>
#include "heap.h"
#include "err.h"
#include "list.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef NDEBUG

unsigned heap_size;

void heap_init(void)
{
#if defined(AGON_LIGHT)
  extern int __heaptop, __heapbot;
  heap_size = (unsigned long)&__heaptop - (unsigned long)&__heapbot;
#else
  heap_size = 512 * 1024;
#endif
}

typedef struct block {
  struct block *prev, *next;
  unsigned size;
} block_t;

static block_t heap = {&heap, &heap, 0};

static void out_of_memory(void)
{
  unsigned total_heap = 0;

  for (block_t *rover = heap.next; rover != &heap; rover = rover->next) {
    total_heap += rover->size;
  }

  err_fatal(ERR_OUT_OF_MEMORY, "used %lu kB", (unsigned long)total_heap/1024);
}

void *heap_malloc(size_t sz)
{
  block_t *blk = malloc(sizeof(block_t) + sz);
  if (!blk)
    out_of_memory();

  blk->size = sz;
  dlist_add(&heap, blk);

  return blk + 1;
}

void *heap_calloc(size_t nmemb, size_t sz)
{
  /* FIXME: Losing calloc's nmemb * sz overflow detection */
  block_t *blk = calloc(1, sizeof(block_t) + (nmemb * sz));
  if (!blk)
    out_of_memory();

  blk->size = sz;
  dlist_add(&heap, blk);

  return blk + 1;
}

void *heap_realloc(void *p, size_t sz)
{
  block_t *blk = NULL;

  if (p) {
    blk = (block_t*)p - 1;
    dlist_remove(blk);
  }

  blk = realloc(blk, sizeof(block_t) + sz);
  if (!blk)
    out_of_memory();

  blk->size = sz;
  dlist_add(&heap, blk);

  return blk + 1;
}

void heap_free(void *p)
{
  if (!p)
    return;

  block_t *blk = (block_t*)p - 1;
  dlist_remove(blk);
  free(blk);
}

static void count_heap(void *_blk, void *_out)
{
  block_t *blk = _blk;
  unsigned *out = _out;

  out[0]++;
  out[1] += blk->size;
}

void heap_info(unsigned *count, unsigned *size)
{
  unsigned out[2] = {0 /* count */, 0 /* size */};

  dlist_forall(&heap, count_heap, out);

  *count = out[0];
  *size = out[1];
}
#else
static void out_of_memory()
{
  err_fatal(ERR_OUT_OF_MEMORY, NULL);
}

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
void heap_free(void *p)
{
  if (p)
    free(p);
}
#endif
