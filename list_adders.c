/*
Alexei Doell cka067 11345642
Kevin Zhang zbk618 11354912
*/

#include <stdint.h>
#include <stdio.h>
#include <list.h>
#include <list_alloc.h>
#include <stdlib.h>
#include <time.h>

/*
 Initializes and returns a pointer to a new list.
*/
LIST* ListCreate(void) {

    unsigned long int list = newList();
    return (LIST*)list;
}

/*
 Adds the new item to list directly after the current item, and makes the new 
 item the current item. If the current pointer is at the end of the list, the 
 item is added at the end.

 Parameters:
    list    a pointer to a LIST struct
    item    a pointer to a generic item to be added as a node

 Return: 0 on success, -1 on failure.
*/
int ListAdd(LIST* list, void* item) {
    unsigned long int n;
    InternalNode* node; 
    InternalNode* cursor;
    InternalNode* next;
    InternalList* internal_list;

    /* create a new node, assign the item, and pull from the table */
    n = newNode();
    if (n == 0) {
        return -1;
    }
    node = getNodeFromTable(n);
    if (node == NULL) {
        return -1;
    }
    node->item = item;
    internal_list = getListFromTable((unsigned long int) list);
    if (internal_list == NULL) {
        return -1;
    }
    cursor = getNodeFromTable(internal_list->cursor);
    if (cursor == NULL && internal_list->size != 0) {
        return -1;
    }
    node->next = 0;

    if (internal_list->size == 0) {
        internal_list->head = n;
        internal_list->tail = n;
    } else if (internal_list->head == internal_list->tail) {
        cursor->next = n;
        node->prev = internal_list->head;
        internal_list->tail = n;
    } else {
        if (cursor->next != 0) {
            next = getNodeFromTable(cursor->next);
            if (next == NULL) {
                return -1;
            }
            next->prev = n;
            node->next = cursor->next; 
        } else {
            internal_list->tail = n;
        }
        cursor->next = n;
        node->prev = internal_list->cursor;
    }
    internal_list->size++;
    internal_list->cursor = n;
    return 0;
}

/*
 Adds item to list directly before the current item, and makes the new item the
 current one. If the current pointer is at the start of the list, the item is 
 added at the start.

 Parameters:
    list    a pointer to a LIST struct
    item    a pointer to a generic item to be added as a node 

 Return: 0 on success, -1 on failure.
*/
int ListInsert(LIST* list, void* item) {
    unsigned long int n;
    InternalNode* node; 
    InternalNode* cursor;
    InternalNode* prev;
    InternalList* internal_list;

    /* create a new node, assign the item, and pull from the table */
    n = newNode();
    if (n == 0) {
        return -1;
    }
    node = getNodeFromTable(n);
    if (node == NULL) {
        return -1;
    }
    node->item = item;
    internal_list = getListFromTable((unsigned long int) list);
    if (internal_list == NULL) {
        return -1;
    }
    cursor = getNodeFromTable(internal_list->cursor);
    if (cursor == NULL && internal_list->size != 0) {
        return -1;
    }
    node->next = 0;

    /* if the list is empty before insertion */
    if (internal_list->size == 0) {
        internal_list->head = n;
        internal_list->tail = n;
    /* if the list contains one node */
    } else if (internal_list->head == internal_list->tail) {
        cursor->prev = n;
        node->next = internal_list->tail;
        internal_list->head = n;
    /* inserstion in the middle of the list */
    } else {
        if (cursor->prev != 0) {
            prev = getNodeFromTable(cursor->prev);
            if (prev == NULL) {
                return -1;
            }
            prev->next = n;
            node->prev = cursor->prev; 
        } else {
            internal_list->head = n;
        }
        cursor->prev = n;
        node->next = internal_list->cursor;
    }
    internal_list->size++;
    internal_list->cursor = n;

    return 0;
}

/*
 adds item to the end of list, and makes the new item the current one. 

 Parameters:
    list    a pointer to a LIST struct
    item    a pointer to a generic item to be added as a node

 Return: 0 on success, -1 on failure.
*/
int ListAppend(LIST* list, void* item) {
    unsigned long int n;
    InternalNode* node; 
    InternalNode* tail;
    InternalList* internal_list;

    /* create a new node, assign the item, and pull from the table */
    n = newNode();
    if (n == 0) {
        return -1;
    }
    node = getNodeFromTable(n);
    if (node == NULL) {
        return -1;
    }
    node->item = item;
    internal_list = getListFromTable((unsigned long int) list);
    if (internal_list == NULL) {
        return -1;
    }
    tail = getNodeFromTable(internal_list->tail);
    if (tail == NULL && internal_list->size != 0) {
        return -1;
    }
    node->next = 0;
    
    /* if the list is empty before insertion */
    if (internal_list->size == 0) {
        internal_list->head = n;
        internal_list->tail = n;
    } else {
        /* insert to the end */
        tail->next = n;
        node->prev = tail->table_entry->index;
        internal_list->tail = n;
    }
    internal_list->cursor = n;    
    internal_list->size++;
    
    return 0;
}

/*
adds item to the front of list, and makes the new item the current one. 

 Parameters:
    list    a pointer to a LIST struct
    item    a pointer to a generic item to be added as a node

 Return: 0 on success, -1 on failure.
*/
int ListPrepend(LIST* list, void* item) {
    unsigned long int n;
    InternalNode* node; 
    InternalNode* head;
    InternalList* internal_list;

    /* create a new node, assign the item, and pull from the table */
    n = newNode();
    if (n == 0) {
        return -1;
    }
    node = getNodeFromTable(n);
    if (node == NULL) {
        return -1;
    }
    node->item = item;
    internal_list = getListFromTable((unsigned long int) list);
    if (internal_list == NULL) {
        return -1;
    }
    head = getNodeFromTable(internal_list->head);
    if (head == NULL && internal_list->size != 0) {
        return -1;
    }
    node->next = 0;
    /* if the list is empty before insertion */
    if (internal_list->size == 0) {
        internal_list->head = n;
        internal_list->tail = n;
    } else {
        /* insert to the beginning */
        head->prev = n;
        node->next = head->table_entry->index;
        internal_list->head = n;
    }
    internal_list->cursor = n;
    internal_list->size++;

    return 0;
}

void ListConcat(LIST* list1, LIST* list2) {
    InternalList* internal_first;
    InternalList* internal_second;
    InternalNode* front_tail;
    InternalNode* second_head;

    internal_first = getListFromTable((unsigned long)list1);
    if (internal_first == NULL) {
        printf("list1 does not exist\n");
        return;
    }
    internal_second = getListFromTable((unsigned long)list2);
    if (internal_second == NULL) {
        printf("list2 does not exist\n");
        return;
    }
    front_tail = getNodeFromTable(internal_first->tail);
    second_head = getNodeFromTable(internal_second->head);
    if (front_tail != NULL) {
        /* we know the list exists, so if the tail does not, then the
           list is empty */
        front_tail->next = internal_second->head;
    }
    if (second_head != NULL) {
        second_head->prev = internal_first->tail;
    }
    if (internal_first->head == 0) {
        internal_first->head = internal_second->head;
    }
    if (internal_second->tail != 0) {
        internal_first->tail = internal_second->tail;
    }

    internal_first->size += internal_second->size;

    destroyList((unsigned long)list2);
}


