/**
 * @file rw_rand.h
 * @brief RIFT rand library
 * 
 * @details Rift rand library derived from ISAAC by Bob Jenkins
 */
#if !defined(__RW_RAND_H__)
#define __RW_RAND_H__

#include "rwlib.h"

__BEGIN_DECLS

/*
 * This file is derived from ISAAC by Bob Jenkins
 * See: http://burtleburtle.net/bob/rand/isaacafa.html
 */

/*
  ------------------------------------------------------------------------------
  rand.h: definitions for a random number generator
  By Bob Jenkins, 1996, Public Domain
  MODIFIED:
  960327: Creation (addition of randinit, really)
  970719: use context, not global variables, for internal state
  980324: renamed seed to flag
  980605: recommend RANDSIZL=4 for noncryptography.
  010626: note this is public domain
  ------------------------------------------------------------------------------
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


#define RW_RANDSIZL   (4)
#define RW_RANDSIZ    (1<<RW_RANDSIZL)

/* context of random number generator */
typedef struct
{
  uint32_t randcnt;
  uint32_t randrsl[RW_RANDSIZ];
  uint32_t randmem[RW_RANDSIZ];
  uint32_t randa;
  uint32_t randb;
  uint32_t randc;
  bool_t   initialized;
  uint8_t pad[16384];
} rw_randctx;

void rw_randinit(rw_randctx *ctx, bool_t flag);
void rw_isaac(rw_randctx *ctx);
uint32_t rw_random(void);

/*
  ------------------------------------------------------------------------------
  Call rand(/o_ randctx *r _o/) to retrieve a single 32-bit random value
  ------------------------------------------------------------------------------
*/
#define rw_rand(r)                                                      \
  (!(r)->randcnt-- ?                                                    \
   (rw_isaac(r), (r)->randcnt=RW_RANDSIZ-1, (r)->randrsl[(r)->randcnt]) : \
   (r)->randrsl[(r)->randcnt])

#define RW_BITS_IN_RANDOM 32


__END_DECLS

#endif /* #if !defined(__RW_RAND_H__) */
