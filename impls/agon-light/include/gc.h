#ifndef _GC_H
#define _GC_H

#include "malval.h"

/** Take out the trash */
extern void gc(bool force);
extern void gc_add(MalVal *);
extern void gc_mark(MalVal *val, void *data);
extern void value_info(unsigned *count, unsigned *size);

// extern void gc_mark_symtab(symtab_t*);

#endif /* _GC_H */
