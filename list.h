/*
Alexei Doell cka067 11345642
Kevin Zhang zbk618 11354912
*/

#ifndef LIST_H
#define LIST_H

#define MAX_LISTS 10
#define MAX_NODES 1000

#include <stdint.h>
#include <stdbool.h>

typedef int LIST;

LIST *ListCreate(void);
int ListCount(LIST *list);
void *ListFirst(LIST *list);
void *ListLast(LIST *list);
void *ListNext(LIST *list);
void *ListPrev(LIST *list);
void *ListCurr(LIST *list);
int ListAdd(LIST *list, void *item);
int ListInsert(LIST *list, void *item);
int ListAppend(LIST *list, void *item);
int ListPrepend(LIST *list, void *item);
void *ListRemove(LIST *list);
void ListConcat(LIST *list1, LIST *list2);
void ListFree(LIST *list, int (*itemFree)(void *));
void *ListTrim(LIST *list);
void *ListSearch(LIST *list, int (*comparator)(void *, void*),
        void *comparisonArg);

#endif
