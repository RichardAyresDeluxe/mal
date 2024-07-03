#include <string.h>

#include "heap.h"
#include "mallist.h"
#include "itoa.h"
#include "printer.h"

extern char *strdup(const char *);

static const char *pr_str_list(MalList *list);

const char *pr_str(const MalVal *val)
{
  char buf[24];

  switch(val->type) {
    case TYPE_NUMBER:
      itoa(val->data.number, buf, 10);
      return strdup(buf);

    case TYPE_LIST:
      return pr_str_list(val->data.list);

    case TYPE_SYMBOL:
      return strdup(val->data.symbol);
  }

  return strdup("Unknown");
}

const char *pr_str_list(MalList *list)
{
  char *s = strdup("(");
  unsigned len = 1;

  for (MalList *rover = list; rover; rover = rover->next) {
    const char *s2 = pr_str(rover->value);
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

  s[len-1] = ')';
  s[len] = '\0';

  return s;
}
