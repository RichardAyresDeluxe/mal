#include "hash.h"
#include "malval.h"

void hash_continue(MalVal *val, void *_hv)
{
    uint16_t *hv = _hv;
    *hv = ((unsigned)*hv * 31 + malval_hash(val)) % 65521;
}

void hash_continue2(MalVal *v1, MalVal *v2, void *hash_value_ptr)
{
    hash_continue(v1, hash_value_ptr);
    hash_continue(v2, hash_value_ptr);
}
