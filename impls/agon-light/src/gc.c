/*
 * Simple mark and sweep garbage collector
 */
#include <stddef.h>
#include "gc.h"
#include "env.h"

extern ENV *repl_env;

static MalVal *all_values = NULL;
static unsigned values_count = 0;
static unsigned values_max = 64;

static void sweep(void)
{
  MalVal **rover = &all_values;
  while (*rover) {
    if (rover[0]->mark) {
      rover[0]->mark = 0;
      rover = &rover[0]->next;
      continue;
    }

    MalVal *unreached = *rover;
    *rover = unreached->next;
    malval_free(unreached);
    values_count--;
  }
}

void gc_mark(MalVal *val, void *data)
{
  if (!val || val->mark)
    return;

  val->mark = 1;

  switch (val->type) {
    case TYPE_VECTOR: 
      list_foreach(val->data.vec, gc_mark, NULL);
      break;
    case TYPE_LIST: 
      list_foreach(val->data.list, gc_mark, NULL);
      break;
    case TYPE_MAP:
      list_foreach(val->data.map, gc_mark, NULL);
      break;
  }
}

void gc(bool force)
{
  if (!force && (values_count <= values_max))
    return;

  gc_mark_env(repl_env, NULL);
  sweep();

  values_max = 2 * values_count;
}

void gc_add(MalVal *value)
{
  value->next = all_values;
  all_values = value;
  values_count++;
}

void value_info(unsigned *count, unsigned *size)
{
  *count = 0;
  *size = 0;
  for (MalVal *rover = all_values; rover != NULL; rover = rover->next) {
    (*count)++;
    *size = *size + malval_size(rover, FALSE);
  }
}

void gc_mark_list(List *list, void *data)
{
  list_foreach(list, gc_mark, data);
}
