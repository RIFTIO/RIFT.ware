
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file rwmemlog_mgmt.cpp
 * @date 2015/09/18
 * @brief RIFT.ware in-memory logging management integration APIs.
 */

#include "rwmemlog_impl.h"
#include <rwmemlog_mgmt.h>

#include <inttypes.h>


rw_status_t rwmemlog_instance_config_validate(
  rwmemlog_instance_t* instance,
  RWPB_T_MSG(RwMemlog_InstanceConfig)* config,
  char* error,
  size_t errorlen)
{
  RW_ASSERT(instance);
  RW_ASSERT(config);

  if (config->has_filter_mask) {
    rwmemlog_filter_mask_t bad_mask
      = config->filter_mask & ~RWMEMLOG_MASK_INSTANCE_CONFIGURABLE;
    if (bad_mask) {
      snprintf( error, errorlen, "Bad flags %0" PRIu64, bad_mask );
      return RW_STATUS_OUT_OF_BOUNDS;
    }
  }

  return RW_STATUS_SUCCESS;
}


rw_status_t rwmemlog_instance_config_apply(
  rwmemlog_instance_t* instance,
  RWPB_T_MSG(RwMemlog_InstanceConfig)* config)
{
  RW_ASSERT(instance);
  RW_ASSERT(config);

  RWMEMLOG_GUARD_LOCK();

  if (config->has_minimum_retired_count) {
    instance->minimum_retired_count = config->minimum_retired_count;
  }

  if (config->has_maximum_keep_count) {
    instance->maximum_keep_count = config->maximum_keep_count;
  }

  if (config->has_filter_mask) {
    rwmemlog_instance_locked_filters_set( instance, config->filter_mask );
  }

  return RW_STATUS_SUCCESS;
}


rw_status_t rwmemlog_instance_command(
  rwmemlog_instance_t* instance,
  RWPB_T_MSG(RwMemlog_Command)* command)
{
  RW_ASSERT(instance);
  RW_ASSERT(command);

  RWMEMLOG_GUARD_LOCK();

  auto clear_msg = command->clear;
  if (clear_msg) {
    bool also_keep = (clear_msg->has_also_keep && clear_msg->also_keep);

    auto buffer = instance->retired_oldest;
    rwmemlog_buffer_t* previous = nullptr;
    while (buffer) {

      auto next_buffer = buffer->header.next;
      if (   (     also_keep
              || ! rwmemlog_buffer_locked_is_kept_or_will_keep(buffer))
          && rwmemlog_buffer_locked_is_command_match( buffer, clear_msg ) ) {
        rwmemlog_buffer_locked_unlink(
          buffer,
          previous,
          &instance->retired_oldest,
          &instance->retired_newest );
        rwmemlog_buffer_locked_clear(buffer);
        rwmemlog_buffer_locked_push_oldest(
          buffer,
          &instance->retired_oldest,
          &instance->retired_newest );
        if (nullptr == previous) {
          previous = buffer;
        }
      } else {
        previous = buffer;
      }
      buffer = next_buffer;
    }

    if (also_keep) {
      buffer = instance->keep_oldest;
      previous = nullptr;
      while (buffer) {
        auto next_buffer = buffer->header.next;
        if (rwmemlog_buffer_locked_is_command_match( buffer, clear_msg )) {
          rwmemlog_buffer_locked_unlink(
            buffer,
            previous,
            &instance->keep_oldest,
            &instance->keep_newest );
          rwmemlog_buffer_locked_clear(buffer);
          rwmemlog_buffer_locked_push_oldest(
            buffer,
            &instance->keep_oldest,
            &instance->keep_newest );
          if (nullptr == previous) {
            previous = buffer;
          }
        } else {
          previous = buffer;
        }
        buffer = next_buffer;
      }
    }
  }

  auto keep_msg = command->keep;
  if (keep_msg) {
    for (size_t i = 0; i < instance->buffer_count; ++i) {
      auto buffer = instance->buffer_pool[i];
      if (rwmemlog_buffer_locked_is_command_match( buffer, keep_msg )) {
        buffer->header.flags_mask |= RWMEMLOG_BUFFLAG_KEEP;
      }
    }
  }

  return RW_STATUS_SUCCESS;
}

