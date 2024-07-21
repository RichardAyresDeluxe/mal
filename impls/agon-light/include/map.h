#ifndef _MAP_H
#define _MAP_H

#include "malval.h"
#include "list.h"

typedef struct Map Map;

Map *map_createN(unsigned init_size);
Map *map_create(void);
Map *map_duplicate(Map*);
Map *map_acquire(Map*);
void map_release(Map*);
void map_add(Map *, MalVal *key, MalVal *value);
void map_remove(Map *, MalVal *key);
unsigned map_size(Map*, bool deep);
bool map_equals(Map *a, Map *b);

unsigned map_count(Map*);

/** returns NULL if not found */
MalVal *map_find(Map *, MalVal *key);
#define map_contains(map, key) (map_find((map), (key)) != NULL)
uint16_t map_hash(Map*);

bool map_is_empty(Map*);

List *map_keys(Map*);
List *map_values(Map*);

typedef void (*KeyValProc)(MalVal *key, MalVal *val, void *data);
void map_foreach(Map *, KeyValProc, void*);

void gc_mark_map(Map *, void*);

#endif /* _MAP_H */
