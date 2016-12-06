
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


/*!
 * @file rw_protobuf_c.cpp
 * @date 2015/09/28
 * @brief RIFT.ware-specific protobuf-c support.
 */

#include <rw_protobuf_c.h>
#include <rw_keyspec.h>
#include <protobuf-c/rift-protobuf-c.h>
#include <protobuf-c/rift/rw_pb_delta.h>
#include "rw_ks_util.hpp"

#include <algorithm>

using namespace rw_yang;


/* ATTN: This function should not be named protobuf_c... */
const ProtobufCFieldDescriptor* protobuf_c_message_descriptor_get_field_by_yang(
  const ProtobufCMessageDescriptor *desc,
  const char *name,
  const char *ns)
{
  PROTOBUF_C_ASSERT (desc);
  PROTOBUF_C_ASSERT (name);

  if (!desc->ypbc_mdesc) {
    return 0;
  }

  //Walk the list of field descriptions from the Yang message descriptions
  const rw_yang_pb_msgdesc_t *y_msg = desc->ypbc_mdesc;

  for (size_t i = 0; i < y_msg->num_fields; i++) {

    const rw_yang_pb_flddesc_t *y_fld = &y_msg->ypbc_flddescs[i];
    RW_ASSERT(y_fld);

    if (!strcmp (name, y_fld->yang_node_name)) {
      return y_fld->pbc_fdesc;
    }
  }

  return 0;
}

/* ATTN: This function should not be named protobuf_c... And it is not message related - it is field */
const char* protobuf_c_message_get_pb_enum (
  const ProtobufCFieldDescriptor *pbcfd,
  const char *yang_enum)
{
  if ((PROTOBUF_C_TYPE_ENUM != pbcfd->type) || (!pbcfd->descriptor)) {
    // no enum descriptor?
    return nullptr;
  }

  const ProtobufCEnumDescriptor* pbced = pbcfd->enum_desc;
  const rw_yang_pb_enumdesc_t* pb_enumdesc = pbced->rw_yang_enum;
  if (!pb_enumdesc) {
    // no enum helper?
    return nullptr;
  }

  // Do a binary search, looking for the lower bound.
  const uint32_t* found = std::lower_bound(
      pb_enumdesc->values_by_yang_name,
      pb_enumdesc->values_by_yang_name + pbced->n_value_names,
      yang_enum,
      [pb_enumdesc](uint32_t i, const char* value) {
        RW_ASSERT(pb_enumdesc->yang_enums[i]);
        RW_ASSERT(pb_enumdesc->yang_enums[i][0]);
        return strcmp(pb_enumdesc->yang_enums[i], value) < 0;
      }
                                               );
  if (found >= (pb_enumdesc->values_by_yang_name + pbced->n_value_names)) {
    // not found
    return nullptr;
  }

  RW_ASSERT(found >= pb_enumdesc->values_by_yang_name);
  auto values_by_name_n = *found;

  /*
   * At this point, values_by_name_n is the index into
   * pbced->values_by_name[] and also pb_enumdesc->yang_enums[]
   * of the lower-bound enum string.
   */

  if (0 != strcmp(pb_enumdesc->yang_enums[values_by_name_n], yang_enum)) {
    // not a match
    return nullptr;
  }

  return (pbced->values_by_name[values_by_name_n].name);

}

/* ATTN: This function should not be named protobuf_c...  And it is not message related - it is field */
const char* protobuf_c_message_get_yang_enum (
  const ProtobufCFieldInfo *finfo)
{
  const ProtobufCFieldDescriptor *fdesc = finfo->fdesc;

  if ((!fdesc) || (PROTOBUF_C_TYPE_ENUM != fdesc->type)) {
    return 0;
  }

  int val = *((int *) finfo->element);
  const ProtobufCEnumDescriptor *enum_desc = fdesc->enum_desc;
  size_t i = 0;

  for (i = 0; (i < enum_desc->n_values) && (enum_desc->values[i].value < val); i++) {
  }

  if ((i == enum_desc->n_values) || (enum_desc->values[i].value != val)) {
    return 0;
  }

  size_t index = i;
  for (i = 0; (i < enum_desc->n_value_names); i++) {
    if (enum_desc->values_by_name[i].index == index) {
      return (enum_desc->values_by_name[i].name);
    }
  }

  return 0;
}

const char* protobuf_c_message_get_yang_node_name(
  const ProtobufCMessageDescriptor *mdesc,
  const ProtobufCFieldDescriptor *fdesc)
{
  const rw_yang_pb_msgdesc_t *ypbc =  mdesc->ypbc_mdesc;

  if ((nullptr == ypbc) || (nullptr == ypbc->ypbc_flddescs))  {
    return nullptr;
  }

  for (size_t i = 0; i < ypbc->num_fields; i++) {
    const rw_yang_pb_flddesc_t *y_desc = &ypbc->ypbc_flddescs[i];
    if (y_desc->pbc_fdesc == fdesc) {
      return y_desc->yang_node_name;
    }
  }

  return nullptr;
}

const rw_yang_pb_msgdesc_t* protobuf_c_message_dscriptor_yang_pb_msgdesc(
  const ProtobufCMessageDescriptor* pbcmd )
{
  RW_ASSERT( PROTOBUF_C_IS_MESSAGE_DESCRIPTOR(pbcmd) );
  return pbcmd->ypbc_mdesc;
}

const char* protobuf_c_message_descriptor_xml_prefix(
  const ProtobufCMessageDescriptor* pbcmd )
{
  RW_ASSERT( PROTOBUF_C_IS_MESSAGE_DESCRIPTOR(pbcmd) );
  auto msgdesc = protobuf_c_message_dscriptor_yang_pb_msgdesc(pbcmd);
  if (!msgdesc) {
    return nullptr;
  }

  // Input may have been keyspec-related.  Convert to message.
  auto msg_msgdesc = rw_yang_pb_msgdesc_get_msg_msgdesc(msgdesc);
  RW_ASSERT(msg_msgdesc);

  auto module = msg_msgdesc->module;
  if (!module) {
    return nullptr;
  }

  return module->prefix;
}

const char* protobuf_c_message_descriptor_xml_ns(
  const ProtobufCMessageDescriptor* pbcmd )
{
  RW_ASSERT( PROTOBUF_C_IS_MESSAGE_DESCRIPTOR(pbcmd) );
  auto msgdesc = protobuf_c_message_dscriptor_yang_pb_msgdesc(pbcmd);
  if (!msgdesc) {
    return nullptr;
  }

  // Input may have been keyspec-related.  Convert to message.
  auto msg_msgdesc = rw_yang_pb_msgdesc_get_msg_msgdesc(msgdesc);
  RW_ASSERT(msg_msgdesc);

  auto module = msg_msgdesc->module;
  if (!module) {
    return nullptr;
  }

  return module->ns;
}

const char* protobuf_c_message_descriptor_xml_element_name(
  const ProtobufCMessageDescriptor* pbcmd )
{
  RW_ASSERT( PROTOBUF_C_IS_MESSAGE_DESCRIPTOR(pbcmd) );
  auto msgdesc = protobuf_c_message_dscriptor_yang_pb_msgdesc(pbcmd);
  if (!msgdesc) {
    return nullptr;
  }

  // Input may have been keyspec-related.  Convert to message.
  auto msg_msgdesc = rw_yang_pb_msgdesc_get_msg_msgdesc(msgdesc);
  RW_ASSERT(msg_msgdesc);

  return msg_msgdesc->yang_node_name;
}

uint64_t
protobuf_c_field_ref_goto_ks(ProtobufCFieldReference* fref,
                             ProtobufCMessage* msg,
                             const rw_keyspec_path_t* ks)
{
  RW_ASSERT (fref);
  RW_ASSERT (msg);
  RW_ASSERT (ks);

  KeySpecHelper ksh(ks);
  RW_ASSERT (!ksh.has_wildcards());

  size_t depth = ksh.get_depth();
  RW_ASSERT (depth);

  auto ypbc_mdesc = msg->descriptor->ypbc_mdesc;
  RW_ASSERT (ypbc_mdesc);

  KeySpecHelper mks(ypbc_mdesc->schema_path_value);
  size_t depth_m = mks.get_depth();

  RW_ASSERT(depth_m < depth);

  auto path_entry = ksh.get_path_entry(depth-1);
  RW_ASSERT (path_entry);

  auto elem_id = rw_keyspec_entry_get_element_id(path_entry);
  RW_ASSERT (elem_id);

  if (depth == (depth_m + 1)) {
    return protobuf_c_field_ref_goto_tag(fref, msg, elem_id->element_tag);
  }

  rw_keyspec_path_t *dup_ks = nullptr;
  rw_keyspec_path_create_dup(ks, NULL, &dup_ks);
  RW_ASSERT (dup_ks);

  UniquePtrKeySpecPath::uptr_t uptr(dup_ks);

  rw_status_t s = rw_keyspec_path_trunc_suffix_n(dup_ks, NULL, depth-1); // ATTN:- Should not use NULL here.
  RW_ASSERT (s == RW_STATUS_SUCCESS);

  KeySpecHelper oks(dup_ks);

  const ProtobufCMessage* target = keyspec_find_deeper(NULL, depth_m, msg, oks, depth-1); // ATTN:- Should not use NULL here.
  RW_ASSERT (target);

  return protobuf_c_field_ref_goto_tag(fref, (ProtobufCMessage *)target, elem_id->element_tag);
}

uint64_t 
protobuf_c_field_ref_goto_xpath(ProtobufCFieldReference* fref,
                                ProtobufCMessage* msg,
                                const gchar* xpath)
{
  RW_ASSERT (fref);
  RW_ASSERT (msg);
  RW_ASSERT (xpath && strlen(xpath));

  auto ypbc_mdesc = msg->descriptor->ypbc_mdesc;
  RW_ASSERT (ypbc_mdesc);

  auto schema = ypbc_mdesc->module->schema;
  RW_ASSERT (schema);

  auto ks = rw_keyspec_path_from_xpath(
      schema, xpath, RW_XPATH_INSTANCEID, NULL); /* ATTN:- Should not use NULL here */

  RW_ASSERT (ks);
  UniquePtrKeySpecPath::uptr_t uptr(ks);

  return protobuf_c_field_ref_goto_ks(fref, msg, ks);
}
