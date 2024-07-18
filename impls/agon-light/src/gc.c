/*
 * Simple mark and sweep garbage collector
 */
#include <stddef.h>
#include "gc.h"
#include "env.h"
#include "function.h"
#include "eval.h"
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
      list_foreach(val->data.vec->vec, gc_mark, data);
      gc_mark(val->data.vec->meta, data);
      break;
    case TYPE_LIST: 
      list_foreach(val->data.list->list, gc_mark, data);
      gc_mark(val->data.list->meta, data);
      break;
    case TYPE_MAP:
      list_foreach(val->data.map->map, gc_mark, data);
      gc_mark(val->data.map->meta, data);
      break;
    case TYPE_FUNCTION:
      function_gc_mark(val->data.fn->fn, data);
      gc_mark(val->data.fn->meta, data);
      break;
    case TYPE_ATOM:
      gc_mark(val->data.atom, data);
      break;
  }
}

void gc(bool force)
{
  if (!force && (values_count <= values_max))
    return;

  env_mark_all();
  gc_mark(exception, NULL);
  sweep();

  values_max = 2 * values_count;
}

void gc_add(MalVal *value)
{
  value->next = all_values;
  all_values = value;
  values_count++;
}

MalVal *gc_pop(void)
{
  if (!all_values)
    return NULL;

  MalVal *result = all_values;
  all_values = all_values->next;
  values_count--;

  return result;
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
