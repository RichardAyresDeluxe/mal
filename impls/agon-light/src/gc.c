/*
 * Simple mark and sweep garbage collector
 */
#include <stddef.h>
#include "gc.h"
#include "mallist.h"
#include "env.h"

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
  if (val->mark)
    return;

  val->mark = 1;

  switch (val->type) {
    case TYPE_VECTOR: 
      mallist_foreach(val->data.vec, gc_mark, NULL);
      break;
    case TYPE_LIST: 
      mallist_foreach(val->data.list, gc_mark, NULL);
      break;
  }
}

void gc(bool force)
{
  if (!force && (values_count <= values_max))
    return;

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

void gc_mark_env(struct ENV *env, void *data)
{
  for (ENV *rover = env; rover && rover->name != NULL; rover++)
    gc_mark(rover->value, data);
}
