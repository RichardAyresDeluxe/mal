#ifndef _READER_H
#define _READER_H

#include "malval.h"
#include "env.h"

extern MalVal *read_str(void);
extern MalVal *read_string(char *s);
extern MalVal *load_file(const char *, ENV *);

#endif /*_READER_H*/
