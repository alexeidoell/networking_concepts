/*
Alexei Doell cka067 11345642
Kevin Zhang zbk618 11354912
*/

#include <list_alloc.h>
#include <stddef.h>
#include <list.h>
#include <stdio.h>

/*
 Return current item and take it out of list. Make the next item the current 
 one.

 Parameters:
    list    a pointer to a LIST struct.
*/
void *ListRemove(LIST *list) {
    unsigned long cursorIdx;
    InternalNode* cursor;
    InternalNode* next;
    InternalNode* prev;
    InternalList* internal_list;
    void* item;

    internal_list = getListFromTable((unsigned long int) list);
    if (internal_list == NULL) {
        return NULL;
    }
    if (internal_list->size == 0) {
        return NULL;
    }
    cursor = getNodeFromTable(internal_list->cursor);
    if (cursor == NULL) {
        return NULL;
    }

    item = cursor->item;
    if (internal_list->head == internal_list->tail) {
        internal_list->head = 0;
        internal_list->tail = 0;
        internal_list->cursor = 0;
    } else {
        if (cursor->next != 0) {
            next = getNodeFromTable(cursor->next);
            next->prev = cursor->prev;
        } 
        if (cursor->prev != 0) {
            prev = getNodeFromTable(cursor->prev);
            prev->next = cursor->next;
        }
    }
    
    cursorIdx = cursor->table_entry->index;
    if (cursor->next == 0) {
        internal_list->cursor = cursor->prev;
        internal_list->tail = internal_list->cursor;
    } else if (cursor->prev == 0) {
        internal_list->cursor = cursor->next;
        internal_list->head = internal_list->cursor;
    } else {
        internal_list->cursor = cursor->next;
    }
    destroyNode(cursorIdx);
    internal_list->size--;
    return item;
}

/*
 Delete list and free all nodes. 

 Parameters:
    list        a pointer to a LIST struct.
    itemFree    a pointer to a function that frees an item
*/
void ListFree(LIST *list, int (*itemFree)(void *)) {
    InternalNode* curr_node;
    unsigned long next_node;
    int i;
    InternalList* internal_list;
    internal_list = getListFromTable((unsigned long)list);
    curr_node = getNodeFromTable((unsigned long)internal_list->head);
    i = 0;
    if (curr_node != NULL) {
        while (curr_node->next != 0) {
            i++;
            itemFree(curr_node->item);
            if (curr_node->table_entry->index == internal_list->head) {
                curr_node = getNodeFromTable((unsigned long)curr_node->next);
                continue;
            }
            next_node = curr_node->next;
            destroyNode(curr_node->prev);
            curr_node = getNodeFromTable(next_node);
        }
        destroyNode(curr_node->table_entry->index);
    }
    destroyList((unsigned long)list);
}

/*
   Return last item and take it out of list. The current pointer shall be the 
   new last item in the list.

Parameters:
list    a pointer to a LIST struct.
*/
void* ListTrim(LIST *list) {
    InternalNode* tail;
    InternalNode* prev;
    void* item;
    InternalList* internal_list;

    internal_list = getListFromTable((unsigned long int) list);
    if (internal_list == NULL) {
        return NULL;
    }
    if (internal_list->size == 0) {
        return NULL;
    }

    tail = getNodeFromTable(internal_list->tail);

    if (internal_list->head == internal_list->tail) {
        internal_list->head = 0;
        internal_list->tail = 0;
        internal_list->cursor = 0;
    } else {
        prev = getNodeFromTable(tail->prev);
        prev->next = 0;
        internal_list->tail = prev->table_entry->index;
    }
    item = tail->item;
    internal_list->cursor = tail->prev;
    destroyNode(tail->table_entry->index);
    internal_list->size--;
    return item;
}
