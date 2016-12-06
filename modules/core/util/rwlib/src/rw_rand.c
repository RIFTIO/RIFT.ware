/**
 * @file rw_rand.c
 * @brief RIFT rand library
 * 
 * @details Rift rand library derived from ISAAC by Bob Jenkins
 */
/*
  ------------------------------------------------------------------------------
  rand.c: By Bob Jenkins.  My random number generator, ISAAC.  Public Domain.
  MODIFIED:
  960327: Creation (addition of randinit, really)
  970719: use context, not global variables, for internal state
  980324: added main (ifdef'ed out), also rearranged randinit()
  010626: Note that this is public domain
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

#include "rw_rand.h"


#define ind(mm,x)  (*(uint32_t *)((uint8_t *)(mm) + ((x) & ((RW_RANDSIZ-1)<<2))))
#define rngstep(mix,a,b,mm,m,m2,r,x)            \
  {                                             \
    x = *m;                                     \
    a = (a^(mix)) + *(m2++);                    \
    *(m++) = y = ind(mm,x) + a + b;             \
    *(r++) = b = ind(mm,y>>RW_RANDSIZL) + x;    \
  }

void rw_isaac(rw_randctx *ctx)
{
  uint32_t a,b,x,y,*m,*mm,*m2,*r,*mend;
  mm=ctx->randmem; r=ctx->randrsl;
  a = ctx->randa; b = ctx->randb + (++ctx->randc);
  for (m = mm, mend = m2 = m+(RW_RANDSIZ/2); m<mend; )
  {
    rngstep( a<<13, a, b, mm, m, m2, r, x);
    rngstep( a>>6 , a, b, mm, m, m2, r, x);
    rngstep( a<<2 , a, b, mm, m, m2, r, x);
    rngstep( a>>16, a, b, mm, m, m2, r, x);
  }
  for (m2 = mm; m2<mend; )
  {
    rngstep( a<<13, a, b, mm, m, m2, r, x);
    rngstep( a>>6 , a, b, mm, m, m2, r, x);
    rngstep( a<<2 , a, b, mm, m, m2, r, x);
    rngstep( a>>16, a, b, mm, m, m2, r, x);
  }
  ctx->randb = b; ctx->randa = a;
}


#define mix(a,b,c,d,e,f,g,h)                    \
  {                                             \
    a^=b<<11; d+=a; b+=c;                       \
    b^=c>>2;  e+=b; c+=d;                       \
    c^=d<<8;  f+=c; d+=e;                       \
    d^=e>>16; g+=d; e+=f;                       \
    e^=f<<10; h+=e; f+=g;                       \
    f^=g>>4;  a+=f; g+=h;                       \
    g^=h<<8;  b+=g; h+=a;                       \
    h^=a>>9;  c+=h; a+=b;                       \
  }

/* if (flag==TRUE), then use the contents of randrsl[] to initialize mm[]. */
void
rw_randinit(rw_randctx *ctx,
            bool_t flag)
{
  uint32_t i;
  uint32_t a,b,c,d,e,f,g,h;
  uint32_t *m,*r;
  ctx->randa = ctx->randb = ctx->randc = 0;
  m=ctx->randmem;
  r=ctx->randrsl;
  a=b=c=d=e=f=g=h=0x9e3779b9;  /* the golden ratio */

  for (i=0; i<4; ++i)          /* scramble it */
  {
    mix(a,b,c,d,e,f,g,h);
  }

  /* By default, just fill in m[] with messy stuff */
  for (i=0; i<RW_RANDSIZ; i+=8)
    {
      mix(a,b,c,d,e,f,g,h);
      m[i  ]=a; m[i+1]=b; m[i+2]=c; m[i+3]=d;
      m[i+4]=e; m[i+5]=f; m[i+6]=g; m[i+7]=h;
    }

  if (flag) 
  {
    /* initialize using the contents of r[] as the seed */
    for (i=0; i<RW_RANDSIZ; i+=8)
    {
      a+=r[i  ]; b+=r[i+1]; c+=r[i+2]; d+=r[i+3];
      e+=r[i+4]; f+=r[i+5]; g+=r[i+6]; h+=r[i+7];
      mix(a,b,c,d,e,f,g,h);
      m[i  ]=a; m[i+1]=b; m[i+2]=c; m[i+3]=d;
      m[i+4]=e; m[i+5]=f; m[i+6]=g; m[i+7]=h;
    }
    /* do a second pass to make all of the seed affect all of m */
    for (i=0; i<RW_RANDSIZ; i+=8)
    {
      a+=m[i  ]; b+=m[i+1]; c+=m[i+2]; d+=m[i+3];
      e+=m[i+4]; f+=m[i+5]; g+=m[i+6]; h+=m[i+7];
      mix(a,b,c,d,e,f,g,h);
      m[i  ]=a; m[i+1]=b; m[i+2]=c; m[i+3]=d;
      m[i+4]=e; m[i+5]=f; m[i+6]=g; m[i+7]=h;
    }
  }

  rw_isaac(ctx);            /* fill in the first set of results */
  ctx->randcnt=RW_RANDSIZ;  /* prepare to use the first set of results */
}

__thread rw_randctx rw_randctx_per_thread;

uint32_t
rw_random(void)
{
  uint32_t ret = 0xffffffff;
  if (!rw_randctx_per_thread.initialized) {
    rw_randinit(&rw_randctx_per_thread,TRUE);
    rw_randctx_per_thread.initialized = TRUE;
  }
  ret = rw_rand(&rw_randctx_per_thread);
  return ret;
}
