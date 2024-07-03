#ifndef _ERR_H
#define _ERR_H

#define ERR_NONE               0
#define ERR_OUT_OF_MEMORY      1
#define ERR_INDEX              2
#define ERR_COMPARE            3
#define ERR_NOT_IMPLEMENTED    4
#define ERR_TOO_LARGE          5
#define ERR_ARGUMENT_MISMATCH  6
#define ERR_SYMBOL_NOT_FOUND   7
#define ERR_OVERFLOW           8
#define ERR_SYNTAX_ERROR       9
#define ERR_INVALID_OPERATION 10
#define ERR_UNKNOWN_ARGUMENT  11
#define ERR_FILE_ERROR        12
#define ERR_LEXER_ERROR 	    13
/* TODO: always update the assembler definitions in common.inc */

/* fmt can be NULL */
extern void err_warning(int err, const char *fmt, ...);
extern void err_fatal(int err, const char *fmt, ...);
// #ifndef NDEBUG
// extern void log_debug(const char *fmt, ...);
// #else
// static inline void log_debug(const char *fmt, ...) {};
// #endif
// extern void log_info(const char *fmt, ...);
// extern void log_warning(const char *fmt, ...);
// extern void log_error(const char *fmt, ...);


#endif /* _ERR_H */
