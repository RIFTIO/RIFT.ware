
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
 * @file test_schema_merge.cpp
 * @author Sujithra Periasamy
 * @date 2015/09/01
 * @brief Google test cases for schema merge
 */

#include <limits.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <map>
#include <vector>
#include <rw_namespace.h>
#include "yangncx.hpp"
#include "yangtest_common.hpp"

#include "company.pb-c.h"
#include "document.pb-c.h"
#include "rw-fpath-d.pb-c.h"
#include "rw-appmgr-d.pb-c.h"
#include "rw-dts-d.pb-c.h"
#include "rw-vcs-d.pb-c.h"
#include "rw-test-au1.pb-c.h"
#include "rw-test-au2.pb-c.h"
#include "rw-composite-d.pb-c.h"
#include "test-a-composite.pb-c.h"
#include "rwut.h"
#include "rwlib.h"
#include "yangmodel.h"
#include "rw_keyspec.h"
#include "rw_pb_schema.h"
#include "struct_test.h"

using namespace rw_yang;

typedef std::map<std::string, const rw_yang_pb_module_t*>  ns_to_mod_map_t;
typedef std::map<std::string, const rw_yang_pb_msgdesc_t*> yname_to_child_map_t;
typedef std::map<std::string, const rw_yang_pb_flddesc_t*> yname_to_fdesc_map_t;
typedef std::map<uint32_t,    const rw_yang_pb_msgdesc_t*> tag_to_child_map_t;

typedef std::map<uint32_t, const unsigned*> tag_to_off_map_t;
tag_to_off_map_t off_map;

typedef std::vector<std::string> top_msg_list;

static void
verify_generated_pbcm_fdescs(const ProtobufCFieldDescriptor* dynamic,
                             const ProtobufCFieldDescriptor* compile,
                             uint32_t n_fields,
                             const rw_yang_pb_msgdesc_t* merged)
{
  tag_to_child_map_t cmsg_map;
  for (unsigned i = 0; i < merged->child_msg_count; ++i) {
    auto child = merged->child_msgs[i];
    cmsg_map.emplace(child->pb_element_tag, child);
  }

  const unsigned *off_list = nullptr; 
  unsigned oc = 0;
  auto it =off_map.find(merged->pb_element_tag);
  if (it != off_map.end()) {
    off_list = it->second;
    off_map.erase(it);
  }

  for (unsigned i = 0; i < n_fields; ++i) {

    EXPECT_STREQ(dynamic[i].name, compile[i].name);
    EXPECT_STREQ(dynamic[i].c_name, compile[i].c_name);
    EXPECT_EQ(dynamic[i].id, compile[i].id);
    EXPECT_EQ(dynamic[i].label, compile[i].label);
    EXPECT_EQ(dynamic[i].type, compile[i].type);

    if ((merged->msg_type != RW_YANGPBC_MSG_TYPE_SCHEMA_ENTRY) &&
        (merged->msg_type != RW_YANGPBC_MSG_TYPE_SCHEMA_PATH)) {
      if (off_list) {
        if (dynamic[i].quantifier_offset) {
          EXPECT_EQ(dynamic[i].quantifier_offset, off_list[oc++]);
        }
        EXPECT_EQ(dynamic[i].offset, off_list[oc++]);
      }
    } else {
      EXPECT_EQ(dynamic[i].quantifier_offset, compile[i].quantifier_offset);
      EXPECT_EQ(dynamic[i].offset, compile[i].offset);
    }

    if ((merged->msg_type != RW_YANGPBC_MSG_TYPE_SCHEMA_ENTRY) &&
        (merged->msg_type != RW_YANGPBC_MSG_TYPE_SCHEMA_PATH)) {

      if (dynamic[i].type == PROTOBUF_C_TYPE_MESSAGE) {
        auto it = cmsg_map.find(dynamic[i].id);
        ASSERT_NE(it, cmsg_map.end());
        ASSERT_EQ(dynamic[i].msg_desc, it->second->pbc_mdesc);
      }
    }

    if (compile[i].default_value) {
      ASSERT_TRUE(dynamic[i].default_value);
    }
    ASSERT_EQ(dynamic[i].flags, compile[i].flags);
    ASSERT_EQ(dynamic[i].reserved_flags, compile[i].reserved_flags);
    ASSERT_EQ(dynamic[i].reserved2, compile[i].reserved2);
    ASSERT_EQ(dynamic[i].reserved3, compile[i].reserved3);

    auto drw_flags = dynamic[i].rw_flags;
    ASSERT_TRUE(drw_flags & RW_PROTOBUF_FOPT_DYNAMIC);
    drw_flags &= ~RW_PROTOBUF_FOPT_DYNAMIC;

    auto crw_flags = compile[i].rw_flags;

    if ((merged->msg_type != RW_YANGPBC_MSG_TYPE_SCHEMA_ENTRY) &&
        (merged->msg_type != RW_YANGPBC_MSG_TYPE_SCHEMA_PATH)) {
      crw_flags &= ~RW_PROTOBUF_FOPT_INLINE;
    }

    ASSERT_EQ(drw_flags, crw_flags);
    ASSERT_EQ(dynamic[i].rw_inline_max, 0);

    if (dynamic[i].type == PROTOBUF_C_TYPE_MESSAGE) {
      ASSERT_EQ(dynamic[i].data_size, dynamic[i].msg_desc->sizeof_message);
    }
    ASSERT_EQ(dynamic[i].ctype, compile[i].ctype);
  }

  if (off_list) {
    ASSERT_EQ(merged->pbc_mdesc->sizeof_message, off_list[oc]);
  }
}

static void
verify_merged_pbcm_descriptor(const rw_yang_pb_schema_t* schema,
                              const ProtobufCMessageDescriptor* compare_mdesc,
                              const rw_yang_pb_msgdesc_t* merged)
{
  auto pbc_mdesc = merged->pbc_mdesc;
  ASSERT_TRUE(pbc_mdesc);
  ASSERT_EQ(pbc_mdesc->ypbc_mdesc, merged);

  ASSERT_EQ(pbc_mdesc->magic, PROTOBUF_C_MESSAGE_DESCRIPTOR_MAGIC);
  std::string name(compare_mdesc->name);
  if (name.find('.') != std::string::npos) {
    name.erase(0, name.find('.'));
  }

  const char *module_n = nullptr;
  if (merged->module) {
    module_n = merged->module->schema->schema_module->module_name;
  } else {
    module_n = schema->schema_module->module_name;
  }
  ASSERT_TRUE(module_n);

  EXPECT_STREQ(pbc_mdesc->name, std::string(module_n+name).c_str());
  EXPECT_STREQ(pbc_mdesc->short_name, compare_mdesc->short_name);

  std::string cname(compare_mdesc->c_name);
  if (cname.find('_') != std::string::npos) {
    cname.erase(0, cname.find('_'));
  }

  std::string ccmn = YangModel::mangle_to_camel_case(module_n);
  EXPECT_STREQ(pbc_mdesc->c_name, std::string(ccmn+cname).c_str());
  EXPECT_STREQ(pbc_mdesc->package_name, module_n);
  EXPECT_EQ(pbc_mdesc->sizeof_message, pbc_mdesc->sizeof_message);

  EXPECT_EQ(pbc_mdesc->n_fields, compare_mdesc->n_fields);
  ASSERT_TRUE(pbc_mdesc->fields);
  verify_generated_pbcm_fdescs(pbc_mdesc->fields, compare_mdesc->fields, pbc_mdesc->n_fields, merged);

  ASSERT_TRUE(pbc_mdesc->fields_sorted_by_name);
  for (unsigned i = 0; i < pbc_mdesc->n_fields; ++i) {
    ASSERT_EQ(pbc_mdesc->fields_sorted_by_name[i], compare_mdesc->fields_sorted_by_name[i]);
  }

  ASSERT_EQ(pbc_mdesc->n_field_ranges, compare_mdesc->n_field_ranges);
  for (unsigned i = 0; i < pbc_mdesc->n_field_ranges+1; ++i) {
    ASSERT_EQ(pbc_mdesc->field_ranges[i].start_value, compare_mdesc->field_ranges[i].start_value);
    ASSERT_EQ(pbc_mdesc->field_ranges[i].orig_index, compare_mdesc->field_ranges[i].orig_index);
  }

  ASSERT_TRUE(pbc_mdesc->init_value);
  ASSERT_FALSE(pbc_mdesc->gi_descriptor);
  ASSERT_EQ(pbc_mdesc->reserved1, compare_mdesc->reserved1);
  ASSERT_EQ(pbc_mdesc->reserved2, compare_mdesc->reserved2);
  ASSERT_EQ(pbc_mdesc->reserved3, compare_mdesc->reserved3);

  auto rw_flags = pbc_mdesc->rw_flags;
  ASSERT_TRUE(rw_flags & RW_PROTOBUF_MOPT_DYNAMIC);
  rw_flags &= ~RW_PROTOBUF_MOPT_DYNAMIC;

  ASSERT_EQ(rw_flags, compare_mdesc->rw_flags);
}

static void
verify_generated_ypbc_fdescs(const rw_yang_pb_msgdesc_t* comp_msg,
                             const rw_yang_pb_msgdesc_t* dynamic)
{

  EXPECT_EQ(comp_msg->num_fields, dynamic->num_fields);
  for (unsigned i = 0; i < comp_msg->num_fields; ++i) {
    auto field_d = &dynamic->ypbc_flddescs[i];
    auto field_c = &comp_msg->ypbc_flddescs[i];
    EXPECT_STREQ(field_d->module->ns, field_c->module->ns);
    EXPECT_EQ(field_d->ypbc_msgdesc, dynamic);
    EXPECT_STREQ(field_d->yang_node_name, field_c->yang_node_name);
    EXPECT_EQ(field_d->stmt_type, field_c->stmt_type);
    EXPECT_EQ(field_d->leaf_type, field_c->leaf_type);
    EXPECT_EQ(field_d->yang_order, field_c->yang_order);
    EXPECT_EQ(field_d->pbc_order, field_c->pbc_order);
    EXPECT_EQ(field_d->pbc_fdesc, &dynamic->pbc_mdesc->fields[field_d->pbc_order-1]);
  }

}

static void
verify_generated_path_entry_all(const rw_yang_pb_schema_t* schema,
                                const rw_yang_pb_msgdesc_t* comp_msg,
                                const rw_yang_pb_msgdesc_t* dynamic)
{
  ASSERT_TRUE(dynamic->schema_entry_ypbc_desc);
  ASSERT_EQ(dynamic->schema_entry_ypbc_desc->module, dynamic->module);
  EXPECT_STREQ(dynamic->schema_entry_ypbc_desc->yang_node_name, comp_msg->schema_entry_ypbc_desc->yang_node_name);
  EXPECT_EQ(dynamic->schema_entry_ypbc_desc->pb_element_tag, comp_msg->schema_entry_ypbc_desc->pb_element_tag);
  EXPECT_EQ(dynamic->schema_entry_ypbc_desc->num_fields, 0);
  EXPECT_EQ(dynamic->schema_entry_ypbc_desc->child_msg_count, 0);
  EXPECT_EQ(dynamic->schema_entry_ypbc_desc->msg_type, RW_YANGPBC_MSG_TYPE_SCHEMA_ENTRY);
  EXPECT_EQ(&dynamic->schema_entry_ypbc_desc->u->msg_msgdesc, dynamic);

  ASSERT_TRUE(dynamic->schema_entry_ypbc_desc->pbc_mdesc);
  ASSERT_NE(dynamic->schema_entry_ypbc_desc->pbc_mdesc, comp_msg->schema_entry_ypbc_desc->pbc_mdesc);
  verify_merged_pbcm_descriptor(schema, comp_msg->schema_entry_ypbc_desc->pbc_mdesc, dynamic->schema_entry_ypbc_desc);

  ASSERT_TRUE(dynamic->schema_entry_value);
  ASSERT_EQ(dynamic->schema_entry_value->base.descriptor, dynamic->schema_entry_ypbc_desc->pbc_mdesc);

  ASSERT_TRUE(protobuf_c_message_is_equal_deep(NULL, &dynamic->schema_entry_value->base,
                                               &comp_msg->schema_entry_value->base));
}

static void
verify_generated_path_spec_all(const rw_yang_pb_schema_t* schema,
                               const rw_yang_pb_msgdesc_t* comp_msg,
                               const rw_yang_pb_msgdesc_t* dynamic)
{
  ASSERT_TRUE(dynamic->schema_path_ypbc_desc);
  ASSERT_EQ(dynamic->schema_path_ypbc_desc->module, dynamic->module);
  EXPECT_STREQ(dynamic->schema_path_ypbc_desc->yang_node_name, comp_msg->schema_path_ypbc_desc->yang_node_name);
  EXPECT_EQ(dynamic->schema_path_ypbc_desc->pb_element_tag, comp_msg->schema_path_ypbc_desc->pb_element_tag);
  EXPECT_EQ(dynamic->schema_path_ypbc_desc->num_fields, 0);
  EXPECT_EQ(dynamic->schema_path_ypbc_desc->child_msg_count, 0);
  EXPECT_EQ(dynamic->schema_path_ypbc_desc->msg_type, RW_YANGPBC_MSG_TYPE_SCHEMA_PATH);
  EXPECT_EQ(&dynamic->schema_path_ypbc_desc->u->msg_msgdesc, dynamic);

  ASSERT_TRUE(dynamic->schema_path_ypbc_desc->pbc_mdesc);
  ASSERT_NE(dynamic->schema_path_ypbc_desc->pbc_mdesc, comp_msg->schema_path_ypbc_desc->pbc_mdesc);
  verify_merged_pbcm_descriptor(schema, comp_msg->schema_path_ypbc_desc->pbc_mdesc, dynamic->schema_path_ypbc_desc);

  ASSERT_TRUE(dynamic->schema_path_value);
  ASSERT_EQ(dynamic->schema_path_value->base.descriptor, dynamic->schema_path_ypbc_desc->pbc_mdesc);

  ASSERT_TRUE(protobuf_c_message_is_equal_deep(NULL, &dynamic->schema_path_value->base,
                                               &comp_msg->schema_path_value->base));
}

static void
verify_generated_root_ypbc_mdesc(const rw_yang_pb_schema_t* schema,
                                 const rw_yang_pb_msgdesc_t* croot,
                                 const rw_yang_pb_msgdesc_t* droot)
{
  ASSERT_FALSE(droot->module);
  ASSERT_FALSE(droot->schema_list_only_ypbc_desc);
  ASSERT_EQ(croot->child_msg_count, droot->child_msg_count);
  ASSERT_FALSE(droot->u);

  EXPECT_STREQ(croot->yang_node_name, droot->yang_node_name);
  EXPECT_EQ(croot->pb_element_tag, droot->pb_element_tag);
  EXPECT_EQ(croot->num_fields, droot->num_fields);
  EXPECT_EQ(croot->msg_type, droot->msg_type);

  verify_merged_pbcm_descriptor(schema, croot->pbc_mdesc, droot);
  verify_generated_path_entry_all(schema, croot, droot);
  verify_generated_path_spec_all(schema, croot, droot);
}

static void
verify_generated_ypbc_mdesc(const rw_yang_pb_schema_t* schema,
                            const rw_yang_pb_msgdesc_t* comp_msg,
                            const rw_yang_pb_msgdesc_t* dynamic)
{
  ASSERT_TRUE(dynamic->module);
  ASSERT_TRUE(NamespaceManager::get_global().ns_is_dynamic(dynamic->module->schema->schema_module->ns));
  ASSERT_NE(dynamic->module, comp_msg->module);
  ASSERT_STREQ (dynamic->module->ns, comp_msg->module->ns);

  EXPECT_STREQ(dynamic->yang_node_name, comp_msg->yang_node_name);
  EXPECT_EQ(dynamic->pb_element_tag, comp_msg->pb_element_tag);
  EXPECT_EQ(dynamic->num_fields, comp_msg->num_fields);
  EXPECT_EQ(dynamic->msg_type, comp_msg->msg_type);

  verify_generated_ypbc_fdescs(comp_msg, dynamic);
  verify_merged_pbcm_descriptor(schema, comp_msg->pbc_mdesc, dynamic);

  verify_generated_path_entry_all(schema, comp_msg, dynamic);
  verify_generated_path_spec_all(schema, comp_msg, dynamic);

  ASSERT_FALSE(dynamic->schema_list_only_ypbc_desc);
  ASSERT_EQ(dynamic->child_msg_count, comp_msg->child_msg_count);

  for (unsigned i = 0; i < comp_msg->child_msg_count; ++i) {
    ASSERT_TRUE(dynamic->child_msgs);
    auto c1 = dynamic->child_msgs[i];
    auto c2 = comp_msg->child_msgs[i];

    if (c1 != c2) {
      verify_generated_ypbc_mdesc(schema, c2, c1);
    }
  }

  if (dynamic->msg_type == RW_YANGPBC_MSG_TYPE_RPC_INPUT ||
      dynamic->msg_type == RW_YANGPBC_MSG_TYPE_RPC_OUTPUT) {
    ASSERT_TRUE(dynamic->u);
  } else {
    ASSERT_FALSE(dynamic->u);
  }
}

static void
verify_dynamic_merged_module_root(const rw_yang_pb_schema_t* schema,
                                  const rw_yang_pb_msgdesc_t* croot,
                                  const rw_yang_pb_msgdesc_t* merged,
                                  bool same_msgs)
{
  EXPECT_STREQ(merged->yang_node_name, "data");
  ASSERT_EQ(merged->pb_element_tag, croot->pb_element_tag);
  ASSERT_EQ(merged->num_fields, croot->num_fields);
  ASSERT_EQ(merged->num_fields, croot->num_fields);
  if (croot->ypbc_flddescs) {
    ASSERT_TRUE(merged->ypbc_flddescs);
  }

  ASSERT_FALSE(merged->parent);
  ASSERT_TRUE(merged->pbc_mdesc);
  ASSERT_EQ(merged->pbc_mdesc->ypbc_mdesc, merged);

  ASSERT_TRUE(merged->schema_entry_ypbc_desc);
  verify_generated_path_entry_all(schema, croot, merged);

  ASSERT_TRUE(merged->schema_path_ypbc_desc);
  verify_generated_path_spec_all(schema, croot, merged);

  ASSERT_FALSE(merged->schema_list_only_ypbc_desc);
  ASSERT_TRUE(merged->schema_entry_value);
  ASSERT_TRUE(merged->schema_path_value);

  ASSERT_TRUE(merged->child_msgs);
  ASSERT_EQ(merged->child_msg_count, croot->child_msg_count);
  ASSERT_EQ(merged->child_msg_count, croot->child_msg_count);
  ASSERT_EQ(merged->msg_type, RW_YANGPBC_MSG_TYPE_MODULE_ROOT);
  ASSERT_FALSE(merged->u);

  if (same_msgs) {
    for (unsigned i = 0; i < croot->child_msg_count; ++i) {

      auto child1 = croot->child_msgs[i];
      auto child2 = merged->child_msgs[i];

      ASSERT_EQ(child1, child2);
    }
  }
}

static void
verify_merged_top_messages(const rw_yang_pb_schema_t* schema,
                           const rw_yang_pb_msgdesc_t* croot,
                           const rw_yang_pb_msgdesc_t* merged,
                           top_msg_list& merged_msgs)
{
  yname_to_child_map_t cschema, dschema;

  ASSERT_EQ(croot->child_msg_count, merged->child_msg_count);

  for (size_t i = 0; i < croot->child_msg_count; i++) {
    auto child = croot->child_msgs[i];
    cschema.emplace(child->yang_node_name, child);
  }

  for (size_t i = 0; i < merged->child_msg_count; i++) {
    auto child = merged->child_msgs[i];
    dschema.emplace(child->yang_node_name, child);
  }

  for (auto& m:merged_msgs) {
    auto it = dschema.find(m);
    ASSERT_NE(it, dschema.end());

    auto it1 = cschema.find(m);
    ASSERT_NE(it1, cschema.end());
    ASSERT_NE(it->second, it1->second);

    ASSERT_NE (it->second->module, it1->second->module);
    ASSERT_STREQ (it->second->module->ns, it1->second->module->ns);
  }

  for (size_t i = 0; i < croot->child_msg_count; ++i) {
    auto top1 = croot->child_msgs[i];
    auto top2 = merged->child_msgs[i];

    ASSERT_STREQ(top1->yang_node_name, top2->yang_node_name);
    ASSERT_EQ(top1->msg_type, top2->msg_type);

    if (top1 != top2) {
      verify_generated_ypbc_mdesc(schema, top1, top2);

      if (top2->msg_type == RW_YANGPBC_MSG_TYPE_RPC_INPUT ||
          top2->msg_type == RW_YANGPBC_MSG_TYPE_RPC_OUTPUT) {

        ASSERT_NE(top1->u, top2->u);
        ASSERT_TRUE(top2->u);

        ASSERT_EQ(top2->u->rpcdef.module, top2->module);
        ASSERT_STREQ(top2->u->rpcdef.yang_node_name, top2->yang_node_name);

        if (top2->msg_type == RW_YANGPBC_MSG_TYPE_RPC_INPUT) {
          ASSERT_EQ(top2->u->rpcdef.input, top2);
        } else {
          ASSERT_EQ(top2->u->rpcdef.output, top2);
        }
      }
    }
  }

  verify_merged_pbcm_descriptor(schema, croot->pbc_mdesc, merged);
  verify_generated_ypbc_fdescs(croot, merged);
}

static void
verify_merged_module_top_msg_list(const rw_yang_pb_msgdesc_t* top1,
                                  const rw_yang_pb_msgdesc_t* top2,
                                  const rw_yang_pb_msgdesc_t* merged,
                                  top_msg_list& merged_msgs,
                                  top_msg_list& schema1_msgs,
                                  top_msg_list& schema2_msgs)
{
  yname_to_child_map_t mergedl, schema1l, schema2l;
  for (size_t i = 0; i < merged->child_msg_count; i++) {
    auto child = merged->child_msgs[i];
    mergedl.emplace(child->yang_node_name, child);
  }

  for (size_t i = 0; i < top1->child_msg_count; i++) {
    auto child = top1->child_msgs[i];
    schema1l.emplace(child->yang_node_name, child);
  }

  for (size_t i = 0; i < top2->child_msg_count; i++) {
    auto child = top2->child_msgs[i];
    schema2l.emplace(child->yang_node_name, child);
  }

  for (auto& m:merged_msgs) {
    auto it = mergedl.find(m);
    ASSERT_NE(it, mergedl.end());

    auto it1 = schema1l.find(m);
    ASSERT_NE(it1, schema1l.end());
    ASSERT_NE(it->second, it1->second);

    auto it2 = schema2l.find(m);
    ASSERT_NE(it2, schema2l.end());
    ASSERT_NE(it->second, it2->second);

    mergedl.erase(it);
  }

  for (auto& m:schema1_msgs) {
    auto it = mergedl.find(m);
    ASSERT_NE(it, mergedl.end());

    auto it1 = schema1l.find(m);
    ASSERT_NE(it1, schema1l.end());

    ASSERT_EQ(it->second, it1->second);
    mergedl.erase(it);
  }

  for (auto& m:schema2_msgs) {
    auto it = mergedl.find(m);
    ASSERT_NE(it, mergedl.end());

    auto it1 = schema2l.find(m);
    ASSERT_NE(it1, schema2l.end());

    ASSERT_EQ(it->second, it1->second);
    mergedl.erase(it);
  }

  for (auto& top:mergedl) {
    auto it1 = schema1l.find(top.first);
    ASSERT_NE(it1, schema1l.end());

    auto it2 = schema2l.find(top.first);
    ASSERT_NE(it2, schema2l.end());

    ASSERT_EQ(top.second, it1->second);
    ASSERT_EQ(top.second, it2->second);
  }
}

static void
verify_merged_module(const rw_yang_pb_module_t* mod1,
                     const rw_yang_pb_module_t* mod2,
                     const rw_yang_pb_module_t* merged)
{
  EXPECT_STREQ(merged->module_name, mod1->module_name);
  EXPECT_STREQ(merged->module_name, mod2->module_name);

  ASSERT_FALSE(merged->revision);

  EXPECT_STREQ(merged->ns, mod1->ns);
  EXPECT_STREQ(merged->ns, mod2->ns);
  EXPECT_STREQ(merged->prefix, mod1->prefix);
  EXPECT_STREQ(merged->prefix, mod2->prefix);

  if (mod1->data_module) {
    ASSERT_TRUE(merged->data_module);
    ASSERT_NE(merged->data_module, mod1->data_module);
    ASSERT_NE(merged->data_module, mod2->data_module);
    ASSERT_EQ(merged->data_module->module, merged);
  } else {
    ASSERT_FALSE(merged->data_module);
  }

  if (mod1->rpci_module) {
    ASSERT_TRUE(merged->rpci_module);
    ASSERT_NE(merged->rpci_module, mod1->rpci_module);
    ASSERT_NE(merged->rpci_module, mod2->rpci_module);
    ASSERT_EQ(merged->rpci_module->module, merged);
  } else {
    ASSERT_FALSE(merged->rpci_module);
  }

  if (mod1->rpco_module) {
    ASSERT_TRUE(merged->rpco_module);
    ASSERT_NE(merged->rpco_module, mod1->rpco_module);
    ASSERT_NE(merged->rpco_module, mod2->rpco_module);
    ASSERT_EQ(merged->rpco_module->module, merged);
  } else {
    ASSERT_FALSE(merged->rpci_module);
  }

  if (mod1->notif_module) {
    ASSERT_TRUE(merged->notif_module);
    ASSERT_NE(merged->notif_module, mod1->notif_module);
    ASSERT_NE(merged->notif_module, mod2->notif_module);
    ASSERT_EQ(merged->notif_module->module, merged);
  } else {
    ASSERT_FALSE(merged->notif_module);
  }
}

static void
verify_dynamic_schema_module(const rw_yang_pb_module_t* dynm,
                             const rw_yang_pb_schema_t* schema)
{
  ASSERT_FALSE(schema->top_msglist);
  ASSERT_EQ(dynm->schema, schema);
  ASSERT_TRUE(dynm->module_name);
  ASSERT_FALSE(dynm->revision);
  ASSERT_TRUE(dynm->ns);
  ASSERT_TRUE(NamespaceManager::get_global().ns_is_dynamic(dynm->ns));
  ASSERT_TRUE(dynm->prefix);
  ASSERT_FALSE(dynm->data_module);
  ASSERT_FALSE(dynm->group_root);
  ASSERT_FALSE(dynm->rpci_module);
  ASSERT_FALSE(dynm->rpco_module);
  ASSERT_FALSE(dynm->notif_module);
}

static void
verify_dynamic_schema(const rw_yang_pb_schema_t* cschema,
                      const rw_yang_pb_schema_t* merged)
{
  ASSERT_TRUE(merged->schema_module);
  ASSERT_TRUE(merged->modules);
  ASSERT_FALSE(merged->top_msglist);

  if (cschema->data_root) {
    ASSERT_TRUE(merged->data_root);
    verify_generated_root_ypbc_mdesc(merged, cschema->data_root, merged->data_root);
  }
}

TEST(RwSchemaMerge, SimpleMerge)
{
  TEST_DESCRIPTION("Tests merge of company.yang and document.yang");

  const rw_yang_pb_schema_t* company  = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(Company);
  const rw_yang_pb_schema_t* document = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(Document);

  const rw_yang_pb_schema_t* unified = rw_schema_merge(NULL, company, document);
  ASSERT_TRUE(unified);

  UniquePtrRwYangPbSchema::uptr_t uptr((rw_yang_pb_schema_t *)unified);
  ASSERT_NE(unified, company);
  ASSERT_NE(unified, document);

  ASSERT_TRUE(unified->schema_module);
  verify_dynamic_schema_module(unified->schema_module, unified);

  ASSERT_TRUE(unified->modules);
  ASSERT_EQ(unified->module_count, 4);

  ns_to_mod_map_t modules;
  for (size_t i = 0; i < unified->module_count; i++) {
    auto module = unified->modules[i];
    modules.emplace(std::string(module->ns), module);
  }

  auto it = modules.find("http://riftio.com/ns/core/util/yangtools/tests/document");
  ASSERT_NE(it, modules.end());
  ASSERT_EQ(it->second, document->schema_module);

  it = modules.find("http://riftio.com/ns/core/util/yangtools/tests/company");
  ASSERT_NE(it, modules.end());
  ASSERT_EQ(it->second, company->schema_module);

  it = modules.find("http://riftio.com/ns/riftware-1.0/rw-pb-ext");
  ASSERT_NE(it, modules.end());
  ASSERT_EQ(it->second, company->modules[0]);
}

TEST(RwSchemaMerge, AugmentedSuperSet)
{
  TEST_DESCRIPTION("Tests merge of rw-fpath-d.yang and rw-base-d.yang. Here rw-fpath-d is a superset of rw-base-d");

  const rw_yang_pb_schema_t* rw_fpath = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwFpathD);
  const rw_yang_pb_schema_t* rw_base  = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwBaseD);

  const rw_yang_pb_schema_t* unified = rw_schema_merge(NULL, rw_fpath, rw_base);
  ASSERT_TRUE(unified);
  ASSERT_EQ(unified, rw_fpath);
}

TEST(RwSchemaMerge, AugmentSelOne)
{
  TEST_DESCRIPTION("Test merge of rw-fpath-d.yang and rw-dts-d.yang. Here rwbase is picked from rw-fpath-d");

  const rw_yang_pb_schema_t* rw_fpath = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwFpathD);
  const rw_yang_pb_schema_t* rw_dts   = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwDtsD);

  const rw_yang_pb_schema_t* unified = rw_schema_merge(NULL, rw_fpath, rw_dts);
  ASSERT_TRUE(unified);
  UniquePtrRwYangPbSchema::uptr_t uptr((rw_yang_pb_schema_t *)unified);

  ASSERT_NE(unified, rw_fpath);
  ASSERT_NE(unified, rw_dts);

  ASSERT_TRUE(unified->schema_module);
  verify_dynamic_schema_module(unified->schema_module, unified);

  ASSERT_TRUE(unified->modules);
  ASSERT_EQ(unified->module_count, 11);

  ns_to_mod_map_t modules;
  for (size_t i = 0; i < unified->module_count; i++) {
    auto module = unified->modules[i];
    modules.emplace(std::string(module->ns), module);
  }

  for (size_t i = 0; i < rw_fpath->module_count; i++) {
    auto mod = rw_fpath->modules[i];
    auto it = modules.find(mod->ns);
    ASSERT_NE(it, modules.end());
    ASSERT_EQ(it->second, mod);
  }

  for (size_t i = 0; i < rw_dts->module_count; i++) {
    auto mod = rw_dts->modules[i];
    auto it = modules.find(mod->ns);
    ASSERT_NE(it, modules.end());
    if (!strcmp(mod->ns, "http://riftio.com/ns/riftware-1.0/rw-base")) {
      ASSERT_NE(it->second, mod);
    } else {
      ASSERT_EQ(it->second, mod);
    }
  }
}

TEST(RwSchemaMerge, AugmentSMerge)
{
  TEST_DESCRIPTION("Tests merge of rw-fpath-d.yang and rw-vcs-d.yang");

  const rw_yang_pb_schema_t* rw_fpath  = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwFpathD);
  const rw_yang_pb_schema_t* rw_vcs    = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwVcsD);

  const rw_yang_pb_schema_t* unified = rw_schema_merge(NULL, rw_fpath, rw_vcs);
  ASSERT_TRUE(unified);
  UniquePtrRwYangPbSchema::uptr_t uptr((rw_yang_pb_schema_t *)unified);

  ASSERT_NE(unified, rw_fpath);
  ASSERT_NE(unified, rw_vcs);

  ASSERT_TRUE(unified->schema_module);
  verify_dynamic_schema_module(unified->schema_module, unified);

  ASSERT_TRUE(unified->modules);
  ASSERT_EQ(unified->module_count, 11);

  ns_to_mod_map_t unif_modules, fpath_modules, vcs_modules;
  for (size_t i = 0; i < unified->module_count; i++) {
    auto module = unified->modules[i];
    unif_modules.emplace(std::string(module->ns), module);
  }

  for (size_t i = 0; i < rw_fpath->module_count; i++) {
    auto module = rw_fpath->modules[i];
    fpath_modules.emplace(std::string(module->ns), module);
  }

  for (size_t i = 0; i < rw_vcs->module_count; i++) {
    auto module = rw_vcs->modules[i];
    vcs_modules.emplace(std::string(module->ns), module);
  }

  const char* rwbase_ns = "http://riftio.com/ns/riftware-1.0/rw-base";

  ns_to_mod_map_t::iterator it, it1, it2;
  for (auto& mod:fpath_modules) {
    it = unif_modules.find(mod.first);
    ASSERT_NE(it, unif_modules.end());
    if (it->first != rwbase_ns) {
      ASSERT_EQ(it->second, mod.second);
    }
  }

  for (auto& mod:vcs_modules) {
    it = unif_modules.find(mod.first);
    ASSERT_NE(it, unif_modules.end());
    if (it->first != rwbase_ns) {
      ASSERT_EQ(it->second, mod.second);
    }
  }

  // rw-base should be the merged one.
  auto merged_b = unif_modules.find(rwbase_ns)->second;
  auto fpath_b = fpath_modules.find(rwbase_ns)->second;
  auto vcs_b = vcs_modules.find(rwbase_ns)->second;

  verify_merged_module(fpath_b, vcs_b, merged_b);
  verify_dynamic_merged_module_root(unified, fpath_b->data_module, merged_b->data_module, false);

  if (fpath_b->rpci_module) {
    verify_dynamic_merged_module_root(unified, fpath_b->rpci_module, merged_b->rpci_module, false);
    verify_dynamic_merged_module_root(unified, fpath_b->rpco_module, merged_b->rpco_module, true);
  }

  if (fpath_b->notif_module) {
    verify_dynamic_merged_module_root(unified, fpath_b->notif_module, merged_b->notif_module, true);
  }

  top_msg_list mergedl; // empty
  top_msg_list fpathl;
  top_msg_list vcsl;

  fpathl.emplace_back("colony");
  fpathl.emplace_back("node");
  fpathl.emplace_back("vcs");
  fpathl.emplace_back("keytestlogging");
  fpathl.emplace_back("mytest-cont");
  vcsl.emplace_back("tasklet");

  verify_merged_module_top_msg_list(
      fpath_b->data_module, vcs_b->data_module, merged_b->data_module, mergedl, fpathl, vcsl);

  auto comp_schema = (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RwTestAu1);
  verify_dynamic_schema(comp_schema, unified);

  const rw_yang_pb_module_t* compare_mod = nullptr;
  for (unsigned i = 0; i < comp_schema->module_count; ++i) {
    if (!strcmp(comp_schema->modules[i]->ns, rwbase_ns)) {
      compare_mod = comp_schema->modules[i];
      break;
    }
  }

  ASSERT_TRUE(compare_mod);
  verify_merged_top_messages(unified, compare_mod->data_module, merged_b->data_module, mergedl);
}

TEST(RwSchemaMerge, AugmentMerged)
{
  TEST_DESCRIPTION("Tests merge of rw-fpath-d.yang and rw-appmgr-d.yang");

  const rw_yang_pb_schema_t* rw_fpath  = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwFpathD);
  const rw_yang_pb_schema_t* rw_appmgr = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwAppmgrD);

  const rw_yang_pb_schema_t* unified = rw_schema_merge(NULL, rw_fpath, rw_appmgr);
  ASSERT_TRUE(unified);
  UniquePtrRwYangPbSchema::uptr_t uptr((rw_yang_pb_schema_t *)unified);

  ASSERT_NE(unified, rw_fpath);
  ASSERT_NE(unified, rw_appmgr);

  ASSERT_TRUE(unified->schema_module);
  verify_dynamic_schema_module(unified->schema_module, unified);

  ASSERT_TRUE(unified->modules);
  ASSERT_EQ(unified->module_count, 12);

  ns_to_mod_map_t unif_modules, fpath_modules, appmgr_modules;
  for (size_t i = 0; i < unified->module_count; i++) {
    auto module = unified->modules[i];
    unif_modules.emplace(std::string(module->ns), module);
  }

  for (size_t i = 0; i < rw_fpath->module_count; i++) {
    auto module = rw_fpath->modules[i];
    fpath_modules.emplace(std::string(module->ns), module);
  }

  for (size_t i = 0; i < rw_appmgr->module_count; i++) {
    auto module = rw_appmgr->modules[i];
    appmgr_modules.emplace(std::string(module->ns), module);
  }

  const char* rwbase_ns = "http://riftio.com/ns/riftware-1.0/rw-base";

  ns_to_mod_map_t::iterator it, it1, it2;
  for (auto& mod:fpath_modules) {
    it = unif_modules.find(mod.first);
    ASSERT_NE(it, unif_modules.end());
    if (it->first != rwbase_ns) {
      ASSERT_EQ(it->second, mod.second);
    }
  }

  for (auto& mod:appmgr_modules) {
    it = unif_modules.find(mod.first);
    ASSERT_NE(it, unif_modules.end());
    if (it->first != rwbase_ns) {
      ASSERT_EQ(it->second, mod.second);
    }
  }

  // rw-base should be the merged one.
  auto merged = unif_modules.find(rwbase_ns)->second;
  auto fpath = fpath_modules.find(rwbase_ns)->second;
  auto appmgr = appmgr_modules.find(rwbase_ns)->second;

  verify_merged_module(fpath, appmgr, merged);
  verify_dynamic_merged_module_root(unified, fpath->data_module, merged->data_module, false);
  if (fpath->rpci_module) {
    verify_dynamic_merged_module_root(unified, fpath->rpci_module, merged->rpci_module, false);
    verify_dynamic_merged_module_root(unified, fpath->rpco_module, merged->rpco_module, true);
  }
  if (fpath->notif_module) {
    verify_dynamic_merged_module_root(unified, fpath->notif_module, merged->notif_module, true);
  }

  top_msg_list mergedl;
  top_msg_list fpathl;
  top_msg_list appmgrl;

  mergedl.emplace_back("colony");
  mergedl.emplace_back("mytest-cont");
  mergedl.emplace_back("vcs");
  fpathl.emplace_back("keytestlogging");
  fpathl.emplace_back("node");
  appmgrl.emplace_back("tasklet");

  verify_merged_module_top_msg_list(
      fpath->data_module, appmgr->data_module, merged->data_module, mergedl, fpathl, appmgrl);

  auto comp_schema = (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RwTestAu2);
  verify_dynamic_schema(comp_schema, unified);

  const rw_yang_pb_module_t* comp_mod = nullptr;
  for (unsigned i = 0; i < comp_schema->module_count; ++i) {
    if (!strcmp(comp_schema->modules[i]->ns, rwbase_ns)) {
      comp_mod = comp_schema->modules[i];
      break;
    }
  }
  ASSERT_TRUE(comp_mod);

  off_map.clear();
  off_map.emplace(RWPB_C_MSG_FIELD_TAG(RwTestAu2_RwBaseD_data, colony), colony_offset_list);
  off_map.emplace(RWPB_C_MSG_FIELD_TAG(RwTestAu2_RwBaseD_data_Vcs_Resources, vm), vm_offset_list);
  off_map.emplace(RWPB_C_MSG_FIELD_TAG(RwTestAu2_RwBaseD_data_Vcs, resources), res_offset_list);
  off_map.emplace(RWPB_C_MSG_FIELD_TAG(RwTestAu2_RwBaseD_data, vcs), vcs_offset_list);
  off_map.emplace(RWPB_C_MSG_FIELD_TAG(RwTestAu2_RwBaseD_data_MytestCont_Mylist_Deep1, deep2), deep2_offset_list);
  off_map.emplace(RWPB_C_MSG_FIELD_TAG(RwTestAu2_RwBaseD_data_MytestCont_Mylist, deep1), deep1_offset_list);
  off_map.emplace(RWPB_C_MSG_FIELD_TAG(RwTestAu2_RwBaseD_data_MytestCont, mylist), mylist_offset_list);
  off_map.emplace(RWPB_C_MSG_FIELD_TAG(RwTestAu2_RwBaseD_data, mytest_cont), mytest_cont_offset_list);

  verify_merged_top_messages(unified, comp_mod->data_module, merged->data_module, mergedl);
  ASSERT_FALSE(off_map.size());

  mergedl.clear();
  fpathl.clear();
  appmgrl.clear();

  mergedl.emplace_back("tracing");
  verify_merged_module_top_msg_list(
      fpath->rpci_module, appmgr->rpci_module, merged->rpci_module, mergedl, fpathl, appmgrl);

  off_map.emplace(RWPB_C_MSG_FIELD_TAG(RwTestAu2_RwBaseD_input, tracing), tracing_offset_list);
  verify_merged_top_messages(unified, comp_mod->rpci_module, merged->rpci_module, mergedl);

  ASSERT_FALSE(off_map.size());
}


TEST(RwSchemaMerge, CreateComposite)
{
  TEST_DESCRIPTION("Create composite through multiple schema merge");

  // This test case leaks and it is known.
  const rw_yang_pb_schema_t *dyn_schema = nullptr;
  create_dynamic_composite_d(&dyn_schema);

  const char* rwbase_ns = "http://riftio.com/ns/riftware-1.0/rw-base";
  auto comp_schema = (const rw_yang_pb_schema_t*)RWPB_G_SCHEMA_YPBCSD(RwCompositeD);
  verify_dynamic_schema(comp_schema, dyn_schema);

  const rw_yang_pb_module_t* comp_mod = nullptr;
  for (unsigned i = 0; i < comp_schema->module_count; ++i) {
    if (!strcmp(comp_schema->modules[i]->ns, rwbase_ns)) {
      comp_mod = comp_schema->modules[i];
      break;
    }
  }
  ASSERT_TRUE(comp_mod);

  const rw_yang_pb_module_t* merged = nullptr;
  for (unsigned i = 0; i < dyn_schema->module_count; ++i) {
    if (!strcmp(dyn_schema->modules[i]->ns, rwbase_ns)) {
      merged = dyn_schema->modules[i];
      break;
    }
  }
  ASSERT_TRUE(merged);

  top_msg_list mergedl;
  mergedl.emplace_back("colony");
  mergedl.emplace_back("mytest-cont");
  mergedl.emplace_back("vcs");
  mergedl.emplace_back("tasklet");

  verify_dynamic_merged_module_root(dyn_schema, comp_mod->data_module, merged->data_module, false);
  verify_merged_top_messages(dyn_schema, comp_mod->data_module, merged->data_module, mergedl);

  mergedl.clear();
  mergedl.emplace_back("tracing");

  verify_dynamic_merged_module_root(dyn_schema, comp_mod->rpci_module, merged->rpci_module, false);
  verify_merged_top_messages(dyn_schema, comp_mod->rpci_module, merged->rpci_module, mergedl);

  verify_dynamic_merged_module_root(dyn_schema, comp_mod->rpco_module, merged->rpco_module, true);
  verify_dynamic_merged_module_root(dyn_schema, comp_mod->notif_module, merged->notif_module, true);
}

#if 0
TEST(RwSchemaMerge, DISABLED_DuplicateNames)
{
  TEST_DESCRIPTION("Test duplicate names in the augmented modules");

  const rw_yang_pb_schema_t* fpath  = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwFpathD);
  const rw_yang_pb_schema_t* dup    = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwDuplicateD);

  // Currently asserting when there are duplicate names
  EXPECT_DEATH(rw_schema_merge(NULL, fpath, dup), "");
}
#endif

TEST(RwSchemaMerge, UseDynamicSchema)
{
  TEST_DESCRIPTION("Test using Dynamic schema in keyspec APIs");

  // This testcase leaks and it is a known thing
  const rw_yang_pb_schema_t *dyn_schema = nullptr;
  create_dynamic_composite_d(&dyn_schema);

  // Create a keyspec and msg using fpath schema and unpack using dynamic
  // schema.
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony, colony);
  strcpy(colony.name, "trafgen");
  colony.has_name = 1;

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext, nc);
  strcpy(nc.name, "nc1");
  nc.has_name = 1;

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface, intf);
  strcpy(intf.name, "intf1");
  intf.has_name = 1;

  intf.n_ip = 2;
  strcpy(intf.ip[0].address, "10.0.1.0");
  strcpy(intf.ip[1].address, "10.0.1.1");
  intf.ip[0].has_address = 1;
  intf.ip[1].has_address = 1;

  nc.n_interface = 1;
  RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface)* intfa[1];
  intfa[0] = &intf;
  nc.interface = &intfa[0];

  colony.n_network_context = 1;
  RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext)* nca[1];
  nca[0] = &nc;

  colony.network_context = &nca[0];

  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony, ks_colony);
  ks_colony.dompath.path000.has_key00 = true;
  strcpy(ks_colony.dompath.path000.key00.name, "trafgen");

  rw_keyspec_path_t *ks = rw_keyspec_path_create_dup_of_type(&ks_colony.rw_keyspec_path_t, NULL, &rw_schema_path_spec__descriptor);
  ASSERT_TRUE(ks);

  UniquePtrKeySpecPath::uptr_t uptr(ks);

  const rw_yang_pb_msgdesc_t* result = nullptr;
  auto s = rw_keyspec_path_find_msg_desc_schema(ks, NULL, dyn_schema, &result, NULL);
  ASSERT_EQ(s, RW_STATUS_SUCCESS);
  ASSERT_TRUE(result);

  auto msg_desc = rw_yang_pb_msgdesc_get_msg_msgdesc(result);
  ASSERT_TRUE(msg_desc);

  size_t len = 0;
  uint8_t* data = protobuf_c_message_serialize(NULL, &colony.base, &len);
  ASSERT_TRUE(data);

  UniquePtrProtobufCFree<>::uptr_t duptr(data);

  ProtobufCMessage* msg = protobuf_c_message_unpack(NULL, msg_desc->pbc_mdesc, len, data);
  ASSERT_TRUE(msg);

  UniquePtrProtobufCMessage<>::uptr_t uptrm(msg);

  ASSERT_TRUE(protobuf_c_message_is_equal_deep(NULL, &colony.base, msg));

  //Try rerooting..
  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface, ksi);
  ksi.dompath.path000.has_key00 = true;
  strcpy(ksi.dompath.path000.key00.name, "trafgen");

  ksi.dompath.path001.has_key00 = true;
  strcpy(ksi.dompath.path001.key00.name, "nc1");

  ksi.dompath.path002.has_key00 = true;
  strcpy(ksi.dompath.path002.key00.name, "intf1");

  ProtobufCMessage* rmsg = rw_keyspec_path_reroot(ks, NULL, msg, &ksi.rw_keyspec_path_t);
  RW_ASSERT(rmsg);

  UniquePtrProtobufCMessage<>::uptr_t uptrrr(rmsg);

  auto intfm = (RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface)*)
      protobuf_c_message_duplicate(NULL, rmsg, RWPB_G_MSG_PBCMD(RwFpathD_RwBaseD_data_Colony_NetworkContext_Interface));

  ASSERT_TRUE(intfm);
  UniquePtrProtobufCMessage<>::uptr_t ifm(&intfm->base);

  EXPECT_EQ(intfm->n_ip, 2);
  EXPECT_STREQ(intfm->ip[0].address, "10.0.1.0");
  EXPECT_STREQ(intfm->ip[1].address, "10.0.1.1");
}

TEST(RwSchemaMerge, XpathUsingDS)
{
  TEST_DESCRIPTION("Test Xpath to ks conversion using dynamic schema");

  // This test case leaks and it is a known thing
  const rw_yang_pb_schema_t *dyn_schema = nullptr;
  create_dynamic_composite_d(&dyn_schema);

  char xpath[1024];
  strcpy(xpath, "D,/rw-base:colony[rw-base:name='trafgen']/rw-fpath:bundle-ether[rw-fpath:name='bundle1']");

  auto keyspec = rw_keyspec_path_from_xpath(dyn_schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec);

  UniquePtrKeySpecPath::uptr_t uptr(keyspec);

  strcpy(xpath, "A,/rw-base:mytest-cont/rw-base:mylist/rw-base:deep1/rw-base:deep2/rw-fpath:fpath-leaf");
  keyspec = rw_keyspec_path_from_xpath(dyn_schema, xpath, RW_XPATH_KEYSPEC, nullptr);
  ASSERT_TRUE(keyspec);
  uptr.reset (keyspec);

  char print_buf[512];
  int ret = rw_keyspec_path_get_print_buffer(keyspec, nullptr , nullptr, print_buf, sizeof(print_buf));
  EXPECT_NE(ret, -1);

  EXPECT_STREQ(print_buf, xpath);
}

static RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony)*
create_fpath_colony_msg()
{
  RWPB_M_MSG_DECL_PTR_ALLOC(RwFpathD_RwBaseD_data_Colony, colony);
  strcpy(colony->name, "trafgen");
  colony->n_bundle_ether = 1;
  strcpy(colony->bundle_ether[0].name, "be1");
  colony->bundle_ether[0].has_descr_string = true;
  strcpy(colony->bundle_ether[0].descr_string, "descp");

  colony->bundle_ether[0].has_open = true;
  colony->bundle_ether[0].open = true;
  colony->bundle_ether[0].has_mtu = true;
  colony->bundle_ether[0].mtu = 1500;

  return colony;
}

static RWPB_T_MSG(RwAppmgrD_RwBaseD_data_Colony)*
create_appmgr_colony_msg()
{
  RWPB_M_MSG_DECL_PTR_ALLOC(RwAppmgrD_RwBaseD_data_Colony, col);
  strcpy(col->name, "trafgen");

  col->n_trafsim_service = 1;
  col->trafsim_service = (RWPB_T_MSG(RwAppmgrD_RwBaseD_data_Colony_TrafsimService)**)
      protobuf_c_instance_alloc(NULL, sizeof(void*));

  RWPB_M_MSG_DECL_PTR_ALLOC(RwAppmgrD_RwBaseD_data_Colony_TrafsimService, ts);
  col->trafsim_service[0] = ts;

  strcpy(ts->name, "trafsim1");
  RWPB_M_MSG_DECL_PTR_ALLOC(RwAppmgrD_RwBaseD_data_Colony_TrafsimService_ProtocolMode, pm);
  ts->protocol_mode = pm;
  RWPB_M_MSG_DECL_PTR_ALLOC(RwAppmgrD_RwBaseD_data_Colony_TrafsimService_ProtocolMode_PktgenSim, ps);
  ts->protocol_mode->pktgen_sim = ps;

  ps->has_instances_per_vm = true;
  ps->instances_per_vm = 100;
  ps->has_number_of_vm = true;
  ps->number_of_vm = 12;
  ps->has_test_file_name = true;
  strcpy(ps->test_file_name, "hello");

  return col;
}

TEST(RwSchemaMerge, YangModelDynSch)
{
  YangModelNcx* model = YangModelNcx::create_model();
  YangModel::ptr_t p(model);
  ASSERT_TRUE(model);

  const rw_yang_pb_schema_t *dyn_schema = nullptr;
  create_dynamic_composite_d(&dyn_schema);
  ASSERT_TRUE(dyn_schema);

  model->load_schema_ypbc(dyn_schema);
  model->register_ypbc_schema(dyn_schema);

  // Search the yangmodel using nsid and tag
  rw_namespace_id_t nsid = NamespaceManager::get_global().get_ns_hash("http://riftio.com/ns/riftware-1.0/rw-base");
  uint32_t tag = RWPB_G_MSG_YPBCD(RwFpathD_RwBaseD_data_Colony)->ypbc.pb_element_tag;

  auto yn = model->search_node(nsid, tag);
  ASSERT_TRUE(yn);

  auto ypbc_mdesc = yn->get_ypbc_msgdesc();
  ASSERT_TRUE(ypbc_mdesc);

  auto dy_ns = ypbc_mdesc->module->schema->schema_module->ns;
  ASSERT_TRUE(NamespaceManager::get_global().ns_is_dynamic(dy_ns));

  auto colonyf = create_fpath_colony_msg();
  UniquePtrProtobufCMessage<>::uptr_t uptr1(&colonyf->base);

  auto colonya = create_appmgr_colony_msg();
  UniquePtrProtobufCMessage<>::uptr_t uptr2(&colonya->base);

  size_t len1 = 0, len2 = 0;
  uint8_t* data1 = protobuf_c_message_serialize(NULL, &colonyf->base, &len1);
  ASSERT_TRUE(data1);
  UniquePtrProtobufCFree<>::uptr_t uptrd(data1);

  uint8_t* data2 = protobuf_c_message_serialize(NULL, &colonya->base, &len2);
  ASSERT_TRUE(data2);
  UniquePtrProtobufCFree<>::uptr_t uptrd1(data2);

  uint8_t res[len1+len2];
  memcpy(res, data1, len1);
  memcpy(res+len1, data2, len2);

  auto outm = protobuf_c_message_unpack(NULL, ypbc_mdesc->pbc_mdesc, len1+len2, res);
  ASSERT_TRUE(outm);
  UniquePtrProtobufCMessage<>::uptr_t uptr3(outm);

  XMLManager::uptr_t mgr(xml_manager_create_xerces(model));
  ASSERT_TRUE(mgr.get());

  rw_yang_netconf_op_status_t status;
  XMLDocument::uptr_t dom(mgr->create_document_from_pbcm (outm, status));
  ASSERT_TRUE(dom.get());

  std::cout << dom->to_string_pretty() << std::endl;
}

TEST(RwSchemaMerge, DynamicSchemaRoot)
{
  XMLManager::uptr_t mgr(xml_manager_create_xerces());
  ASSERT_TRUE(mgr.get());

  YangModel* model = mgr->get_yang_model();
  ASSERT_TRUE(model);

  const rw_yang_pb_schema_t *dyn_schema = nullptr;
  create_dynamic_composite_d(&dyn_schema);
  ASSERT_TRUE(dyn_schema);

  model->load_schema_ypbc(dyn_schema);
  model->register_ypbc_schema(dyn_schema);

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data, rwbase);
  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony, colony1);
  strcpy(colony1.name, "colony1");
  colony1.has_name = 1;
  colony1.n_bundle_ether = 2;

  for (unsigned i = 0; i < 2; ++i) {
    sprintf(colony1.bundle_ether[i].name, "%s%d", "be1", i+1);
    colony1.bundle_ether[i].has_name = 1;
    colony1.bundle_ether[i].has_descr_string = true;
    strcpy(colony1.bundle_ether[i].descr_string, "bundle ether1");
    colony1.bundle_ether[i].has_open = true;
    colony1.bundle_ether[i].open = true;
    colony1.bundle_ether[i].has_mtu = true;
    colony1.bundle_ether[i].mtu = 1500;
    colony1.bundle_ether[i].has_shutdown = true;
    colony1.bundle_ether[i].shutdown = true;
    colony1.bundle_ether[i].has_lacp = true;
    colony1.bundle_ether[i].lacp.has_system = true;
    colony1.bundle_ether[i].lacp.system.has_priority = true;
    colony1.bundle_ether[i].lacp.system.priority = 1;
  }

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony_BundleState, bs);
  bs.n_bundle_ether = 1;
  strcpy(bs.bundle_ether[0].name, "bundle1");
  bs.bundle_ether[0].has_name = 1;
  colony1.bundle_state = &bs;

  RWPB_M_MSG_DECL_INIT(RwFpathD_RwBaseD_data_Colony, colony2);
  strcpy(colony2.name, "colony2");
  colony2.has_name = 1;
  colony2.n_bundle_ether = 2;

  for (unsigned i = 0; i < 2; ++i) {
    sprintf(colony2.bundle_ether[i].name, "%s%d", "be2", i+1);
    colony2.bundle_ether[i].has_name = 1;
    colony2.bundle_ether[i].has_descr_string = true;
    strcpy(colony2.bundle_ether[i].descr_string, "bundle ether1");
    colony2.bundle_ether[i].has_open = true;
    colony2.bundle_ether[i].open = true;
    colony2.bundle_ether[i].has_mtu = true;
    colony2.bundle_ether[i].mtu = 1500;
    colony2.bundle_ether[i].has_shutdown = true;
    colony2.bundle_ether[i].shutdown = true;
    colony2.bundle_ether[i].has_bundle = true;
    colony2.bundle_ether[i].bundle.has_minimum_active = true;
    colony2.bundle_ether[i].bundle.minimum_active.has_links = true;
    colony2.bundle_ether[i].bundle.minimum_active.links = 100;
    colony2.bundle_ether[i].n_vlan = 1;
    colony2.bundle_ether[i].vlan[0].id = 1234;
    colony2.bundle_ether[i].vlan[0].has_id = 1;
    colony2.bundle_ether[i].vlan[0].has_descr_string = true;
    strcpy(colony2.bundle_ether[i].vlan[0].descr_string, "VLAN1");
    colony2.bundle_ether[i].vlan[0].has_open = true;
    colony2.bundle_ether[i].vlan[0].open = true;
  }

  RWPB_T_MSG(RwFpathD_RwBaseD_data_Colony)* arr[2];
  arr[0] = &colony1;
  arr[1] = &colony2;

  rwbase.n_colony = 2;
  rwbase.colony = &arr[0];

  size_t len = 0;
  uint8_t* data = protobuf_c_message_serialize(NULL, &rwbase.base, &len);
  ASSERT_TRUE(data);
  UniquePtrProtobufCFree<>::uptr_t uptrd(data);

  auto mod = model->search_module_ns("http://riftio.com/ns/riftware-1.0/rw-base");
  ASSERT_TRUE(mod);

  auto root_ydesc = mod->get_ypbc_module()->data_module;
  ASSERT_TRUE(root_ydesc);

  ProtobufCMessage* out = protobuf_c_message_unpack(NULL, root_ydesc->pbc_mdesc, len, data);
  ASSERT_TRUE(out);
  UniquePtrProtobufCMessage<>::uptr_t uptr(out);

  rw_yang_netconf_op_status_t status;
  XMLDocument::uptr_t dom(mgr->create_document_from_pbcm (out, status));
  ASSERT_TRUE(dom.get());

  rw_keyspec_path_t* rks = nullptr;
  rw_keyspec_path_create_dup(root_ydesc->schema_path_value, NULL, &rks);

  ASSERT_TRUE(rks);

  UniquePtrKeySpecPath::uptr_t uptrks(rks);

  auto ynode = mod->search_node("colony", "http://riftio.com/ns/riftware-1.0/rw-base")->
      search_child("bundle-state", "http://riftio.com/ns/riftware-1.0/rw-fpath");
  ASSERT_TRUE(ynode);

  RWPB_M_PATHSPEC_DECL_INIT(RwFpathD_RwBaseD_data_Colony_BundleState, ksbs);
  ksbs.dompath.path000.has_key00 = true;
  strcpy(ksbs.dompath.path000.key00.name, "colony1");

  ProtobufCMessage *res = rw_keyspec_path_reroot(rks, NULL, out, &ksbs.rw_keyspec_path_t);
  ASSERT_TRUE(res);

  uptr.reset(res);
}

static void create_test_composite(const rw_yang_pb_schema_t** dyns)
{
  const rw_yang_pb_schema_t* mano_base  = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(TestAManoBase);
  const rw_yang_pb_schema_t* clid = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwuagentCliD);

  const rw_yang_pb_schema_t* unified = rw_schema_merge(NULL, mano_base, clid);
  ASSERT_TRUE(unified);

  ASSERT_NE(unified, clid);
  ASSERT_NE(unified, mano_base);

  const rw_yang_pb_schema_t* iprecad  = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(TestARwIpreceiverappData);
  const rw_yang_pb_schema_t* unified2 = rw_schema_merge(NULL, unified, iprecad);

  ASSERT_TRUE(unified2);
  ASSERT_NE(unified2, iprecad);
  ASSERT_NE(unified2, unified);

  const rw_yang_pb_schema_t* vnf_base = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(TestARwVnfBaseConfig);
  const rw_yang_pb_schema_t* unified3 = rw_schema_merge(NULL, unified2, vnf_base);

  ASSERT_TRUE(unified3);
  ASSERT_NE(unified3, vnf_base);
  ASSERT_NE(unified3, unified2);

  const rw_yang_pb_schema_t* rw_base  = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(RwBaseD);
  const rw_yang_pb_schema_t* unified4 = rw_schema_merge(NULL, unified3, rw_base);

  ASSERT_TRUE(unified4);
  ASSERT_NE(unified4, rw_base);
  ASSERT_NE(unified4, unified3);

  const rw_yang_pb_schema_t* dest_nat = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(TestARwDestNat);
  const rw_yang_pb_schema_t* unified5 = rw_schema_merge(NULL, unified4, dest_nat);

  ASSERT_TRUE(unified5);
  ASSERT_NE(unified5, dest_nat);
  ASSERT_NE(unified5, unified4);

  const rw_yang_pb_schema_t* ip_recv = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(TestARwIpReceiverApp);
  const rw_yang_pb_schema_t* unified6 = rw_schema_merge(NULL, unified5, ip_recv);

  ASSERT_TRUE(unified6);
  ASSERT_NE(unified6, ip_recv);
  ASSERT_NE(unified6, unified5);

  const rw_yang_pb_schema_t* vnfb_op = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(TestARwVnfBaseOpdata);
  const rw_yang_pb_schema_t* unified7 = rw_schema_merge(NULL, unified6, vnfb_op);

  ASSERT_TRUE(unified7);
  ASSERT_EQ(unified7, unified6);

  const rw_yang_pb_schema_t* sfmgr = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(TestARwSfmgrData);
  const rw_yang_pb_schema_t* unified8 = rw_schema_merge(NULL, unified7, sfmgr);

  ASSERT_TRUE(unified8);
  ASSERT_NE(unified8, sfmgr);
  ASSERT_NE(unified8, unified7);

  const rw_yang_pb_schema_t* trafgen_d = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(TestARwTrafgenData);
  const rw_yang_pb_schema_t* unified9 = rw_schema_merge(NULL, unified8, trafgen_d);

  ASSERT_TRUE(unified9);
  ASSERT_NE(unified9, trafgen_d);
  ASSERT_NE(unified9, unified8);

  const rw_yang_pb_schema_t* fpath_d = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(TestARwFpathData);
  const rw_yang_pb_schema_t* unified10 = rw_schema_merge(NULL, unified9, fpath_d);

  ASSERT_TRUE(unified10);
  ASSERT_NE(unified10, fpath_d);
  ASSERT_NE(unified10, unified9);

  *dyns = unified10;
}

TEST(RwSchemaMerge, RealComposite)
{
  const rw_yang_pb_schema_t *dyn_schema = nullptr;

  create_test_composite(&dyn_schema);
  ASSERT_TRUE (dyn_schema);

  const rw_yang_pb_schema_t* composite = (const rw_yang_pb_schema_t*) RWPB_G_SCHEMA_YPBCSD(TestAComposite);

  ASSERT_EQ (composite->module_count, dyn_schema->module_count);
  verify_dynamic_schema(composite, dyn_schema);

  ns_to_mod_map_t modules;
  for (size_t i = 0; i < composite->module_count; i++) {
    auto module = composite->modules[i];
    modules.emplace(std::string(module->ns), module);
  }

  unsigned merged_mod = 0;
  for (size_t i = 0; i < dyn_schema->module_count; i++) {
    auto mod = dyn_schema->modules[i];
    if (NamespaceManager::get_global().ns_is_dynamic(mod->ns)){
      continue;
    }
    auto it = modules.find(mod->ns);
    ASSERT_NE(it, modules.end());
    if (it->second != mod) {
      merged_mod++;
    }
  }

  ASSERT_EQ (merged_mod, 3);

  const char* mbns  = "http://riftio.com/ns/riftware-1.0/test-a-mano-base";
  const rw_yang_pb_module_t* comp_mod = nullptr;
  for (unsigned i = 0; i < composite->module_count; ++i) {
    if (!strcmp(composite->modules[i]->ns, mbns)) {
      comp_mod = composite->modules[i];
      break;
    }
  }
  ASSERT_TRUE(comp_mod);

  const rw_yang_pb_module_t* merged = nullptr;
  for (unsigned i = 0; i < dyn_schema->module_count; ++i) {
    if (!strcmp(dyn_schema->modules[i]->ns, mbns)) {
      merged = dyn_schema->modules[i];
      break;
    }
  }
  ASSERT_TRUE(merged);
  ASSERT_NE (merged, comp_mod);

  top_msg_list mergedl;
  mergedl.emplace_back("vnf-config");
  mergedl.emplace_back("vnf-opdata");

  verify_dynamic_merged_module_root(dyn_schema, comp_mod->data_module, merged->data_module, false);
  verify_merged_top_messages(dyn_schema, comp_mod->data_module, merged->data_module, mergedl);

  const char* vnfopb  = "http://riftio.com/ns/riftware-1.0/rw-vnf-base-opdata";
  comp_mod = nullptr;
  for (unsigned i = 0; i < composite->module_count; ++i) {
    if (!strcmp(composite->modules[i]->ns, vnfopb)) {
      comp_mod = composite->modules[i];
      break;
    }
  }
  ASSERT_TRUE(comp_mod);

  merged = nullptr;
  for (unsigned i = 0; i < dyn_schema->module_count; ++i) {
    if (!strcmp(dyn_schema->modules[i]->ns, vnfopb)) {
      merged = dyn_schema->modules[i];
      break;
    }
  }
  ASSERT_TRUE(merged);
  ASSERT_NE (merged, comp_mod);

  verify_dynamic_merged_module_root(dyn_schema, comp_mod->rpci_module, merged->rpci_module, false);
  verify_dynamic_merged_module_root(dyn_schema, comp_mod->rpco_module, merged->rpco_module, true);

  mergedl.clear();
  mergedl.emplace_back("clear-data");
  verify_merged_top_messages(dyn_schema, comp_mod->rpci_module, merged->rpci_module, mergedl);
}

TEST(RwPbSchema, FindMsgDesc)
{
  // Test finding descriptors for various top-level nodes
  // 1. Notification
  const rw_yang_pb_schema_t *dyn_schema = nullptr;
  create_dynamic_composite_d(&dyn_schema);
  ASSERT_TRUE(dyn_schema);

  auto schema_id = rw_keyspec_entry_get_schema_id((const rw_keyspec_entry_t *)RWPB_G_PATHENTRY_VALUE(RwDtsApiLogD_notif_DtsapiDebug));

  auto ypbc_mdesc = rw_yang_pb_schema_get_ypbc_mdesc(dyn_schema, schema_id.ns, schema_id.tag);
  ASSERT_TRUE(ypbc_mdesc);
  ASSERT_EQ(ypbc_mdesc, (const rw_yang_pb_msgdesc_t*)RWPB_G_MSG_YPBCD(RwDtsApiLogD_notif_DtsapiDebug));

  // 2. Data Node
  schema_id = rw_keyspec_entry_get_schema_id((const rw_keyspec_entry_t *)RWPB_G_PATHENTRY_VALUE(RwFpathD_RwBaseD_data_Colony));
  ypbc_mdesc = rw_yang_pb_schema_get_ypbc_mdesc(dyn_schema, schema_id.ns, schema_id.tag);
  ASSERT_TRUE(ypbc_mdesc);
  ASSERT_NE(ypbc_mdesc, (const rw_yang_pb_msgdesc_t*)RWPB_G_MSG_YPBCD(RwFpathD_RwBaseD_data_Colony));

  schema_id = rw_keyspec_entry_get_schema_id((const rw_keyspec_entry_t *)RWPB_G_PATHENTRY_VALUE(RwmanifestD_data_Manifest));
  ypbc_mdesc = rw_yang_pb_schema_get_ypbc_mdesc(dyn_schema, schema_id.ns, schema_id.tag);
  ASSERT_TRUE(ypbc_mdesc);
  ASSERT_EQ(ypbc_mdesc, (const rw_yang_pb_msgdesc_t*)RWPB_G_MSG_YPBCD(RwmanifestD_data_Manifest));

  //3. Rpci
  schema_id = rw_keyspec_entry_get_schema_id((const rw_keyspec_entry_t *)RWPB_G_PATHENTRY_VALUE(RwFpathD_input_Start));
  ypbc_mdesc = rw_yang_pb_schema_get_ypbc_mdesc(dyn_schema, schema_id.ns, schema_id.tag);
  ASSERT_TRUE(ypbc_mdesc);
  ASSERT_EQ(ypbc_mdesc, (const rw_yang_pb_msgdesc_t*)RWPB_G_MSG_YPBCD(RwFpathD_input_Start));

  //4. Rpco
  schema_id = rw_keyspec_entry_get_schema_id((const rw_keyspec_entry_t *)RWPB_G_PATHENTRY_VALUE(RwFpathD_output_FpathDebug));
  ypbc_mdesc = rw_yang_pb_schema_get_ypbc_mdesc(dyn_schema, schema_id.ns, schema_id.tag);
  ASSERT_TRUE(ypbc_mdesc);
  ASSERT_EQ(ypbc_mdesc, (const rw_yang_pb_msgdesc_t*)RWPB_G_MSG_YPBCD(RwFpathD_output_FpathDebug));

  //5. Error case
  schema_id = rw_keyspec_entry_get_schema_id((const rw_keyspec_entry_t *)RWPB_G_PATHENTRY_VALUE(RwFpathD_RwBaseD_data_Colony_NetworkContext));
  ypbc_mdesc = rw_yang_pb_schema_get_ypbc_mdesc(dyn_schema, schema_id.ns, schema_id.tag);
  ASSERT_FALSE(ypbc_mdesc);
}

