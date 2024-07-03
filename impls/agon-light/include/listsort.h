#ifndef _LISTSORT_H
#define _LISTSORT_H

#include <stdint.h>

typedef int8_t (*sort_compare_func)(void *, void *, void *);
void linked_list_sort_raw(void** head_ref, sort_compare_func compfn, void *data);
void linked_list_reverse(void **head_ref);

#endif /* _LISTSORT_H */
