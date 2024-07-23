#include <string.h>

#include "heap.h"
#include "list.h"
#include "itoa.h"
#include "printer.h"
#include "str.h"
#include "function.h"

static char *pr_str_readable(const char *);

char *pr_str(const MalVal *val, bool readable)
{
  char buf[24];

  if (!val)
    return strdup("null");

  switch(val->type) {
    case TYPE_FLOAT:
      ftoa(VAL_FLOAT(val), buf, 6);
      return strdup(buf);

    case TYPE_NUMBER:
      itoa(VAL_NUMBER(val), buf, 10);
      return strdup(buf);

    case TYPE_BYTE:
      if (VAL_BYTE(val) < 16) {
        strcpy(buf, "0x0");
        itoa(VAL_BYTE(val), &buf[3], 16);
      } else {
        strcpy(buf, "0x");
        itoa(VAL_BYTE(val), &buf[2], 16);
      }
      return strdup(buf);

    case TYPE_LIST:
      return pr_str_list(VAL_LIST(val), readable);

    case TYPE_VECTOR:
      return pr_str_vector(VAL_VEC(val), readable);

    case TYPE_MAP:
      return pr_str_map(VAL_MAP(val), readable);

    case TYPE_SET:
      return pr_str_set(VAL_SET(val), readable);

    case TYPE_STRING:
      if (readable) {
        return pr_str_readable(VAL_STRING(val));
      }
      return strdup(VAL_STRING(val));

    case TYPE_SYMBOL:
      if (VAL_IS_KEYWORD(val)) {
        /* is keyword */
        char *s = strdup(VAL_STRING(val));
        s[0] = ':';
        return s;
      }
      return strdup(VAL_STRING(val));

    case TYPE_NIL:
      return strdup("nil");

    case TYPE_BOOL:
      return strdup(VAL_IS_TRUE(val) ? "true" : "false");

    case TYPE_FUNCTION: {
      Function *f = VAL_FUNCTION(val);
      if (f->is_builtin) {
        return strdup("#<builtin>");
      } else {
        char *s = strdup(f->is_macro ? "#<macro> " : "#<function> ");
        struct Body *body = f->fn.bodies;
        while (body) {
          char *b = pr_str(body->body, TRUE);
          catstr(&s, b);
          if (body->next)
            catstr(&s, "\n");
          heap_free(b);
          body = body->next;
        }

        return s;
      }

      return strdup("#<function>");
    }

    case TYPE_ATOM: {
      char *s = strdup("(atom ");
      char *s1 = pr_str(val->data.atom, TRUE);
      catstr(&s, s1);
      catstr(&s, ")");
      heap_free(s1);
      return s;
    }
      
  }

  return strdup("Unknown");
}

static char *pr_str_container(char pfx, char sfx, List *list, bool readable)
{
  char *s = heap_malloc(3);
  s[0] = pfx;
  s[1] = '\0';
  unsigned len = 1;

  for (List *rover = list; rover; rover = rover->tail) {
    char *s2 = pr_str(rover->head, readable);
    unsigned newlen = len + strlen(s2) + 1;
    char *newstr = heap_malloc(newlen + 2);

    strcpy(newstr, s);
    strcat(newstr, s2);

    newstr[newlen - 1] = ' ';
    newstr[newlen] = '\0';

    heap_free(s2);
    heap_free(s);

    s = newstr;
    len = newlen;
  }

  if (len > 1) {
    s[len-1] = sfx;
    s[len] = '\0';
  }
  else {
    s[1] = sfx;
    s[2] = '\0';
  }

  return s;

}

char *pr_str_list(List *list, bool readable)
{
  return pr_str_container('(', ')', list, readable);
}

char *pr_str_vector(Vec *vec, bool readable)
{
  List *l = list_from_vec(vec);
  char *rv = pr_str_container('[', ']', l, readable);
  list_release(l);
  return rv;
}

struct pr_map_t {
  char *s;
  bool readable;
  bool show_val;
};

static void _pr_map(MalVal *key, MalVal *val, void *_data)
{
  struct pr_map_t *pr = _data;
  char *k = pr_str(key, pr->readable);
  catstr(&pr->s, k);
  catstr(&pr->s, " ");
  heap_free(k);
  if (pr->show_val) {
    char *v = pr_str(val, pr->readable);
    catstr(&pr->s, v);
    catstr(&pr->s, " ");
    heap_free(v);
  }
}

char *pr_str_map(Map *map, bool readable)
{
  if (map_count(map) == 0)
    return strdup("{}");

  struct pr_map_t pr = { strdup("{"), readable, TRUE };

  map_foreach(map, _pr_map, &pr);
  pr.s[strlen(pr.s)-1] = '\0';

  catstr(&pr.s, "}");

  return pr.s;
}

char *pr_str_set(Map *set, bool readable)
{
  if (map_count(set) == 0)
      return strdup("#{}");

  struct pr_map_t pr = { strdup("#{"), readable, FALSE };

  map_foreach(set, _pr_map, &pr);
  pr.s[strlen(pr.s)-1] = '\0';

  catstr(&pr.s, "}");

  return pr.s;

}

static char *catchar(char **sptr, char c)
{
  unsigned len = strlen(*sptr);
  char *s = heap_malloc(len + 3);
  strcpy(s, *sptr);

  switch (c) {
    case '"':
      strcat(&s[len], "\\\"");
      break;
    case '\\':
      strcat(&s[len], "\\\\");
      break;
    case 0x09:
      strcat(&s[len], "\\t");
      break;
    case 0x0a:
      strcat(&s[len], "\\n");
      break;
    default:
      s[len] = c;
      s[len+1] = '\0';
      break;
  }
  heap_free(*sptr);
  *sptr = s;
  return s;
}

char *pr_str_readable(const char *in)
{
  char *s = heap_malloc(2);
  s[0] = '"';
  s[1] = '\0';

  for (const char *c = in; *c; c++) {
    s = catchar(&s, *c);
  }

  unsigned l = strlen(s);
  char *rv = heap_malloc(l + 2);
  strcpy(rv, s);
  rv[l] = '"';
  rv[l+1] = '\0';
  heap_free(s);

  return rv;
}
