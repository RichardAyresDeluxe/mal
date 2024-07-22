#include "vec.h"
#include "heap.h"
#include "hash.h"

#include <assert.h>
#include <string.h>

#define BLOCK_SIZE 8

typedef struct {
  unsigned ref_count;
  MalVal *values[BLOCK_SIZE];
} Block;

struct Vec {
  unsigned offset;
  unsigned count;
  unsigned num_blocks;
  Block **blocks;
};

static void get_block_and_offset(Vec *vec, int offset, unsigned *b, unsigned *poff)
{
  offset += vec->offset;
  *b = offset / BLOCK_SIZE;
  *poff = offset - (*b * BLOCK_SIZE);
}

static void block_release(Block *blk)
{
  if (blk && blk->ref_count-- == 1)
    heap_free(blk);
}

Vec *vec_create(void)
{
  return vec_createN(BLOCK_SIZE);
}

Vec *vec_createN(unsigned size)
{
  unsigned num_blocks = 1 + (size-1) / BLOCK_SIZE;
  Vec *vec = heap_malloc(sizeof(Vec));
  vec->offset = 0;
  vec->count = 0;
  vec->num_blocks = num_blocks;
  vec->blocks = heap_calloc(num_blocks, sizeof(Block*));
  return vec;
}

void vec_destroy(Vec *vec)
{
  for (unsigned b = 0; b < vec->num_blocks; b++) {
    block_release(vec->blocks[b]);
  }
  heap_free(vec->blocks);
  heap_free(vec);
}

Vec *vec_duplicate(Vec *vec)
{
  Vec *out = heap_malloc(sizeof(Vec));
  out->offset = vec->offset;
  out->count = vec->count;
  out->num_blocks = vec->num_blocks;
  out->blocks = heap_calloc(vec->num_blocks, sizeof(Block*));
  for (unsigned b = 0; b < vec->num_blocks; b++) {
    out->blocks[b] = vec->blocks[b];
    if (out->blocks[b])
      out->blocks[b]->ref_count++;
  }
  return out;
}

unsigned vec_count(Vec *vec)
{
  return vec->count;
}

static Block *block_duplicate(Block *block)
{
  Block *new = heap_malloc(sizeof(Block));
  new->ref_count = 1;
  memcpy(new->values, block->values, sizeof(MalVal*) * BLOCK_SIZE);
  block_release(block);
  return new;

}

void vec_append(Vec *vec, MalVal *val)
{
  unsigned b, off;
  get_block_and_offset(vec, vec->count, &b, &off);

  if (b >= vec->num_blocks) {
    Block **new_blocks = heap_calloc(b + 1, sizeof(Block*));
    memcpy(new_blocks, vec->blocks, vec->num_blocks * sizeof(Block*));
    heap_free(vec->blocks);
    vec->num_blocks = b + 1;
    vec->blocks = new_blocks;
  }

  if (vec->blocks[b] == NULL) {
    assert(off == 0);
    vec->blocks[b] = heap_calloc(1, sizeof(Block));
    vec->blocks[b]->ref_count = 1;
    vec->blocks[b]->values[off] = val;
    vec->count++;
    return;
  }

  if (vec->blocks[b]->ref_count > 1) {
    /* duplicate block */
    vec->blocks[b] = block_duplicate(vec->blocks[b]);
  }
  vec->blocks[b]->values[off] = val;
  vec->count++;
}

void vec_prepend(Vec *vec, MalVal *val)
{
  if (vec->offset > 0) {
    /* there is space in the first block */
    if (vec->blocks[0]->ref_count > 1) {
      vec->blocks[0] = block_duplicate(vec->blocks[0]);
    }
    vec->offset--;
    vec->blocks[0]->values[vec->offset] = val;
    vec->count++;
    return;
  }

  /* we have to insert a block at the start */
  Block **new_blocks = heap_malloc(sizeof(Block*) * (vec->num_blocks + 1));
  memcpy(&new_blocks[1], vec->blocks, sizeof(Block*) * vec->num_blocks);
  new_blocks[0] = heap_calloc(1, sizeof(Block));
  new_blocks[0]->ref_count = 1;
  new_blocks[0]->values[BLOCK_SIZE - 1] = val;

  heap_free(vec->blocks);
  vec->blocks = new_blocks;
  vec->num_blocks++;
  vec->count++;
  vec->offset = BLOCK_SIZE - 1;
}

void vec_update(Vec *vec, int offset, MalVal *val)
{
  unsigned b, off;
  get_block_and_offset(vec, offset, &b, &off);

  if (vec->blocks[b]->ref_count > 1) 
    vec->blocks[b] = block_duplicate(vec->blocks[b]);

  vec->blocks[b]->values[off] = val;
}

MalVal *vec_get(Vec *vec, int offset)
{
  unsigned b, off;
  get_block_and_offset(vec, offset, &b, &off);

  MalVal *v = vec->blocks[b]->values[off];
  return v ? v : NIL;
}

void vec_foreach(Vec *vec, MalValProc p, void *data)
{
  unsigned off = vec->offset,
           b = 0;

  while (b < vec->num_blocks) {
    while (off < BLOCK_SIZE) {
      if (vec->blocks[b] && vec->blocks[b]->values[off]) {
        p(vec->blocks[b]->values[off], data);
      }
      off++;
    }
    off = 0;
    b++;
  }
}

/* TODO: slice should c-o-w blocks when possible */
Vec *vec_slice(Vec *vec, int offset, unsigned count)
{
  unsigned b, off, n = count;
  get_block_and_offset(vec, offset, &b, &off);

  Vec *ret = vec_createN(count);
  while (n > 0 && b < vec->num_blocks) {
    while (n > 0 && off < BLOCK_SIZE) {
      vec_append(ret, vec->blocks[b]->values[off]);
      n--;
      off++;
    }
    off = 0;
    b++;
  }

  return ret;
}

Vec *vec_splice(Vec *this, int this_offset, Vec *that, int that_offset, unsigned count)
{
  if (this_offset < 0)
    this_offset += this->count;

  Vec *r1 = vec_slice(this, 0, this_offset);
  Vec *r2 = vec_slice(that, that_offset, count);
  Vec *r3 = vec_slice(this, this_offset, -1);

  Vec *r4 = vec_concat(r1, r2);
  Vec *r5 = vec_concat(r4, r3);

  vec_destroy(r1);
  vec_destroy(r2);
  vec_destroy(r3);
  vec_destroy(r4);
  return r5;
}

static void _append(MalVal *val, void *vec)
{
  vec_append(vec, val);
}

/* TODO: concat should c-o-w blocks when possible */
Vec *vec_concat(Vec *this, Vec *that)
{
  Vec *ret = vec_createN(vec_count(this) + vec_count(that));
  vec_foreach(this, _append, ret);
  vec_foreach(that, _append, ret);
  return ret;
}

uint16_t vec_hash(Vec *vec)
{
  uint16_t hv = HASH_INIT_VEC;
  vec_foreach(vec, hash_continue, &hv);
  return hv;
}

bool vec_equals(Vec *a, Vec *b)
{
  int c = vec_count(a);

  if (c != vec_count(b))
    return FALSE;

  for (int i = 0; i < c; i++) {
    if (!malval_equals(vec_get(a, i), vec_get(b, i)))
      return FALSE;
  }

  return TRUE;
}

List *list_from_vec(Vec *vec)
{
  List *result = NULL;
  int i = vec_count(vec);

  while (i --> 0) {
    result = cons_weak(vec_get(vec, i), result);
  }

  return result;
}
