/*
 * File: linkl.h
 *
 */

#ifndef LINKL_H
#define LINKL_H

#include    "common.h"

typedef struct _LinkLeaf
{
    struct  _LinkLeaf*	next;
    struct  _LinkLeaf*	prev;
    Pointer		    contents;
}
    LinkLeaf;

typedef struct
{
    LinkLeaf		    *top;
    LinkLeaf		    *bottom;
    LinkLeaf		    *current;
}
    LinkList;

#define ListIsEmpty(list)		    ((list)->top == (LinkLeaf *)(list))
#define TopOfList(list)			    (list)->top->contents
#define CurrentContentsOfList(list)	    (list)->current->contents

#define ForAllInList(list)  for						    \
			    (						    \
				((list)->current) = ((list)->top);	    \
				((list)->current) != (LinkLeaf *)(list);    \
				((list)->current) = ((list)->current)->next \
			    )


/* function prototypes: */
/* linkl.c: */
Global void initList P((LinkList *list ));
Global void prependToList P((LinkList *list , Pointer contents ));
Global void appendToList P((LinkList *list , Pointer contents ));
Global Pointer deleteFirst P((LinkList *list ));
Global Pointer deleteLast P((LinkList *list ));
Global Pointer deleteCurrent P((LinkList *list ));
Global void AppendToCurrent P((LinkList *list , Pointer contents ));
Global void PrependToCurrent P((LinkList *list , Pointer contents ));
Global void deleteList P((LinkList *list ));
Global void freeList P((LinkList *list ));
Global void freeCurrent P((LinkList *list ));
Global Bool freeMatchingLeaf P((LinkList *list , Pointer contents ));
Global LinkLeaf *findMatchingLeaf P((LinkList *list , Pointer contents ));
Global int indexOfContents P((LinkList *list , Pointer contents ));
Global Pointer getIndexedContents P((LinkList *list , int index ));
Global void freeLeaf P((LinkList *list , LinkLeaf *leaf ));
Global Pointer deleteLeaf P((LinkList *list , LinkLeaf *leaf ));

/* end function prototypes */

#endif	/* LINKL_H */
