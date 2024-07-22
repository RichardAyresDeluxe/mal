#ifndef _GC_H
#define _GC_H

#include "malval.h"
#include "list.h"

/** This is a stack of temporary values that do not exist in
 * an environment and so can't be found, but should not be 
 * garbage collected yet */
extern void push_temp(MalVal*);
extern void pop_temp(void);
extern void pop_temps(int);
extern unsigned temps_count(void);

/** Take out the trash */
extern void gc(bool force);
extern void gc_add(MalVal *);
/** Pop the last item of "all values" list - used for values that should never
 * be deleted, i.e. nil, true, false */
extern MalVal *gc_pop(void);
extern void gc_mark(MalVal *val, void *data);
extern void gc_mark_env(struct ENV*, void *);
extern void gc_mark_list(List*, void*);
extern void value_info(unsigned *count, unsigned *size);

// extern void gc_mark_symtab(symtab_t*);

#endif /* _GC_H */
