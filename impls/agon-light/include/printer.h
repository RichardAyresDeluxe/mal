#ifndef _PRINTER_H
#define _PRINTER_H

#include "malval.h"
#include "list.h"

extern char *pr_str(const MalVal*, bool);
extern char *pr_str_list(List *, bool);
extern char *pr_str_vector(List *list, bool readable);
extern char *pr_str_map(List *list, bool readable);

#endif /* _PRINTER_H */
