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





#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "linkedlist.h"

/* {{{ SimpleListNode_delete
 * -------------------------------------------------------------------------
 */
void SimpleListNode_delete(void *node) {
    if (node) {
        SimpleListNode_finalize((struct SimpleListNode *)node);
        free(node);
    }
}
/* }}} */

/****************************************************************************
 * Public Methods
 ****************************************************************************/
/* {{{ SimpleList_init
 * -------------------------------------------------------------------------
 * Returns FL_FALSE if initialization failed
 */
FLBool SimpleList_init(struct SimpleList *retVal) {
    memset(retVal, 0, sizeof(*retVal));

    // The default node destructor is the free function
    retVal->m_nodeDestructor = SimpleListNode_delete;

    return FL_TRUE;
}
/* }}} */
/* {{{ SimpleList_finalize
 * -------------------------------------------------------------------------
 */
void SimpleList_finalize(struct SimpleList *me) {
    struct SimpleListNode *node = NULL;

    assert(me);
    
    if (me->m_nodeDestructor) {
        while(me->m_begin) {
            node = me->m_begin->m_next;
            me->m_nodeDestructor(me->m_begin);
            me->m_begin = node;
        }
    }
    me->m_end = NULL;
    me->m_count = 0;
}
/* }}} */

/* {{{ SimpleList_new
 * -------------------------------------------------------------------------
 */
struct SimpleList * SimpleList_new(void) {
    struct SimpleList * retVal = NULL;

    retVal = (struct SimpleList *)malloc(sizeof(struct SimpleList));
    if (!retVal) {
        return NULL;
    }

    if (!SimpleList_init(retVal)) {
        free(retVal);
        return NULL;
    }

    return retVal;
}
/* }}} */
/* {{{ SimpleList_delete
 * -------------------------------------------------------------------------
 */
void SimpleList_delete(struct SimpleList *me) {
    SimpleList_finalize(me);
    free(me);
}
/* }}} */

/* {{{ SimpleList_addToBegin
 * -------------------------------------------------------------------------
 */
FLBool SimpleList_addToBegin(struct SimpleList *me, struct SimpleListNode *node) {
    if (me->m_begin) {
        node->m_prev = NULL;
        node->m_next = me->m_begin;
        me->m_begin->m_prev = node;
        me->m_begin = node;
    } else {
        /* List is empty */
        me->m_begin = me->m_end = node;
        node->m_next = node->m_prev = NULL;
    }
    ++me->m_count;
    return FL_TRUE;

}
/* }}} */
/* {{{ SimpleList_addToEnd
 * -------------------------------------------------------------------------
 */
FLBool SimpleList_addToEnd(struct SimpleList *me, struct SimpleListNode *node) {
    if (me->m_end) {
        node->m_prev = me->m_end;
        node->m_next = NULL;
        me->m_end->m_next = node;
        me->m_end = node;
    } else {
        /* List is empty */
        me->m_begin = me->m_end = node;
        node->m_next = node->m_prev = NULL;
    }
    ++me->m_count;
    return FL_TRUE;

}
/* }}} */
/* {{{ SimpleList_addOrdered
 * -------------------------------------------------------------------------
 */
FLBool SimpleList_addOrdered(struct SimpleList *me, struct SimpleListNode *node, SimpleListNodeComparatorFn cmpFn) {
    struct SimpleListNode *iter;
    // List empty? insert at the beginning?
    if ( (!me->m_begin) || (cmpFn(node, me->m_begin) <= 0) ) {
        return SimpleList_addToBegin(me, node);
    }

    // Scans through the list until found the insertion point
    for (iter = me->m_begin->m_next; iter; iter = iter->m_next) {
        if (cmpFn(node, iter) <= 0) {
            return SimpleList_insertBefore(me, iter, node);
        }
    }

    // Insert at the end
    return SimpleList_addToEnd(me, node);
}
/* }}} */
/* {{{ SimpleList_remove
 * -------------------------------------------------------------------------
 */
FLBool SimpleList_remove(struct SimpleList *me, struct SimpleListNode *node) {
    assert(me);

    /* Unlink it */
    if (node->m_next) node->m_next->m_prev = node->m_prev;
    if (node->m_prev) node->m_prev->m_next = node->m_next;

    /* Adjust begin/end */
    if (node == me->m_begin) {
        me->m_begin = me->m_begin->m_next;
    }
    if (node == me->m_end) {
        me->m_end = me->m_end->m_prev;
    }
    if (me->m_nodeDestructor) {
        me->m_nodeDestructor(node);
    }

    --me->m_count;
    return FL_TRUE;
}
/* }}} */
/* {{{ SimpleList_insertAfter
 * -------------------------------------------------------------------------
 */
FLBool SimpleList_insertAfter(struct SimpleList *me, 
        struct SimpleListNode *pos, 
        struct SimpleListNode *node) {
    assert(me);

    if (pos == me->m_end) {
        // Append to end
        pos->m_next = node;
        node->m_prev = pos;
        node->m_next = NULL;
        me->m_end = node;
    } else {
        node->m_next = pos->m_next;
        node->m_prev = pos;
        pos->m_next = node;
        pos->m_next->m_prev = pos;
    }

    ++me->m_count;
    return FL_TRUE;
}
/* }}} */
/* {{{ SimpleList_insertBefore
 * -------------------------------------------------------------------------
 */
FLBool SimpleList_insertBefore(struct SimpleList *me, 
        struct SimpleListNode *pos, 
        struct SimpleListNode *node) {
    assert(me);

    if (pos == me->m_begin) {
        // Append to front
        pos->m_prev = node;
        node->m_next = pos;
        node->m_prev = NULL;
        me->m_begin = node;
    } else {
        node->m_next = pos;
        node->m_prev = pos->m_prev;
        pos->m_prev = node;
        node->m_prev->m_next = node;
    }

    ++me->m_count;
    return FL_TRUE;
}
/* }}} */



/****************************************************************************
 * SimplePointerListNode & SimplePointerList
 ****************************************************************************/
/* {{{ SimplePointerList_init
 * -------------------------------------------------------------------------
 */
FLBool SimplePointerList_init(struct SimplePointerList *me) {

    if (!SimpleList_init(&me->m_parent)) {
        return FL_FALSE;
    }
    // By default the list will not delete the pointed object
    me->m_ptrDestructor = NULL;
    return FL_TRUE;
}

/* }}} */
/* {{{ SimplePointerList_finalize
 * -------------------------------------------------------------------------
 */
void SimplePointerList_finalize(struct SimplePointerList *me) {
    struct SimpleListNode *node;
    struct SimplePointerListNode *ptrListNode;

    assert(me);

    // This is not pretty as the best way should be to define a node destructor
    // that perform the optional free() on m_ptr.
    // But that will require to either one of those:
    //  1. passing the pointer to the ptr destructor to the free (or a pointer
    //     to the SimplePointerList object), breaking the compatibility with free()
    //  2. Adding a pointer to the SimplePointerList object to each node so it can
    //     be retrieved automatically.
    // This solution although not pretty but is very efficient at runtime (but not
    // so much during destruction as it will iterate the list twice).
    if (me->m_ptrDestructor) {
        // Iterates through all the nodes and invoke the pointer destructor
        // for each node, then set the pointer to NULL.
        // The SimpelPointerListNode object will be deleted by the SimpleList_finalize
        // function.
        for (node = SimpleList_getBegin(&me->m_parent); node; node = SimpleListNode_getNext(node)) {
            ptrListNode = (struct SimplePointerListNode *)node;
            if (ptrListNode->m_ptr) {
                me->m_ptrDestructor(ptrListNode->m_ptr);
            }
            ptrListNode->m_ptr = NULL;
        }
    }
    SimpleList_finalize(&me->m_parent);
}

/* }}} */
/* {{{ SimplePointerList_new
 * -------------------------------------------------------------------------
 */
struct SimplePointerList * SimplePointerList_new(void) {
    struct SimplePointerList *retVal;

    retVal = calloc(1, sizeof(*retVal));
    if (!retVal) {
        return NULL;
    }

    if (!SimplePointerList_init(retVal)) {
        free(retVal);
        return NULL;
    }
    return retVal;
}


/* }}} */
/* {{{ SimplePointerList_delete
 * -------------------------------------------------------------------------
 */
void SimplePointerList_delete(struct SimplePointerList *me) {

    SimplePointerList_finalize(me);
    free(me);
}

/* }}} */
/* {{{ SimplePointerList_addToBegin
 * -------------------------------------------------------------------------
 */
FLBool SimplePointerList_addToBegin(struct SimplePointerList *me, void *value) {
    struct SimplePointerListNode *node = calloc(1, sizeof(*node));
    SimpleListNode_init(&node->m_parent);
    node->m_ptr = value;

    if (!SimpleList_addToBegin(&me->m_parent, &node->m_parent)) {
        free(node);
        return FL_FALSE;
    }
    return FL_TRUE;
}

/* }}} */
/* {{{ SimplePointerList_addToEnd
 * -------------------------------------------------------------------------
 */
FLBool SimplePointerList_addToEnd(struct SimplePointerList *me, void *value) {
    struct SimplePointerListNode *node = calloc(1, sizeof(*node));
    SimpleListNode_init(&node->m_parent);
    node->m_ptr = value;

    if (!SimpleList_addToEnd(&me->m_parent, &node->m_parent)) {
        free(node);
        return FL_FALSE;
    }
    return FL_TRUE;
}

/* }}} */
/* {{{ SimplePointerList_removePtr
 * -------------------------------------------------------------------------
 */
FLBool SimplePointerList_removePtr(struct SimplePointerList *me, const void *ptr) {
    struct SimplePointerListNode *node;
    assert(me);
    if ((node = SimplePointerList_searchByPtr(me, ptr)) != NULL) {
        return SimplePointerList_remove(me, node);
    }
    // Not found
    return FL_TRUE;
}

/* }}} */
/* {{{ SimplePointerList_searchByPtr
 * -------------------------------------------------------------------------
 */
struct SimplePointerListNode * SimplePointerList_searchByPtr(struct SimplePointerList *me, const void *ptr) {
    struct SimplePointerListNode *node;

    for (node = SimplePointerList_getBegin(me); node; node = SimplePointerListNode_getNext(node)) {
        if (node->m_ptr == ptr) {
            return node;
        }
    }
    return NULL;
}

/* }}} */
/* {{{ SimplePointerList_remove
 * -------------------------------------------------------------------------
 */
FLBool SimplePointerList_remove(struct SimplePointerList *me, struct SimplePointerListNode *node) {
    if (me->m_ptrDestructor && node->m_ptr) {
        me->m_ptrDestructor(node->m_ptr);
        node->m_ptr = NULL;
    }
    if (!SimpleList_remove(&me->m_parent, &node->m_parent)) {
        return FL_FALSE;
    }
    return FL_TRUE;
}
/* }}} */


/****************************************************************************
 * SimpleConstPointerListNode & SimpleConstPointerList
 ****************************************************************************/
/* {{{ SimpleConstPointerList_init
 * -------------------------------------------------------------------------
 */
FLBool SimpleConstPointerList_init(struct SimpleConstPointerList *me) {

    if (!SimpleList_init(&me->m_parent)) {
        return FL_FALSE;
    }
    return FL_TRUE;
}


/* }}} */
/* {{{ SimpleConstPointerList_finalize
 * -------------------------------------------------------------------------
 */
void SimpleConstPointerList_finalize(struct SimpleConstPointerList *me) {
    SimpleList_finalize(&me->m_parent);
}

/* }}} */
/* {{{ SimpleConstPointerList_new
 * -------------------------------------------------------------------------
 */
struct SimpleConstPointerList * SimpleConstPointerList_new() {
    struct SimpleConstPointerList *retVal;

    retVal = calloc(1, sizeof(*retVal));
    if (!retVal) {
        return NULL;
    }

    if (!SimpleConstPointerList_init(retVal)) {
        free(retVal);
        return NULL;
    }
    return retVal;
}


/* }}} */
/* {{{ SimpleConstPointerList_delete
 * -------------------------------------------------------------------------
 */
void SimpleConstPointerList_delete(struct SimpleConstPointerList *me) {

    SimpleConstPointerList_finalize(me);
    free(me);
}

/* }}} */
/* {{{ SimpleConstPointerList_addToBegin
 * -------------------------------------------------------------------------
 */
FLBool SimpleConstPointerList_addToBegin(struct SimpleConstPointerList *me, const void *value) {
    struct SimpleConstPointerListNode *node = calloc(1, sizeof(*node));
    SimpleListNode_init(&node->m_parent);
    node->m_ptr = value;

    if (!SimpleList_addToBegin(&me->m_parent, &node->m_parent)) {
        free(node);
        return FL_FALSE;
    }
    return FL_TRUE;
}

/* }}} */
/* {{{ SimpleConstPointerList_addToEnd
 * -------------------------------------------------------------------------
 */
FLBool SimpleConstPointerList_addToEnd(struct SimpleConstPointerList *me, const void *value) {
    struct SimpleConstPointerListNode *node = calloc(1, sizeof(*node));
    SimpleListNode_init(&node->m_parent);
    node->m_ptr = value;

    if (!SimpleList_addToEnd(&me->m_parent, &node->m_parent)) {
        free(node);
        return FL_FALSE;
    }
    return FL_TRUE;
}

/* }}} */
/* {{{ SimpleConstPointerList_removePtr
 * -------------------------------------------------------------------------
 */
FLBool SimpleConstPointerList_removePtr(struct SimpleConstPointerList *me, const void *ptr) {
    struct SimpleConstPointerListNode *node;
    assert(me);
    if ((node = SimpleConstPointerList_searchByPtr(me, ptr)) != NULL) {
        return SimpleConstPointerList_remove(me, node);
    }
    // Not found
    return FL_TRUE;
}

/* }}} */
/* {{{ SimpleConstPointerList_search
 * -------------------------------------------------------------------------
 */
struct SimpleConstPointerListNode * SimpleConstPointerList_searchByPtr(struct SimpleConstPointerList *me, const void *ptr) {
    struct SimpleConstPointerListNode *node;

    for (node = SimpleConstPointerList_getBegin(me); node; node = SimpleConstPointerListNode_getNext(node)) {
        if (node->m_ptr == ptr) {
            return node;
        }
    }
    return NULL;
}

/* }}} */
/* {{{ SimpleConstPointerList_remove
 * -------------------------------------------------------------------------
 */
FLBool SimpleConstPointerList_remove(struct SimpleConstPointerList *me, struct SimpleConstPointerListNode *node) {
    // The _remove methhod call the node destructor
    if (!SimpleList_remove(&me->m_parent, &node->m_parent)) {
        return FL_FALSE;
    }
    return FL_TRUE;
}
/* }}} */




