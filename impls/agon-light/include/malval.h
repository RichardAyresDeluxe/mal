#ifndef _MALVAL_H
#define _MALVAL_H

#include <stdint.h>

typedef uint8_t bool;
#define FALSE 0
#define TRUE 1

#define MASK_TYPE   0x3F
#define MASK_FLAGS  0xC0

#define FLAG_MARK   0x80

#define TYPE_NIL      0x00
#define TYPE_NUMBER   0x11
#define TYPE_BOOL     0x12
#define TYPE_LIST     0x21
#define TYPE_VECTOR   0x22
#define TYPE_STRING   0x31
#define TYPE_SYMBOL   0x32

typedef struct MalVal {
  struct MalVal *next;      /* used for garbage collection */
  uint8_t type:6;
  uint8_t unused:1;
  uint8_t mark:1;           /* marked - do not collect garbage */
  union {
    int number;
    struct MalList *list;
    struct MalList *vec;
    const char *string;
    void *data;
  } data;
} MalVal;

typedef void (*MalValProc)(MalVal *, void *);

MalVal *malval_create(uint8_t type);
#define malval_nil() malval_create(TYPE_NIL)

#define VAL_IS_NIL(val) ((val)->type == TYPE_NIL)

void malval_free(MalVal*);
unsigned malval_size(MalVal*, bool);

#endif /* _MALVAL_H */
