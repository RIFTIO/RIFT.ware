
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
 * @file rw_ks_helper.cpp
 * @author Sujithra Periasamy
 * @date 2014/08/29
 * @brief Keyspec helper class implementattion
 */

#include <time.h>       /* for time, localtime... */
#include <iostream>
#include <iomanip>
#include "rw_keyspec.h"
#include "rw_schema.pb-c.h"
#include "rw_ks_util.hpp"

/**********************************************************
 * Keyspec helper class
 * ********************************************************/
KeySpecHelper::KeySpecHelper(const rw_keyspec_path_t *ks)
  :keyspec_(ks)
{
}

const rw_keyspec_path_t*
KeySpecHelper::get() const
{
  return keyspec_;
}

void 
KeySpecHelper::set(const rw_keyspec_path_t *ks)
{
  keyspec_ = ks;
}

const ProtobufCMessage*
KeySpecHelper::get_dompath()
{
  ProtobufCMessage* dom_path = nullptr;

  const ProtobufCFieldDescriptor *dom_fd = nullptr;
  const ProtobufCFieldDescriptor *bin_fd = nullptr;
  protobuf_c_boolean is_dptr = false;
  void *field_ptr = nullptr;
  size_t offset = 0;

  size_t count = protobuf_c_message_get_field_desc_count_and_offset(&keyspec_->base,
        RW_SCHEMA_TAG_KEYSPEC_DOMPATH, &dom_fd, &field_ptr, &offset, &is_dptr);

  RW_ASSERT(dom_fd);
  RW_ASSERT(dom_fd->label != PROTOBUF_C_LABEL_REPEATED);
  RW_ASSERT(dom_fd->type == PROTOBUF_C_TYPE_MESSAGE);

  if (!count) {

    size_t bcount = protobuf_c_message_get_field_desc_count_and_offset(&keyspec_->base,
            RW_SCHEMA_TAG_KEYSPEC_BINPATH, &bin_fd, &field_ptr, &offset, &is_dptr);

    if (!bcount) {
      return dom_path;
    }

    RW_ASSERT(bin_fd);
    RW_ASSERT(bin_fd->type == PROTOBUF_C_TYPE_BYTES);
    RW_ASSERT(bin_fd->label != PROTOBUF_C_LABEL_REPEATED);

    uint8_t *data = nullptr;
    size_t len = 0;

    if (bin_fd->rw_flags & RW_PROTOBUF_FOPT_INLINE) {
      data = ((ProtobufCFlatBinaryData *)field_ptr)->data;
      len =  ((ProtobufCFlatBinaryData *)field_ptr)->len;
    } else {
      data = ((ProtobufCBinaryData *)field_ptr)->data;
      len =  ((ProtobufCBinaryData *)field_ptr)->len;
    }

    dom_path = protobuf_c_message_unpack(nullptr, dom_fd->msg_desc, len, data);
    if (dom_path) {
      dompath_.reset(dom_path);
    }
  } else {
    dom_path = (ProtobufCMessage *)field_ptr;
  }

  return dom_path;
}

size_t
KeySpecHelper::get_binpath(const uint8_t **bdata)
{
  size_t ret_len = 0;
  *bdata = nullptr;

  const ProtobufCFieldDescriptor *bin_fd = nullptr;
  const ProtobufCFieldDescriptor *dom_fd = nullptr;
  protobuf_c_boolean is_dptr = false;
  void *field_ptr = nullptr;
  size_t offset = 0;

  size_t count = protobuf_c_message_get_field_desc_count_and_offset(&keyspec_->base,
      RW_SCHEMA_TAG_KEYSPEC_BINPATH, &bin_fd, &field_ptr, &offset, &is_dptr);

  RW_ASSERT(bin_fd);
  RW_ASSERT(bin_fd->type == PROTOBUF_C_TYPE_BYTES);
  RW_ASSERT(bin_fd->label != PROTOBUF_C_LABEL_REPEATED);

  if (!count) {

    size_t dcount = protobuf_c_message_get_field_desc_count_and_offset(&keyspec_->base,
        RW_SCHEMA_TAG_KEYSPEC_DOMPATH, &dom_fd, &field_ptr, &offset, &is_dptr);

    if (!dcount) {
      return ret_len;
    }

    RW_ASSERT(dom_fd->type == PROTOBUF_C_TYPE_MESSAGE);
    RW_ASSERT(dom_fd->label != PROTOBUF_C_LABEL_REPEATED);

    size_t dsize = protobuf_c_message_get_packed_size(nullptr, (ProtobufCMessage *)field_ptr);
    RW_ASSERT(dsize);

    ProtobufCFlatBinaryData *bd = (ProtobufCFlatBinaryData *)malloc(sizeof(ProtobufCFlatBinaryData)+dsize);
    bd->len = dsize;

    RW_ASSERT(protobuf_c_message_pack(nullptr, (ProtobufCMessage*)field_ptr, bd->data) == dsize);

    *bdata = bd->data;
    ret_len = bd->len;

    binpath_.reset(bd);

  } else {

    if (bin_fd->rw_flags & RW_PROTOBUF_FOPT_INLINE) {
      *bdata = ((ProtobufCFlatBinaryData *)field_ptr)->data;
      ret_len = ((ProtobufCFlatBinaryData *)field_ptr)->len;
    } else {
      *bdata = ((ProtobufCBinaryData *)field_ptr)->data;
      ret_len = ((ProtobufCBinaryData *)field_ptr)->len;
    }
  }

  return ret_len;
}

const char *
KeySpecHelper::get_strpath()
{
  const char *strpath = nullptr;
  protobuf_c_boolean is_dptr = false;
  const ProtobufCFieldDescriptor *str_fd = nullptr;
  void *field_ptr = nullptr;
  size_t offset = 0;

  size_t count = protobuf_c_message_get_field_desc_count_and_offset(
      &keyspec_->base, RW_SCHEMA_TAG_KEYSPEC_STRPATH, &str_fd, &field_ptr, &offset, &is_dptr);

  RW_ASSERT(str_fd);
  RW_ASSERT(str_fd->type == PROTOBUF_C_TYPE_STRING);
  RW_ASSERT(str_fd->label != PROTOBUF_C_LABEL_REPEATED);

  if (count) {
    //The above function always returns the pointer to the
    //actual string irrespective of whether the field is flat/pointy.
    strpath = (char *)field_ptr;
  }

  return strpath;
}

bool
KeySpecHelper::has_binpath()
{
  return protobuf_c_message_has_field(nullptr, &keyspec_->base, 
                                      RW_SCHEMA_TAG_KEYSPEC_BINPATH);
}

bool
KeySpecHelper::has_dompath()
{
  return protobuf_c_message_has_field(nullptr, &keyspec_->base, 
                                      RW_SCHEMA_TAG_KEYSPEC_DOMPATH);
}

bool
KeySpecHelper::has_strpath()
{
  return protobuf_c_message_has_field(nullptr, &keyspec_->base, 
                                      RW_SCHEMA_TAG_KEYSPEC_STRPATH);
}

RwSchemaCategory
KeySpecHelper::get_category()
{
  RwSchemaCategory category = RW_SCHEMA_CATEGORY_ANY;
  const ProtobufCMessage* dom_path = get_dompath();

  if (dom_path) {

    const ProtobufCFieldDescriptor *fd = nullptr;
    protobuf_c_boolean is_dptr = false;
    void *field_ptr = nullptr;
    size_t offset = 0;

    size_t count = protobuf_c_message_get_field_desc_count_and_offset(
        dom_path, RW_SCHEMA_TAG_KEYSPEC_CATEGORY, &fd, &field_ptr, &offset, &is_dptr);

    if (count) {
      RW_ASSERT(fd->type == PROTOBUF_C_TYPE_ENUM);
      RW_ASSERT(fd->label != PROTOBUF_C_LABEL_REPEATED);
      RW_ASSERT(&rw_schema_category__descriptor == fd->descriptor);

      category = *(RwSchemaCategory*)field_ptr;
    }
  }

  return category;
}

size_t
KeySpecHelper::get_depth()
{
  const ProtobufCMessage* dom_path = get_dompath();
  if (!dom_path) {
    return 0;
  }

  size_t i = 0;
  for (i = 0; i <= RW_SCHEMA_PB_MAX_PATH_ENTRIES; i++) {
    if (!protobuf_c_message_has_field(nullptr, dom_path, 
                                      RW_SCHEMA_TAG_PATH_ENTRY_START+i)) {
      break;
    }
  }

  return i;
}

const rw_keyspec_entry_t*
KeySpecHelper::get_path_entry(size_t index)
{
  const ProtobufCMessage* dom_path = get_dompath();

  if (!dom_path) {
    return nullptr;
  }

  const ProtobufCFieldDescriptor *fd = nullptr;
  void *field_ptr = nullptr;
  size_t offset = 0;
  protobuf_c_boolean is_dptr = false;

  size_t count = protobuf_c_message_get_field_desc_count_and_offset(
      dom_path, RW_SCHEMA_TAG_PATH_ENTRY_START+index, &fd, &field_ptr, &offset, &is_dptr);

  if (!count) {
    return nullptr;
  }

  RW_ASSERT(fd->type == PROTOBUF_C_TYPE_MESSAGE);
  RW_ASSERT(fd->label != PROTOBUF_C_LABEL_REPEATED);

  return (const rw_keyspec_entry_t *)field_ptr;
}

const rw_keyspec_entry_t*
KeySpecHelper::get_specific_path_entry(uint32_t tag)
{
  const ProtobufCMessage* dom_path = get_dompath();

  if (!dom_path) {
    return nullptr;
  }

  const ProtobufCFieldDescriptor *fd = nullptr;
  void *field_ptr = nullptr;
  size_t offset = 0;
  protobuf_c_boolean is_dptr = false;

  size_t count = protobuf_c_message_get_field_desc_count_and_offset(
      dom_path, tag, &fd, &field_ptr, &offset, &is_dptr);

  if (!count) {
    return nullptr;
  }

  RW_ASSERT(fd->type == PROTOBUF_C_TYPE_MESSAGE);
  RW_ASSERT(fd->label != PROTOBUF_C_LABEL_REPEATED);

  return (const rw_keyspec_entry_t *)field_ptr;
}

bool
KeySpecHelper::is_rooted()
{
  const ProtobufCMessage *dom_path = get_dompath();

  if (!dom_path) {
    return false;
  }

  size_t offset;
  protobuf_c_boolean is_dptr = FALSE;
  void *field_ptr = nullptr;
  const ProtobufCFieldDescriptor *fd = nullptr;

  size_t count = protobuf_c_message_get_field_desc_count_and_offset(dom_path,
      RW_SCHEMA_TAG_KEYSPEC_ROOTED, &fd, &field_ptr, &offset, &is_dptr);

  if (!count) {
    return false;
  }

  RW_ASSERT(fd->type == PROTOBUF_C_TYPE_BOOL);
  RW_ASSERT(fd->label != PROTOBUF_C_LABEL_REPEATED);

  if (*(protobuf_c_boolean *)field_ptr) {
    return true;
  }

  return false;
}

bool
KeySpecHelper::has_wildcards(size_t start, size_t depth)
{
  size_t depth_k = get_depth();

  if (!depth || depth > depth_k) depth = depth_k;

  for (unsigned i = start; i < depth; i++) {

    auto path_entry = get_path_entry(i);
    RW_ASSERT(path_entry);

    size_t n_keys = rw_keyspec_entry_num_keys(path_entry);

    for (unsigned j = 0; j < n_keys; j++) {

      if (!protobuf_c_message_has_field(nullptr, &path_entry->base, 
                                        RW_SCHEMA_TAG_ELEMENT_KEY_START+j)) {
        return true;
      }
    }
  }

  return false;
}

bool
KeySpecHelper::is_listy()
{
  size_t depth = get_depth();

  if (!depth) {
    return false;
  }

  auto path_entry = get_path_entry(depth-1);
  RW_ASSERT(path_entry);

  auto element = rw_keyspec_entry_get_element_id(path_entry);
  RW_ASSERT(element);

  bool ret_val = (element->element_type >= RW_SCHEMA_ELEMENT_TYPE_LEAF_LIST &&
                  element->element_type <= RW_SCHEMA_ELEMENT_TYPE_LISTY_9);

  return ret_val;
}

bool
KeySpecHelper::is_leafy()
{
  size_t depth = get_depth();

  if (!depth) {
    return false;
  }

  auto path_entry = get_path_entry(depth-1);
  RW_ASSERT(path_entry);

  auto element = rw_keyspec_entry_get_element_id(path_entry);
  RW_ASSERT(element);

  bool ret_val = (element->element_type == RW_SCHEMA_ELEMENT_TYPE_LEAF);

  return ret_val;
}

bool
KeySpecHelper::is_generic()
{
  return (((const ProtobufCMessage *)keyspec_)->descriptor == &rw_schema_path_spec__descriptor);
}

bool
KeySpecHelper::is_root()
{
  return (is_schema_root() || is_module_root());
}

bool
KeySpecHelper::is_module_root()
{
  const rw_keyspec_entry_t* pe = get_specific_path_entry(RW_SCHEMA_TAG_PATH_ENTRY_MODULE_ROOT);
  if (!pe) {
    return false;
  }

  auto ele_id = rw_keyspec_entry_get_element_id(pe);
  RW_ASSERT(ele_id->element_type == RW_SCHEMA_ELEMENT_TYPE_MODULE_ROOT);
  return true;
}

bool
KeySpecHelper::is_schema_root()
{
  const rw_keyspec_entry_t* pe = get_specific_path_entry(RW_SCHEMA_TAG_PATH_ENTRY_ROOT);
  if (!pe) {
    return false;
  }

  auto ele_id = rw_keyspec_entry_get_element_id(pe);
  RW_ASSERT(ele_id->element_type == RW_SCHEMA_ELEMENT_TYPE_ROOT);
  return true;
}

bool
KeySpecHelper::is_equal(KeySpecHelper& other)
{
  bool  retval = false;

  const uint8_t *data1 = nullptr; 
  size_t len1 = get_binpath(&data1);
  
  if (!data1) {
    return retval;
  }

  const uint8_t *data2 = nullptr;
  size_t len2 = other.get_binpath(&data2);

  if (!data2) {
    return retval;
  }

  if (len1 == len2) {
    if (memcmp(data1, data2, len1) == 0) {
      retval = true;
    }
  }

  return retval;
}

const ProtobufCFieldDescriptor*
KeySpecHelper::get_leaf_pb_desc()
{
  if (is_generic()){
    return NULL;
  }

  if (!is_leafy()) {
    return NULL;
  }

  size_t depth = get_depth();

  if (depth < 2) {
    return NULL;
  }

  auto leaf_entry = get_path_entry(depth-1);
  RW_ASSERT(leaf_entry);
  auto element_id = ((RwSchemaPathEntry*)leaf_entry)->element_id;
  RW_ASSERT(element_id);
  RW_ASSERT(element_id->element_tag);
  auto leaf_tag = element_id->element_tag;

  auto parent_entry = get_path_entry(depth-2);
  RW_ASSERT(parent_entry);
  if (!(parent_entry->base.descriptor->ypbc_mdesc &&
        parent_entry->base.descriptor->ypbc_mdesc->u)) {
    return NULL;
  }

  auto mdesc = parent_entry->base.descriptor->ypbc_mdesc->u->msg_msgdesc;
  RW_ASSERT(mdesc.num_fields);

  auto parent_tag = mdesc.pb_element_tag;

  if ((parent_tag > leaf_tag) || ((parent_tag+mdesc.num_fields) < leaf_tag)) {
    return NULL;
  }

  auto index = leaf_tag - parent_tag -1;
  const ProtobufCFieldDescriptor *fd = mdesc.ypbc_flddescs[index].pbc_fdesc;

  RW_ASSERT(fd);
  return fd;
}

/***************************************************
 * KS error handling
 * *************************************************/

void
keyspec_default_error_cb(rw_keyspec_instance_t* instance,
                         const char* errmsg)
{
  CircularErrorBuffer::get_instance().add_error(errmsg);
}

void keyspec_dump_error_records()
{
  time_t curr_time = time(NULL);
  CircularErrorBuffer& errorbuf = CircularErrorBuffer::get_instance();

  for (unsigned c = 0; c < errorbuf.get_capacity(); ++c) {
    const ErrRecord* r = errorbuf[c];
    if (r->time && r->time <= curr_time) {
      struct tm tm;
      localtime_r((const time_t*)(&r->time), &tm);
      char tbuf[64] = { 0 };
      strftime(tbuf, sizeof(tbuf), "%Y%m%d-%H%M%S", &tm);

      std::cerr << '[' << std::setfill('0') << std::setw(3) << c;
      std::cerr << "] " << tbuf << ": " << r->errmsg << std::endl;
    }
  }
}

void keyspec_clear_error_records()
{
  CircularErrorBuffer::get_instance().clear();
}

int keyspec_export_ebuf(rw_keyspec_instance_t* instance,
                        ProtobufCMessage* msg,
                        const char* m_rname,
                        const char* ts_fname,
                        const char* es_fname)
{
  RW_ASSERT(instance);

  /*
   * This is a kind of hack to export error records.
   * Assumptions about protobufc structure is made here.
   * Not worring as it is just a debug code. 
   */
  RW_ASSERT(msg);
  RW_ASSERT(m_rname);
  RW_ASSERT(ts_fname);
  RW_ASSERT(es_fname);

  const ProtobufCFieldDescriptor* mfdesc = 
      protobuf_c_message_descriptor_get_field_by_name(msg->descriptor, m_rname);
  
  if (!mfdesc) {
    return 0;
  }

  // Make sure to assert all the assumptions!
  RW_ASSERT(mfdesc->type == PROTOBUF_C_TYPE_MESSAGE);
  RW_ASSERT(mfdesc->label == PROTOBUF_C_LABEL_REPEATED);
  RW_ASSERT(!(mfdesc->rw_flags & RW_PROTOBUF_FOPT_INLINE));

  const ProtobufCFieldDescriptor* tsfdesc = 
      protobuf_c_message_descriptor_get_field_by_name(mfdesc->msg_desc, ts_fname);

  if (!tsfdesc) {
    return 0;
  }

  RW_ASSERT(tsfdesc->type == PROTOBUF_C_TYPE_STRING);
  RW_ASSERT(tsfdesc->label == PROTOBUF_C_LABEL_OPTIONAL);
  RW_ASSERT(tsfdesc->rw_flags & RW_PROTOBUF_FOPT_INLINE);

  const ProtobufCFieldDescriptor* esfdesc =
      protobuf_c_message_descriptor_get_field_by_name(mfdesc->msg_desc, es_fname);

  if (!esfdesc) {
    return 0;
  }

  RW_ASSERT(esfdesc->type == PROTOBUF_C_TYPE_STRING);
  RW_ASSERT(esfdesc->label == PROTOBUF_C_LABEL_OPTIONAL);
  RW_ASSERT(esfdesc->rw_flags & RW_PROTOBUF_FOPT_INLINE);

  // Make sure that the field is not allocated earlier.
  RW_ASSERT(!STRUCT_MEMBER(ProtobufCMessage**, msg, mfdesc->offset));
  RW_ASSERT(!STRUCT_MEMBER(uint32_t, msg, mfdesc->quantifier_offset));

  CircularErrorBuffer& errorbuf = CircularErrorBuffer::get_instance();

  ProtobufCMessage** records = (ProtobufCMessage **)protobuf_c_instance_alloc(
      instance->pbc_instance, errorbuf.get_capacity()*sizeof(void*));

  // Caution! dont call any function that may log error.
  time_t stime = time(NULL);
  unsigned tot_records = 0;

  /* 
   * Export the records in last-in-first out order so that the
   * recent errors are at the top.
   */
  size_t idx = errorbuf.get_last_index();
  size_t cap = errorbuf.get_capacity();

  for (size_t c = 0; c < cap; c++) {
    const ErrRecord* r = errorbuf[idx];
    if (!idx) { idx = (cap - 1); }
    else { idx--; }

    if (r->time <= stime && r->time ) {
      struct tm tm;
      localtime_r((const time_t*)(&r->time), &tm);

      ProtobufCMessage* record = (ProtobufCMessage *)protobuf_c_instance_alloc(
          instance->pbc_instance, mfdesc->msg_desc->sizeof_message);
      protobuf_c_message_init(mfdesc->msg_desc, record);

      char *tbuf = STRUCT_MEMBER_PTR(char, record, tsfdesc->offset);
      strftime(tbuf, tsfdesc->data_size, "%Y%m%d-%H%M%S", &tm);
      // Set the has field.
      STRUCT_MEMBER(uint32_t, record, tsfdesc->quantifier_offset) = 1;

      char *estr = STRUCT_MEMBER_PTR(char, record, esfdesc->offset);
      RW_ASSERT(esfdesc->data_size >= sizeof(r->errmsg));
      strncpy(estr, (const char*)r->errmsg, sizeof(r->errmsg)-1);
      estr[sizeof(r->errmsg)-1] = 0;
      // Set the has field.
      STRUCT_MEMBER(uint32_t, record, esfdesc->quantifier_offset) = 1;

      records[tot_records++] = record;
    }
  }

  if (tot_records) {
    STRUCT_MEMBER(ProtobufCMessage**, msg, mfdesc->offset) = records;
    STRUCT_MEMBER(uint32_t, msg, mfdesc->quantifier_offset) = tot_records;
  } else {
    protobuf_c_instance_free(instance->pbc_instance, records);
  }

  return tot_records;
}

/****************************************************
 * XpathIIParser
 * **************************************************/
XpathIIParser::XpathIIParser(const char* xpath, 
                             rw_xpath_type_t type)
: xpath_(xpath),
  xpath_type_(type)
{
  parse_ok_ = false;
  depth_ = 0;
  is_root_ = false;
}

// Yang identifiers should start with ALPA or _
#define IS_VALID_FNAME_CHAR(c) (isascii((c)) &&  \
                                (isalpha((c)) || (c) == '_'))

// Non-first valid yang identifier charaters.
#define IS_VALID_NAME_CHAR(c) (isascii((c)) && \
                              (isalnum((c)) || \
                              (c) == '_' || \
                              (c) == '-' || \
                              (c) == '.'))

#define ISWSP(c) ((c) == ' ' || (c) == '\t')

inline bool
XpathIIParser::parse_identifer(const char *str, 
                               unsigned *length)
{
  const char *cptr = str;
  unsigned len = 1;

  if (IS_VALID_FNAME_CHAR(*cptr)) {
    cptr++;
    while((*cptr) && IS_VALID_NAME_CHAR(*cptr)) {
      len++; cptr++;
    }
  }

  if (!len) {
    return false;
  }

  // A valid identifer must not start with 'XML/xml'
  if (len >= 3) {
    if ((str[0] == 'X' || str[0] == 'x') &&
        (str[1] == 'M' || str[1] == 'm') &&
        (str[2] == 'L' || str[2] == 'l')) {
      return false;
    }
  }

  *length = len;
  return true;
}

inline bool 
XpathIIParser::parse_qstring(const char* str, 
                             unsigned* length,
                             char q)
{
  unsigned len = 0;

  while ((*str) && (*str != q)) {
    len++; str++;
  }
  // Should end with same quote
  if (len && *str == q) {
    *length = len;
    return true;
  }
  return false;
}

inline bool 
XpathIIParser::parse_number(const char* str, 
                            unsigned* length)
{
  const char *num = str;
  unsigned len = 0;

  while ((*num) && isdigit(*num++)) {
    len++;
  }
  // If the number starts with zero, then it must be
  // the only digit.
  if (*str == '0' && len > 1) {
    return false;
  }

  *length = len;
  return true;
}

bool
XpathIIParser::validate_next_token(TokenType curr,
                                   TokenType next,
                                   bool predicate)
{
  bool retval = false;
  switch (curr) {
    case TokenType::TT_FSLASH:
      if (next == TokenType::TT_PREFIX ||
          next == TokenType::TT_IDENTIFIER) {
        retval = true;
      }
      break;
    case TokenType::TT_PREFIX:
      if (next == TokenType::TT_COLON) {
        retval = true;
      }
      break;
    case TokenType::TT_COLON:
      if (next == TokenType::TT_IDENTIFIER) {
        retval = true;
      }
      break;
    case TokenType::TT_IDENTIFIER:
      if (predicate) {
        if (next == TokenType::TT_EQUAL) {
          return true;
        }
      } else {
        if (next == TokenType::TT_FSLASH ||
            next == TokenType::TT_LBRACK ||
            next == TokenType::TT_NONE) {
          retval = true;
        }
      }
      break;
    case TokenType::TT_LBRACK:
      if (next == TokenType::TT_PREFIX ||
          next == TokenType::TT_IDENTIFIER ||
          next == TokenType::TT_DOT ||
          next == TokenType::TT_NUMBER) {
        retval = true;
      }
      break;
    case TokenType::TT_EQUAL:
      if (next == TokenType::TT_STRING) {
        retval = true;
      }
      break;
    case TokenType::TT_DOT:
      if (next == TokenType::TT_EQUAL) {
        retval = true;
      }
      break;
    case TokenType::TT_NUMBER:
    case TokenType::TT_STRING:
      if (next == TokenType::TT_RBRACK) {
        retval = true;
      }
      break;
    case TokenType::TT_RBRACK:
      if (next == TokenType::TT_LBRACK ||
          next == TokenType::TT_FSLASH ||
          next == TokenType::TT_NONE) {
        retval = true;
      }
      break;
    case TokenType::TT_NONE:
      if (next == TokenType::TT_FSLASH) {
        retval = true;
      }
      break;
  }
  return retval;
}

bool 
XpathIIParser::parse_xpath()
{
  const char* str = xpath_;

  if (!*str || *str++ != '/') {
    // empty string or not abosulte path.
    return parse_ok_;
  }

  TokenType prev_tok = TokenType::TT_NONE;
  TokenType curr_tok = TokenType::TT_FSLASH;

  tokens_.emplace_back(curr_tok, std::string(1, '/'));
  bool predicate = false;
  parse_ok_ = true;

  if (!*str) {
    // Root xpath??
    is_root_ = true;
    return parse_ok_;
  }

  while ((*str) && (parse_ok_)) {

    // skip white spaces
    if (ISWSP(*str)) {
      str++; continue;
    }

    prev_tok = curr_tok;
    curr_tok = TokenType::TT_NONE;

    if (IS_VALID_FNAME_CHAR(*str)) {

      // Start of an identifier
      unsigned len = 0;
      parse_ok_ = parse_identifer(str, &len);
      if (parse_ok_) {
        if (*(str+len) == ':') {
          curr_tok = TokenType::TT_PREFIX;
        } else {
          curr_tok = TokenType::TT_IDENTIFIER;
        }
        tokens_.emplace_back(curr_tok, std::string(str, len));
        str += len;
      }
      
    } else if (isdigit(*str)) {

      // Start of a number
      unsigned len = 0;
      parse_ok_ = parse_number(str, &len);
      if (parse_ok_) {
        curr_tok = TokenType::TT_NUMBER;
        tokens_.emplace_back(curr_tok, std::string(str, len));
        str += len;
      }

    } else if (*str == '\'' || *str == '\"') {

      // Start of a single/double quoted string
      unsigned len = 0;
      char q = *str++;
      parse_ok_ = parse_qstring(str, &len, q);
      if (parse_ok_) {
        curr_tok = TokenType::TT_STRING;
        tokens_.emplace_back(curr_tok, std::string(str, len));
        str += len+1;
      }

    } else {

      switch(*str) {
        case ':':
          curr_tok = TokenType::TT_COLON;
          break;
        case '/':
          curr_tok = TokenType::TT_FSLASH;
          break;
        case '[':
          curr_tok = TokenType::TT_LBRACK;
          break;
        case ']':
          curr_tok = TokenType::TT_RBRACK;
          break;
        case '.':
          curr_tok = TokenType::TT_DOT;
          break;
        case '=':
          curr_tok = TokenType::TT_EQUAL;
          break;
        default:  // parse error
          parse_ok_ = false;
          break;
      }
      if (parse_ok_) {
        tokens_.emplace_back(curr_tok, std::string(1, *str++));
      }
    }

    if (parse_ok_) {
      parse_ok_ = validate_next_token(prev_tok, curr_tok, predicate);

      if (curr_tok == TokenType::TT_LBRACK) {
        predicate = true;
      } else if (curr_tok == TokenType::TT_RBRACK) {
        predicate = false;
      }
    }
  }

  if (parse_ok_) {
    // Make sure the last token is a valid end token.
    parse_ok_ = validate_next_token(curr_tok, TokenType::TT_NONE, predicate);
  }

  return parse_ok_;
}

bool
XpathIIParser::verify_and_add_keys(const rw_yang_pb_msgdesc_t* mdesc,
                                   size_t index,
                                   predicates_t& keys)
{
  if (!mdesc) {
    return true;
  }

  if (mdesc->msg_type == RW_YANGPBC_MSG_TYPE_LEAF_LIST ||
      mdesc->msg_type == RW_YANGPBC_MSG_TYPE_LIST) {

    if (xpath_type_ == RW_XPATH_INSTANCEID) {
      unsigned num_keys = rw_keyspec_entry_num_keys(mdesc->schema_entry_value);
      if (num_keys != keys.size()) {
        return false;
      }
    }

    if (keys.size()) {
      // insert the keys list in the map.
      keys_map_.emplace(index, keys);
    }
  }

  keys.clear();
  return true;
}

const rw_yang_pb_msgdesc_t*
XpathIIParser::validate_xpath(const rw_yang_pb_schema_t* schema,
                              RwSchemaCategory category)
{
  if (!parse_ok_) {
    return nullptr;
  }

  unsigned num_tokens = tokens_.size();
  RW_ASSERT(num_tokens);

  const rw_yang_pb_msgdesc_t *cnode_mdesc = nullptr;
  bool ok = true;

  if (is_root_) {
    if (category != RW_SCHEMA_CATEGORY_DATA &&
        category != RW_SCHEMA_CATEGORY_CONFIG &&
        category != RW_SCHEMA_CATEGORY_ANY) {
      return nullptr;
    }
    cnode_mdesc = schema->data_root;
    return cnode_mdesc;
  }

  std::string prefix, node;
  bool predicate = false;
  predicates_t keys;

  for (unsigned i = 0; i < num_tokens && (ok); i++) {

    switch (tokens_[i].first) {

      case TokenType::TT_PREFIX: {
        prefix = tokens_[i].second;
        break;
      }

      case TokenType::TT_IDENTIFIER: {
        node = tokens_[i].second;
        if (predicate) {
          RW_ASSERT(cnode_mdesc);
          ok = validate_key_leaf(cnode_mdesc, prefix, node);
        } else {
          depth_++;
          if (!cnode_mdesc) {
            cnode_mdesc = find_schema_top_node(schema, category, prefix, node);
          } else {
            cnode_mdesc = find_msg_child_node(cnode_mdesc, prefix, node);
          }
          if (!cnode_mdesc) {
            ok = false;
          }
        }
        break;
      }

      case TokenType::TT_LBRACK: {
        node.clear(); prefix.clear();
        predicate = true;
        break;
      }

      case TokenType::TT_FSLASH: {
        // Start of next path entry.
        node.clear(); prefix.clear();
        ok = verify_and_add_keys(cnode_mdesc, depth_-1, keys);
        break;
      }

      case TokenType::TT_DOT: {
        node = ".";
        // fall through
      }
      case TokenType::TT_COLON:
      case TokenType::TT_EQUAL: {
        // do nothing
        break;
      }

      case TokenType::TT_RBRACK: {
        predicate = false;
        break;
      }

      case TokenType::TT_STRING: {
        if (keys.find(node) != keys.end()) {
          ok = false; // Multiple predicates for the same key?
        } else {
          keys.emplace(node, tokens_[i].second);
        }
        break;
      }

      case TokenType::TT_NUMBER: {
        // Make sure that the node type is a keyless list
        RW_ASSERT(cnode_mdesc);
        auto elem = rw_keyspec_entry_get_element_id(
            cnode_mdesc->schema_entry_value);
        ok = (elem->element_type == RW_SCHEMA_ELEMENT_TYPE_LISTY_0);
        if (ok) {
          pos_map_.emplace(depth_, std::stoi(tokens_[i].second));
        }
        break;
      }

      default:
        ok = false;
        break;
    }
  }

  if (ok && !is_leaf_) {
    ok = verify_and_add_keys(cnode_mdesc, depth_-1, keys);
  }

  if (!ok) {
    return nullptr;
  }

  return cnode_mdesc;
}

const rw_yang_pb_msgdesc_t*
XpathIIParser::find_schema_top_node(const rw_yang_pb_schema_t* schema,
                                    RwSchemaCategory category,
                                    const std::string& prefix,
                                    const std::string& node)
{
  RW_ASSERT(schema);
  const rw_yang_pb_msgdesc_t* mdesc = nullptr;

  if (!schema->module_count) {
    return mdesc;
  }

  if (!node.length()) {
    return mdesc;
  }

  unsigned m = 0;
  if (prefix.length()) {
    for (m = 0; m < schema->module_count; m++) {
      if (prefix == schema->modules[m]->prefix) {
        break;
      }
    }

    if (m == schema->module_count) {
      return mdesc;
    }
  }

  do {

    auto ymodule = schema->modules[m++];

    switch (category) {
      case RW_SCHEMA_CATEGORY_ANY:
      case RW_SCHEMA_CATEGORY_CONFIG:
      case RW_SCHEMA_CATEGORY_DATA: {
        auto data_module = ymodule->data_module;
        if (data_module) {
          for (unsigned i = 0; i < data_module->child_msg_count; i++) {
            auto top_msg = data_module->child_msgs[i];
            RW_ASSERT(top_msg);
            if (node == top_msg->yang_node_name) {
              mdesc = top_msg;
              break;
            }
          }
        }
        break;
      }
      default:
        break;
    }

    if (mdesc) {
      break;
    }

    switch (category) {
      case RW_SCHEMA_CATEGORY_ANY:
      case RW_SCHEMA_CATEGORY_RPC_INPUT: {
        auto rpci_module = ymodule->rpci_module;
        if (rpci_module) {
          for (unsigned i = 0; i < rpci_module->child_msg_count; i++) {
            auto top_msg = rpci_module->child_msgs[i];
            RW_ASSERT(top_msg);
            if (node == top_msg->yang_node_name) {
              mdesc = top_msg;
              break;
            }
          }
        }
        break;
      }
      default:
        break;
    }

    if (mdesc) {
      break;
    }

    switch (category) {
      case RW_SCHEMA_CATEGORY_ANY:
      case RW_SCHEMA_CATEGORY_RPC_OUTPUT: {
        auto rpco_module = ymodule->rpco_module;
        if (rpco_module) {
          for (unsigned i = 0; i < rpco_module->child_msg_count; i++) {
            auto top_msg = rpco_module->child_msgs[i];
            RW_ASSERT(top_msg);
            if (node == top_msg->yang_node_name) {
              mdesc = top_msg;
              break;
            }
          }
        }
        break;
      }
      default:
        break;
    }

    if (mdesc) {
      break;
    }

    switch (category) {
      case RW_SCHEMA_CATEGORY_ANY:
      case RW_SCHEMA_CATEGORY_NOTIFICATION: {
        auto notif_module = ymodule->notif_module;
        if (notif_module) {
          for (unsigned i = 0; i < notif_module->child_msg_count; i++) {
            auto top_msg = notif_module->child_msgs[i];
            RW_ASSERT(top_msg);
            if (node == top_msg->yang_node_name) {
              mdesc = top_msg;
              break;
            }
          }
        }
        break;
      }
      default:
        break;
    }

    if (mdesc || prefix.length()) {
      break;
    }

  } while (m < schema->module_count);

  return mdesc;
}

const rw_yang_pb_msgdesc_t*
XpathIIParser::find_msg_child_node(const rw_yang_pb_msgdesc_t* parent,
                                   const std::string& prefix,
                                   const std::string& node)
{
  RW_ASSERT(parent);
  const rw_yang_pb_msgdesc_t* child = nullptr;

  if (!node.length()) {
    return nullptr;
  }

  for (unsigned i = 0; i < parent->child_msg_count; i++) {
    auto child_msg = parent->child_msgs[i];
    if (node == child_msg->yang_node_name) {
      if (prefix.length()) {
        if (prefix == child_msg->module->prefix) {
          child = child_msg;
        }
      } else {
        child = child_msg;
      }
      break;
    }
  }

  if (!child) {
    // Maybe a leaf node??
    auto ypbc_fdesc = parent->ypbc_flddescs;
    for (unsigned i = 0; i < parent->num_fields; i++) {
      if (node == ypbc_fdesc[i].yang_node_name) {
        if (!prefix.length() || (prefix == ypbc_fdesc[i].module->prefix)) {
          child = parent;
          is_leaf_ = true;
        }
        break;
      }
    }
  }

  return child;
}

bool
XpathIIParser::validate_key_leaf(const rw_yang_pb_msgdesc_t* parent,
                                 const std::string& prefix,
                                 const std::string& node)
{
  RW_ASSERT(parent);

  auto ypbc_fdesc = parent->ypbc_flddescs;
  for (unsigned i = 0; i < parent->num_fields; i++) {
    if ((node == ypbc_fdesc[i].yang_node_name) &&
        (ypbc_fdesc[i].pbc_fdesc->rw_flags & RW_PROTOBUF_FOPT_KEY)) {
      return true;
    }
  }

  return false;
}

rw_status_t
XpathIIParser::populate_keys(rw_keyspec_instance_t* instance,
                             rw_keyspec_entry_t* entry,
                             size_t index)
{
  rw_status_t s = RW_STATUS_SUCCESS;

  auto iter = keys_map_.find(index);
  if (iter == keys_map_.cend()) {
    if (xpath_type_ == RW_XPATH_INSTANCEID) {
      return RW_STATUS_FAILURE;
    }
    return s; // All keys are wildcarded
  }

  auto keys = iter->second;
  for (auto it = keys.cbegin(); it != keys.cend(); ++it) {

    const std::string& key_node = it->first;
    const std::string& key_value = it->second;

    const ProtobufCFieldDescriptor* km_fdesc = nullptr;
    const ProtobufCFieldDescriptor* k_fdesc = nullptr;

    s = rw_keyspec_path_key_descriptors_by_yang_name(
        entry, key_node.c_str(), NULL, &km_fdesc, &k_fdesc);

    if (s != RW_STATUS_SUCCESS) {
      break;
    }

    ProtobufCMessage* key_msg = nullptr;
    protobuf_c_boolean ok = protobuf_c_message_set_field_message(
        instance->pbc_instance, &entry->base, km_fdesc, &key_msg);

    RW_ASSERT(ok);
    RW_ASSERT(key_msg);

    ok = protobuf_c_message_set_field_text_value(
        instance->pbc_instance, key_msg, k_fdesc, key_value.c_str(), key_value.length());

    if (!ok) {
      s = RW_STATUS_FAILURE;
      break;
    }
  }

  return s;
}

rw_status_t
XpathIIParser::populate_keys(rw_keyspec_instance_t* instance,
                             ProtobufCMessage* msg,
                             size_t index)
{
  RW_ASSERT(msg);
  rw_status_t s = RW_STATUS_SUCCESS;

  auto iter = keys_map_.find(index);
  if (iter == keys_map_.cend()) {
    return s; // All keys are wildcarded
  }

  auto keys = iter->second;
  for (auto it = keys.cbegin(); it != keys.cend(); ++it) {

    const std::string& key_node = it->first;
    const std::string& key_value = it->second;

    auto k_fdesc = protobuf_c_message_descriptor_get_field_by_name(
        msg->descriptor, key_node.c_str());
    RW_ASSERT(k_fdesc);

    protobuf_c_boolean ok = protobuf_c_message_set_field_text_value(
        instance->pbc_instance, msg, k_fdesc, key_value.c_str(), key_value.length());

    if (!ok) {
      s = RW_STATUS_FAILURE;
      break;
    }
  }

  return s;
}
