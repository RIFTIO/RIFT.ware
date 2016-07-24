/**
 * @file rw_dl.h
 * @author Matt Harper (matt.harper@riftio.com)
 * @brief RIFT Double linked list library header file
 * 
 * @details Rift DLL library that works without any mallocs
 */
/*
 * Copyright (c) 1995-2013  Matt Harper. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modifications, are permitted provided that the above copyright notice
 * and this paragraph are duplicated in all such forms and that any
 * documentation, advertising materials, and other materials related to
 * such distribution and use acknowledge that the software was developed
 * by Matt Harper. The author's name may not be used to
 * endorse or promote products derived from this software without
 * specific prior written permission. THIS SOFTWARE IS PROVIDED ``AS IS''
 * AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
 * LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE.
 */
#if !defined(__RW_DL_H__)
#define __RW_DL_H__

#include "rwlib.h"

__BEGIN_DECLS

/* #define RW_DL_DEBUG */
#ifdef  RW_DL_DEBUG
#define RW_DL_DEBUG_ONLY(_x) _x
#else
#define RW_DL_DEBUG_ONLY(_x)
#endif

#ifdef RW_DL_ASSERT_DISABLE
#define RW_DL_ASSERT(_X)
#else
#define RW_DL_ASSERT(_X) RW_ASSERT(_X)
#endif


/*
 * IMPORTANT NOTE:
 * For effeciency reasons, there is an implicit assumption that both
 * RW_DL_INIT() and RW_DL_ELEMENT_INIT() can be replaced by a call to memset().
 * This means it isn't possible to add an "initialized" attribute flag to
 * either structure, OR to require them to be initialized with non-ZERO values
 */


typedef struct rw_dl_element {
  struct rw_dl_element *dl_next;
  struct rw_dl_element *dl_prev;
  RW_DL_DEBUG_ONLY(int dl_flags;)
} rw_dl_element_t;

#define RW_DL_ELEMENT_FLAG_ON_LIST     0x01

typedef struct {
  rw_dl_element_t dl_list;
  uint32_t        dl_length;
} rw_dl_t;


#define RW_DL_ELEMENT_PTR(list_type, list_element_ptr, element )        \
  ((list_type *) (((char *) element) - offsetof(list_type,list_element_ptr)))


#define RW_DL_ELEMENT_NEXT(element)  ((element)->dl_next)
#define RW_DL_ELEMENT_PREV(element)  ((element)->dl_prev)
#define RW_DL_ELEMENT_HEAD(listhead) ((listhead)->dl_list.dl_prev)
#define RW_DL_ELEMENT_TAIL(listhead) ((listhead)->dl_list.dl_next)


#define RW_DL_ELEMENT_UNLINK(listhead_in,element_in)                    \
  {                                                                     \
    rw_dl_t *listhead = (listhead_in);                                  \
    rw_dl_element_t *element = (element_in);                            \
    RW_DL_ASSERT(listhead != NULL);                                     \
    RW_DL_DEBUG_ONLY(RW_DL_ASSERT(element->dl_flags & RW_DL_ELEMENT_FLAG_ON_LIST);) \
        RW_DL_ASSERT(listhead->dl_list.dl_next != NULL);                \
    RW_DL_ASSERT(listhead->dl_list.dl_prev != NULL);                    \
    RW_DL_ASSERT(element != NULL);                                      \
    RW_DL_ASSERT(listhead->dl_length > 0);                              \
    if (element->dl_prev) {                                             \
      element->dl_prev->dl_next = element->dl_next;                     \
    }                                                                   \
    else {                                                              \
      RW_DL_ASSERT(listhead->dl_list.dl_prev == element);               \
      listhead->dl_list.dl_prev = element->dl_next;                     \
    }                                                                   \
    if (element->dl_next) {                                             \
      element->dl_next->dl_prev = element->dl_prev;                     \
    }                                                                   \
    else {                                                              \
      RW_DL_ASSERT(listhead->dl_list.dl_next == element);               \
      listhead->dl_list.dl_next = element->dl_prev;                     \
    }                                                                   \
    element->dl_next = NULL;                                            \
    element->dl_prev = NULL;                                            \
    listhead->dl_length--;                                              \
    RW_DL_DEBUG_ONLY(element->dl_flags &= ~RW_DL_ELEMENT_FLAG_ON_LIST;) \
        RW_DL_ASSERT((listhead->dl_length!=0) || ((listhead->dl_list.dl_next==NULL) && (listhead->dl_list.dl_prev==NULL))); \
  }

#define RW_DL_ELEMENT_REMOVE_HEAD(listhead)                             \
  RW_DL_ELEMENT_HEAD((listhead));                                       \
  if (RW_DL_ELEMENT_HEAD((listhead))) {                                 \
    RW_DL_ELEMENT_UNLINK((listhead), RW_DL_ELEMENT_HEAD((listhead)));   \
  }

#define RW_DL_ELEMENT_REMOVE_TAIL(listhead)                             \
  RW_DL_ELEMENT_TAIL((listhead));                                       \
  if (RW_DL_ELEMENT_TAIL((listhead))) {                                 \
    RW_DL_ELEMENT_UNLINK((listhead), RW_DL_ELEMENT_TAIL((listhead)));   \
  }


#define RW_DL_ELEMENT_INSERT_BEFORE(listhead_in, element_in, insert_before_element_in) \
  {                                                                     \
    rw_dl_t *listhead = (listhead_in);                                  \
    rw_dl_element_t *element = (element_in);                            \
    rw_dl_element_t *insert_before_element = (insert_before_element_in); \
    RW_DL_ASSERT(element->dl_next == NULL);                             \
    RW_DL_ASSERT(element->dl_prev == NULL);                             \
    RW_DL_DEBUG_ONLY(RW_DL_ASSERT(!(element->dl_flags & RW_DL_ELEMENT_FLAG_ON_LIST));) \
        if (insert_before_element == listhead->dl_list.dl_prev) {       \
          /* insert element at the head of the list */                  \
          element->dl_next = listhead->dl_list.dl_prev;                 \
          listhead->dl_list.dl_prev = element;                          \
          if (element->dl_next) {                                       \
            element->dl_next->dl_prev = element;                        \
          }                                                             \
          else {                                                        \
            listhead->dl_list.dl_next = element;                        \
            RW_DL_ASSERT(listhead->dl_list.dl_next == listhead->dl_list.dl_prev); \
          }                                                             \
        }                                                               \
        else if (insert_before_element == NULL) {                       \
          /* insert element at tail of list */                          \
          element->dl_prev = listhead->dl_list.dl_next;                 \
          if (element->dl_prev) {                                       \
            element->dl_prev->dl_next = element;                        \
          }                                                             \
          listhead->dl_list.dl_next = element;                          \
        }                                                               \
        else {                                                          \
          /* insert element in middle of list */                        \
          element->dl_next = insert_before_element;                     \
          element->dl_prev = insert_before_element->dl_prev;            \
          element->dl_prev->dl_next = element;                          \
          element->dl_next->dl_prev = element;                          \
        }                                                               \
    listhead->dl_length++;                                              \
    RW_DL_DEBUG_ONLY(element->dl_flags |= RW_DL_ELEMENT_FLAG_ON_LIST;)  \
        }


#define RW_DL_ELEMENT_INSERT_HEAD(listhead, element)                    \
  RW_DL_ELEMENT_INSERT_BEFORE((listhead), (element), RW_DL_ELEMENT_HEAD((listhead)))

#define RW_DL_ELEMENT_INSERT_TAIL(listhead, element)            \
  RW_DL_ELEMENT_INSERT_BEFORE((listhead), (element), NULL)


/*
 * The following macros are used to manipulate lists at an abstract level
 * so that user-level code doesn't need to call RW_DL_ELEMENT_PTR()
 * to derive the base object from a list element
 */
#define RW_DL_INIT(listhead)                    \
  do {                                          \
    (listhead)->dl_length = 0;                  \
    RW_DL_ELEMENT_INIT((&(listhead)->dl_list)); \
  } while (0)

#define RW_DL_ELEMENT_INIT(elem)                \
  do {                                          \
    (elem)->dl_next = (elem)->dl_prev = NULL;   \
    RW_DL_DEBUG_ONLY((elem)->dl_flags = 0);     \
  } while (0)



#define RW_DL_LENGTH(listhead)       ((listhead)->dl_length)

#define RW_DL_HEAD(list, type, element)                                 \
  (RW_DL_ELEMENT_HEAD(list) ? RW_DL_ELEMENT_PTR(type, element, RW_DL_ELEMENT_HEAD(list)) : NULL)

#define RW_DL_TAIL(list, type, element)                                 \
  (RW_DL_ELEMENT_TAIL(list) ? RW_DL_ELEMENT_PTR(type, element, RW_DL_ELEMENT_TAIL(list)) : NULL)

#define RW_DL_NEXT(value, type, element)                                \
  (RW_DL_ELEMENT_NEXT(&value->element) ? RW_DL_ELEMENT_PTR(type, element, RW_DL_ELEMENT_NEXT(&value->element)) : NULL)

#define RW_DL_PREV(value, type, element)                                \
  (RW_DL_ELEMENT_PREV(&value->element) ? RW_DL_ELEMENT_PTR(type, element, RW_DL_ELEMENT_PREV(&value->element)) : NULL)


#define RW_DL_INSERT_HEAD(list, value, element) RW_DL_ELEMENT_INSERT_HEAD(list, &value->element)

#define RW_DL_INSERT_TAIL(list, value, element) RW_DL_ELEMENT_INSERT_TAIL(list, &value->element)

#define RW_DL_REMOVE(list, value, element) RW_DL_ELEMENT_UNLINK(list, &value->element)

#define RW_DL_REMOVE_HEAD(list, type, element) ({			\
  type *_ohead = (RW_DL_ELEMENT_HEAD(list) ? RW_DL_ELEMENT_PTR(type, element, RW_DL_ELEMENT_HEAD(list)) : NULL); \
  (void)RW_DL_ELEMENT_REMOVE_HEAD(list); 				\
  _ohead;								\
})

#define RW_DL_REMOVE_TAIL(list, type, element) ({			\
  type *_otail = (RW_DL_ELEMENT_TAIL(list) ? RW_DL_ELEMENT_PTR(type, element, RW_DL_ELEMENT_TAIL(list)) : NULL); \
  (void)RW_DL_ELEMENT_REMOVE_TAIL(list);				\
  _otail;								\
})

#define RW_DL_PUSH         RW_DL_INSERT_HEAD
#define RW_DL_POP          RW_DL_REMOVE_HEAD
#define RW_DL_ENQUEUE      RW_DL_INSERT_TAIL
#define RW_DL_DEQUEUE      RW_DL_REMOVE_HEAD
#define RW_DL_PUTBACKQUEUE RW_DL_INSERT_HEAD

/*
 * The following debug-functions are available in rw_dl.c
 */
bool_t
rw_dl_contains_element(rw_dl_t *listhead, rw_dl_element_t *element);

uint32_t
rw_dl_walking_length(rw_dl_t *listhead);

rw_status_t
rw_dl_unit_test(void);

__END_DECLS

#endif /* !defined(__RW_DL_H__) */
