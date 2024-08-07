#ifndef _STR_H
#define _STR_H

#include <stdint.h>

/**
 * Replacement for standard strdup() - which isn't in C99.
 * This version uses our heap_malloc(), so will abort if
 * no memory, and possibly garbage collect in the future
 */
extern char *strdup(const char*);

/**
 * Allocate a new string (using heap_malloc) which is
 * the concatenation of the two supplied strings. 
 * De-allocate the old string, set the first parameter to
 * point to the new string, and return the new string.
 */
extern char *catstr(char **, const char *);

extern uint16_t string_hash(const char *s);
extern uint16_t symbol_hash(const char *s);

#endif /* _STR_H */
