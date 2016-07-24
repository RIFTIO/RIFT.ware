
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/*!
 * @file rwmemlog_impl.h
 * @date 2015/09/11
 * @brief RIFT.ware in-memory logging, private implementation header.
 */

#ifndef RW_RWMEMLOG_IMPL_H_
#define RW_RWMEMLOG_IMPL_H_

#include <rwmemlog.h>
#include <rwlib.h>

#include <mutex>


typedef std::mutex rwmemlog_mutex_t;
typedef std::lock_guard<rwmemlog_mutex_t> rwmemlog_guard_t;

rwmemlog_mutex_t& rwmemlog_instance_mutex(
  rwmemlog_instance_t* instance );

void rwmemlog_instance_locked_slow_path_output(
  rwmemlog_instance_t* instance,
  uint64_t filter,
  const rwmemlog_entry_t* entry );

void rwmemlog_instance_locked_enable_traces(
  rwmemlog_instance_t* instance,
  rwmemlog_filter_mask_t filter_mask );

void rwmemlog_instance_locked_disable_traces(
  rwmemlog_instance_t* instance,
  rwmemlog_filter_mask_t filter_mask );

bool rwmemlog_instance_is_good(
  rwmemlog_instance_t* instance,
  bool crash );

bool rwmemlog_instance_locked_is_good(
  rwmemlog_instance_t* instance,
  bool crash );


#define RWMEMLOG_GUARD_LOCK() \
  rwmemlog_guard_t guard(rwmemlog_instance_mutex(instance))

#define RWMEMLOG_BUFFER_GUARD_LOCK() \
  rwmemlog_guard_t guard(rwmemlog_instance_mutex(buffer->header.instance))


namespace {

static inline bool rwmemlog_buffer_locked_is_active(
  rwmemlog_buffer_t* buffer )
{
  return buffer->header.flags_mask & RWMEMLOG_BUFFLAG_ACTIVE;
}

static inline bool rwmemlog_buffer_locked_is_kept(
  rwmemlog_buffer_t* buffer )
{
  return buffer->header.flags_mask & RWMEMLOG_BUFFLAG_ON_KEEP_LIST;
}

static inline bool rwmemlog_buffer_locked_is_kept_or_will_keep(
  rwmemlog_buffer_t* buffer )
{
  return buffer->header.flags_mask
    & (RWMEMLOG_BUFFLAG_KEEP | RWMEMLOG_BUFFLAG_ON_KEEP_LIST);
}

static inline void rwmemlog_buffer_locked_clear(
  rwmemlog_buffer_t* buffer )
{
  buffer->header.version++;
  buffer->header.used_units = 0;
  buffer->header.time_ns = 0;
  memset( buffer->header.data, 0, RWMEMLOG_BUFFER_DATA_SIZE_BYTES );
}

static inline void rwmemlog_buffer_locked_push_oldest(
  rwmemlog_buffer_t* buffer,
  rwmemlog_buffer_t** oldest,
  rwmemlog_buffer_t** newest )
{
  buffer->header.next = *oldest;
  *oldest = buffer;
  if (nullptr == *newest) {
    *newest = buffer;
  }
}

static inline void rwmemlog_buffer_locked_push_newest(
  rwmemlog_buffer_t* buffer,
  rwmemlog_buffer_t** oldest,
  rwmemlog_buffer_t** newest )
{
  auto previous = *newest;
  if (previous) {
    previous->header.next = buffer;
  }
  *newest = buffer;
  if (nullptr == *oldest) {
    *oldest = buffer;
  }
  buffer->header.next = nullptr;
}

static inline rwmemlog_buffer_t* rwmemlog_buffer_locked_pop_oldest(
  rwmemlog_buffer_t** oldest,
  rwmemlog_buffer_t** newest )
{
  if (nullptr == *oldest) {
    return nullptr;
  }
  auto buffer = *oldest;
  *oldest = buffer->header.next;
  if (buffer == *newest) {
    *newest = nullptr;
  }
  buffer->header.next = nullptr;
  return buffer;
}

static inline void rwmemlog_buffer_locked_unlink(
  rwmemlog_buffer_t* buffer,
  rwmemlog_buffer_t* previous,
  rwmemlog_buffer_t** oldest,
  rwmemlog_buffer_t** newest )
{
  if (previous) {
    previous->header.next = buffer->header.next;
  }
  if (*newest == buffer) {
    *newest = previous;
  }
  if (*oldest == buffer) {
    *oldest = buffer->header.next;
  }
  buffer->header.next = nullptr;
}

template <typename CommandMsg>
static inline bool rwmemlog_buffer_locked_is_command_match(
  rwmemlog_buffer_t* buffer,
  const CommandMsg* command_msg)
{
  return
       buffer->header.object_name
    && (   NULL == command_msg->object_name
        || 0 == strcmp( buffer->header.object_name, command_msg->object_name ) )
    && (   !command_msg->has_object_id
        || command_msg->object_id == buffer->header.object_id );
}

} // empty namespace

#endif /* ndef RW_RWMEMLOG_IMPL_H_ */
