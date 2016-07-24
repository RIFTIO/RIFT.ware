
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/*!
 * @file rwmemlog_output.cpp
 * @date 2015/09/11
 * @brief RIFT.ware in-memory logging output support.
 */

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <rw_pb.h>

#include "rwmemlog_impl.h"
#include <rwmemlog_mgmt.h>


static rw_status_t rwmemlog_output_instance(
  rwmemlog_mgmt_output_t* output );

static rw_status_t rwmemlog_output_buffer(
  rwmemlog_mgmt_output_t* output );

static rw_status_t rwmemlog_output_entry(
  rwmemlog_mgmt_output_t* output );

static void rwmemlog_output_instance_to_stdout(
  rwmemlog_mgmt_output_t* output );

static rw_status_t rwmemlog_output_instance_to_string(
  rwmemlog_mgmt_output_t* output );

static rw_status_t rwmemlog_output_buffer_to_stdout(
  rwmemlog_mgmt_output_t* output );

static rw_status_t rwmemlog_output_buffer_to_string(
  rwmemlog_mgmt_output_t* output );

static rw_status_t rwmemlog_output_entry_to_stdout(
  rwmemlog_mgmt_output_t* output );

static rw_status_t rwmemlog_output_entry_to_string(
  rwmemlog_mgmt_output_t* output );

rw_status_t rwmemlog_instance_get_output(
  rwmemlog_instance_t* instance,
  const rw_keyspec_path_t* input_ks,
  rw_keyspec_path_t** output_ks,
  RWPB_T_MSG(RwMemlog_InstanceState)** output_msg )
{
  RW_ASSERT(instance);
  RW_ASSERT(input_ks);
  RW_ASSERT(output_ks);
  RW_ASSERT(nullptr == *output_ks);
  RW_ASSERT(output_msg);
  RW_ASSERT(nullptr == *output_msg);

  RWMEMLOG_GUARD_LOCK();

  // Make a keyspec for the output message.
  UniquePtrKeySpecPath::uptr_t instance_ks(
    rw_keyspec_path_create_dup_of_type_trunc(
      input_ks,
      (rw_keyspec_instance_t *)instance->ks_instance,
      RWPB_G_PATHSPEC_PBCMD(RwMemlog_InstanceState) ) );
  RW_ASSERT(instance_ks.get());

  /*
   * ATTN: Future: Parse the input ks for specific instance name,
   * buffer, or entry.  For now, we'll just fill the whole thing in.
   */

  rwmemlog_mgmt_output_t output = {};
  output.instance = instance;
  output.ks_instance = (rw_keyspec_instance_t *)instance->ks_instance;
  if (output.ks_instance) {
    output.pbc_instance = output.ks_instance->pbc_instance;
  }

  // Keep the message in uptr until handed to the caller.
  RWPB_M_MSG_DECL_UPTR_ALLOC( RwMemlog_InstanceState, instance_msg );
  output.instance_msg = instance_msg.get();

  auto rs = rwmemlog_output_instance( &output );
  if (rs == RW_STATUS_FAILURE) {
    return rs;
  }

  *output_ks = instance_ks.release();
  *output_msg = instance_msg.release();
  return RW_STATUS_SUCCESS;
}

static rw_status_t rwmemlog_output_instance(
  rwmemlog_mgmt_output_t* output )
{
  RW_ASSERT(output);

  auto instance_msg = output->instance_msg;
  RW_ASSERT(instance_msg);
  auto instance = output->instance;
  RW_ASSERT(instance);

  instance_msg->instance_name = strdup(instance->instance_name);
  instance_msg->buffer_count = instance->buffer_count;
  instance_msg->has_buffer_count = true;
  instance_msg->filter_mask = instance->filter_mask;
  instance_msg->has_filter_mask = true;

  for (unsigned i = 0; i < instance->buffer_count; ++i) {
    output->buffer = instance->buffer_pool[i];
    RW_ASSERT(output->buffer);
    auto rs = rwmemlog_output_buffer( output );
    if (rs == RW_STATUS_FAILURE) {
      return rs;
    }
  }

  return RW_STATUS_SUCCESS;
}


static rw_status_t rwmemlog_output_buffer(
  rwmemlog_mgmt_output_t* output )
{
  RW_ASSERT(output);

  RWPB_T_MSG(RwMemlog_InstanceState_Buffer)* buffer_msg = nullptr;
  if (output->instance_msg) {
    // Allocate a new buffer message.
    ProtobufCMessage* buffer_pbcm = nullptr;
    bool ok = protobuf_c_message_set_field_message(
      output->pbc_instance,
      &output->instance_msg->base,
      RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState, buffer),
      &buffer_pbcm );
    if (!ok) {
      return RW_STATUS_FAILURE;
    }
    buffer_msg = RWPB_M_MSG_CAST(RwMemlog_InstanceState_Buffer, buffer_pbcm);
  } else {
    buffer_msg = output->buffer_msg;
    RW_ASSERT(buffer_msg);
  }
  RW_ASSERT(buffer_msg);
  output->buffer_msg = buffer_msg;

  // Fill-in the basic buffer information.
  auto buffer = output->buffer;
  RW_ASSERT(buffer);
  const rwmemlog_buffer_header_t* header = &buffer->header;
  buffer_msg->address = reinterpret_cast<intptr_t>(buffer);
  buffer_msg->has_address = true;
  buffer_msg->object_id = header->object_id;
  buffer_msg->has_object_id = true;
  buffer_msg->version = header->version;
  buffer_msg->has_version = true;
  buffer_msg->time = header->time_ns;
  buffer_msg->has_time = true;
  buffer_msg->is_active = !!(header->flags_mask & RWMEMLOG_BUFFLAG_ACTIVE);
  buffer_msg->has_is_active = true;
  buffer_msg->is_keep = !!(header->flags_mask & RWMEMLOG_BUFFLAG_KEEP);
  buffer_msg->has_is_keep = true;

  if (header->object_name) {
    buffer_msg->object_name = strdup(header->object_name);
  }

  output->entry_index = 0;
  for (uint16_t unit_index = 0;
       unit_index < RWMEMLOG_BUFFER_DATA_SIZE_UNITS;
       ++output->entry_index) {

    // ATTN: memory barrier
    auto loc = (const rwmemlog_location_t*)header->data[unit_index];
    auto entry_decode = (uint64_t)header->data[unit_index+1];

    if (nullptr == loc || entry_decode == 0) {
      // bad entry header.
      break;
    }

    // barrier?
    uint16_t units = (uint16_t)(
        RWMEMLOG_ENTRY_HEADER_SIZE_UNITS
      + (   (entry_decode >> RWMEMLOG_ENTRY_DECODE_ARG0_SHIFT)
          & RWMEMLOG_ENTRY_DECODE_ARG0_MASK)
      + (   (entry_decode >> RWMEMLOG_ENTRY_DECODE_ARG1_SHIFT)
          & RWMEMLOG_ENTRY_DECODE_ARG1_MASK)
      + (   (entry_decode >> RWMEMLOG_ENTRY_DECODE_ARG2_SHIFT)
          & RWMEMLOG_ENTRY_DECODE_ARG2_MASK)
      + (   (entry_decode >> RWMEMLOG_ENTRY_DECODE_ARG3_SHIFT)
          & RWMEMLOG_ENTRY_DECODE_ARG3_MASK)
      + (   (entry_decode >> RWMEMLOG_ENTRY_DECODE_ARG4_SHIFT)
          & RWMEMLOG_ENTRY_DECODE_ARG4_MASK)
      + (   (entry_decode >> RWMEMLOG_ENTRY_DECODE_ARG5_SHIFT)
          & RWMEMLOG_ENTRY_DECODE_ARG5_MASK)
      + (   (entry_decode >> RWMEMLOG_ENTRY_DECODE_ARG6_SHIFT)
          & RWMEMLOG_ENTRY_DECODE_ARG6_MASK)
      + (   (entry_decode >> RWMEMLOG_ENTRY_DECODE_ARG7_SHIFT)
          & RWMEMLOG_ENTRY_DECODE_ARG7_MASK)
      + (   (entry_decode >> RWMEMLOG_ENTRY_DECODE_ARG8_SHIFT)
          & RWMEMLOG_ENTRY_DECODE_ARG8_MASK)
      + (   (entry_decode >> RWMEMLOG_ENTRY_DECODE_ARG9_SHIFT)
          & RWMEMLOG_ENTRY_DECODE_ARG9_MASK)
    );

    if (   units+unit_index > header->used_units
        || units > RWMEMLOG_ENTRY_TOTAL_SIZE_MAX_UNITS) {
      // bad decoder ring.
      break;
    }

    rwmemlog_unit_t local_copy[RWMEMLOG_ENTRY_TOTAL_SIZE_MAX_UNITS];
    memcpy( local_copy, &header->data[unit_index], sizeof(header->data[0])*units );

    // ATTN: barrier
    auto loc2 = (const rwmemlog_location_t*)header->data[unit_index];
    if (loc2 != loc) {
      // the entry changed concurrently
      break;
    }

    output->entry = (const rwmemlog_entry_t*)local_copy;
    rwmemlog_output_entry( output );

    unit_index += units;
  }
  return RW_STATUS_SUCCESS;
}


static rw_status_t rwmemlog_output_entry(
  rwmemlog_mgmt_output_t* output )
{
  RW_ASSERT(output);
  RW_ASSERT(output->buffer_msg);

  // Allocate a new entry message.
  ProtobufCMessage* entry_pbcm = nullptr;
  bool ok = protobuf_c_message_set_field_message(
    output->pbc_instance,
    &output->buffer_msg->base,
    RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer, entry),
    &entry_pbcm );
  if (!ok) {
    return RW_STATUS_FAILURE;
  }
  auto entry_msg = RWPB_M_MSG_CAST(RwMemlog_InstanceState_Buffer_Entry, entry_pbcm);
  RW_ASSERT(entry_msg);
  output->entry_msg = entry_msg;

  /* Fill-in the basic entry information. */
  auto entry = output->entry;
  RW_ASSERT(entry);
  auto location = entry->location;
  if (!location) {
    return RW_STATUS_FAILURE;
  }
  entry_msg->index = output->entry_index;
  entry_msg->has_index = true;
  entry_msg->file = strdup(location->file);
  entry_msg->line = location->line;
  entry_msg->has_line = true;

  // Extract the argument iteration context.
  const intptr_t* meta = (const intptr_t*)(&location[1]);
  const rwmemlog_unit_t* unit = (const rwmemlog_unit_t*)(&entry[1]);
  const uint8_t* meta_decode = &location->meta_decode[RWMEMLOG_LOCATION_INDEX_DECODE_ARG0];
  uint64_t entry_decode = entry->entry_decode;

  // Iteration support for the simplified PB field setting
  static const ProtobufCFieldDescriptor* const arg_fdesc[] = {
    RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer_Entry, arg0),
    RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer_Entry, arg1),
    RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer_Entry, arg2),
    RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer_Entry, arg3),
    RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer_Entry, arg4),
    RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer_Entry, arg5),
    RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer_Entry, arg6),
    RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer_Entry, arg7),
    RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer_Entry, arg8),
    RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer_Entry, arg9),
    nullptr
  };
  auto fdescp = &arg_fdesc[0];

  // Okay, now ready to iterate the arguments and turn them into strings. Yay!
  for (output->args_output.arg_index = 0;
       output->args_output.arg_index < RWMEMLOG_ARG_MAX_COUNT;
       ++output->args_output.arg_index) {

    auto unit_count = entry_decode & RWMEMLOG_ENTRY_DECODE_PER_ARG_MASK;
    auto meta_info = *meta_decode;
    auto arg_type = (meta_info >> RWMEMLOG_META_DECODE_ARGTYPE_SHIFT)
        & RWMEMLOG_META_DECODE_ARGTYPE_MASK;
    auto meta_count = (meta_info >> RWMEMLOG_META_DECODE_META_COUNT_SHIFT)
        & RWMEMLOG_META_DECODE_META_COUNT_MASK;

    /* Use stack buffer for the arg string, in the hopes that it is enough. */
    char arg_string[1024];
    arg_string[0] = '\0';
    output->args_output.arg_string = arg_string;
    output->args_output.arg_eos = arg_string;
    output->args_output.arg_size = sizeof(arg_string);
    output->args_output.arg_left = output->args_output.arg_size;

    /* Try to convert the argument to a string. */
    rw_status_t rs = RW_STATUS_SUCCESS;
    switch (arg_type) {
      case RWMEMLOG_ARGTYPE_NONE: {
        continue;
      }
      case RWMEMLOG_ARGTYPE_CONST_STRING: {
        auto metap = (const rwmemlog_argtype_const_string_meta_t*)meta;
        if (metap->string) {
          rs = rwmemlog_output_strcpy( &output->args_output, metap->string );
        }
        break;
      }
      case RWMEMLOG_ARGTYPE_FUNC: {
        auto metap = (const rwmemlog_argtype_func_meta_t*)meta;
        if (metap->fp) {
          rs = (metap->fp)( &output->args_output, unit_count, unit );
        }
        break;
      }
      case RWMEMLOG_ARGTYPE_FUNC_STRING: {
        auto metap = (const rwmemlog_argtype_func_string_meta_t*)meta;
        if (metap->fp && metap->string) {
          rs = (metap->fp)( &output->args_output, metap->string, unit_count, unit );
        }
        break;
      }
      case RWMEMLOG_ARGTYPE_ENUM_STRING: {
        auto metap = (const rwmemlog_argtype_enum_string_meta_t*)meta;
        if (metap->string) {
          rs = rwmemlog_output_snprintf( &output->args_output, "%s=", metap->string );
        }
        if (RW_STATUS_SUCCESS == rs && metap->fp) {
          auto s = (metap->fp)(*(int*)unit);
          if (s) {
            rs = rwmemlog_output_snprintf( &output->args_output, "%s=", s );
          }
        }
        if (RW_STATUS_SUCCESS == rs) {
          rs = rwmemlog_output_snprintf( &output->args_output, "%d", *(int*)unit );
        }
        break;
      }
      default: {
        // ATTN: Is this okay?
        continue;
      }
    }

    if (arg_string[0]) {
      if (arg_string != output->args_output.arg_string) {
        // Ensure NUL termination.
        arg_string[sizeof(arg_string) - 1] = '\0';
      }
      protobuf_c_message_set_field_text_value(
        output->pbc_instance,
        &entry_msg->base,
        *fdescp,
        output->args_output.arg_string,
        0/*val_len*/ );

      if (arg_string != output->args_output.arg_string) {
        // ATTN: Would be nice to give the string to PB, when possible.
        protobuf_c_instance_free( output->pbc_instance, output->args_output.arg_string );
      }
    }

    // Iterate the argument.
    ++fdescp;
    meta += meta_count;
    unit += unit_count;
    entry_decode >>= RWMEMLOG_ENTRY_DECODE_PER_ARG_SHIFT;
    ++meta_decode;
  }
  return RW_STATUS_SUCCESS;
}

void rwmemlog_instance_to_stdout(
  rwmemlog_instance_t* instance )
{
  if (!instance) {
    return;
  }

  rwmemlog_mgmt_output_t output = {};
  output.instance = instance;
  output.ks_instance = (rw_keyspec_instance_t *)instance->ks_instance;
  if (output.ks_instance) {
    output.pbc_instance = output.ks_instance->pbc_instance;
  }

  RWPB_M_MSG_DECL_UPTR_ALLOC( RwMemlog_InstanceState, instance_msg );
  output.instance_msg = instance_msg.get();
  rwmemlog_output_instance( &output );
  rwmemlog_output_instance_to_stdout( &output );
}

static void rwmemlog_output_instance_to_stdout(
  rwmemlog_mgmt_output_t* output )
{
  RW_ASSERT(output);

  char out_string[65536];
  out_string[0] = '\0';
  output->args_output.arg_string = out_string;
  output->args_output.arg_eos = out_string;
  output->args_output.arg_size = sizeof(out_string);
  output->args_output.arg_left = output->args_output.arg_size;

  auto rs = rwmemlog_output_instance_to_string( output );
  fprintf( stderr, "%s\n", output->args_output.arg_string );

  auto instance_msg = output->instance_msg;
  for (unsigned i = 0; RW_STATUS_FAILURE != rs; ++i) {
    ProtobufCFieldInfo finfo = {};
    bool ok = protobuf_c_message_get_field_instance(
      output->pbc_instance,
      &instance_msg->base,
      RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState, buffer),
      i/*repeated_index*/,
      &finfo );
    if (!ok) {
      break;
    }
    auto pbcm = (const ProtobufCMessage*)finfo.element;
    output->buffer_msg = RWPB_M_MSG_SAFE_CAST(RwMemlog_InstanceState_Buffer, pbcm);
    if (!output->buffer_msg) {
      break;
    }
    rs = rwmemlog_output_buffer_to_stdout(output);
  }
}

static rw_status_t rwmemlog_output_instance_to_string(
  rwmemlog_mgmt_output_t* output )
{
  RW_ASSERT(output);
  auto instance_msg = output->instance_msg;
  if (!instance_msg) {
    rwmemlog_output_strcpy( &output->args_output, "No instance data" );
    return RW_STATUS_FAILURE;
  }

  auto pbc_instance = output->pbc_instance;
  ProtobufCFieldInfo finfo = {};

  rwmemlog_output_strcpy( &output->args_output, "Instance: " );

  if (protobuf_c_message_get_field_instance(
        pbc_instance,
        &instance_msg->base,
        RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState, instance_name),
        0/*repeated_index*/,
        &finfo) ) {

    PROTOBUF_C_CHAR_BUF_DECL_INIT(pbc_instance, cbuf);
    if (protobuf_c_field_info_get_xml( &finfo, &cbuf )) {
      rwmemlog_output_snprintf( &output->args_output, "name='%s' ", cbuf.buffer );
    }
    PROTOBUF_C_CHAR_BUF_CLEAR(&cbuf);
  }

  if (protobuf_c_message_get_field_instance(
        pbc_instance,
        &instance_msg->base,
        RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState, buffer_count),
        0/*repeated_index*/,
        &finfo) ) {

    PROTOBUF_C_CHAR_BUF_DECL_INIT(pbc_instance, cbuf);
    if (protobuf_c_field_info_get_xml( &finfo, &cbuf )) {
      rwmemlog_output_snprintf( &output->args_output, "bufs=%s ", cbuf.buffer );
    }
    PROTOBUF_C_CHAR_BUF_CLEAR(&cbuf);
  }

  if (protobuf_c_message_get_field_instance(
        pbc_instance,
        &instance_msg->base,
        RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState, filter_mask),
        0/*repeated_index*/,
        &finfo) ) {

    PROTOBUF_C_CHAR_BUF_DECL_INIT(pbc_instance, cbuf);
    if (protobuf_c_field_info_get_xml( &finfo, &cbuf )) {
      rwmemlog_output_snprintf( &output->args_output, "mask=%s ", cbuf.buffer );
    }
    PROTOBUF_C_CHAR_BUF_CLEAR(&cbuf);
  }

  return RW_STATUS_SUCCESS;
}

void rwmemlog_buffer_to_stdout(
  rwmemlog_buffer_t* buffer )
{
  if (!buffer || !buffer->header.instance) {
    return;
  }

  rwmemlog_mgmt_output_t output = {};
  output.buffer = buffer;
  output.instance = buffer->header.instance;
  output.ks_instance = (rw_keyspec_instance_t *)output.instance->ks_instance;
  if (output.ks_instance) {
    output.pbc_instance = output.ks_instance->pbc_instance;
  }

  RWPB_M_MSG_DECL_UPTR_ALLOC( RwMemlog_InstanceState_Buffer, buffer_msg );
  output.buffer_msg = buffer_msg.get();
  rwmemlog_output_buffer( &output );
  rwmemlog_output_buffer_to_stdout( &output );
}

void rwmemlog_buffer_to_stdout_with_chain(
  rwmemlog_buffer_t* buffer )
{
  (void)buffer;
}

void rwmemlog_buffer_to_stdout_with_retired(
  rwmemlog_buffer_t* buffer )
{
  (void)buffer;
}

static rw_status_t rwmemlog_output_buffer_to_stdout(
  rwmemlog_mgmt_output_t* output )
{
  RW_ASSERT(output);

  char out_string[65536];
  out_string[0] = '\0';
  output->args_output.arg_string = out_string;
  output->args_output.arg_eos = out_string;
  output->args_output.arg_size = sizeof(out_string);
  output->args_output.arg_left = output->args_output.arg_size;

  auto rs = rwmemlog_output_buffer_to_string( output );
  fprintf( stderr, "  %s\n", output->args_output.arg_string );

  auto buffer_msg = output->buffer_msg;
  for (unsigned i = 0; RW_STATUS_FAILURE != rs; ++i) {
    ProtobufCFieldInfo finfo = {};
    bool ok = protobuf_c_message_get_field_instance(
      output->pbc_instance,
      &buffer_msg->base,
      RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer, entry),
      i/*repeated_index*/,
      &finfo );
    if (!ok) {
      break;
    }
    auto pbcm = (const ProtobufCMessage*)finfo.element;
    output->entry_msg = RWPB_M_MSG_SAFE_CAST(RwMemlog_InstanceState_Buffer_Entry, pbcm);
    if (!output->entry_msg) {
      break;
    }
    rs = rwmemlog_output_entry_to_stdout(output);
  }
  return RW_STATUS_SUCCESS;
}

static rw_status_t rwmemlog_output_buffer_to_string(
  rwmemlog_mgmt_output_t* output )
{
  RW_ASSERT(output);
  auto buffer_msg = output->buffer_msg;
  if (!buffer_msg) {
    rwmemlog_output_strcpy( &output->args_output, "No buffer data" );
    return RW_STATUS_FAILURE;
  }

  auto pbc_instance = output->pbc_instance;
  ProtobufCFieldInfo finfo = {};

  rwmemlog_output_strcpy( &output->args_output, "  Buffer: " );

  static const struct {
    const ProtobufCFieldDescriptor* fdesc;
    const char* name;
  } tbl[] = {
    { RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer, address), "addr" },
    { RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer, object_name), "obj" },
    { RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer, object_id), "id" },
    { RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer, is_active), "act" },
    { RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer, is_keep), "keep" },
    { nullptr, nullptr, },
  };

  for (auto f = &tbl[0]; f->fdesc; ++f ) {
    if (protobuf_c_message_get_field_instance(
          pbc_instance,
          &buffer_msg->base,
          f->fdesc,
          0/*repeated_index*/,
          &finfo) ) {

      PROTOBUF_C_CHAR_BUF_DECL_INIT(pbc_instance, cbuf);
      if (protobuf_c_field_info_get_xml( &finfo, &cbuf )) {
        rwmemlog_output_snprintf( &output->args_output, "%s=%s ", f->name, cbuf.buffer );
      }
      PROTOBUF_C_CHAR_BUF_CLEAR(&cbuf);
    }
  }

  return RW_STATUS_SUCCESS;
}

static rw_status_t rwmemlog_output_entry_to_stdout(
  rwmemlog_mgmt_output_t* output )
{
  RW_ASSERT(output);

  char out_string[65536];
  out_string[0] = '\0';
  output->args_output.arg_string = out_string;
  output->args_output.arg_eos = out_string;
  output->args_output.arg_size = sizeof(out_string);
  output->args_output.arg_left = output->args_output.arg_size;

  auto rs = rwmemlog_output_entry_to_string( output );
  fprintf( stderr, "    %s\n", output->args_output.arg_string );
  return rs;
}

static rw_status_t rwmemlog_output_entry_to_string(
  rwmemlog_mgmt_output_t* output )
{
  RW_ASSERT(output);
  auto entry_msg = output->entry_msg;
  if (!entry_msg) {
    rwmemlog_output_strcpy( &output->args_output, "No entry data" );
    return RW_STATUS_FAILURE;
  }

  auto pbc_instance = output->pbc_instance;
  ProtobufCFieldInfo finfo = {};

  rwmemlog_output_strcpy( &output->args_output, "    " );

  if (protobuf_c_message_get_field_instance(
        pbc_instance,
        &entry_msg->base,
        RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer_Entry, file),
        0/*repeated_index*/,
        &finfo) ) {

    PROTOBUF_C_CHAR_BUF_DECL_INIT(pbc_instance, cbuf);
    if (protobuf_c_field_info_get_xml( &finfo, &cbuf )) {
      char* file = strrchr(cbuf.buffer, '/');
      if (file) {
        ++file;
      } else {
        file = cbuf.buffer;
      }
      rwmemlog_output_snprintf( &output->args_output, "%s:", file );
    }
    PROTOBUF_C_CHAR_BUF_CLEAR(&cbuf);
  }

  if (protobuf_c_message_get_field_instance(
        pbc_instance,
        &entry_msg->base,
        RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer_Entry, line),
        0/*repeated_index*/,
        &finfo) ) {

    PROTOBUF_C_CHAR_BUF_DECL_INIT(pbc_instance, cbuf);
    if (protobuf_c_field_info_get_xml( &finfo, &cbuf )) {
      rwmemlog_output_snprintf( &output->args_output, "%s: ", cbuf.buffer );
    }
    PROTOBUF_C_CHAR_BUF_CLEAR(&cbuf);
  }

  static const struct {
    const ProtobufCFieldDescriptor* fdesc;
    const char* name;
  } tbl[] = {
    { RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer_Entry, arg0), "a0" },
    { RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer_Entry, arg1), "a1" },
    { RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer_Entry, arg2), "a2" },
    { RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer_Entry, arg3), "a3" },
    { RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer_Entry, arg4), "a4" },
    { RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer_Entry, arg5), "a5" },
    { RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer_Entry, arg6), "a6" },
    { RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer_Entry, arg7), "a7" },
    { RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer_Entry, arg8), "a8" },
    { RWPB_G_MSG_FIELD_PBCFD(RwMemlog_InstanceState_Buffer_Entry, arg9), "a9" },
    { nullptr, nullptr, },
  };

  for (auto f = &tbl[0]; f->fdesc; ++f ) {
    if (protobuf_c_message_get_field_instance(
          pbc_instance,
          &entry_msg->base,
          f->fdesc,
          0/*repeated_index*/,
          &finfo) ) {

      PROTOBUF_C_CHAR_BUF_DECL_INIT(pbc_instance, cbuf);
      if (protobuf_c_field_info_get_xml( &finfo, &cbuf )) {
        rwmemlog_output_snprintf( &output->args_output, "%s=%s ", f->name, cbuf.buffer );
      }
      PROTOBUF_C_CHAR_BUF_CLEAR(&cbuf);
    }
  }

  return RW_STATUS_SUCCESS;
}
