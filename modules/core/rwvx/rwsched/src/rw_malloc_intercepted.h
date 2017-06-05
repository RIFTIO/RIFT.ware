
/*
 * STANDARD_RIFT_IO_COPYRIGHT
 *
 */

#ifndef RW_MALLOC_INTERCEPTED_H 
#define RW_MALLOC_INTERCEPTED_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define __USE_GNU
#include <dlfcn.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <ck.h>

#include <glib.h>
#include "rwlib.h"

extern void *malloc (size_t);
extern int g_malloc_intercepted;

#define malloc(_size) \
({                     \
  RW_RESOURCE_TRACK_HANDLE rwresource_track_handle = g_rwresource_track_handle;  \
  void *ret;                                                  \
  g_rwresource_track_handle = NULL;                           \
  ret = malloc(_size);                                       \
  g_rwresource_track_handle = rwresource_track_handle;        \
  if (g_malloc_intercepted && ret)                                                    \
    RW_RESOURCE_TRACK_ATTACH_CHILD_W_LOC(g_rwresource_track_handle, ret, _size, RW_MALLOC_TAG, G_STRLOC); \
  ret; \
})

#endif /*RW_MALLOC_INTERCEPTED_H*/

