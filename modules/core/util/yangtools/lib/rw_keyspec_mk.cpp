
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file rw_keyspec_mk.cpp
 * @author Sujithra Periasamy
 * @date 2014/11/27
 * @brief RiftWare general schema minikey utilities.
 */

#include "rw_schema.pb-c.h"
#include "rw_keyspec.h"
#include "rw_keyspec_mk.h"

#include <algorithm>
#include <stack>
#include <sstream>
#include <inttypes.h>

using namespace rw_yang;
#define IS_PATH_ENTRY_GENERIC(entry_) (((const ProtobufCMessage *)entry_)->descriptor == &rw_schema_path_entry__descriptor)

/*
 * Minikey descriptors.
 */
const rw_minikey_desc_t rw_var_minikey_basic_int32_desc = 
{
  .pbc_mdesc = nullptr, 
  .key_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_1_INT32,
};

const rw_minikey_desc_t rw_var_minikey_basic_int64_desc =
{
  .pbc_mdesc = nullptr,
  .key_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_1_INT64,
};

const rw_minikey_desc_t rw_var_minikey_basic_uint32_desc =
{
  .pbc_mdesc = nullptr,
  .key_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_1_UINT32,
};

const rw_minikey_desc_t rw_var_minikey_basic_uint64_desc =
{
  .pbc_mdesc = nullptr,
  .key_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_1_UINT64,
};

const rw_minikey_desc_t rw_var_minikey_basic_float_desc = 
{
  .pbc_mdesc = nullptr,
  .key_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_1_FLOAT,
};

const rw_minikey_desc_t rw_var_minikey_basic_double_desc = 
{
  .pbc_mdesc = nullptr,
  .key_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_1_DOUBLE,
};

const rw_minikey_desc_t rw_var_minikey_basic_string_pointy_desc =
{
  .pbc_mdesc = nullptr,
  .key_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_1,
};

const rw_minikey_desc_t rw_var_minikey_basic_string_desc =
{
  .pbc_mdesc = nullptr,
  .key_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_1_STRING,
};

const rw_minikey_desc_t rw_var_minikey_basic_bool_desc =
{
  .pbc_mdesc = nullptr,
  .key_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_1_BOOL,
};

const rw_minikey_desc_t rw_var_minikey_basic_bytes_pointy_desc =
{
  .pbc_mdesc = nullptr,
  .key_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_1,
};

const rw_minikey_desc_t rw_var_minikey_basic_bytes_desc =
{
  .pbc_mdesc = nullptr,
  .key_type = RW_SCHEMA_ELEMENT_TYPE_LISTY_1_BYTES,
};

static size_t rw_schema_mk_num_keys(
    const rw_schema_minikey_opaque_t *mk)
{
  const rw_minikey_desc_t *const *desc = reinterpret_cast<const rw_minikey_desc_t *const *>(mk);
  size_t n_keys = 0;

  if (!*desc) {
    return n_keys;
  }

  switch((*desc)->key_type) {
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_FLOAT:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_DOUBLE:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_INT32:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SINT32:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SFIXED32:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_UINT32:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_FIXED32:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_INT64:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SINT64:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SFIXED64:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_UINT64:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_FIXED64:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_BOOL:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_STRING:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_BYTES:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1:
      n_keys = 1;
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_2:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_3:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_4:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_5:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_6:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_7:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_8:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_9:
      n_keys = 2 + ((*desc)->key_type) - (RW_SCHEMA_ELEMENT_TYPE_LISTY_2);
      break;
    default:
      {
        RW_ASSERT_NOT_REACHED();
      }
  }

  return n_keys;
}

static inline ProtobufCType
rw_schema_elem_type_to_ctype(RwSchemaElementType elem_type)
{
  ProtobufCType ctype = PROTOBUF_C_TYPE_INT32;

  switch(elem_type) {
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_FLOAT:
      ctype = PROTOBUF_C_TYPE_FLOAT;
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_DOUBLE:
      ctype = PROTOBUF_C_TYPE_DOUBLE;
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_INT32:
      ctype = PROTOBUF_C_TYPE_INT32;
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SINT32:
      ctype = PROTOBUF_C_TYPE_SINT32;
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SFIXED32:
      ctype = PROTOBUF_C_TYPE_SFIXED32;
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_UINT32:
      ctype =  PROTOBUF_C_TYPE_UINT32;
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_FIXED32:
      ctype = PROTOBUF_C_TYPE_FIXED32;
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_INT64:
      ctype = PROTOBUF_C_TYPE_INT64;
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SINT64:
      ctype = PROTOBUF_C_TYPE_SINT64;
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SFIXED64:
      ctype = PROTOBUF_C_TYPE_SFIXED64;
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_UINT64:
      ctype = PROTOBUF_C_TYPE_UINT64;
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_FIXED64:
      ctype = PROTOBUF_C_TYPE_FIXED64;
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_BOOL:
      ctype = PROTOBUF_C_TYPE_BOOL;
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_STRING:
      ctype = PROTOBUF_C_TYPE_STRING;
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_BYTES:
      ctype = PROTOBUF_C_TYPE_BYTES;
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  return ctype;
}

static
bool init_mk_based_on_type(rw_schema_minikey_opaque_t *mk,
                           RwSchemaElementType type)
{
  bool ret_val = true;

  switch(type) {
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_FLOAT:
      {
        rw_minikey_basic_float_t *mk_f = 
            (rw_minikey_basic_float_t *)mk;
        rw_minikey_basic_float_init(mk_f);
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_DOUBLE:
      {
        rw_minikey_basic_double_t *mk_d =
            (rw_minikey_basic_double_t *)mk;
        rw_minikey_basic_double_init(mk_d);
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_INT32:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SINT32:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SFIXED32:
      {
        rw_minikey_basic_int32_t *mk_i =
            (rw_minikey_basic_int32_t *)mk;
        rw_minikey_basic_int32_init(mk_i);
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_UINT32:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_FIXED32:
      {
         rw_minikey_basic_uint32_t *mk_u =
             (rw_minikey_basic_uint32_t *)mk;
        rw_minikey_basic_uint32_init(mk_u);
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_INT64:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SINT64:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SFIXED64:
      {
        rw_minikey_basic_int64_t *mk_i64 =
            (rw_minikey_basic_int64_t *)mk;
        rw_minikey_basic_int64_init(mk_i64);
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_UINT64:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_FIXED64:
      {
        rw_minikey_basic_uint64_t *mk_u64=
            (rw_minikey_basic_uint64_t *)mk;
        rw_minikey_basic_uint64_init(mk_u64);
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_BOOL:
      {
        rw_minikey_basic_bool_t *mk_b=
            (rw_minikey_basic_bool_t *)mk;
        rw_minikey_basic_bool_init(mk_b);
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_STRING:
      {
        rw_minikey_basic_string_t *mk_s=
            (rw_minikey_basic_string_t *)mk;
        rw_minikey_basic_string_init(mk_s);
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_BYTES:
    default:
      {
        // ATTN: Fix ME
        RW_ASSERT_NOT_REACHED();
      }
  }

  return (ret_val);
}

static
bool fill_simple_mk_based_on_type(rw_schema_minikey_opaque_t *mk,
                                  RwSchemaElementType type,
                                  const void *value)
{
  bool ret_val = true;

  switch(type) {
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_FLOAT:
      {
        rw_minikey_basic_float_t *mk_f =
            (rw_minikey_basic_float_t *)mk;
        mk_f->k.key = *(float *)value;
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_DOUBLE:
      {
        rw_minikey_basic_double_t *mk_d =
            (rw_minikey_basic_double_t *)mk;
        mk_d->k.key = *(double *)value;
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_INT32:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SINT32:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SFIXED32:
      {
        rw_minikey_basic_int32_t *mk_i =
            (rw_minikey_basic_int32_t *)mk;
        mk_i->k.key = *(int32_t *)value;
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_UINT32:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_FIXED32:
      {
        rw_minikey_basic_uint32_t *mk_ui =
            (rw_minikey_basic_uint32_t *)mk;
        mk_ui->k.key = *(uint32_t *)value;
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_INT64:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SINT64:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SFIXED64:
      {
        rw_minikey_basic_int64_t *mk_i64 = 
            (rw_minikey_basic_int64_t *)mk;
        mk_i64->k.key = *(int64_t *)value;
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_UINT64:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_FIXED64:
      {
        rw_minikey_basic_uint64_t *mk_u64 = 
            (rw_minikey_basic_uint64_t *)mk;
        mk_u64->k.key = *(int64_t *)value;
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_BOOL:
      {
        rw_minikey_basic_bool_t *mk_b=
            (rw_minikey_basic_bool_t *)mk;
        mk_b->k.key = *(int *)value;
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_STRING:
      {
        rw_minikey_basic_string_t *mk_s=
            (rw_minikey_basic_string_t *)mk;
        char *str_val = (char *)value;
        strcpy(mk_s->k.key, str_val);
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_BYTES:
    default:
      {
        // ATTN: Fix me
        RW_ASSERT_NOT_REACHED();
      }
  }

  return ret_val;
}

static inline
const RwSchemaElementId* path_entry_get_element_id(
    const rw_keyspec_entry_t *path_entry)
{
  const ProtobufCFieldDescriptor *fd = nullptr;
  RW_ASSERT(path_entry);

  void *msg_ptr = nullptr;
  size_t offset = 0;
  protobuf_c_boolean is_dptr = false;

  size_t count = protobuf_c_message_get_field_desc_count_and_offset(&path_entry->base,
        RW_SCHEMA_TAG_ELEMENT_ID, &fd, &msg_ptr, &offset, &is_dptr);

  RW_ASSERT(count);
  RW_ASSERT(fd->type == PROTOBUF_C_TYPE_MESSAGE);

  return (RwSchemaElementId *)msg_ptr;
}

ProtobufCType 
rw_keyspec_entry_get_ctype(
    const rw_keyspec_entry_t *path_entry)
{
  const ProtobufCFieldDescriptor *fd = nullptr;
  RW_ASSERT(path_entry);

  void *msg_ptr = nullptr;
  size_t offset = 0;
  protobuf_c_boolean is_dptr = false;

  size_t count = protobuf_c_message_get_field_desc_count_and_offset(&path_entry->base,
        RW_SCHEMA_TAG_ELEMENT_ID, &fd, &msg_ptr, &offset, &is_dptr);

  RW_ASSERT(count);
  RW_ASSERT(fd->type == PROTOBUF_C_TYPE_MESSAGE);

  return rw_schema_elem_type_to_ctype(((RwSchemaElementId *)msg_ptr)->element_type);
}

rw_status_t
rw_schema_mk_get_from_message(const ProtobufCMessage *msg,
                              rw_schema_minikey_opaque_t *mk)
{
  RW_ASSERT(msg);

  rw_status_t status = RW_STATUS_FAILURE;

  const rw_yang_pb_msgdesc_t *ypbc = msg->descriptor->ypbc_mdesc;
  RW_ASSERT(ypbc);

  if (ypbc->msg_type == RW_YANGPBC_MSG_TYPE_LIST) {

    const rw_keyspec_entry_t *path_entry = 
        reinterpret_cast<const rw_keyspec_entry_t *>(ypbc->schema_entry_value);

    const RwSchemaElementId *elem_id = 
        path_entry_get_element_id(path_entry);

    if (elem_id->element_type < RW_SCHEMA_ELEMENT_TYPE_LISTY_1) {
      return status;
    }

    bool ret = init_mk_based_on_type(mk, elem_id->element_type);

    if (!ret) {
      return status;
    }

    const ProtobufCFieldDescriptor* kfd = nullptr;
    for (unsigned i = 0; i < msg->descriptor->n_fields; i++) {
      if (msg->descriptor->fields[i].rw_flags & RW_PROTOBUF_FOPT_KEY) {
        kfd = &msg->descriptor->fields[i];
        break;
      }
    }

    if (!kfd) {
      return status;
    }

    ProtobufCFieldInfo field = { };
    ret = protobuf_c_message_get_field_instance(nullptr, msg, kfd, 0, &field);

    if (!ret) {
      return status;
    }

    ret = fill_simple_mk_based_on_type(mk, elem_id->element_type, field.element); 

    if (!ret) {
      return status;
    }

    status = RW_STATUS_SUCCESS;
  }

  return status;
}

rw_status_t
rw_schema_mk_get_from_path_entry(const rw_keyspec_entry_t *path_entry,
                                 rw_schema_minikey_opaque_t *mk)
{
  rw_status_t status = RW_STATUS_FAILURE;
  RW_ASSERT(path_entry);

  const RwSchemaElementId *elem = 
      path_entry_get_element_id(path_entry);

  if (elem->element_type < RW_SCHEMA_ELEMENT_TYPE_LISTY_1) {
    return status;
  }

  bool ret = init_mk_based_on_type(mk, elem->element_type);

  if (!ret) {
    return status;
  }

  ProtobufCMessage* key = rw_keyspec_entry_get_key(path_entry, 0); // Only single key supported for now.
  if (!key) {
    return status;
  }

  const void *field_ptr = nullptr;
  UniquePtrProtobufCFree<char>::uptr_t uptr;

  if (IS_PATH_ENTRY_GENERIC(path_entry)) {

    RW_ASSERT(key->unknown_buffer);
    ProtobufCType ctype = rw_schema_elem_type_to_ctype(elem->element_type);
    ProtobufCScalarFieldUnion field;

    protobuf_c_boolean rc = protobuf_c_unknown_parse_scalar_type(
        nullptr, &key->unknown_buffer->unknown_fields[0], ctype, &field);
    
    if (!rc) {
      return status;
    }

    if (ctype == PROTOBUF_C_TYPE_STRING) {
      RW_ASSERT(field.char_v);
      field_ptr = (void *)field.char_v;
      uptr.reset(field.char_v);
    } else {
      field_ptr = &field;
    }

  } else {

    RW_ASSERT(key->descriptor->n_fields == 2);

    const ProtobufCFieldDescriptor *kfd = &key->descriptor->fields[1];
    RW_ASSERT(kfd);

    ProtobufCFieldInfo field = { };
    ret = protobuf_c_message_get_field_instance(nullptr, key, kfd, 0, &field);

    if (!ret) {
      return status;
    }

    field_ptr = field.element;
  }

  ret = fill_simple_mk_based_on_type(mk, elem->element_type, field_ptr);

  if (!ret) {
    return status;
  }

  status = RW_STATUS_SUCCESS;
  return (status);
}

static bool copy_key_to_proto(const rw_schema_minikey_opaque_t *mk,
                       const ProtobufCFieldDescriptor* key_fd,
                       ProtobufCMessage *msg)
{
  const rw_minikey_desc_t *const *desc = reinterpret_cast<const rw_minikey_desc_t *const *>(mk);
  
  if (!*desc) {
    return false;
  }

  RwSchemaElementType type = (*desc)->key_type;

  RW_ASSERT((type >= RW_SCHEMA_ELEMENT_TYPE_LISTY_1) 
            && (type < RW_SCHEMA_ELEMENT_TYPE_LISTY_2));

  uint8_t *base = reinterpret_cast<uint8_t *>(msg);

  switch(type) {
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_FLOAT:
      {
        rw_minikey_basic_float_t *mk_f = (rw_minikey_basic_float_t *)mk;
        *((float *)(base + key_fd->offset)) = mk_f->k.key;
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_DOUBLE:
      {
        rw_minikey_basic_double_t *mk_d = (rw_minikey_basic_double_t *)mk;
        *((double *)(base + key_fd->offset)) = mk_d->k.key;
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_INT32:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SINT32:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SFIXED32:
      {
        rw_minikey_basic_int32_t *mk_i = (rw_minikey_basic_int32_t *)mk;
        *((int32_t *)(base + key_fd->offset)) = mk_i->k.key;
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_UINT32:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_FIXED32:
      {
        rw_minikey_basic_uint32_t *mk_u = (rw_minikey_basic_uint32_t *)mk;
        *((uint32_t *)(base + key_fd->offset)) = mk_u->k.key;
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_INT64:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SINT64:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SFIXED64:
      {
        rw_minikey_basic_int64_t *mk_i64 = (rw_minikey_basic_int64_t *)mk;
        *((int64_t *)(base + key_fd->offset)) = mk_i64->k.key;
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_UINT64:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_FIXED64:
      {
        rw_minikey_basic_uint64_t *mk_u64= (rw_minikey_basic_uint64_t *)mk;
        *((uint64_t *)(base + key_fd->offset)) = mk_u64->k.key;
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_BOOL:
      {
        rw_minikey_basic_bool_t *mk_b= (rw_minikey_basic_bool_t *)mk;
        *((protobuf_c_boolean *)(base + key_fd->offset)) = mk_b->k.key;
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_STRING:
      {
        RW_ASSERT(key_fd->rw_flags & RW_PROTOBUF_FOPT_INLINE);
        rw_minikey_basic_string_t *mk_s = (rw_minikey_basic_string_t *)mk;
        strcpy((char *)(base + key_fd->offset), mk_s->k.key);
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1:
      {
        switch(key_fd->type)
        {
          case PROTOBUF_C_TYPE_STRING:
            {
              if (!(key_fd->rw_flags & RW_PROTOBUF_FOPT_INLINE)) {
                rw_minikey_basic_string_pointy_t *mk_p = 
                    (rw_minikey_basic_string_pointy_t *)mk; // A minimal support until we find out a better way!
                RW_ASSERT(mk_p->k.key);
                *((char **)(base + key_fd->offset)) = strdup(mk_p->k.key);
              } else {
                // Inline size greater than max-size??
                RW_ASSERT_NOT_REACHED();
              }
            }
            break;
          case PROTOBUF_C_TYPE_BYTES:
          default:
            RW_ASSERT_NOT_REACHED(); // Not implemented.
        }
      }
      break;
    default:
      RW_ASSERT_NOT_REACHED(); // Not implemented.
  }

  // Keys are required fields.. they will not have the has_ field.
  if (key_fd->quantifier_offset) {
    *((protobuf_c_boolean *)(base + key_fd->quantifier_offset)) = 1;
  }

  return true;
}

rw_status_t
rw_schema_mk_copy_to_path_entry(const rw_schema_minikey_opaque_t *mk,
                                rw_keyspec_entry_t *path_entry)
{
  const ProtobufCFieldDescriptor *fd = NULL;
  const ProtobufCFieldDescriptor *key_fd = NULL;
  const ProtobufCMessageDescriptor *mdesc = NULL;
  protobuf_c_boolean is_dptr = false;
  rw_status_t status = RW_STATUS_FAILURE;
  void *key_ptr = NULL;
  size_t off = 0;

  RW_ASSERT(mk);
  RW_ASSERT(path_entry);

  size_t n_keys = rw_schema_mk_num_keys(mk);
  RW_ASSERT(n_keys == 1); // Supporting only single key minikeys for now.

  size_t count = protobuf_c_message_get_field_desc_count_and_offset(&path_entry->base,
              RW_SCHEMA_TAG_ELEMENT_KEY_START, &fd, &key_ptr, &off, &is_dptr);

  RW_ASSERT(fd);
  RW_ASSERT(fd->type == PROTOBUF_C_TYPE_MESSAGE);

  if (count > 1) {
    // The path entry already has keys??. We are deleting the old key value.
    if (!protobuf_c_message_delete_field(nullptr, &path_entry->base, RW_SCHEMA_TAG_ELEMENT_KEY_START)) {
      return status;
    }
  }

  ProtobufCMessage *key_msg = NULL;
  protobuf_c_message_set_field_message(nullptr, &path_entry->base, fd, &key_msg);

  if (!key_msg) {
    return status;
  }

  mdesc = (const ProtobufCMessageDescriptor *)fd->descriptor;
  RW_ASSERT(mdesc);
  RW_ASSERT(mdesc->n_fields == 2);
  key_fd = &mdesc->fields[1];

  if (!copy_key_to_proto(mk, key_fd, key_msg)) {
    return status;
  }

  status = RW_STATUS_SUCCESS;
  return status;
}

rw_status_t
rw_schema_mk_copy_to_keyspec(rw_keyspec_path_t *ks,
                            rw_schema_minikey_opaque_t *mk,
                            size_t path_index)
{
  rw_status_t status;

  RW_ASSERT(ks);
  RW_ASSERT(mk);

  const rw_keyspec_entry_t *path_entry = NULL;
  path_entry = rw_keyspec_path_get_path_entry(ks, path_index);

  RW_ASSERT(path_entry);

  status = rw_schema_mk_copy_to_path_entry(mk, 
                const_cast<rw_keyspec_entry_t *>(path_entry));
  return status;
}

bool
rw_schema_mk_is_equal(const rw_schema_minikey_opaque_t *mk1,
                      const rw_schema_minikey_opaque_t *mk2)
{
  bool ret_val = false;

  RW_ASSERT(mk1);
  RW_ASSERT(mk2);

  // ATTN: Is this ok?
  ret_val = memcmp(mk1, mk2, RWPB_MINIKEY_OPAQ_LEN);
  return ret_val;
}

bool
rw_schema_mk_path_entry_is_equal(const rw_schema_minikey_opaque_t *mk,
                                 const rw_keyspec_entry_t *path_entry)
{
  rw_schema_minikey_opaque_t mkp;
  rw_status_t status;
  bool ret_val = false;

  RW_ASSERT(mk);
  RW_ASSERT(path_entry);

  status = rw_schema_mk_get_from_path_entry(path_entry, &mkp);
  if (status != RW_STATUS_SUCCESS) {
    return ret_val; 
  }

  ret_val = rw_schema_mk_is_equal(mk, &mkp);
  return ret_val;
}

bool
rw_schema_mk_message_is_equal(const rw_schema_minikey_opaque_t *mk,
                              const ProtobufCMessage *msg)
{
  rw_schema_minikey_opaque_t mkm;
  rw_status_t status;
  bool ret_val = false;

  RW_ASSERT(mk);
  RW_ASSERT(msg);

  status = rw_schema_mk_get_from_message(msg, &mkm);
  if (status != RW_STATUS_SUCCESS) {
    return ret_val; 
  }

  ret_val = rw_schema_mk_is_equal(mk, &mkm);
  return ret_val;
}

int
rw_schema_mk_get_print_buffer(const rw_schema_minikey_opaque_t *mk,
                              char *buff,
                              size_t buff_len)
{
  int ret_val = -1;

  RW_ASSERT(mk);
  const rw_minikey_desc_t *const *desc = reinterpret_cast<const rw_minikey_desc_t *const *>(mk);

  if (!*desc) {
    return ret_val;
  }

  if (!buff) {
    return ret_val;
  }

  switch((*desc)->key_type) {
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_FLOAT:
      {
        rw_minikey_basic_float_t *mk_f = 
            (rw_minikey_basic_float_t *)mk;
        ret_val = snprintf(buff, buff_len, "%f", mk_f->k.key);
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_DOUBLE:
      {
        rw_minikey_basic_double_t *mk_d =
            (rw_minikey_basic_double_t *)mk;
        ret_val = snprintf(buff, buff_len, "%lf", mk_d->k.key);
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_INT32:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SINT32:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SFIXED32:
      {
        rw_minikey_basic_int32_t *mk_i =
            (rw_minikey_basic_int32_t *)mk;
        ret_val = snprintf(buff, buff_len, "%d", mk_i->k.key);
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_UINT32:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_FIXED32:
      {
        rw_minikey_basic_uint32_t *mk_u =
            (rw_minikey_basic_uint32_t *)mk;
        ret_val = snprintf(buff, buff_len, "%u", mk_u->k.key);
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_INT64:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SINT64:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SFIXED64:
      {
        rw_minikey_basic_int64_t *mk_i64 =
            (rw_minikey_basic_int64_t *)mk;
        ret_val = snprintf(buff, buff_len, "%" SCNd64, mk_i64->k.key);
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_UINT64:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_FIXED64:
      {
        rw_minikey_basic_uint64_t *mk_u64=
            (rw_minikey_basic_uint64_t *)mk;
        ret_val = snprintf(buff, buff_len, "%" SCNu64, mk_u64->k.key);
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_BOOL:
      {
        rw_minikey_basic_bool_t *mk_b=
            (rw_minikey_basic_bool_t *)mk;
        if (mk_b->k.key) {
          ret_val = snprintf(buff, buff_len, "True");
        } else {
          ret_val = snprintf(buff, buff_len, "False");
        }
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_STRING:
      {
        rw_minikey_basic_string_t *mk_s=
            (rw_minikey_basic_string_t *)mk;
        ret_val = snprintf(buff, buff_len, "%s", mk_s->k.key);
      }
      break;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_BYTES:
    default:
      {
        // ATTN: Fix ME
        RW_ASSERT_NOT_REACHED();
      }
  }

  return ret_val;
}

rw_status_t
rw_schema_minikey_get_from_path_entry(const rw_keyspec_entry_t *path_entry,
                                      rw_schema_minikey_t **mk)
{

   rw_schema_minikey_opaque_t local_mk;
   rw_status_t rw_status = rw_schema_mk_get_from_path_entry(path_entry, &local_mk);

   if (rw_status != RW_STATUS_SUCCESS) {
     return rw_status;
   } 

   *mk = (rw_schema_minikey_t *)RW_MALLOC0(sizeof(rw_schema_minikey_t));

   if (!*mk) {
     return RW_STATUS_FAILURE;
   } 

   memcpy((char *)*mk, (char *)&local_mk, sizeof(rw_schema_minikey_t));
   
   return RW_STATUS_SUCCESS;
}

static rw_schema_minikey_t* rw_schema_minikey_gi_ref(
  rw_schema_minikey_t* minikey)
{
  rw_schema_minikey_t* dup_minikey = NULL;
  dup_minikey = (rw_schema_minikey_t *)RW_MALLOC0(sizeof(rw_schema_minikey_t));
  RW_ASSERT(dup_minikey);
  memcpy((char *)dup_minikey, (char *)minikey, sizeof(rw_schema_minikey_t));
  return dup_minikey;
}

static void rw_schema_minikey_gi_unref(
  rw_schema_minikey_t* minikey)
{
  RW_FREE(minikey);
  return;
}

G_DEFINE_BOXED_TYPE(
  rw_schema_minikey_t,
  rw_schema_minikey,
  rw_schema_minikey_gi_ref,
  rw_schema_minikey_gi_unref);

