/**
 * Merge sort algorithm, shamelessly stolen from ChatGPT, who stole it from
 * some other poor sap */
#include "listsort.h"

#include <stdlib.h>

struct linked_list {
    struct linked_list *next;
};

// static int8_t compare_values(void *a, void *b, void *data)
// {
//     value_compare_func *compfn = (value_compare_func*)data;
//     return (*compfn)(((list_t*)a)->v, ((list_t*)b)->v);
// }
//
// /* Merge sort function */
// void list_sort_inplace(struct list** head_ref, value_compare_func compfn)
// {
//     linked_list_sort_raw((void**)head_ref, compare_values, (void*)&compfn);
// }

#if !defined(EZ80)
/* Function to merge two sorted lists */
static
struct linked_list* merge_raw(
    struct linked_list* l1,
    struct linked_list* l2,
    sort_compare_func compfn,
    void *data
) {
    if (l1 == NULL)
        return l2;
    if (l2 == NULL)
        return l1;
    
    struct linked_list* result = NULL;

    if (compfn(l1, l2, data) < 0) {
        result = l1;
        result->next = merge_raw(l1->next, l2, compfn, data);
    } else {
        result = l2;
        result->next = merge_raw(l1, l2->next, compfn, data);
    }
    
    return result;
}

/* Function to split the list into two halves */
static void split(
    struct linked_list* source,
    struct linked_list** front,
    struct linked_list** back
) {
    struct linked_list* fast = source->next;
    struct linked_list* slow = source;
    
    while (fast != NULL) {
        fast = fast->next;
        if (fast != NULL) {
            slow = slow->next;
            fast = fast->next;
        }
    }
    
    *front = source;
    *back = slow->next;
    slow->next = NULL;
}

void linked_list_sort_raw(void** head_ref, sort_compare_func compfn, void *data)
{
    struct linked_list* head = *((struct linked_list**)head_ref);
    struct linked_list* a;
    struct linked_list* b;
    
    if (head == NULL || head->next == NULL)
        return;
    
    split(head, &a, &b);
    
    linked_list_sort_raw((void**)&a, compfn, data);
    linked_list_sort_raw((void**)&b, compfn, data);
    
    *head_ref = merge_raw(a, b, compfn, data);
}


void linked_list_reverse(void **head_ref)
{
    struct linked_list *head = *((struct linked_list**)head_ref);
    struct linked_list *reversed = NULL;

    while (head) {
        struct linked_list *next = head->next;
        head->next = reversed;
        reversed = head;
        head = next;
    }

    *head_ref = (void*)reversed;
}
#endif
