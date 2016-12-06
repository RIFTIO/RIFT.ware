
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
 */



/**
 * @file rwlib.c
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @date 07/20/2013
 * @brief RIFT utilities
 * 
 * @details 
 *
 */

#include "rwlib.h"

rw_code_location_info_t g_assert_code_info;

/**
 * This function computes a quick hash for all the locations
 */
uint32_t rw_compute_code_location_hash(rw_code_location_info_t *info)
{
  uint32_t i;
  uint32_t hash = 0;

  RW_ASSERT(info);

  for (i = 0; i < info->location_count; ++i) {
    int lineno = info->location[i].lineno;
    /* use the upper five bits of lineno to set a bit in a 32 bit number 
     * For example if the lineno is 8, the 8th bit will be set
     */
    hash = hash | (1 << (lineno & 0x1F));
  }

  return hash;
}

/**
 * This function searches the existing code locations for a
 * given code location. If a match is found the index to
 * the code location is returned, else -1 is returned.
 */
int rw_find_code_location(rw_code_location_info_t *info,
	                	      rw_code_location_t *location)
{
  uint32_t i;

  RW_ASSERT(info);

  for (i = 0; i < info->location_count; ++i) {
    if (!memcmp(&info->location[i],
                location,
                sizeof(rw_code_location_t))) {
      return i;
    }
  }

  return -1;
}

int rw_get_code_location_count(rw_code_location_info_t *info)
{
  RW_ASSERT(info);
  return info->location_count;
}

rw_status_t rw_set_code_location(rw_code_location_info_t *info,
		                             rw_code_location_t *location)
{
  RW_ASSERT(info);

  if (info->location_count >= RW_MAX_CODE_LOCATIONS) {
    return RW_STATUS_OUT_OF_BOUNDS;
  }

  if (rw_find_code_location(info, location) >= 0) {
    /* trace location is already set, return quietly */
    return RW_STATUS_DUPLICATE;
  }

  memcpy(&info->location[info->location_count],
         location,
         sizeof(rw_code_location_t));

  info->location_count++;
  /* this is not strictly thread safe, but that is ok since it is not
   * material */
  info->location_hash = rw_compute_code_location_hash(info);

  return RW_STATUS_SUCCESS;
}

rw_status_t rw_unset_code_location(rw_code_location_info_t *info,
		                               rw_code_location_t *location)
{
  int index;
  uint32_t i;

  RW_ASSERT(info);

  if (info->location_count == 0) {
    /* NO trace location is set, return quietly */
    return RW_STATUS_NOTFOUND;
  }

  index = rw_find_code_location(info, location);
  if (index < 0) {
    /* this trace location is NOT set, return quietly */
    return RW_STATUS_NOTFOUND;
  }

  for (i = index; i < info->location_count-1; ++i) {
    memcpy(&info->location[i],
           &info->location[i+1],
           sizeof(rw_code_location_t));
  }

  info->location_count--;

  /* this is not strictly thread safe, but that is ok since it is not
   * material */
  info->location_hash = rw_compute_code_location_hash(info);

  return RW_STATUS_SUCCESS;
}

rw_status_t rw_init_assert_ignore_location_info(void)
{
  memset(&g_assert_code_info, 0, sizeof(rw_code_location_info_t));
  return RW_STATUS_SUCCESS;
}

rw_status_t rw_set_assert_ignore_location(rw_code_location_t *location)
{
  return rw_set_code_location(&g_assert_code_info, location);
}

rw_status_t rw_unset_assert_ignore_location(rw_code_location_t *location)
{
  return rw_unset_code_location(&g_assert_code_info, location);
}

int rw_get_assert_ignore_location_count(void)
{
  return g_assert_code_info.location_count;
}

_RW_THREAD_ RW_RESOURCE_TRACK_HANDLE g_rwresource_track_handle;
pthread_mutex_t g_rwresource_track_mutex = PTHREAD_MUTEX_INITIALIZER;
int g_malloc_intercepted;
int g_callstack_depth;
int g_heap_track_nth;
int g_heap_track_bigger_than;
int g_heap_decode_using;

static void __attribute__((constructor)) _rw_res_track_init(void)
{
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&g_rwresource_track_mutex, &attr);
}
