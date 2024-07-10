#include <stdlib.h>
#include "heap.h"
#include "err.h"

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
  unsigned size;
  struct block *prev, *next;
} block_t;

static block_t heap = {0, &heap, &heap };

static void block_link(block_t *blk)
{
  blk->prev = heap.next->prev;
  blk->next = heap.next;
  heap.next->prev = blk;
  heap.next = blk;
}

static void block_unlink(block_t *blk)
{
  blk->prev->next = blk->next;
  blk->next->prev = blk->prev;
}

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
  block_link(blk);

  return blk + 1;
}

void *heap_calloc(size_t nmemb, size_t sz)
{
  /* FIXME: Losing calloc's nmemb * sz overflow detection */
  block_t *blk = calloc(1, sizeof(block_t) + (nmemb * sz));
  if (!blk)
    out_of_memory();

  blk->size = sz;
  block_link(blk);

  return blk + 1;
}

void *heap_realloc(void *p, size_t sz)
{
  block_t *blk = NULL;

  if (p) {
    blk = (block_t*)p - 1;
    block_unlink(blk);
  }

  blk = realloc(blk, sizeof(block_t) + sz);
  if (!blk)
    out_of_memory();

  blk->size = sz;
  block_link(blk);

  return blk + 1;
}

void heap_free(void *p)
{
  if (!p)
    return;

  block_t *blk = (block_t*)p - 1;
  block_unlink(blk);
  free(blk);
}

void heap_info(unsigned *count, unsigned *size)
{
  *count = 0;
  *size = 0;
  for (block_t *rover = heap.next; rover != &heap; rover = rover->next) {
    (*count)++;
    *size = *size + rover->size;
  }
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
