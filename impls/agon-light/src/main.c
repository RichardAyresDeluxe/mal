#include <stdio.h>
#include <stdlib.h>

#include "malval.h"
#include "gc.h"
#include "reader.h"
#include "printer.h"

MalVal *EVAL(MalVal *data)
{
  gc_mark(data, NULL);
  gc(FALSE);
  return data;
}

MalVal *READ(void)
{
  return read_str();
}

const char *PRINT(const MalVal *val)
{
  return pr_str(val, TRUE);
}

const char *rep(void)
{
  MalVal *val = READ();
  val = EVAL(val);
  const char *s = PRINT(val);
  return s;
}
  
int main(int argc, char **argv)
{
  while (1) {
    const char *s = rep();
    if (!s) {
      gc(TRUE);
      unsigned count, size;
      value_info(&count, &size);
      printf("\nValues remaining: %u (%u bytes)\n", count, size);
      exit(0);
    }
    fputs(s, stdout);
    fputc('\n', stdout);
    gc(FALSE);
  }
}

int fputs(const char *s, FILE *stream)
{
  int c = 0;
  while (s && *s != '\0') {
    fputc(*s++, stream);
    c++;
  }
  return c;
}
