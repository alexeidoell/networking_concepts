/*
Alexei Doell cka067 11345642
Kevin Zhang zbk618 11354912
*/

#ifndef LISTALLOC_H
#define LISTALLOC_H
#include <list.h>

typedef struct FreeNodeTable FreeNodeTable;
typedef struct FreeListTable FreeListTable;

struct FreeNodeTable {
    unsigned long index;
    FreeNodeTable* next;
};

struct FreeListTable {
    unsigned long index;
    FreeListTable* next;
};


typedef struct InternalNode InternalNode;
typedef struct InternalList InternalList;

struct InternalNode {
    void *item;
    bool in_use;
    unsigned long next;
    unsigned long prev;
    FreeNodeTable* table_entry;
    InternalNode* next_free;
};

struct InternalList {
    unsigned long head;
    unsigned long tail;
    unsigned long cursor;
    int  size;
    bool in_use;
    FreeListTable* table_entry;
    InternalList* next_free;
};

typedef struct {
    InternalList* listArr;
    InternalList* free_lists;
    unsigned int size;
    unsigned int used_lists;
    unsigned int table_size;
} ListMem;

typedef struct {
    InternalNode* nodeArr;
    InternalNode* free_nodes;
    unsigned int size;
    unsigned int used_nodes;
    unsigned int table_size;
} NodeMem;

int listInit(void);

int listMemIncrease(void);
int nodeMemIncrease(void);
int listMemDecrease(void);
int nodeMemDecrease(void);

unsigned long int newList(void);
unsigned long int newNode(void);
InternalList* getListFromTable(unsigned long int index);
InternalNode* getNodeFromTable(unsigned long int index);
int destroyList(unsigned long int index);
int destroyNode(unsigned long int index);

#endif


