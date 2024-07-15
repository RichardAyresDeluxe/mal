#include "map.h"
#include "heap.h"
#include "gc.h"
#include "str.h"

#include <string.h>

static unsigned primes[] = {3, 7, 17, 37, 59, 127, 251};

struct entry {
  struct entry *next;
  char *key;
  MalVal *value;
};

struct Map {
  unsigned table_size;
  struct entry **table;
};

static uint16_t string_hash(const char *s)
{
  unsigned p = 57;
  unsigned m = 65521;  // highest prime inside 16 bits
  unsigned hv = 0;
  unsigned p_pow = 1;
  for (const char *c = s; *c; c++) {
    hv = (hv + (*c - 'a' + 1) * p_pow) % m;
    p_pow = (p_pow * (uint32_t)p) % m;
  }

  return (uint16_t)hv;
}

static unsigned next_prime(unsigned p)
{
  unsigned nprimes = sizeof(primes)/sizeof(primes[0]);
  for (unsigned i = 0; i < nprimes; i++) {
    if (primes[i] >= p)
      return primes[i];
  }
  return primes[nprimes - 1];
}

Map *map_createN(unsigned init_size)
{
  Map *map = heap_malloc(sizeof(Map));
  map->table_size = next_prime(init_size);
  map->table = heap_calloc(map->table_size, sizeof(struct entry*));
  return map;
}

Map *map_create(void)
{
  return map_createN(primes[0]);
}

void map_destroy(Map *map)
{
  if (!map)
    return;

  for (unsigned idx = 0; idx < map->table_size; idx++) {
    struct entry *rover = map->table[idx];
    while (rover) {
      struct entry *next = rover->next;
      heap_free(rover->key);
      heap_free(rover);
      rover = next;
    }
  }

  heap_free(map->table);
  heap_free(map);
}

void map_add(Map *map, const char *key, MalVal *val)
{
  unsigned idx = string_hash(key) % map->table_size;
  struct entry *entry = heap_malloc(sizeof(struct entry));

  entry->key = strdup(key);
  entry->value = val;
  entry->next = map->table[idx];
  map->table[idx] = entry;
}

MalVal *map_find(Map *map, const char *key)
{
  if (!map)
    return NULL;

  unsigned idx = string_hash(key) % map->table_size;
  
  for (struct entry *entry = map->table[idx]; entry; entry = entry->next) {
    if (strcmp(entry->key, key) == 0)
      return entry->value;
  }

  return NULL;
}

static void gc_mark_entry(const char *key, MalVal *val, void *data)
{
  gc_mark(val, data);
}

void gc_mark_map(struct Map *map, void *data)
{
  map_foreach(map, gc_mark_entry, data);
}

void map_foreach(Map *map, KeyValProc p, void *data)
{
  for (unsigned idx = 0; idx < map->table_size; idx++) {
    for (struct entry *entry = map->table[idx]; entry; entry = entry->next) {
      p(entry->key, entry->value, data);
    }
  }
}
