
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
 * @file rwmemlog_base.cpp
 * @date 2015/09/11
 * @brief RIFT.ware in-memory logging.
 */

#include "rwmemlog_impl.h"

#include <algorithm>
#include <set>


static void rwmemlog_instance_locked_buffer_pool_extend(
  rwmemlog_instance_t* instance,
  size_t buf_count );

static rwmemlog_buffer_t* rwmemlog_instance_locked_buffer_activate(
  rwmemlog_instance_t* instance,
  rwmemlog_ns_t ns );

static void rwmemlog_instance_locked_buffer_retire(
  rwmemlog_instance_t* instance,
  rwmemlog_buffer_t* buffer,
  rwmemlog_ns_t ns );

static void rwmemlog_instance_locked_buffer_keep(
  rwmemlog_instance_t* instance,
  rwmemlog_buffer_t* buffer );


static rwmemlog_filter_mask_t rwmemlog_filter_enable_fixup(
  rwmemlog_filter_mask_t filter_mask )
{
  filter_mask &= RWMEMLOG_MASK_INSTANCE_CONFIGURABLE;

  /*
   * Make sure there are no MEM-level gaps.  Use the highest set MEM
   * level as the upper bound of all levels to set.  So, if MEM4 is the
   * highest MEM level set, then also set MEM1-MEM3.
   */
  rwmemlog_filter_mask_t set_levels = RWMEMLOG_MASK_ALL_LEVELS;
  rwmemlog_filter_mask_t check_level = RWMEMLOG_MEM7;
  while (check_level != RWMEMLOG_MEM0) {
    RW_ASSERT(check_level & RWMEMLOG_MASK_ALL_LEVELS);
    if (filter_mask & check_level) {
      break;
    }
    set_levels = (set_levels >> 1) & RWMEMLOG_MASK_ALL_LEVELS;
    check_level >>= 1;
  }
  filter_mask |= set_levels;
  filter_mask |= RWMEMLOG_MASK_INSTANCE_ALWAYS_SET;
  return filter_mask;
}

static rwmemlog_filter_mask_t rwmemlog_filter_disable_fixup(
  rwmemlog_filter_mask_t filter_mask )
{
  filter_mask &= RWMEMLOG_MASK_INSTANCE_CONFIGURABLE;

  /*
   * Make sure there are no MEM-level gaps.  Use the lowest clear MEM
   * level the lower bound of all the levels to clear.  So, if MEM4 is
   * the lowest MEM level clear, then also clear all of MEM4-MEM7.
   */
  rwmemlog_filter_mask_t clear_levels = RWMEMLOG_MASK_ALL_LEVELS & ~RWMEMLOG_MEM0;
  rwmemlog_filter_mask_t check_level = RWMEMLOG_MEM1;
  while (check_level != RWMEMLOG_MEM7) {
    RW_ASSERT(check_level & RWMEMLOG_MASK_ALL_LEVELS);
    if (0 == (filter_mask & check_level)) {
      break;
    }
    clear_levels = (clear_levels << 1) & RWMEMLOG_MASK_ALL_LEVELS;
    check_level <<= 1;
  }
  filter_mask &= ~clear_levels;
  filter_mask |= RWMEMLOG_MASK_INSTANCE_ALWAYS_SET;
  return filter_mask;
}

rwmemlog_instance_t* rwmemlog_instance_alloc(
  const char* descr,
  uint32_t minimum_retired_count,
  uint32_t maximum_keep_count )
{
  RW_ASSERT(descr);
  RW_ASSERT(descr[0]);

  RW_STATIC_ASSERT(   (sizeof(int) == sizeof(rwmemlog_unit_t))
                   || (sizeof(void*) == sizeof(rwmemlog_unit_t)));
  RW_STATIC_ASSERT(sizeof(rwmemlog_filter_mask_t) == sizeof(rwmemlog_filter_t));

  rwmemlog_instance_t* instance = (rwmemlog_instance_t*)calloc(1, sizeof(rwmemlog_instance_t));
  RW_ASSERT(instance);

  // Default the minimum
  if (!minimum_retired_count) {
    minimum_retired_count = RWMEMLOG_INSTANCE_POOL_SIZE_DEFAULT_MIN_BUFS;
  }
  instance->minimum_retired_count = minimum_retired_count;

  // Default the maximum
  if (!maximum_keep_count) {
    maximum_keep_count = RWMEMLOG_INSTANCE_POOL_SIZE_DEFAULT_MAX_KEEP_BUFS;
  }
  instance->maximum_keep_count = maximum_keep_count;

  instance->mutex = new rwmemlog_mutex_t();
  RWMEMLOG_GUARD_LOCK();

  rwmemlog_instance_locked_buffer_pool_extend( instance, minimum_retired_count );

  strncpy( instance->instance_name, descr, sizeof(instance->instance_name) );
  instance->instance_name[sizeof(instance->instance_name)-1] = '\0';
  instance->filter_mask = RWMEMLOG_MASK_INSTANCE_DEFAULT;

  return instance;
}

rwmemlog_mutex_t& rwmemlog_instance_mutex(
  rwmemlog_instance_t* instance )
{
  return *(rwmemlog_mutex_t*)instance->mutex;
}

void rwmemlog_instance_destroy(
  rwmemlog_instance_t* instance )
{
  RW_ASSERT(instance);
  RW_ASSERT(!instance->dts_reg_state);
  RW_ASSERT(!instance->dts_reg_rpc);
  RW_ASSERT(!instance->dts_config_group);
  RW_ASSERT(!instance->dts_config_reg);
  RW_ASSERT(!instance->dts_api);
  RW_ASSERT(!instance->tasklet_info);

  RW_ASSERT(instance->buffer_pool);
  RW_ASSERT(instance->mutex);

  rwmemlog_mutex_t* mutex = &rwmemlog_instance_mutex(instance);
  {
    RWMEMLOG_GUARD_LOCK();

    for (size_t i = 0; i < instance->buffer_count; ++i) {
      auto buffer = instance->buffer_pool[i];
      RW_ASSERT( !rwmemlog_buffer_locked_is_active(buffer) ); // must be no active buffers
      free( buffer );
    }
    free( instance->buffer_pool );
    instance->buffer_pool = nullptr;
    instance->mutex = nullptr;
  }
  delete mutex;
  free( instance );
}

void rwmemlog_instance_register_app(
  rwmemlog_instance_t* instance,
  intptr_t app_context,
  rwmemlog_callback_rwlog_fp_t rwlog_callback,
  rwmemlog_callback_rwtrace_fp_t rwtrace_callback,
  void* ks_instance )
{
  RW_ASSERT(instance);
  /* Don't verify the args - the app is free to NULL any and all of them. */

  RWMEMLOG_GUARD_LOCK();
  instance->app_context = app_context;
  instance->rwlog_callback = rwlog_callback;
  instance->rwtrace_callback = rwtrace_callback;
  instance->ks_instance = ks_instance;
}

rw_status_t rwmemlog_instance_invoke_locked(
  rwmemlog_instance_t* instance,
  rwmemlog_instance_lock_fp_t function,
  intptr_t context )
{
  RW_ASSERT(instance);
  RW_ASSERT(function);

  RWMEMLOG_GUARD_LOCK();
  return function(instance, context);
}

static void rwmemlog_instance_locked_buffer_pool_extend(
  rwmemlog_instance_t* instance,
  size_t buf_count )
{
  size_t new_size = buf_count + instance->buffer_count;
  instance->buffer_pool = (rwmemlog_buffer_t**)realloc(
    instance->buffer_pool,
    new_size*sizeof(instance->buffer_pool[0]) );
  RW_ASSERT(instance->buffer_pool);

  for (size_t i = 0; i < buf_count; ++i) {
    auto buffer = (rwmemlog_buffer_t*)calloc( 1, sizeof(rwmemlog_buffer_t) );
    RW_ASSERT(buffer);

    buffer->header.instance = instance;
    buffer->header.version = 1;
    buffer->header.flags_mask = RWMEMLOG_BUFFLAG_ACTIVE;

    instance->buffer_pool[instance->buffer_count] = buffer;
    instance->buffer_count++;

    rwmemlog_instance_locked_buffer_retire( instance, buffer, 0 );
  }
}

static rwmemlog_buffer_t* rwmemlog_instance_locked_buffer_activate(
  rwmemlog_instance_t* instance,
  rwmemlog_ns_t ns )
{
  while (1) {
    if (instance->current_retired_count <= instance->minimum_retired_count) {
      rwmemlog_instance_locked_buffer_pool_extend(
        instance,
        std::min(
          instance->minimum_retired_count/2,
          (uint32_t)(RWMEMLOG_INSTANCE_POOL_SIZE_DEFAULT_MIN_BUFS)) );
    }

    auto buffer = rwmemlog_buffer_locked_pop_oldest(
      &instance->retired_oldest,
      &instance->retired_newest );
    RW_ASSERT(buffer);
    --instance->current_retired_count;

    if (buffer->header.flags_mask & RWMEMLOG_BUFFLAG_KEEP) {
      rwmemlog_instance_locked_buffer_keep( instance, buffer );
      continue;
    }

    rwmemlog_buffer_locked_clear(buffer);
    buffer->header.time_ns = ns;
    buffer->header.flags_mask = RWMEMLOG_BUFFLAG_ACTIVE;
    return buffer;
  }
}

static void rwmemlog_instance_locked_buffer_retire(
  rwmemlog_instance_t* instance,
  rwmemlog_buffer_t* buffer,
  rwmemlog_ns_t ns )
{
  RW_ASSERT( rwmemlog_buffer_locked_is_active(buffer) );
  buffer->header.time_ns = ns;
  ++instance->current_retired_count;

  if (0 == buffer->header.used_units) {
    // Buffer is empty, so reuse quickly
    buffer->header.object_name = nullptr;
    buffer->header.object_id = 0;
    buffer->header.time_ns = 0;
    buffer->header.flags_mask = 0;
    rwmemlog_buffer_locked_push_oldest(
      buffer,
      &instance->retired_oldest,
      &instance->retired_newest );
  } else {
    // Buffer is not empty, so reuse slowly
    buffer->header.flags_mask &= ~RWMEMLOG_BUFFLAG_ACTIVE;
    rwmemlog_buffer_locked_push_newest(
      buffer,
      &instance->retired_oldest,
      &instance->retired_newest );
  }
}

static void rwmemlog_instance_locked_buffer_keep(
  rwmemlog_instance_t* instance,
  rwmemlog_buffer_t* buffer )
{
  RW_ASSERT(!rwmemlog_buffer_locked_is_active(buffer)); // must be retired
  RW_ASSERT(buffer->header.flags_mask & RWMEMLOG_BUFFLAG_KEEP);

  buffer->header.flags_mask |= RWMEMLOG_BUFFLAG_ON_KEEP_LIST;
  rwmemlog_buffer_locked_push_newest(
    buffer,
    &instance->keep_oldest,
    &instance->keep_newest );

  if (instance->current_keep_count < instance->maximum_keep_count) {
    ++instance->current_keep_count;
    return;
  }

  // Need to re-retire the oldest kept buffer.
  auto reretire = rwmemlog_buffer_locked_pop_oldest(
    &instance->keep_oldest,
    &instance->keep_newest );
  RW_ASSERT(reretire);

  reretire->header.flags_mask &= ~(RWMEMLOG_BUFFLAG_KEEP | RWMEMLOG_BUFFLAG_ON_KEEP_LIST);

  rwmemlog_buffer_locked_push_newest(
    reretire,
    &instance->retired_oldest,
    &instance->retired_newest );
  ++instance->current_retired_count;
}

rwmemlog_buffer_t* rwmemlog_instance_get_buffer(
  rwmemlog_instance_t* instance,
  const char* object_name,
  intptr_t object_id )
{
  RW_ASSERT(instance);
  RW_ASSERT(object_name);
  RW_ASSERT(object_name[0]);

  RWMEMLOG_GUARD_LOCK();
  auto buffer = rwmemlog_instance_locked_buffer_activate( instance, rwmemlog_ns() );
  buffer->header.object_name = object_name;
  buffer->header.object_id = object_id;
  buffer->header.filter_mask = instance->filter_mask;
  buffer->header.next = buffer;
  return buffer;
}

void rwmemlog_buffer_return_all(
  rwmemlog_buffer_t* buffer )
{
  RW_ASSERT(buffer);

  auto instance = buffer->header.instance;
  RW_ASSERT(instance);
  RWMEMLOG_GUARD_LOCK();

  // There might be a chain, so iterate over the whole chain.
  auto ns = rwmemlog_ns();
  auto p = buffer;
  while (p) {
    auto next = p->header.next;
    rwmemlog_instance_locked_buffer_retire( instance, p, ns );
    p = next;
    if (buffer == p) {
      break;
    }
  }
}

void rwmemlog_buffer_filters_set(
  rwmemlog_buffer_t* buffer,
  rwmemlog_filter_mask_t filter_mask )
{
  RW_ASSERT(buffer);
  RWMEMLOG_BUFFER_GUARD_LOCK();
  buffer->header.filter_mask = rwmemlog_filter_enable_fixup(filter_mask);
  buffer->header.flags_mask |= RWMEMLOG_BUFFLAG_EXPLICIT_FILTER;
}

void rwmemlog_buffer_filters_enable(
  rwmemlog_buffer_t* buffer,
  rwmemlog_filter_mask_t filter_mask )
{
  RW_ASSERT(buffer);
  RWMEMLOG_BUFFER_GUARD_LOCK();
  filter_mask = buffer->header.filter_mask | filter_mask;
  buffer->header.filter_mask = rwmemlog_filter_enable_fixup(filter_mask);
  buffer->header.flags_mask |= RWMEMLOG_BUFFLAG_EXPLICIT_FILTER;
}

void rwmemlog_buffer_filters_disable(
  rwmemlog_buffer_t* buffer,
  rwmemlog_filter_mask_t filter_mask )
{
  RW_ASSERT(buffer);
  RWMEMLOG_BUFFER_GUARD_LOCK();
  filter_mask = buffer->header.filter_mask & ~filter_mask;
  buffer->header.filter_mask = rwmemlog_filter_disable_fixup(filter_mask);
  buffer->header.flags_mask |= RWMEMLOG_BUFFLAG_EXPLICIT_FILTER;
}

void rwmemlog_buffer_slow_path(
  rwmemlog_buffer_t* const* bufp,
  rwmemlog_filter_mask_t match_mask,
  const rwmemlog_entry_t* entry,
  rwmemlog_ns_t ns )
{
  RW_ASSERT(bufp);
  rwmemlog_buffer_t* buffer = *bufp;
  RW_ASSERT(buffer);
  rwmemlog_buffer_header_t* header = &buffer->header;
  RW_ASSERT(entry);

  auto instance = header->instance;
  RW_ASSERT(instance);
  RWMEMLOG_GUARD_LOCK();

  // Allocate a new buffer?
  if (match_mask & RWMEMLOG_RUNTIME_ALLOC) {
    // retire the buffer under next
    ns = ns ? ns : rwmemlog_ns();
    auto old = header->next;
    if (buffer != old && old) {
      header->next = old->header.next;
      rwmemlog_instance_locked_buffer_retire( instance, old, ns );
    }

    auto blank = rwmemlog_instance_locked_buffer_activate( instance, ns+1 );
    blank->header.next = header->next;
    header->next = blank;
    blank->header.object_name = header->object_name;
    blank->header.object_id = header->object_id;
    blank->header.filter_mask = header->filter_mask;
  }

  // Need to output the message?
  if (match_mask & RWMEMLOG_MASK_ANY_OUTPUT) {
    rwmemlog_instance_locked_slow_path_output( instance, match_mask, entry );
  }

  // Need to output the message?
  if (match_mask & RWMEMLOG_MASK_ANY_KEEP) {
    header->flags_mask |= RWMEMLOG_BUFFLAG_KEEP;

    /* Also set previous buffer as kept? */
    if (   header->used_units < RWMEMLOG_BUFFER_USED_SIZE_ALLOC_THRESHOLD_UNITS
        && header->next) {
      header->next->header.flags_mask |= RWMEMLOG_BUFFLAG_KEEP;
    }
  }
}

static void rwmemlog_instance_locked_update_buffer_filters(
  rwmemlog_instance_t* instance )
{
  /*
   * Change all of the active buffers that don't have explicit
   * filters.
   */
  for (size_t i = 0; i < instance->buffer_count; ++i) {
    auto buffer = instance->buffer_pool[i];
    if (   rwmemlog_buffer_locked_is_active( buffer )
        && 0 == (buffer->header.flags_mask & RWMEMLOG_BUFFLAG_EXPLICIT_FILTER) ) {
      buffer->header.filter_mask = instance->filter_mask;
    }
  }
}

void rwmemlog_instance_locked_filters_set(
  rwmemlog_instance_t* instance,
  rwmemlog_filter_mask_t filter_mask )
{
  instance->filter_mask = rwmemlog_filter_enable_fixup(filter_mask);
  rwmemlog_instance_locked_update_buffer_filters(instance);
}

void rwmemlog_instance_locked_filters_enable(
  rwmemlog_instance_t* instance,
  rwmemlog_filter_mask_t filter_mask )
{
  filter_mask = instance->filter_mask | filter_mask;
  instance->filter_mask = rwmemlog_filter_enable_fixup(filter_mask);
  rwmemlog_instance_locked_update_buffer_filters(instance);
}

void rwmemlog_instance_locked_filters_disable(
  rwmemlog_instance_t* instance,
  rwmemlog_filter_mask_t filter_mask )
{
  filter_mask = instance->filter_mask & ~filter_mask;
  instance->filter_mask = rwmemlog_filter_disable_fixup(filter_mask);
  rwmemlog_instance_locked_update_buffer_filters(instance);
}

bool rwmemlog_instance_is_good(
  rwmemlog_instance_t* instance,
  bool crash )
{
  RW_ASSERT(instance);
  RWMEMLOG_GUARD_LOCK();
  return rwmemlog_instance_locked_is_good( instance, crash );
}

static bool rwmemlog_instance_broken(
  bool crash )
{
  RW_ASSERT(!crash);
  return false;
}

bool rwmemlog_instance_locked_is_good(
  rwmemlog_instance_t* instance,
  bool crash )
{
  RW_ASSERT(instance);

  #define RWMEMLOG_VERIFY_CHECK(x_) \
    if (!(x_)) { \
      return rwmemlog_instance_broken(crash); \
    } \
    else

  typedef std::set<rwmemlog_buffer_t*> buf_set_t;
  buf_set_t all;
  buf_set_t retired;
  buf_set_t keep;
  buf_set_t active;
  buf_set_t active_next;

  unsigned i;
  for (i = 0; i < instance->buffer_count; ++i) {
    auto buffer = instance->buffer_pool[i];
    auto ok = all.emplace(buffer);
    RWMEMLOG_VERIFY_CHECK(ok.second);

    if (rwmemlog_buffer_locked_is_active(buffer)) {
      ok = active.emplace(buffer);
      RWMEMLOG_VERIFY_CHECK(ok.second);
      ok = active_next.emplace(buffer->header.next);
      RWMEMLOG_VERIFY_CHECK(ok.second);
    } else if (rwmemlog_buffer_locked_is_kept(buffer)) {
      ok = keep.emplace(buffer);
      RWMEMLOG_VERIFY_CHECK(ok.second);
    } else {
      ok = retired.emplace(buffer);
      RWMEMLOG_VERIFY_CHECK(ok.second);
    }
  }

  RWMEMLOG_VERIFY_CHECK(instance->current_retired_count <= instance->buffer_count);
  RWMEMLOG_VERIFY_CHECK(instance->current_retired_count == retired.size());
  RWMEMLOG_VERIFY_CHECK(instance->current_keep_count <= instance->buffer_count);
  RWMEMLOG_VERIFY_CHECK(instance->current_keep_count == keep.size());
  RWMEMLOG_VERIFY_CHECK(active.size() == active_next.size());

  auto buffer = instance->retired_oldest;
  for (i = 0; buffer && i < instance->buffer_count; ++i) {
    RWMEMLOG_VERIFY_CHECK(all.erase(buffer));
    RWMEMLOG_VERIFY_CHECK(retired.erase(buffer));
    if (!buffer->header.next) {
      RWMEMLOG_VERIFY_CHECK(buffer == instance->retired_newest);
    }
    buffer = buffer->header.next;
  }
  RWMEMLOG_VERIFY_CHECK(i == instance->current_retired_count);
  RWMEMLOG_VERIFY_CHECK(retired.empty());

  buffer = instance->keep_oldest;
  for (i = 0; buffer && i < instance->buffer_count; ++i) {
    RWMEMLOG_VERIFY_CHECK(all.erase(buffer));
    RWMEMLOG_VERIFY_CHECK(keep.erase(buffer));
    if (!buffer->header.next) {
      RWMEMLOG_VERIFY_CHECK(buffer == instance->keep_newest);
    }
    buffer = buffer->header.next;
  }
  RWMEMLOG_VERIFY_CHECK(i == instance->current_keep_count);
  RWMEMLOG_VERIFY_CHECK(keep.empty());

  for (i = 0; i < instance->buffer_count; ++i) {
    auto iter = active.begin();
    if (iter == active.end()) {
      break;
    }
    RWMEMLOG_VERIFY_CHECK(all.erase(*iter));
    RWMEMLOG_VERIFY_CHECK(active_next.erase(*iter));
    active.erase(iter);
  };
  RWMEMLOG_VERIFY_CHECK(active.empty());
  RWMEMLOG_VERIFY_CHECK(active_next.empty());
  RWMEMLOG_VERIFY_CHECK(all.empty());

  RWMEMLOG_VERIFY_CHECK(
       (i + instance->current_retired_count + instance->current_keep_count)
    == instance->buffer_count);

  return true;
}

void rwmemlog_instance_locked_slow_path_output(
  rwmemlog_instance_t* instance,
  rwmemlog_filter_mask_t match_mask,
  const rwmemlog_entry_t* entry )
{
  if (   (match_mask & RWMEMLOG_MASK_ANY_TRACE)
      && instance->rwtrace_callback) {
    // ATTN: make trace callback.
  }
  if (   (match_mask & RWMEMLOG_MASK_ANY_LOG)
      && instance->rwlog_callback) {
    // ATTN: make log callback.
  }
}

rw_status_t rwmemlog_output_snprintf(
  rwmemlog_output_t* output,
  const char* format,
  ... )
{
  RW_ASSERT(output);

  va_list ap;
  va_start(ap, format);
  int bytes = vsnprintf( output->arg_eos, output->arg_left, format, ap );
  va_end(ap);

  if ( bytes < 0 ) {
    *output->arg_eos = '\0';
    return RW_STATUS_FAILURE;
  }

  if ( (size_t)bytes > output->arg_left ) {
    output->arg_eos = &output->arg_eos[output->arg_left];
    output->arg_eos -= 4;
    strcpy( output->arg_eos, "..." );
    output->arg_eos += 3;
    output->arg_left = 0;
    return RW_STATUS_OUT_OF_BOUNDS;
  }

  output->arg_eos = &output->arg_eos[bytes];
  output->arg_left -= (size_t)bytes;

  return RW_STATUS_SUCCESS;
}

rw_status_t rwmemlog_output_entry_tsns(
  rwmemlog_output_t* output,
  size_t ndata,
  const rwmemlog_unit_t* data )
{
  RW_ASSERT(output);
  if (ndata != 1 || !data) {
    return RW_STATUS_FAILURE;
  }

  rwmemlog_ns_t ns = *(rwmemlog_ns_t*)data;
  auto seconds = (time_t)(ns / (rwmemlog_ns_t)1000000000);
  auto nanos = (unsigned)(ns % (rwmemlog_ns_t)1000000000);
  auto usecs = nanos / 1000;

  struct tm tm;
  if (&tm != gmtime_r( &seconds, &tm )) {
    return RW_STATUS_FAILURE;
  }

  char timebuf[64];
  auto bytes = strftime( timebuf, sizeof(timebuf), "%FT%T", &tm );
  if (19 != bytes) {
    return RW_STATUS_FAILURE;
  }

  return rwmemlog_output_snprintf( output, "%s.%06.6uZ", timebuf, usecs );
}

rw_status_t rwmemlog_output_strncpy(
  rwmemlog_output_t* output,
  const char* string,
  size_t n )
{
  if (!string) {
    return RW_STATUS_SUCCESS;
  }
  return rwmemlog_output_snprintf( output, "%.*s", n, string );
}

rw_status_t rwmemlog_output_strcpy(
  rwmemlog_output_t* output,
  const char* string )
{
  if (!string) {
    return RW_STATUS_SUCCESS;
  }
  return rwmemlog_output_snprintf( output, "%s", string );
}

rw_status_t rwmemlog_output_entry_strncpy(
  rwmemlog_output_t* output,
  size_t ndata,
  const rwmemlog_unit_t* data )
{
  RW_ASSERT(output);
  if (ndata < 1 || !data) {
    return RW_STATUS_FAILURE;
  }

  return rwmemlog_output_strncpy( output, (const char*)data, ndata*sizeof(rwmemlog_unit_t) );
}

rw_status_t rwmemlog_output_entry_printf_intptr(
  rwmemlog_output_t* output,
  const char* format,
  size_t ndata,
  const rwmemlog_unit_t* data )
{
  RW_ASSERT(output);
  if (ndata != 1 || !data) {
    return RW_STATUS_FAILURE;
  }

  auto v = *(intptr_t*)data;
  return rwmemlog_output_snprintf( output, format, v );
}
