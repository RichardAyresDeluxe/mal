#include "map.h"
#include "heap.h"
#include "gc.h"
#include "str.h"

#include <string.h>

/* Array of allowed table sizes */
static const unsigned sizes[] = {3, 7, 17, 37, 59, 127, 251};

#define TABLE_SIZE_INIT sizes[0]
#define TABLE_SIZE_MAX sizes[sizeof(sizes)/sizeof(sizes[0]) - 1]

struct entry {
  struct entry *next;
  char *key;
  MalVal *value;
};

struct Map {
  unsigned table_size;
  struct entry **table;
};

typedef struct entry Entry;

/* Rebuild this map for the given number of entries */
static void rebuild(Map *, unsigned);

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

static unsigned next_size(unsigned p)
{
  unsigned nsizes = sizeof(sizes)/sizeof(sizes[0]);
  for (unsigned i = 0; i < nsizes; i++) {
    if (sizes[i] >= p)
      return sizes[i];
  }
  return sizes[nsizes - 1];
}

Map *map_createN(unsigned init_size)
{
  Map *map = heap_malloc(sizeof(Map));
  map->table_size = next_size(init_size);
  map->table = heap_calloc(map->table_size, sizeof(struct entry*));
  return map;
}

Map *map_create(void)
{
  return map_createN(TABLE_SIZE_INIT);
}

static void destroy_table(Map *map)
{
  for (unsigned idx = 0; idx < map->table_size; idx++) {
    struct entry *rover = map->table[idx];
    while (rover) {
      struct entry *next = rover->next;
      heap_free(rover->key);
      heap_free(rover);
      rover = next;
    }
  }
}

void map_destroy(Map *map)
{
  if (!map)
    return;

  destroy_table(map);
  heap_free(map->table);
  heap_free(map);
}

unsigned map_count(Map *map)
{
  unsigned count = 0;
  for (unsigned idx = 0; idx < map->table_size; idx++) {
    for (Entry *rover = map->table[idx]; rover; rover = rover->next)
      count++;
  }
  return count;
}

void map_add(Map *map, const char *key, MalVal *val)
{
  unsigned hv = string_hash(key);
  unsigned idx = hv % map->table_size;

  if (map->table_size < TABLE_SIZE_MAX && map->table[idx] != NULL) {
    /* This bucket already has entries - try to rebuild */
    rebuild(map, 1 + map_count(map));
    idx = hv % map->table_size;
  }

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

static void insert_entry(const char *key, MalVal *val, void *data)
{
  Map *map = data;

  unsigned idx = string_hash(key) % map->table_size;

  Entry *entry = heap_malloc(sizeof(Entry));
  entry->key = strdup(key);
  entry->value = val;
  entry->next = map->table[idx];
  map->table[idx] = entry;
}

static void rebuild(Map *map, unsigned for_size)
{
  unsigned new_size = next_size(for_size);
  struct entry **new_table = heap_calloc(new_size, sizeof(struct entry*));

  Map tmp = {new_size, new_table};
  map_foreach(map, insert_entry, &tmp);

  destroy_table(map);
  heap_free(map->table);
  map->table_size = new_size;
  map->table = new_table;
}
