#ifndef _PRINTER_H
#define _PRINTER_H

#include "malval.h"
#include "list.h"
#include "vec.h"
#include "map.h"

extern char *pr_str(const MalVal*, bool);
extern char *pr_str_list(List *, bool);
extern char *pr_str_vector(Vec *vec, bool readable);
extern char *pr_str_map(Map *map, bool readable);
extern char *pr_str_set(Map *set, bool readable);

#endif /* _PRINTER_H */
