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

  switch(val->type) {
    case TYPE_NUMBER:
      itoa(val->data.number, buf, 10);
      return strdup(buf);

    case TYPE_LIST:
      return pr_str_list(val->data.list, readable);

    case TYPE_VECTOR:
      return pr_str_vector(val->data.vec, readable);

    case TYPE_MAP:
      return pr_str_map(val->data.map, readable);

    case TYPE_STRING:
      if (readable) {
        return pr_str_readable(val->data.string);
      }
      return strdup(val->data.string);

    case TYPE_SYMBOL:
      if (val->data.string[0] == -1) {
        /* is keyword */
        char *s = heap_malloc(strlen(val->data.string) + 1);
        s[0] = ':';
        strcpy(&s[1], &val->data.string[1]);
        return s;
      }
      return strdup(val->data.string);

    case TYPE_NIL:
      return strdup("nil");

    case TYPE_BOOL:
      return strdup(val->data.bool ? "true" : "false");

    case TYPE_FUNCTION: {
      Function *f = val->data.fn;
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

char *pr_str_vector(List *list, bool readable)
{
  return pr_str_container('[', ']', list, readable);
}

char *pr_str_map(List *map, bool readable)
{
  return pr_str_container('{', '}', map, readable);
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
