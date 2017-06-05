/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rw_keyspec.cpp
 * @author Tom Seidenberg
 * @date 2014/08/29
 * @brief RiftWare general schema utilities
 */

#include <rw_keyspec.h>
#include <rw_schema.pb-c.h>
#include <rw_namespace.h>
#include "rw_ks_util.hpp"
#include <protobuf-c/rift/rw_pb_delta.h>

#include <execinfo.h>
#include <algorithm>
#include <stack>
#include <sstream>
#include <boost/format.hpp>

using namespace rw_yang;

#define IS_PATH_ENTRY_GENERIC(entry_) (((const ProtobufCMessage *)entry_)->descriptor == &rw_schema_path_entry__descriptor)
#define IS_KEYSPEC_GENERIC(ks_) (((const ProtobufCMessage *)ks_)->descriptor == &rw_schema_path_spec__descriptor)

#define KEYSPEC_ERROR(instance_, ...) \
    keyspec_error_message(instance_, __LINE__, ##__VA_ARGS__)

#define KEYSPEC_INC_ERROR_STATS(instance_, name_) \
    instance_->stats.error.name_++;  // Need atomic increment here? keyspec instance is inside dts api handle which is already thread safe??

#define KEYSPEC_INC_FCALL_STATS(instance_, type_, name_) \
    instance_->stats.fcall.type_.name_++;

static inline bool is_good_ks(const rw_keyspec_path_t* ks) {
  return    ks
         && ks->base.descriptor
         && PROTOBUF_C_MESSAGE_DESCRIPTOR_MAGIC == ks->base.descriptor->magic;
}

static inline bool is_good_pe(const rw_keyspec_entry_t* pe) {
  return    pe
         && pe->base.descriptor
         && PROTOBUF_C_MESSAGE_DESCRIPTOR_MAGIC == pe->base.descriptor->magic;
}

static const char*
get_yang_name_from_pb_name(const ProtobufCMessageDescriptor *mdesc,
                           const char* pb_fname);

// Global default keyspec instance.
rw_keyspec_instance_t keyspec_default_instance =
{
  .pbc_instance = NULL,
  .ymodel = NULL,
  .ylib_data = NULL,
  .user_data = NULL,
  .error = keyspec_default_error_cb,
};

typedef enum {
  REROOT_ITER_INIT = 0,
  REROOT_ITER_FIRST,
  REROOT_ITER_NEXT,
  REROOT_ITER_DONE
} iter_state_t;

typedef enum {
  PE_CONTAINER,
  PE_LISTY_WITH_ALL_KEYS,
  PE_LISTY_WITH_ALL_WC,
  PE_LISTY_WITH_WC_AND_KEYS
} listy_pe_type_t;

typedef struct {
  iter_state_t next_state;
  KeySpecHelper *ks_in;
  KeySpecHelper *ks_out;
  rw_keyspec_instance_t* instance;
  const ProtobufCMessage *in_msg;
  size_t depth_i;
  size_t depth_o;
  rw_keyspec_path_t *cur_ks;
  const ProtobufCMessage *cur_msg;
  uint32_t n_path_ens;
  protobuf_c_boolean is_out_ks_wc;
  protobuf_c_boolean ks_has_iter_pes;
  struct {
    uint32_t count;
    uint32_t next_index;
    protobuf_c_boolean is_listy;
    protobuf_c_boolean is_iteratable;
    listy_pe_type_t type;
    protobuf_c_boolean is_last_iter_index;
    protobuf_c_boolean is_first_iter_index;
  } path_entries[RW_SCHEMA_PB_MAX_PATH_ENTRIES];

} keyspec_reroot_iter_state_t;


static_assert((sizeof(keyspec_reroot_iter_state_t) == RW_SCHEMA_REROOT_STATE_SIZE),
              "keyspec_reroot_iter_state_t structure modified. Update RW_SCHEMA_REROOT_STATE_SIZE");

// keyspec instance related APIs.
rw_keyspec_instance_t* rw_keyspec_instance_default()
{
  // Set to the default.
  keyspec_default_instance.pbc_instance = NULL;
  keyspec_default_instance.ymodel = NULL;
  keyspec_default_instance.ylib_data = NULL;
  keyspec_default_instance.user_data = NULL;
  keyspec_default_instance.error = keyspec_default_error_cb;

  return &keyspec_default_instance;
}

ProtobufCInstance* rw_keyspec_instance_set_pbc_instance(
    rw_keyspec_instance_t* ks_inst,
    ProtobufCInstance* pbc_inst)
{
  ProtobufCInstance* old_inst = ks_inst->pbc_instance;
  ks_inst->pbc_instance = pbc_inst;
  return old_inst;
}

rw_yang_model_t* rw_keyspec_instance_set_yang_model(
    rw_keyspec_instance_t* ks_inst,
    rw_yang_model_t* ymodel)
{
  rw_yang_model_t* old_ymodel = ks_inst->ymodel;
  ks_inst->ymodel = ymodel;
  return old_ymodel;
}

void* rw_keyspec_instance_set_user_data(
    rw_keyspec_instance_t* ks_inst,
    void* user_data)
{
  void* old_ud = ks_inst->user_data;
  ks_inst->user_data = user_data;
  return old_ud;
}

void* rw_keyspec_instance_get_user_data(
    rw_keyspec_instance_t* ks_inst)
{
  return ks_inst->user_data;
}

static inline rw_keyspec_instance_t* ks_instance_get(
    rw_keyspec_instance_t* ks_inst)
{
  if (ks_inst) {
    return ks_inst;
  }

  return &keyspec_default_instance;
}

static inline void extract_args(
    std::ostringstream& oss,
    boost::format& msg)
{
  oss << msg;
}

template<typename TValue, typename... TArgs>
static inline void extract_args(
    std::ostringstream& oss,
    boost::format& msg,
    TValue arg,
    TArgs... args)
{
  msg % arg;
  extract_args(oss, msg, args...);
}

template<typename... TArgs>
static inline void extract_args(
    std::ostringstream& oss,
    const char* format,
    TArgs... args)
{
  boost::format fstr(format);
  extract_args(oss, fstr, args...);
}

template<typename... TArgs>
static inline void extract_args(
    std::ostringstream& oss,
    const rw_keyspec_path_t* ks,
    TArgs... args)
{
  auto ypbc_mdesc = ks->base.descriptor->ypbc_mdesc;
  if (ypbc_mdesc) {
    oss << ypbc_mdesc->module->module_name
        << ',' << ypbc_mdesc->pbc_mdesc->short_name;
  } else {
    // Generic keyspec. Print the nsid and tag??
  }
  oss << "(" << ks->base.descriptor << ")";
  extract_args(oss, args...);
}

#define KEYSPEC_ERROR_NUM_BT_ELEMENTS 5

template<typename... TArgs>
static inline void keyspec_error_message(
    rw_keyspec_instance_t* instance,
    int line_no,
    TArgs... args)
{
  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_ERROR_STATS(instance, total_errors);

  if (!instance->error) {
    return;
  }

  void *array[KEYSPEC_ERROR_NUM_BT_ELEMENTS];
  size_t size;
  char **strings;
  size = backtrace (array, 5);
  strings = backtrace_symbols (array, size);

  std::ostringstream oss;
  oss << "Line:" << line_no << ' ';

  size_t i;
  // Skip the first element as it will be this function
  for (i = 1; i < size; i++) {
    oss << i << ':' << strings[i] << ' ';
  }
  free (strings);

  extract_args(oss, args...);

  instance->error(instance, oss.str().c_str());
}

static ProtobufCMessage* dup_and_convert_to_type(
    rw_keyspec_instance_t* instance,
    const ProtobufCMessage* in_msg,
    const ProtobufCMessageDescriptor* out_pbcmd)
{
  RW_ASSERT(in_msg);
  RW_ASSERT(out_pbcmd);
  RW_ASSERT(PROTOBUF_C_MESSAGE_DESCRIPTOR_MAGIC == out_pbcmd->magic);
  RW_ASSERT(instance);

  // Encode the old.  Hope that it fits on the stack.
  uint8_t stackbuf[4096];
  uint8_t* buf = nullptr;
  size_t size = protobuf_c_message_get_packed_size(instance->pbc_instance, in_msg);
  if (size <= sizeof(stackbuf)) {
    buf = stackbuf;
  } else {
    buf = (uint8_t*)malloc(size);
    RW_ASSERT(buf);
  }
  size_t actual_size = protobuf_c_message_pack(instance->pbc_instance, in_msg, buf);
  RW_ASSERT(actual_size == size);

  // Allocate new and decode into it.
  ProtobufCMessage* out_msg = protobuf_c_message_unpack(
      instance->pbc_instance, out_pbcmd, size, buf);
  // ATTN: Want this?: RW_ASSERT(out_msg);

  if (buf != stackbuf) {
    free(buf);
  }

  return out_msg;
}

static bool dup_and_convert_in_place(
    rw_keyspec_instance_t* instance,
    const ProtobufCMessage* in_msg,
    ProtobufCMessage* out_msg)
{
  RW_ASSERT(in_msg);
  RW_ASSERT(out_msg);
  RW_ASSERT(out_msg->descriptor);
  RW_ASSERT(PROTOBUF_C_MESSAGE_DESCRIPTOR_MAGIC == out_msg->descriptor->magic);
  RW_ASSERT(instance);

  // Encode the old.  Hope that it fits on the stack.
  uint8_t stackbuf[4096];
  uint8_t* buf = nullptr;
  size_t size = protobuf_c_message_get_packed_size(instance->pbc_instance, in_msg);
  if (size <= sizeof(stackbuf)) {
    buf = stackbuf;
  } else {
    buf = (uint8_t*)malloc(size);
    RW_ASSERT(buf);
  }
  size_t actual_size = protobuf_c_message_pack(instance->pbc_instance, in_msg, buf);
  RW_ASSERT(actual_size == size);

  // Decode into the new.
  ProtobufCMessage* dup_out = protobuf_c_message_unpack_usebody(
    instance->pbc_instance, out_msg->descriptor, size, buf, out_msg);

  if (buf != stackbuf) {
    free(buf);
  }

  return (dup_out == out_msg);
}


rw_status_t rw_keyspec_path_create_dup(
  const rw_keyspec_path_t* input,
  rw_keyspec_instance_t* instance,
  rw_keyspec_path_t** output)
{
  RW_ASSERT(is_good_ks(input));
  RW_ASSERT(output);
  RW_ASSERT(!*output);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, create_dup);

  // Dup to the same type
  ProtobufCMessage* out_msg = dup_and_convert_to_type(
      instance, &input->base, input->base.descriptor);

  if (out_msg) {
    *output = reinterpret_cast<rw_keyspec_path_t*>(out_msg);
    return RW_STATUS_SUCCESS;
  }

  KEYSPEC_ERROR(instance, input, "Create dup failed(%p)", input->base.descriptor);
  return RW_STATUS_FAILURE;
}

rw_status_t rw_keyspec_path_create_dup_category(
  const rw_keyspec_path_t* input,
  rw_keyspec_instance_t* instance,
  rw_keyspec_path_t** output,
  RwSchemaCategory category)
{
  RW_ASSERT(is_good_ks(input));
  RW_ASSERT(output);
  RW_ASSERT(!*output);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  // Dup to the same type
  ProtobufCMessage* out_msg = dup_and_convert_to_type(
      instance, &input->base, input->base.descriptor);

  if (out_msg) {
    *output = reinterpret_cast<rw_keyspec_path_t*>(out_msg);
    rw_keyspec_path_set_category(*output, instance, category);
    return RW_STATUS_SUCCESS;
  }

  KEYSPEC_ERROR(instance, input, "Create dup failed(%p)", input->base.descriptor);
  return RW_STATUS_FAILURE;
}

rw_keyspec_path_t* rw_keyspec_path_create_dup_of_type(
  const rw_keyspec_path_t* input,
  rw_keyspec_instance_t* instance,
  const ProtobufCMessageDescriptor* out_pbcmd)
{
  RW_ASSERT(is_good_ks(input));
  RW_ASSERT(out_pbcmd);
  RW_ASSERT(PROTOBUF_C_MESSAGE_DESCRIPTOR_MAGIC == out_pbcmd->magic);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, create_dup_type);

  // Dup to the other type
  ProtobufCMessage* out_msg = dup_and_convert_to_type(
      instance, &input->base, out_pbcmd);

  if (out_msg) {
    return reinterpret_cast<rw_keyspec_path_t*>(out_msg);
  }

  KEYSPEC_ERROR(instance, input, "Create dup failed(%p)", out_pbcmd);
  return nullptr;
}

rw_keyspec_path_t* rw_keyspec_path_create_dup_of_type_trunc(
  const rw_keyspec_path_t* input,
  rw_keyspec_instance_t* instance,
  const ProtobufCMessageDescriptor* out_pbcmd)
{
  RW_ASSERT(is_good_ks(input));
  RW_ASSERT(out_pbcmd);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, create_dup_type_trunc);

  // Dup to the other type
  rw_keyspec_path_t* out_ks = rw_keyspec_path_create_dup_of_type(
      input, instance, out_pbcmd);

  if (out_ks) {
    rw_status_t status = rw_keyspec_path_trunc_concrete(out_ks, instance);
    RW_ASSERT(RW_STATUS_SUCCESS == status);
    return out_ks;
  }

  KEYSPEC_ERROR(instance, input, "Create dup&trunc failed(%p)", out_pbcmd);
  return nullptr;
}

rw_status_t rw_keyspec_path_find_spec_ks(
    const rw_keyspec_path_t* ks,
    rw_keyspec_instance_t* instance,
    const rw_yang_pb_schema_t* schema,
    rw_keyspec_path_t** spec_ks)
{
  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, find_spec_ks);

  const rw_yang_pb_msgdesc_t* curr_ypbc_msgdesc = nullptr;

  rw_status_t rs = rw_keyspec_path_find_msg_desc_schema (
      ks, instance, schema, &curr_ypbc_msgdesc, nullptr);

  if (RW_STATUS_SUCCESS != rs) {
    return rs;
  }

  RW_ASSERT(curr_ypbc_msgdesc);
  RW_ASSERT(curr_ypbc_msgdesc->pbc_mdesc);

  *spec_ks = rw_keyspec_path_create_dup_of_type(
      ks, instance, curr_ypbc_msgdesc->pbc_mdesc);

  return RW_STATUS_SUCCESS;
}

rw_status_t rw_keyspec_path_copy(
  const rw_keyspec_path_t* input,
  rw_keyspec_instance_t* instance,
  rw_keyspec_path_t* output)
{
  RW_ASSERT(is_good_ks(input));
  RW_ASSERT(is_good_ks(output));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  // Dup, possibly to a different type.
  bool okay = dup_and_convert_in_place(instance, &input->base, &output->base);
  if (okay) {
    return RW_STATUS_SUCCESS;
  }

  KEYSPEC_ERROR(instance, input, "Copy ks failed(%p)", &input->base.descriptor);
  return RW_STATUS_FAILURE;
}

rw_status_t rw_keyspec_path_move(
  rw_keyspec_instance_t* instance,
  rw_keyspec_path_t* input,
  rw_keyspec_path_t* output)
{
  RW_ASSERT(is_good_ks(input));
  RW_ASSERT(is_good_ks(output));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  // Dup, possibly to a different type.
  bool okay = dup_and_convert_in_place(instance, &input->base, &output->base);
  if (okay) {
    return RW_STATUS_SUCCESS;
  }

  KEYSPEC_ERROR(instance, input, "Move ks failed(%p)", &input->base.descriptor);
  return RW_STATUS_FAILURE;
}

rw_status_t rw_keyspec_path_swap(
  rw_keyspec_instance_t* instance,
  rw_keyspec_path_t* a,
  rw_keyspec_path_t* b)
{
  RW_ASSERT(is_good_ks(a));
  RW_ASSERT(is_good_ks(b));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  // Encode the a.  Hope that it fits on the stack.
  uint8_t stackbufa[4096];
  uint8_t* bufa = nullptr;
  size_t sizea = protobuf_c_message_get_packed_size(
      instance->pbc_instance, &a->base);
  if (sizea <= sizeof(stackbufa)) {
    bufa = stackbufa;
  } else {
    bufa = (uint8_t*)malloc(sizea);
    RW_ASSERT(bufa);
  }
  size_t actual_sizea = protobuf_c_message_pack(
      instance->pbc_instance, &a->base, bufa);
  RW_ASSERT(actual_sizea == sizea);

  // Encode the old.  Hope that it fits on the stack.
  uint8_t stackbufb[4096];
  uint8_t* bufb = nullptr;
  size_t sizeb = protobuf_c_message_get_packed_size(
      instance->pbc_instance, &b->base);
  if (sizeb <= sizeof(stackbufb)) {
    bufb = stackbufb;
  } else {
    bufb = (uint8_t*)malloc(sizeb);
    RW_ASSERT(bufb);
  }
  size_t actual_sizeb = protobuf_c_message_pack(
      instance->pbc_instance, &b->base, bufb);
  RW_ASSERT(actual_sizeb == sizeb);

  // Free current contents (just in case they are bumpy).
  const ProtobufCMessageDescriptor* pbcmda = a->base.descriptor;
  const ProtobufCMessageDescriptor* pbcmdb = b->base.descriptor;
  protobuf_c_message_free_unpacked_usebody(instance->pbc_instance, &a->base);
  protobuf_c_message_free_unpacked_usebody(instance->pbc_instance, &b->base);

  // Ensure that protobuf-c won't crash on unpack...
  protobuf_c_message_init(pbcmda, &a->base);
  protobuf_c_message_init(pbcmdb, &b->base);

  // Decode a into b.
  ProtobufCMessage* b_dup = protobuf_c_message_unpack_usebody(
    instance->pbc_instance, b->base.descriptor, sizea, bufa, &b->base);

  // Decode b into a.
  ProtobufCMessage* a_dup = protobuf_c_message_unpack_usebody(
    instance->pbc_instance, a->base.descriptor, sizeb, bufb, &a->base);

  if (bufa != stackbufa) {
    free(bufa);
  }
  if (bufb != stackbufb) {
    free(bufb);
  }

  if (a_dup == &a->base && b_dup == &b->base) {
    return RW_STATUS_SUCCESS;
  }

  protobuf_c_message_free_unpacked_usebody(instance->pbc_instance, &a->base);
  protobuf_c_message_free_unpacked_usebody(instance->pbc_instance, &b->base);

  KEYSPEC_ERROR(instance, "KS swap failed(%p)(%p)", pbcmda, pbcmdb);

  return RW_STATUS_FAILURE;
}

rw_status_t rw_keyspec_path_update_binpath(
  rw_keyspec_path_t* k,
  rw_keyspec_instance_t* instance)
{
  rw_status_t status = RW_STATUS_FAILURE;
  RW_ASSERT(is_good_ks(k));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KeySpecHelper ks(k);

  if (!ks.has_dompath()) {
    KEYSPEC_ERROR(instance, k, "Update binpath, domapth not set");
    return status;
  }

  if (ks.has_binpath()) {
    if (rw_keyspec_path_delete_binpath(k, instance) != RW_STATUS_SUCCESS) {
      return status;
    }
  }

  RW_ASSERT(!ks.has_binpath());

  const ProtobufCMessage *dom_path = ks.get_dompath();
  RW_ASSERT(dom_path);

  std::unique_ptr<uint8_t> uptr;
  uint8_t stack_buff[2048], *buff = nullptr;
  size_t len = protobuf_c_message_get_packed_size(
      instance->pbc_instance, dom_path);

  if (len > 2048) {
    buff = (uint8_t *)malloc(len);
    RW_ASSERT(buff);
    uptr.reset(buff);
  } else {
    buff = &stack_buff[0];
  }

  if ((protobuf_c_message_pack(instance->pbc_instance, dom_path, buff)) != len) {
    return status;
  }

  auto bin_fd = protobuf_c_message_descriptor_get_field(
      k->base.descriptor, RW_SCHEMA_TAG_KEYSPEC_BINPATH);
  RW_ASSERT(bin_fd);

  if (!protobuf_c_message_set_field_text_value(
          instance->pbc_instance, &k->base, bin_fd, (char *)buff, len)) {
    KEYSPEC_ERROR(instance, k, "Failed to set binpath in ks");
    return status;
  }

  return RW_STATUS_SUCCESS;
}

rw_status_t rw_keyspec_path_delete_binpath(
    rw_keyspec_path_t* k,
    rw_keyspec_instance_t* instance)
{
  RW_ASSERT(is_good_ks(k));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  protobuf_c_boolean ok = protobuf_c_message_delete_field(
      instance->pbc_instance, &k->base, RW_SCHEMA_TAG_KEYSPEC_BINPATH);

  return ok?RW_STATUS_SUCCESS:RW_STATUS_FAILURE;
}

rw_status_t rw_keyspec_path_update_dompath(
  rw_keyspec_path_t* k,
  rw_keyspec_instance_t* instance)
{
  rw_status_t status = RW_STATUS_FAILURE;
  RW_ASSERT(is_good_ks(k));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KeySpecHelper ks(k);

  if (!ks.has_binpath()) {
    KEYSPEC_ERROR(instance, k, "Update dompath, binpath not set");
    return status;
  }

  if (ks.has_dompath()) {
    if (rw_keyspec_path_delete_dompath(k, instance) != RW_STATUS_SUCCESS) {
      return status;
    }
  }

  const uint8_t *bd = nullptr;
  size_t len = ks.get_binpath(&bd);

  RW_ASSERT(bd);

  ProtobufCMessage *dom_path = nullptr;
  auto fd = protobuf_c_message_descriptor_get_field(
      k->base.descriptor, RW_SCHEMA_TAG_KEYSPEC_DOMPATH);

  protobuf_c_message_set_field_message(
      instance->pbc_instance, &k->base, fd, &dom_path);
  RW_ASSERT(dom_path);

  if (protobuf_c_message_unpack_usebody(
          instance->pbc_instance, dom_path->descriptor, len, bd, dom_path) != dom_path) {
    KEYSPEC_ERROR(instance, k, "Update dompath, failed to unpack");
    rw_keyspec_path_delete_dompath(k, instance);
    return status;
  }

  return RW_STATUS_SUCCESS;
}

rw_status_t rw_keyspec_path_delete_dompath(
    rw_keyspec_path_t *k,
    rw_keyspec_instance_t* instance)
{
  RW_ASSERT(is_good_ks(k));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  protobuf_c_boolean ok = protobuf_c_message_delete_field(
      instance->pbc_instance, &k->base, RW_SCHEMA_TAG_KEYSPEC_DOMPATH);

  return ok?RW_STATUS_SUCCESS:RW_STATUS_FAILURE;
}

rw_status_t rw_keyspec_path_delete_strpath(
    rw_keyspec_path_t *k,
    rw_keyspec_instance_t* instance)
{
  RW_ASSERT(is_good_ks(k));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  protobuf_c_boolean ok = protobuf_c_message_delete_field(
      instance->pbc_instance, &k->base, RW_SCHEMA_TAG_KEYSPEC_STRPATH);

  return ok?RW_STATUS_SUCCESS:RW_STATUS_FAILURE;
}

rw_status_t rw_keyspec_path_get_binpath(
  rw_keyspec_path_t* k,
  rw_keyspec_instance_t* instance,
  const uint8_t** buf,
  size_t* len)
{
  rw_status_t status = RW_STATUS_FAILURE;

  RW_ASSERT(is_good_ks(k));
  RW_ASSERT(buf);
  RW_ASSERT(len);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KeySpecHelper ks(k);

  if (!ks.has_binpath()) {
    if (rw_keyspec_path_update_binpath(k, instance) != RW_STATUS_SUCCESS) {
      return status;
    }
  }

  RW_ASSERT(ks.has_binpath());

  *len = ks.get_binpath(buf);
  RW_ASSERT(*buf);

  return RW_STATUS_SUCCESS;
}

#define IS_SAME_ELEMENT(e1, e2) \
    ((e1->system_ns_id == e2->system_ns_id) && \
    (e1->element_tag == e2->element_tag) && \
    (e1->element_type == e2->element_type))


rw_schema_ns_and_tag_t rw_keyspec_entry_get_schema_id (
    const rw_keyspec_entry_t *path_entry) {

  auto elem_id =
      rw_keyspec_entry_get_element_id (path_entry);
  rw_schema_ns_and_tag_t schema_id;

  schema_id.ns = elem_id->system_ns_id;
  schema_id.tag = elem_id->element_tag;

  return schema_id;
}

const RwSchemaElementId* rw_keyspec_entry_get_element_id(
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
  RW_ASSERT(fd->label != PROTOBUF_C_LABEL_REPEATED);

  return (RwSchemaElementId *)msg_ptr;
}

size_t rw_keyspec_path_get_depth(
    const rw_keyspec_path_t* k)
{
  RW_ASSERT(is_good_ks(k));

  return KeySpecHelper(k).get_depth();
}

RwSchemaCategory rw_keyspec_path_get_category(
  const rw_keyspec_path_t* k)
{
  RW_ASSERT(is_good_ks(k));

  return KeySpecHelper(k).get_category();
}

void rw_keyspec_path_set_category(
  rw_keyspec_path_t* k,
  rw_keyspec_instance_t* instance,
  RwSchemaCategory category)
{
  RW_ASSERT(is_good_ks(k));
  RW_ASSERT((category >= RW_SCHEMA_CATEGORY_ANY) &&
            (category <= RW_SCHEMA_CATEGORY_NOTIFICATION));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, set_category);

  KeySpecHelper ks(k);

  if (!ks.has_dompath()) {
    rw_status_t s = rw_keyspec_path_update_dompath(k, instance);
    RW_ASSERT(s == RW_STATUS_SUCCESS);
  }

  RW_ASSERT(ks.has_dompath());
  ProtobufCMessage *dom_ptr = (ProtobufCMessage *)(ks.get_dompath());

  const ProtobufCFieldDescriptor *fd = protobuf_c_message_descriptor_get_field(
      dom_ptr->descriptor, RW_SCHEMA_TAG_KEYSPEC_CATEGORY);

  RW_ASSERT(fd->type == PROTOBUF_C_TYPE_ENUM);
  RW_ASSERT(fd->label == PROTOBUF_C_LABEL_OPTIONAL);
  RW_ASSERT(&rw_schema_category__descriptor == fd->descriptor);

  auto field_ptr = (RwSchemaCategory *)((uint8_t *)dom_ptr + fd->offset);
  *field_ptr = category;

  RW_ASSERT(fd->quantifier_offset);
  auto qf = (protobuf_c_boolean *)((uint8_t *)dom_ptr + fd->quantifier_offset);
  *qf = TRUE;

  rw_keyspec_path_delete_binpath(k, instance);
  rw_keyspec_path_delete_strpath(k, instance);
}

const rw_keyspec_entry_t* rw_keyspec_path_get_path_entry(
  const rw_keyspec_path_t* k,
  size_t entry_index)
{
  RW_ASSERT(is_good_ks(k));

  KeySpecHelper ks(k);

  // To return the path_entry, dompath must be present in the keyspec.
  if (!ks.has_dompath()) {
    return nullptr;
  }

  return ks.get_path_entry(entry_index);
}

size_t rw_keyspec_entry_num_keys(
    const rw_keyspec_entry_t *path_entry)
{
  const RwSchemaElementId *element = rw_keyspec_entry_get_element_id(path_entry);
  switch (element->element_type) {
    case RW_SCHEMA_ELEMENT_TYPE_ROOT:
    case RW_SCHEMA_ELEMENT_TYPE_MODULE_ROOT:
    case RW_SCHEMA_ELEMENT_TYPE_CONTAINER:
    case RW_SCHEMA_ELEMENT_TYPE_LEAF:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_0:
      return 0;
    case RW_SCHEMA_ELEMENT_TYPE_LEAF_LIST:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_FLOAT:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_DOUBLE:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_INT32:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SFIXED32:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SINT32:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_UINT32:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_FIXED32:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_INT64:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SINT64:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_SFIXED64:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_UINT64:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_FIXED64:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_BOOL:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_BYTES:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_1_STRING:
      return 1;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_2:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_3:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_4:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_5:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_6:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_7:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_8:
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_9:
      return 2 + element->element_type - RW_SCHEMA_ELEMENT_TYPE_LISTY_2;
    case _RW_SCHEMA_ELEMENT_TYPE_IS_INT_SIZE:
      break;
  }
  // Don't assert here - element-=id could be from the wire.
  return 0;
}

static int find_mis_matching_key_value_index(
    rw_keyspec_instance_t* instance,
    const rw_keyspec_entry_t *path_entry1,
    const rw_keyspec_entry_t *path_entry2,
    bool is_wild_card_match)
{
  void *key1 = nullptr;
  void *key2 = nullptr;

  size_t count1 = 0, offset = 0, count2 = 0;
  size_t len1 = 0, len2 = 0;

  ProtobufCMessage *mkey1 = nullptr, *mkey2 = nullptr;
  protobuf_c_boolean is_dptr = false;
  const ProtobufCFieldDescriptor *fd1 = nullptr, *fd2 = nullptr;
  uint8_t stackbuf1[1024];
  uint8_t stackbuf2[1024];

  size_t n_keys = rw_keyspec_entry_num_keys(path_entry1);
  // Number of keys is determined from the element type. Hence
  // it should be same in both the path entries.

  for (unsigned i = 0; i < n_keys; i++) {

    count1 = protobuf_c_message_get_field_desc_count_and_offset(&path_entry1->base,
               RW_SCHEMA_TAG_ELEMENT_KEY_START+i, &fd1, &key1, &offset, &is_dptr);

    if (count1) {
      RW_ASSERT(fd1->type == PROTOBUF_C_TYPE_MESSAGE);
      RW_ASSERT(fd1->label != PROTOBUF_C_LABEL_REPEATED);

      mkey1 = (ProtobufCMessage *)key1;
      size_t size = protobuf_c_message_get_packed_size(
          instance->pbc_instance, mkey1);

      RW_ASSERT(size <= sizeof(stackbuf1));
      len1 = protobuf_c_message_pack(instance->pbc_instance, mkey1, stackbuf1);
      RW_ASSERT(len1 == size);
      key1 = stackbuf1;
    }

    count2 = protobuf_c_message_get_field_desc_count_and_offset(&path_entry2->base,
              RW_SCHEMA_TAG_ELEMENT_KEY_START+i, &fd2, &key2, &offset, &is_dptr);

    if (count2) {
      RW_ASSERT(fd2->type == PROTOBUF_C_TYPE_MESSAGE);
      RW_ASSERT(fd2->label != PROTOBUF_C_LABEL_REPEATED);

      mkey2 = (ProtobufCMessage *)key2;
      size_t size = protobuf_c_message_get_packed_size(
          instance->pbc_instance, mkey2);

      RW_ASSERT(size <= sizeof(stackbuf2));
      len2 = protobuf_c_message_pack(instance->pbc_instance, mkey2, stackbuf2);
      RW_ASSERT(len2 == size);
      key2 = stackbuf2;
    }

    if (count1 && count2) {
      if (len1 != len2) {
        return i;
      }

      if (memcmp(key1, key2, len1)) {
        return i;
      }
    } else {
      if (!is_wild_card_match) {
        return i;
      }
    }
  }

  return -1;
}

static bool compare_path_entries(
    rw_keyspec_instance_t* instance,
    const rw_keyspec_entry_t *path_entry1,
    const rw_keyspec_entry_t *path_entry2,
    int* key_index,
    bool wc_match)
{
  if (key_index) {
    *key_index = -1;
  }

  if (protobuf_c_message_is_equal_deep(instance->pbc_instance,
                                       &path_entry1->base,
                                       &path_entry2->base)) {
    return true;
  }

  auto elemid1 = rw_keyspec_entry_get_element_id(path_entry1);
  RW_ASSERT(elemid1);

  auto elemid2 = rw_keyspec_entry_get_element_id(path_entry2);
  RW_ASSERT(elemid2);

  // Compare element ids.
  if (IS_SAME_ELEMENT(elemid1, elemid2)) {
    if (key_index) {
      *key_index = find_mis_matching_key_value_index(
          instance, path_entry1, path_entry2, wc_match);
    }
    return true;
  }

  return false;
}

bool rw_keyspec_path_is_entry_at_tip(
  const rw_keyspec_path_t* k,
  rw_keyspec_instance_t* instance,
  const rw_keyspec_entry_t* entry,
  size_t* entry_index,
  int* key_index)
{
  bool retval = false;

  RW_ASSERT(is_good_ks(k));
  RW_ASSERT(is_good_pe(entry));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, has_entry);

  KeySpecHelper ks(k);
  size_t depth = ks.get_depth();

  if (!depth) {
    return retval;
  }

  if (key_index) {
    *key_index = -1;
  }

  std::stack <uint32_t> pe_path;
  auto pe_ypbc = entry->base.descriptor->ypbc_mdesc;
  RW_ASSERT(pe_ypbc);

  while (pe_ypbc) {
    pe_path.push(pe_ypbc->pb_element_tag);
    pe_ypbc = pe_ypbc->parent;
  }

  for (unsigned i = 0; i < depth; i++) {

    auto path_entry = ks.get_path_entry(i);
    RW_ASSERT(path_entry);

    auto id = pe_path.top();
    pe_ypbc = path_entry->base.descriptor->ypbc_mdesc;

    if ((nullptr == pe_ypbc) || (pe_ypbc->pb_element_tag != id)) {
      return false;
    }

    pe_path.pop();
    if (!pe_path.empty()) {
      continue;
    }

    if (i != depth - 1) {
      // lengths differ
      return false;
    }
    int ki = -1;
    retval = compare_path_entries(instance, path_entry, entry, &ki, false);

    if (retval) {
      if (entry_index) {
        *entry_index = i;
      }
      if (key_index) {
        *key_index = ki;
      }
      break;
    }
  }

  return retval;
}

bool rw_keyspec_path_has_element(
  const rw_keyspec_path_t* k,
  rw_keyspec_instance_t* instance,
  const RwSchemaElementId* element,
  size_t* element_index)
{
  bool retval = false;

  RW_ASSERT(is_good_ks(k));
  RW_ASSERT(element);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KeySpecHelper ks(k);
  size_t depth = ks.get_depth();

  for (unsigned i = 0; i < depth; i++) {

    auto path_entry = ks.get_path_entry(i);
    RW_ASSERT(path_entry);

    auto elem_id = rw_keyspec_entry_get_element_id(path_entry);
    RW_ASSERT(elem_id);

    if (IS_SAME_ELEMENT(element, elem_id)) {
      *element_index = i;
      retval = true;
      break;
    }
  }

  return retval;
}

bool rw_keyspec_entry_has_key (
    const rw_keyspec_entry_t* entry,
    int key_index)
{
  if (!entry) {
    return false;
  }
  const ProtobufCFieldDescriptor *fd = nullptr;
  void *msg_ptr = nullptr;
  size_t offset = 0;
  protobuf_c_boolean is_dptr = false;

  size_t count = protobuf_c_message_get_field_desc_count_and_offset (
      &entry->base, RW_SCHEMA_TAG_ELEMENT_KEY_START + key_index, &fd,
      &msg_ptr, &offset, &is_dptr);

  RW_ASSERT(count < 2);
  return (0 != count);
}

rw_status_t rw_keyspec_entry_get_key_value(const rw_keyspec_entry_t* entry,
                                           int key_index,
                                           ProtobufCFieldInfo *value)
{
  if (entry->base.descriptor ==
      (const ProtobufCMessageDescriptor *)&rw_schema_path_entry__concrete_descriptor) {
    return RW_STATUS_FAILURE;
  }

  if (!rw_keyspec_entry_has_key (entry, key_index)) {
    return RW_STATUS_FAILURE;
  }

  memset (value, 0, sizeof (ProtobufCFieldInfo));
  const ProtobufCMessage *msg = (const ProtobufCMessage *)entry;

  // The keyXXX message
  const ProtobufCFieldDescriptor *fld = protobuf_c_message_descriptor_get_field (
      msg->descriptor,RW_SCHEMA_TAG_ELEMENT_KEY_START + key_index);
  RW_ASSERT(nullptr != fld);

  ProtobufCMessage *key =
      STRUCT_MEMBER_PTR (ProtobufCMessage, msg, fld->offset);

  const ProtobufCMessageDescriptor *desc = key->descriptor;

  size_t i = 0;
  while (i < desc->n_fields) {
    // the named key within the field
    if (PROTOBUF_C_LABEL_REQUIRED == desc->fields[i].label) {
      break;
    }
    ++i;
  }
  if (i >= desc->n_fields) {
    return RW_STATUS_FAILURE;
  }

  auto found_key = protobuf_c_message_get_field_instance(
      nullptr, key, &desc->fields[i], 0, value);
  RW_ASSERT(found_key);

  return RW_STATUS_SUCCESS;
}

ProtobufCMessage*
rw_keyspec_entry_get_key(const rw_keyspec_entry_t *path_entry,
                         int index)
{
  const ProtobufCFieldDescriptor *fd = NULL;
  protobuf_c_boolean is_dptr = false;
  void *key_msg = NULL;
  size_t off = 0;

  RW_ASSERT(path_entry);

  size_t count = protobuf_c_message_get_field_desc_count_and_offset(
      &path_entry->base, RW_SCHEMA_TAG_ELEMENT_KEY_START+index, &fd, &key_msg, &off, &is_dptr);

  if (count) {
    RW_ASSERT(key_msg);
    return (ProtobufCMessage *)key_msg;
  }

  return NULL;
}

uint8_t*
rw_keyspec_entry_get_key_packed(const rw_keyspec_entry_t* entry,
                                rw_keyspec_instance_t* instance,
                                int key_index,
                                size_t* len)
{
  RW_ASSERT(is_good_pe(entry));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  ProtobufCMessage* key = rw_keyspec_entry_get_key(entry, key_index);
  if (!key) {
    return nullptr;
  }

  return protobuf_c_message_serialize(instance->pbc_instance, key, len);
}

uint8_t*
rw_keyspec_entry_get_keys_packed(const rw_keyspec_entry_t* entry,
                                rw_keyspec_instance_t* instance,
                                size_t* len)
{
  RW_ASSERT(is_good_pe(entry));
  instance = ks_instance_get(instance);
  RW_ASSERT(instance);
  uint8_t *packed_buffer = NULL;

  *len = 0;
  size_t num_keys = rw_keyspec_entry_num_keys(entry);
  for (size_t key_index = 0; key_index< num_keys; key_index++) {
    ProtobufCMessage* key = rw_keyspec_entry_get_key(entry, key_index);
    if (!key) {
      // free the current buffer if any also
      return nullptr;
    }
    packed_buffer = protobuf_c_message_serialize_append(instance->pbc_instance, key, packed_buffer, len);
    if (packed_buffer == 0) {
      return nullptr;
    }
  }
  return packed_buffer;
}

bool rw_keyspec_path_is_equal(
  rw_keyspec_instance_t* instance,
  const rw_keyspec_path_t* a,
  const rw_keyspec_path_t* b)
{

  RW_ASSERT(is_good_ks(a));
  RW_ASSERT(is_good_ks(b));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, is_equal);

  KeySpecHelper ks_a(a);
  KeySpecHelper ks_b(b);

  return ks_a.is_equal(ks_b);
}

bool rw_keyspec_path_is_equal_detail(
  rw_keyspec_instance_t* instance,
  const rw_keyspec_path_t* a,
  const rw_keyspec_path_t* b,
  size_t* entry_index,
  int* key_index)
{
  /*
   * ATTN: This function should return an indication of *HOW* the
   * keyspecs differ - not just which PE and key.
   */

  RW_ASSERT(is_good_ks(a));
  RW_ASSERT(is_good_ks(b));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, is_equal_detail);

  if (entry_index) {
    *entry_index = 0;
  }
  if (key_index) {
    *key_index = -1;
  }

  bool retval = false;

  KeySpecHelper ks_a(a);
  KeySpecHelper ks_b(b);

  RwSchemaCategory cat1 = ks_a.get_category();
  RwSchemaCategory cat2 = ks_b.get_category();

  if (cat1 != cat2) {
    return false;
  }

  if (ks_a.is_rooted() != ks_b.is_rooted()) {
    return false;
  }

  const ProtobufCMessage *dompath_a = ks_a.get_dompath();
  if (!dompath_a) {
    return retval;
  }

  const ProtobufCMessage *dompath_b = ks_b.get_dompath();
  if (!dompath_b) {
    return retval;
  }

  if (protobuf_c_message_is_equal_deep(
          instance->pbc_instance, dompath_a, dompath_b)) {
    retval = true;
    return retval;
  }

  size_t depth_a = ks_a.get_depth();
  RW_ASSERT(depth_a);

  size_t depth_b = ks_b.get_depth();
  RW_ASSERT(depth_b);

  if (depth_a != depth_b) {
    if (!entry_index) {
      return retval;
    }
  }

  size_t path_len = std::min(depth_a, depth_b);
  unsigned i;
  for (i = 0; i < path_len; i++) {

    auto path_entry1 = ks_a.get_path_entry(i);
    RW_ASSERT(path_entry1);

    auto path_entry2 = ks_b.get_path_entry(i);
    RW_ASSERT(path_entry2);

    int ki = -1;
    retval = compare_path_entries(instance, path_entry1, path_entry2, &ki, false);

    if (!retval || ki != -1) {
      if (key_index) {
        *key_index = ki;
      }
      break;
    }
  }

  if ((i == path_len) && (depth_a == depth_b)) {
    retval = true;
  } else {
    if (entry_index) {
      *entry_index = i;
    }
    retval = false;
  }

  return retval;
}

bool rw_keyspec_path_is_match_detail(
  rw_keyspec_instance_t* instance,
  const rw_keyspec_path_t* a,
  const rw_keyspec_path_t* b,
  size_t* entry_index,
  int* key_index)
{
  RW_ASSERT(is_good_ks(a));
  RW_ASSERT(is_good_ks(b));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, is_match_detail);

  if (entry_index) {
    *entry_index = 0;
  }
  if (key_index) {
    *key_index = -1;
  }

  bool retval = false;
  KeySpecHelper ks_a(a);
  KeySpecHelper ks_b(b);

  RwSchemaCategory cat1 = ks_a.get_category();
  RwSchemaCategory cat2 = ks_b.get_category();

  if (   cat1 != cat2
      && cat1 != RW_SCHEMA_CATEGORY_ANY
      && cat2 != RW_SCHEMA_CATEGORY_ANY) {
    return retval;
  }

  if (ks_a.is_rooted() != ks_b.is_rooted()) {
    return retval;
  }

  const ProtobufCMessage *dompath_a = ks_a.get_dompath();
  if (!dompath_a) {
    return retval;
  }

  const ProtobufCMessage *dompath_b = ks_b.get_dompath();
  if (!dompath_b) {
    return retval;
  }

  if (protobuf_c_message_is_equal_deep(
          instance->pbc_instance, dompath_a, dompath_b)) {
    return true;
  }

  size_t depth_a = ks_a.get_depth();
  RW_ASSERT(depth_a);

  size_t depth_b = ks_b.get_depth();
  RW_ASSERT(depth_b);

  if (depth_a != depth_b) {
    if (!entry_index) {
      return retval;
    }
  }

  size_t path_len = std::min(depth_a, depth_b);
  unsigned i = 0;

  for (i = 0; i < path_len; i++) {

    auto path_entry1 = ks_a.get_path_entry(i);
    RW_ASSERT(path_entry1);

    auto path_entry2 = ks_b.get_path_entry(i);
    RW_ASSERT(path_entry2);

    int ki = -1;
    retval = compare_path_entries(instance, path_entry1, path_entry2, &ki, true);

    if (!retval || ki != -1) {
      if (key_index) {
        *key_index = ki;
      }
      break;
    }
  }

  if ((i == path_len) && (depth_a == depth_b)) {
    retval = true;
  } else {
    if (entry_index) {
      *entry_index = i;
    }
    retval = false;
  }

  return retval;
}

bool rw_keyspec_path_is_same_path_detail(
  rw_keyspec_instance_t* instance,
  const rw_keyspec_path_t* a,
  const rw_keyspec_path_t* b,
  size_t* entry_index)
{
  RW_ASSERT(is_good_ks(a));
  RW_ASSERT(is_good_ks(b));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, is_path_detail);

  bool retval = false;
  KeySpecHelper ks_a(a);
  KeySpecHelper ks_b(b);

  size_t depth1 = ks_a.get_depth();
  RW_ASSERT(depth1);

  size_t depth2 = ks_b.get_depth();
  RW_ASSERT(depth2);

  if (depth1 != depth2) {
    if (!entry_index) {
      return retval;
    }
  }

  if (entry_index) {
    *entry_index = 0;
  }

  RwSchemaCategory cat1 = ks_a.get_category();
  RwSchemaCategory cat2 = ks_b.get_category();
  if (   cat1 != cat2
      && cat1 != RW_SCHEMA_CATEGORY_ANY
      && cat2 != RW_SCHEMA_CATEGORY_ANY) {
    return retval;
  }

  if (ks_a.is_rooted() != ks_b.is_rooted()) {
    return retval;
  }

  size_t path_len = std::min(depth1, depth2);
  unsigned i = 0;

  for (i = 0; i < path_len; i++) {

    auto path_entry1 = ks_a.get_path_entry(i);
    RW_ASSERT(path_entry1);

    auto path_entry2 = ks_b.get_path_entry(i);
    RW_ASSERT(path_entry2);

    retval = compare_path_entries(instance, path_entry1, path_entry2, NULL, false);

    if (!retval) {
      break;
    }
  }

  if ((i == path_len) && (depth1 == depth2)) {
    retval = true;
  } else {
    if (entry_index) {
      *entry_index = i;
    }
    retval = false;
  }

  return retval;
}

bool rw_keyspec_path_is_branch_detail(
  rw_keyspec_instance_t* instance,
  const rw_keyspec_path_t* prefix,
  const rw_keyspec_path_t* suffix,
  bool ignore_keys,
  size_t* start_index,
  size_t* len)
{
  RW_ASSERT(is_good_ks(prefix));
  RW_ASSERT(is_good_ks(suffix));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, is_branch_detail);

  KeySpecHelper ks_p(prefix);
  KeySpecHelper ks_s(suffix);

  RwSchemaCategory cat1 = ks_p.get_category();
  RwSchemaCategory cat2 = ks_s.get_category();

  if (   cat1 != cat2
      && cat1 != RW_SCHEMA_CATEGORY_ANY
      && cat2 != RW_SCHEMA_CATEGORY_ANY) {
    return false;
  }

  size_t depth_s = ks_s.get_depth();
  if (!depth_s) {
    return false;
  }

  size_t depth_p = ks_p.get_depth();
  if (!depth_p) {
    return false;
  }

  if (len) {
    *len = 0;
  }

  unsigned pi = 0, si = 0;
  bool start_found = false;

  while(pi < depth_p && si < depth_s) {

    auto pe_s = ks_s.get_path_entry(si);
    RW_ASSERT(pe_s);

    auto pe_p = ks_p.get_path_entry(pi);
    RW_ASSERT(pe_p);

    int ki = -1;
    bool retval = compare_path_entries(instance, pe_s, pe_p, &ki, false);

    if (retval && (ignore_keys || ki == -1)) {

      if (len) (*len)++;

      if (!start_found) {
        start_found = true;
        if (start_index) {
          *start_index = pi;
        }
      }

      pi++; si++;

    } else {
      if (start_found) {
        break;
      }
      pi++;
    }
  }

  return start_found;
}

static rw_status_t copy_path_entry_at(
    rw_keyspec_instance_t* instance,
    ProtobufCMessage *dom_path,
    const rw_keyspec_entry_t *entry,
    unsigned index)
{
  rw_status_t status = RW_STATUS_FAILURE;

  RW_ASSERT(is_good_pe(entry));
  RW_ASSERT(dom_path);

  auto fd = protobuf_c_message_descriptor_get_field(
      dom_path->descriptor, RW_SCHEMA_TAG_PATH_ENTRY_START+index);

  if (!fd) {
    return status;
  }

  RW_ASSERT(fd->type == PROTOBUF_C_TYPE_MESSAGE);
  RW_ASSERT(fd->label != PROTOBUF_C_LABEL_REPEATED);

  // Make sure to delete any previous value.
  protobuf_c_message_delete_field(
      instance->pbc_instance, dom_path, RW_SCHEMA_TAG_PATH_ENTRY_START+index);

  ProtobufCMessage *pmsg = nullptr;
  protobuf_c_message_set_field_message(instance->pbc_instance, dom_path, fd, &pmsg);
  RW_ASSERT(pmsg);

  if (!protobuf_c_message_copy_usebody(
          instance->pbc_instance, &entry->base, pmsg)) {
    KEYSPEC_ERROR(instance, "Failed to copy path entry to ks(%p)", pmsg->descriptor);
    protobuf_c_message_delete_field(instance->pbc_instance, dom_path, fd->id);
    return status;
  }

  return RW_STATUS_SUCCESS;
}

rw_status_t rw_keyspec_path_append_entry(
  rw_keyspec_path_t* k,
  rw_keyspec_instance_t* instance,
  const rw_keyspec_entry_t* entry)
{
  rw_status_t status = RW_STATUS_FAILURE;

  RW_ASSERT(is_good_ks(k));
  RW_ASSERT(is_good_pe(entry));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, append_entry);

  KeySpecHelper ks(k);

  if (!ks.has_dompath()) {
    if (rw_keyspec_path_update_dompath(k, instance) != RW_STATUS_SUCCESS) {
      return status;
    }
  }

  RW_ASSERT(ks.has_dompath());
  size_t depth = ks.get_depth();

  ProtobufCMessage *dom_path = (ProtobufCMessage *)ks.get_dompath();
  RW_ASSERT(dom_path);

  status = copy_path_entry_at(instance, dom_path, entry, depth);

  if (status == RW_STATUS_SUCCESS) {
    rw_keyspec_path_delete_binpath(k, instance);
    rw_keyspec_path_delete_strpath(k, instance);
  }

  return status;
}

rw_status_t rw_keyspec_path_append_entries(
  rw_keyspec_path_t* k,
  rw_keyspec_instance_t* instance,
  const rw_keyspec_entry_t* const* entries,
  size_t len)
{
  rw_status_t status = RW_STATUS_FAILURE;

  RW_ASSERT(is_good_ks(k));
  RW_ASSERT(entries);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KeySpecHelper ks(k);

  if (!ks.has_dompath()) {
    if (rw_keyspec_path_update_dompath(k, instance) != RW_STATUS_SUCCESS) {
      return status;
    }
  }

  RW_ASSERT(ks.has_dompath());
  size_t depth = ks.get_depth();

  ProtobufCMessage *dom_path = (ProtobufCMessage *)ks.get_dompath();
  RW_ASSERT(dom_path);

  rw_keyspec_path_delete_binpath(k, instance);
  rw_keyspec_path_delete_strpath(k, instance);

  unsigned i = 0, j = 0;
  for (i = depth, j = 0; j < len; i++, j++) {

    RW_ASSERT(entries[j]);
    status = copy_path_entry_at(instance, dom_path, entries[j], i);
    if (status != RW_STATUS_SUCCESS) {
      break;
    }
  }

  if (j == len) {
    status = RW_STATUS_SUCCESS;
  } else {
    // Failed to append all the entries. Free any allocated path entries.
    for (i = depth; i < (depth+j); i++) {
      protobuf_c_message_delete_field(instance->pbc_instance, dom_path,
                      RW_SCHEMA_TAG_PATH_ENTRY_START+i);
    }
  }

  return status;
}

rw_status_t rw_keyspec_path_append_keyspec(
  rw_keyspec_path_t* k,
  rw_keyspec_instance_t* instance,
  const rw_keyspec_path_t* other)
{
  rw_status_t status = RW_STATUS_FAILURE;

  RW_ASSERT(is_good_ks(k));
  RW_ASSERT(is_good_ks(other));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KeySpecHelper ks(k);
  KeySpecHelper ks_o(other);

  if (!ks.has_dompath()) {
    if (rw_keyspec_path_update_dompath(k, instance) != RW_STATUS_SUCCESS) {
      return status;
    }
  }

  RW_ASSERT(ks.has_dompath());

  size_t depth_s = ks.get_depth();
  size_t depth_d = ks_o.get_depth();

  if (!depth_d) {
    return status;
  }

  rw_keyspec_path_delete_binpath(k, instance);
  rw_keyspec_path_delete_strpath(k, instance);

  ProtobufCMessage *dom_path = (ProtobufCMessage *)ks.get_dompath();
  RW_ASSERT(dom_path);

  unsigned i = 0, j = depth_s;
  for (i = 0; i < depth_d; i++, j++) {

    auto pentry = ks_o.get_path_entry(i);
    RW_ASSERT(pentry);

    status = copy_path_entry_at(instance, dom_path, pentry, j);

    if (status != RW_STATUS_SUCCESS) {
      break;
    }
  }

  if (i == depth_d) {
    status = RW_STATUS_SUCCESS;
  }
  else if (i != depth_d) {
    // Free any allocated messages.
    for (unsigned j = depth_s; j < (depth_s+i); j++) {
      protobuf_c_message_delete_field(
          instance->pbc_instance, dom_path, RW_SCHEMA_TAG_PATH_ENTRY_START+j);
    }
  }

  return status;
}

rw_status_t rw_keyspec_path_append_replace(
  rw_keyspec_path_t* k,
  rw_keyspec_instance_t* instance,
  const rw_keyspec_path_t* other,
  int k_index,
  size_t other_index,
  int other_len)
{
  rw_status_t status = RW_STATUS_FAILURE;

  RW_ASSERT(is_good_ks(k));
  RW_ASSERT(is_good_ks(other));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KeySpecHelper ks(k);
  KeySpecHelper ks_o(other);

  if (!ks.has_dompath()) {
    if (rw_keyspec_path_update_dompath(k, instance) != RW_STATUS_SUCCESS) {
      return status;
    }
  }

  RW_ASSERT(ks.has_dompath());

  size_t depth_s = ks.get_depth();
  size_t depth_d = ks_o.get_depth();

  if (k_index < -1) {
    return RW_STATUS_OUT_OF_BOUNDS;
  }

  if ((k_index > 0) && ((size_t)k_index > depth_s)) {
    return RW_STATUS_OUT_OF_BOUNDS;
  }

  if ((other_index >= depth_d) || (other_len <= 0) || ((other_index + other_len) > depth_d)) {
    return RW_STATUS_OUT_OF_BOUNDS;
  }

  if (((size_t)k_index == depth_s) || (k_index == -1)) {
    k_index = depth_s;
  }

  rw_keyspec_path_delete_binpath(k, instance);
  rw_keyspec_path_delete_strpath(k, instance);

  ProtobufCMessage *dom_path = (ProtobufCMessage *)ks.get_dompath();
  RW_ASSERT(dom_path);

  unsigned i = 0;
  for (i = other_index; i < (other_index+other_len); i++) {

    auto pentry = ks_o.get_path_entry(i);
    RW_ASSERT(pentry);

    status = copy_path_entry_at(instance, dom_path, pentry, k_index++);

    if (status != RW_STATUS_SUCCESS) {
      break;
    }
  }

  if (i != (other_index+other_len)) {
    unsigned st = k_index;
    for (unsigned j = st; j < (i-other_index)+1; j++) {
      protobuf_c_message_delete_field(instance->pbc_instance, dom_path,
                    RW_SCHEMA_TAG_PATH_ENTRY_START+j);
    }
  } else {
    status = RW_STATUS_SUCCESS;
  }

  return status;
}

rw_status_t rw_keyspec_path_trunc_suffix_n(
  rw_keyspec_path_t* k,
  rw_keyspec_instance_t* instance,
  size_t index)
{
  rw_status_t status = RW_STATUS_FAILURE;

  RW_ASSERT(is_good_ks(k));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, trunc_suffix);

  KeySpecHelper ks(k);

  if (!ks.has_dompath()) {
    if (rw_keyspec_path_update_dompath(k, instance) != RW_STATUS_SUCCESS) {
      return status;
    }
  }

  RW_ASSERT(ks.has_dompath());
  size_t depth = ks.get_depth();

  if (index > depth) {
    status = RW_STATUS_OUT_OF_BOUNDS;
    return status;
  }

  rw_keyspec_path_delete_binpath(k, instance);
  rw_keyspec_path_delete_strpath(k, instance);

  ProtobufCMessage *dom_path = (ProtobufCMessage *)ks.get_dompath();
  RW_ASSERT(dom_path);

  for (unsigned i = index; i < depth; i++) {
    if(!protobuf_c_message_delete_field(
            instance->pbc_instance, dom_path, RW_SCHEMA_TAG_PATH_ENTRY_START+i)) {
      return status;
    }
  }

  /* Delete any path entries in the unknown buffer */
  for (unsigned i = RW_SCHEMA_TAG_PATH_ENTRY_START+depth; 
       (i <= RW_SCHEMA_TAG_PATH_ENTRY_END && (nullptr != dom_path->unknown_buffer)); 
       ++i) {

    protobuf_c_message_delete_field(instance->pbc_instance, dom_path, i);
  }

  return RW_STATUS_SUCCESS;
}

rw_status_t rw_keyspec_path_trunc_concrete(
  rw_keyspec_path_t* k,
  rw_keyspec_instance_t* instance)
{
  RW_ASSERT(k);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  // Get the field descriptor for dompath
  const ProtobufCFieldDescriptor* path_fd
    = protobuf_c_message_descriptor_get_field(
        k->base.descriptor,
        RW_SCHEMA_TAG_KEYSPEC_DOMPATH);
  RW_ASSERT(path_fd);
  RW_ASSERT(path_fd->type == PROTOBUF_C_TYPE_MESSAGE);
  RW_ASSERT(path_fd->label != PROTOBUF_C_LABEL_REPEATED);
  RW_ASSERT(!path_fd->ctype);

  // Get the message descriptor for dompath
  const ProtobufCMessageDescriptor* dom_mdesc
    = reinterpret_cast<const ProtobufCMessageDescriptor*>(path_fd->descriptor);

  // Find the truncation depth - the first non-concrete path entry
  unsigned entry = RW_SCHEMA_TAG_PATH_ENTRY_START;
  for (; entry <= RW_SCHEMA_TAG_PATH_ENTRY_END; ++entry) {
    const ProtobufCFieldDescriptor* entry_fd
      = protobuf_c_message_descriptor_get_field(dom_mdesc, entry);
    if (   entry_fd == NULL
        || entry_fd->descriptor == &rw_schema_path_entry__descriptor) {
      break;
    }
  }

  // Truncate all generic path elements
  rw_status_t rs = rw_keyspec_path_trunc_suffix_n(k, instance, entry-RW_SCHEMA_TAG_PATH_ENTRY_START);
  if (RW_STATUS_SUCCESS != rs) {
    return rs;
  }

  protobuf_c_boolean ok
    = protobuf_c_message_delete_unknown_all(instance->pbc_instance,  &k->base);
  if (ok) {
    return RW_STATUS_SUCCESS;
  }

  return RW_STATUS_SUCCESS;
}

rw_status_t rw_keyspec_path_serialize_dompath(
  const rw_keyspec_path_t* k,
  rw_keyspec_instance_t* instance,
  ProtobufCBinaryData* data)
{
  rw_status_t status = RW_STATUS_FAILURE;

  RW_ASSERT(is_good_ks(k));
  RW_ASSERT(data && !data->data);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, pack_dompath);

  KeySpecHelper ks(k);

  const uint8_t *bd = nullptr;
  size_t len = ks.get_binpath(&bd);

  if (!bd) {
    KEYSPEC_ERROR(instance, k, "Failed to serialize dompath");
    return status;
  }

  data->data = (uint8_t *)malloc(len);
  RW_ASSERT(data->data);

  data->len = len;
  memcpy(data->data, bd, len);

  return RW_STATUS_SUCCESS;
}

rw_keyspec_path_t* rw_keyspec_path_create_with_binpath_buf(
    rw_keyspec_instance_t* instance,
    size_t len,
    const void* data)
{
  RW_ASSERT(data);
  RW_ASSERT(len);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, create_with_binpath);

  RwSchemaPathSpec *kspec = (RwSchemaPathSpec *)malloc(sizeof(RwSchemaPathSpec));
  RW_ASSERT(kspec);

  rw_schema_path_spec__init(kspec);
  kspec->has_binpath = true;
  kspec->binpath.len = len;
  kspec->binpath.data = (uint8_t *)malloc(len);
  RW_ASSERT(kspec->binpath.data);

  memcpy(kspec->binpath.data, data, len);

  rw_status_t s = rw_keyspec_path_update_dompath(
      &kspec->rw_keyspec_path_t, instance);

  if (s != RW_STATUS_SUCCESS) {
    KEYSPEC_ERROR(instance, "Failed to unpack ks to generic type");
    rw_schema_path_spec__free_unpacked(instance->pbc_instance, kspec);
    return NULL;
  }

  return &kspec->rw_keyspec_path_t;
}

rw_keyspec_path_t* rw_keyspec_path_create_with_binpath_binary_data(
    rw_keyspec_instance_t* instance,
    const ProtobufCBinaryData* data)
{
  RW_ASSERT(data);
  RW_ASSERT(data->data);
  RW_ASSERT(data->len);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, create_with_binpath);

  RwSchemaPathSpec *kspec = (RwSchemaPathSpec *)malloc(sizeof(RwSchemaPathSpec));
  RW_ASSERT(kspec);

  rw_schema_path_spec__init(kspec);
  kspec->has_binpath = true;
  kspec->binpath.len = data->len;
  kspec->binpath.data = (uint8_t *)malloc(data->len);
  RW_ASSERT(kspec->binpath.data);

  memcpy(kspec->binpath.data, data->data, data->len);

  rw_status_t s = rw_keyspec_path_update_dompath(
      &kspec->rw_keyspec_path_t, instance);

  if (s != RW_STATUS_SUCCESS) {
    KEYSPEC_ERROR(instance, "Failed to unpack ks to generic type");
    rw_schema_path_spec__free_unpacked(instance->pbc_instance, kspec);
    return NULL;
  }

  return &kspec->rw_keyspec_path_t;
}

rw_keyspec_path_t* rw_keyspec_path_create_with_dompath_binary_data(
    rw_keyspec_instance_t* instance,
    const ProtobufCBinaryData* data)
{
  RW_ASSERT(data);
  RW_ASSERT(data->data);
  RW_ASSERT(data->len);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, create_with_dompath);

  RwSchemaPathSpec *kspec = (RwSchemaPathSpec *)malloc(sizeof(RwSchemaPathSpec));
  RW_ASSERT(kspec);

  rw_schema_path_spec__init(kspec);
  kspec->dompath = (RwSchemaPathDom *)malloc(sizeof(RwSchemaPathDom));
  RW_ASSERT(kspec->dompath);

  rw_schema_path_dom__init(kspec->dompath);

  if (!protobuf_c_message_unpack_usebody(
          instance->pbc_instance, kspec->dompath->base.descriptor,
          data->len, data->data, &kspec->dompath->base)) {

    KEYSPEC_ERROR(instance, "Failed to unpack ks to generic type");
    rw_schema_path_spec__free_unpacked(instance->pbc_instance, kspec);
    return NULL;
  }

  return &kspec->rw_keyspec_path_t;
}

rw_keyspec_path_t* rw_keyspec_path_create_concrete_with_dompath_binary_data(
    rw_keyspec_instance_t* instance,
    const ProtobufCBinaryData* data,
    const ProtobufCMessageDescriptor *mdesc)
{
  RW_ASSERT(data);
  RW_ASSERT(data->data);
  RW_ASSERT(data->len);
  RW_ASSERT(mdesc);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, create_with_dompath);

  ProtobufCMessage *out_ks = nullptr;
  out_ks = (ProtobufCMessage *)malloc(mdesc->sizeof_message);
  RW_ASSERT(out_ks);

  protobuf_c_message_init(mdesc, out_ks);

  const ProtobufCFieldDescriptor *fd = nullptr;
  fd = protobuf_c_message_descriptor_get_field(out_ks->descriptor,
                  RW_SCHEMA_TAG_KEYSPEC_DOMPATH);
  RW_ASSERT(fd);

  ProtobufCMessage *dom = nullptr;
  protobuf_c_message_set_field_message(instance->pbc_instance, out_ks, fd, &dom);
  RW_ASSERT(dom);

  if (!protobuf_c_message_unpack_usebody(
          instance->pbc_instance, (const ProtobufCMessageDescriptor *)(fd->descriptor),
          data->len, data->data, dom)) {
    KEYSPEC_ERROR(instance, "Failed to unpack ks(%p)", mdesc);
    protobuf_c_message_free_unpacked(instance->pbc_instance, out_ks);
    return NULL;
  }

  return (rw_keyspec_path_t *)out_ks;
}

rw_status_t rw_keyspec_path_free(
    rw_keyspec_path_t* k,
    rw_keyspec_instance_t* instance)
{
  RW_ASSERT(is_good_ks(k));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, free);

  protobuf_c_message_free_unpacked(instance->pbc_instance, &k->base);

  return RW_STATUS_SUCCESS;
}

bool rw_keyspec_path_has_wildcards(
     const rw_keyspec_path_t *k)
{
  RW_ASSERT(is_good_ks(k));

  return KeySpecHelper(k).has_wildcards();
}

bool rw_keyspec_path_has_wildcards_for_depth(
    const rw_keyspec_path_t *k,
    size_t depth)
{
  RW_ASSERT(is_good_ks(k));

  if (!depth) {
    return false;
  }

  return KeySpecHelper(k).has_wildcards(0, depth);
}

bool rw_keyspec_path_has_wildcards_for_last_entry(
     const rw_keyspec_path_t *k)
{
  RW_ASSERT(is_good_ks(k));

  KeySpecHelper ks(k);

  size_t depth = ks.get_depth();

  if (!depth) {
    return false;
  }

  return ks.has_wildcards(depth-1);
}

static listy_pe_type_t get_path_entry_type(
    rw_keyspec_instance_t* instance,
    const rw_keyspec_entry_t *entry)
{
  RW_ASSERT(entry);
  const RwSchemaElementId *element = rw_keyspec_entry_get_element_id(entry);

  switch (element->element_type) {
    case RW_SCHEMA_ELEMENT_TYPE_CONTAINER:
      return PE_CONTAINER;
    case RW_SCHEMA_ELEMENT_TYPE_LISTY_0:
      return PE_LISTY_WITH_ALL_WC;
    case RW_SCHEMA_ELEMENT_TYPE_LEAF:
    case RW_SCHEMA_ELEMENT_TYPE_LEAF_LIST:
      RW_ASSERT_NOT_REACHED();
    default:
      break;
  }

  unsigned no_wc = 0, no_keys = 0;
  // any wildcarded path entries are also iteratable.
  // Listy type. Check whether the key values are specified.
  no_keys = rw_keyspec_entry_num_keys(entry);
  for (unsigned j = 0; j < no_keys; j++) {
    if (!protobuf_c_message_has_field(instance->pbc_instance,
            &entry->base, RW_SCHEMA_TAG_ELEMENT_KEY_START+j)) {
      no_wc++;
    }
  }

  RW_ASSERT(no_keys);

  if (no_wc == 0) {
    return PE_LISTY_WITH_ALL_KEYS;
  }

  if (no_wc == no_keys) {
    return PE_LISTY_WITH_ALL_WC;
  }

  return PE_LISTY_WITH_WC_AND_KEYS;
}

bool rw_keyspec_entry_is_listy(
  const rw_keyspec_entry_t *entry)
{
  RW_ASSERT(entry);

  auto element = rw_keyspec_entry_get_element_id(entry);
  RW_ASSERT(element);

  return ((element->element_type >= RW_SCHEMA_ELEMENT_TYPE_LEAF_LIST)
          && (element->element_type <= RW_SCHEMA_ELEMENT_TYPE_LISTY_9));
}

bool rw_keyspec_path_is_listy(
  const rw_keyspec_path_t *ks)
{
  RW_ASSERT(is_good_ks(ks));

  return KeySpecHelper(ks).is_listy();
}

bool rw_keyspec_path_is_generic(const rw_keyspec_path_t *ks)
{
  RW_ASSERT(is_good_ks(ks));

  return KeySpecHelper(ks).is_generic();
}

bool rw_keyspec_path_is_leafy(const rw_keyspec_path_t *ks)
{
  RW_ASSERT(is_good_ks(ks));

  return KeySpecHelper(ks).is_leafy();
}

const ProtobufCFieldDescriptor*
rw_keyspec_path_get_leaf_field_desc(const rw_keyspec_path_t *ks)
{
  RW_ASSERT(is_good_ks(ks));
  return KeySpecHelper(ks).get_leaf_pb_desc();
}

bool rw_keyspec_path_is_rooted(
   const rw_keyspec_path_t* ks)
{

  RW_ASSERT(is_good_ks(ks));

  return KeySpecHelper(ks).is_rooted();
}

bool rw_keyspec_path_is_module_root(
    const rw_keyspec_path_t* ks)
{
  RW_ASSERT(is_good_ks(ks));
  return KeySpecHelper(ks).is_module_root();
}

bool rw_keyspec_path_is_root(
    const rw_keyspec_path_t* ks)
{
  RW_ASSERT(is_good_ks(ks));
  return KeySpecHelper(ks).is_schema_root();
}

static const rw_yang_pb_msgdesc_t*
rw_keyspec_path_find_root_mdesc_schema(
    KeySpecHelper& k,
    rw_keyspec_instance_t* instance,
    const rw_yang_pb_schema_t* schema)
{
  RW_ASSERT(schema);
  RW_ASSERT(k.is_module_root()); // Only module root supported as of now

  auto path_entry = k.get_specific_path_entry(RW_SCHEMA_TAG_PATH_ENTRY_MODULE_ROOT);
  RW_ASSERT(path_entry);

  auto elem_id = rw_keyspec_entry_get_element_id(path_entry);
  RW_ASSERT(elem_id);

  auto cat = k.get_category();
  const rw_yang_pb_msgdesc_t *mr_mdesc = nullptr;

  for (unsigned m = 0; m < schema->module_count; m++) {
    auto ypbc_module = schema->modules[m];
    RW_ASSERT(ypbc_module);

    switch (cat) {
      case RW_SCHEMA_CATEGORY_ANY:
      case RW_SCHEMA_CATEGORY_DATA:
      case RW_SCHEMA_CATEGORY_CONFIG:
        if (ypbc_module->data_module) {
          mr_mdesc = ypbc_module->data_module;
        }
        break;
      case RW_SCHEMA_CATEGORY_RPC_INPUT:
        if (ypbc_module->rpci_module) {
          mr_mdesc = ypbc_module->rpci_module;
        }
        break;
      case RW_SCHEMA_CATEGORY_RPC_OUTPUT:
        if (ypbc_module->rpco_module) {
          mr_mdesc = ypbc_module->rpco_module;
        }
        break;
      case RW_SCHEMA_CATEGORY_NOTIFICATION:
        if (ypbc_module->notif_module) {
          mr_mdesc = ypbc_module->notif_module;
        }
        break;
      default:
        RW_ASSERT_NOT_REACHED();
    }

    if (mr_mdesc) {
      auto elem_id_c = rw_keyspec_entry_get_element_id(mr_mdesc->schema_entry_value);
      if (IS_SAME_ELEMENT(elem_id, elem_id_c)) {
        return mr_mdesc;
      }
      mr_mdesc = nullptr;
    }
  }

  return mr_mdesc;
}

rw_status_t rw_keyspec_path_find_msg_desc_schema(
  const rw_keyspec_path_t* k,
  rw_keyspec_instance_t* instance,
  const rw_yang_pb_schema_t* schema,
  const rw_yang_pb_msgdesc_t** result_ypbc_msgdesc,
  rw_keyspec_path_t** remainder)
{
  rw_status_t status = RW_STATUS_FAILURE;

  RW_ASSERT(is_good_ks(k));
  RW_ASSERT(result_ypbc_msgdesc);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, find_mdesc);

  KeySpecHelper ks(k);

  *result_ypbc_msgdesc = nullptr;
  if (remainder) {
    *remainder = nullptr;
  }

  if (!schema) {
    auto ypbc_msgdesc = k->base.descriptor->ypbc_mdesc;
    if (!ypbc_msgdesc) {
      KEYSPEC_ERROR(instance, "Generic KS input without schema");
      return status;
    }
    schema = ypbc_msgdesc->module->schema;
  }

  if (!ks.is_rooted()) {
    //Keyspec is not rooted. What to do??
    return status;
  }

  size_t depth_k = ks.get_depth();
  if (!depth_k) {
    *result_ypbc_msgdesc = rw_keyspec_path_find_root_mdesc_schema(ks, instance, schema);
    if (!*result_ypbc_msgdesc) {
      return status;
    }
    return RW_STATUS_SUCCESS;
  }

  auto path_entry = ks.get_path_entry(0);
  RW_ASSERT(path_entry);

  auto elemid = rw_keyspec_entry_get_element_id(path_entry);
  RW_ASSERT(elemid);

  for (unsigned m = 0; m < schema->module_count; m++) {
    auto ypbc_module = schema->modules[m];
    RW_ASSERT(ypbc_module);

    auto data_module = ypbc_module->data_module;
    if (data_module) {
      for (unsigned i = 0; i < data_module->child_msg_count; i++) {
        auto top_level_msg = data_module->child_msgs[i];
        RW_ASSERT(top_level_msg);

        auto elemid_s = rw_keyspec_entry_get_element_id(
            top_level_msg->schema_entry_value);
        RW_ASSERT(elemid_s);

        if (IS_SAME_ELEMENT(elemid, elemid_s)) {
          // This top level message matched, go deep further.
          status = rw_keyspec_path_find_msg_desc_msg(
              k, instance, top_level_msg, 0, result_ypbc_msgdesc, remainder);
          return status;
        }
      }
    }

    auto rpci_module = ypbc_module->rpci_module;
    if (rpci_module) {
      for (unsigned i = 0; i < rpci_module->child_msg_count; i++) {
        auto top_level_msg = rpci_module->child_msgs[i];
        RW_ASSERT(top_level_msg);

        auto elemid_s = rw_keyspec_entry_get_element_id(
            top_level_msg->schema_entry_value);
        RW_ASSERT(elemid_s);

        if (IS_SAME_ELEMENT(elemid_s, elemid)) {
          // This top level message matched, go deep further.
          status = rw_keyspec_path_find_msg_desc_msg(
              k, instance, top_level_msg, 0, result_ypbc_msgdesc, remainder);
          return status;
        }
      }
    }

    auto rpco_module = ypbc_module->rpco_module;
    if (rpco_module) {
      for (unsigned i = 0; i < rpco_module->child_msg_count; i++) {
        auto top_level_msg = rpco_module->child_msgs[i];
        RW_ASSERT(top_level_msg);

        auto elemid_s = rw_keyspec_entry_get_element_id(
            top_level_msg->schema_entry_value);
        RW_ASSERT(elemid_s);

        if (IS_SAME_ELEMENT(elemid_s, elemid)) {
          // This top level message matched, go deep further.
          status = rw_keyspec_path_find_msg_desc_msg(
              k, instance, top_level_msg, 0, result_ypbc_msgdesc, remainder);
          return status;
        }
      }
    }

    auto notif_module = ypbc_module->notif_module;
    if (notif_module) {
      for (unsigned i = 0; i < notif_module->child_msg_count; i++) {
        auto top_level_msg = notif_module->child_msgs[i];
        RW_ASSERT(top_level_msg);

        auto elemid_s = rw_keyspec_entry_get_element_id(
            top_level_msg->schema_entry_value);
        RW_ASSERT(elemid_s);

        if (IS_SAME_ELEMENT(elemid_s, elemid)) {
          // This top level message matched, go deep further.
          status = rw_keyspec_path_find_msg_desc_msg(
              k, instance, top_level_msg, 0, result_ypbc_msgdesc, remainder);
          return status;
        }
      }
    }
  }

  return status;
}

rw_status_t rw_keyspec_path_find_msg_desc_msg(
  const rw_keyspec_path_t* k,
  rw_keyspec_instance_t* instance,
  const rw_yang_pb_msgdesc_t* start_ypbc_msgdesc,
  size_t start_depth,
  const rw_yang_pb_msgdesc_t** result_ypbc_msgdesc,
  rw_keyspec_path_t** remainder)
{
  rw_status_t status = RW_STATUS_FAILURE;

  RW_ASSERT(start_ypbc_msgdesc);
  RW_ASSERT(k);
  RW_ASSERT(result_ypbc_msgdesc);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KeySpecHelper ks(k);

  size_t depth_k = ks.get_depth();
  if (start_depth >= (depth_k)) {
    return status;
  }

  *result_ypbc_msgdesc = NULL;
  if (remainder) {
    *remainder = NULL;
  }

  unsigned path_index = start_depth+1;
  const rw_keyspec_entry_t *path_entry = nullptr;
  auto temp = start_ypbc_msgdesc;
  auto deepest_match = start_ypbc_msgdesc;
  const RwSchemaElementId* ele_id_k = nullptr;

  while (path_index < depth_k) {

    path_entry = ks.get_path_entry(path_index);
    RW_ASSERT(path_entry);

    ele_id_k = rw_keyspec_entry_get_element_id(path_entry);
    RW_ASSERT(ele_id_k);

    unsigned i = 0;
    const rw_yang_pb_msgdesc_t *child = nullptr;
    for (i = 0; i < temp->child_msg_count; i++) {
      child = temp->child_msgs[i];
      RW_ASSERT(child);

      auto ele_id_s = rw_keyspec_entry_get_element_id(child->schema_entry_value);
      RW_ASSERT(ele_id_s);

      if (IS_SAME_ELEMENT(ele_id_s, ele_id_k)) {
        break;
      }
    }

    if (i == temp->child_msg_count) {
      break;
    }

    temp = child;
    deepest_match = child;
    path_index++;
  }

  if (path_index == depth_k) {
    *result_ypbc_msgdesc = deepest_match->schema_path_ypbc_desc;
    return RW_STATUS_SUCCESS;
  }

  if (path_index == depth_k - 1) {

    const ProtobufCMessageDescriptor *desc = deepest_match->pbc_mdesc;
    RW_ASSERT(desc);

    if (protobuf_c_message_descriptor_get_field(
            desc, ele_id_k->element_tag) != nullptr) {

      *result_ypbc_msgdesc = deepest_match->schema_path_ypbc_desc;
      return RW_STATUS_SUCCESS;
    }
  }

  status = RW_STATUS_OUT_OF_BOUNDS;
  *result_ypbc_msgdesc = deepest_match->schema_path_ypbc_desc;

  if (remainder) {

    RwSchemaPathSpec* new_ks =
        (RwSchemaPathSpec*)malloc(sizeof(RwSchemaPathSpec));
    RW_ASSERT(new_ks);

    rw_schema_path_spec__init(new_ks);
    new_ks->dompath = (RwSchemaPathDom*)malloc(sizeof(RwSchemaPathDom));
    RW_ASSERT(new_ks->dompath);

    rw_schema_path_dom__init(new_ks->dompath);
    if (rw_keyspec_path_append_replace(
            &new_ks->rw_keyspec_path_t, instance, k, 0, path_index,
            (depth_k - path_index)) != RW_STATUS_SUCCESS) {

      rw_schema_path_spec__free_unpacked(instance->pbc_instance, new_ks);
    } else {
      *remainder = &new_ks->rw_keyspec_path_t;
    }
  }

  return status;
}

static bool rw_keyspec_path_is_prefix_match_impl(
  rw_keyspec_instance_t* instance,
  KeySpecHelper& ks_a,
  KeySpecHelper& ks_b,
  size_t depth_a,
  size_t depth_b)
{
  bool retval = false;

  size_t min_depth = (depth_a > depth_b) ? depth_b: depth_a;
  unsigned i = 0;

  for (i = 0; i < min_depth; i++) {
    auto path_entry1 = ks_a.get_path_entry(i);
    RW_ASSERT(path_entry1);

    auto path_entry2 = ks_b.get_path_entry(i);
    RW_ASSERT(path_entry2);

    int ki = -1;
    retval = compare_path_entries(instance, path_entry1, path_entry2, &ki, true);

    if (!retval || ki != -1) {
      retval = false;
      break;
    }
  }

  if (i == min_depth) {
    retval = true;
  }
  return retval;
}

bool rw_keyspec_path_is_a_sub_keyspec(
  rw_keyspec_instance_t* instance,
  const rw_keyspec_path_t *a,
  const rw_keyspec_path_t *b)
{
  RW_ASSERT(a);
  RW_ASSERT(b);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, is_sub_ks);

  KeySpecHelper ks_a(a);
  KeySpecHelper ks_b(b);

  size_t depth_a = ks_a.get_depth();
  size_t depth_b = ks_b.get_depth();

  if (!depth_a || !depth_b) {
    // Empty keyspec
    // ATTN: Why false?  Shouldn't it be true?
    return false;
  }

  return rw_keyspec_path_is_prefix_match_impl(
      instance, ks_a, ks_b, depth_a, depth_b);
}

bool rw_keyspec_path_is_match_or_prefix(
  const rw_keyspec_path_t *shorter_ks,
  rw_keyspec_instance_t* instance,
  const rw_keyspec_path_t *longer_ks)
{
  RW_ASSERT(shorter_ks);
  RW_ASSERT(longer_ks);

  KeySpecHelper ks_s(shorter_ks);
  KeySpecHelper ks_l(longer_ks);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  size_t depth_a = ks_s.get_depth();
  size_t depth_b = ks_l.get_depth();

  if (!depth_a || !depth_b) {
    // Empty keyspec
    // ATTN: Why false?  Shouldn't it be true?
    return false;
  }
  if (depth_a > depth_b) {
    // Can't be prefix match
    return false;
  }

  return rw_keyspec_path_is_prefix_match_impl(
      instance, ks_s, ks_l, depth_a, depth_b);
}

/*
 * Gets the key_tag from the path_entry message.
 * This function should not be called for generic
 * path_entry types.
 */
static uint32_t get_key_tag_from_path_entry_desc(
    const rw_keyspec_entry_t* path_entry,
    uint32_t key_index)
{
  RW_ASSERT(path_entry);
  uint32_t key_tag = 0;

  const ProtobufCFieldDescriptor* fd = nullptr;
  fd = protobuf_c_message_descriptor_get_field(
      path_entry->base.descriptor, RW_SCHEMA_TAG_ELEMENT_KEY_START+key_index);

  if (!fd) {
    return (key_tag);
  }

  RW_ASSERT(fd->type == PROTOBUF_C_TYPE_MESSAGE);
  const ProtobufCMessageDescriptor *mdesc = (const ProtobufCMessageDescriptor *)fd->descriptor;

  RW_ASSERT(mdesc);
  RW_ASSERT(mdesc->n_fields == 2);

  key_tag = mdesc->fields[1].id;

  return (key_tag);
}


/*
 * The below function converts the generic path_entry
 * to concerete path entry using the schema. This is required
 * to extract the key values from the generic path entry
 * which will be in the unknown fields list.
 */
static rw_keyspec_entry_t* convert_generic_pe_to_concerete(
    rw_keyspec_instance_t* instance,
    const rw_keyspec_entry_t *path_entry,
    const rw_yang_pb_msgdesc_t *msg_ypbmd)
{
  RW_ASSERT(is_good_pe(path_entry));
  RW_ASSERT(msg_ypbmd);

  if (!IS_PATH_ENTRY_GENERIC(path_entry)) {
    return nullptr;
  }

  const rw_yang_pb_msgdesc_t *entry_ypbmd = nullptr;

  auto elem_id = rw_keyspec_entry_get_element_id(path_entry);
  RW_ASSERT(elem_id);

  if (elem_id->element_type == RW_SCHEMA_ELEMENT_TYPE_LEAF_LIST) {
    // For leaf-list, this is the parent ypb-msg desc.
    for (unsigned i = 0; i < msg_ypbmd->child_msg_count; i++) {
      if (msg_ypbmd->child_msgs[i]->pb_element_tag == elem_id->element_tag) {
        entry_ypbmd = msg_ypbmd->child_msgs[i]->schema_entry_ypbc_desc;
        break;
      }
    }
  } else {
    entry_ypbmd = msg_ypbmd->schema_entry_ypbc_desc;
  }

  if (!entry_ypbmd) {
    return nullptr;
  }

  rw_keyspec_entry_t* cpentry = rw_keyspec_entry_create_dup_of_type(
      path_entry, instance, entry_ypbmd->pbc_mdesc);

  return cpentry;
}

/*
 * Gets the Field description and Value
 * pointer from the Path entry
 */
static bool get_key_finfo_by_index(
    const rw_keyspec_entry_t* path_entry,
    uint32_t key_index,
    ProtobufCFieldInfo* finfo)
{
  RW_ASSERT(path_entry);
  bool retval = false;
  size_t off = 0;
  void* key_ptr = nullptr;
  protobuf_c_boolean is_dptr = false;
  const ProtobufCFieldDescriptor* fd = nullptr;

  size_t count = protobuf_c_message_get_field_desc_count_and_offset(
      &path_entry->base, RW_SCHEMA_TAG_ELEMENT_KEY_START+key_index,
      &fd, &key_ptr, &off, &is_dptr);

  if (!count) {
    return retval;
  }

  RW_ASSERT(fd->type == PROTOBUF_C_TYPE_MESSAGE);
  RW_ASSERT(key_ptr);

  ProtobufCMessage *key = (ProtobufCMessage *)key_ptr;

  // ATTN: Change this when the option string_key is removed
  // from the actual key entries.
  if (key->descriptor->n_fields == 2) {
    fd = &key->descriptor->fields[1];
  } else {
    // Generic path-entry. This is just a place-holder.
    // We cannot extract the keys anyway even if they are present in the
    // unknown list.
    // ATTN: We should try to compare serialized data
    // against keys that are also serialized data.
    //*odesc = &key->descriptor->fields[0];
    return false;
  }

  auto found_key = protobuf_c_message_get_field_instance(
    NULL, key, fd, 0, finfo );
  return found_key;
}

/*
 * Gets the string value of the key.
 */
static bool get_key_string_from_path_entry(
    rw_keyspec_instance_t* instance,
    const rw_keyspec_entry_t *path_entry,
    uint32_t key_index,
    char *key_str,
    size_t *key_len)
{
  RW_ASSERT(path_entry);
  bool retval = false;

  const ProtobufCFieldDescriptor* fd = nullptr;
  const ProtobufCFieldDescriptor* kfd = nullptr;
  void *key_ptr = nullptr;
  size_t off = 0;
  protobuf_c_boolean is_dptr = false;

  size_t count = protobuf_c_message_get_field_desc_count_and_offset(
      &path_entry->base, RW_SCHEMA_TAG_ELEMENT_KEY_START+key_index,
      &fd, &key_ptr, &off, &is_dptr);

  if (!count) {
    return retval;
  }

  RW_ASSERT(fd->type == PROTOBUF_C_TYPE_MESSAGE);
  RW_ASSERT(key_ptr);

  ProtobufCMessage *key = (ProtobufCMessage *)key_ptr;

  // ATTN: Change this when the option string_key is removed
  // from the actual key entries.
  if (key->descriptor->n_fields == 2) {
    kfd = &key->descriptor->fields[1];
  } else {
    // Generic path-entry. This is just a place-holder.
    // We cannot extract the keys anyway even if they are present in the
    // unknown list.
    kfd = &key->descriptor->fields[0];
  }

  RW_ASSERT(kfd);
  count = protobuf_c_message_get_field_count_and_offset(
      key, kfd, &key_ptr, &off, &is_dptr);

  if (count) {
    retval = protobuf_c_field_get_text_value(
        instance->pbc_instance, kfd, key_str, key_len, key_ptr);
  }

  return retval;
}

static bool rw_keyspec_entry_has_wildcards(
    const rw_keyspec_entry_t *path_entry)
{
  RW_ASSERT(is_good_pe(path_entry));

  uint32_t num_keys = rw_keyspec_entry_num_keys(path_entry);
  for (unsigned i = 0; i < num_keys; i++) {
    if (!rw_keyspec_entry_has_key(path_entry, i)) {
      return true;
    }
  }
  return false;
}

static rw_status_t path_entry_copy_keys_to_msg(
    rw_keyspec_instance_t* instance,
    const rw_keyspec_entry_t *path_entry,
    ProtobufCMessage *msg)
{
  RW_ASSERT(msg);
  RW_ASSERT(is_good_pe(path_entry));

  rw_status_t status = RW_STATUS_FAILURE;
  UniquePtrKeySpecEntry::uptr_t uptr(nullptr, instance);

  if (IS_PATH_ENTRY_GENERIC(path_entry)) {

    rw_keyspec_entry_t* cpe = convert_generic_pe_to_concerete(
        instance, path_entry, msg->descriptor->ypbc_mdesc);

    if (!cpe) {
      return status;
    }

    uptr.reset(cpe);
    path_entry = uptr.get();
  }

  uint32_t num_keys = rw_keyspec_entry_num_keys(path_entry);
  for (unsigned i = 0; i < num_keys; i++) {

    if (!rw_keyspec_entry_has_key(path_entry, i)) {
      continue;
    }

    ProtobufCMessage* keym = rw_keyspec_entry_get_key(path_entry, i);
    RW_ASSERT(keym);
    RW_ASSERT(keym->descriptor->n_fields == 2);  // ATTN: This should be fixed when the implementation changes

    ProtobufCFieldInfo field = {};
    protobuf_c_boolean ok = protobuf_c_message_get_field_instance(
        instance->pbc_instance, keym, &keym->descriptor->fields[1], 0, &field);
    RW_ASSERT(ok);

    field.fdesc = protobuf_c_message_descriptor_get_field(
        msg->descriptor, keym->descriptor->fields[1].id);
    RW_ASSERT(field.fdesc);

    ok = protobuf_c_message_set_field(instance->pbc_instance, msg, &field);
    if (!ok) {
      return status;
    }
  }

  return RW_STATUS_SUCCESS;
}

#define KEY_STR_LEN 512

/*
 * Given a path_entry in the keyspec
 * and its corresponding protobuf message
 * this functions checks whether the keys are
 * equal in both.
 */
static bool compare_path_entry_and_message(
    rw_keyspec_instance_t* instance,
    const rw_keyspec_entry_t *path_entry,
    const ProtobufCMessage *msg,
    bool match_wc)
{
  RW_ASSERT(path_entry);
  RW_ASSERT(msg);
  bool retval = false;
  unsigned i = 0;
  const rw_yang_pb_msgdesc_t *ypbc_md = msg->descriptor->ypbc_mdesc;
  rw_keyspec_entry_t *conc_pe = NULL;

  RW_ASSERT(ypbc_md);

  if (IS_PATH_ENTRY_GENERIC(path_entry)) {

    /* If this was a generic keyspec (generic pathentry), try
       converting it to concerete pathentry using the schema. This is
       required to compare key values. */
    conc_pe = convert_generic_pe_to_concerete(instance, path_entry, ypbc_md);

    if (conc_pe) {
      path_entry = conc_pe;
    }
  }

  auto num_keys = rw_keyspec_entry_num_keys(path_entry);

  for(i = 0; i < num_keys; i++) {
    ProtobufCFieldInfo key_finfo;
    bool rc = get_key_finfo_by_index(path_entry, i, &key_finfo);

    if (!rc) {
      if (!match_wc) {
        break;
      }
      continue;
    }

    auto key_tag = get_key_tag_from_path_entry_desc(
        ypbc_md->schema_entry_value, i);
    RW_ASSERT(key_tag);

    auto fd = protobuf_c_message_descriptor_get_field( msg->descriptor, key_tag );
    if (!fd) {
      break;
    }

    ProtobufCFieldInfo msg_finfo;
    auto found_key = protobuf_c_message_get_field_instance(
      instance->pbc_instance, msg, fd, 0, &msg_finfo );
    if (!found_key) {
      break;
    }

    int cmp = protobuf_c_field_info_compare( &key_finfo, &msg_finfo );
    if (cmp != 0) {
      break;
    }
  }

  if (i == num_keys) {
    retval = true;
  }

  if (conc_pe) {
    protobuf_c_message_free_unpacked(instance->pbc_instance, &conc_pe->base);
  }

  return (retval);
}

const ProtobufCMessage* keyspec_find_deeper(
    rw_keyspec_instance_t* instance,
    size_t depth_i,
    const ProtobufCMessage *in_msg,
    KeySpecHelper& ks_o,
    size_t depth_o)
{
  const ProtobufCFieldDescriptor *fd = nullptr;
  const ProtobufCMessage *final_msg = nullptr;
  const ProtobufCMessage *out_msg = nullptr;
  void *msg_ptr = nullptr;
  protobuf_c_boolean is_dptr = false;
  size_t off = 0;
  size_t index = depth_i;

  final_msg = in_msg;
  while (index < depth_o) {

    auto path_entry = ks_o.get_path_entry(index);
    RW_ASSERT(path_entry);

    auto element_id = rw_keyspec_entry_get_element_id(path_entry);
    RW_ASSERT(element_id);

    //Get the msg corresponding to this entry.
    size_t count = protobuf_c_message_get_field_desc_count_and_offset(
        final_msg, element_id->element_tag, &fd, &msg_ptr, &off, &is_dptr);

    if (!count) {
      break;
    }
    RW_ASSERT(fd->type == PROTOBUF_C_TYPE_MESSAGE);
    if (is_dptr) {
      final_msg = *((ProtobufCMessage **)msg_ptr);
    } else {
      final_msg = (ProtobufCMessage *)msg_ptr;
    }
    // Compare keyvalues if any.
    if (element_id->element_type > RW_SCHEMA_ELEMENT_TYPE_LISTY_0) {
      RW_ASSERT(fd->label == PROTOBUF_C_LABEL_REPEATED);
      unsigned i;
      for (i = 0; i < count; i++) { //Find the matching message.
        if (is_dptr) {
          final_msg = *((ProtobufCMessage **)msg_ptr + i);
        } else {
          final_msg = (ProtobufCMessage *)(((char *)msg_ptr + (off * i)));
        }
        if (compare_path_entry_and_message(instance, path_entry, final_msg, false)) {
          break;
        }
      }
      if (i == count) {
        // No messages matched.
        break;
      }
    }
    index++;
  }

  if (index == depth_o) {
    // return the msg corresponding to the keyspec.
    out_msg = final_msg;
  }

  return out_msg;
}

static ProtobufCMessage* keyspec_reroot_shallower(
    rw_keyspec_instance_t *instance,
    const rw_yang_pb_schema_t* schema,
    KeySpecHelper& ks_i,
    size_t depth_i,
    const ProtobufCMessage *in_msg,
    KeySpecHelper& ks_o,
    size_t depth_o)
{
  // One of in_msg or schema can be null, but not both
  const ProtobufCFieldDescriptor *fd = nullptr;
  ProtobufCMessage *final_msg = nullptr;
  ProtobufCMessage *out_msg = nullptr;

  if ((nullptr == schema) && (nullptr == in_msg)) {
    // no way to find a message descriptor
    return nullptr;
  }

  const rw_yang_pb_msgdesc_t* ypbc_msgdesc;
  std::stack<const rw_yang_pb_msgdesc_t*> msg_stack;

  if (in_msg) {
    ypbc_msgdesc = in_msg->descriptor->ypbc_mdesc;
  } else {
    // Find the specific ks msg descriptor
    rw_keyspec_path_t *unused = 0;
    rw_status_t rs = rw_keyspec_path_find_msg_desc_schema (
        ks_i.get(), instance, schema, &ypbc_msgdesc, &unused);

    if (RW_STATUS_SUCCESS != rs) {
      return nullptr;
    }
    RW_ASSERT(RW_YANGPBC_MSG_TYPE_SCHEMA_PATH == ypbc_msgdesc->msg_type);

    // The desc of the message to generate is in the union
    ypbc_msgdesc = (const rw_yang_pb_msgdesc_t*)ypbc_msgdesc->u;

    // This descriptor needs to be in the stack as well
    // Push it in now, so that the rest of the code is generic
    msg_stack.push (ypbc_msgdesc);
  }

  RW_ASSERT(ypbc_msgdesc);

  size_t shallow_d = (depth_i - depth_o);
  for (unsigned i = shallow_d; i > 0; i--) {
    if (ypbc_msgdesc->parent) {
      ypbc_msgdesc = ypbc_msgdesc->parent;
    } else {
      RW_ASSERT(i == 1);
      RW_ASSERT(ks_o.is_module_root());
      ypbc_msgdesc = rw_yang_pb_get_module_root_msgdesc(ypbc_msgdesc);
    }
    RW_ASSERT(ypbc_msgdesc);
    msg_stack.push(ypbc_msgdesc);
  }

  RW_ASSERT(!msg_stack.empty());
  size_t index = depth_o - 1;
  ProtobufCMessage *temp_msg = nullptr;

  while (!msg_stack.empty()) {
    ypbc_msgdesc = msg_stack.top();
    msg_stack.pop();

    if (!final_msg) { // The message that the out_ks corresponds to.

      final_msg = (ProtobufCMessage *)malloc(ypbc_msgdesc->pbc_mdesc->sizeof_message);
      RW_ASSERT(final_msg);

      protobuf_c_message_init(ypbc_msgdesc->pbc_mdesc, final_msg);
      temp_msg = final_msg;
    } else {

      fd = protobuf_c_message_descriptor_get_field(
          temp_msg->descriptor, ypbc_msgdesc->pb_element_tag);
      RW_ASSERT(fd);

      // This function will internally initialize the sub message.
      ProtobufCMessage *inner_msg = nullptr;
      protobuf_c_message_set_field_message(
          instance->pbc_instance, temp_msg, fd, &inner_msg);
      RW_ASSERT(inner_msg);
      temp_msg = inner_msg;
    }

    // Populate the keyvalues if any from the in keyspec.
    auto path_entry = ks_i.get_path_entry(index++);
    if (path_entry) {
      if (rw_keyspec_entry_num_keys(path_entry)) {
        rw_status_t s = path_entry_copy_keys_to_msg(instance, path_entry, temp_msg);
        RW_ASSERT(s == RW_STATUS_SUCCESS);
      }
    }
  }

  // Finally copy the in_msg into the new_message.
  if (in_msg) {

    fd = protobuf_c_message_descriptor_get_field(
        temp_msg->descriptor, in_msg->descriptor->ypbc_mdesc->pb_element_tag);
    RW_ASSERT(fd);

    ProtobufCMessage *inner_msg = nullptr;
    protobuf_c_message_set_field_message(instance->pbc_instance, temp_msg, fd, &inner_msg);

    RW_ASSERT(inner_msg);
    protobuf_c_message_copy_usebody(instance->pbc_instance, in_msg, inner_msg);
  }

  out_msg = final_msg;
  return out_msg;
}

ProtobufCMessage* rw_keyspec_path_schema_reroot(
    const rw_keyspec_path_t *in_k,
    rw_keyspec_instance_t* instance,
    const rw_yang_pb_schema_t* schema,
    const rw_keyspec_path_t *out_k)
{
  RW_ASSERT(is_good_ks(in_k));
  RW_ASSERT(schema);
  RW_ASSERT(is_good_ks(out_k));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, reroot);

  ProtobufCMessage *out_msg = nullptr;

  KeySpecHelper ks_i(in_k);
  KeySpecHelper ks_o(out_k);

  size_t depth_i = ks_i.get_depth();
  size_t depth_o = ks_o.get_depth();

  if (!depth_i || !depth_o) {
    return nullptr;
  }

  if (depth_i < depth_o) {
    // The new keyspec is deeper. There is no deeper message, since there is no in msg
    return nullptr;
  }

  // Check whether the keyspecs are for the same schema with varying depths.
  if (!rw_keyspec_path_is_prefix_match_impl(instance, ks_i, ks_o, depth_i, depth_o)) {
    KEYSPEC_ERROR(instance, in_k, out_k, "Keyspecs are not prefix match");
    return nullptr;
  }

  out_msg = keyspec_reroot_shallower(
      instance, schema, ks_i, depth_i, nullptr, ks_o, depth_o);

  return out_msg;
}

ProtobufCMessage* rw_keyspec_path_reroot(
    const rw_keyspec_path_t *in_k,
    rw_keyspec_instance_t* instance,
    const ProtobufCMessage *in_msg,
    const rw_keyspec_path_t *out_k)
{
  ProtobufCMessage *out_msg = nullptr;
  RW_ASSERT(is_good_ks(in_k));
  RW_ASSERT(is_good_ks(out_k));
  RW_ASSERT(in_msg);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, reroot);

  KeySpecHelper ks_i(in_k);
  KeySpecHelper ks_o(out_k);

  // Assertion to check that the in_ks is not wildcarded..
  RW_ASSERT(!ks_i.has_wildcards());

  size_t depth_i = ks_i.get_depth();
  size_t depth_o = ks_o.get_depth();

  // Check whether the keyspecs are for the same schema with varying depths.
  if (!rw_keyspec_path_is_prefix_match_impl(instance, ks_i, ks_o, depth_i, depth_o)) {
    KEYSPEC_ERROR(instance, in_k, out_k, "Keyspecs are not prefix match");
    return out_msg;
  }

  if (depth_i == depth_o) {
    return protobuf_c_message_duplicate(
        instance->pbc_instance, in_msg, in_msg->descriptor);
  }

  if (depth_i < depth_o) {
    // The new keyspec is deeper. Extract the message corresponding to the
    // keyspec from the original message.
    const ProtobufCMessage *target =
        keyspec_find_deeper(instance, depth_i, in_msg, ks_o, depth_o);

    if (target) {
      out_msg = protobuf_c_message_duplicate(
          instance->pbc_instance, target, target->descriptor);
    }
  }
  else {
    // The new keyspec is shallower than the original message.
    // Create a new message.
    out_msg = keyspec_reroot_shallower(
        instance, nullptr, ks_i, depth_i, in_msg, ks_o, depth_o);
  }

  return out_msg;
}

static bool add_key_to_path_entry(
    rw_keyspec_instance_t* instance,
    ProtobufCMessage *path_entry,
    unsigned index,
    char *key,
    size_t key_len)
{
  const ProtobufCFieldDescriptor* fd = nullptr;
  const ProtobufCFieldDescriptor* key_fd = nullptr;
  ProtobufCMessage *key_msg = nullptr;

  fd = protobuf_c_message_descriptor_get_field(
      path_entry->descriptor, RW_SCHEMA_TAG_ELEMENT_KEY_START+index);

  RW_ASSERT(fd);
  RW_ASSERT(fd->type == PROTOBUF_C_TYPE_MESSAGE);
  const ProtobufCMessageDescriptor *mdesc = (const ProtobufCMessageDescriptor *)fd->descriptor;

  RW_ASSERT(mdesc);
  RW_ASSERT(mdesc->n_fields == 2);
  key_fd = &mdesc->fields[1];

  protobuf_c_message_set_field_message(
      instance->pbc_instance, path_entry, fd, &key_msg);

  if (!key_msg) {
    return false;
  }

  if (protobuf_c_message_set_field_text_value(
          instance->pbc_instance, key_msg, key_fd, key, key_len)) {
    return true;
  }

  return false;
}

rw_keyspec_entry_t* rw_keyspec_entry_create_from_proto(
    rw_keyspec_instance_t* instance,
    const ProtobufCMessage *msg)
{
  const ProtobufCFieldDescriptor *key_fd = nullptr;
  RW_ASSERT(msg);

  const rw_yang_pb_msgdesc_t *ypbc_msgdesc = msg->descriptor->ypbc_mdesc;
  RW_ASSERT(ypbc_msgdesc);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, entry, create_from_proto);

  // Clone a path entry message from the global static object.
  auto path_entry = (rw_keyspec_entry_t *)protobuf_c_message_duplicate(
      instance->pbc_instance, &ypbc_msgdesc->schema_entry_value->base,
      ypbc_msgdesc->schema_entry_ypbc_desc->pbc_mdesc);

  if (!path_entry) {
    return NULL;
  }

  // Populate key values if any from the message.
  auto elem_id =  rw_keyspec_entry_get_element_id(path_entry);
  RW_ASSERT(elem_id);

  if (elem_id->element_type > RW_SCHEMA_ELEMENT_TYPE_LISTY_0) {

    auto num_keys = rw_keyspec_entry_num_keys(path_entry);
    for (unsigned i = 0; i < num_keys; i++) {

      auto key_tag = get_key_tag_from_path_entry_desc(
          ypbc_msgdesc->schema_entry_value, i);

      size_t off = 0;
      void *key_ptr = nullptr;
      protobuf_c_boolean is_dptr = false;
      size_t count = protobuf_c_message_get_field_desc_count_and_offset(
          msg, key_tag, &key_fd, &key_ptr, &off, &is_dptr);

      if (!count) {
        // Key not present in the message. What to do??
        continue;
      }

      RW_ASSERT(key_ptr);
      char key_str[KEY_STR_LEN];
      size_t key_str_len = sizeof(key_str);
      // Get the key string from the message.
      if (!protobuf_c_field_get_text_value(
              instance->pbc_instance, key_fd, key_str, &key_str_len, key_ptr)) {
        continue;
      }
      //copy the key to the pathentry.
      if (!key_str_len) {
        key_str_len = strlen(key_str);
      }
      add_key_to_path_entry(instance, &path_entry->base, i, key_str, key_str_len);
    }
  }

  return path_entry;
}

// Used only in this file. If needed expose.
static const char*
get_yang_name_from_pb_name(const ProtobufCMessageDescriptor *mdesc,
                           const char* pb_fname)
{
  const rw_yang_pb_msgdesc_t *ypbc =  mdesc->ypbc_mdesc;

  if ((nullptr == ypbc) || (nullptr == ypbc->ypbc_flddescs))  {
    return nullptr;
  }

  for (size_t i = 0; i < ypbc->num_fields; i++) {
    const rw_yang_pb_flddesc_t *y_desc = &ypbc->ypbc_flddescs[i];
    if (!strcmp(y_desc->pbc_fdesc->name, pb_fname)) {
      return y_desc->yang_node_name;
    }
  }

  return nullptr;
}

static void print_buffer_add_key_values(
    rw_keyspec_instance_t* instance,
    std::ostringstream& oss,
    const rw_yang_pb_msgdesc_t *ypbc_msgdesc,
    const rw_keyspec_entry_t *path_entry)
{
  const ProtobufCFieldDescriptor *fd = nullptr;
  const ProtobufCMessageDescriptor *kdesc = nullptr;
  rw_keyspec_entry_t *con_pe = nullptr;

  uint32_t n_keys = rw_keyspec_entry_num_keys(path_entry);

  if (IS_PATH_ENTRY_GENERIC(path_entry)) {
    // Convert into concerete type to extract key values if any.
    con_pe = convert_generic_pe_to_concerete(instance, path_entry, ypbc_msgdesc);
    if (!con_pe) {
      return;
    }
    path_entry = con_pe;
  }

  auto elem_id = rw_keyspec_entry_get_element_id(path_entry);

  for (unsigned k = 0; k < n_keys; k++) {

    fd = protobuf_c_message_descriptor_get_field(
        path_entry->base.descriptor, RW_SCHEMA_TAG_ELEMENT_KEY_START+k);

    if(!fd) {
      break;
    }

    kdesc = (const ProtobufCMessageDescriptor *)fd->descriptor;
    RW_ASSERT(kdesc);
    RW_ASSERT(kdesc->n_fields == 2);

    char key_str[KEY_STR_LEN];
    size_t key_len = KEY_STR_LEN;
    bool rc = get_key_string_from_path_entry(instance, path_entry, k, key_str, &key_len);

    if (!rc) {
      continue; // Dont print wildcarded keys.
    }

    oss << "[";

    if (elem_id->element_type == RW_SCHEMA_ELEMENT_TYPE_LEAF_LIST) {
      oss << ".=";
    } else {
      const char* ynode_name = get_yang_name_from_pb_name(
          ypbc_msgdesc->pbc_mdesc, kdesc->fields[1].name);
      RW_ASSERT(ynode_name);
      oss <<  ypbc_msgdesc->module->prefix <<":" << ynode_name << "=";
    }
    oss << "\'" << key_str << "\'";
    oss << "]";
  }

  if (con_pe) {
    protobuf_c_message_free_unpacked(instance->pbc_instance, &con_pe->base);
  }

  return;
}

rw_status_t rw_keyspec_path_deserialize_msg_ks_pair (
    rw_keyspec_instance_t* instance,
    const rw_yang_pb_schema_t* schema,
    const ProtobufCBinaryData* msg_b,
    const ProtobufCBinaryData* keyspec_b,
    ProtobufCMessage** msg,
    rw_keyspec_path_t** ks)
{
  RW_ASSERT(schema);
  RW_ASSERT(msg_b);
  RW_ASSERT(keyspec_b);
  RW_ASSERT(msg);
  RW_ASSERT(ks);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  rw_keyspec_path_t *gen_ks = rw_keyspec_path_create_with_binpath_binary_data(
      instance, keyspec_b);

  if (nullptr == gen_ks) {
    return RW_STATUS_FAILURE;
  }

  rw_status_t rs = rw_keyspec_path_find_spec_ks (gen_ks, instance, schema, ks);
  RW_ASSERT(RW_STATUS_SUCCESS == rs);

  rw_keyspec_path_free (gen_ks, instance);
  gen_ks = nullptr;

  ProtobufCMessage *tmp_msg_ptr = (ProtobufCMessage *) *ks;
  RW_ASSERT(tmp_msg_ptr);
  RW_ASSERT(tmp_msg_ptr->descriptor);

  const rw_yang_pb_msgdesc_t* ypbc_desc = tmp_msg_ptr->descriptor->ypbc_mdesc;
  RW_ASSERT(ypbc_desc);
  RW_ASSERT(RW_YANGPBC_MSG_TYPE_SCHEMA_PATH == ypbc_desc->msg_type);

  // Get to the messages message definition
  ypbc_desc = (const rw_yang_pb_msgdesc_t* )ypbc_desc->u;
  RW_ASSERT(ypbc_desc);

  *msg = protobuf_c_message_unpack(
      instance->pbc_instance, ypbc_desc->pbc_mdesc, msg_b->len, msg_b->data);

  if (nullptr == *msg) {
    return RW_STATUS_FAILURE;
  }

  return RW_STATUS_SUCCESS;
}

static void print_buffer_add_key_values(
    rw_keyspec_instance_t* instance,
    std::ostringstream& oss,
    const rw_keyspec_entry_t *path_entry)
{
  const ProtobufCFieldDescriptor *fd = nullptr;
  size_t off = 0;
  void *key_ptr = nullptr;
  protobuf_c_boolean is_dptr = false;

  uint32_t n_keys = rw_keyspec_entry_num_keys(path_entry);

  oss << "[";
  for (unsigned k = 0; k < n_keys; k++) {

    char key_str[KEY_STR_LEN];
    size_t key_len = KEY_STR_LEN;
    bool rc = get_key_string_from_path_entry(instance, path_entry, k, key_str, &key_len);

    if (rc) {
      if (k) {
        oss << ",";
      }
      oss << "Key" << k+1 << "=" << "\'" << key_str << "\'";
    } else {
      // Keys can be present in unknown fields for generic keyspecs
      size_t count = protobuf_c_message_get_field_desc_count_and_offset(
          &path_entry->base, RW_SCHEMA_TAG_ELEMENT_KEY_START+k,
          &fd, &key_ptr, &off, &is_dptr);

      if (count) {
        RW_ASSERT(key_ptr);
        ProtobufCMessage *msg = (ProtobufCMessage *)key_ptr;
        if (protobuf_c_message_unknown_get_count(msg) > 0) {
          if (k) {
            oss << ",";
          }
          auto unkf = protobuf_c_message_unknown_get_index(msg, 0);
          RW_ASSERT(unkf);

          oss << "Unkey" << k+1 << "=\'Tag:" << unkf->base_.tag << "\'";
        }
      }
    }
  }

  oss << "]";
}

static bool create_xpath_string_from_ks(
    rw_keyspec_instance_t* instance,
    KeySpecHelper& ks,
    size_t depth,
    const rw_yang_pb_schema_t *schema,
    std::ostringstream &oss)
{
  bool ret_val = false;

  if (!depth) {
    RW_ASSERT(ks.is_root());
    oss << "/";
    return true;
  }

  const rw_yang_pb_module_t *ypbc_module = nullptr;
  const rw_yang_pb_msgdesc_t *top_msgdesc = nullptr;

  auto path_entry = ks.get_path_entry(0);
  RW_ASSERT(path_entry);

  auto element_id = rw_keyspec_entry_get_element_id(path_entry);
  RW_ASSERT(element_id);

  // find the top level message that the ks corresponds to.
  for (unsigned m = 0; m < schema->module_count; m++) {
    ypbc_module = schema->modules[m];
    auto data_module = ypbc_module->data_module;
    if (data_module) {
      unsigned i = 0;
      for (i = 0; i < data_module->child_msg_count; i++) {
        top_msgdesc = data_module->child_msgs[i];
        if (top_msgdesc->pb_element_tag == element_id->element_tag) {
          break;
        }
      }
      if (i != data_module->child_msg_count) {
        break;
      }
    }

    auto rpci_module = ypbc_module->rpci_module;
    if (rpci_module) {
      unsigned i = 0;
      for (i = 0; i < rpci_module->child_msg_count; i++) {
        top_msgdesc = rpci_module->child_msgs[i];
        if (top_msgdesc->pb_element_tag == element_id->element_tag) {
          break;
        }
      }
      if (i != rpci_module->child_msg_count) {
        break;
      }
    }

    auto rpco_module = ypbc_module->rpco_module;
    if (rpco_module) {
      unsigned i = 0;
      for (i = 0; i < rpco_module->child_msg_count; i++) {
        top_msgdesc = rpco_module->child_msgs[i];
        if (top_msgdesc->pb_element_tag == element_id->element_tag) {
          break;
        }
      }
      if (i != rpco_module->child_msg_count) {
        break;
      }
    }

    auto notif_module = ypbc_module->notif_module;
    if (notif_module) {
      unsigned i = 0;
      for (i = 0; i < notif_module->child_msg_count; i++) {
        top_msgdesc = notif_module->child_msgs[i];
        if (top_msgdesc->pb_element_tag == element_id->element_tag) {
          break;
        }
      }
      if (i != notif_module->child_msg_count) {
        break;
      }
    }
    // else try searching other modules.
    top_msgdesc = nullptr;
  }

  if (top_msgdesc) {

    size_t index = 1;
    oss << "/" << top_msgdesc->module->prefix << ":"
        << top_msgdesc->yang_node_name;

    if (element_id->element_type > RW_SCHEMA_ELEMENT_TYPE_LISTY_0) {
      // Include key values;
      print_buffer_add_key_values(instance, oss, top_msgdesc, path_entry);
    }

    auto curr_msgdesc = top_msgdesc;
    while (index < depth) {

      auto path_entry = ks.get_path_entry(index);
      RW_ASSERT(path_entry);

      auto element_id = rw_keyspec_entry_get_element_id(path_entry);
      RW_ASSERT(element_id);

      // Get the child message corresponding to the path entry.
      unsigned i;

      if (element_id->element_type == RW_SCHEMA_ELEMENT_TYPE_LEAF) {
        RW_ASSERT(index == (depth-1));
        // Search the yang leaf-nodes.
        for (i = 0; curr_msgdesc && i < curr_msgdesc->num_fields; i++) {
          if (curr_msgdesc->ypbc_flddescs[i].pbc_fdesc->id
              == element_id->element_tag) {
            break;
          }
        }
        if (!curr_msgdesc || (i == curr_msgdesc->num_fields)) {
          // No leaf node matched. Just op a non-texty entry
          oss << "/" << element_id->system_ns_id << ":"
              << element_id->element_tag;
        } else {
          oss << "/" << curr_msgdesc->ypbc_flddescs[i].module->prefix << ":"
              << curr_msgdesc->ypbc_flddescs[i].yang_node_name;
        }

      } else {
        const rw_yang_pb_msgdesc_t *child_msgdesc = nullptr;
        for (i = 0; curr_msgdesc && i < curr_msgdesc->child_msg_count; i++) {
          child_msgdesc = curr_msgdesc->child_msgs[i];
          if (child_msgdesc->pb_element_tag == element_id->element_tag) {
            break;
          }
        }
        // No child message matched this path_entry.
        if (!curr_msgdesc || (i == curr_msgdesc->child_msg_count)) {

          oss << "/" << element_id->system_ns_id << ":"
              << element_id->element_tag;

          if (rw_keyspec_entry_num_keys(path_entry)) {
            print_buffer_add_key_values(instance, oss, path_entry);
          }
          curr_msgdesc = nullptr;
        } else {

          oss << "/" << child_msgdesc->module->prefix << ":"
              << child_msgdesc->yang_node_name;

          if (rw_keyspec_entry_num_keys(path_entry)) {
            // Include key values.
            print_buffer_add_key_values(instance, oss, child_msgdesc, path_entry);
          }
          curr_msgdesc = child_msgdesc;
        }
      }
      index++;
    }

    if (index == depth) {
      // Perfect.
      ret_val = true;
    }
  }

  return ret_val;
}


static std::string generate_ks_print_buffer_from_schema(
    rw_keyspec_instance_t* instance,
    KeySpecHelper& ks,
    const rw_yang_pb_schema_t *schema)
{
  std::ostringstream oss;

  bool is_rooted = ks.is_rooted();
  // Let us not try walking the schema for an unrooted keyspec.
  if (!is_rooted) {
    return oss.str();
  }

  RwSchemaCategory cat = ks.get_category();
  switch(cat) {
    case RW_SCHEMA_CATEGORY_DATA:
      oss << "D,";
      break;
    case RW_SCHEMA_CATEGORY_CONFIG:
      oss << "C,";
      break;
    case RW_SCHEMA_CATEGORY_RPC_INPUT:
      oss << "I,";
      break;
    case RW_SCHEMA_CATEGORY_RPC_OUTPUT:
      oss << "O,";
      break;
    case RW_SCHEMA_CATEGORY_NOTIFICATION:
      oss << "N,";
      break;
    case RW_SCHEMA_CATEGORY_ANY:
    default:
      oss << "A,";
  }

  size_t depth = ks.get_depth();
  if (!create_xpath_string_from_ks(instance, ks, depth, schema, oss)) {
    return std::string();
  }

  return oss.str();
}

static std::string generate_ks_print_buffer_raw(
    rw_keyspec_instance_t* instance,
    KeySpecHelper& ks)
{
  // Try normal plain printing of keyspec.
  std::ostringstream oss;
  bool is_rooted = ks.is_rooted();
  size_t depth = ks.get_depth();

  if (!depth) {
    RW_ASSERT(ks.is_root());
  }

  // Output category
  RwSchemaCategory cat = ks.get_category();
  switch(cat) {
    case RW_SCHEMA_CATEGORY_DATA:
      oss << "D,";
      break;
    case RW_SCHEMA_CATEGORY_CONFIG:
      oss << "C,";
      break;
    case RW_SCHEMA_CATEGORY_RPC_INPUT:
      oss << "I,";
      break;
    case RW_SCHEMA_CATEGORY_RPC_OUTPUT:
      oss << "O,";
      break;
    case RW_SCHEMA_CATEGORY_NOTIFICATION:
      oss << "N,";
      break;
    case RW_SCHEMA_CATEGORY_ANY:
    default:
      oss << "A,";
  }

  if (is_rooted) {
    oss << "/";
  }

  for (unsigned i = 0; i < depth; i++) {

    auto path_entry = ks.get_path_entry(i);
    RW_ASSERT(path_entry);

    auto element_id = rw_keyspec_entry_get_element_id(path_entry);
    RW_ASSERT(element_id);

    oss << element_id->system_ns_id << ":" << element_id->element_tag;

    if (rw_keyspec_entry_num_keys(path_entry)) {
      print_buffer_add_key_values(instance, oss, path_entry);
    }

    if (i != (depth-1)) {
      oss << "/";
    }
  }

  return oss.str();
}

static void ltruncate_keystr(const char* keystr, char *buf, int buflen)
{
  int src_len = strlen(keystr);
  int src_pos = 0;

  // buflen should be atleast 3
  // Skip first 2 chars
  *buf++ = keystr[0];
  *buf++ = keystr[1];
  buflen -= 2;
  if (keystr[2] == '/') {
    // Rooted, copy the / too
    *buf++ = keystr[2];
    buflen--;
  }

  // Calculate the truncation positions
  // reserve 3 for '...' and 1 for null
  if (buflen <  4) {
    // Not enough buffer, return only the first few
    *buf = '\0';
    return;
  }

  strcpy(buf, "...");
  buf += 3;
  buflen -= 3;

  src_pos = src_len - buflen + 1;

  memcpy(buf, keystr + src_pos, buflen - 1);
  buf[buflen-1] = '\0';
}

int rw_keyspec_path_get_print_buffer(
   const rw_keyspec_path_t *k,
   rw_keyspec_instance_t* instance,
   const rw_yang_pb_schema_t *schema,
   char *buf,
   size_t buflen)
{
  int ret_val = -1;
  RW_ASSERT(k);
  RW_ASSERT(buf);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, get_print_buff);

  // Perform sanity checks.
  if (!buflen) {
    return ret_val;
  }

  if (buflen < 3) {
    return ret_val;
  }

  if (!schema) {
    auto ypbc_msgdesc = k->base.descriptor->ypbc_mdesc;
    if (ypbc_msgdesc) {
      RW_ASSERT(ypbc_msgdesc->module);
      schema = ypbc_msgdesc->module->schema;
    }
  }

  KeySpecHelper ks(k);

  // Check if the str_path of the keyspec is present.
  if (ks.has_strpath()) {

    const char *strpath = ks.get_strpath();
    size_t len = strlen(strpath);
    if (len < (buflen - 1)) {
      memcpy(buf, strpath, len);
      buf[len] = '\0';
      ret_val = len;
    } else {
      memcpy(buf, strpath, (buflen-1));
      buf[(buflen-1)] = '\0';
      ret_val = buflen-1;
    }

    return ret_val;
  }

  // If we have schema, then generate a better readable print buffer.
  std::string keystr;
  if (schema) {
    keystr = generate_ks_print_buffer_from_schema(instance, ks, schema);
  }

  if (keystr.empty()) {
    // Try a normal plain print
    keystr = generate_ks_print_buffer_raw(instance, ks);
  }

  if (keystr.empty()) {
    // Both methods failed
    return -1;
  }

  size_t len = keystr.size();
  if (len < buflen) {
    memcpy(buf, keystr.c_str(), len);
    buf[len] = '\0';
    ret_val = len;
  } else {
    memcpy(buf, keystr.c_str(), buflen - 1);
    buf[buflen - 1] = '\0';
    ret_val = buflen - 1;
  }

  return ret_val;
}

int rw_keyspec_path_get_print_buffer_ltruncate(
   const rw_keyspec_path_t *k,
   rw_keyspec_instance_t* instance,
   const rw_yang_pb_schema_t *schema,
   char *buf,
   size_t buflen)
{
  int ret_val = -1;
  RW_ASSERT(k);
  RW_ASSERT(buf);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, get_print_buff);

  // Perform sanity checks.
  if (!buflen) {
    return ret_val;
  }

  if (buflen < 3) {
    return ret_val;
  }

  if (!schema) {
    auto ypbc_msgdesc = k->base.descriptor->ypbc_mdesc;
    if (ypbc_msgdesc) {
      RW_ASSERT(ypbc_msgdesc->module);
      schema = ypbc_msgdesc->module->schema;
    }
  }

  KeySpecHelper ks(k);

  // Check if the str_path of the keyspec is present.
  if (ks.has_strpath()) {

    const char *strpath = ks.get_strpath();
    size_t len = strlen(strpath);
    if (len < (buflen - 1)) {
      memcpy(buf, strpath, len);
      buf[len] = '\0';
      ret_val = len;
    } else {
      ltruncate_keystr(strpath, buf, buflen);
      ret_val = strlen(buf);
    }

    return ret_val;
  }

  // If we have schema, then generate a better readable print buffer.
  std::string keystr;
  if (schema) {
    keystr = generate_ks_print_buffer_from_schema(instance, ks, schema);
  }

  if (keystr.empty()) {
    // Try a normal plain print
    keystr = generate_ks_print_buffer_raw(instance, ks);
  }

  if (keystr.empty()) {
    // Both methods failed
    return -1;
  }

  size_t len = keystr.size();
  if (len < buflen) {
    memcpy(buf, keystr.c_str(), len);
    buf[len] = '\0';
    ret_val = len;
  } else {
    ltruncate_keystr(keystr.c_str(), buf, buflen);
    ret_val = strlen(buf);
  }

  return ret_val;
}

int rw_keyspec_path_get_new_print_buffer(
   const rw_keyspec_path_t *k,
   rw_keyspec_instance_t* instance,
   const rw_yang_pb_schema_t *schema,
   char **buf)
{
  int ret_val = -1;
  RW_ASSERT(k);
  RW_ASSERT(buf);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, get_print_buff);

  *buf = NULL;

  if (!schema) {
    auto ypbc_msgdesc = k->base.descriptor->ypbc_mdesc;
    if (ypbc_msgdesc) {
      RW_ASSERT(ypbc_msgdesc->module);
      schema = ypbc_msgdesc->module->schema;
    }
  }

  KeySpecHelper ks(k);

  // Check if the str_path of the keyspec is present.
  if (ks.has_strpath()) {

    const char *strpath = ks.get_strpath();
    ret_val = strlen(strpath);
    *buf = strdup(strpath);

    return ret_val;
  }

  // If we have schema, then generate a better readable print buffer.
  std::string keystr;
  if (schema) {
    keystr = generate_ks_print_buffer_from_schema(instance, ks, schema);
  }

  if (keystr.empty()) {
    // Try a normal plain print
    keystr = generate_ks_print_buffer_raw(instance, ks);
  }

  if (keystr.empty()) {
    // Both methods failed
    return -1;
  }

  ret_val = keystr.size();
  *buf = strdup(keystr.c_str());

  return ret_val;
}

rw_status_t rw_keyspec_path_get_strpath(
  rw_keyspec_path_t* k,
  rw_keyspec_instance_t* instance,
  const rw_yang_pb_schema_t* schema,
  const char** str_path)
{
  rw_status_t status = RW_STATUS_FAILURE;

  RW_ASSERT(k);
  RW_ASSERT(!*str_path);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KeySpecHelper ks(k);

  if (!ks.has_strpath()) {
    if (rw_keyspec_path_update_strpath(k, instance, schema) != RW_STATUS_SUCCESS) {
      return status;
    }
  }

  RW_ASSERT(ks.has_strpath());
  *str_path = ks.get_strpath();

  return RW_STATUS_SUCCESS;
}

rw_status_t rw_keyspec_path_update_strpath(
  rw_keyspec_path_t* k,
  rw_keyspec_instance_t* instance,
  const rw_yang_pb_schema_t* schema)
{
  rw_status_t status = RW_STATUS_FAILURE;
  RW_ASSERT(is_good_ks(k));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KeySpecHelper ks(k);

  if (ks.has_strpath()) {
    if (rw_keyspec_path_delete_strpath(k, instance) != RW_STATUS_SUCCESS) {
      return status;
    }
  }

  char *str_buf = NULL;

  int len = rw_keyspec_path_get_new_print_buffer(k, instance, schema, &str_buf);
  if (len == -1) {
    return status;
  }

  auto str_fd = protobuf_c_message_descriptor_get_field(
      k->base.descriptor, RW_SCHEMA_TAG_KEYSPEC_STRPATH);
  RW_ASSERT(str_fd);

  protobuf_c_boolean ok = protobuf_c_message_set_field_text_value(
      instance->pbc_instance, &k->base, str_fd, str_buf, len);
  free(str_buf);

  if (!ok) {
    return status;
  }

  return RW_STATUS_SUCCESS;
}


static int rw_keyspec_path_get_common_prefix_index(
    rw_keyspec_instance_t* instance,
    KeySpecHelper& ks1,
    KeySpecHelper& ks2,
    size_t depth_1,
    size_t depth_2)
{
  int index = -1;

  if (!depth_1 || !depth_2) {
    return index;
  }

  size_t min_depth = (depth_1 < depth_2) ? depth_1 : depth_2;

  for (index = 0; index < (int)min_depth; index++) {
    auto path_entry1 = ks1.get_path_entry(index);
    RW_ASSERT(path_entry1);

    auto path_entry2 = ks2.get_path_entry(index);
    RW_ASSERT(path_entry2);

    switch (get_path_entry_type(instance, path_entry1)) {
      case PE_CONTAINER:
      case PE_LISTY_WITH_ALL_KEYS:
        break;
      case PE_LISTY_WITH_ALL_WC:
      case PE_LISTY_WITH_WC_AND_KEYS:
        goto ret;
      default:
        RW_ASSERT_NOT_REACHED();  
    }

    switch (get_path_entry_type(instance, path_entry2)){
      case PE_CONTAINER:
      case PE_LISTY_WITH_ALL_KEYS:
        break;
      case PE_LISTY_WITH_ALL_WC:
      case PE_LISTY_WITH_WC_AND_KEYS:
        goto ret;
      default:
        RW_ASSERT_NOT_REACHED();  
    }
    
    if (!protobuf_c_message_is_equal_deep(
            instance->pbc_instance, &path_entry1->base, &path_entry2->base)) {
      break;
    }
  }

ret:  
  index -= 1;
  return index;
}

static rw_keyspec_path_t* create_parent_keyspec(
    rw_keyspec_instance_t* instance,
    const rw_keyspec_path_t *ks,
    const ProtobufCMessage *msg,
    size_t parent_index)
{
  const rw_yang_pb_msgdesc_t* current = nullptr;

  current = msg->descriptor->ypbc_mdesc;
  RW_ASSERT(current);

  for (unsigned i = 0; i < parent_index; i++) {
    current = current->parent;
    RW_ASSERT(current);
  }

  // The Generic path entry and the unknown's will be deleted.
  rw_keyspec_path_t *out_ks = rw_keyspec_path_create_dup_of_type_trunc(
      ks, instance, current->schema_path_ypbc_desc->pbc_mdesc);

  return (out_ks);
}

static rw_status_t path_entry_copy_keys(
    rw_keyspec_instance_t* instance,
    const ProtobufCMessage *msg,
    rw_keyspec_entry_t *path_entry)
{
  RW_ASSERT(msg);
  RW_ASSERT(path_entry);

  // Need to convert generic ks into concrete for this function to work
  UniquePtrProtobufCMessage<rw_keyspec_entry_t>::uptr_t path_entry_temp;
  rw_keyspec_entry_t* path_entry_save = path_entry;
  const ProtobufCMessageDescriptor *path_entry_mdesc = path_entry->base.descriptor;

  if (path_entry_mdesc == &rw_schema_path_entry__concrete_descriptor.base) {
    // Convert generic path entry to concrete.
    const rw_yang_pb_msgdesc_t* p_ydesc = rw_schema_pbcm_get_entry_msgdesc(
        instance, msg, nullptr);
    RW_ASSERT(p_ydesc); // the message must be concrete and have schema
    RW_ASSERT(p_ydesc->pbc_mdesc);
    path_entry_temp.reset(
      rw_keyspec_entry_create_dup_of_type(path_entry, instance, p_ydesc->pbc_mdesc) );
    path_entry = path_entry_temp.get();
  }

  rw_status_t status = RW_STATUS_SUCCESS;
  uint32_t n_keys = rw_keyspec_entry_num_keys(path_entry);

  for (unsigned i = 0; i < n_keys; i++) {

    const ProtobufCFieldDescriptor *p_kfd = nullptr;
    ProtobufCMessage *p_keym = nullptr;
    protobuf_c_boolean is_dptr = false;
    size_t off = 0;

    size_t count = protobuf_c_message_get_field_desc_count_and_offset(
        &path_entry->base, RW_SCHEMA_TAG_ELEMENT_KEY_START+i,
        &p_kfd, (void **)&p_keym, &off, &is_dptr);

    if (count) {
      continue;
    }

    RW_ASSERT(p_kfd);
    protobuf_c_message_set_field_message(
        instance->pbc_instance, &path_entry->base, p_kfd, &p_keym);
    RW_ASSERT(p_keym);

    const ProtobufCMessageDescriptor *p_kmdesc = p_kfd->msg_desc;
    RW_ASSERT(p_kmdesc);
    RW_ASSERT(p_kmdesc->n_fields == 2);
    uint32_t key_tag = p_kmdesc->fields[1].id; // ATTN: This is bogus - we cannot assume the index here!

    void *m_keyp = nullptr;
    const ProtobufCFieldDescriptor *m_kfd = nullptr;
    count = protobuf_c_message_get_field_desc_count_and_offset(msg,
                key_tag, &m_kfd, &m_keyp, &off, &is_dptr);
    RW_ASSERT(count);

    char m_key[1024];
    size_t m_key_len = sizeof(m_key);
    bool ret = protobuf_c_field_get_text_value(
        instance->pbc_instance, m_kfd, m_key, &m_key_len, m_keyp);
    if (!ret) {
      return RW_STATUS_FAILURE;
    }

    protobuf_c_boolean ok = protobuf_c_message_set_field_text_value(
        instance->pbc_instance, p_keym, &p_kmdesc->fields[1], m_key, m_key_len);
    if (!ok) {
      return RW_STATUS_FAILURE;
    }
  }

  if (   RW_STATUS_SUCCESS == status
      && path_entry_save != path_entry) {
    protobuf_c_message_free_unpacked_usebody(instance->pbc_instance, &path_entry_save->base);
    // Here the path-entry that we freed is generic, so it is fine to call init.
    protobuf_c_message_init(path_entry_mdesc, &path_entry_save->base);
    protobuf_c_boolean ok = protobuf_c_message_copy_usebody(
        instance->pbc_instance, &path_entry->base, &path_entry_save->base);
    if (!ok) {
      status = RW_STATUS_FAILURE;
    }
  }

  return status;
}

static rw_status_t path_entry_copy_keys(
    rw_keyspec_instance_t* instance,
    const rw_keyspec_entry_t* in_entry,
    rw_keyspec_entry_t* out_entry)
{
  rw_status_t status = RW_STATUS_SUCCESS;
  const ProtobufCFieldDescriptor *p_kfd1 = NULL;
  const ProtobufCFieldDescriptor *p_kfd2 = NULL;
  void *p_keym = NULL;
  size_t off = 0;
  protobuf_c_boolean is_dptr = false;

  RW_ASSERT(in_entry);
  RW_ASSERT(out_entry);

  uint32_t n_keys = rw_keyspec_entry_num_keys(in_entry);

  for (unsigned i = 0; i < n_keys; i++) {

    size_t count = protobuf_c_message_get_field_desc_count_and_offset(
        &out_entry->base, RW_SCHEMA_TAG_ELEMENT_KEY_START+i, &p_kfd1,
        (void **)&p_keym, &off, &is_dptr);

    if (count) {
      continue; // This key is already present. Dont overwrite it.
    }

    count = protobuf_c_message_get_field_desc_count_and_offset(
        &in_entry->base, RW_SCHEMA_TAG_ELEMENT_KEY_START+i, &p_kfd2,
        (void **)&p_keym, &off, &is_dptr);

    if (!count) {
      continue; // No key in in_entry.
    }

    RW_ASSERT(p_keym);

    ProtobufCMessage *inner_msg = nullptr;
    auto ok = protobuf_c_message_set_field_message(
        instance->pbc_instance, &out_entry->base, p_kfd1, &inner_msg);
    RW_ASSERT(ok);

    ok = protobuf_c_message_copy_usebody(
        instance->pbc_instance, (ProtobufCMessage *)p_keym, inner_msg);
    RW_ASSERT(ok);
  }

  return status;
}


static rw_status_t keyspec_copy_keys(
    rw_keyspec_instance_t* instance,
    KeySpecHelper& in_ks,
    KeySpecHelper& out_ks,
    size_t depth)
{
  rw_status_t status = RW_STATUS_SUCCESS;

  for (unsigned i = 0; i < depth; i++) {

    auto path_entry1 = in_ks.get_path_entry(i);
    RW_ASSERT(path_entry1);

    auto path_entry2 = out_ks.get_path_entry(i);
    RW_ASSERT(path_entry2);

    rw_keyspec_entry_t *path_entry_o = const_cast<rw_keyspec_entry_t *>(path_entry2);

    auto element_id = rw_keyspec_entry_get_element_id(path_entry1);

    if (element_id->element_type > RW_SCHEMA_ELEMENT_TYPE_LISTY_0) {
      status = path_entry_copy_keys(instance, path_entry1, path_entry_o);
    }
  }

  return status;
}

ProtobufCMessage*
rw_keyspec_path_reroot_and_merge(
    rw_keyspec_instance_t* instance,
    const rw_keyspec_path_t *k1,
    const rw_keyspec_path_t *k2,
    const ProtobufCMessage *msg1,
    const ProtobufCMessage *msg2,
    rw_keyspec_path_t **ks_out)
{
  ProtobufCMessage *out_msg = nullptr;

  RW_ASSERT(is_good_ks(k1));
  RW_ASSERT(is_good_ks(k2));
  RW_ASSERT(msg1);
  RW_ASSERT(msg2);
  RW_ASSERT(!*ks_out);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, reroot_and_merge);

  KeySpecHelper ks1(k1);
  KeySpecHelper ks2(k2);

  size_t depth1 = ks1.get_depth();
  size_t depth2 = ks2.get_depth();

  int c_prefix_idx =
      rw_keyspec_path_get_common_prefix_index(instance, ks1, ks2, depth1, depth2);

  if (c_prefix_idx == -1) {
    KEYSPEC_ERROR(instance, k1, k2, "No common prefix to reroot");
    return out_msg;
  }

  rw_keyspec_path_t *out_ks =
      create_parent_keyspec(instance, ks1.get(), msg1, (depth1-(c_prefix_idx+1)));
  RW_ASSERT(out_ks);

  if (ks1.get_category() != ks2.get_category()) {
    rw_keyspec_path_set_category(out_ks, instance, RW_SCHEMA_CATEGORY_ANY);
  }

  ProtobufCMessage *rr_msg1 = nullptr;
  ProtobufCMessage *rr_msg2 = nullptr;

  size_t depth_o = c_prefix_idx+1;
  KeySpecHelper ks_o(out_ks);

  do {
    // Now reroot msg1 and msg2 to out_ks.
    if (depth_o < depth1) {

      rr_msg1 = keyspec_reroot_shallower(
          instance, nullptr, ks1, depth1, msg1, ks_o, depth_o);

      if (!rr_msg1) {
        KEYSPEC_ERROR(instance, k1, out_ks, "Failed to reroot to shallower depth(%d)", depth_o);
        break;
      }
    } else {
      rr_msg1 = (ProtobufCMessage *)msg1;
    }

    if (depth_o < depth2) {

      rr_msg2 = keyspec_reroot_shallower(
          instance, nullptr, ks2, depth2, msg2, ks_o, depth_o);

      if (!rr_msg2) {
        KEYSPEC_ERROR(instance, k2, out_ks, "Failed to reroot to shallower depth(%d)", depth_o);
        break;
      }
    } else {
      rr_msg2 = (ProtobufCMessage *)malloc(msg2->descriptor->sizeof_message);
      RW_ASSERT(rr_msg2);

      protobuf_c_message_init(msg2->descriptor, rr_msg2);
      if (!protobuf_c_message_copy_usebody(
              instance->pbc_instance, msg2, rr_msg2)) {
        break;
      }
    }

    // Now merge the rerooted messages.
    protobuf_c_boolean ret = protobuf_c_message_merge_new(
        instance->pbc_instance, rr_msg1, rr_msg2);

    if (!ret) {
      KEYSPEC_ERROR(instance, "Merge failed(%p)", rr_msg1->descriptor);
      break;
    }

    out_msg = rr_msg2;
    *ks_out = out_ks;

  } while(0);

  if (rr_msg1 != msg1) {
    protobuf_c_message_free_unpacked(instance->pbc_instance, rr_msg1);
  }

  if (!out_msg) {
    protobuf_c_message_free_unpacked(instance->pbc_instance, rr_msg2);
    rw_keyspec_path_free(out_ks, instance);
  }

  return out_msg;
}


void rw_keyspec_path_reroot_iter_init(
     const rw_keyspec_path_t *in_ks,
     rw_keyspec_instance_t* instance,
     rw_keyspec_path_reroot_iter_state_t *st,
     const ProtobufCMessage *in_msg,
     const rw_keyspec_path_t *out_ks)
{
  RW_ASSERT(is_good_ks(in_ks));
  RW_ASSERT(is_good_ks(out_ks));
  RW_ASSERT(in_msg);
  RW_ASSERT(st);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, reroot_iter);

  keyspec_reroot_iter_state_t *state = NULL;
  state = (keyspec_reroot_iter_state_t *)st;

  state->ks_in    = new KeySpecHelper(in_ks);
  state->ks_out   = new KeySpecHelper(out_ks);
  state->in_msg   = in_msg;
  state->instance = instance;
  state->cur_ks   = NULL;
  state->cur_msg  = NULL;
  state->depth_i  = state->ks_in->get_depth();
  state->depth_o  = state->ks_out->get_depth();

  if (!rw_keyspec_path_is_prefix_match_impl(
          instance, *state->ks_in, *state->ks_out, state->depth_i, state->depth_o)) {
    KEYSPEC_ERROR(instance, in_ks, out_ks, "Keyspecs are not prefix match");
    state->next_state = REROOT_ITER_DONE;
    return;
  }

  state->n_path_ens = 0;
  state->is_out_ks_wc = state->ks_out->has_wildcards();
  state->ks_has_iter_pes = false;

  RW_ASSERT(!state->ks_in->has_wildcards());

  unsigned last_iter_idx =  UINT_MAX;
  unsigned first_iter_idx = UINT_MAX;

  // Let us store the details about each path entries in the out ks.
  for (unsigned i = state->depth_i; i < state->depth_o; i++) {

    auto path_entry = state->ks_out->get_path_entry(i);
    RW_ASSERT(path_entry);

    // Is this path entry listy?
    state->path_entries[state->n_path_ens].is_listy = rw_keyspec_entry_is_listy(path_entry);

    if (state->path_entries[state->n_path_ens].is_listy) {
      state->path_entries[state->n_path_ens].type = get_path_entry_type(instance, path_entry);

      auto pe_type = state->path_entries[state->n_path_ens].type;
      RW_ASSERT(pe_type != PE_CONTAINER);
      // Iteratable path entries are the ones with wildcards and listy_0
      if (pe_type != PE_LISTY_WITH_ALL_KEYS) {
        state->path_entries[state->n_path_ens].is_iteratable = true;
      } else {
        state->path_entries[state->n_path_ens].is_iteratable = false;
      }

      // Keep track of the first and the last iteratable index.
      if (state->path_entries[state->n_path_ens].is_iteratable) {
        last_iter_idx = state->n_path_ens;
        if (first_iter_idx == UINT_MAX) {
          first_iter_idx = state->n_path_ens;
        }
      }
    } else {
      state->path_entries[state->n_path_ens].is_iteratable = false;
      state->path_entries[state->n_path_ens].type = PE_CONTAINER;
    }

    state->path_entries[state->n_path_ens].count = 0;
    state->path_entries[state->n_path_ens].next_index = 0;
    state->path_entries[state->n_path_ens].is_last_iter_index = false;
    state->path_entries[state->n_path_ens++].is_first_iter_index = false;
  }

  // Mark the last iteratable index in the out keyspec.
  if (last_iter_idx !=  UINT_MAX) {
    state->path_entries[last_iter_idx].is_last_iter_index = true;
    state->ks_has_iter_pes = true;
  }

  // Mark the first iteratable index in the out keyspec.
  if (first_iter_idx != UINT_MAX) {
    state->path_entries[first_iter_idx].is_first_iter_index = true;
  }

  state->next_state = REROOT_ITER_FIRST;
}

static bool bt_and_check_for_iter_end(
    keyspec_reroot_iter_state_t *state,
    int pe_idx)
{

  if (!pe_idx) {
    return true;
  }

  int i;
  for (i = pe_idx - 1; i >= 0; i--) {
    if (state->path_entries[i].is_iteratable) {
      state->path_entries[i].next_index++;
      if (state->path_entries[i].next_index < state->path_entries[i].count) {
        // We still have not exhausted this level.
        return false;
      }
      // This level is exhausted.
      if (state->path_entries[i].is_first_iter_index) { // Top level, cannot iterate anymore
        return true;
      }
      // Not top-level, reset the index and continue going up
      state->path_entries[i].next_index = 0;
    }
  }

  return true;
}

#define PREPARE_FOR_ITERATION(state_)\
  msg = state_->in_msg; \
  pe_idx = 0; \
  index = state_->depth_i; \
  if (state->cur_ks != nullptr) { \
    rw_keyspec_path_free(state->cur_ks, state->instance); \
  } \
  state->cur_ks = nullptr;\
  rw_keyspec_path_create_dup(state->ks_out->get(), state->instance, &state->cur_ks); \
  RW_ASSERT(state->cur_ks);\
  ks_cur.set(state->cur_ks);\
  if (state_->is_out_ks_wc) { \
    rw_keyspec_path_delete_binpath(state->cur_ks, state->instance); \
    size_t min_depth = (state->depth_i < state->depth_o)?state->depth_i:state->depth_o; \
    if (ks_cur.has_wildcards(0, min_depth)) {\
      rw_status_t s_ = keyspec_copy_keys(state->instance, *state->ks_in, ks_cur, min_depth);\
      RW_ASSERT(s_ == RW_STATUS_SUCCESS); \
    }\
  }


/* ATTN: This function can be optimized. It does many repeated processing over
   the iterations. This can be avoided by stored results from the previous
   iterations.
 */
bool rw_keyspec_path_reroot_iter_next(
    rw_keyspec_path_reroot_iter_state_t *st)
{
  const ProtobufCFieldDescriptor *fd = nullptr;
  size_t count = 0, off = 0;
  void *msg_ptr = nullptr;
  protobuf_c_boolean is_dptr = false;
  bool retval = false;
  const ProtobufCMessage *msg = nullptr;
  size_t index = 0;
  int pe_idx = 0;

  keyspec_reroot_iter_state_t *state = (keyspec_reroot_iter_state_t *)st;

  KEYSPEC_INC_FCALL_STATS(state->instance, path, reroot_iter);

  if (state->next_state == REROOT_ITER_DONE) {
    rw_keyspec_path_reroot_iter_done(st);
    return retval;
  }

  KeySpecHelper ks_cur(state->cur_ks);

  PREPARE_FOR_ITERATION(state);

  // Shallower case.
  if (index > state->depth_o) {

    RW_ASSERT(state->next_state == REROOT_ITER_FIRST);

    state->cur_msg = keyspec_reroot_shallower(
        state->instance, nullptr, *state->ks_in, state->depth_i, 
        state->in_msg, *state->ks_out, state->depth_o);

    // curr_ks is already setup as part of the macro PREPARE_FOR_ITERATION.
    state->next_state = REROOT_ITER_DONE;
    if (!state->cur_msg) {
      rw_keyspec_path_reroot_iter_done(st);
      return false;
    }

    return true;
  }

  // Deeper case
  while (index < state->depth_o) {

    bool exit_wloop = false;

    auto path_entry = state->ks_out->get_path_entry(index);
    RW_ASSERT(path_entry);

    auto path_entry2 = ks_cur.get_path_entry(index);
    RW_ASSERT(path_entry2);

    auto element_id = rw_keyspec_entry_get_element_id(path_entry);
    RW_ASSERT(element_id);

    count = protobuf_c_message_get_field_desc_count_and_offset(msg,
              element_id->element_tag, &fd, &msg_ptr, &off, &is_dptr);

    if (!count) {
      // There is no message for this path entry. Check whether we can continue
      // the iteration
      if (!bt_and_check_for_iter_end(state, pe_idx)) {
        // Start over the iteration again.
        PREPARE_FOR_ITERATION(state);
        continue;
      }
      break;
    }

    RW_ASSERT(fd->type == PROTOBUF_C_TYPE_MESSAGE);
    if (is_dptr) {
      msg = *((ProtobufCMessage **)msg_ptr);
    } else {
      msg = (ProtobufCMessage *)msg_ptr;
    }

    if (element_id->element_type >= RW_SCHEMA_ELEMENT_TYPE_LISTY_0) {
      state->path_entries[pe_idx].count = count;
      RW_ASSERT(state->path_entries[pe_idx].is_listy);
    }

    if (state->path_entries[pe_idx].is_listy) {

      RW_ASSERT(fd->label == PROTOBUF_C_LABEL_REPEATED);

      switch (state->path_entries[pe_idx].type) {
        // All the keys are specified.
        case PE_LISTY_WITH_ALL_KEYS:
          {
            unsigned i;
            for (i = 0; i < count; i++) {
              if (is_dptr) {
                msg = *((ProtobufCMessage **)msg_ptr + i);
              } else {
                msg = (ProtobufCMessage *)(((char *)msg_ptr + (off * i)));
              }

              if (compare_path_entry_and_message(
                      state->instance, path_entry, msg, false)) {
                break;
              }
            }

            if (i == count) {
              // No message matched for the path entry.
              if (!bt_and_check_for_iter_end(state, pe_idx)) {
                // Start over the iteration again.
                PREPARE_FOR_ITERATION(state);
                continue;
              }
              exit_wloop = true;
            }
          }
          break;
        case PE_LISTY_WITH_ALL_WC:
          {
            /* The path_entry is completely wildcarded. */

            if (state->path_entries[pe_idx].next_index < state->path_entries[pe_idx].count) {
              if (is_dptr) {
                msg = *((ProtobufCMessage **)msg_ptr + state->path_entries[pe_idx].next_index);
              } else {
                msg = (ProtobufCMessage *)(((char *)msg_ptr + (off * state->path_entries[pe_idx].next_index)));
              }

              path_entry_copy_keys(state->instance, msg, (rw_keyspec_entry_t *)path_entry2);

            } else {
              // No more messages.
              exit_wloop = true;
            }
          }
          break;
        case PE_LISTY_WITH_WC_AND_KEYS:
          {
            /* Only some keys in the path entry are specified.
             * Other keys are wildcarded.
             */
            unsigned i =  state->path_entries[pe_idx].next_index;
            while (i < count) {
              if (is_dptr) {
                msg = *((ProtobufCMessage **)msg_ptr + i);
              } else {
                msg = (ProtobufCMessage *)(((char *)msg_ptr + (off * i)));
              }

              if (compare_path_entry_and_message(
                      state->instance, path_entry, msg, true)) {
                break;
              }
              i++;
            }

            if (i == count) {
              // No message matched for the path entry.
              if (!bt_and_check_for_iter_end(state, pe_idx)) {
                // Start over the iteration again.
                PREPARE_FOR_ITERATION(state);
                continue;
              }
              exit_wloop = true;
            } else {
              // A message was found.
              path_entry_copy_keys(state->instance, msg, const_cast<rw_keyspec_entry_t *>(path_entry2));
              state->path_entries[pe_idx].next_index = i;
            }
          }
          break;

        default:
          RW_ASSERT_NOT_REACHED();
      }
    }

    if (exit_wloop) {
      break;
    }

    index++;
    pe_idx++;
  }

  if (index == state->depth_o) {

    state->cur_msg = msg;

    if (!state->ks_has_iter_pes) {

      RW_ASSERT(state->next_state == REROOT_ITER_FIRST);
      state->next_state = REROOT_ITER_DONE;

    } else {

      // Update indices for next iteration.
      for (unsigned i = 0; i < state->n_path_ens; ++i) {
        if (state->path_entries[i].is_last_iter_index) {
          state->path_entries[i].next_index++;
          if (state->path_entries[i].next_index == state->path_entries[i].count) {
            if (!state->path_entries[i].is_first_iter_index) {
              state->path_entries[i].next_index = 0;
              bt_and_check_for_iter_end(state, i); // ignore the return value.
            }
          }
          break;
        }
      }

      state->next_state = REROOT_ITER_NEXT;
    }

    return true;
  }

  rw_keyspec_path_reroot_iter_done(st);
  return retval;
}

const rw_keyspec_path_t* rw_keyspec_path_reroot_iter_get_ks(
    rw_keyspec_path_reroot_iter_state_t *st)
{
  keyspec_reroot_iter_state_t *state = (keyspec_reroot_iter_state_t *)st;
  RW_ASSERT(state->cur_ks);
  return (state->cur_ks);
}

const ProtobufCMessage* rw_keyspec_path_reroot_iter_get_msg(
    rw_keyspec_path_reroot_iter_state_t *st)
{
  keyspec_reroot_iter_state_t *state = (keyspec_reroot_iter_state_t *)st;
  RW_ASSERT(state->cur_msg);
  return (state->cur_msg);
}

rw_keyspec_path_t* rw_keyspec_path_reroot_iter_take_ks(
    rw_keyspec_path_reroot_iter_state_t *st)
{
  keyspec_reroot_iter_state_t *state = (keyspec_reroot_iter_state_t *)st;
  RW_ASSERT(state->cur_ks);
  rw_keyspec_path_t *out_ks = state->cur_ks;
  state->cur_ks = NULL;
  return (out_ks);
}

ProtobufCMessage* rw_keyspec_path_reroot_iter_take_msg(
    rw_keyspec_path_reroot_iter_state_t *st)
{
  keyspec_reroot_iter_state_t *state = (keyspec_reroot_iter_state_t *)st;
  RW_ASSERT(state->cur_msg);

  ProtobufCMessage *out_msg = nullptr;
  if (state->depth_i > state->depth_o) {
    // for shallower case, the  cur_msg is always a new message.
    out_msg = (ProtobufCMessage *)state->cur_msg;
  } else {
    out_msg = protobuf_c_message_duplicate(
        state->instance->pbc_instance, state->cur_msg, state->cur_msg->descriptor);
  }
  state->cur_msg = NULL;

  return (out_msg);
}

void rw_keyspec_path_reroot_iter_done(
    rw_keyspec_path_reroot_iter_state_t *st)
{
  keyspec_reroot_iter_state_t *state = (keyspec_reroot_iter_state_t *)st;

  if (state->cur_ks) {
    rw_keyspec_path_free(state->cur_ks, state->instance);
  }

  if (state->depth_i > state->depth_o) {
    // for shallower case, the  cur_msg is always a new message.
    if (state->cur_msg) {
      protobuf_c_message_free_unpacked(
          state->instance->pbc_instance, (ProtobufCMessage *)state->cur_msg);
    }
  }

  state->cur_ks = NULL;
  state->cur_msg = NULL;

  delete state->ks_out;
  state->ks_out = NULL;

  delete state->ks_in;
  state->ks_in = NULL;

  state->in_msg = NULL;
  state->instance = NULL;
  state->next_state = REROOT_ITER_INIT;
}

rw_status_t rw_keyspec_path_add_keys_to_entry(
    rw_keyspec_path_t *k,
    rw_keyspec_instance_t* instance,
    const rw_keyspec_entry_t *path_entry,
    size_t index)
{
  rw_status_t status = RW_STATUS_FAILURE;

  RW_ASSERT(k);
  RW_ASSERT(path_entry);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, add_keys);

  KeySpecHelper ks(k);

  if (!ks.has_dompath()) {
    if (rw_keyspec_path_update_dompath(k, instance) != RW_STATUS_SUCCESS) {
      return status;
    }
  }

  RW_ASSERT(ks.has_dompath());

  auto ks_path_entry = (rw_keyspec_entry_t *) ks.get_path_entry(index);
  if (!ks_path_entry) {
    KEYSPEC_ERROR(instance, k, "No path entry at index(%d)", index);
    return status;
  }

  auto elem_id1 = rw_keyspec_entry_get_element_id(path_entry);
  RW_ASSERT(elem_id1);

  auto elem_id2 = rw_keyspec_entry_get_element_id(ks_path_entry);
  RW_ASSERT(elem_id2);

  if (!IS_SAME_ELEMENT(elem_id1, elem_id2)) {
    return status;
  }

  if (path_entry_copy_keys(
          instance, path_entry, ks_path_entry) != RW_STATUS_SUCCESS) {
    return status;
  }

  rw_keyspec_path_delete_binpath(k, instance);
  rw_keyspec_path_delete_strpath(k, instance);

  return RW_STATUS_SUCCESS;
}

rw_status_t rw_keyspec_entry_create_dup(
  const rw_keyspec_entry_t* input,
  rw_keyspec_instance_t* instance,
  rw_keyspec_entry_t** output)
{
  RW_ASSERT(is_good_pe(input));
  RW_ASSERT(output);
  RW_ASSERT(!*output);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, entry, create_dup);

  // Dup to the same type
  ProtobufCMessage* out_msg = dup_and_convert_to_type(
      instance, &input->base, input->base.descriptor);

  if (out_msg) {
    *output = reinterpret_cast<rw_keyspec_entry_t *>(out_msg);
    return RW_STATUS_SUCCESS;
  }

  return RW_STATUS_FAILURE;
}

rw_keyspec_entry_t* rw_keyspec_entry_create_dup_of_type(
  const rw_keyspec_entry_t* input,
  rw_keyspec_instance_t* instance,
  const ProtobufCMessageDescriptor* out_pbcmd)
{
  RW_ASSERT(is_good_pe(input));
  RW_ASSERT(out_pbcmd);
  RW_ASSERT(PROTOBUF_C_MESSAGE_DESCRIPTOR_MAGIC == out_pbcmd->magic);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, entry, create_dup_type);

  // Dup to the other type
  ProtobufCMessage* out_msg = dup_and_convert_to_type(
      instance, &input->base, out_pbcmd);

  if (out_msg) {
    return reinterpret_cast<rw_keyspec_entry_t *>(out_msg);
  }

  return nullptr;
}

rw_status_t rw_keyspec_entry_free(
    rw_keyspec_entry_t *path_entry,
    rw_keyspec_instance_t* instance)
{
  RW_ASSERT(is_good_pe(path_entry));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  protobuf_c_message_free_unpacked(
      instance->pbc_instance, &path_entry->base);

  return RW_STATUS_SUCCESS;
}

rw_status_t rw_keyspec_entry_free_usebody(
    rw_keyspec_entry_t *path_entry,
    rw_keyspec_instance_t* instance)
{
  RW_ASSERT(is_good_pe(path_entry));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  protobuf_c_message_free_unpacked_usebody(
      instance->pbc_instance, &path_entry->base);

  return RW_STATUS_SUCCESS;
}

int rw_keyspec_entry_get_print_buffer(
    const rw_keyspec_entry_t *path_entry,
    rw_keyspec_instance_t* instance,
    char *buf,
    size_t buflen)
{
  RW_ASSERT(is_good_pe(path_entry));
  RW_ASSERT(buf);
  RW_ASSERT(buflen);

  int ret_val = -1;
  std::ostringstream oss;

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  const rw_yang_pb_msgdesc_t *ypbc = path_entry->base.descriptor->ypbc_mdesc;

  const RwSchemaElementId *elem =
      rw_keyspec_entry_get_element_id(path_entry);

  if (ypbc) {
    oss << ypbc->module->module_name << ":";
    oss << ypbc->yang_node_name;
  } else {
    oss << elem->system_ns_id << ":" << elem->element_tag;
  }

  if (rw_keyspec_entry_num_keys(path_entry)) {
    if (ypbc) {
      print_buffer_add_key_values(
          instance, oss, &ypbc->u->msg_msgdesc, path_entry);
    } else {
      print_buffer_add_key_values(instance, oss, path_entry);
    }
  }

  std::string out_str = oss.str();
  size_t len = out_str.length();

  if (len) {
    if (len < buflen-1) {
      memcpy(buf, out_str.c_str(), len);
      buf[len] = 0;
      ret_val = len;
    } else {
      memcpy(buf, out_str.c_str(), buflen-1);
      buf[buflen-1] = 0;
      ret_val = buflen-1;
    }
  }

  return ret_val;
}

rw_keyspec_entry_t* rw_keyspec_entry_unpack(
    rw_keyspec_instance_t* instance,
    const ProtobufCMessageDescriptor* mdesc,
    uint8_t *data,
    size_t len)
{
  rw_keyspec_entry_t *path_entry = nullptr;

  RW_ASSERT(data);
  RW_ASSERT(len);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  ProtobufCMessage *out = nullptr;
  if (mdesc) {
    out = protobuf_c_message_unpack(instance->pbc_instance, mdesc, len, data);
  } else {
    // Create a generic path entry.
    out = protobuf_c_message_unpack(
        instance->pbc_instance, &rw_schema_path_entry__descriptor, len, data);
  }

  if (out) {
    path_entry = reinterpret_cast<rw_keyspec_entry_t *>(out);
  }

  return path_entry;
}

size_t rw_keyspec_entry_pack(
    const rw_keyspec_entry_t* path_entry,
    rw_keyspec_instance_t* instance,
    uint8_t *buf,
    size_t len)
{
  size_t ret_len = -1;

  RW_ASSERT(path_entry);
  RW_ASSERT(buf);
  RW_ASSERT(len);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  size_t size = protobuf_c_message_get_packed_size(instance->pbc_instance, &path_entry->base);
  if (size > len) {
    return ret_len;
  }

  ret_len = protobuf_c_message_pack(instance->pbc_instance, &path_entry->base, buf);
  RW_ASSERT(ret_len == size);

  return ret_len;
}

rw_status_t rw_keyspec_path_create_leaf_entry(
    rw_keyspec_path_t* k,
    rw_keyspec_instance_t* instance,
    const rw_yang_pb_msgdesc_t *ypb_mdesc,
    const char *leaf)
{
  RW_ASSERT(is_good_ks(k));
  RW_ASSERT(ypb_mdesc);
  RW_ASSERT(leaf);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  rw_status_t status = RW_STATUS_FAILURE;

  KeySpecHelper ks(k);
  RW_ASSERT(ks.has_dompath());

  if (ks.is_leafy()) {
    // already refers to leaf??
    return status;
  }

  size_t depth = ks.get_depth();
  ProtobufCMessage *dom_path = (ProtobufCMessage *)ks.get_dompath();

  auto fd = protobuf_c_message_descriptor_get_field(
      dom_path->descriptor, RW_SCHEMA_TAG_PATH_ENTRY_START+depth);

  if ((!fd) || (fd->msg_desc != &rw_schema_path_entry__descriptor)) {
    return status;
  }

  auto ypb_fdescs = ypb_mdesc->ypbc_flddescs;
  unsigned i = 0;
  for (i = 0; i < ypb_mdesc->num_fields; i++) {
    if (!strcmp(leaf, ypb_fdescs[i].yang_node_name)) {
      break;
    }
  }

  if (i == ypb_mdesc->num_fields) {
    return status;
  }

  ProtobufCMessage* leaf_entry = nullptr;
  protobuf_c_message_set_field_message(
      instance->pbc_instance, dom_path, fd, &leaf_entry);
  RW_ASSERT(leaf_entry);

  auto elem = rw_keyspec_entry_get_schema_id(ypb_mdesc->schema_entry_value);
  elem.tag =  ypb_fdescs[i].pbc_fdesc->id;
  RwSchemaPathEntry *pe = (RwSchemaPathEntry *)leaf_entry;
  RW_ASSERT(pe);
  pe->element_id = (RwSchemaElementId *)malloc(sizeof(RwSchemaElementId));
  RW_ASSERT(pe->element_id);
  rw_schema_element_id__init(pe->element_id);
  if (ypb_fdescs[i].module != ypb_mdesc->module) {
    pe->element_id->system_ns_id =
        NamespaceManager::get_global().get_ns_hash(ypb_fdescs[i].module->ns); // ATTN:- Should we need to keep the nsid in ypbc module?
  } else {
    pe->element_id->system_ns_id = elem.ns;
  }
  pe->element_id->element_tag = elem.tag;
  pe->element_id->element_type = RW_SCHEMA_ELEMENT_TYPE_LEAF;

  rw_keyspec_path_delete_binpath(k, instance);
  rw_keyspec_path_delete_strpath(k, instance);

  return RW_STATUS_SUCCESS;
}

const char* rw_keyspec_path_leaf_name(
    const rw_keyspec_path_t* k,
    rw_keyspec_instance_t* instance,
    const ProtobufCMessageDescriptor *pbcmd)
{

  RW_ASSERT(is_good_ks(k));
  RW_ASSERT(pbcmd);

  if (!pbcmd->ypbc_mdesc) {
    return NULL;
  }

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KeySpecHelper ks(k);

  size_t depth = ks.get_depth();
  if (depth < 2) {
    return NULL;
  }

  const rw_keyspec_entry_t *entry = ks.get_path_entry(depth-1);

  const RwSchemaElementId *elem_id =
      rw_keyspec_entry_get_element_id(entry);

  RW_ASSERT(elem_id);

  if (elem_id->element_type != RW_SCHEMA_ELEMENT_TYPE_LEAF) {
    KEYSPEC_ERROR(instance, k, "KS is not for leaf(%d)", elem_id->element_type);
    return NULL;
  }

  /* Validations to make sure that correct descriptor has been passed. */
  const rw_yang_pb_msgdesc_t* msg_ypbd =
      rw_yang_pb_msgdesc_get_msg_msgdesc(pbcmd->ypbc_mdesc);

  RW_ASSERT(msg_ypbd);
  RW_ASSERT(msg_ypbd->schema_path_value);

  KeySpecHelper cks(msg_ypbd->schema_path_value);
  size_t depth_c = cks.get_depth();

  if (depth != depth_c + 1) {
    return NULL;
  }

  if (!rw_keyspec_path_is_prefix_match_impl(instance, cks, ks, depth_c, depth)) {
    KEYSPEC_ERROR(instance, k, "KS and ypbcdesc doesnt match");
    return NULL;
  }

  /* Ok, now check for the leaf node */
  auto ypbc_fdescs = msg_ypbd->ypbc_flddescs;
  for (unsigned i = 0; i < msg_ypbd->num_fields; i++) {
    if (ypbc_fdescs[i].pbc_fdesc->id == elem_id->element_tag) {
      return ypbc_fdescs[i].yang_node_name;
    }
  }

  return NULL;
}

rw_keyspec_path_t*
rw_keyspec_path_from_xpath(const rw_yang_pb_schema_t* schema,
                           const char* xpath,
                           rw_xpath_type_t xpath_type,
                           rw_keyspec_instance_t* instance)
{
  RW_ASSERT(schema);
  RW_ASSERT(xpath);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, from_xpath);

  rw_keyspec_path_t* keyspec = nullptr;

  if (!strlen(xpath)) {
    return keyspec;
  }

  char cat = 0;
  if (xpath[0] != '/') {
    if (strlen(xpath) < 3 || xpath[1] != ',') {
      return keyspec;
    }
    cat = xpath[0];
    xpath += 2;
  }

  RwSchemaCategory category = RW_SCHEMA_CATEGORY_ANY;
  if (cat != 0) {
    switch (cat) {
      case 'D':
        category = RW_SCHEMA_CATEGORY_DATA;
        break;
      case 'C':
        category = RW_SCHEMA_CATEGORY_CONFIG;
        break;
      case 'I':
        category = RW_SCHEMA_CATEGORY_RPC_INPUT;
        break;
      case 'O':
        category = RW_SCHEMA_CATEGORY_RPC_OUTPUT;
        break;
      case 'N':
        category = RW_SCHEMA_CATEGORY_NOTIFICATION;
        break;
      case 'A':
        break;
      default:
        KEYSPEC_ERROR(instance, "Invalid category in xpath(%c)", cat);
        return nullptr;
    }
  }

  XpathIIParser parser(xpath, xpath_type);

  if (!parser.parse_xpath()) {
    KEYSPEC_ERROR(instance, "Failed to parse xpath(%s)", xpath);
    return keyspec;
  }

  auto ypbc_mdesc = parser.validate_xpath(schema, category);
  if (!ypbc_mdesc) {
    KEYSPEC_ERROR(instance, "xpath(%s)-schema validation error", xpath);
    return keyspec;
  }

  rw_status_t s = rw_keyspec_path_create_dup(
      ypbc_mdesc->schema_path_value, instance, &keyspec);

  RW_ASSERT(s == RW_STATUS_SUCCESS);
  RW_ASSERT(keyspec);

  UniquePtrKeySpecPath::uptr_t uptr(keyspec);

  KeySpecHelper ks(keyspec);
  size_t depth = ks.get_depth();

  if (depth < parser.get_depth()) {
    if (depth + 1 != parser.get_depth()) {
      return nullptr;
    }
  }

  for (unsigned i = 0; i < depth; i++) {
    auto path_entry = ks.get_path_entry(i);
    RW_ASSERT(path_entry);

    if (rw_keyspec_entry_num_keys(path_entry)) {
      s = parser.populate_keys(
          instance, const_cast<rw_keyspec_entry_t*>(path_entry), i);
      if (s != RW_STATUS_SUCCESS) {
        KEYSPEC_ERROR(instance, "Failed to add keys to xpath(%s)", xpath);
        break;
      }
    }
  }

  if (s == RW_STATUS_SUCCESS && (depth + 1 == parser.get_depth())) {
    // Keypsec for leaf.
    s  = rw_keyspec_path_create_leaf_entry(
        keyspec, instance, ypbc_mdesc, parser.get_leaf_token().c_str());
  }

  if (s != RW_STATUS_SUCCESS) {
    return nullptr;
  }

  if (cat != 0) {
    rw_keyspec_path_set_category(keyspec, instance, category);
  }

  return uptr.release();
}

char* rw_keyspec_path_to_xpath(
    const rw_keyspec_path_t* keyspec,
    const rw_yang_pb_schema_t* schema,
    rw_keyspec_instance_t* instance)
{
  std::ostringstream oss;
  char *buf = nullptr;

  RW_ASSERT(schema);
  RW_ASSERT(keyspec);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KeySpecHelper ks(keyspec);

  bool is_rooted = ks.is_rooted();
  // Let us not try walking the schema for an unrooted keyspec.
  if (!is_rooted) {
    return nullptr;
  }

  size_t depth = ks.get_depth();
  if (!create_xpath_string_from_ks(instance, ks, depth, schema, oss)) {
    return nullptr;
  }

  std::string out_str = oss.str();
  auto out_len = out_str.length();
  buf = (char *)malloc(out_len+1);
  RW_ASSERT(buf);
  strncpy(buf, out_str.c_str(), out_len);
  buf[out_len] = 0;

  return buf;
}

static rw_keyspec_path_t* create_lcp_keyspec(
    rw_keyspec_instance_t* instance,
    const rw_keyspec_path_t *ks1,
    const rw_keyspec_path_t *ks2,
    size_t depth1,
    size_t depth2,
    int lcp_index)
{
  rw_keyspec_path_t *out_ks = NULL;
  const rw_keyspec_path_t *conc_ks = NULL;
  rw_status_t status;

  size_t depth = 0;

  if (!IS_KEYSPEC_GENERIC(ks1)) {
    conc_ks = ks1;
    depth = depth1;
  } else if (!IS_KEYSPEC_GENERIC(ks2)) {
    conc_ks = ks2;
    depth = depth2;
  }

  if (conc_ks != NULL) {
    auto ypbc_d = conc_ks->base.descriptor->ypbc_mdesc;
    RW_ASSERT(ypbc_d);

    for (int i = depth - 1; i > lcp_index; i--) {
      ypbc_d = ypbc_d->parent;
      RW_ASSERT(ypbc_d);
    }

    // The Generic path entry and the unknowns will be stripped off.
    out_ks = rw_keyspec_path_create_dup_of_type_trunc(
        conc_ks, instance, ypbc_d->pbc_mdesc);

  } else {
    // Both the keyspecs are generic KS. Create a generic out keyspec.
    if (depth1 <= depth2) {
      status = rw_keyspec_path_create_dup(ks1, instance, &out_ks);
      depth = depth1;
    } else {
      status = rw_keyspec_path_create_dup(ks2, instance, &out_ks);
      depth = depth2;
    }

    if (status == RW_STATUS_SUCCESS) {
      for (int i = depth - 1; i > lcp_index; i--) {
        status = rw_keyspec_path_trunc_suffix_n(out_ks, instance, i);
        if (status != RW_STATUS_SUCCESS) {
          rw_keyspec_path_free(out_ks, instance);
          out_ks = NULL;
          break;
        }
      }
    }
  }

  if (out_ks) {
    rw_keyspec_path_delete_binpath(out_ks, instance);
    rw_keyspec_path_delete_strpath(out_ks, instance);
  }

  return (out_ks);
}

static size_t reroot_opaque_get_packed_msg_size(
    rw_keyspec_instance_t* instance,
    const ProtobufCBinaryData *msg,
    KeySpecHelper& ks,
    int curr_index,
    int end_index)
{
  size_t rv = 0;

  if (curr_index == end_index) {
    rv += msg->len;
    return rv;
  }

  auto path_entry = ks.get_path_entry(curr_index);
  RW_ASSERT(path_entry);

  auto elemid = rw_keyspec_entry_get_element_id(path_entry);
  RW_ASSERT(elemid);

  if (rw_keyspec_entry_is_listy(path_entry)) {

    int n_keys = rw_keyspec_entry_num_keys(path_entry);

    for (int k = 0; k < n_keys; k++) {
      ProtobufCMessage *key = rw_keyspec_entry_get_key(path_entry, k);
      if (key) {
        if (IS_PATH_ENTRY_GENERIC(path_entry)) {
          /* For generic keyspec, keys are in the unknown fields list..*/
          RW_ASSERT(protobuf_c_message_unknown_get_count(key));
          rv += protobuf_c_message_get_unknown_fields_pack_size(key);
        } else {
          rv += protobuf_c_message_get_packed_size(instance->pbc_instance, key);
        }
      }
    }
  }

  RW_ASSERT(curr_index+1 <= end_index);

  auto path_entry_next = ks.get_path_entry(curr_index+1);
  RW_ASSERT(path_entry_next);

  auto elemid_next = rw_keyspec_entry_get_element_id(path_entry_next);
  RW_ASSERT(elemid_next);

  rv += protobuf_c_message_get_tag_size(elemid_next->element_tag);

  size_t sub_rv = reroot_opaque_get_packed_msg_size(instance, msg, ks, curr_index + 1, end_index);
  RW_ASSERT(sub_rv);
  rv += protobuf_c_message_get_uint32_size(sub_rv);
  rv += sub_rv;

  return rv;
}

static size_t reroot_opaque_create_packed_msg(
    rw_keyspec_instance_t* instance,
    const ProtobufCBinaryData *msg,
    KeySpecHelper& ks,
    int curr_index,
    int end_index,
    uint8_t *out)
{
  size_t rv = 0;

  if (curr_index == end_index) {
    memcpy(out, msg->data, msg->len);
    rv += msg->len;
    return rv;
  }

  auto path_entry = ks.get_path_entry(curr_index);
  RW_ASSERT(path_entry);

  auto elemid = rw_keyspec_entry_get_element_id(path_entry);
  RW_ASSERT(elemid);

  if (rw_keyspec_entry_is_listy(path_entry)) {

    int n_keys = rw_keyspec_entry_num_keys(path_entry);

    for (int k = 0; k < n_keys; k++) {
      ProtobufCMessage *key = rw_keyspec_entry_get_key(path_entry, k);
      if (key) {
        if (IS_PATH_ENTRY_GENERIC(path_entry)) {
          /* Generic keyspec.. Keys are already in unknown list..just combine.. */
          RW_ASSERT(protobuf_c_message_unknown_get_count(key));
          rv += protobuf_c_message_pack_unknown_fields(instance->pbc_instance, key, out + rv);
        } else {
          // Conceret keyspec..
          rv += protobuf_c_message_pack(instance->pbc_instance, key, out + rv);
        }
      }
    }
  }

  RW_ASSERT(curr_index+1 <= end_index);

  auto next_path_entry = ks.get_path_entry(curr_index+1);
  RW_ASSERT(next_path_entry);

  auto next_elemid = rw_keyspec_entry_get_element_id(next_path_entry);
  RW_ASSERT(next_elemid);

  uint8_t *out1 = out + rv;
  rv += protobuf_c_message_pack_tag(next_elemid->element_tag, out1);
  out1[0] |= PROTOBUF_C_WIRE_TYPE_LENGTH_PREFIXED;

  out1 = out + rv + 1;
  size_t msg_rv = 0;

  msg_rv += reroot_opaque_create_packed_msg(instance, msg, ks, curr_index+1, end_index, out1);
  RW_ASSERT(msg_rv);
  size_t rv_packed_size = protobuf_c_message_get_uint32_size(msg_rv);
  if (rv_packed_size != 1) {
    rv_packed_size -= 1;
    memmove(out1 + rv_packed_size, out1, msg_rv);
  }
  rv += msg_rv + protobuf_c_message_pack_uint32(msg_rv, out + rv);

  return rv;
}

rw_status_t
rw_keyspec_path_reroot_and_merge_opaque(
    rw_keyspec_instance_t* instance,
    const rw_keyspec_path_t *k1,
    const rw_keyspec_path_t *k2,
    const ProtobufCBinaryData *msg1,
    const ProtobufCBinaryData *msg2,
    rw_keyspec_path_t **ks_out,
    ProtobufCBinaryData *msg_out)
{

  RW_ASSERT(is_good_ks(k1));
  RW_ASSERT(is_good_ks(k2));
  RW_ASSERT(msg1);
  RW_ASSERT(msg2);
  RW_ASSERT(!*ks_out);
  RW_ASSERT(msg_out);
  RW_ASSERT(!msg_out->data);

  rw_status_t status = RW_STATUS_FAILURE;

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, reroot_and_merge_op);

  KeySpecHelper ks1(k1);
  KeySpecHelper ks2(k2);

  size_t depth1 = ks1.get_depth();
  size_t depth2 = ks2.get_depth();

  /*
   * Get least common prefix index. This does an extact match of the
   * path entries.
   */
  int lcp = rw_keyspec_path_get_common_prefix_index(
      instance, ks1, ks2, depth1, depth2);
  if (lcp == -1) {
    KEYSPEC_ERROR(instance, k1, k2, "No common prefix to merge at");
    return status;
  }

  RW_ASSERT((msg1->len) && (msg1->data));
  RW_ASSERT((msg2->len) && (msg2->data));

  if ((depth1 == depth2) && ((lcp + 1) == static_cast<int>(depth1))) {
    /*
     * No re-root is required. Just combine the serialized data and return the
     * result
     */
    msg_out->len = msg1->len + msg2->len;
    msg_out->data = (uint8_t *)malloc(msg_out->len);
    RW_ASSERT(msg_out->data);
    memcpy(msg_out->data, msg1->data, msg1->len);
    memcpy(msg_out->data+msg1->len, msg2->data, msg2->len);
    rw_keyspec_path_create_dup(ks1.get(), instance, ks_out);

    return (RW_STATUS_SUCCESS);
  }

  *ks_out = create_lcp_keyspec(instance, k1, k2, depth1, depth2, lcp);
  if (!*ks_out) {
    return status;
  }

  size_t rr_size1 = msg1->len;
  if (lcp != static_cast<int>(depth1 - 1)) {
    rr_size1 = reroot_opaque_get_packed_msg_size(
        instance, msg1, ks1, lcp, depth1-1);
    RW_ASSERT(rr_size1);
  }

  size_t rr_size2 = msg2->len;
  if (lcp != static_cast<int>(depth2 - 1)) {
    rr_size2 = reroot_opaque_get_packed_msg_size(
        instance, msg2, ks2, lcp, depth2-1);
    RW_ASSERT(rr_size2);
  }

  msg_out->len = rr_size1 + rr_size2;
  msg_out->data = (uint8_t *)RW_MALLOC0(msg_out->len);
  RW_ASSERT(msg_out->data);

  if (lcp != static_cast<int>(depth1 - 1)) {
    size_t ret_sz = reroot_opaque_create_packed_msg(
        instance, msg1, ks1, lcp, depth1-1, msg_out->data);
    RW_ASSERT(ret_sz == rr_size1);
  } else {
    memcpy(msg_out->data, msg1->data, rr_size1);
  }

  if (lcp != static_cast<int>(depth2 - 1)) {
    size_t ret_sz = reroot_opaque_create_packed_msg(
        instance, msg2, ks2, lcp, depth2-1, msg_out->data + rr_size1);
    RW_ASSERT(ret_sz == rr_size2);
  } else {
    memcpy(msg_out->data + rr_size1, msg2->data, rr_size2);
  }

  status = RW_STATUS_SUCCESS;
  return status;
}

rw_keyspec_path_t * rw_keyspec_path_merge(
    rw_keyspec_instance_t* instance,
    const rw_keyspec_path_t *k1,
    const rw_keyspec_path_t *k2)
{
  rw_keyspec_path_t *out = NULL;
  rw_keyspec_path_t *inp = NULL;

  RW_ASSERT(is_good_ks(k1));
  RW_ASSERT(is_good_ks(k2));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KeySpecHelper ks1(k1);
  KeySpecHelper ks2(k2);

  size_t depth_1 = ks1.get_depth();
  size_t depth_2 = ks2.get_depth();

  if (!depth_1 || !depth_2) {
    return out;
  }

  if (!rw_keyspec_path_is_prefix_match_impl(instance, ks1, ks2, depth_1, depth_2)) {
    // One or more path elements didnot match for the common depth.
    KEYSPEC_ERROR(instance, k1, k2, "Keyspecs are not prefix match");
    return out;
  }

  // For the merge to happen, both the keyspec should be of same type.
  // Convert concrete keyspec to generic and do a merge.
  inp = (rw_keyspec_path_t *)k1;
  if (!ks1.is_generic()) {
    inp = rw_keyspec_path_create_dup_of_type(k1, instance, &rw_schema_path_spec__descriptor);
    RW_ASSERT(inp);
    out = inp;
  }

  if (!ks2.is_generic()) {
    out = rw_keyspec_path_create_dup_of_type(k2, instance, &rw_schema_path_spec__descriptor);
    RW_ASSERT(out);
  } else {
    if (NULL != out) {
      inp = (rw_keyspec_path_t *)k2; // To ensure that we always create the minimun number of dups.
    }
  }

  // Out should always be the dupped one, as the original input keyspec should
  // not be altered.
  if (NULL == out) {
    rw_keyspec_path_create_dup(k2, instance, &out);
    RW_ASSERT(out);
  }

  if (!protobuf_c_message_merge_new(instance->pbc_instance, &inp->base, &out->base)) {
    rw_keyspec_path_free(out, instance);
    out = NULL;
  }

  if ((inp != k1) && (inp != k2)) {
    rw_keyspec_path_free(inp, instance); // Can happen if both the keyspecs are concrete type.
  }

  return out;
}

static rw_keyspec_entry_t* rw_keyspec_entry_gi_ref(
  rw_keyspec_entry_t* pe)
{
  rw_keyspec_entry_t* dup_pe = NULL;
  rw_status_t rs = rw_keyspec_entry_create_dup(pe, NULL, &dup_pe);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  return dup_pe;
}

static void rw_keyspec_entry_gi_unref(
  rw_keyspec_entry_t* pe)
{
  rw_status_t rs = rw_keyspec_entry_free(pe, NULL);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
}

G_DEFINE_BOXED_TYPE(
  rw_keyspec_entry_t,
  rw_keyspec_entry,
  rw_keyspec_entry_gi_ref,
  rw_keyspec_entry_gi_unref);


static rw_keyspec_path_t* rw_keyspec_path_gi_ref(
  rw_keyspec_path_t* ks)
{
  rw_keyspec_path_t *out_ks = NULL;
  rw_status_t rs = rw_keyspec_path_create_dup(ks, NULL, &out_ks);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);
  return out_ks;
}

static void rw_keyspec_path_gi_unref(
  rw_keyspec_path_t* ks)
{
  rw_status_t rs = rw_keyspec_path_free(ks, NULL);
  RW_ASSERT(rs  == RW_STATUS_SUCCESS);
}

G_DEFINE_BOXED_TYPE(
  rw_keyspec_path_t,
  rw_keyspec_path,
  rw_keyspec_path_gi_ref,
  rw_keyspec_path_gi_unref);

G_DEFINE_POINTER_TYPE(rw_keyspec_instance_t,
                      rw_keyspec_instance)

static const GEnumValue _rw_xpath_type_enum_values[] = {
  { RW_XPATH_KEYSPEC, "KEYSPEC", "KEYSPEC" },
  { RW_XPATH_INSTANCEID, "INSTANCE_ID", "INSTANCE_ID"},
  { 0, NULL, NULL}
};

GType rw_keyspec_xpath_type_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */
  if (!type) {
    type = g_enum_register_static("rw_xpath_type_t", _rw_xpath_type_enum_values);
  }
  return type;
}

ProtobufCGiMessageBox* rw_keyspec_entry_check_and_create_gi_dup_of_type(
  const rw_keyspec_path_t *k,
  rw_keyspec_instance_t* instance,
  const ProtobufCMessageDescriptor* pbcmd)
{
  RW_ASSERT(is_good_ks(k));
  RW_ASSERT(pbcmd);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  /*
   * Must have a schema-aware protobuf descriptor for this function to
   * work, because there needs to be some way to discriminate which
   * path entry is desired.
   */
  if (nullptr == pbcmd->ypbc_mdesc) {
    return nullptr;
  }
  const rw_yang_pb_msgdesc_t* msg_msgdesc
    = rw_yang_pb_msgdesc_get_msg_msgdesc(pbcmd->ypbc_mdesc);

  /*
   * Compare the keyspec we have (ks) with the keyspec that we want
   * (the one associated with msg_msgdesc).  Assume the provided
   * msgdesc is the message's desc.
   */

  RW_ASSERT(msg_msgdesc->schema_path_value);

  KeySpecHelper ks(k);
  KeySpecHelper ks_c(msg_msgdesc->schema_path_value);

  size_t depth_c = ks_c.get_depth();
  size_t depth = ks.get_depth();

  if (depth_c > depth) {
    // Cannot be a prefix match.
    return nullptr;
  }

  if (!rw_keyspec_path_is_prefix_match_impl(instance, ks_c, ks, depth_c, depth)) {
    // Not a prefix or match, so cannot create a box.
    return nullptr;
  }

  /*
   * The schema location in msg_msgdesc matches with ks.  Find the
   * corresponding path entry in ks - it might not be the last one in
   * ks, but it is the last one in the schema location.
   */
  const rw_keyspec_entry_t* pe_orig = ks.get_path_entry(depth_c-1);
  RW_ASSERT(pe_orig);

  /*
   * Create a new concrete path entry message.
   */
  const rw_yang_pb_msgdesc_t* pe_msgdesc = msg_msgdesc->schema_entry_ypbc_desc;
  RW_ASSERT(pe_msgdesc);

  const ProtobufCMessageDescriptor* pe_pbcmd = pe_msgdesc->pbc_mdesc;
  RW_ASSERT(pe_pbcmd);

  ProtobufCMessage* pe_cc = protobuf_c_message_duplicate(
      instance->pbc_instance, &pe_orig->base, pe_pbcmd);
  if (!pe_cc) {
    // Cound not create it?  Maybe there's something wrong with keys or extensions?
    return nullptr;
  }

  /*
   * Put the path entry in a box.
   */
  const ProtobufCMessageGiDescriptor* pe_pbcgid = pe_pbcmd->gi_descriptor;
  RW_ASSERT(pe_pbcgid);
  RW_ASSERT(pe_pbcgid->new_adopt);
  ProtobufCGiMessageBox* box = (pe_pbcgid->new_adopt)(
    pe_cc,
    nullptr/*parent*/,
    nullptr/*parent_unref*/);
  if (!box) {
    protobuf_c_message_free_unpacked(instance->pbc_instance, pe_cc);
  }
  return box;
}

rw_status_t
rw_keyspec_path_key_descriptors_by_yang_name (const rw_keyspec_entry_t* entry,
                                              const char *name,
                                              const char *ns,
                                              const ProtobufCFieldDescriptor** k_desc,
                                              const ProtobufCFieldDescriptor** f_desc)
{
  if (nullptr == entry) {
    return RW_STATUS_FAILURE;
  }
  ProtobufCMessage *path = (ProtobufCMessage *)entry;
  const ProtobufCMessageDescriptor *desc = path->descriptor;

  // Find the associated descriptor of the protobuf message

  const rw_yang_pb_msgdesc_t *ypbc = desc->ypbc_mdesc;

  if ((nullptr == ypbc) || (ypbc->msg_type != RW_YANGPBC_MSG_TYPE_SCHEMA_ENTRY)
      || (nullptr == ypbc->u)) {
    return RW_STATUS_FAILURE;
  }

  uint32_t tag = 0;

  if (ypbc->u->msg_msgdesc.msg_type == RW_YANGPBC_MSG_TYPE_LEAF_LIST) {

    RW_ASSERT(!ypbc->u->msg_msgdesc.pbc_mdesc);
    tag = ypbc->u->msg_msgdesc.pb_element_tag; // Same as the key tag.

  } else {

    const ProtobufCFieldDescriptor *fd =
        protobuf_c_message_descriptor_get_field_by_yang(ypbc->u->msg_msgdesc.pbc_mdesc,
                                                        name, ns);
    if (nullptr == fd) {
      return RW_STATUS_FAILURE;
    }

    tag = fd->id;
  }

  // Search for this tag in each of the submessages of this descriptor
  for (size_t i = 0; i < desc->n_fields; i++) {
    if (desc->fields[i].type != PROTOBUF_C_TYPE_MESSAGE) {
      continue;
    }

    *k_desc = &desc->fields[i];
    *f_desc = protobuf_c_message_descriptor_get_field (
        (const ProtobufCMessageDescriptor *)(*k_desc)->descriptor,tag);

    if (*f_desc) {
      return RW_STATUS_SUCCESS;
    }
  }

  RW_ASSERT(nullptr == *f_desc);
  *k_desc = nullptr;
  return RW_STATUS_FAILURE;
}

rw_status_t rw_keyspec_entry_get_key_at_or_after(
    const rw_keyspec_entry_t* entry,
    size_t *key_index)
{
  size_t key_count = rw_keyspec_entry_num_keys (entry);

  // THE PASSED IN INDEX SHOULD NOT CHANGE IF THERE IS NO KEY AFTER THIS.

  for (size_t i = *key_index; i < key_count; i++) {
    if (rw_keyspec_entry_has_key (entry, i)) {
      *key_index = i;
      return RW_STATUS_SUCCESS;
    }
  }

  return RW_STATUS_FAILURE;
}

// ATTN: This function must not make requirements on the caller to
// check and free.  There should be a unique_ptr in here somehow.
static rw_keyspec_entry_t*
get_key_from_leaf_list_path_entry(
    rw_keyspec_instance_t* instance,
    const rw_keyspec_entry_t *path_entry,
    const rw_yang_pb_msgdesc_t *parent_mdesc)
{
  RW_ASSERT(parent_mdesc);
  RW_ASSERT(path_entry);

  const rw_yang_pb_msgdesc_t *ll_ymdesc = nullptr;
  rw_keyspec_entry_t *cpe = const_cast<rw_keyspec_entry_t*>(path_entry);

  if (path_entry->base.descriptor == &rw_schema_path_entry__descriptor) {
    // Convert the generic path entry to concerete type to extract keys.
    // For leaf-list path entries, the element id is same as the key's tag.
    // So, search for the child-msg-desc using the element id in parent ypbc msg
    // desc.
    auto elem_id = rw_keyspec_entry_get_element_id(path_entry);
    for (unsigned i = 0; i < parent_mdesc->child_msg_count; i++) {
      if (parent_mdesc->child_msgs[i]->pb_element_tag == elem_id->element_tag) {
        ll_ymdesc = parent_mdesc->child_msgs[i];
        break;
      }
    }

    if (!ll_ymdesc) {
      return nullptr;
    }

    // ATTN: Caller of this function should free cpe
    // at the called site.
    cpe = rw_keyspec_entry_create_dup_of_type(
        path_entry, instance, ll_ymdesc->schema_entry_ypbc_desc->pbc_mdesc);

    if (!cpe) {
      return nullptr;
    }
  }

  return cpe;
}

bool rw_keyspec_path_matches_message (
    const rw_keyspec_path_t *k,
    rw_keyspec_instance_t* instance,
    const ProtobufCMessage *msg,
    bool match_c_struct)
{
  RW_ASSERT(k);
  RW_ASSERT(msg);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, matches_msg);

  const ProtobufCMessage *path = &k->base;
  if ((nullptr == path->descriptor) ||
      (nullptr == path->descriptor->ypbc_mdesc)) {
    return false;
  }

  const rw_yang_pb_msgdesc_t* ypbc =
      rw_yang_pb_msgdesc_get_msg_msgdesc (path->descriptor->ypbc_mdesc);

  if ((nullptr == msg->descriptor) ||
      (nullptr == msg->descriptor->ypbc_mdesc)) {
    return false;
  }
   
  if ( match_c_struct && (ypbc != msg->descriptor->ypbc_mdesc)) {
    return false;
  }

  if (ypbc != msg->descriptor->ypbc_mdesc) {
    if ( (ypbc->pb_element_tag != msg->descriptor->ypbc_mdesc->pb_element_tag) ||
         (strcmp(ypbc->module->ns, msg->descriptor->ypbc_mdesc->module->ns)) ) {
      return false;
    }
  }

  // Match the values at the tip of the keyspec, removing the extraneous
  // leaf path
  KeySpecHelper ks(k);

  size_t count = ks.get_depth() - 1;
  const rw_keyspec_entry_t *entry = ks.get_path_entry(count);
  RW_ASSERT(nullptr != entry);

  count = rw_keyspec_entry_num_keys (entry);

  for (size_t i = 0; i < count; i++) {
    ProtobufCFieldInfo key_value;

    if (RW_STATUS_SUCCESS == rw_keyspec_entry_get_key_value (entry, i, &key_value)) {
      // The key is present, and has a value
      if (!protobuf_c_message_has_field_with_value (msg, key_value.fdesc->name,
                                                    &key_value)) {
        return false;
      }
    }
  }

  return true;
}

rw_status_t rw_keyspec_path_delete_proto_field(
    rw_keyspec_instance_t* instance,
    const rw_keyspec_path_t* msg_ks,
    const rw_keyspec_path_t* del_ks,
    ProtobufCMessage *msg)
{
  rw_status_t status = RW_STATUS_FAILURE;

  RW_ASSERT(msg_ks);
  RW_ASSERT(del_ks);
  RW_ASSERT(msg);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  KEYSPEC_INC_FCALL_STATS(instance, path, delete_proto_field);

  KeySpecHelper ks_m(msg_ks);
  KeySpecHelper ks_d(del_ks);

  size_t depth_m = ks_m.get_depth();
  size_t depth_d = ks_d.get_depth();

  if (depth_d <= depth_m) {
    KEYSPEC_ERROR(instance, del_ks, msg_ks, "Delete ks(%d) shallower than msg ks(%d)", depth_d, depth_m);
    return status;
  }

  // Sanity checks.
  if (!rw_keyspec_path_is_prefix_match_impl(instance, ks_m, ks_d, depth_m, depth_d)) {
    KEYSPEC_ERROR(instance, del_ks, msg_ks, "Keyspecs are not prefix match");
    return status;
  }

  // Only the last path entry can be wildcarded?
  if (ks_d.has_wildcards(0, depth_d-1)) {
    KEYSPEC_ERROR(instance, del_ks, "Delete keyspec has wildcards for non terminals");
    return status;
  }

  ProtobufCMessage *parent_msg =
      (ProtobufCMessage *)keyspec_find_deeper(instance, depth_m, msg, ks_d, depth_d - 1);

  if (!parent_msg) {
    return status;
  }

  auto path_entry = ks_d.get_path_entry(depth_d - 1);
  RW_ASSERT(path_entry);

  auto element_id = rw_keyspec_entry_get_element_id(path_entry);
  RW_ASSERT(element_id);

  uint32_t field_tag = element_id->element_tag;
  auto msg_fd = protobuf_c_message_descriptor_get_field( parent_msg->descriptor, field_tag );
  if (!msg_fd) {
    return status;
  }

  size_t count = protobuf_c_message_get_field_count( parent_msg, msg_fd );
  if (0 == count) {
    // The field to delete is not set, is this an error?
    return status;
  }

  if (!rw_keyspec_entry_is_listy(path_entry)) {
    protobuf_c_boolean ret = protobuf_c_message_delete_field(
        instance->pbc_instance, parent_msg, field_tag);
    if (!ret) {
      return RW_STATUS_FAILURE;
    }
    return RW_STATUS_SUCCESS;
  }

  int field_index = -1;

  /*
   * ATTN: Currently handles only deleting a single repeated element
   * or all the elements.  Can the delete request contain wildcards for
   * only for some keys??
   */
  if (!ks_d.has_wildcards(depth_d-1)) {
    if (element_id->element_type == RW_SCHEMA_ELEMENT_TYPE_LEAF_LIST) {

      auto cpe = get_key_from_leaf_list_path_entry(
                   instance, path_entry, parent_msg->descriptor->ypbc_mdesc);

      if (cpe == nullptr) {
        return status;
      }

      ProtobufCFieldInfo key_finfo;
      if (!get_key_finfo_by_index(cpe, 0, &key_finfo)) {
        return status;
      }

      for (field_index = 0; 1; field_index++) {
        ProtobufCFieldInfo msg_finfo;
        auto found_data = protobuf_c_message_get_field_instance(
            instance->pbc_instance, parent_msg, msg_fd, field_index, &msg_finfo );
        if (!found_data) {
          return status;
        }

        int cmp = protobuf_c_field_info_compare( &key_finfo, &msg_finfo );
        if (cmp == 0) {
          break;
        }
      }

      // ATTN: Fix this.  This function should not know the
      // implementation details of get_key_from_leaf_list_path_entry
      if (path_entry->base.descriptor == &rw_schema_path_entry__descriptor) {
          rw_keyspec_entry_free (cpe, instance);
      }
    } else {
      for (field_index = 0; 1; field_index++) {
        ProtobufCFieldInfo msg_finfo;
        auto found_data = protobuf_c_message_get_field_instance(
            instance->pbc_instance, parent_msg, msg_fd, field_index, &msg_finfo );
        if (!found_data) {
          return status;
        }

        if (compare_path_entry_and_message(
              instance, path_entry, (ProtobufCMessage*)(msg_finfo.element), false)) {
          break;
        }
      }
    }
  }

  protobuf_c_boolean ret = false;
  if (field_index != -1) {
    // delete one list entry
    ret = protobuf_c_message_delete_field_index(
        instance->pbc_instance, parent_msg, field_tag, field_index);
  } else {
    // delete whole list
    ret = protobuf_c_message_delete_field(
        instance->pbc_instance, parent_msg, field_tag);
  }

  if (!ret) {
    return status;
  }

  return RW_STATUS_SUCCESS;
}

/*
 * return an allocated string  -- the caller owns the string
 */

char*
rw_keyspec_path_create_string(const rw_keyspec_path_t*   keyspec,
                              const rw_yang_pb_schema_t* schema,
                              rw_keyspec_instance_t*     instance)
{
  int retval;

  char* buffer = NULL;

  retval = rw_keyspec_path_get_new_print_buffer(keyspec,
                                                instance,
                                                schema,
                                                &buffer);
  RW_ASSERT((retval > 0  && *buffer) || buffer == NULL);

  return buffer;
}

namespace rw_yang {
rw_status_t get_string_value (const ProtobufCFieldInfo *val,
                              std::string& str)
{
  const ProtobufCFieldDescriptor* fld = val->fdesc;
  const void *v = val->element;

  switch (fld->type) {
    case PROTOBUF_C_TYPE_MESSAGE: {
      const ProtobufCMessageDescriptor *desc =
          (const ProtobufCMessageDescriptor *)fld->descriptor;
      RW_ASSERT(desc);
      RW_ASSERT(desc->ypbc_mdesc);

      str = desc->ypbc_mdesc->yang_node_name;
    }
      break;

    case PROTOBUF_C_TYPE_INT32:
    case PROTOBUF_C_TYPE_FIXED32:
    case PROTOBUF_C_TYPE_SINT32:
    case PROTOBUF_C_TYPE_SFIXED32:
      str = std::to_string(*(const int32_t *) v);
      break;
    case PROTOBUF_C_TYPE_UINT32:
      str = std::to_string(*(const uint32_t *) v);
      break;

    case PROTOBUF_C_TYPE_INT64:
    case PROTOBUF_C_TYPE_FIXED64:
    case PROTOBUF_C_TYPE_SINT64:
    case PROTOBUF_C_TYPE_SFIXED64:
      str =  std::to_string(*(const int64_t *) v);
      break;

    case PROTOBUF_C_TYPE_UINT64:
      str =  std::to_string(*(const uint64_t *) v);
      break;
    case PROTOBUF_C_TYPE_FLOAT:
      str =  std::to_string(*(const float *) v);
      break;

    case PROTOBUF_C_TYPE_DOUBLE:
      str =  std::to_string(*(const double *) v);
      break;

    case PROTOBUF_C_TYPE_BOOL:
      str = (*(const uint32_t *)v)?"true":"false";
      break;

    case PROTOBUF_C_TYPE_ENUM: {
      const ProtobufCEnumValue *enum_v = protobuf_c_enum_descriptor_get_value (
          (const ProtobufCEnumDescriptor *)fld->descriptor, *(int *)v);
      RW_ASSERT(enum_v);
      str = enum_v->name;
    }
      break;

    case PROTOBUF_C_TYPE_STRING:
      str = (const char *) v;
      break;

    case PROTOBUF_C_TYPE_BYTES:
    default:
      RW_CRASH();
  }

  return RW_STATUS_SUCCESS;

}
}

int rw_keyspec_export_error_records(rw_keyspec_instance_t* instance,
                                    ProtobufCMessage* msg,
                                    const char* record_fname,
                                    const char* ts_fname,
                                    const char* es_fname)
{
  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  if (!msg || !record_fname || !ts_fname || !es_fname) {
    return 0;
  }

  return keyspec_export_ebuf(instance, msg, record_fname, ts_fname, es_fname);
}

rw_keyspec_path_t*
rw_keyspec_path_create_output_from_input(const rw_keyspec_path_t* input,
                                         rw_keyspec_instance_t* instance,
                                         const rw_yang_pb_schema_t* schema)
{
  RW_ASSERT(is_good_ks(input));
  rw_keyspec_path_t* output = nullptr;

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  RwSchemaCategory cat = rw_keyspec_path_get_category(input);
  if (cat != RW_SCHEMA_CATEGORY_RPC_OUTPUT &&
      cat != RW_SCHEMA_CATEGORY_RPC_INPUT) {
    KEYSPEC_ERROR(instance, input, "KS is not RPC IN/OUT");
    return output;
  }

  auto ypbc_mdesc = input->base.descriptor->ypbc_mdesc;
  if (!ypbc_mdesc && !schema) {
    KEYSPEC_ERROR(instance, "Generic KS and no schema");
    return output;
  }

  if (!ypbc_mdesc) {
    rw_status_t rs = rw_keyspec_path_find_msg_desc_schema(
        input, instance, schema, &ypbc_mdesc, NULL);

    if (rs != RW_STATUS_SUCCESS) {
      KEYSPEC_ERROR(instance, input, "Schema and KS mismatch");
      return output;
    }
  }

  RW_ASSERT(ypbc_mdesc->msg_type == RW_YANGPBC_MSG_TYPE_SCHEMA_PATH);
  const rw_yang_pb_msgdesc_t* msg_ypbd =
      rw_yang_pb_msgdesc_get_msg_msgdesc(ypbc_mdesc);
  RW_ASSERT(msg_ypbd);

  const rw_yang_pb_msgdesc_t* output_mypbcd = nullptr;

  switch (msg_ypbd->msg_type) {
    case RW_YANGPBC_MSG_TYPE_CONTAINER:
    case RW_YANGPBC_MSG_TYPE_LIST:
    case RW_YANGPBC_MSG_TYPE_LEAF_LIST: {
      // Get the parent rpc input/output ypbc msg descriptor.
      msg_ypbd = msg_ypbd->parent;
      while (msg_ypbd) {
        if (msg_ypbd->msg_type == RW_YANGPBC_MSG_TYPE_RPC_INPUT ||
            msg_ypbd->msg_type == RW_YANGPBC_MSG_TYPE_RPC_OUTPUT) {
          output_mypbcd = msg_ypbd;
          break;
        }
        msg_ypbd = msg_ypbd->parent;
      }
      RW_ASSERT(output_mypbcd);
      if (output_mypbcd->msg_type == RW_YANGPBC_MSG_TYPE_RPC_OUTPUT) {
        break;
      }
      //Fall through
    }
    case RW_YANGPBC_MSG_TYPE_RPC_INPUT: {
      RW_ASSERT(msg_ypbd->u);
      const rw_yang_pb_rpcdef_t* rpc_def = &msg_ypbd->u->rpcdef;
      output_mypbcd = rpc_def->output;
      break;
    }
    case RW_YANGPBC_MSG_TYPE_RPC_OUTPUT: {
      output_mypbcd = msg_ypbd;
      break;
    }
    default:
      RW_ASSERT_NOT_REACHED();
  }

  RW_ASSERT(output_mypbcd);
  RW_ASSERT(output_mypbcd->msg_type == RW_YANGPBC_MSG_TYPE_RPC_OUTPUT);
  rw_keyspec_path_create_dup(output_mypbcd->schema_path_value, instance, &output);

  return output;
}

ProtobufCMessage*
rw_keyspec_path_create_delete_delta(const rw_keyspec_path_t* del_ks,
                                    rw_keyspec_instance_t* instance,
                                    const rw_keyspec_path_t* reg_ks)
{
  RW_ASSERT(is_good_ks(del_ks));
  RW_ASSERT(is_good_ks(reg_ks));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  UniquePtrKeySpecPath::uptr_t ks_uptr;

  if (!IS_KEYSPEC_GENERIC(del_ks)) {
    auto generic_ks = rw_keyspec_path_create_dup_of_type(
        del_ks, instance, &rw_schema_path_spec__descriptor);

    if (!generic_ks) {
      return nullptr;
    }
    del_ks = generic_ks;
    ks_uptr.reset(generic_ks);
  }

  KeySpecHelper ks_r(reg_ks);
  KeySpecHelper ks_d(del_ks);

  size_t depth_r = ks_r.get_depth();
  size_t depth_d = ks_d.get_depth();

  ProtobufCMessage* out_msg = nullptr;

  if (depth_d < depth_r) {
    KEYSPEC_ERROR(instance, del_ks, reg_ks, "Delete ks(%d) shallower than reg ks(%d)", depth_d, depth_r);
    return out_msg;
  }

  // Sanity checks.
  if (!rw_keyspec_path_is_prefix_match_impl(instance, ks_r, ks_d, depth_r, depth_d)) {
    KEYSPEC_ERROR(instance, del_ks, reg_ks, "Keyspecs are not prefix match");
    return out_msg;
  }

  // Only the last path entry can be wildcarded?
  if (ks_d.has_wildcards(0, depth_d-1)) {
    KEYSPEC_ERROR(instance, del_ks, "Delete keyspec has wildcards for non terminals");
    return out_msg;
  }

  RW_ASSERT(reg_ks->base.descriptor->ypbc_mdesc);

  auto ypbc_mdesc = rw_yang_pb_msgdesc_get_msg_msgdesc(
      reg_ks->base.descriptor->ypbc_mdesc);
  RW_ASSERT(ypbc_mdesc);

  out_msg = protobuf_c_message_create(NULL, ypbc_mdesc->pbc_mdesc);
  RW_ASSERT(out_msg);

  UniquePtrProtobufCMessage<>::uptr_t uptr(out_msg);

  ProtobufCMessage* cur_msg = out_msg, *parent = NULL;
  size_t index = depth_r - 1;

  auto path_entry = ks_d.get_path_entry(index++);
  RW_ASSERT(path_entry);

  auto elem_id = rw_keyspec_entry_get_element_id(path_entry);
  RW_ASSERT(elem_id);

  rw_status_t status;

  if (rw_keyspec_entry_num_keys(path_entry)) {
    if (rw_keyspec_entry_has_wildcards(path_entry)) {
      RW_ASSERT (depth_d == depth_r);

      auto reg_pathe = ks_r.get_path_entry(depth_r-1);
      RW_ASSERT (reg_pathe);

      if (rw_keyspec_entry_has_wildcards(reg_pathe)) {
        KEYSPEC_ERROR(instance, del_ks, reg_ks, "Cannot create PbDelta");
        return nullptr;
      }

      status = path_entry_copy_keys_to_msg(instance, reg_pathe, cur_msg);
      RW_ASSERT (status == RW_STATUS_SUCCESS);

    } else {
      status = path_entry_copy_keys_to_msg(instance, path_entry, cur_msg);
      RW_ASSERT (status == RW_STATUS_SUCCESS);
    }
  }

  while (index < depth_d) {

    RW_ASSERT(cur_msg);
    parent = cur_msg; cur_msg = NULL;

    path_entry = ks_d.get_path_entry(index);
    RW_ASSERT(path_entry);

    elem_id = rw_keyspec_entry_get_element_id(path_entry);
    RW_ASSERT(elem_id);

    // Find me in my parent.
    const ProtobufCFieldDescriptor* fdesc =
        protobuf_c_message_descriptor_get_field(parent->descriptor, elem_id->element_tag);

    if (!fdesc) {
      KEYSPEC_ERROR(instance, "Del Ks and reg Ks are from diff schema (%u)", elem_id->element_tag);
      return nullptr;
    }

    if ((index == (depth_d - 1)) &&
        (elem_id->element_type == RW_SCHEMA_ELEMENT_TYPE_LEAF ||
         elem_id->element_type == RW_SCHEMA_ELEMENT_TYPE_CONTAINER)) {
      break;
    }

    RW_ASSERT(elem_id->element_type != RW_SCHEMA_ELEMENT_TYPE_LEAF);

    if (elem_id->element_type != RW_SCHEMA_ELEMENT_TYPE_LEAF_LIST) {

      protobuf_c_message_set_field_message(instance->pbc_instance, parent, fdesc, &cur_msg);
      RW_ASSERT(cur_msg);
      status = path_entry_copy_keys_to_msg(instance, path_entry, cur_msg);
      RW_ASSERT (status == RW_STATUS_SUCCESS);

    } else {
      status = path_entry_copy_keys_to_msg(instance, path_entry, parent);
      RW_ASSERT (status == RW_STATUS_SUCCESS);
    }

    index++;
  }

  ProtobufCFieldReference fref = PROTOBUF_C_FIELD_REF_INIT_VALUE;
  if (parent) {
    protobuf_c_field_ref_goto_tag(&fref, parent, elem_id->element_tag);
    protobuf_c_field_ref_mark_field_deleted(&fref);
  }

  if (cur_msg) {
    protobuf_c_field_ref_goto_whole_message(&fref, cur_msg);
    protobuf_c_field_ref_mark_field_deleted(&fref);
  }

  return uptr.release();
}

const ProtobufCMessageDescriptor*
rw_keyspec_get_pbcmd_from_xpath(const char* xpath,
                                const rw_yang_pb_schema_t* schema,
                                rw_keyspec_instance_t* instance)
{
  RW_ASSERT(xpath);
  RW_ASSERT(schema);

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  if (!strlen(xpath)) {
    return nullptr;
  }

  char cat = 0;
  if (xpath[0] != '/') {
    if (strlen(xpath) < 3 || xpath[1] != ',') {
      return nullptr;
    }
    cat = xpath[0];
    xpath += 2;
  }

  RwSchemaCategory category = RW_SCHEMA_CATEGORY_ANY;
  if (cat != 0) {
    switch (cat) {
      case 'D':
        category = RW_SCHEMA_CATEGORY_DATA;
        break;
      case 'C':
        category = RW_SCHEMA_CATEGORY_CONFIG;
        break;
      case 'I':
        category = RW_SCHEMA_CATEGORY_RPC_INPUT;
        break;
      case 'O':
        category = RW_SCHEMA_CATEGORY_RPC_OUTPUT;
        break;
      case 'N':
        category = RW_SCHEMA_CATEGORY_NOTIFICATION;
        break;
      case 'A':
        break;
      default:
        KEYSPEC_ERROR(instance, "Invalid category in xpath(%c)", cat);
        return nullptr;
    }
  }

  XpathIIParser parser(xpath, RW_XPATH_KEYSPEC);

  if (!parser.parse_xpath()) {
    KEYSPEC_ERROR(instance, "Failed to parse xpath(%s)", xpath);
    return nullptr;
  }

  const rw_yang_pb_msgdesc_t* ypbc_mdesc = parser.validate_xpath(schema, category);
  if (!ypbc_mdesc) {
    KEYSPEC_ERROR(instance, "xpath(%s)-schema validation error", xpath);
    return nullptr;
  }

  if (RW_YANGPBC_MSG_TYPE_LEAF_LIST == ypbc_mdesc->msg_type) {
    KEYSPEC_ERROR(instance, "xpath(%s) - Get pbcmd for leaf-list", xpath);
    return nullptr;
  }

  // Make sure that the xpath was for a message
  KeySpecHelper ks(ypbc_mdesc->schema_path_value);
  size_t depth = ks.get_depth();

  if (depth != parser.get_depth()) {
    KEYSPEC_ERROR(instance, "xpath(%s) - get pbcmd for leaf?", xpath);
    return nullptr;
  }

  return ypbc_mdesc->pbc_mdesc;
}

ProtobufCMessage*
rw_keyspec_pbcm_create_from_keyspec(rw_keyspec_instance_t* instance,
                                    const rw_keyspec_path_t *ks)
{
  RW_ASSERT(is_good_ks(ks));

  instance = ks_instance_get(instance);
  RW_ASSERT(instance);

  if (IS_KEYSPEC_GENERIC(ks)) {
    return nullptr;
  }

  ProtobufCMessage* out_msg = nullptr;

  RW_ASSERT(ks->base.descriptor->ypbc_mdesc);

  auto ypbc_mdesc = rw_yang_pb_msgdesc_get_msg_msgdesc(
      ks->base.descriptor->ypbc_mdesc);
  RW_ASSERT(ypbc_mdesc);

  out_msg = protobuf_c_message_create(NULL, ypbc_mdesc->pbc_mdesc);
  RW_ASSERT(out_msg);

  // TODO: Add key value to the protobuf??

  return out_msg;
}
