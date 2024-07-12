#ifndef _MALVAL_H
#define _MALVAL_H

#include <stdint.h>

typedef uint8_t bool;
#define FALSE 0
#define TRUE 1

#define MASK_TYPE   0x3F
#define MASK_FLAGS  0xC0

#define FLAG_MARK   0x80

typedef uint8_t MalType;
#define TYPE_NIL      ((MalType)0x00)
#define TYPE_FUNCTION ((MalType)0x01)
#define TYPE_ATOM     ((MalType)0x03)
#define TYPE_NUMBER   ((MalType)0x11)
#define TYPE_BOOL     ((MalType)0x12)
#define TYPE_LIST     ((MalType)0x21)
#define TYPE_VECTOR   ((MalType)0x22)
#define TYPE_MAP      ((MalType)0x23)
#define TYPE_STRING   ((MalType)0x31)
#define TYPE_SYMBOL   ((MalType)0x32)
// #define TYPE_KEYWORD   TYPE_SYMBOL /* keywords are symbols */

#define METATYPE_MASK       0x30
#define METATYPE_SCALAR     0x00
#define METATYPE_NUMERIC    0x10
#define METATYPE_CONTAINER  0x20
#define METATYPE_STRING     0x30

#define VAL_METATYPE(val) ((val)->type & METATYPE_MASK)
#define VAL_IS_NUMERIC(val) (VAL_METATYPE(val) == METATYPE_NUMERIC)
#define VAL_IS_CONTAINER(val) (VAL_METATYPE(val) == METATYPE_CONTAINER)

struct List;
struct ENV;
struct MalVal;
struct Function;

typedef struct MalVal {
  struct MalVal *next;      /* used for garbage collection */
  uint8_t type:6;
  uint8_t temp:1;           /* temporary value - don't garbage collect, yet */
  uint8_t mark:1;           /* marked - do not collect garbage */
  union {
    int number;
    bool bool;
    struct List *list;
    struct List *vec;
    struct List *map;
    struct Function *fn;
    struct MalVal *atom;
    char *string;
    void *data;
  } data;
} MalVal;

typedef void (*MalValProc)(MalVal *, void *);

MalVal *malval_create(MalType type);
#define malval_nil() malval_create(TYPE_NIL)
MalVal *malval_bool(bool);
MalVal *malval_symbol(const char *);
MalVal *malval_string(const char *);
MalVal *malval_list(struct List*);
MalVal *malval_vector(struct List*);
MalVal *malval_map(struct List*);
MalVal *malval_function(struct Function*);
MalVal *malval_atom(struct MalVal*);
MalVal *malval_number(int);

void malval_reset_temp(MalVal *, void*);

extern MalVal *_nil, *_true, *_false;

#define NIL _nil
#define T _true
#define F _false
#define VAL_TYPE(val) ((val)->type)
#define VAL_IS_NIL(val) (VAL_TYPE(val) == TYPE_NIL)
#define VAL_IS_TRUE(val) (VAL_TYPE(val) == TYPE_BOOL && (val)->data.bool == TRUE)
#define VAL_IS_FALSE(val) (VAL_TYPE(val) == TYPE_BOOL && (val)->data.bool == FALSE)
#define VAL_FALSE(val) (VAL_IS_NIL(val) || VAL_IS_FALSE(val))
#define VAL_TRUE(val) (!VAL_FALSE(val))
#define VAL_IS_KEYWORD(val) (VAL_TYPE(val) == TYPE_SYMBOL && (val)->data.string[0] == -1)

void malval_free(MalVal*);
unsigned malval_size(MalVal*, bool);

bool malval_equals(MalVal*, MalVal*);

/* map-as-list stuff */
MalVal *map_get(struct List *map, MalVal *key);
#define map_contains(map, key) (map_get((map), (key)) != NULL)
struct List *map_normalise(struct List *map);

#endif /* _MALVAL_H */
