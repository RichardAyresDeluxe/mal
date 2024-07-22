#include "map.h"
#include "heap.h"
#include "gc.h"
#include "hash.h"

#include <string.h>

/* Array of allowed table sizes */
static const unsigned sizes[] = {3, 7, 17, 37, 59, 127, 251};

#define TABLE_SIZE_INIT sizes[0]
#define TABLE_SIZE_MAX sizes[sizeof(sizes)/sizeof(sizes[0]) - 1]

struct entry {
  struct entry *next;
  MalVal *key;
  MalVal *value;
};

struct Map {
  unsigned table_size;
  unsigned ref_count;
  struct entry **table;
};

typedef struct entry Entry;

/* Rebuild this map for the given number of entries */
static void rebuild(Map *, unsigned);

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
  map->ref_count = 1;
  map->table_size = next_size(init_size);
  map->table = heap_calloc(map->table_size, sizeof(struct entry*));
  return map;
}

Map *map_create(void)
{
  return map_createN(TABLE_SIZE_INIT);
}

Map *map_acquire(Map *map)
{
  if (map)
    map->ref_count++;
  return map;
}

Map *map_duplicate(Map *in)
{
  Map *map = heap_malloc(sizeof(Map));
  map->ref_count = 1;
  map->table_size = in->table_size;
  map->table = heap_calloc(map->table_size, sizeof(Entry*));

  for (unsigned idx = 0; idx < map->table_size; idx++) {
    for (Entry *entry = in->table[idx]; entry; entry = entry->next) {
      Entry *new = heap_malloc(sizeof(Entry));
      new->key = entry->key;
      new->value = entry->value;
      new->next = map->table[idx];
      map->table[idx] = new;
    }
  }

  return map;
}

static void destroy_table(Map *map)
{
  for (unsigned idx = 0; idx < map->table_size; idx++) {
    struct entry *rover = map->table[idx];
    while (rover) {
      struct entry *next = rover->next;
      heap_free(rover);
      rover = next;
    }
  }
}

static void map_destroy(Map *map)
{
  if (!map)
    return;

  destroy_table(map);
  heap_free(map->table);
  heap_free(map);
}

void map_release(Map *map)
{
  if (map->ref_count-- == 1)
    map_destroy(map);
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

static Entry *find_entry(Entry *entries, MalVal *key)
{
  for (Entry *rover = entries; rover; rover = rover->next) {
    if (malval_equals(rover->key, key))
      return rover;
  }
  return NULL;
}

void map_add(Map *map, MalVal *key, MalVal *val)
{
  Entry *entry;
  unsigned hv = malval_hash(key);
  unsigned idx = hv % map->table_size;

  if ((entry = find_entry(map->table[idx], key)) != NULL) {
    /* There is already an entry for that key - re-use (overwrite) it */
    entry->value = val;
    return;
  }

  if (map->table_size < TABLE_SIZE_MAX && map->table[idx] != NULL) {
    /* This bucket already has entries - try to rebuild */
    rebuild(map, 1 + map_count(map));
    idx = hv % map->table_size;
  }

  entry = heap_malloc(sizeof(struct entry));
  entry->key = key;
  entry->value = val;
  entry->next = map->table[idx];
  map->table[idx] = entry;
}

MalVal *map_find(Map *map, MalVal *key)
{
  if (!map)
    return NULL;

  unsigned idx = malval_hash(key) % map->table_size;
  
  Entry *found;
  if ((found = find_entry(map->table[idx], key)) != NULL)
    return found->value;

  return NULL;
}

void map_remove(Map *map, MalVal *key)
{
  unsigned idx = malval_hash(key) % map->table_size;

  Entry *result = NULL;
  Entry *rover = map->table[idx];
  while (rover) {
    Entry *next = rover->next;
    if (malval_equals(rover->key, key)) {
      heap_free(rover);
    }
    else {
      rover->next = result;
      result = rover;
    }
    rover = next;
  }
  map->table[idx] = result;
}

static void gc_mark_entry(MalVal *key, MalVal *val, void *data)
{
  gc_mark(key, data);
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

static void insert_entry(MalVal *key, MalVal *val, void *data)
{
  Map *map = data;

  unsigned idx = malval_hash(key) % map->table_size;

  Entry *entry = heap_malloc(sizeof(Entry));
  entry->key = key;
  entry->value = val;
  entry->next = map->table[idx];
  map->table[idx] = entry;
}

static void rebuild(Map *map, unsigned for_size)
{
  unsigned new_size = next_size(for_size);
  struct entry **new_table = heap_calloc(new_size, sizeof(struct entry*));

  Map tmp = {new_size, 1, new_table};
  map_foreach(map, insert_entry, &tmp);

  destroy_table(map);
  heap_free(map->table);
  map->table_size = new_size;
  map->table = new_table;
}

static void _map_size(MalVal *key, MalVal *val, void *_size)
{
  unsigned *psize = _size;
  *psize += malval_size(key, FALSE) + malval_size(val, FALSE) + sizeof(Entry);
}

unsigned map_size(Map *map, bool deep)
{
  unsigned sz = sizeof(Map) + sizeof(Entry*) * map->table_size;
  map_foreach(map, _map_size, &sz);
  return sz;
}

struct equals_t {
  Map *that;
  bool result;
};

static void _map_eq(MalVal *key, MalVal *val, void *_data)
{
  struct equals_t *eq = _data;
  if (!eq->result)
    return;

  MalVal *found = map_find(eq->that, key);
  eq->result = (found && malval_equals(val, found));
}

bool map_equals(Map *a, Map *b)
{
  if (map_count(a) != map_count(b))
    return FALSE;

  struct equals_t eq = {b, TRUE};
  map_foreach(a, _map_eq, &eq);

  return eq.result;
}

uint16_t map_hash(Map *map)
{
  uint16_t hv = HASH_INIT_MAP;
  map_foreach(map, hash_continue2, &hv);
  return hv;
}

bool map_is_empty(Map *map)
{
  for (unsigned idx = 0; idx < map->table_size; idx++) {
    if (map->table[idx] != NULL)
      return FALSE;
  }
  return TRUE;
}

static void _map_keys(MalVal *key, MalVal *val, void *_data)
{
  List **keys = _data;
  *keys = cons_weak(key, *keys);
}

List *map_keys(Map *map)
{
  List *keys = NULL;
  map_foreach(map, _map_keys, &keys);
  return keys;
}

static void _map_vals(MalVal *key, MalVal *val, void *_data)
{
  List **vals = _data;
  *vals = cons_weak(val, *vals);
}

List *map_values(Map *map)
{
  List *vals = NULL;
  map_foreach(map, _map_vals, &vals);
  return vals;
}
