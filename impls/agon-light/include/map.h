#ifndef _MAP_H
#define _MAP_H

#include "malval.h"

struct Map;
typedef struct Map Map;

Map *map_createN(unsigned init_size);
Map *map_create(void);
void map_destroy(Map*);
void map_add(Map *, MalVal *key, MalVal *value);
void map_remove(Map *, MalVal *key);

unsigned map_count(Map*);

/** returns NULL if not found */
MalVal *map_find(Map *, MalVal *key);

typedef void (*KeyValProc)(MalVal *key, MalVal *val, void *data);
void map_foreach(Map *, KeyValProc, void*);

void gc_mark_map(Map *, void*);

#endif /* _MAP_H */
