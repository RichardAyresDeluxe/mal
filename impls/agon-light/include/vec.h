#ifndef _VEC_H
#define _VEC_H

#include "malval.h"
#include "list.h"

typedef struct Vec Vec;

extern Vec *vec_create(void);
extern Vec *vec_createN(unsigned size);
extern Vec *vec_duplicate(Vec*);
extern void vec_destroy(Vec*);

extern unsigned vec_count(Vec*);

extern void vec_append(Vec*, MalVal*);
extern void vec_prepend(Vec*, MalVal*);
extern void vec_update(Vec*, int offset, MalVal*);

extern MalVal *vec_get(Vec*, int offset);

extern Vec *vec_slice(Vec*, int offset, unsigned count);
/** returns new Vec */
extern Vec *vec_splice(Vec *this, int this_offset, Vec *that, int that_offset, unsigned len);

/** returns new Vec */
extern Vec *vec_concat(Vec *, Vec *);

extern void vec_foreach(Vec *, MalValProc, void*);

extern bool vec_equals(Vec*, Vec*);
extern uint16_t vec_hash(Vec*);
extern List *list_from_vec(Vec*);

#endif /* _VEC_H */
