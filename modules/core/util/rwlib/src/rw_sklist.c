/**
 * @file rw_sklist.c
 * @author Matt Harper (matt.harper@riftio.com)
 * @brief RIFT SkipList library
 * 
 * @details Rift SkipList library that uses an intra-struct threading element
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
#include "rw_sklist.h"
#include "rw_rand.h"

/*
 * This file contains an implementation of SkipLists
 *
 * Binary trees can be used for representing abstract data types such as 
 * dictionaries and ordered lists. They work well when the elements are 
 * inserted in a random order. Some sequences of operations, such as 
 * inserting the elements in order, produce degenerate data structures that 
 * give very poor performance. If it were possible to randomly permute the 
 * list of items to be inserted, trees would work well with high 
 * probability for any input sequence. In most cases queries must be 
 * answered on-line, so permuting randomly the input is impractical. 
 * Balanced tree algorithms rearrange the tree as operations are performed 
 * to maintain certain balance conditions and assure good performance. 
 * Skip lists are a probabilistic alternative to balanced trees. Skip 
 * lists are balanced by consulting a random number generator. Although 
 * skip lists have bad worst-case performance, no input sequence 
 * consistently produces the worst-case performance (much like quicksort 
 * when the pivot element is chosen randomly). It is very unlikely a skip 
 * list data structure will be significantly unbalanced (e.g., for a 
 * dictionary of more than 250 elements, the chance that a search will take 
 * more than 3 times the expected time is less than one in a million). Skip 
 * lists have balance properties similar to that of search trees built by 
 * random insertions, yet do not require insertions to be random.
 * Balancing a data structure probabilistically is easier than 
 * explicitly maintaining the balance. For many applications, skip lists 
 * are a more natural representation than trees, also leading to simpler 
 * algorithms. The simplicity of skip list algorithms makes them easier to 
 * implement and provides significant constant factor speed improvements 
 * over balanced tree and self-adjusting tree algorithms. Skip lists are 
 * also very space efficient. They can easily be configured to require an 
 * average of 1 1/3 pointers per element (or even less) and do not require 
 *  balance or priority information to be stored with each node. 
 *
 * Skip Lists are described in the June 1990 issue of CACM.
 *
 *
 * A couple of notes about this implementation:
 *
 * The routine rw_sklist_random_level() has been hard-coded to
 * generate random levels using either P=0.25. or P=0.50.
 * It can be easily changed by using the #define RW_SKLIST_USE_P25
 * by default P=0.50 with RW_SKLIST_MAX_LEVELS as the max
 * levels in the skip-list tree.
 *
 * Some special error checking variables are kept in each sklist
 * node when RW_SKLIST_DEBUG is defined.
 *
 * The insertion routine has been implemented so as to use the
 * dirty hack described in the CACM paper: if a random level is
 * generated that is more than the current maximum level, the
 * current maximum level plus one is used instead.  This simplies
 * the insertion process and minimizes overhead.
 */


/*
 * PRIVATE variable and function declarations
 */
static __thread uint32_t randomBits;     /* temp cache of random bits */
static __thread uint32_t randomskleft;   /* # of random bits in cache */

static int rw_sklist_random_level(rw_sklist_t *skl);



/**
 * Allocate a SkipList and initialize it
 * 
 * On SUCCESS, returns a pointer to a SkipList
 */
rw_sklist_t *
rw_sklist_alloc(rw_sklist_t *supplied_skl,
               const rw_sklist_params_t *sklparams)
{
  rw_sklist_t *skl;
  RW_ASSERT(sklparams);
  RW_ASSERT(sklparams->key_comp_func);

  if (NULL == supplied_skl) {  
    /*
     * Allocate the base SkipList structure
     */
    skl = (rw_sklist_t *)RW_MALLOC0(sizeof(rw_sklist_t));
    RW_ASSERT(skl);
    skl->dynamic = TRUE;
  }
  else {
    skl = supplied_skl;
    RW_ZERO_VARIABLE(skl);
  }

  // Save the skiplist's configuration
  skl->skl_params = sklparams;

  return skl;
}



/**
 * Free an Empty SkipList
 */
rw_status_t
rw_sklist_free(rw_sklist_t *skl)
{
  RW_ASSERT(NULL != skl);
  RW_ASSERT(RW_SKLIST_LENGTH(skl) == 0);
  RW_ASSERT(NULL == skl->head);

  if (skl->dynamic) {
    /* RW_ZERO_VARIABLE(skl); */
    RW_FREE(skl);
  }

  return RW_STATUS_SUCCESS;
}



/**
 * Calculate a random depth for the SkipList
 * Probability of level N is P(n) = p^(n+1)
 * where (N = 0..RW_SKLIST_MAX_LEVELS-1)
 * and p = 0.25 or 0.50
 *
 * Returns a random value from 0..RW_SKLIST_MAX_LEVELS-1
 *
 */
static int
rw_sklist_random_level(rw_sklist_t *skl)
{
  int level = 0;
  int bits;
  UNUSED(skl);

  if (0 == randomskleft) {
    randomBits = rw_random();
    randomskleft = RW_BITS_IN_RANDOM/2;
  }
  
  do {
#ifdef RW_SKLIST_USE_P25
    bits = randomBits&3; /* P for each level = 0.25 */
    if (!bits) level++;
    randomBits>>=2;
    if (--(randomskleft) == 0) {
      randomBits = rw_random();
      randomskleft = RW_BITS_IN_RANDOM/2;
    };
#else
    bits = randomBits&1; /* P for each level = 0.50 */
    if (!bits) level++;
    randomBits>>=1;
    if (--(randomskleft) == 0) {
      randomBits = rw_random();
      randomskleft = RW_BITS_IN_RANDOM;
    };
#endif
  } while ((!bits) && (level < (RW_SKLIST_MAX_LEVELS-1)));

  return level;
}



/**
 * Lookup an element in the SkipList with the specified key
 * if searching_next == TRUE, then if the key if found, return the next item
 * or if first item beyond the specified key value
 */
rw_status_t
rw_sklist_lookup_by_key(rw_sklist_t *skl,
                       bool_t performing_getnext,
                       void * keyPointer,
                       void **valuePointer)
{
  int k, last_key_comp_result = 0;
  rw_sklist_node_t *p, *q;
  RW_ASSERT(skl);
  RW_ASSERT(valuePointer != NULL);
  RW_ASSERT(keyPointer != NULL);

  /* NO RESULT YET */
  *valuePointer = NULL;
  q = NULL;

  if (NULL == skl->head) {
    // The sklist is totally empty
    RW_ASSERT(0 == RW_SKLIST_LENGTH(skl));
    goto finish;
  }

  /*
   * traverse the sklist for a match
   */
  p = skl->head;
  k = skl->level;
  for(k = skl->level; k >=0 ; k--) {
    q = p->forward[k];
    while (q && ((last_key_comp_result = skl->skl_params->key_comp_func((void *)(((char *)(q->value))+skl->skl_params->key_offset),keyPointer)) < 0)) {
      p = q;
      q = p->forward[k];
    }
  }

finish:
  if (performing_getnext) {
    if (q) {
      if (0 == last_key_comp_result) {
        // found item with matching key, need to advance q
        q = q->forward[0];
      }
    }
    if (q) {
      *valuePointer = q->value; /* match found, populate result */
    }
    return RW_STATUS_SUCCESS;
  }
  else {
    /*
     * Check if a match was found
     */
    if ((NULL == q) || last_key_comp_result) {
      return RW_STATUS_FAILURE; /* NO match found */
    }

    *valuePointer = q->value; /* match found, populate result */
    return RW_STATUS_SUCCESS;
  }
}



/**
 * Insert an element into the SkipList
 * NOTE: the key for insertion is contained within "value"
 */
rw_status_t
rw_sklist_insert(rw_sklist_t *skl, void * value) 
{
  int32_t k;
  rw_sklist_node_t *update[RW_SKLIST_MAX_LEVELS];
  rw_sklist_node_t *p, *q;
  rw_sklist_element_t *elem;

  if (NULL == skl->head) {
    skl->head =  (rw_sklist_node_t *)RW_MALLOC0(sizeof(rw_sklist_node_t)+((RW_SKLIST_MAX_LEVELS-1)*sizeof(rw_sklist_node_t *)));
    // NOTE: there is no possibility of a failed insert in this case
  }
  
  /*
   * Search for the place to insert the value
   * Record the search path to be updated in "update" while doing the search
   */
  p = skl->head;
  q = NULL;
  for (k = skl->level; k >= 0; k--) {
    q = p->forward[k];
    while (q && (skl->skl_params->key_comp_func((void *)((char *)(q->value)+skl->skl_params->key_offset),
                                              (void *)(((char *)value)+skl->skl_params->key_offset)) <  0)) {
      p = q;
      q = p->forward[k];
    }
    update[k] = p;
  }

  /*
   * If an exact-match was found, fail the insertion, duplicates are NOT allowed
   */
  if ((NULL != q) &&
      (0 == skl->skl_params->key_comp_func((void *)((char *)(q->value)+skl->skl_params->key_offset),
                                         (void *)(((char *)value)+skl->skl_params->key_offset)))) {
    return RW_STATUS_DUPLICATE;
  }

  /*
   * Insertion is occuring after element q
   * insert the value onto the "inorder" list maintained in the SkipList head
   */

  /*
   * Select a random insertion level for the value
   *
   * If the new insertion level exceeds the previous level, then
   * at most increase the dpth of the SkipList by 1
   */
  k = rw_sklist_random_level(skl);
  if (k > (int32_t)skl->level) {
    k = ++skl->level;
    update[k] = skl->head;
  }

  /*
   * Allocate a node to hold the new value
   * and update the SkipList forwarding entries
   */
  q = (rw_sklist_node_t *) RW_MALLOC0(sizeof(rw_sklist_node_t)+(k*sizeof(rw_sklist_node_t *)));
  RW_ASSERT(q != NULL);
  q->value = value;
  elem = (rw_sklist_element_t *)(((char *)value)+skl->skl_params->elem_offset);
  elem->node = q;
#ifdef RW_SKLIST_DEBUG
  elem->sklist = skl;
#endif
  skl->sklist_len++;

  for(; k >= 0; k--) {
    p = update[k];
    q->forward[k] = p->forward[k];
    p->forward[k] = q;
  }

  return RW_STATUS_SUCCESS;
}



/**
 * Remove an element with the specified key from the SkipList
 */
rw_status_t
rw_sklist_remove_by_key(rw_sklist_t *skl, void * key, void * *value)
{
  register int k,m;
  rw_sklist_node_t *update[RW_SKLIST_MAX_LEVELS];
  rw_sklist_node_t *p, *q;
  rw_sklist_element_t *elem;

  if (NULL == skl->head) {
    // Empty list, so there is nothing to remove
    return RW_STATUS_FAILURE;
  }
  
  if (value != NULL) {
    *value = NULL;
  }
  p = skl->head;
  q = NULL;
  for( k = m = skl->level; k >= 0; k--) {
    q = p->forward[k];
    while (q && (skl->skl_params->key_comp_func((void *)(((char *)(q->value)+skl->skl_params->key_offset)),key) < 0)) {
      p = q;
      q = p->forward[k];
    }
    update[k] = p;
  }

  if ((q != NULL) && (skl->skl_params->key_comp_func((void *)(((char *)q->value)+skl->skl_params->key_offset),key) == 0)) {
    if (value != NULL) {
      *value = q->value;
    }
    for(k=0; k<=m && (p=update[k])->forward[k] == q; k++) 
      p->forward[k] = q->forward[k];
    RW_FREE(q);
    while( skl->head->forward[m] == NULL && m > 0 )
      m--;
    skl->level = m;

    elem = (rw_sklist_element_t *)(((char *)*value)+skl->skl_params->elem_offset);
#ifdef RW_SKLIST_DEBUG
    RW_ASSERT(elem->sklist == skl);
    elem->sklist = NULL;
#endif
    elem->node = NULL;
    skl->sklist_len--;

    if (skl->sklist_len == 0) {
      RW_FREE(skl->head);
      skl->head = NULL;
    }

    return RW_STATUS_SUCCESS;
  }

  return RW_STATUS_FAILURE;
}


/**
 * This routine walks a Skiplist and performs a sanity-check on it
 */
rw_status_t
rw_sklist_debug_sanity_check(rw_sklist_t *skl)
{
  rw_sklist_node_t *p, *next;
  uint32_t count, errors = 0;
  RW_ASSERT(skl);

  // Walk the sklist to determine its actual length and verify pinter integrity
  count = 0;
  if (skl->head) {
    for(count=0,p=skl->head[0].forward[0];p;p=p->forward[0]) {
      count++;
    }
  }
  RW_ASSERT(count == RW_SKLIST_LENGTH(skl));
  errors += (count == RW_SKLIST_LENGTH(skl))?0:1;

  /*
   * Verify sklist is in sorted order
   * Checking the full transitive ordering of all elements would require n^2 operations,
   * so just check all adjacent nodes
   */
  if (skl->head) {
    int rc;
    p =  skl->head[0].forward[0];

    next = NULL;
    if (p) {
      next = p->forward[0];
    }

    while(next) {
      rc = skl->skl_params->key_comp_func((void *)(((char *)(p->value)+skl->skl_params->key_offset)),
                                        (void *)(((char *)(next->value)+skl->skl_params->key_offset)));
      RW_ASSERT(rc < 0);
      errors += (rc < 0)?0:1;

      p = next;
      next = p->forward[0];
    }
  }

  return (0 == errors)?RW_STATUS_SUCCESS:RW_STATUS_FAILURE;
}



/**
 * Function to compare two integers keys of a SkipList
 */
int
rw_sklist_comp_int(void * v1, void * v2)
{
  int i1, i2;
  RW_ASSERT(v1 != NULL);
  RW_ASSERT(v2 != NULL);
  i1 = *(int *)v1;
  i2 = *(int *)v2;
  
  if (i1 > i2) {
    return 1;
  }
  if (i2 > i1) {
    return -1;
  }
  return 0;
}



/**
 * Function to compare two uint32_t keys of a SkipList
 */
int
rw_sklist_comp_uint32_t(void * v1, void * v2)
{
  uint32_t i1, i2;
  RW_ASSERT(v1 != NULL);
  RW_ASSERT(v2 != NULL);
  i1 = *(uint32_t *)v1;
  i2 = *(uint32_t *)v2;
  if (i1 > i2) {
    return 1;
  }
  if (i2 > i1) {
    return -1;
  }
  return 0;
}

/**
 * Function to compare two uint32_t keys of a SkipList
 */
int
rw_sklist_comp_uint64_t(void * v1, void * v2)
{
  uint64_t i1, i2;
  RW_ASSERT(v1 != NULL);
  RW_ASSERT(v2 != NULL);
  i1 = *(uint64_t *)v1;
  i2 = *(uint64_t *)v2;
  if (i1 > i2) {
    return 1;
  }
  if (i2 > i1) {
    return -1;
  }
  return 0;
}

/**
 * Function to compare two character buffer keys of a SkipList
 */
int
rw_sklist_comp_charbuf(void * v1, void * v2)
{
  const uint8_t *p1, *p2;
  int32_t result;
  RW_ASSERT(v1 != NULL);
  RW_ASSERT(v2 != NULL);
  p1 = (const uint8_t *)v1;
  p2 = (const uint8_t *)v2;
  result = (int32_t)*p1 - (int32_t)*p2;
  return result?result:strcmp((const char *)p1,(const char *)p2); /* try to avoid strcmp */
}



/**
 * Function to compare two character pointer keys of a SkipList
 */
int rw_sklist_comp_charptr(void * v1, void * v2)
{
  const char *p1, *p2;
  int result;
  RW_ASSERT(v1 != NULL);
  RW_ASSERT(v2 != NULL);
  p1 = *(const char **)v1;
  p2 = *(const char **)v2;
  RW_ASSERT(p1 != NULL);
  RW_ASSERT(p2 != NULL);
  result = (int)*p1 - (int)*p2;
  return result?result:strcmp(p1,p2); /* try to avoid strcmp */
}

