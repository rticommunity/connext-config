/*******************************************************************************
Copyright (c) 2017, Fabrizio Bertocci (fabriziobertocci@gmail.com)
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the <organization> nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/





// *************************************************************************
// This module is borrowed from FabLib:
//     https://fabriziobertocci@github.com/fabriziobertocci/fablib.git
// it does not use all the features and is quite isolated...
// Refer to the FabLib project for additional info
// *************************************************************************



#ifndef __FL_LINKEDLIST_H_INCLUDED__
#define __FL_LINKEDLIST_H_INCLUDED__

typedef enum FLBool_t {
    FL_FALSE = 0,
    FL_TRUE  = 1
} FLBool;
#define FLINLINE       static inline

#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Example on how to use the list:

    struct MyNode {
        struct SimpleListNode parent;
        int a;
    };
    struct MyNode *MyNode_new(int val) {
        struct MyNode *retVal = calloc(1, sizeof(struct MyNode));
        if (retVal) retVal->a = val;
        return retVal;
    }
    void MyNode_delete(struct MyNode *node) {
        if (node) free(node);
    }

    struct SimpleList *list = NULL;

    void create() {
        list = SimpleList_new();
        ...
    }

    void insert() {
        int i;
        for(i = 0; i < 10; ++i) {
            SimpleList_addToBegin(list, (struct SimpleListNode *)MyNode_new(i));
        }
    }

    void iterate_fifo() {
        struct MyNode *node = SimpleList_getEnd(myList);
        for (; node; node = SimpleListNode_getPrev(node)) {
            printf("%d\n", node->a);
        }
    }

    void iterate_lifo() {
        struct MyNode *node = SimpleList_getBegin(myList);
        for (; node; node = SimpleListNode_getNext(node)) {
            printf("%d\n", node->a);
        }
    }
*/

struct SimpleListNode {
    struct SimpleListNode * m_next;
    struct SimpleListNode * m_prev;
};
/* {{{ SimpleListNode_getNext
 * -------------------------------------------------------------------------
 * Gets the next element of the list
 */
FLINLINE struct SimpleListNode * SimpleListNode_getNext(struct SimpleListNode *node) {
    assert(node);
    return node->m_next;
}
/* }}} */
/* {{{ SimpleListNode_getPrev
 * -------------------------------------------------------------------------
 * Gets the previous element of the list
 */
FLINLINE struct SimpleListNode * SimpleListNode_getPrev(struct SimpleListNode *node) {
    assert(node);
    return node->m_prev;
}
/* }}} */
/* {{{ SimpleListNode_init
 * -------------------------------------------------------------------------
 * Initializes the list node
 */
FLINLINE void SimpleListNode_init(struct SimpleListNode *node) {
    node->m_prev = node->m_next = NULL;
}
/* }}} */
/* {{{ SimpleListNode_finalize
 * -------------------------------------------------------------------------
 * Cleanups the node
 */
FLINLINE void SimpleListNode_finalize(struct SimpleListNode *node) {
    assert(node);
    node->m_prev = node->m_next = NULL;
}

/* }}} */
/* {{{ SimpleListNode_delete
 * -------------------------------------------------------------------------
 * Deletes the node
 */
void SimpleListNode_delete(void *node);
/* }}} */


/****************************************************************************
 * SimpleList definition
 ****************************************************************************/
typedef int (* SimpleListNodeComparatorFn)(struct SimpleListNode *node1, struct SimpleListNode *node2);

typedef void (* SimpleListNodeDestructorFn)(void *);

struct SimpleList {
    struct SimpleListNode *     m_begin;
    struct SimpleListNode *     m_end;
    unsigned int                m_count;          // Number of elements
    SimpleListNodeDestructorFn  m_nodeDestructor;
};


/* {{{ SimpleList_init
 * -------------------------------------------------------------------------
 * Returns FL_FALSE if initialization failed
 */
FLBool SimpleList_init(struct SimpleList *retVal);
/* }}} */
/* {{{ SimpleList_finalize
 * -------------------------------------------------------------------------
 * Scans through the entire list and delete all the elements invoking the
 * node registered destructor (that by default is a call to free());
 */
void SimpleList_finalize(struct SimpleList *me);
/* }}} */
/* {{{ SimpleList_new
 * -------------------------------------------------------------------------
 * Builds a new SimpleList.
 */
struct SimpleList * SimpleList_new(void);
/* }}} */
/* {{{ SimpleList_delete
 * -------------------------------------------------------------------------
 * Deletes a simple list. If there are elements inside the list, it will
 * iterate and delete each single node by invoking the registered node
 * destructor.
 *
 * \param me the list to delete
 *
 * NOTE: If your objects requires a custom free() operation (i.e. they have
 *       an internal dynamically allocated buffer), you must do a manual delete
 *       of all the elements. Ref. to SimplePointerList implementation for an 
 *       example.
 */
void SimpleList_delete(struct SimpleList *me);
/* }}} */
/* {{{ SimpleList_addToBegin
 * -------------------------------------------------------------------------
 * Inserts a new element to the beginning of the list.
 * Returns FL_FALSE if an error occurred.
 */
FLBool SimpleList_addToBegin(struct SimpleList *me, struct SimpleListNode *node);
/* }}} */
/* {{{ SimpleList_addToEnd
 * -------------------------------------------------------------------------
 * Inserts a new element to the end of the list.
 * Returns FL_FALSE if an error occurred.
 */
FLBool SimpleList_addToEnd(struct SimpleList *me, struct SimpleListNode *node);
/* }}} */
/* {{{ SimpleList_addOrdered
 * -------------------------------------------------------------------------
 * Inserts the given node in a position in the list so that the list
 * is maintained ordered according to the given function.
 * Returns FL_FALSE if an error occurred.
 */
FLBool SimpleList_addOrdered(struct SimpleList *me, 
        struct SimpleListNode *pos, 
        SimpleListNodeComparatorFn cmpFn);
/* }}} */
/* {{{ SimpleList_getBegin
 * -------------------------------------------------------------------------
 * Returns the element at the beginning of the list
 * Returns NULL if the list is empty
 */
FLINLINE struct SimpleListNode * SimpleList_getBegin(struct SimpleList *me) {
    assert(me);
    return me->m_begin;
}
/* }}} */
/* {{{ SimpleList_getEnd
 * -------------------------------------------------------------------------
 * Returns the element at the end of the list.
 * Returns NULL if the list is empty
 */
FLINLINE struct SimpleListNode * SimpleList_getEnd(struct SimpleList *me) {
    assert(me);
    return me->m_end;
}
/* }}} */
/* {{{ SimpleList_remove
 * -------------------------------------------------------------------------
 * Removes the given element from the list.
 * Returns FL_FALSE if an error occurred.
 * Element removed is not freed
 */
FLBool SimpleList_remove(struct SimpleList *me, struct SimpleListNode *node);
/* }}} */
/* {{{ SimpleList_insertAfter
 * -------------------------------------------------------------------------
 * Inserts the given node after the selected one
 * Returns FL_FALSE if an error occurred.
 */
FLBool SimpleList_insertAfter(struct SimpleList *me, 
        struct SimpleListNode *pos, 
        struct SimpleListNode *node);
/* }}} */
/* {{{ SimpleList_insertBefore
 * -------------------------------------------------------------------------
 * Inserts the given node before the selected one
 * Returns FL_FALSE if an error occurred.
 */
FLBool SimpleList_insertBefore(struct SimpleList *me, 
        struct SimpleListNode *pos, 
        struct SimpleListNode *node);
/* }}} */
/* {{{ SimpleList_getCount
 * -------------------------------------------------------------------------
 * Returns the number of elements in the list
 */
FLINLINE size_t SimpleList_getCount(struct SimpleList *me) {
    assert(me);
    return me->m_count;
}

/* }}} */
/* {{{ SimpleList_setNodeDestructor
 * -------------------------------------------------------------------------
 * Assigns the node destructor to be used when the SimpleList_finalize()
 * method is called.
 */
FLINLINE void SimpleList_setNodeDestructor(struct SimpleList *me, 
        SimpleListNodeDestructorFn destructor) {
    assert(me);
    me->m_nodeDestructor = destructor;
}
/* }}} */
/* {{{ SimpleList_getNodeDestructor
 * -------------------------------------------------------------------------
 */
FLINLINE SimpleListNodeDestructorFn SimpleList_getNodeDestructor(struct SimpleList *me) {
    assert(me);
    return me->m_nodeDestructor;
}
/* }}} */



/****************************************************************************
 * SimplePointerList definition
 ****************************************************************************
 * A simple pointer list is a specialization of a SimpleList that holds
 * pointers.
 * You can directly push pointers into the list without bothering with the
 * list node.
 * By default the list will not touch the object identified by the pointer,
 * but you can supply a pointer destructor function to automatically invoke it
 * whenever the node holding the pointer gets destroyed.
 *
 * Install a custom pointerDestructor in the list initializer to automatically
 * call the destructor over the stored pointer whenever the pointer is removed
 * from the list or when the list is destroyed.
 *
 * Set the ptrDestructor to NULL to avoid automatic release of memory.
 *
 * If you plan to use a simple pointer list to store const object, consider
 * using SimpleConstPointerList instead.
 */

/* The signature of the function that can be used to free the pointed object. */
typedef void (* SimplePointerListPtrDestructorFn)(void *ptr);


/* The specialization of a simple list for storing pointers */
struct SimplePointerListNode {
    struct SimpleListNode   m_parent;
    void *                  m_ptr;
};


struct SimplePointerList {
    struct SimpleList                   m_parent;

    SimplePointerListPtrDestructorFn    m_ptrDestructor;
};




/* {{{ SimplePointerList_init
 * -------------------------------------------------------------------------
 */
FLBool SimplePointerList_init(struct SimplePointerList *me);
/* }}} */
/* {{{ SimplePointerList_finalize
 * -------------------------------------------------------------------------
 * Empty the list, optionally invoking the pointer destructor over each
 * non-NULL pointer stored in the list
 */
void SimplePointerList_finalize(struct SimplePointerList *me);
/* }}} */
/* {{{ SimplePointerList_new
 * -------------------------------------------------------------------------
 * Builds a new SimplePointerList.
 * \param destructor    A pointer to a function that will be invoked when an
 *                      element is removed from the list. Destructor will be
 *                      called over the pointed object.
 * \return NULL if an out of memory occurred
 */
struct SimplePointerList * SimplePointerList_new(void);
/* }}} */
/* {{{ SimplePointerList_delete
 * -------------------------------------------------------------------------
 * Deletes a SimplePointerList
 *
 * \param me the list to delete
 *
 * NOTE: If your objects requires a custom free() operation (i.e. they have
 *       an internal dynamically allocated buffer), you must do a manual delete
 *       of all the elements
 */
void SimplePointerList_delete(struct SimplePointerList *me);
/* }}} */
/* {{{ SimplePointerList_addToBegin
 * -------------------------------------------------------------------------
 * Inserts a new element to the beginning of the list.
 * Returns FL_FALSE if an error occurred.
 */
FLBool SimplePointerList_addToBegin(struct SimplePointerList *me, void *value);
/* }}} */
/* {{{ SimplePointerList_addToEnd
 * -------------------------------------------------------------------------
 * Inserts a new element to the end of the list.
 * Returns FL_FALSE if an error occurred.
 */
FLBool SimplePointerList_addToEnd(struct SimplePointerList *me, void *value);
/* }}} */
/* {{{ SimplePointerList_removePtr
 * -------------------------------------------------------------------------
 * Searches for the given pointer inside the list and removes it
 * Returns FL_FALSE if an error occurred and the element could not
 * be removed. Returns FL_TRUE if the element was not found.
 * Element removed is not freed
 */
FLBool SimplePointerList_removePtr(struct SimplePointerList *me, const void *ptr);
/* }}} */
/* {{{ SimplePointerList_searchByPtr
 * -------------------------------------------------------------------------
 * Returns pointer to the SimplePointerListNode if found or NULL if not found
 */
struct SimplePointerListNode * SimplePointerList_searchByPtr(struct SimplePointerList *me, const void *ptr);
/* }}} */
/* {{{ SimplePointerList_remove
 * -------------------------------------------------------------------------
 */
FLBool SimplePointerList_remove(struct SimplePointerList *me, struct SimplePointerListNode *node);
/* }}} */
/* {{{ SimplePointerList_getBegin
 * -------------------------------------------------------------------------
 */
#define  SimplePointerList_getBegin(me)        \
                (struct SimplePointerListNode *)SimpleList_getBegin((struct SimpleList *)(me))
/* }}} */
/* {{{ SimplePointerList_getEnd
 * -------------------------------------------------------------------------
 */
#define  SimplePointerList_getEnd(me)          \
                (struct SimplePointerListNode *)SimpleList_getEnd((struct SimpleList *)(me))
/* }}} */
/* {{{ SimplePointerList_getCount
 * -------------------------------------------------------------------------
 */
#define SimplePointerList_getCount(me)         \
                SimpleList_getCount((struct SimpleList *)(me))
/* }}} */
/* {{{ SimplePointerList_setPtrDestructor
 * -------------------------------------------------------------------------
 * Sets a new pointer destructor. Set it to free for example if you want to
 * simply call free on the pointer when the node containing it gets deleted.
 * Set to NULL to disable the callback of the pointer.
 */
FLINLINE void SimplePointerList_setPtrDestructor(struct SimplePointerList *me, 
    SimplePointerListPtrDestructorFn destructor) {
    assert(me);
    me->m_ptrDestructor = destructor;
}
/* }}} */
/* {{{ SimplePointerList_getPtrDestructor
 * -------------------------------------------------------------------------
 */
FLINLINE SimplePointerListPtrDestructorFn SimplePointerList_getPtrDestructor(struct SimplePointerList *me) {
    assert(me);
    return me->m_ptrDestructor;
}
/* }}} */

/* {{{ SimplePointerListNode_getPtr
 * -------------------------------------------------------------------------
 */
#define  SimplePointerListNode_getPtr(me)         ((me) ? ((struct SimplePointerListNode *)me)->m_ptr : NULL)
/* }}} */
/* {{{ SimplePointerListNode_getNext
 * -------------------------------------------------------------------------
 * Just a convenience macro to avoid to write tons of casts
 */
#define  SimplePointerListNode_getNext(me)      \
        (struct SimplePointerListNode *)SimpleListNode_getNext((struct SimpleListNode *)(me))
/* }}} */
/* {{{ SimplePointerListNode_getPrev
 * -------------------------------------------------------------------------
 * Just a convenience macro to avoid to write tons of casts
 */
#define  SimplePointerListNode_getPrev(me)      \
        (struct SimplePointerListNode *)SimpleListNode_getPrev((struct SimpleListNode *)(me))
/* }}} */


/****************************************************************************
 * SimpleConstPointerList definition
 ****************************************************************************/
/* The specialization of a simple list for storing const pointers
 * Differently than the SimplePointerList, there is no need to declare
 * a node destructor here (the pointed value is const and cannot be deleted)
 */
struct SimpleConstPointerListNode {
    struct SimpleListNode m_parent;
    const void * m_ptr;
};


struct SimpleConstPointerList {
    struct SimpleList               m_parent;
};



/* {{{ SimpleConstPointerList_init
 * -------------------------------------------------------------------------
 */
FLBool SimpleConstPointerList_init(struct SimpleConstPointerList *me);
/* }}} */
/* {{{ SimpleConstPointerList_finalize
 * -------------------------------------------------------------------------
 */
void SimpleConstPointerList_finalize(struct SimpleConstPointerList *me);
/* }}} */
/* {{{ SimpleConstPointerList_new
 * -------------------------------------------------------------------------
 * Builds a new SimpleConstPointerList.
 * \return NULL if an out of memory occurred
 */
struct SimpleConstPointerList * SimpleConstPointerList_new(void);
/* }}} */
/* {{{ SimpleConstPointerList_delete
 * -------------------------------------------------------------------------
 * Deletes a SimpleConstPointerList
 *
 * \param me the list to delete
 * \param deleteNodes a boolean to tell whether to iterate and delete all the
 *        nodes currently present in the list or not. The deletion will be
 *        performed using the registered destructor
 *
 * NOTE: If your objects requires a custom free() operation (i.e. they have
 *       an internal dynamically allocated buffer), you must do a manual delete
 *       of all the elements
 */
void SimpleConstPointerList_delete(struct SimpleConstPointerList *me);
/* }}} */
/* {{{ SimpleConstPointerList_addToBegin
 * -------------------------------------------------------------------------
 * Inserts a new element to the beginning of the list.
 * Returns FL_FALSE if an error occurred.
 */
FLBool SimpleConstPointerList_addToBegin(struct SimpleConstPointerList *me, const void *value);
/* }}} */
/* {{{ SimpleConstPointerList_addToEnd
 * -------------------------------------------------------------------------
 * Inserts a new element to the end of the list.
 * Returns FL_FALSE if an error occurred.
 */
FLBool SimpleConstPointerList_addToEnd(struct SimpleConstPointerList *me, const void *value);
/* }}} */
/* {{{ SimpleConstPointerList_removePtr
 * -------------------------------------------------------------------------
 * Searches for the given pointer inside the list and removes it
 * Returns FL_FALSE if an error occurred and the element could not
 * be removed. Returns FL_TRUE if the element was not found.
 * Element removed is not freed
 */
FLBool SimpleConstPointerList_removePtr(struct SimpleConstPointerList *me, const void *ptr);
/* }}} */
/* {{{ SimpleConstPointerList_searchByPtr
 * -------------------------------------------------------------------------
 * Returns pointer to the SimpleConstPointerListNode if found or NULL if not found
 */
struct SimpleConstPointerListNode * SimpleConstPointerList_searchByPtr(struct SimpleConstPointerList *me, const void *ptr);
/* }}} */
/* {{{ SimpleConstPointerList_remove
 * -------------------------------------------------------------------------
 */
FLBool SimpleConstPointerList_remove(struct SimpleConstPointerList *me, struct SimpleConstPointerListNode *node);
/* }}} */
/* {{{ SimpleConstPointerList_getBegin
 * -------------------------------------------------------------------------
 */
#define  SimpleConstPointerList_getBegin(me)        \
                (struct SimpleConstPointerListNode *)SimpleList_getBegin((struct SimpleList *)(me))
/* }}} */
/* {{{ SimpleConstPointerList_getEnd
 * -------------------------------------------------------------------------
 */
#define  SimpleConstPointerList_getEnd(me)          \
                (struct SimpleConstPointerListNode *)SimpleList_getEnd((struct SimpleList *)(me))
/* }}} */
/* {{{ SimpleConstPointerList_getCount
 * -------------------------------------------------------------------------
 * Returns the number of elements in the list
 */
#define SimpleConstPointerList_getCount(me)         \
                SimpleList_getCount((struct SimpleList *)(me))
/* }}} */

/* {{{ SimpleConstPointerListNode_getPtr
 * -------------------------------------------------------------------------
 */
FLINLINE const void * SimpleConstPointerListNode_getPtr(struct SimpleConstPointerListNode *me) {
    assert(me);
    return me->m_ptr;
}
/* }}} */
/* {{{ SimpleConstPointerListNode_getNext
 * -------------------------------------------------------------------------
 */
#define  SimpleConstPointerListNode_getNext(me)     \
                (struct SimpleConstPointerListNode *)SimpleListNode_getNext((struct SimpleListNode *)(me))
/* }}} */
/* {{{ SimpleConstPointerListNode_getPrev
 * -------------------------------------------------------------------------
 */
#define  SimpleConstPointerListNode_getPrev(me)     \
                (struct SimpleConstPointerListNode *)SimpleListNode_getPrev((struct SimpleListNode *)(me))
/* }}} */


#ifdef __cplusplus
}
#endif

#endif      /* !defined(__FL_LINKEDLIST_H_INCLUDED__) */


