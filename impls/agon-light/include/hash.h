#ifndef _HASH_H
#define _HASH_H

#include "malval.h"

#define HASH_INIT_STRING  53
#define HASH_INIT_SYMBOL  59
#define HASH_INIT_ATOM    61
#define HASH_INIT_LIST    67
#define HASH_INIT_VEC     71
#define HASH_INIT_MAP     73
#define HASH_INIT_FUNCTION 97
#define HASH_INIT_NUMBER 251
#define HASH_INIT_BYTE 257

/** Take an uint16_t pointer to a hash value and
 * combine it with the hash of the given value, and
 * update the pointer. */
extern void hash_continue(MalVal *value, void *hash_value_ptr);
extern void hash_continue2(MalVal *v1, MalVal *v2, void *hash_value_ptr);


#endif /* _HASH_H */
