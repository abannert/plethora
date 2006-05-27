/**
 * (c) 2006 Codemass, Inc.
 * Author: Aaron Bannert
 *
 * Simple definition of a RING (a double-linked list)
 */

#ifndef __ring_h
#define __ring_h

/**
 * Define a RING (a double-linked list in a loop). Used in various
 * lists below that require appending rather than just plain pushing.
 */
#define RING_NEXT_T(type) type *next
#define RING_PREV_T(type) type *prev
#define RING_T(type) RING_PREV_T(type); RING_NEXT_T(type)
#define RING_INIT(headptr) \
    do { \
        headptr->prev = headptr; \
        headptr->next = headptr; \
    } while (0);

#define RING_APPEND(headptr, nodeptr) \
    do { \
        if (headptr == NULL) { \
            headptr = nodeptr; \
            nodeptr->next = nodeptr->prev = nodeptr; \
        } else { \
            nodeptr->next = headptr; \
            nodeptr->prev = headptr->prev; \
            headptr->prev->next = nodeptr; \
            headptr->prev = nodeptr; \
        } \
    } while (0);

#define RING_INSERT(headptr, nodeptr) \
    do { \
        if (headptr == NULL) { \
            headptr = nodeptr; \
            nodeptr->next = nodeptr->prev = nodeptr; \
        } else { \
            RING_APPEND(headptr, nodeptr); \
            headptr = headptr->prev; \
        } \
    } while (0);

/* Remove the top node, storing a reference to it in nodeptr */
#define RING_POP(headptr, nodeptr) \
    do { \
        if (headptr->prev == headptr) { /* last node */ \
            headptr->prev = headptr->next = NULL; \
            nodeptr = headptr; \
            headptr = NULL; \
        } else { /* many nodes left */ \
            nodeptr = headptr; \
            headptr = headptr->next; \
            headptr->prev = nodeptr->prev; \
            nodeptr->prev->next = headptr; \
            nodeptr->next = nodeptr->prev = NULL; \
        } \
    } while (0);

#endif /* __ring_h */

