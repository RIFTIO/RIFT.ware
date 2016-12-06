
/*
 * 
 *   Copyright 2016 RIFT.IO Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
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

