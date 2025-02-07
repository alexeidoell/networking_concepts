/*
Alexei Doell cka067 11345642
Kevin Zhang zbk618 11354912
*/

#include <list.h>
#include <list_alloc.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LISTMIN 3 
#define NODEMIN 3

static ListMem g_list_mem;
static NodeMem g_node_mem;
static InternalList** g_list_table;
static InternalNode** g_node_table;
static FreeNodeTable* g_free_node_table_ptr;
static FreeNodeTable* g_free_node_table;
static FreeListTable* g_free_list_table;
static FreeListTable* g_free_list_table_ptr;


/*
 Initializes the memory for all list and node structs.
*/
int listInit(void) {
    int i;

    memset(&g_list_mem, 0, sizeof(ListMem));
    g_list_mem.listArr = calloc(LISTMIN, sizeof(InternalList));
    if (g_list_mem.listArr == NULL) {
        return -1; 
    }
    g_list_mem.size = LISTMIN;
    memset(&g_node_mem, 0, sizeof(NodeMem));
    g_node_mem.nodeArr = calloc(NODEMIN, sizeof(InternalNode));
    if (g_node_mem.nodeArr == NULL) {
        return -1; 
    }
    g_node_mem.size = NODEMIN;
    g_list_table = calloc(LISTMIN, sizeof(InternalList*));
    if (g_list_table == NULL) {
        return -1; 
    }
    g_node_table = calloc(NODEMIN, sizeof(InternalNode*));
    if (g_node_table == NULL) {
        return -1; 
    }
    g_node_mem.table_size = NODEMIN;
    g_free_node_table_ptr = calloc(NODEMIN, sizeof(FreeNodeTable));
    if (g_free_node_table_ptr == NULL) {
        return -1; 
    }
    g_list_mem.table_size = LISTMIN;
    g_free_list_table_ptr = calloc(NODEMIN, sizeof(FreeListTable));
    if (g_free_list_table_ptr == NULL) {
        return -1; 
    }


    /* after allocing the memory for the lists/nodes and the tables
       we need to then initialize them with a spot in the table */
    g_list_mem.free_lists = &g_list_mem.listArr[0];
    for (i = 1; i < LISTMIN; ++i) {
        g_list_mem.listArr[i - 1].next_free = &g_list_mem.listArr[i];
    }
    g_list_mem.listArr[i - 1].next_free = NULL;

    g_node_mem.free_nodes = &g_node_mem.nodeArr[0];
    for (i = 1; i < NODEMIN; ++i) {
        g_node_mem.nodeArr[i - 1].next_free = &g_node_mem.nodeArr[i];
    }
    g_node_mem.nodeArr[i - 1].next_free = NULL;

    g_free_list_table_ptr[0].index = 1;
    for (i = 1; i < LISTMIN; ++i) {
        g_free_list_table_ptr[i].index = i + 1;
        g_free_list_table_ptr[i - 1].next = &g_free_list_table_ptr[i];
    }
    g_free_list_table_ptr[i - 1].next = NULL;

    g_free_node_table_ptr[0].index = 1;
    for (i = 1; i < NODEMIN; ++i) {
        g_free_node_table_ptr[i].index = i + 1;
        g_free_node_table_ptr[i - 1].next = &g_free_node_table_ptr[i];
    }
    g_free_node_table_ptr[i - 1].next = NULL;

    g_free_list_table = g_free_list_table_ptr;
    g_free_node_table = g_free_node_table_ptr;
    return 0;
}

/*
 Increases the memory for lists by calling realloc and multiplying the original
 size by 2. Also does the same for the list table containing the lists structs.

 Returns: 0 on success, -1 on failure 
*/
int listMemIncrease(void) {
    InternalList* list_tmp;
    InternalList* curr_list;
    InternalList** table_tmp;
    FreeListTable* free_lists_tmp;
    ptrdiff_t offset;
    unsigned int i;
    unsigned long old_free_head = 0;
    list_tmp = realloc(g_list_mem.listArr,
            2 * g_list_mem.size * sizeof(InternalList));
    if (list_tmp == NULL) {
        return -1;
    }
    memset(list_tmp + g_list_mem.size, 
            0, g_list_mem.size * sizeof(InternalList));
    if (g_list_mem.table_size == g_list_mem.used_lists) {
        table_tmp = realloc(g_list_table,
                2 * g_list_mem.size * sizeof(InternalList*));
        if (table_tmp == NULL) {
            return -1;
        } else {
            memset(table_tmp + g_list_mem.size, 
                    0, g_list_mem.size * sizeof(InternalList*));
            g_list_table = table_tmp;
        }
        if (g_free_list_table != NULL) {
            old_free_head = g_free_list_table->index;
        }
        free_lists_tmp = realloc(g_free_list_table_ptr, 
                2 * g_list_mem.size * sizeof(FreeListTable));
        if (free_lists_tmp == NULL) {
            return -1;
        } else {
            memset(free_lists_tmp + g_list_mem.size,
                    0, g_list_mem.size * sizeof(FreeListTable));
            g_free_list_table_ptr = free_lists_tmp;
        }
        for (i = 0; i < 2 * g_list_mem.size; ++i) {
            if (g_free_list_table_ptr[i].index == 0) {
                g_free_list_table_ptr[i].index = i + 1;
            }
            if (i != 0) {
                g_free_list_table_ptr[i - 1].next = &g_free_list_table_ptr[i];
            }
        }
        if (old_free_head != 0) {
            g_free_list_table_ptr[i - 1].next = 
                &g_free_list_table_ptr[old_free_head - 1];
        } else {
            g_free_list_table_ptr[i - 1].next = NULL;
        }
        g_free_list_table = &g_free_list_table_ptr[g_list_mem.size];
        g_list_mem.table_size *= 2;
    }
    if (list_tmp != g_list_mem.listArr) {
        offset = (char*)g_list_mem.listArr - (char*)list_tmp;
        g_list_mem.listArr = list_tmp;
        for (i = 0; i < g_list_mem.table_size; ++i) {
            g_list_table[i] = (InternalList*)((char*)g_list_table[i] - offset);
        }
    }
    g_list_mem.free_lists = &g_list_mem.listArr[g_list_mem.size];
    for (i = g_list_mem.size; i < 2 * g_list_mem.size; ++i) {
        g_list_mem.listArr[i - 1].next_free = &g_list_mem.listArr[i];
    }
    for (i = 0; i < g_list_mem.size; ++i) {
        curr_list = getListFromTable(g_free_list_table_ptr[i].index);
        curr_list->table_entry = &g_free_list_table_ptr[i];
    }
    g_list_mem.size *= 2;
    return 0;
}


/*
 Reduces the allocated list memory by half when called.

 Returns: -1 on failure, 0 on success.
*/
int listMemDecrease(void) {
    InternalList* list_tmp;
    InternalList* curr_list;
    InternalList* old_ptr;
    unsigned int counter = 0;
    unsigned int i;
    /* temp smaller node memory */
    list_tmp = malloc(g_list_mem.size * sizeof(InternalList) / 2);
    if (list_tmp == NULL) {
        return -1;
    }
    for (i = 0; i < g_list_mem.size; ++i) {
        curr_list = &(g_list_mem.listArr[i]);
        if (curr_list->in_use == true) {
            g_list_table[curr_list->table_entry->index - 1] = 
            memmove(list_tmp + counter, curr_list, sizeof(InternalList));
            counter++;
        }
    }
    g_list_mem.size /= 2;
    old_ptr = g_list_mem.listArr;
    g_list_mem.listArr = list_tmp;
    free(old_ptr);
    old_ptr = NULL;
    return 0;
}

/*
 Increases the memory for nodes by calling realloc and multiplying the original
 size by 2. Also does the same for the list table containing the lists structs.

Returns: 0 on success, -1 on failure 
*/
int nodeMemIncrease(void) {
    InternalNode* node_tmp;
    InternalNode* curr_node;
    InternalNode** table_tmp;
    ptrdiff_t offset;
    unsigned long old_free_head = 0;
    unsigned int i;
    FreeNodeTable* free_nodes_tmp;
    node_tmp = realloc(g_node_mem.nodeArr,
            2 * g_node_mem.size * sizeof(InternalNode));
    if (node_tmp == NULL) {
        return -1;
    }
    memset(node_tmp + g_node_mem.size, 0,
            g_node_mem.size * sizeof(InternalNode));
    if (g_node_mem.table_size == g_node_mem.used_nodes) {
        table_tmp = realloc(g_node_table, 
                2 * g_node_mem.size * sizeof(InternalNode*));
        if (table_tmp == NULL) {
            return -1;
        } else {
            memset(table_tmp + g_node_mem.size,
                    0, g_node_mem.size * sizeof(InternalNode*));
            g_node_table = table_tmp;
        }
        if (g_free_node_table != NULL) {
            old_free_head = g_free_node_table->index;
        }
        free_nodes_tmp = realloc(g_free_node_table_ptr, 
                2 * g_node_mem.size * sizeof(FreeNodeTable));
        if (free_nodes_tmp == NULL) {
            return -1;
        } else {
            memset(free_nodes_tmp + g_node_mem.size,
                    0, g_node_mem.size * sizeof(FreeNodeTable));
            g_free_node_table_ptr = free_nodes_tmp;
        }
        
        for (i = 0; i < 2 * g_node_mem.size; ++i) {
            if (g_free_node_table_ptr[i].index == 0) {
                g_free_node_table_ptr[i].index = i + 1;
            }
            if (i != 0) {
                g_free_node_table_ptr[i - 1].next = &g_free_node_table_ptr[i];
            }
        }
        if (old_free_head != 0) {
        g_free_node_table_ptr[i - 1].next = 
            &g_free_node_table_ptr[old_free_head - 1];
        } else {
            g_free_node_table_ptr[i - 1].next = NULL;
        }
        g_free_node_table = &g_free_node_table_ptr[g_node_mem.size];
        g_node_mem.table_size *= 2;
    }
    if (node_tmp != g_node_mem.nodeArr) {
        offset = (char*)g_node_mem.nodeArr - (char*)node_tmp;
        g_node_mem.nodeArr = node_tmp;
        for (i = 0; i < g_node_mem.table_size; ++i) {
            g_node_table[i] = (InternalNode*)((char*)g_node_table[i] - offset);
        }
    }
    g_node_mem.free_nodes = &g_node_mem.nodeArr[g_node_mem.size];
    for (i = g_node_mem.size; i < 2 * g_node_mem.size; ++i) {
        g_node_mem.nodeArr[i - 1].next_free = &g_node_mem.nodeArr[i];
    }
    for (i = 0; i < g_node_mem.size; ++i) {
        curr_node = getNodeFromTable(g_free_node_table_ptr[i].index);
        curr_node->table_entry = &g_free_node_table_ptr[i];
    }
    g_node_mem.size *= 2;
    return 0;
}

/*
 Reduces the allocated node memory by half when called.

 Returns: -1 on failure, 0 on success.
*/
int nodeMemDecrease(void) {
    InternalNode* node_tmp;
    InternalNode* curr_node;
    InternalNode* old_ptr;
    unsigned int counter = 0;
    unsigned int i;
    /* temp smaller node memory */
    node_tmp = malloc(g_node_mem.size * sizeof(InternalNode) / 2);
    if (node_tmp == NULL) {
        return -1;
    }

    for (i = 0; i < g_node_mem.size; ++i) {
        /* if list != 0, the node exists */
        curr_node = &(g_node_mem.nodeArr[i]);
        if (curr_node->in_use) {
            /* need to implement table shrinking for nodes */
            g_node_table[curr_node->table_entry->index - 1] = 
            memmove(node_tmp + counter, curr_node, sizeof(InternalNode));
            counter++;
        }
    }
    g_node_mem.size /= 2;
    old_ptr = g_node_mem.nodeArr;
    g_node_mem.nodeArr = node_tmp;
    free(old_ptr);
    old_ptr = NULL;
    return 0;
}

/*
   Creates a new list and generates an index to access it from the table

Returns: The index to the new list struct in the table 
*/
unsigned long int newList(void) {
    /* this function returns a fake LIST pointer i.e. an index of the table */

    InternalList* real_ptr;
    if (g_list_mem.listArr == NULL) {
        listInit();
    }
    if (g_list_mem.used_lists == g_list_mem.size) {
        listMemIncrease();
    }
    real_ptr = g_list_mem.free_lists;
    real_ptr->in_use = true;
    g_list_mem.free_lists = real_ptr->next_free;
    real_ptr->table_entry = g_free_list_table;
    g_free_list_table = g_free_list_table->next;
    real_ptr->table_entry->next = NULL;
    real_ptr->next_free = NULL;
    g_list_mem.used_lists++;
    g_list_table[real_ptr->table_entry->index - 1] = real_ptr;

    return real_ptr->table_entry->index;
}

/*
   Creates a new node and generates an index to access it from the table

Returns: The index to the new node struct in the table 
*/
unsigned long int newNode(void) {
    /* this function returns a fake NODE pointer i.e. an index of the table */
    InternalNode* real_ptr;
    /* need to replace this after nodeMemDecrease is implemented */
    if (g_node_mem.nodeArr == NULL) {
        return 0;
    }
    if (g_node_mem.used_nodes == g_node_mem.size) {
        nodeMemIncrease();
    }

    real_ptr = g_node_mem.free_nodes;
    real_ptr->in_use = true;
    g_node_mem.free_nodes = g_node_mem.free_nodes->next_free;
    real_ptr->table_entry = g_free_node_table;
    g_free_node_table = g_free_node_table->next;
    real_ptr->table_entry->next = NULL;
    real_ptr->next_free = NULL;
    g_node_mem.used_nodes++;
    g_node_table[real_ptr->table_entry->index - 1] = real_ptr;

    return real_ptr->table_entry->index;
}

/*
   Indexes the table and returns the corresponding list struct

Returns: The actual list struct corresponding to the index
*/
InternalList* getListFromTable(unsigned long int index) {
    if (index == 0 || g_list_mem.listArr == NULL) {
        return NULL;
    }

    return g_list_table[index-1];
}

/*
   Indexes the table and returns the corresponding node struct

Returns: The actual node struct corresponding to the index
*/
InternalNode* getNodeFromTable(unsigned long int index) {
    if (index == 0 || g_node_mem.nodeArr == NULL) {
        return NULL;
    }
    return g_node_table[index-1];
}

/* 
   Destroys a node. Removes from the table and node array 

Parameters:
index   an index from the node table referring to the node

Returns: integer return code  
*/
int destroyNode(unsigned long index) {
    InternalNode* node;
    if (index == 0) {
        return -1;
    }
    node = getNodeFromTable(index);
    node->in_use = false;
    node->table_entry->next = g_free_node_table;
    g_free_node_table = node->table_entry;
    memset(node, 0, sizeof(InternalNode));
    node->next_free = g_node_mem.free_nodes;
    g_node_mem.free_nodes = node;
    g_node_mem.used_nodes--;
    if (g_node_mem.size > NODEMIN &&
            g_node_mem.used_nodes <= g_node_mem.size / 2) {
        nodeMemDecrease();
    }
    return 0;
}

/* 
 Destroys a list. Removes from the table and list array 

 Parameters:
    index   an index from the list table referring to the list 

 Returns: integer return code  
*/
int destroyList(unsigned long index) {
    InternalList* list;
    if (index == 0) {
        return -1;
    }
    list = getListFromTable(index);
    list->in_use = false;
    list->table_entry->next = g_free_list_table;
    g_free_list_table = list->table_entry;
    memset(list, 0, sizeof(InternalList));
    list->next_free = g_list_mem.free_lists;
    g_list_mem.free_lists = list;
    g_list_mem.used_lists--;
    if (g_list_mem.used_lists == 0) {
        /* free all list and node memory
           we only free the nodes if all lists are gone
           because the assumption is that if there are still
           lists in use, the nodes will be used soon as well */
        free(g_list_mem.listArr);
        g_list_mem.listArr = NULL;
        free(g_node_mem.nodeArr);
        g_node_mem.nodeArr = NULL;
        free(g_list_table);
        g_list_table = NULL;
        free(g_node_table);
        g_node_table = NULL;
        free(g_free_list_table_ptr);
        g_free_list_table_ptr = NULL;
        g_free_list_table = NULL;
        free(g_free_node_table_ptr);
        g_free_node_table_ptr = NULL;
        g_free_node_table = NULL;
    } else if (g_list_mem.size > LISTMIN &&
            g_list_mem.used_lists <= g_list_mem.size / 2) {
        listMemDecrease();
    }
    return 0;
}
