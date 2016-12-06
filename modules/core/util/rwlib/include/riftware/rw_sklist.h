/**
 * @file rw_sklist.h
 * @author Matt Harper (matt.harper@riftio.com)
 * @brief RIFT SkipList library
 * 
 * @details Rift SkipList library that uses an embedded threading element
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
#if !defined(__RW_SKLIST_H__)
#define __RW_SKLIST_H__

#include "rwlib.h"

__BEGIN_DECLS

/*
 *
 * This header file contains an implementation of generic SkipLists
 */


/*
 * Compilation Options for SKIPLISTS:
 */

/*
 * Define the maximum depth of a SkipList
 * This parameter effects performance for very large skiplists
 */
#define RW_SKLIST_MAX_LEVELS 16

/* #define RW_SKLIST_DEBUG   */
/* #define RW_SKLIST_DEBUG   */   /* Provide extra SKLIST debugging info  */
/* #define RW_SKLIST_USE_P25 */   /* Use P=0.25 for sklist depth vs. 0.50 */


/*
 * This type is a node in the SkipList
 */
typedef struct rw_sklist_node {
  void                    *value;      /* value attached at this node           */
  /* variable sized array of forward[] skip ptrs - must be last field in struct */
  struct rw_sklist_node *forward[1];
} rw_sklist_node_t;

typedef int (*rw_sklist_key_comp_fn_t)(void *v1, void *v2);

typedef struct {
  rw_sklist_key_comp_fn_t key_comp_func; /* element comparison function      */
  uint32_t                  key_offset;    /* offset of key into value         */
  uint32_t                  elem_offset;   /* offset of sklist_elem into value */
} rw_sklist_params_t;

/*
 * This type defines the head of a SkipList
 */
typedef struct {
  rw_sklist_node_t         *head;        /* pointer to head of the SkipList            */
  const rw_sklist_params_t *skl_params;  /* This skiplist's parameters                 */
  uint32_t                   sklist_len;   /* length of sklist                           */
  uint32_t                   level:8;      /* Current Max. level of the SkipList         */
  uint32_t                   dynamic:1;    /* Was the rw_sklist dynamically allocated? */
} rw_sklist_t;

/*
 * This type defines a SkipList element
 * which is required to be part of any base type
 * which desires to be a member of a SkipList
 */
typedef struct {
  rw_sklist_node_t *node;
#ifdef RW_SKLIST_DEBUG
  rw_sklist_t      *sklist; /* for debug - what sklist is element on */
#endif
} rw_sklist_element_t;



rw_sklist_t *
rw_sklist_alloc(rw_sklist_t *supplied_skl, const rw_sklist_params_t *skl_params);
rw_status_t
rw_sklist_free(rw_sklist_t *skl);
rw_status_t
rw_sklist_insert(rw_sklist_t *skl, void * value);
rw_status_t
rw_sklist_lookup_by_key(rw_sklist_t *skl, bool_t performing_getnext, void * key, void **valuePointer);
rw_status_t
rw_sklist_remove_by_key(rw_sklist_t *skl, void * key, void * *value);
rw_status_t
rw_sklist_debug_sanity_check(rw_sklist_t *skl);
rw_status_t
rw_sklist_unit_test(void);



/*
 * Some utilities for various types of SkipList Keys
 */
int rw_sklist_comp_int(void * v1, void * v2);
int rw_sklist_comp_uint32_t(void * v1, void * v2);
int rw_sklist_comp_uint64_t(void * v1, void * v2);
int rw_sklist_comp_charbuf(void * v1, void * v2);
int rw_sklist_comp_charptr(void * v1, void * v2);

#define RW_SKLIST_LENGTH(_skl) ((_skl)->sklist_len)

#if 0
/* This contains a redundant NULL check */
#define RW_SKLIST_HEAD(_skl, _type)                                   \
  ((NULL==(_skl)->head)?(_type *)NULL:((_type *)((_skl)->head[0].forward[0]?(_skl)->head[0].forward[0]->value:NULL)))
#else
#define RW_SKLIST_HEAD(_skl, _type)                                   \
  ((NULL==(_skl)->head)?(_type *)NULL:((_type *)(_skl)->head[0].forward[0]->value))
#endif /* 0 */

#define RW_SKLIST_NEXT(_ptr, _type, _skl_elem)                        \
  ((_type *) (_ptr->_skl_elem.node->forward[0]?_ptr->_skl_elem.node->forward[0]->value:NULL))

#define RW_SKLIST_PARAMS_DECL(_pname,_type,_key,_key_comp_func,_skl_elem) \
  static const rw_sklist_params_t _pname = {                          \
    _key_comp_func,                                                     \
    offsetof(_type, _key),                                              \
    offsetof(_type, _skl_elem)                                          \
  }

#define RW_SKLIST_FOREACH(var, type, sklist, field) \
  for ((var) = ((sklist)->head) ? (type *)(sklist)->head[0].forward[0]->value : NULL; \
      (var); \
      (var) = ((var)->field.node->forward[0] ? (type *)(var)->field.node->forward[0]->value : NULL))

#define RW_SKLIST_FOREACH_SAFE(var, type, sklist, field, bkup) \
  for ((var) = ((sklist)->head) ? (type *)(sklist)->head[0].forward[0]->value : NULL; \
      (var) && (((bkup) = ((var)->field.node->forward[0] ? (type *)(var)->field.node->forward[0]->value : NULL)) || true); \
      (var) = (bkup))


#define RW_SKLIST_ALLOC(_skl_params) rw_sklist_alloc(NULL,_skl_params)
#define RW_SKLIST_INIT(_skl,_skl_params) rw_sklist_alloc(_skl,_skl_params)
#define RW_SKLIST_FREE(_skl) rw_sklist_free(_skl)
#define RW_SKLIST_INSERT(_skl,_value) rw_sklist_insert((_skl),(void *)(_value))
#define RW_SKLIST_LOOKUP_BY_KEY(_skl,_keyPtr,_result) rw_sklist_lookup_by_key((_skl),FALSE,(void *)(_keyPtr),(void **)(_result))
#define RW_SKLIST_LOOKUP_NEXT_BY_KEY(_skl,_keyPtr,_result) rw_sklist_lookup_by_key((_skl),TRUE,(void *)(_keyPtr),(void **)(_result))
#define RW_SKLIST_REMOVE_BY_KEY(_skl,_key,_result) rw_sklist_remove_by_key((_skl),(void *)(_key),(void **)(_result))


__END_DECLS

#endif /* !defined(__RW_SKLIST_H__) */
