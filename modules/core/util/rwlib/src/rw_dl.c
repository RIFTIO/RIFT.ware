/**
 * @file rw_dl.c
 * @author Matt Harper (matt.harper@riftio.com)
 * @brief RIFT Double linked list library
 * 
 * @details Rift DLL library that works without any mallocs
 * This file contains doubly-linked list debug routines and unit test
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
#include "rw_dl.h"

/**
 * Determine if a list contains the specified element
 */
bool_t
rw_dl_contains_element(rw_dl_t *listhead, rw_dl_element_t *element)
{
  bool_t result = FALSE;
  rw_dl_element_t *elem_p;
  RW_DL_ASSERT(listhead != NULL);  
  RW_DL_ASSERT(element != NULL);

  for(elem_p=listhead->dl_list.dl_prev;elem_p;elem_p=elem_p->dl_next) {
    if (elem_p == element) {
      result = TRUE;
      break;
    }
  }
  return result;
}



/**
 * A debug function to verify that the specified list actually
 * contains the correct number of elements
 */
uint32_t
rw_dl_walking_length(rw_dl_t *listhead)
{
  uint32_t length = 0;
  rw_dl_element_t *elem_p;
  RW_DL_ASSERT(listhead != NULL);
  
  for(elem_p=listhead->dl_list.dl_prev;elem_p;elem_p=elem_p->dl_next) {
    RW_DL_ASSERT((elem_p->dl_next != NULL) || (listhead->dl_list.dl_next == elem_p));
    length++;
  }

  RW_DL_ASSERT((length > 0) || (listhead->dl_list.dl_next == NULL));
  RW_DL_ASSERT(length == listhead->dl_length);
  return length;
}




typedef struct foo {
  uint32_t foo1;
  double foo2;
  char ch;
  rw_dl_element_t elem;
} foo_t;

/**
 * A unit test for the linked list package
 */
rw_status_t
rw_dl_unit_test(void)
{
  rw_dl_t list;
  foo_t a,b,c,d, *ptr;
  uint32_t len;
  bool_t brc;
  
  RW_DL_INIT(&list);
  len = rw_dl_walking_length(&list);
  RW_ASSERT(0 == len);
  
  RW_DL_ELEMENT_INIT(&a.elem);
  a.ch = 'a';
  RW_ZERO_VARIABLE(&b);
  b.ch = 'b';
  RW_ZERO_VARIABLE(&c);
  c.ch = 'c';
  RW_ZERO_VARIABLE(&d);
  d.ch = 'd';
  
  brc = rw_dl_contains_element(&list, &b.elem);
  RW_ASSERT(FALSE == brc);

  ptr = RW_DL_ELEMENT_PTR(foo_t,elem,&a.elem);
  RW_ASSERT(&a == ptr);
  
  RW_DL_PUSH(&list,(&a),elem);
  RW_DL_PUSH(&list,(&b),elem);
  RW_DL_PUSH(&list,(&c),elem);
  RW_DL_PUSH(&list,(&d),elem);
  len = RW_DL_LENGTH(&list);
  RW_ASSERT(4 == len);
  len = rw_dl_walking_length(&list);
  RW_ASSERT(4 == len);

  brc = rw_dl_contains_element(&list, &b.elem);
  RW_ASSERT(TRUE == brc);

  ptr = RW_DL_POP(&list,foo_t,elem);
  RW_ASSERT(ptr == &d);

  ptr = RW_DL_POP(&list,foo_t,elem);
  RW_ASSERT(ptr == &c);

  brc = rw_dl_contains_element(&list, &b.elem);
  RW_ASSERT(TRUE == brc);

  ptr = RW_DL_POP(&list,foo_t,elem);
  RW_ASSERT(ptr == &b);

  brc = rw_dl_contains_element(&list, &b.elem);
  RW_ASSERT(FALSE == brc);

  ptr = RW_DL_POP(&list,foo_t,elem);
  RW_ASSERT(ptr == &a);

  len = RW_DL_LENGTH(&list);
  RW_ASSERT(0 == len);
  len = rw_dl_walking_length(&list);
  RW_ASSERT(0 == len);

  ptr = RW_DL_HEAD(&list,foo_t,elem);
  RW_ASSERT(ptr == NULL);
  ptr = RW_DL_TAIL(&list,foo_t,elem);
  RW_ASSERT(ptr == NULL);
  ptr = RW_DL_DEQUEUE(&list,foo_t,elem);
  RW_ASSERT(ptr == NULL);

  RW_DL_ENQUEUE(&list,(&a),elem);
  RW_DL_ENQUEUE(&list,(&b),elem);
  RW_DL_ENQUEUE(&list,(&c),elem);
  RW_DL_ENQUEUE(&list,(&d),elem);

  len = RW_DL_LENGTH(&list);
  RW_ASSERT(4 == len);
  len = rw_dl_walking_length(&list);
  RW_ASSERT(4 == len);

  ptr = RW_DL_DEQUEUE(&list,foo_t,elem);
  RW_ASSERT(ptr == &a);

  ptr = RW_DL_HEAD(&list,foo_t,elem);
  RW_ASSERT(ptr == &b);

  ptr = RW_DL_TAIL(&list,foo_t,elem);
  RW_ASSERT(ptr == &d);

  ptr = RW_DL_DEQUEUE(&list,foo_t,elem);
  RW_ASSERT(ptr == &b);

  ptr = RW_DL_DEQUEUE(&list,foo_t,elem);
  RW_ASSERT(ptr == &c);

  ptr = RW_DL_DEQUEUE(&list,foo_t,elem);
  RW_ASSERT(ptr == &d);

  len = RW_DL_LENGTH(&list);
  RW_ASSERT(0 == len);
  len = rw_dl_walking_length(&list);
  RW_ASSERT(0 == len);


  RW_DL_ENQUEUE(&list,(&a),elem);
  RW_DL_ENQUEUE(&list,(&b),elem);
  RW_DL_ENQUEUE(&list,(&c),elem);
  RW_DL_ENQUEUE(&list,(&d),elem);
  /* a b c d */
  RW_DL_REMOVE(&list,(&c),elem);
  /* a b d */
  RW_ASSERT(3 == RW_DL_LENGTH(&list));

  ptr = RW_DL_TAIL(&list,foo_t,elem);
  RW_ASSERT(ptr == &d);
  ptr = RW_DL_REMOVE_TAIL(&list,foo_t,elem);
  RW_ASSERT(ptr == &d);
  /* a b */
  ptr = RW_DL_REMOVE_TAIL(&list,foo_t,elem);
  RW_ASSERT(ptr == &b);
  /* a */
  ptr = RW_DL_HEAD(&list,foo_t,elem);
  RW_ASSERT(ptr == &a);
  ptr = RW_DL_TAIL(&list,foo_t,elem);
  RW_ASSERT(ptr == &a);
  RW_ASSERT(RW_DL_LENGTH(&list) == 1);
  RW_DL_REMOVE(&list,(&a),elem);
  /* empty */
  RW_ASSERT(RW_DL_LENGTH(&list) == 0);
  ptr = RW_DL_TAIL(&list,foo_t,elem);
  RW_ASSERT(ptr == NULL);
  ptr = RW_DL_DEQUEUE(&list,foo_t,elem);
  RW_ASSERT(ptr == NULL);
  
  return RW_STATUS_SUCCESS;
}
