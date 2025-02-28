/*
Alexei Doell cka067 11345642
Kevin Zhang zbk618 11354912
*/
#include "list_alloc.h"
#include <stddef.h>
#include <list.h>
#include <stdio.h>

/*
 Returns the number of items in the list

 Parameters:
    list    a pointer to a LIST struct

 Return: the number of items in the list
*/
int ListCount(LIST *list) {
    if (list == NULL) {
        return 0;
    }

    return getListFromTable((unsigned long int)list)->size;
}

/*
 Returns a pointer to the first item in the list and moves the cursor to the 
 first item.

 Parameters:
    list    a pointer to a LIST struct
 Return value:
    void* to item at head

*/
void* ListFirst(LIST *list) {
    InternalList* internal_list;
    if (list == NULL) {
        return NULL;
    }
    internal_list = getListFromTable((unsigned long int)list);
    if (internal_list == NULL) {
        return NULL;
    }
    if (internal_list->size == 0) {
        return NULL;
    }
    internal_list->cursor = internal_list->head;
    /* if internal_list->size != 0 then the node must exist */
    return getNodeFromTable(internal_list->cursor)->item;
}

/*
 Returns a pointer to the last item in list and makes the last item the current 
 one.

 Parameters:
    list    a pointer to a LIST struct
 Return value:
    void* to item at tail
*/
void* ListLast(LIST *list) {
    InternalList* internal_list;
    if (list == NULL) {
        return NULL;
    }
    internal_list = getListFromTable((unsigned long int)list);
    if (internal_list == NULL) {
        return NULL;
    }
    if (internal_list->size == 0) {
        return NULL;
    }
    internal_list->cursor = internal_list->tail;
    /* if internal_list->size != 0 then the node must exist */
    return getNodeFromTable(internal_list->cursor)->item;
}

/*
 Advances the cursor by one, and returns a pointer to the new current item. If
 this operation attempts to advance the current item beyond the end of the list,
 a NULL pointer is returned.
   
 Parameters:
    list    a pointer to a LIST struct
 Return value:
    void* to item at item after cursor
*/
void* ListNext(LIST *list) {
    InternalList* internal_list;
    unsigned int next;
    if (list == NULL) {
        return NULL;
    }
    internal_list = getListFromTable((unsigned long int)list);
    if (internal_list == NULL) {
        return NULL;
    }
    if (internal_list->size == 0) {
        return NULL;
    }
    if (internal_list->cursor == internal_list->tail) {
        return NULL;
    }
    next = getNodeFromTable(internal_list->cursor)->next;
    internal_list->cursor = next;
    return getNodeFromTable(next)->item;
}

/*
 Backs up the list's cursor by one, and returns a pointer to the new 
 current item. If this operation attempts to back up the current item beyond 
 the start of the list, a NULL pointer is returned

 Parameters:
    list    a pointer to a LIST struct
 Return value:
    void* to item at item before cursor
*/
void* ListPrev(LIST *list) {
    InternalList* internal_list;
    unsigned int prev;
    if (list == NULL) {
        return NULL;
    }
    internal_list = getListFromTable((unsigned long int)list);
    if (internal_list == NULL) {
        return NULL;
    }
    if (internal_list->size == 0) {
        return NULL;
    }
    if (internal_list->cursor == internal_list->head) {
        return NULL;
    }
    prev = getNodeFromTable(internal_list->cursor)->prev;
    internal_list->cursor = prev;
    return getNodeFromTable(prev)->item;

}

/*
 Returns the current cursor position.

 Parameters:
    list    a pointer to a LIST struct
 Return value:
    void* to item at item at cursor
*/
void* ListCurr(LIST *list) {
    InternalList* real_list;
    if (list == NULL) {
        return NULL;
    }
    real_list = getListFromTable((unsigned long int) list);
    if (real_list->cursor == 0) {
        return NULL;
    }

    return getNodeFromTable(real_list->cursor)->item;
}

/*
 Searches list starting at the current item until the end is reached or a match
 is found. In this context, a match is determined by the comparator parameter. 
 This parameter is a pointer to a routine that takes as its first argument an 
 item pointer, and as its second argument comparisonArg. Comparator returns 0 
 if the item and comparisonArg don't match (i.e. didn't find it), or 1 if they 
 do (i.e. found it). Exactly what constitutes a match is up to the implementor 
 of comparator. If a match is found, the current pointer is left at the matched
 item and the pointer to that item is returned. If no match is found, the 
 current pointer is left at the end of the list and a NULL pointer is returned. 

 Parameters:
    list            a pointer to a LIST struct
    comparator      a pointer to a function that compares an item
    comparisonArg   a pointer to an item that is compared with an item
*/
void* ListSearch(LIST *list, int (*comparator)(void *, void*),
        void *comparisonArg) {
    InternalNode* curr;
    InternalList* internal_list;

    internal_list = getListFromTable((unsigned long) list);
    curr = getNodeFromTable(internal_list->cursor);

    while (curr != NULL) {
       if (comparator(curr->item, comparisonArg)) {
           internal_list->cursor = curr->table_entry->index;
           return curr->item;
       }
       curr = getNodeFromTable((unsigned long)curr->next);
    }
    return NULL;
}
