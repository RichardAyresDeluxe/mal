#ifndef _GC_H
#define _GC_H

#include "malval.h"
#include "list.h"

/** Take out the trash */
extern void gc(bool force);
extern void gc_add(MalVal *);
extern void gc_mark(MalVal *val, void *data);
extern void gc_mark_env(struct ENV*, void*);
extern void gc_mark_list(List*, void*);
extern void value_info(unsigned *count, unsigned *size);

// extern void gc_mark_symtab(symtab_t*);

#endif /* _GC_H */
