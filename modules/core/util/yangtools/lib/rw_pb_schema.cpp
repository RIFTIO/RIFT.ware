
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/**
 * @file rw_schema_merge.cpp
 * @author Sujithra Periasamy
 * @date 2015/09/01
 * @brief Dynamic merging of Protobuf Schemas
 */

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <rw_namespace.h>

#include "yangmodel.h"
#include "rw_keyspec.h"
#include "rw_pb_schema.h"

using namespace rw_yang;

/*
 * Memory alloc/free support routines.
 */
static inline void* 
mem_alloc(rw_keyspec_instance_t* instance,
         size_t size)
{
  if (!instance) {
    instance = rw_keyspec_instance_default();
  }
  RW_ASSERT(instance);
  return protobuf_c_instance_alloc(instance->pbc_instance, size);
}

static inline void
mem_free(rw_keyspec_instance_t* instance,
         void* ptr)
{
  if (!instance) {
    instance = rw_keyspec_instance_default();
  }
  RW_ASSERT(instance);
  protobuf_c_instance_free(instance->pbc_instance, ptr);
}

static inline void*
mem_zalloc(rw_keyspec_instance_t* instance,
           size_t size)
{
  if (!instance) {
    instance = rw_keyspec_instance_default();
  }
  RW_ASSERT(instance);
  return protobuf_c_instance_zalloc(instance->pbc_instance, size);
}

static inline void*
mem_realloc(rw_keyspec_instance_t* instance,
            void* ptr,
            size_t size)
{
  if (!instance) {
    instance = rw_keyspec_instance_default();
  }
  RW_ASSERT(instance);
  return protobuf_c_instance_realloc(instance->pbc_instance, ptr, size);
}

static inline char*
mem_strdup(rw_keyspec_instance_t* instance,
           const char* str)
{
  size_t strsize = 1 + strlen(str);
  char *retstr = (char *)mem_alloc(instance, strsize);
  memcpy (retstr, str, strsize);
  return retstr;
}

static inline rw_keyspec_instance_t*
ks_get_instance(rw_keyspec_instance_t* instance)
{
  if (!instance) {
    instance = rw_keyspec_instance_default();
  }
  RW_ASSERT(instance);
  return instance;
}

class PbSchemaMerger
{

 public:

  // Maps..
  typedef std::map<std::string,  const rw_yang_pb_module_t*> ns_mod_map_t;
  typedef std::map<std::string,  const rw_yang_pb_msgdesc_t*> yn_msg_map_t;
  typedef std::map<uint32_t, const rw_yang_pb_msgdesc_t*> tag_msg_map_t;
  typedef std::map<std::string,  unsigned> fn_index_map_t;
  typedef std::map<std::string,  uint32_t> yn_tag_map_t;
  typedef std::map<uint32_t, const ProtobufCFieldDescriptor*> tag_fdesc_map_t;

  // Unordered Maps..
  typedef std::unordered_map<std::string, const rw_yang_pb_msgdesc_t*> yn_msg_umap_t;
  typedef std::unordered_map<std::string, const rw_yang_pb_flddesc_t*> yn_fdesc_umap_t;
  typedef std::unordered_map<uint32_t, const rw_yang_pb_msgdesc_t*> tag_msg_umap_t;

  // vectors..
  typedef std::vector<const rw_yang_pb_module_t*> module_list_t;
  typedef std::vector<const rw_yang_pb_msgdesc_t*> msg_list_t;

 public:

  PbSchemaMerger(rw_keyspec_instance_t* instance, 
                 const rw_yang_pb_schema_t* schema1, 
                 const rw_yang_pb_schema_t* schema2);
  const rw_yang_pb_schema_t* merge_schemas();

 private:

  void create_unified_schema();
  void fill_pbc_fdesc(const ProtobufCFieldDescriptor* o_fdesc,
                      ProtobufCFieldDescriptor* n_fdesc);

  void copy_pbc_fdesc(const ProtobufCFieldDescriptor* src,
                      ProtobufCFieldDescriptor* dest);

  void create_pbc_fdescs(const ProtobufCMessageDescriptor* mdesc1,
                         const ProtobufCMessageDescriptor* mdesc2,
                         rw_yang_pb_msgdesc_t* merged,
                         ProtobufCMessageDescriptor* m_mdesc);

  void create_root_pbc_mdesc(rw_yang_pb_msgdesc_t* merged);
  void create_pbc_mdesc(const ProtobufCMessageDescriptor* mdesc1,
                        const ProtobufCMessageDescriptor* mdesc2,
                        rw_yang_pb_msgdesc_t* merged);

  void fill_ypbc_fdesc(const rw_yang_pb_flddesc_t* o_fdesc,
                       rw_yang_pb_flddesc_t* n_fdesc,
                       const rw_yang_pb_msgdesc_t* parent,
                       unsigned yang_order);

  void copy_pbc_mdesc(const ProtobufCMessageDescriptor* src,
                      ProtobufCMessageDescriptor* dest,
                      const rw_yang_pb_msgdesc_t* ypbc_mdesc);

  void create_ypbc_fdesc(const rw_yang_pb_msgdesc_t* msg1,
                         const rw_yang_pb_msgdesc_t* msg2,
                         rw_yang_pb_msgdesc_t* merged);

  rw_yang_pb_msgdesc_t* merge_messages(const rw_yang_pb_msgdesc_t* msg1,
                                       const rw_yang_pb_msgdesc_t* msg2);
  const rw_yang_pb_msgdesc_t* compare_messages(const rw_yang_pb_msgdesc_t* msg1,
                                               const rw_yang_pb_msgdesc_t* msg2);
  void merge_module_root(msg_list_t& msgs);
  void merge_augmented_module();

  rw_yang_pb_module_t* create_and_init_new_module(const rw_yang_pb_module_t* mod1,
                                                  const rw_yang_pb_module_t* mod2);

  void create_and_init_schema_root_msg(const rw_yang_pb_msgdesc_t* cdata_root,
                                       size_t child_count);

  rw_yang_pb_msgdesc_t* create_and_init_root_msg(const rw_yang_pb_msgdesc_t* in_root,
                                                 const rw_yang_pb_module_t* out_mod);

  ProtobufCIntRange* create_pbc_field_ranges(const ProtobufCFieldDescriptor* fdescs,
                                             int n_values,
                                             unsigned* n_ranges);

  void create_pathspec_mdesc(const rw_yang_pb_msgdesc_t* mdesc1,
                             const rw_yang_pb_msgdesc_t* mdesc2,
                             rw_yang_pb_msgdesc_t* merged,
                             size_t depth);

  void create_pathentry_mdesc(const rw_yang_pb_msgdesc_t* fentry,
                              rw_yang_pb_msgdesc_t* merged);

  void populate_data_module_root();
  void populate_rpcio_module_root();
  void populate_notif_module_root();
  void create_schema_data_root();
  void clear_state();

 private:

  module_list_t modules_;
  ns_mod_map_t module_map_;

  const rw_yang_pb_schema_t* schema1_ = nullptr;
  const rw_yang_pb_schema_t* schema2_ = nullptr;

  const rw_yang_pb_module_t* merge_mod1_ = nullptr;
  const rw_yang_pb_module_t* merge_mod2_ = nullptr;

  const rw_yang_pb_msgdesc_t* root1_ = nullptr;
  const rw_yang_pb_msgdesc_t* root2_ = nullptr;

  rw_keyspec_instance_t* instance_ = nullptr;
  rw_yang_pb_schema_t* unified_ = nullptr;

  rw_yang_pb_module_t* merged_mod_ = nullptr;

  msg_list_t data_msgs_;
  msg_list_t rpci_msgs_;
  msg_list_t rpco_msgs_;
  msg_list_t notif_msgs_;
};

PbSchemaMerger::PbSchemaMerger(rw_keyspec_instance_t* instance,
                               const rw_yang_pb_schema_t* schema1,
                               const rw_yang_pb_schema_t* schema2)
{
  instance_ = instance;
  schema1_ = schema1;
  schema2_ = schema2;

  if (!instance_) {
    instance_ = ks_get_instance(instance_);
  }
}

void
PbSchemaMerger::create_unified_schema()
{
  unified_ = (rw_yang_pb_schema_t *)mem_alloc(instance_, sizeof(rw_yang_pb_schema_t));
  unified_->top_msglist = nullptr;
  unified_->modules = nullptr;
  unified_->module_count = 0;
  unified_->data_root = nullptr;

  auto dyn_module = (rw_yang_pb_module_t *)mem_alloc(instance_, sizeof(rw_yang_pb_module_t));
  unified_->schema_module = dyn_module;

  Namespace dyns_ns = NamespaceManager::get_global().get_new_dynamic_schema_ns();

  dyn_module->schema       = unified_;
  dyn_module->module_name  = mem_strdup(instance_, dyns_ns.get_module());
  dyn_module->revision     = nullptr;
  dyn_module->ns           = mem_strdup(instance_, dyns_ns.get_ns());
  dyn_module->prefix       = mem_strdup(instance_, dyns_ns.get_prefix());
  dyn_module->data_module  = nullptr;
  dyn_module->group_root   = nullptr;
  dyn_module->rpci_module  = nullptr;
  dyn_module->rpco_module  = nullptr;
  dyn_module->notif_module = nullptr;
}

void
PbSchemaMerger::fill_pbc_fdesc(const ProtobufCFieldDescriptor* o_fdesc,
                               ProtobufCFieldDescriptor* n_fdesc)
{
  n_fdesc->name              = mem_strdup(instance_, o_fdesc->name);
  n_fdesc->c_name            = mem_strdup(instance_, o_fdesc->c_name);
  n_fdesc->id                = o_fdesc->id;
  n_fdesc->label             = o_fdesc->label;
  n_fdesc->type              = o_fdesc->type;
  n_fdesc->flags             = o_fdesc->flags;
  n_fdesc->rw_flags          = o_fdesc->rw_flags;
  n_fdesc->rw_flags         &= ~RW_PROTOBUF_FOPT_INLINE; // Remove the inline option for dynamic fdescs
  n_fdesc->rw_flags         |= RW_PROTOBUF_FOPT_DYNAMIC;
  n_fdesc->rw_inline_max     = 0;
  n_fdesc->data_size         = 0;
  n_fdesc->ctype             = o_fdesc->ctype;
  n_fdesc->default_value     = o_fdesc->default_value; // ATTN: Create new default value
  n_fdesc->quantifier_offset = o_fdesc->quantifier_offset;
  if (n_fdesc->ctype) {
    n_fdesc->data_size         = o_fdesc->data_size;
  }

  if ((n_fdesc->label == PROTOBUF_C_LABEL_OPTIONAL) &&
      (n_fdesc->type == PROTOBUF_C_TYPE_STRING ||
       n_fdesc->type == PROTOBUF_C_TYPE_MESSAGE)) {
    n_fdesc->quantifier_offset = 0;
  }
}

void
PbSchemaMerger::copy_pbc_fdesc(const ProtobufCFieldDescriptor* src,
                               ProtobufCFieldDescriptor* dest)
{
  dest->name              = mem_strdup(instance_, src->name);
  dest->c_name            = mem_strdup(instance_, src->c_name);
  dest->id                = src->id;
  dest->label             = src->label;
  dest->type              = src->type;
  dest->quantifier_offset = src->quantifier_offset;
  dest->offset            = src->offset;
  dest->descriptor        = src->descriptor;
  dest->default_value     = src->default_value; //ATTN: Check this
  dest->flags             = src->flags;
  dest->reserved_flags    = src->reserved_flags;
  dest->reserved2         = src->reserved2;
  dest->reserved3         = src->reserved3;
  dest->rw_flags          = src->rw_flags;
  dest->rw_flags         |= RW_PROTOBUF_FOPT_DYNAMIC;
  dest->rw_inline_max     = src->rw_inline_max;
  dest->data_size         = src->data_size;
  dest->ctype             = src->ctype;
}

ProtobufCIntRange*
PbSchemaMerger::create_pbc_field_ranges(const ProtobufCFieldDescriptor* fdescs,
                                        int n_values,
                                        unsigned* n_ranges)
{
  *n_ranges = 1;
  for (int i = 1; i < n_values; i++) {
    if (fdescs[i-1].id + 1 != fdescs[i].id)
      (*n_ranges)++;
  }

  auto ranges = (ProtobufCIntRange *)mem_zalloc(
      instance_, ((*n_ranges + 1) *sizeof(ProtobufCIntRange)));

  int last_range_start = 0, index = 0;

  for (int i = 1; i < n_values; i++) {

    if (fdescs[i-1].id + 1 != fdescs[i].id) {

      int count = i - last_range_start;
      int expected = fdescs[i-1].id + 1;

      ranges[index].start_value = expected - count;
      ranges[index++].orig_index = last_range_start;
      last_range_start = i;
    }
  }
  // write last real entry
  int i = n_values;
  int count = i - last_range_start;
  int expected = fdescs[i-1].id + 1;

  ranges[index].start_value = expected - count;
  ranges[index++].orig_index = last_range_start;

  // write sentinel entry
  ranges[index].start_value = 0;
  ranges[index++].orig_index = n_values;
  return ranges;
}

void
PbSchemaMerger::create_pbc_fdescs(const ProtobufCMessageDescriptor* mdesc1,
                                  const ProtobufCMessageDescriptor* mdesc2,
                                  rw_yang_pb_msgdesc_t* merged,
                                  ProtobufCMessageDescriptor* m_mdesc)
{
  tag_msg_umap_t cmsgs;
  for (size_t i = 0; i < merged->child_msg_count; ++i) {
    auto child = merged->child_msgs[i];
    cmsgs.emplace(child->pb_element_tag, child);
  }

  // The fields should be sorted based on tag
  tag_fdesc_map_t tag_map;
  for (size_t i = 0; i < mdesc1->n_fields; ++i) {

    auto field = &mdesc1->fields[i];
    tag_map.emplace(field->id, field);
  }

  for (size_t i = 0; i < mdesc2->n_fields; ++i) {

    auto field = &mdesc2->fields[i];
    auto it = tag_map.find(field->id);

    if (it == tag_map.end()) {
      tag_map.emplace(field->id, field);
    }
  }

  m_mdesc->sizeof_message = sizeof (ProtobufCMessage);
  auto fdescs = (ProtobufCFieldDescriptor *)mem_zalloc(
      instance_, tag_map.size() * sizeof(ProtobufCFieldDescriptor));

  m_mdesc->n_fields = 0;
  m_mdesc->fields = fdescs;

  fn_index_map_t fname_map;
  for (auto& f:tag_map) {

    // Duplicate field names not allowed for now.
    auto it = fname_map.find(f.second->name);
    RW_ASSERT(it == fname_map.end());

    ProtobufCFieldDescriptor* field = &fdescs[m_mdesc->n_fields];
    fill_pbc_fdesc(f.second, field);

    if (field->quantifier_offset) {

      size_t fsize = 0;
      if (field->label == PROTOBUF_C_LABEL_OPTIONAL) {
        fsize = sizeof (protobuf_c_boolean);
      } else if (field->label == PROTOBUF_C_LABEL_REPEATED) {
        fsize = sizeof (size_t);
      } else {
        RW_ASSERT_NOT_REACHED();
      }
      m_mdesc->sizeof_message += (m_mdesc->sizeof_message % fsize);

      field->quantifier_offset = m_mdesc->sizeof_message;
      m_mdesc->sizeof_message += fsize;
    }

    if (field->type == PROTOBUF_C_TYPE_MESSAGE) {

      auto it = cmsgs.find(field->id);
      RW_ASSERT(it != cmsgs.end());

      field->msg_desc = it->second->pbc_mdesc;
      field->data_size = field->msg_desc->sizeof_message;

    } else if (field->type == PROTOBUF_C_TYPE_ENUM) {
      RW_ASSERT(f.second->enum_desc);
      field->enum_desc = f.second->enum_desc; // Regenerate it?
    }

    size_t fsize = protobuf_c_message_get_field_size(field);
    RW_ASSERT(fsize);

    m_mdesc->sizeof_message += (m_mdesc->sizeof_message % fsize);
    field->offset = m_mdesc->sizeof_message;
    m_mdesc->sizeof_message += fsize;

    fname_map.emplace(field->name, m_mdesc->n_fields++);
  }
  //Make sure the structure is 8 byte alligned
  m_mdesc->sizeof_message += (m_mdesc->sizeof_message % sizeof(void*));

  // Fields sorted by name
  unsigned *sname = (unsigned *)mem_alloc(instance_, m_mdesc->n_fields * sizeof(unsigned));
  unsigned count = 0;
  for (auto& field:fname_map) {
    sname[count++] = field.second;
  }
  m_mdesc->fields_sorted_by_name = sname;

  // Field ranges
  m_mdesc->n_field_ranges = 0;
  m_mdesc->field_ranges = create_pbc_field_ranges(
      fdescs, m_mdesc->n_fields, &m_mdesc->n_field_ranges);
  RW_ASSERT(m_mdesc->field_ranges);
}

void
PbSchemaMerger::create_root_pbc_mdesc(rw_yang_pb_msgdesc_t* merged)
{
  auto pbc_mdesc = (ProtobufCMessageDescriptor *)mem_zalloc(
      instance_, sizeof(ProtobufCMessageDescriptor));

  const char* schema_mn = unified_->schema_module->module_name;
  RW_ASSERT(schema_mn);
  std::string name = std::string(schema_mn) + ".YangData";

  std::string ccase_mn = YangModel::mangle_to_camel_case(schema_mn) + "__YangData";

  pbc_mdesc->magic        = PROTOBUF_C_MESSAGE_DESCRIPTOR_MAGIC;
  pbc_mdesc->name         = mem_strdup(instance_, name.c_str());
  pbc_mdesc->short_name   = mem_strdup(instance_, "YangData");
  pbc_mdesc->c_name       = mem_strdup(instance_, ccase_mn.c_str());
  pbc_mdesc->package_name = mem_strdup(instance_, schema_mn);
  pbc_mdesc->n_fields     = merged->child_msg_count;
  pbc_mdesc->rw_flags    |= RW_PROTOBUF_MOPT_DYNAMIC;
  pbc_mdesc->ypbc_mdesc   = merged;
  merged->pbc_mdesc       = pbc_mdesc;

  pbc_mdesc->sizeof_message = sizeof (ProtobufCMessage);
  auto fdescs = (ProtobufCFieldDescriptor *)mem_zalloc(
      instance_, pbc_mdesc->n_fields * sizeof(ProtobufCFieldDescriptor));
  pbc_mdesc->fields = fdescs;
  pbc_mdesc->sizeof_message = sizeof (ProtobufCMessage);

  tag_msg_map_t tag_fmap;
  for (unsigned i = 0; i < merged->child_msg_count; ++i) {
    tag_fmap.emplace(merged->child_msgs[i]->pb_element_tag, merged->child_msgs[i]);
  }

  fn_index_map_t fname_map;
  unsigned i = 0;
  for (auto& cmsgs:tag_fmap) {

    auto child_msg = cmsgs.second;
    ProtobufCFieldDescriptor *fd = &fdescs[i];

    std::string pn = YangModel::mangle_camel_case_to_underscore(
        YangModel::mangle_to_c_identifier(child_msg->module->module_name).c_str());
    
    fd->name       = mem_strdup(instance_, pn.c_str());
    fd->c_name     = mem_strdup(instance_, pn.c_str());
    fd->id         = child_msg->pb_element_tag;
    fd->label      = PROTOBUF_C_LABEL_OPTIONAL;
    fd->type       = PROTOBUF_C_TYPE_MESSAGE;
    fd->rw_flags  |= RW_PROTOBUF_FOPT_DYNAMIC;
    fd->msg_desc   = child_msg->pbc_mdesc;
    fd->data_size  = child_msg->pbc_mdesc->sizeof_message;
    size_t fsize   = sizeof(void*);
    pbc_mdesc->sizeof_message += (pbc_mdesc->sizeof_message % fsize);
    fd->offset     = pbc_mdesc->sizeof_message;
    pbc_mdesc->sizeof_message += fsize;

    fname_map.emplace(pn, i++);
  }

  pbc_mdesc->sizeof_message += (pbc_mdesc->sizeof_message % sizeof(void*));

  // Fields sorted by name
  unsigned *sname = (unsigned *)mem_alloc(instance_, pbc_mdesc->n_fields * sizeof(unsigned));
  unsigned count = 0;
  for (auto& field:fname_map) {
    sname[count++] = field.second;
  }
  pbc_mdesc->fields_sorted_by_name = sname;

  // Field ranges
  pbc_mdesc->n_field_ranges = 0;
  pbc_mdesc->field_ranges = create_pbc_field_ranges(
      fdescs, pbc_mdesc->n_fields, &pbc_mdesc->n_field_ranges);
  RW_ASSERT(pbc_mdesc->field_ranges);

  protobuf_c_message_create_init_value (instance_->pbc_instance, pbc_mdesc);
  RW_ASSERT(pbc_mdesc->init_value);

  pbc_mdesc->debug_stats = (ProtobufCMessageDebugStats*)mem_zalloc(
      instance_, sizeof(ProtobufCMessageDebugStats));
}

void
PbSchemaMerger::create_pbc_mdesc(const ProtobufCMessageDescriptor* mdesc1,
                                 const ProtobufCMessageDescriptor* mdesc2,
                                 rw_yang_pb_msgdesc_t* merged)
{
  auto pbc_mdesc = (ProtobufCMessageDescriptor *)mem_zalloc(
      instance_, sizeof(ProtobufCMessageDescriptor));

  const char* schema_mn = merged->module->schema->schema_module->module_name;
  RW_ASSERT(schema_mn);

  std::string name(mdesc1->name);
  std::size_t dot = name.find('.');
  if (dot != std::string::npos) {
    name.replace(0, dot, schema_mn);
  }

  std::string ccase_mn = YangModel::mangle_to_camel_case(schema_mn);
  std::string cname(mdesc1->c_name);
  dot = cname.find('_');
  if (dot != std::string::npos) {
    cname.replace(0, dot, ccase_mn);
  }

  pbc_mdesc->magic        = PROTOBUF_C_MESSAGE_DESCRIPTOR_MAGIC;
  pbc_mdesc->name         = mem_strdup(instance_, name.c_str());
  pbc_mdesc->short_name   = mem_strdup(instance_, mdesc1->short_name);
  pbc_mdesc->c_name       = mem_strdup(instance_, cname.c_str());
  pbc_mdesc->package_name = mem_strdup(instance_, schema_mn);
  pbc_mdesc->n_fields     = merged->num_fields;
  pbc_mdesc->rw_flags     = mdesc1->rw_flags;
  pbc_mdesc->rw_flags    &= ~RW_PROTOBUF_MOPT_FLAT;
  pbc_mdesc->rw_flags    |= RW_PROTOBUF_MOPT_DYNAMIC;
  pbc_mdesc->ypbc_mdesc   = merged;

  create_pbc_fdescs (mdesc1, mdesc2, merged, pbc_mdesc);
  RW_ASSERT(pbc_mdesc->fields);

  protobuf_c_message_create_init_value (instance_->pbc_instance, pbc_mdesc);
  RW_ASSERT(pbc_mdesc->init_value);

  pbc_mdesc->debug_stats = (ProtobufCMessageDebugStats*)mem_zalloc(
      instance_, sizeof(ProtobufCMessageDebugStats));

  merged->pbc_mdesc = pbc_mdesc;
}

void
PbSchemaMerger::fill_ypbc_fdesc(const rw_yang_pb_flddesc_t* o_fdesc,
                                rw_yang_pb_flddesc_t* n_fdesc,
                                const rw_yang_pb_msgdesc_t* parent,
                                unsigned yang_order)
{
  if (!strcmp (parent->module->ns, o_fdesc->module->ns)) {
    n_fdesc->module         = parent->module;
  } else {
    n_fdesc->module         = o_fdesc->module;
  }
  n_fdesc->ypbc_msgdesc   = parent;
  n_fdesc->yang_node_name = mem_strdup(instance_, o_fdesc->yang_node_name);
  n_fdesc->stmt_type      = o_fdesc->stmt_type;
  n_fdesc->leaf_type      = o_fdesc->leaf_type;
  n_fdesc->yang_order     = yang_order;
  
  auto fdesc = protobuf_c_message_descriptor_get_field(parent->pbc_mdesc, o_fdesc->pbc_fdesc->id);
  RW_ASSERT(fdesc);

  n_fdesc->pbc_fdesc = fdesc;
  n_fdesc->pbc_order = ((unsigned)(fdesc - parent->pbc_mdesc->fields))+1;
}

void 
PbSchemaMerger::create_ypbc_fdesc(const rw_yang_pb_msgdesc_t* msg1,
                                  const rw_yang_pb_msgdesc_t* msg2,
                                  rw_yang_pb_msgdesc_t* merged)
{
  RW_ASSERT(merged->pbc_mdesc);
  merged->num_fields = merged->pbc_mdesc->n_fields;

  auto fdescs = (rw_yang_pb_flddesc_t*)mem_alloc(
      instance_, merged->num_fields * sizeof(rw_yang_pb_flddesc_t));

  yn_fdesc_umap_t fmap;
  size_t fcount = 0;
  unsigned yang_order = 1;

  for (size_t i = 0; i < msg1->num_fields; ++i) {

    auto field = &msg1->ypbc_flddescs[i];
    fill_ypbc_fdesc (field, &fdescs[fcount], merged, yang_order++);

    fmap.emplace(fdescs[fcount].yang_node_name, &fdescs[fcount]);
    fcount++;
  }

  for (size_t i = 0; i < msg2->num_fields; ++i) {

    auto field = &msg2->ypbc_flddescs[i];
    auto it = fmap.find(field->yang_node_name);

    if (it == fmap.end()) {
      fill_ypbc_fdesc (field, &fdescs[fcount], merged, yang_order++);
      fcount++;
    }
  }

  RW_ASSERT (fcount == merged->num_fields);
  merged->ypbc_flddescs = fdescs;
}

rw_yang_pb_msgdesc_t*
PbSchemaMerger::merge_messages(const rw_yang_pb_msgdesc_t* msg1,
                               const rw_yang_pb_msgdesc_t* msg2)
{
  RW_ASSERT(msg1 != msg2);
  RW_ASSERT(!strcmp(msg1->yang_node_name, msg2->yang_node_name));
  RW_ASSERT(msg1->msg_type == msg2->msg_type);

  rw_yang_pb_msgdesc_t* merged = (rw_yang_pb_msgdesc_t *)mem_zalloc(
      instance_, sizeof(rw_yang_pb_msgdesc_t));

  if (!strcmp(msg1->module->ns, merged_mod_->ns)) {
    merged->module         = merged_mod_;
  } else {
    // This message belongs to different name-space.
    const rw_yang_pb_module_t* msg_mod = nullptr;

    auto it = module_map_.find(std::string(msg1->module->ns));
    if (it != module_map_.end()) {
      msg_mod = it->second;
    } else {
      msg_mod = create_and_init_new_module(msg1->module, msg2->module);
    }
    RW_ASSERT (msg_mod && (msg_mod->schema == unified_));
    merged->module = msg_mod;
  }
  merged->yang_node_name = mem_strdup(instance_, msg1->yang_node_name);
  merged->pb_element_tag = msg1->pb_element_tag;
  merged->msg_type       = msg1->msg_type;

  create_pathentry_mdesc (msg1->schema_entry_ypbc_desc, merged);
  RW_ASSERT(merged->schema_entry_ypbc_desc);
  RW_ASSERT(merged->schema_entry_value);

  yn_msg_umap_t mmap;
  for (size_t i = 0; i < msg1->child_msg_count; ++i) {
    auto child = msg1->child_msgs[i];
    mmap.emplace(child->yang_node_name, child);
  }

  for (size_t i = 0; i < msg2->child_msg_count; ++i) {

    auto child = msg2->child_msgs[i];
    auto it = mmap.find(child->yang_node_name);

    if ((it != mmap.end()) && (it->second != child)) {

      auto res = compare_messages(it->second, child);
      if (!res) {

        auto mnew = merge_messages(it->second, child);
        RW_ASSERT(mnew);
        mnew->parent = merged;

        auto entry_mdesc = (rw_yang_pb_msgdesc_t *)mnew->schema_entry_ypbc_desc;
        entry_mdesc->parent = merged->schema_entry_ypbc_desc;
        it->second = mnew;

      } else {
        it->second = res;
      }
    } else {
      if (it == mmap.end()) {
        mmap.emplace(child->yang_node_name, child);
      }
    }
  }

  if (mmap.size()) {

    merged->child_msgs = (rw_yang_pb_msgdesc_t **)mem_alloc(
        instance_, mmap.size() * sizeof(void*));

    auto child_msgs = (const rw_yang_pb_msgdesc_t **)(merged->child_msgs);
    for (size_t i = 0; i < msg1->child_msg_count; ++i) {

      auto child = msg1->child_msgs[i];
      auto it = mmap.find(child->yang_node_name);

      RW_ASSERT(it != mmap.end());
      child_msgs[merged->child_msg_count++] = it->second; 
      mmap.erase(it);
    }

    for (size_t i = 0; i < msg2->child_msg_count; ++i) {

      auto child = msg2->child_msgs[i];
      auto it = mmap.find(child->yang_node_name);

      if (it != mmap.end()) {
        child_msgs[merged->child_msg_count++] = it->second;
      }
    }
  }

  create_pbc_mdesc (msg1->pbc_mdesc, msg2->pbc_mdesc, merged);
  RW_ASSERT(merged->pbc_mdesc);

  create_ypbc_fdesc (msg1, msg2, merged);
  RW_ASSERT(merged->ypbc_flddescs);

  return merged;
}

const rw_yang_pb_msgdesc_t*
PbSchemaMerger::compare_messages(const rw_yang_pb_msgdesc_t* msg1,
                                 const rw_yang_pb_msgdesc_t* msg2)
{
  RW_ASSERT(msg1 != msg2);
  RW_ASSERT(!strcmp(msg1->yang_node_name, msg2->yang_node_name));
  RW_ASSERT(msg1->msg_type == msg2->msg_type);

  size_t n_fields1 = msg1->num_fields;
  size_t n_fields2 = msg2->num_fields;

  yn_tag_map_t fmap1, fmap2;
  for (size_t i = 0; i < n_fields1; ++i) {
    auto field = &msg1->ypbc_flddescs[i];
    fmap1.emplace(field->yang_node_name, field->pbc_fdesc->id);
  }

  for (size_t i = 0; i < n_fields2; ++i) {
    auto field = &msg2->ypbc_flddescs[i];
    fmap2.emplace(field->yang_node_name, field->pbc_fdesc->id);
  }

  const rw_yang_pb_msgdesc_t* smsg = nullptr;
  if (n_fields1 < n_fields2) {
    if (!std::includes(fmap2.begin(), fmap2.end(), fmap1.begin(), fmap1.end())) {
      return nullptr;
    }
    smsg = msg2;
  } else {
    if (!std::includes(fmap1.begin(), fmap1.end(), fmap2.begin(), fmap2.end())) {
      return nullptr;
    }
    if (n_fields1 > n_fields2) {
      smsg = msg1;
    }
  }

  yn_msg_map_t mmap1, mmap2;
  for (size_t i = 0; i < msg1->child_msg_count; ++i) {
    auto child = msg1->child_msgs[i];
    mmap1.emplace(child->yang_node_name, child);
  }

  for (size_t i = 0; i < msg2->child_msg_count; ++i) {
    auto child = msg2->child_msgs[i];
    mmap2.emplace(child->yang_node_name, child);
  }

  for (auto& cm:mmap1) {
    auto it = mmap2.find(cm.first);
    if (it != mmap2.end()) {
      if (it->second != cm.second) {
        auto res = compare_messages(it->second, cm.second);
        if (!res) {
          return nullptr;
        }
        const rw_yang_pb_msgdesc_t* cparent = (res == it->second)?msg2:msg1;
        if (smsg && cparent != smsg) {
          return nullptr;
        }
        smsg = cparent;
      }
    }
  }

  if (!smsg) {
    // This can happen when both the messages are identical, pick one.
    RW_ASSERT (n_fields1 == n_fields2);
    smsg = msg1; // Pick one.
  }

  return smsg;
}

void
PbSchemaMerger::copy_pbc_mdesc(const ProtobufCMessageDescriptor* src,
                               ProtobufCMessageDescriptor* dest,
                               const rw_yang_pb_msgdesc_t* ypbc_mdesc)
{
  const char* schema_mn = unified_->schema_module->module_name;
  RW_ASSERT( schema_mn );

  std::string name(src->name);
  std::size_t dot = name.find('.');
  if (dot != std::string::npos) {
    name.replace(0, dot, schema_mn);
  }

  std::string ccase_mn = YangModel::mangle_to_camel_case(schema_mn);
  std::string cname(src->c_name);
  dot = cname.find('_');
  if (dot != std::string::npos) {
    cname.replace(0, dot, ccase_mn);
  }

  dest->magic          = src->magic;
  dest->name           = mem_strdup(instance_, name.c_str());
  dest->short_name     = mem_strdup(instance_, src->short_name);
  dest->c_name         = mem_strdup(instance_, cname.c_str());
  dest->package_name   = mem_strdup(instance_, schema_mn);
  dest->sizeof_message = src->sizeof_message;
  dest->n_fields       = src->n_fields;

  size_t fsize = src->n_fields * sizeof(ProtobufCFieldDescriptor);
  dest->fields = (ProtobufCFieldDescriptor *)mem_alloc(instance_, fsize);

  for (unsigned i = 0; i < dest->n_fields; ++i) {

    auto fsrc = &src->fields[i];
    auto fdest = (ProtobufCFieldDescriptor *)&dest->fields[i];
    copy_pbc_fdesc(fsrc, fdest);
  }

  // fields by name
  fsize = dest->n_fields * sizeof(unsigned);
  dest->fields_sorted_by_name = (unsigned *)mem_alloc(instance_, fsize);
  memcpy ((void*)dest->fields_sorted_by_name, src->fields_sorted_by_name, fsize);

  // field ranges
  dest->n_field_ranges = src->n_field_ranges;
  fsize = (dest->n_field_ranges + 1) * sizeof(ProtobufCIntRange);

  dest->field_ranges = (ProtobufCIntRange *)mem_alloc(instance_, fsize);
  memcpy ((void*)dest->field_ranges, src->field_ranges, fsize);

  dest->init_value = nullptr;
  dest->gi_descriptor = nullptr;
  dest->reserved1 = dest->reserved2 = dest->reserved3 = nullptr;
  dest->rw_flags = src->rw_flags;
  dest->rw_flags |= RW_PROTOBUF_MOPT_DYNAMIC;

  dest->debug_stats = (ProtobufCMessageDebugStats*)mem_zalloc(
      instance_, sizeof(ProtobufCMessageDebugStats));
}

void
PbSchemaMerger::create_pathentry_mdesc(const rw_yang_pb_msgdesc_t* fentry,
                                       rw_yang_pb_msgdesc_t* merged)
{
  auto pe_mdesc = (rw_yang_pb_msgdesc_t*)mem_zalloc (
      instance_, sizeof (rw_yang_pb_msgdesc_t));

  pe_mdesc->module = merged->module;
  pe_mdesc->yang_node_name = mem_strdup(instance_, fentry->yang_node_name);
  pe_mdesc->pb_element_tag = fentry->pb_element_tag;
  pe_mdesc->msg_type       = RW_YANGPBC_MSG_TYPE_SCHEMA_ENTRY;

  auto pbc_mdesc = (ProtobufCMessageDescriptor *)mem_zalloc (
      instance_, sizeof (ProtobufCMessageDescriptor));

  pe_mdesc->pbc_mdesc = pbc_mdesc;
  pbc_mdesc->ypbc_mdesc = pe_mdesc;
  copy_pbc_mdesc (fentry->pbc_mdesc, pbc_mdesc, merged);

  protobuf_c_message_create_init_value (instance_->pbc_instance, pbc_mdesc);
  RW_ASSERT (pbc_mdesc->init_value);

  merged->schema_entry_ypbc_desc = pe_mdesc;
  pe_mdesc->u = (rw_yang_pb_msgdesc_union_t *)merged;

  // Global path entry value
  merged->schema_entry_value = rw_keyspec_entry_create_dup_of_type (
      fentry->u->msg_msgdesc.schema_entry_value, NULL, pbc_mdesc);
  RW_ASSERT (merged->schema_entry_value);
}

void
PbSchemaMerger::create_pathspec_mdesc(const rw_yang_pb_msgdesc_t* mdesc1,
                                      const rw_yang_pb_msgdesc_t* mdesc2,
                                      rw_yang_pb_msgdesc_t* merged,
                                      size_t depth)
{

  const rw_yang_pb_msgdesc_t* fmdesc = nullptr;

  if (mdesc1) fmdesc = mdesc1;
  else fmdesc = mdesc2;

  RW_ASSERT(fmdesc);

  auto ks_mdesc = (rw_yang_pb_msgdesc_t*)mem_zalloc(instance_, sizeof(rw_yang_pb_msgdesc_t));

  ks_mdesc->module         = merged->module;
  ks_mdesc->yang_node_name = mem_strdup(instance_, merged->yang_node_name);
  ks_mdesc->pb_element_tag = merged->pb_element_tag;
  ks_mdesc->msg_type       = RW_YANGPBC_MSG_TYPE_SCHEMA_PATH;

  auto pbc_mdesc = (ProtobufCMessageDescriptor *)mem_zalloc (
      instance_, sizeof(ProtobufCMessageDescriptor));

  ks_mdesc->pbc_mdesc = pbc_mdesc;
  pbc_mdesc->ypbc_mdesc = ks_mdesc;

  copy_pbc_mdesc (fmdesc->schema_path_ypbc_desc->pbc_mdesc, pbc_mdesc, merged);
  merged->schema_path_ypbc_desc = ks_mdesc;
  ks_mdesc->u = (rw_yang_pb_msgdesc_union_t *)merged;

  auto dom_fd = (ProtobufCFieldDescriptor*)
      protobuf_c_message_descriptor_get_field (pbc_mdesc, RW_SCHEMA_TAG_KEYSPEC_DOMPATH);
  RW_ASSERT(dom_fd);

  //Create new dompath pbc mdesc
  auto dom_mdesc = (ProtobufCMessageDescriptor *)mem_zalloc(instance_, sizeof(ProtobufCMessageDescriptor));
  copy_pbc_mdesc (dom_fd->msg_desc, dom_mdesc, merged);
  dom_fd->msg_desc = dom_mdesc;

  //Change the pathentry pbc descriptors to current schema's values.
  const rw_yang_pb_msgdesc_t* curr_msg = merged;

  if (merged->msg_type == RW_YANGPBC_MSG_TYPE_MODULE_ROOT ||
      merged->msg_type == RW_YANGPBC_MSG_TYPE_ROOT) {

    unsigned tag = 0;
    if (merged->msg_type == RW_YANGPBC_MSG_TYPE_MODULE_ROOT) {
      tag = RW_SCHEMA_TAG_PATH_ENTRY_MODULE_ROOT;
    } else {
      tag = RW_SCHEMA_TAG_PATH_ENTRY_ROOT;
    }
    auto pe_fd = (ProtobufCFieldDescriptor*)protobuf_c_message_descriptor_get_field(dom_mdesc, tag);

    RW_ASSERT(pe_fd);
    RW_ASSERT(curr_msg->schema_entry_ypbc_desc);

    pe_fd->msg_desc = curr_msg->schema_entry_ypbc_desc->pbc_mdesc;

  } else {

    for (int i = depth-1; i >= 0; --i) {
      unsigned tag = RW_SCHEMA_TAG_PATH_ENTRY_START + i;
      auto pe_fd = (ProtobufCFieldDescriptor*)protobuf_c_message_descriptor_get_field(dom_mdesc, tag);
      RW_ASSERT(pe_fd);

      RW_ASSERT(curr_msg);
      RW_ASSERT(curr_msg->schema_entry_ypbc_desc);

      pe_fd->msg_desc = curr_msg->schema_entry_ypbc_desc->pbc_mdesc;
      curr_msg = curr_msg->parent;
    }
  }

  protobuf_c_message_create_init_value (instance_->pbc_instance, dom_mdesc);
  RW_ASSERT(dom_mdesc->init_value);

  protobuf_c_message_create_init_value (instance_->pbc_instance, pbc_mdesc);
  RW_ASSERT(pbc_mdesc->init_value);

  merged->schema_path_value = rw_keyspec_path_create_dup_of_type (
      fmdesc->schema_path_value, NULL, pbc_mdesc);
  RW_ASSERT(merged->schema_path_value);

  if (merged->msg_type == RW_YANGPBC_MSG_TYPE_ROOT) {
    return;
  }

  yn_msg_umap_t cumap1, cumap2;
  if (mdesc1) {
    for (size_t i = 0; i < mdesc1->child_msg_count; ++i) {
      auto child = mdesc1->child_msgs[i];
      cumap1.emplace(child->yang_node_name, child);
    }
  }

  if (mdesc2) {
    for (size_t i = 0; i < mdesc2->child_msg_count; ++i) {
      auto child = mdesc2->child_msgs[i];
      cumap2.emplace(child->yang_node_name, child);
    }
  }

  for (size_t i = 0; i < merged->child_msg_count; ++i) {

    if (merged->child_msgs[i]->module->schema == merged->module->schema) {

      auto mchild = (rw_yang_pb_msgdesc_t*)merged->child_msgs[i];
      const rw_yang_pb_msgdesc_t *child1 = nullptr, *child2 = nullptr;

      auto it1 = cumap1.find(mchild->yang_node_name);
      if (it1 != cumap1.end()) {
        child1 = it1->second;
      }

      auto it2 = cumap2.find(mchild->yang_node_name);
      if (it2 != cumap2.end()) {
        child2 = it2->second;
      }

      create_pathspec_mdesc (child1, child2, mchild, depth+1);
    }
  }
}

void
PbSchemaMerger::populate_rpcio_module_root()
{
  auto rpci1 = merge_mod1_->rpci_module;
  auto rpci2 = merge_mod2_->rpci_module;

  rw_yang_pb_msgdesc_t* mrpci = (rw_yang_pb_msgdesc_t *)merged_mod_->rpci_module;
  RW_ASSERT(mrpci->child_msg_count == rpci1->child_msg_count);
  RW_ASSERT(rpci_msgs_.size() == mrpci->child_msg_count);

  auto rpco1 = merge_mod1_->rpco_module;
  auto rpco2 = merge_mod2_->rpco_module;

  rw_yang_pb_msgdesc_t* mrpco = (rw_yang_pb_msgdesc_t *)merged_mod_->rpco_module;
  RW_ASSERT(mrpco->child_msg_count == rpco1->child_msg_count);

  RW_ASSERT(rpco_msgs_.size() == mrpco->child_msg_count);
  RW_ASSERT(mrpci->child_msg_count == mrpco->child_msg_count);

  auto rpci_cmsgs = (rw_yang_pb_msgdesc_t **)mrpci->child_msgs;
  auto rpco_cmsgs = (rw_yang_pb_msgdesc_t **)mrpco->child_msgs;

  unsigned mcount = rpci_msgs_.size();
  for (unsigned i = 0; i < mcount; ++i) {

    rpci_cmsgs[i] = (rw_yang_pb_msgdesc_t *)rpci_msgs_[i];
    rpco_cmsgs[i] = (rw_yang_pb_msgdesc_t *)rpco_msgs_[i];

    RW_ASSERT(rpci_cmsgs[i]->msg_type == RW_YANGPBC_MSG_TYPE_RPC_INPUT);
    RW_ASSERT(rpco_cmsgs[i]->msg_type == RW_YANGPBC_MSG_TYPE_RPC_OUTPUT);
    RW_ASSERT(!strcmp(rpci_cmsgs[i]->yang_node_name, rpco_cmsgs[i]->yang_node_name));

    if (rpci_cmsgs[i]->module == merged_mod_ ||
        rpco_cmsgs[i]->module == merged_mod_) {

      auto rpcdef = (rw_yang_pb_rpcdef_t*)mem_alloc(instance_, sizeof(rw_yang_pb_rpcdef_t));

      rpcdef->module = merged_mod_;
      rpcdef->yang_node_name = mem_strdup(instance_, rpci_cmsgs[i]->yang_node_name);
      rpcdef->input  = rpci_cmsgs[i];
      rpcdef->output = rpco_cmsgs[i];

      if (rpci_cmsgs[i]->module == merged_mod_) {
        rpci_cmsgs[i]->u = (rw_yang_pb_msgdesc_union_t *)rpcdef;
      }

      if (rpco_cmsgs[i]->module == merged_mod_) {
        rpco_cmsgs[i]->u = (rw_yang_pb_msgdesc_union_t *)rpcdef;
      }
    }
  }

  create_pbc_mdesc (rpci1->pbc_mdesc, rpci2->pbc_mdesc, mrpci);
  RW_ASSERT(mrpci->pbc_mdesc);

  create_pbc_mdesc (rpco1->pbc_mdesc, rpco2->pbc_mdesc, mrpco);
  RW_ASSERT(mrpco->pbc_mdesc);

  // Create keyspec top-down!
  create_pathspec_mdesc(rpci1, rpci2, mrpci, 0);
  create_pathspec_mdesc(rpco1, rpco2, mrpco, 0);
}

void
PbSchemaMerger::populate_notif_module_root()
{
  RW_ASSERT(merged_mod_->notif_module);

  auto root  = (rw_yang_pb_msgdesc_t *)merged_mod_->notif_module;
  auto root1 = merge_mod1_->notif_module;
  auto root2 = merge_mod2_->notif_module;

  RW_ASSERT(notif_msgs_.size() == root->child_msg_count);
  auto child_msgs = (rw_yang_pb_msgdesc_t **)root->child_msgs;

  for (size_t i = 0; i < root->child_msg_count; ++i) {
    child_msgs[i] = (rw_yang_pb_msgdesc_t*) notif_msgs_[i];
  }

  create_pbc_mdesc (root1->pbc_mdesc, root2->pbc_mdesc, root);
  RW_ASSERT (root->pbc_mdesc);

  create_ypbc_fdesc (root1, root2, root);
  RW_ASSERT (root->ypbc_flddescs);

  // Create keyspecs top-down
  create_pathspec_mdesc (root1, root2, root, 0);
}

void
PbSchemaMerger::populate_data_module_root()
{
  auto data_module = (rw_yang_pb_msgdesc_t *) merged_mod_->data_module;
  RW_ASSERT(data_module);

  auto root1 = merge_mod1_->data_module;
  auto root2 = merge_mod2_->data_module;

  RW_ASSERT (data_msgs_.size() == data_module->child_msg_count);
  auto child_msgs = (rw_yang_pb_msgdesc_t **)data_module->child_msgs;

  for (size_t i = 0; i < data_module->child_msg_count; ++i) {
    child_msgs[i] = (rw_yang_pb_msgdesc_t*) data_msgs_[i];
  }

  create_pbc_mdesc (root1->pbc_mdesc, root2->pbc_mdesc, data_module);
  RW_ASSERT (data_module->pbc_mdesc);

  create_ypbc_fdesc (root1, root2, data_module);
  RW_ASSERT (data_module->ypbc_flddescs);

  // Create keyspecs top-down
  create_pathspec_mdesc (root1, root2, data_module, 0);
}

rw_yang_pb_msgdesc_t*
PbSchemaMerger::create_and_init_root_msg(const rw_yang_pb_msgdesc_t* in_root,
                                         const rw_yang_pb_module_t* out_mod)
{
  rw_yang_pb_msgdesc_t* out_root = (rw_yang_pb_msgdesc_t *)mem_zalloc(
      instance_, sizeof(rw_yang_pb_msgdesc_t));

  out_root->module          = out_mod;
  out_root->yang_node_name  = mem_strdup(instance_, "data");
  out_root->msg_type        = RW_YANGPBC_MSG_TYPE_MODULE_ROOT;
  out_root->pb_element_tag  = in_root->pb_element_tag;
  out_root->num_fields      = in_root->num_fields;
  out_root->child_msg_count = in_root->child_msg_count;

  RW_ASSERT(out_root->child_msg_count);
  out_root->child_msgs = (rw_yang_pb_msgdesc_t **)
      mem_alloc(instance_, out_root->child_msg_count * sizeof(void*));

  RW_ASSERT(in_root->schema_entry_ypbc_desc);
  create_pathentry_mdesc (in_root->schema_entry_ypbc_desc, out_root);

  return out_root;
} 

void
PbSchemaMerger::create_and_init_schema_root_msg(const rw_yang_pb_msgdesc_t* cdata_root,
                                                size_t child_count)
{
  rw_yang_pb_msgdesc_t* data_root = (rw_yang_pb_msgdesc_t *)mem_zalloc(
      instance_, sizeof(rw_yang_pb_msgdesc_t));

  data_root->yang_node_name  = mem_strdup(instance_, "data");
  data_root->msg_type        = RW_YANGPBC_MSG_TYPE_ROOT;
  data_root->pb_element_tag  = 0;
  data_root->num_fields      = 0;
  data_root->child_msg_count = child_count;

  data_root->child_msgs = (rw_yang_pb_msgdesc_t **)
      mem_alloc(instance_, child_count * sizeof(void*));
  RW_ASSERT(data_root->child_msgs);

  RW_ASSERT(cdata_root->schema_entry_ypbc_desc);
  create_pathentry_mdesc (cdata_root->schema_entry_ypbc_desc, data_root);

  unified_->data_root = data_root;
}

rw_yang_pb_module_t*
PbSchemaMerger::create_and_init_new_module(const rw_yang_pb_module_t* mod1,
                                           const rw_yang_pb_module_t* mod2)
{

  auto merged_mod = (rw_yang_pb_module_t*)mem_zalloc(instance_, sizeof(rw_yang_pb_module_t));
  merged_mod->schema      = unified_;
  merged_mod->module_name = mem_strdup(instance_, mod1->module_name);
  merged_mod->ns          = mem_strdup(instance_, mod1->ns);
  merged_mod->prefix      = mem_strdup(instance_, mod1->prefix);

  if (mod1->data_module) {

    RW_ASSERT(mod2->data_module);
    RW_ASSERT(mod1->data_module->child_msg_count == mod2->data_module->child_msg_count);

    merged_mod->data_module = create_and_init_root_msg(mod1->data_module, merged_mod);
    RW_ASSERT(merged_mod->data_module);
  }

  if (mod1->rpci_module) {

    RW_ASSERT(mod2->rpci_module);
    RW_ASSERT(mod1->rpci_module->child_msg_count == mod2->rpci_module->child_msg_count);

    merged_mod->rpci_module = create_and_init_root_msg(mod1->rpci_module, merged_mod);
    RW_ASSERT(merged_mod->rpci_module);
  }

  if (mod1->rpco_module) {

    RW_ASSERT(mod2->rpco_module);
    RW_ASSERT(mod1->rpco_module->child_msg_count == mod2->rpco_module->child_msg_count);

    merged_mod->rpco_module = create_and_init_root_msg(mod1->rpco_module, merged_mod);
    RW_ASSERT(merged_mod->rpci_module);
  }

  if (mod1->notif_module) {

    RW_ASSERT(mod2->notif_module);
    RW_ASSERT(mod1->notif_module->child_msg_count == mod2->notif_module->child_msg_count);

    merged_mod->notif_module = create_and_init_root_msg(mod1->notif_module, merged_mod);
    RW_ASSERT(merged_mod->notif_module);
  }

  modules_.push_back(merged_mod);
  RW_ASSERT (module_map_.find(std::string(merged_mod->ns)) == module_map_.end());
  module_map_.emplace(std::string(merged_mod->ns), merged_mod);

  return merged_mod;
}

void PbSchemaMerger::merge_module_root(msg_list_t& msgs)
{
  size_t msg_count = root1_->child_msg_count;
  RW_ASSERT(msg_count == root2_->child_msg_count);

  yn_msg_umap_t cumap1, cumap2;
  // ATTN: Need a map here? The top msgs must be in the same order?
  for (size_t i = 0; i < msg_count; ++i) {
    auto child = root1_->child_msgs[i];
    cumap1.emplace(child->yang_node_name, child);
  }

  for (size_t i = 0; i < msg_count; ++i) {
    auto child = root2_->child_msgs[i];
    cumap2.emplace(child->yang_node_name, child);
  }

  ns_mod_map_t rmap;
  for (size_t i = 0; i < msg_count; ++i) {

    auto child1 = root1_->child_msgs[i];
    auto child2 = cumap2.find(child1->yang_node_name)->second;

    if (child1 != child2) {

      auto res = compare_messages(child1, child2);

      if (!res) {
        rmap.emplace(child1->yang_node_name, nullptr);
        if (!merged_mod_) {
          merged_mod_ = create_and_init_new_module(merge_mod1_, merge_mod2_);
          RW_ASSERT(merged_mod_);
        }
        msgs.push_back(merge_messages(child1, child2));
      } else {
        if (res == child1) {
          rmap.emplace(child1->yang_node_name, merge_mod1_);
        } else {
          rmap.emplace(child2->yang_node_name, merge_mod2_);
        }
        msgs.push_back(res);
      }
    } else {
      msgs.push_back(child1);
    }
  }

  if (!rmap.size()) {
    return; // Both are same.
  }

  if (!merged_mod_) {
    for (auto& cm:rmap) {
      if ((!cm.second) || (merged_mod_ && (merged_mod_ != cm.second))) {
        merged_mod_ = nullptr;
        break;
      }
      merged_mod_ = (rw_yang_pb_module_t *)cm.second;
    }
    if (merged_mod_) {
      return;
    }

    merged_mod_ = create_and_init_new_module(merge_mod1_, merge_mod2_);
    RW_ASSERT (merged_mod_);
  }
}

void PbSchemaMerger::clear_state()
{
  data_msgs_.clear();
  rpci_msgs_.clear();
  rpco_msgs_.clear();
  notif_msgs_.clear();

  merged_mod_ = nullptr;
  merge_mod1_ = merge_mod2_ = nullptr;

  root1_ = root2_ = nullptr;
}

void PbSchemaMerger::merge_augmented_module()
{
  RW_ASSERT(!strcmp(merge_mod1_->ns, merge_mod2_->ns));

  rw_yang_pb_module_t* smod_list[4] = {};
  unsigned nroot = 0;
  
  if (merge_mod1_->data_module) {
    RW_ASSERT(merge_mod2_->data_module);

    root1_ = merge_mod1_->data_module;
    root2_ = merge_mod2_->data_module;

    merge_module_root(data_msgs_);

    if (merged_mod_ ==  merge_mod1_ || 
        merged_mod_ ==  merge_mod2_) {
      smod_list[nroot++] = merged_mod_;
      merged_mod_ = nullptr;
    }
  }

  if (merge_mod1_->rpci_module) {
    RW_ASSERT(merge_mod2_->rpci_module);

    root1_ = merge_mod1_->rpci_module;
    root2_ = merge_mod2_->rpci_module;

    merge_module_root(rpci_msgs_);

    if (merged_mod_ ==  merge_mod1_ || 
        merged_mod_ ==  merge_mod2_) {
      smod_list[nroot++] = merged_mod_;
      merged_mod_ = nullptr;
    }

    RW_ASSERT(merge_mod1_->rpco_module);
    RW_ASSERT(merge_mod2_->rpco_module);

    root1_ = merge_mod1_->rpco_module;
    root2_ = merge_mod2_->rpco_module;

    merge_module_root(rpco_msgs_);

    if (merged_mod_ ==  merge_mod1_ || 
        merged_mod_ ==  merge_mod2_) {
      smod_list[nroot++] = merged_mod_;
      merged_mod_ = nullptr;
    }
  }

  if (merge_mod1_->notif_module) {
    RW_ASSERT(merge_mod2_->notif_module);

    root1_ = merge_mod1_->notif_module;
    root2_ = merge_mod2_->notif_module;

    merge_module_root(notif_msgs_);

    if (merged_mod_ ==  merge_mod1_ || 
        merged_mod_ ==  merge_mod2_) {
      smod_list[nroot++] = merged_mod_;
      merged_mod_ = nullptr;
    }
  }

  if (!merged_mod_) {

      rw_yang_pb_module_t* smod = smod_list[0];
      unsigned i = 0;
      for (i = 0; i < nroot; ++i) {
        if (!smod_list[i]) continue;
        if (!smod) {
          smod = smod_list[i];
        } else if (smod != smod_list[i]) {
          break;
        }
      }

    if (i == nroot && smod) {
      merged_mod_ = smod;
      modules_.push_back(merged_mod_);
      module_map_.emplace(std::string(merged_mod_->ns), merged_mod_);
      return;
    }

    merged_mod_ = create_and_init_new_module(merge_mod1_, merge_mod2_);
    RW_ASSERT (merged_mod_);

  }

  if (data_msgs_.size()) {
    populate_data_module_root();
  }
  if (rpci_msgs_.size()) {
    populate_rpcio_module_root();
  }
  if (notif_msgs_.size()) {
    populate_notif_module_root();
  }
}

void
PbSchemaMerger::create_schema_data_root()
{
  module_list_t data_mods;
  if (schema1_->data_root || schema2_->data_root) {
    for (unsigned i = 0; i < unified_->module_count; ++i) {
      auto mod = unified_->modules[i];
      if (mod->data_module) {
        data_mods.push_back(mod);
      }
    }

    if (data_mods.size()) {
      auto cdata_root = schema1_->data_root?schema1_->data_root:schema2_->data_root;
      create_and_init_schema_root_msg(cdata_root, data_mods.size());

      RW_ASSERT(unified_->data_root);
      rw_yang_pb_msgdesc_t *data_root = (rw_yang_pb_msgdesc_t *)unified_->data_root;

      auto child_msgs = (const rw_yang_pb_msgdesc_t **)data_root->child_msgs;
      for (unsigned i = 0; i < data_mods.size(); i++) {
        child_msgs[i] = data_mods[i]->data_module;
      }

      create_root_pbc_mdesc (data_root);
      create_pathspec_mdesc (cdata_root, nullptr, data_root, 0);
    }
  }
}

const rw_yang_pb_schema_t*
PbSchemaMerger::merge_schemas()
{
  if (schema1_ == schema2_) {
    return schema1_;
  }

  size_t s1_mod_count = schema1_->module_count;
  size_t s2_mod_count = schema2_->module_count;

  ns_mod_map_t s1_modules, s2_modules;

  for (size_t i = 0; i < s1_mod_count; i++) {
    auto module = schema1_->modules[i];
    s1_modules.emplace(std::string(module->ns), module);
  }

  for (size_t i = 0; i < s2_mod_count; i++) {
    auto module = schema2_->modules[i];
    s2_modules.emplace(std::string(module->ns), module);
  }

  auto it = s2_modules.find(schema1_->schema_module->ns);
  if (it != s2_modules.end()) {
    // schema2 already includes schema1
    return schema2_;
  }

  it = s1_modules.find(schema2_->schema_module->ns);
  if (it != s1_modules.end()) {
    // schema1 already includes schema2
    return schema1_;
  }

  NamespaceManager& ns_mgr = NamespaceManager::get_global();

  // Determine common modules, unique modules and augmented modules.
  ns_mod_map_t common_mods, unique_mods, augmented_mods;
  for (auto& mod: s1_modules) {
    auto it = s2_modules.find(mod.first);
    if (it != s2_modules.end()) {
      if (it->second == mod.second) {
        common_mods.emplace(mod.first, mod.second);
      } else {
        augmented_mods.emplace(mod.first, mod.second);
      }
    } else {
      if (!ns_mgr.ns_is_dynamic(mod.first.c_str())) {
        unique_mods.emplace(mod.first, mod.second);
      }
    }
  }

  for (auto& mod: s2_modules) {
    auto it = common_mods.find(mod.first);
    if (it == common_mods.end()) {
      it = augmented_mods.find(mod.first);
      if (it == augmented_mods.end()) {
        if (!ns_mgr.ns_is_dynamic(mod.first.c_str())) {
          unique_mods.emplace(mod.first, mod.second);
        }
      }
    }
  }

  create_unified_schema();
  RW_ASSERT(unified_);

  for (auto& cmod:common_mods) {
    modules_.push_back(cmod.second);
    module_map_.emplace(cmod.first, cmod.second);
  }

  for (auto& umod:unique_mods) {
    modules_.push_back(umod.second);
    module_map_.emplace(umod.first, umod.second);
  }

  for (auto& amod:augmented_mods) {

    clear_state();

    auto it = module_map_.find(amod.first);
    if (it != module_map_.end()) {
      merged_mod_ = (rw_yang_pb_module_t *)it->second;
    }

    auto it1 = s1_modules.find(amod.first);
    RW_ASSERT(it1 != s1_modules.end());

    auto it2 = s2_modules.find(amod.first);
    RW_ASSERT(it2 != s2_modules.end());

    merge_mod1_ = it1->second;
    merge_mod2_ = it2->second;

    merge_augmented_module();
  }

  unified_->module_count = common_mods.size() + unique_mods.size() + augmented_mods.size() + 1;
  unified_->modules = (rw_yang_pb_module_t **)mem_alloc(instance_, unified_->module_count * sizeof(void*));
  
  unsigned mc = 0;
  const rw_yang_pb_module_t** modules = (const rw_yang_pb_module_t **)unified_->modules;
  for (auto& mod:modules_) {
    modules[mc++] = mod;
  }

  modules[mc++] = unified_->schema_module;
  RW_ASSERT(mc == unified_->module_count);

  create_schema_data_root();

  return unified_;
}

const rw_yang_pb_schema_t*
rw_schema_merge(rw_keyspec_instance_t* instance,
                const rw_yang_pb_schema_t* schema1,
                const rw_yang_pb_schema_t* schema2)
{
  RW_ASSERT(schema1);
  RW_ASSERT(schema2);

  if (schema1 == schema2) {
    return schema1;
  }

  PbSchemaMerger smerger(instance, schema1, schema2);
  return smerger.merge_schemas();
}

static void
rw_schema_free_pbc_mdesc(rw_keyspec_instance_t* instance,
                         ProtobufCMessageDescriptor* pbc_mdesc)
{
  RW_ASSERT(pbc_mdesc->rw_flags & RW_PROTOBUF_MOPT_DYNAMIC);

  for (unsigned i = 0; i < pbc_mdesc->n_fields; ++i) {
    auto field = &pbc_mdesc->fields[i];
    RW_ASSERT(field->rw_flags & RW_PROTOBUF_FOPT_DYNAMIC);

    mem_free(instance, (void*)field->name);
    mem_free(instance, (void*)field->c_name);
    // ATTN: free default value
  }

  mem_free(instance, (void*)pbc_mdesc->fields);
  mem_free(instance, (void*)pbc_mdesc->name);
  mem_free(instance, (void*)pbc_mdesc->short_name);
  mem_free(instance, (void*)pbc_mdesc->c_name);
  mem_free(instance, (void*)pbc_mdesc->package_name);
  mem_free(instance, (void*)pbc_mdesc->fields_sorted_by_name);
  mem_free(instance, (void*)pbc_mdesc->field_ranges);
  mem_free(instance, (void*)pbc_mdesc->init_value);
  mem_free(instance, pbc_mdesc);
}

static void
rw_schema_free_pathspec_pbc_mdesc(rw_keyspec_instance_t* instance,
                                  ProtobufCMessageDescriptor* pbc_mdesc)
{
  RW_ASSERT(pbc_mdesc->rw_flags & RW_PROTOBUF_MOPT_DYNAMIC);

  for (unsigned i = 0; i < pbc_mdesc->n_fields; ++i) {
    auto field = &pbc_mdesc->fields[i];
    RW_ASSERT(field->rw_flags & RW_PROTOBUF_FOPT_DYNAMIC);

    mem_free(instance, (void*)field->name);
    mem_free(instance, (void*)field->c_name);
    // ATTN: free default value

    if (field->id == RW_SCHEMA_TAG_KEYSPEC_DOMPATH) {
      rw_schema_free_pbc_mdesc(instance, (ProtobufCMessageDescriptor *)field->msg_desc);
    }
  }

  mem_free(instance, (void*)pbc_mdesc->fields);
  mem_free(instance, (void*)pbc_mdesc->name);
  mem_free(instance, (void*)pbc_mdesc->short_name);
  mem_free(instance, (void*)pbc_mdesc->c_name);
  mem_free(instance, (void*)pbc_mdesc->package_name);
  mem_free(instance, (void*)pbc_mdesc->fields_sorted_by_name);
  mem_free(instance, (void*)pbc_mdesc->field_ranges);
  mem_free(instance, (void*)pbc_mdesc->init_value);
  mem_free(instance, pbc_mdesc);
}

static void
rw_schema_free_rpcdef(rw_keyspec_instance_t* instance,
                      rw_yang_pb_rpcdef_t* rpcdef)
{
  mem_free(instance, (void*)rpcdef->yang_node_name);
  if (&rpcdef->input->u->rpcdef == rpcdef) {
    auto ip = (rw_yang_pb_msgdesc_t*)rpcdef->input;
    ip->u = nullptr;
  }
  if (&rpcdef->output->u->rpcdef == rpcdef) {
    auto op = (rw_yang_pb_msgdesc_t*)rpcdef->output;
    op->u = nullptr;
  }
  mem_free(instance, rpcdef);
}

static void
rw_schema_free_ypbc_mdesc(rw_keyspec_instance_t* instance,
                          rw_yang_pb_msgdesc_t* ypbc_mdesc)
{
  if (ypbc_mdesc->msg_type != RW_YANGPBC_MSG_TYPE_ROOT) {
    for (unsigned i = 0; i < ypbc_mdesc->child_msg_count; ++i) {
      auto child = (rw_yang_pb_msgdesc_t*)ypbc_mdesc->child_msgs[i];
      if (child->module == ypbc_mdesc->module) {
        rw_schema_free_ypbc_mdesc(instance, child);
      }
    }
  }

  if (ypbc_mdesc->schema_entry_value) {
    rw_keyspec_entry_free((rw_keyspec_entry_t *)ypbc_mdesc->schema_entry_value, instance);
  }
  if (ypbc_mdesc->schema_path_value) {
    rw_keyspec_path_free((rw_keyspec_path_t *)ypbc_mdesc->schema_path_value, instance);
  }

  if (ypbc_mdesc->msg_type == RW_YANGPBC_MSG_TYPE_SCHEMA_PATH) {
    rw_schema_free_pathspec_pbc_mdesc(instance, (ProtobufCMessageDescriptor *)ypbc_mdesc->pbc_mdesc);
  } else {
    rw_schema_free_pbc_mdesc(instance, (ProtobufCMessageDescriptor *)ypbc_mdesc->pbc_mdesc);
  }
  if (ypbc_mdesc->schema_entry_ypbc_desc) {
    rw_schema_free_ypbc_mdesc(instance, (rw_yang_pb_msgdesc_t *)ypbc_mdesc->schema_entry_ypbc_desc);
  }
  if (ypbc_mdesc->schema_path_ypbc_desc) {
    rw_schema_free_ypbc_mdesc(instance, (rw_yang_pb_msgdesc_t *)ypbc_mdesc->schema_path_ypbc_desc);
  }

  if (ypbc_mdesc->msg_type == RW_YANGPBC_MSG_TYPE_RPC_INPUT) {
    if (ypbc_mdesc->u->rpcdef.module == ypbc_mdesc->module) {
      rw_schema_free_rpcdef(instance, (rw_yang_pb_rpcdef_t *)ypbc_mdesc->u);
    }
  }

  if (ypbc_mdesc->msg_type == RW_YANGPBC_MSG_TYPE_RPC_OUTPUT) {
    if (ypbc_mdesc->u && ypbc_mdesc->u->rpcdef.module == ypbc_mdesc->module) {
      rw_schema_free_rpcdef(instance, (rw_yang_pb_rpcdef_t *)ypbc_mdesc->u);
    }
  }

  mem_free(instance, (void*)ypbc_mdesc->child_msgs);
  mem_free(instance, (void*)ypbc_mdesc->yang_node_name);

  for (unsigned i = 0; i < ypbc_mdesc->num_fields; ++i) {
    mem_free(instance, (void *)ypbc_mdesc->ypbc_flddescs[i].yang_node_name);
  }
  mem_free(instance, (void*)ypbc_mdesc->ypbc_flddescs);
  mem_free(instance, ypbc_mdesc);
}

static void
rw_schema_free_module(rw_keyspec_instance_t* instance,
                      rw_yang_pb_module_t* module)
{
  if (module->data_module) {
    rw_schema_free_ypbc_mdesc(instance, (rw_yang_pb_msgdesc_t *)module->data_module);
  }

  if (module->group_root) {
    rw_schema_free_ypbc_mdesc(instance, (rw_yang_pb_msgdesc_t *)module->group_root);
  }

  if (module->rpci_module) {
    rw_schema_free_ypbc_mdesc(instance, (rw_yang_pb_msgdesc_t *)module->rpci_module);
  }

  if (module->rpco_module) {
    rw_schema_free_ypbc_mdesc(instance, (rw_yang_pb_msgdesc_t *)module->rpco_module);
  }

  if (module->notif_module) {
    rw_schema_free_ypbc_mdesc(instance, (rw_yang_pb_msgdesc_t *)module->notif_module);
  }

  mem_free(instance, (void*)module->module_name);
  mem_free(instance, (void*)module->revision);
  mem_free(instance, (void*)module->ns);
  mem_free(instance, (void*)module->prefix);
  mem_free(instance, module);
}

void rw_schema_free(rw_keyspec_instance_t* instance,
                    rw_yang_pb_schema_t* schema)
{
  RW_ASSERT(schema);
  RW_ASSERT(schema->schema_module);
  RW_ASSERT(NamespaceManager::get_global().ns_is_dynamic(schema->schema_module->ns));

  for (unsigned i = 0; i < schema->module_count; ++i) {
    auto mod = (rw_yang_pb_module_t *)schema->modules[i];
    if (mod->schema == schema) {
      rw_schema_free_module(instance, mod);
    }
  }

  if (schema->data_root) {
    rw_schema_free_ypbc_mdesc(instance, (rw_yang_pb_msgdesc_t *)schema->data_root);
  }

  mem_free(instance, (void*)schema->modules);
  mem_free(instance, schema);
}

static void
rw_schema_print_pbc_fdesc(std::ostringstream& oss,
                          const ProtobufCFieldDescriptor* fdesc,
                          unsigned indent)
{
  std::string pad(indent+1, ' ');
  std::string pad1(indent+2, ' ');

  oss << pad << "{" << std::endl;
  oss << pad1 << fdesc->name << "," << std::endl;
  oss << pad1 << fdesc->c_name << "," << std::endl;
  oss << pad1 << fdesc->id << "," << std::endl;
  oss << pad1 << fdesc->label << "," << std::endl;
  oss << pad1 << fdesc->type << "," << std::endl;
  oss << pad1 << fdesc->quantifier_offset << "," << std::endl;
  oss << pad1 << fdesc->offset << "," << std::endl;
  if (fdesc->descriptor) {
    oss << pad1 << fdesc->descriptor << "," << std::endl;
  } else {
    oss << pad1 << "nullptr," << std::endl;
  }
  if (fdesc->default_value) {
    oss << pad1 << fdesc->default_value << "," << std::endl;
  } else {
    oss << pad1 << "nullptr," << std::endl;
  }
  oss << pad1 << fdesc->flags << std::endl;
  oss << pad1 << fdesc->reserved_flags << std::endl;
  if (fdesc->reserved2) {
    oss << pad1 << fdesc->reserved2 << "," << std::endl;
  } else {
    oss << pad1 << "nullptr," << std::endl;
  }
  if (fdesc->reserved3) {
    oss << pad1 << fdesc->reserved3 << "," << std::endl;
  } else {
    oss << pad1 << "nullptr," << std::endl;
  }
  oss << pad1 << fdesc->rw_flags << "," << std::endl;
  oss << pad1 << fdesc->rw_inline_max << "," << std::endl;
  oss << pad1 << fdesc->data_size << "," << std::endl;
  if (fdesc->ctype) {
    oss << pad1 << fdesc->ctype << "," << std::endl;
  } else {
    oss << pad1 << "nullptr," << std::endl;
  }
  oss << pad << "}," << std::endl;
}

static void
rw_schema_print_pbc_field_string(std::ostringstream& oss,
                                 const ProtobufCFieldDescriptor* fdesc,
                                 unsigned indent)
{
  std::string pad(indent, ' ');

  if (fdesc->quantifier_offset) {
    if (fdesc->label == PROTOBUF_C_LABEL_REPEATED) {
      oss << pad << "uint32_t n_";
    } else {
      oss << pad << "protobuf_c_boolean has_";
    }
    oss << fdesc->c_name << ";" << std::endl;
  }

  RW_ASSERT(!(fdesc->rw_flags & RW_PROTOBUF_FOPT_INLINE));

  switch (fdesc->type) {
    case PROTOBUF_C_TYPE_SINT32: 
    case PROTOBUF_C_TYPE_SFIXED32: 
    case PROTOBUF_C_TYPE_INT32:
      oss << pad << "int32_t";
      break;
    case PROTOBUF_C_TYPE_SINT64: 
    case PROTOBUF_C_TYPE_SFIXED64:
    case PROTOBUF_C_TYPE_INT64: 
      oss << pad << "int64_t";
      break;
    case PROTOBUF_C_TYPE_UINT32: 
    case PROTOBUF_C_TYPE_FIXED32: 
      oss << pad << "uint32_t";
      break;
    case PROTOBUF_C_TYPE_UINT64: 
    case PROTOBUF_C_TYPE_FIXED64: 
      oss << pad << "uint64_t";
      break;
    case PROTOBUF_C_TYPE_FLOAT: 
      oss << pad << "float";
      break;
    case PROTOBUF_C_TYPE_DOUBLE: 
      oss << pad << "double";
      break;
    case PROTOBUF_C_TYPE_BOOL:
      oss << pad << "protobuf_c_boolean";
      break;
    case PROTOBUF_C_TYPE_ENUM: 
      oss << pad << fdesc->enum_desc->c_name;
      break;
    case PROTOBUF_C_TYPE_STRING:
      oss << pad << "char*";
      break;
    case PROTOBUF_C_TYPE_BYTES:
      oss << pad << "ProtobufCBinaryData";
      break;
    case PROTOBUF_C_TYPE_MESSAGE:
      oss << pad << fdesc->msg_desc->c_name << "*";
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }

  if (fdesc->label == PROTOBUF_C_LABEL_REPEATED) {
    oss << "*" << std::endl;
  }
  oss << " " << fdesc->c_name << ";" << std::endl;
}

static void
rw_schema_print_pbc_mdesc(std::ostringstream& oss,
                          const ProtobufCMessageDescriptor* mdesc,
                          unsigned indent)
{
  std::string pad(indent+1, ' ');

  oss << "-------------------- PBC MESSAGE -------------------------------" << std::endl;
  oss << "struct " << mdesc->c_name << "{" << std::endl;
  oss << pad << "ProtobufCMessage base;" << std::endl;
  for (size_t i = 0; i < mdesc->n_fields; ++i) {
    rw_schema_print_pbc_field_string(oss, &mdesc->fields[i], 1);
  }
  oss << "}" << std::endl;
  oss << "----------------------------------------------------------------" << std::endl;

  oss << "----------------------- PBC MESSAGE DESC -----------------------" << std::endl;
  oss << "const ProtobufCMessageDescriptor* pbc_mdesc(" << mdesc << ") = {" << std::endl;
  oss << pad << mdesc->magic << "," << std::endl;
  oss << pad << mdesc->name  << "," << std::endl;
  oss << pad << mdesc->short_name << "," << std::endl;
  oss << pad << mdesc->c_name << "," << std::endl;
  oss << pad << mdesc->package_name << "," << std::endl;
  oss << pad << mdesc->sizeof_message << "," << std::endl;
  oss << pad << mdesc->n_fields << "," << std::endl;
  oss << pad << mdesc->fields << "," << std::endl;
  oss << pad << mdesc->fields_sorted_by_name << "," << std::endl;
  oss << pad << mdesc->n_field_ranges << "," << std::endl;
  oss << pad << mdesc->field_ranges << "," << std::endl;
  oss << pad << mdesc->init_value << "," << std::endl;
  if (mdesc->gi_descriptor) {
    oss << pad << mdesc->gi_descriptor << "," << std::endl;
  } else {
    oss << pad << "nullptr," << std::endl;
  }
  if (mdesc->reserved1) {
    oss << pad << mdesc->reserved1 << "," << std::endl;
  } else {
    oss << pad << "nullptr," << std::endl;
  }
  if (mdesc->reserved2) {
    oss << pad << mdesc->reserved2 << "," << std::endl;
  } else {
    oss << pad << "nullptr," << std::endl;
  }
  if (mdesc->reserved3) {
    oss << pad << mdesc->reserved3 << "," << std::endl;
  } else {
    oss << pad << "nullptr," << std::endl;
  }
  oss << pad << mdesc->rw_flags << "," << std::endl;
  oss << pad << mdesc->ypbc_mdesc << "," << std::endl;
  oss << pad << mdesc->debug_stats << "," << std::endl;
  oss << "}" << std::endl;
  oss << "------------------------------------------------------------" << std::endl;

  if (mdesc->fields) {
    oss << "-------------------- PBC FLD DESC -------------------------------------" << std::endl;
    oss << "const ProtobufCFieldDescriptor* pbc_fdescs(" << mdesc->fields << ") = {" << std::endl;
    for (unsigned i = 0; i < mdesc->n_fields; ++i) {
      rw_schema_print_pbc_fdesc(oss, &mdesc->fields[i], 0);
    }
    oss << "}" << std::endl;
    oss << "-----------------------------------------------------------------------" << std::endl;
  }
}

static void
rw_schema_print_ypbc_fdesc(std::ostringstream& oss,
                           const rw_yang_pb_flddesc_t* fdesc,
                           unsigned indent)
{
  std::string pad(indent+1, ' ');
  std::string pad1(indent+2, ' ');

  oss << pad << "{" << std::endl;
  oss << pad1 << fdesc->module << "," << std::endl;
  oss << pad1 << fdesc->ypbc_msgdesc << "," << std::endl;
  oss << pad1 << fdesc->pbc_fdesc << "," << std::endl;
  oss << pad1 << fdesc->yang_node_name << "," << std::endl;
  oss << pad1 << "RW_YANG_STMT_TYPE_" << rw_yang_stmt_type_string(fdesc->stmt_type) << "," << std::endl;
  oss << pad1 << "RW_YANG_LEAF_TYPE_" << rw_yang_leaf_type_string(fdesc->leaf_type) <<"," << std::endl;
  oss << pad1 << fdesc->yang_order << std::endl;
  oss << pad1 << fdesc->pbc_order << std::endl;
  oss << pad << "}," << std::endl;
}

static void
rw_schema_print_rpcdef(std::ostringstream& oss,
                       const rw_yang_pb_rpcdef_t* rpcdef,
                       unsigned indent)
{
  std::string pad(indent+1, ' ');

  oss << "const rw_yang_pb_rpcdef_t *rpcdef(" << rpcdef << ") = {" << std::endl;
  oss << pad << rpcdef->module << "," << std::endl;
  oss << pad << rpcdef->yang_node_name << "," << std::endl;
  oss << pad << rpcdef->input << "," << std::endl;
  oss << pad << rpcdef->output << "," << std::endl;
  oss << "}" << std::endl;
}

static void
rw_schema_print_ypbc_mdesc(std::ostringstream& oss,
                           const rw_yang_pb_msgdesc_t* mdesc,
                           unsigned indent)
{
  std::string pad(indent+1, ' ');
  std::string pad1(indent+2, ' ');

  oss << "---------------------YPBC MDESC----------------------------" << std::endl;
  oss << "const rw_yang_pb_msgdesc_t* ypbc_mdesc(" << mdesc << ") = {" << std::endl;
  oss << pad << mdesc->module << "," << std::endl;
  oss << pad << mdesc->yang_node_name << "," << std::endl;
  oss << pad << mdesc->pb_element_tag << "," << std::endl;
  oss << pad << mdesc->num_fields << "," << std::endl;
  oss << pad << ".ypbc_flddescs = ";
  if (mdesc->ypbc_flddescs) {
    oss << mdesc->ypbc_flddescs << "," << std::endl;
  } else {
    oss << "nullptr," << std::endl;
  }
  oss << pad << ".parent = ";
  if (mdesc->parent) {
    oss << mdesc->parent << "," << std::endl;
  } else {
    oss << "nullptr," << std::endl;
  }
  oss << pad << ".pbc_mdesc = ";
  if (mdesc->pbc_mdesc) {
    oss << mdesc->pbc_mdesc << "," << std::endl;
  } else {
    oss << "nullptr," << std::endl;
  }
  oss << pad << ".schema_entry_ypbc_desc = ";
  if (mdesc->schema_entry_ypbc_desc) {
    oss << mdesc->schema_entry_ypbc_desc << "," << std::endl;
  } else {
    oss << "nullptr," << std::endl;
  }
  oss << pad << ".schema_path_ypbc_desc = ";
  if (mdesc->schema_path_ypbc_desc) {
    oss << mdesc->schema_path_ypbc_desc << "," << std::endl;
  } else {
    oss << "nullptr," << std::endl;
  }
  oss << pad << ".schema_list_only_ypbc_desc = ";
  if (mdesc->schema_list_only_ypbc_desc) {
    oss << mdesc->schema_list_only_ypbc_desc << "," << std::endl;
  } else {
    oss << "nullptr," << std::endl;
  }
  oss << pad << ".schema_entry_value = ";
  if (mdesc->schema_entry_value) {
    oss << mdesc->schema_entry_value << "," << std::endl;
  } else {
    oss << "nullptr," << std::endl;
  }
  oss << pad << ".schema_path_value = ";
  if (mdesc->schema_path_value) {
    oss << mdesc->schema_path_value << "," << std::endl;
  } else {
    oss << "nullptr," << std::endl;
  }
  oss << pad << ".child_msgs = ";
  if (mdesc->child_msgs) {
    oss << mdesc->child_msgs << "," << std::endl;
  } else {
    oss << "nullptr," << std::endl;
  }
  oss << pad << "{" << std::endl;
  for (unsigned i = 0; i < mdesc->child_msg_count; ++i) {
    oss << pad1 << mdesc->child_msgs[i] << "(" << mdesc->child_msgs[i]->yang_node_name << ")," << std::endl;
  }
  oss << pad << "}" << std::endl;
  oss << pad << mdesc->child_msg_count << "," << std::endl;
  oss << pad << "RW_YANGPBC_MSG_TYPE_" << rw_yang_pb_msg_type_string(mdesc->msg_type) << "," << std::endl;
  if (mdesc->u) {
    oss << pad << mdesc->u << "," << std::endl;
  } else {
    oss << pad << "nullptr," << std::endl;
  }

  oss << "}" << std::endl;
  oss << "------------------------------------------------------------" << std::endl;

  if (mdesc->ypbc_flddescs) {
    oss << "-------------------------YPBC FLDDESC---------------------------" << std::endl;
    oss << "const rw_yang_pb_flddesc_t* fdescs(" << mdesc->ypbc_flddescs << ") = {" << std::endl;
    for (unsigned i = 0; i < mdesc->num_fields; ++i) {
      rw_schema_print_ypbc_fdesc(oss, &mdesc->ypbc_flddescs[i],  0);
    }
    oss << "}" << std::endl;
    oss << "---------------------------------------------------------" << std::endl;
  }

  if (mdesc->msg_type == RW_YANGPBC_MSG_TYPE_RPC_INPUT) {
    if (mdesc->u->rpcdef.module == mdesc->module) {
      rw_schema_print_rpcdef(oss, &mdesc->u->rpcdef,  0);
    }
  }

  if (mdesc->msg_type == RW_YANGPBC_MSG_TYPE_RPC_OUTPUT) {
    if (mdesc->u->rpcdef.module == mdesc->module) {
      if (mdesc->u->rpcdef.input->module != mdesc->module) {
        rw_schema_print_rpcdef(oss, &mdesc->u->rpcdef,  0);
      }
    }
  }

  if (mdesc->pbc_mdesc) {
    rw_schema_print_pbc_mdesc(oss, mdesc->pbc_mdesc,  0);
  }

#if 0
  if (mdesc->schema_entry_ypbc_desc) {
    rw_schema_print_ypbc_mdesc(mdesc->schema_entry_ypbc_desc,  0);
  }

  if (mdesc->schema_path_ypbc_desc) {
    rw_schema_print_ypbc_mdesc(mdesc->schema_path_ypbc_desc,  0);
  }
#endif

  if (mdesc->msg_type == RW_YANGPBC_MSG_TYPE_ROOT) {
    return;
  }

  for (unsigned i = 0; i < mdesc->child_msg_count; ++i) {
    auto child = mdesc->child_msgs[i];
    if (NamespaceManager::get_global().ns_is_dynamic(child->module->schema->schema_module->ns)) {
      rw_schema_print_ypbc_mdesc(oss, child,  0);
    }
  }
}

static void
rw_schema_print_module_info(std::ostringstream& oss,
                            const rw_yang_pb_module_t* module,
                            unsigned indent)
{
  std::string pad(indent+2, ' ');

  oss << "---------------------MODULE----------------------------" << std::endl;
  oss << "const rw_yang_pb_module_t* module(" << module << ") = {" << std::endl;
  oss << pad << module->schema << "," << std::endl;
  oss << pad << module->module_name << "," << std::endl;
  if (module->revision) {
    oss << pad << module->revision << "," << std::endl;
  } else {
    oss << pad << "nullptr," << std::endl;
  }
  oss << pad << module->ns << "," << std::endl;
  oss << pad << module->prefix << "," << std::endl;
  if (module->data_module) {
    oss << pad << module->data_module << "," << std::endl;
  } else {
    oss << pad << "nullptr," << std::endl;
  }
  if (module->group_root) {
    oss << pad << module->group_root << "," << std::endl;
  } else {
    oss << pad << "nullptr," << std::endl;
  }
  if (module->rpci_module) {
    oss << pad << module->rpci_module << "," << std::endl;
  } else {
    oss << pad << "nullptr," << std::endl;
  }
  if (module->rpco_module) {
    oss << pad << module->rpco_module << "," << std::endl;
  } else {
    oss << pad << "nullptr," << std::endl;
  }
  if (module->notif_module) {
    oss << pad << module->notif_module << "," << std::endl;
  } else {
    oss << pad << "nullptr," << std::endl;
  }

  oss << "}" << std::endl;
  oss << "------------------------------------------------------" << std::endl;

  if (module->data_module) {
    rw_schema_print_ypbc_mdesc(oss, module->data_module,  0);
  }

  if (module->rpci_module) {
    rw_schema_print_ypbc_mdesc(oss, module->rpci_module,  0);
  }

  if (module->rpco_module) {
    rw_schema_print_ypbc_mdesc(oss, module->rpco_module,  0);
  }

  if (module->notif_module) {
    rw_schema_print_ypbc_mdesc(oss, module->notif_module,  0);
  }
}

void
rw_schema_print_dynamic_schema(const rw_yang_pb_schema_t* schema)
{
  std::ostringstream oss;
  std::string pad1(2, ' ');

  oss << "RUNTIME GENERATED DYNAMIC SCHEMA:" << std::endl;
  oss << "-------------------- SCHEMA --------------------------" << std::endl;
  oss << "const rw_yang_pb_schema_t* dyn_schema(" << schema << ") = {" << std::endl;
  oss << pad1 << schema->schema_module << " (" << schema->schema_module->module_name << ")," << std::endl;
  oss << pad1 << "nullptr," << std::endl;
  for (unsigned i = 0; i < schema->module_count; ++i) {
    oss << pad1 << schema->modules[i] << " (" << schema->modules[i]->module_name << ")," << std::endl;
  }
  oss << pad1 << schema->module_count << "," << std::endl;
  oss << "}" << std::endl;
  oss << "-------------------------------------------------------" << std::endl;

  for (unsigned i = 0; i < schema->module_count; ++i) {
    if (strstr(schema->modules[i]->schema->schema_module->module_name, "rw-dynamic")) {
      rw_schema_print_module_info(oss, schema->modules[i],  0);
      oss << std::endl;
    }
  }

  if (schema->data_root) {
    rw_schema_print_ypbc_mdesc(oss, schema->data_root,  0);
  }
}

const rw_yang_pb_msgdesc_t*
rw_yang_pb_schema_get_ypbc_mdesc(const rw_yang_pb_schema_t *schema,
                                 uint32_t system_nsid,
                                 uint32_t element_tag)
{
  RW_ASSERT (schema);

  const rw_yang_pb_msgdesc_t* ypbc_mdesc = nullptr;

  NamespaceManager& nsmgr = NamespaceManager::get_global();

  for (unsigned i = 0; i < schema->module_count; ++i) {

    auto mod = schema->modules[i];
    RW_ASSERT (mod);

    rw_namespace_id_t nsid = nsmgr.get_ns_hash(mod->ns);
    if (nsid == system_nsid) {

      if (mod->notif_module) {
        for (unsigned j = 0; j < mod->notif_module->child_msg_count; ++j) {
          auto child = mod->notif_module->child_msgs[j];
          if (child->pb_element_tag == element_tag) {
            ypbc_mdesc = child;
            break;
          }
        }
      }

      if (!ypbc_mdesc && mod->data_module) {
        for (unsigned j = 0; j < mod->data_module->child_msg_count; ++j) {
          auto child = mod->data_module->child_msgs[j];
          if (child->pb_element_tag == element_tag) {
            ypbc_mdesc = child;
            break;
          }
        }
      }

      if (!ypbc_mdesc && mod->rpci_module) {
        for (unsigned j = 0; j < mod->rpci_module->child_msg_count; ++j) {
          auto child = mod->rpci_module->child_msgs[j];
          if (child->pb_element_tag == element_tag) {
            ypbc_mdesc = child;
            break;
          }
        }
      }

      if (!ypbc_mdesc && mod->rpco_module) {
        for (unsigned j = 0; j < mod->rpco_module->child_msg_count; ++j) {
          auto child = mod->rpco_module->child_msgs[j];
          if (child->pb_element_tag == element_tag) {
            ypbc_mdesc = child;
            break;
          }
        }
      }

      break;
    }
  }

  return ypbc_mdesc;
}
