#include <stdio.h>
#include <stdlib.h>

#include "err.h"

void err_warning(int err, const char *msg, ...)
{
  fputs("WARNING: ", stderr);
  if (msg)
    fputs(msg, stderr);
  fputc('\n', stderr);
}

void err_fatal(int err, const char *msg, ...)
{
  fputs("FATAL: ", stderr);
  if (msg)
    fputs(msg, stderr);
  fputc('\n', stderr);
  exit(err);
}
