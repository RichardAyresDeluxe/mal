#ifndef _VEC_H
#define _VEC_H

#include "malval.h"

typedef struct vec Vec;

extern Vec *vec_create(void);
extern Vec *vec_createN(unsigned size);
extern Vec *vec_duplicate(Vec*);
extern void vec_destroy(Vec*);

extern unsigned vec_count(Vec*);

extern void vec_append(Vec*, MalVal*);
extern void vec_prepend(Vec*, MalVal*);
extern void vec_update(Vec*, int offset, MalVal*);

extern Vec *vec_slice(Vec*, int offset, unsigned count);
/** returns new Vec */
extern Vec *vec_splice(Vec *this, int this_offset, Vec *that, int that_offset, unsigned len);

/** returns new Vec */
extern Vec *vec_concat(Vec *, Vec *);

extern void vec_foreach(Vec *, MalValProc, void*);

#endif /* _VEC_H */
