
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
 * @file rw_ks_util.hpp
 * @author Sujithra Periasamy
 * @date 2015/06/17
 * @brief helper classes or utilities for keyspec
 */

#ifndef RW_KS_UTIL_HPP_
#define RW_KS_UTIL_HPP_

#if __cplusplus < 201103L
#error "Requires C++11"
#endif

#include <rw_keyspec.h>

/**
 * A keyspec helper class that take a const keyspec and
 * provides member functions to access  various fields in the
 * keyspec. Dompath/Binpath are created and owned by the helper class
 * if the keyspec doesn't have it.
 */
class KeySpecHelper
{
 public:
  /** constructor */
  KeySpecHelper(const rw_keyspec_path_t *ks);

  /** Cannot copy. */
  KeySpecHelper(const KeySpecHelper&) = delete;

  /** No assignment */
  KeySpecHelper& operator=(const KeySpecHelper&) = delete;

  /** Get the pointer to dompath. */
  const ProtobufCMessage* get_dompath();

  /** Get the category of the keyspec. */
  RwSchemaCategory get_category();

  /** Get the path-entry at index */
  const rw_keyspec_entry_t* get_path_entry(size_t index);

  /** Get a path-entry given its tag */
  const rw_keyspec_entry_t* get_specific_path_entry(uint32_t tag);

  /** Get rooted flag of the keyspec */
  bool is_rooted();

  /** Get the depth of the keyspc */
  size_t get_depth();

  /** Retunrs the const keyspec the helper holds */
  const rw_keyspec_path_t* get() const;

  /** Change the keyspec that it holds */
  void set(const rw_keyspec_path_t *);

  /** Get the binpath of the keyspec. */
  size_t get_binpath(const uint8_t** data);

  /** Get strpath of the keyspec. */
  const char* get_strpath();

  /** Does keyspec have the binpath set? */
  bool has_binpath();

  /** Does keyspec have the dompath set? */
  bool has_dompath();

  /** Does keyspec have the strpath set? */
  bool has_strpath();

  /** Does keyspec have wildcard for the path-entries? */
  bool has_wildcards(size_t start = 0, size_t depth = 0);

  /** Does the keyspec specify a listy node? */
  bool is_listy();

  /** Does the keyspec specifies a leaf? */
  bool is_leafy();

  /** Is the Keypsec generic? */
  bool is_generic();

  /** Is this a root keyspec? */
  bool is_root();

  /** Is this a module root keyspec? */
  bool is_module_root();

  /** Is this a schema root keyspec? */
  bool is_schema_root();

  /** The binpaths of the keyspec's equal? */
  bool is_equal(KeySpecHelper &other);

  /** Get leaf type */
  const ProtobufCFieldDescriptor* get_leaf_pb_desc();

 private:
  /** The const keyspec the helper holds */
  const rw_keyspec_path_t *keyspec_;

  /** Unique ptr to the dompath created locally */
  UniquePtrProtobufCMessage<>::uptr_t dompath_;

  /** Unique ptr to the binpath created locally */
  UniquePtrBinaryData::uptr_t binpath_;
};

// Keyspec error handling..

#define BUFFER_SIZE (16 * 1024)
#define RECORD_SIZE 256
#define ERROR_MSG_LEN (RECORD_SIZE - sizeof(time_t))

struct ErrRecord
{
  time_t time;
  char errmsg[ERROR_MSG_LEN];
};

class CircularErrorBuffer
{
 private:

  CircularErrorBuffer(size_t size) : size_(size)
  {
    buffer_ = (char *)RW_MALLOC0(size_);
    RW_ASSERT(buffer_);
    capacity_ = size_ / RECORD_SIZE;
    next_index_ = 0;
  }

 public:
  static CircularErrorBuffer& get_instance()
  {
    static CircularErrorBuffer ebuf(BUFFER_SIZE);
    return ebuf;
  }

  /* Cannot copy and assign */
  CircularErrorBuffer(const CircularErrorBuffer&) = delete;
  CircularErrorBuffer& operator =(const CircularErrorBuffer&) = delete;

  void add_error(const char* errmsg)
  {
    auto record = (ErrRecord *)(buffer_ + (next_index_ * RECORD_SIZE));
    record->time = time(NULL);
    strncpy(record->errmsg, errmsg, ERROR_MSG_LEN-1);
    next_index_ = (next_index_ + 1) % capacity_;
  }

  void clear()
  {
    memset(buffer_, 0, size_);
    next_index_ = 0;
  }

  const ErrRecord* operator[] (size_t index) const
  {
    if (index > capacity_ - 1) {
      index = 0;
    }
    size_t offset = index * RECORD_SIZE;
    return (ErrRecord *)(buffer_ + offset);
  }

  ErrRecord* operator[] (size_t index)
  {
    RW_ASSERT(index <= capacity_ - 1);
    size_t offset = index * RECORD_SIZE;
    return (ErrRecord *)(buffer_ + offset);
  }

  size_t get_capacity() const
  {
    return capacity_;
  }

  size_t get_last_index() const
  {
     return (next_index_) ? (next_index_ - 1): (capacity_ - 1);
  }

 private:
  char *buffer_;
  const size_t size_;
  size_t capacity_;
  size_t next_index_;
};

/*!
 * Get a pointer to the element id in the path entry.
 *
 * @param[in] The path entry. Must be valid.
 *
 * @return The Pointer to the element id in the path entry.
 *         NULL if the protomsg is not of path entry type.
 */
const
RwSchemaElementId* rw_keyspec_entry_get_element_id(
    const rw_keyspec_entry_t *path_entry);

/*!
 * Given a msg and a keyspec deeper, get the msg pointer
 * corresponding to the keyspec.
 * 
 * @param[in] instance The keyspec instance pointer
 * @param[in] depth_i  The depth of the input message.
 * @param[in] in_msg   The input message.
 * @param[in] ks_o     The out ks helper.
 * @param[in] depth_o  The depth of the outks
 * 
 * @return The pointer to the msg corresponding to the ks.
 * NULL if there is an error.
 */
const ProtobufCMessage* keyspec_find_deeper(
    rw_keyspec_instance_t* instance,
    size_t depth_i,
    const ProtobufCMessage *in_msg,
    KeySpecHelper& ks_o,
    size_t depth_o);

/*!
 * The keyspec default error handling function.
 *
 * @param[in] instance, the keyspec instance pointer.
 * @param[in] error, the error string reported by the API call.
 */
void keyspec_default_error_cb(rw_keyspec_instance_t* instance, const char *error);


int keyspec_export_ebuf(rw_keyspec_instance_t* instance,
                        ProtobufCMessage* msg,
                        const char* m_rname,
                        const char* ts_fname,
                        const char* es_fname);

/*!
 * Parser for the instance-identifier ABNF defined in
 * RFC 6020, page 159.
 */
class XpathIIParser
{
 public:

  /** Valid tokens that can appear in instance identifer */
  enum class TokenType
  {
    TT_NONE,        /** Invalid value */
    TT_FSLASH,      /** Front slash "/" */
    TT_PREFIX,      /** Prefix identifier */
    TT_COLON,       /** Colon */
    TT_IDENTIFIER,  /** Node identifier */
    TT_STRING,      /** Predicate string */
    TT_NUMBER,      /** Index number for keyless lists */
    TT_LBRACK,      /** Left bracket "[" */
    TT_RBRACK,      /** Right bracket "]" */
    TT_DOT,         /** Dot "." */
    TT_EQUAL        /** Equal "=" */
  };

  // typedefs.
  typedef std::pair<TokenType, std::string> token_t;
  typedef std::vector<token_t> token_list_t;

  typedef std::unordered_map<std::string, std::string> predicates_t;
  typedef std::unordered_map<int, predicates_t> predicates_map_t;

 public:

  /** Constructor, initialized with xpath string to be parsed */
  XpathIIParser(const char* xpath, rw_xpath_type_t type = RW_XPATH_KEYSPEC);

  /** Cannot copy. */
  XpathIIParser(const XpathIIParser&) = delete;

  /** No assignment */
  XpathIIParser& operator=(const XpathIIParser&) = delete;

  /** Parse the xpath instance identifier */
  bool parse_xpath();

  /** Validate the parsed xpath with the schema provided */
  const rw_yang_pb_msgdesc_t* validate_xpath(const rw_yang_pb_schema_t* schema,
                                             RwSchemaCategory category);

  /** Populate key values to the path entry */
  rw_status_t populate_keys(rw_keyspec_instance_t* instance,
                            rw_keyspec_entry_t* entry, size_t index);

  /** Populate key values to the message */
  rw_status_t populate_keys(rw_keyspec_instance_t* instance,
                            ProtobufCMessage* msg,
                            size_t index);

  /** Get the depth of the parsed xpath */
  size_t get_depth() const { return depth_; }

  /** Is the xpath for a leaf node? */
  bool is_leaf() const { return is_leaf_; }

  /** Get the last token */
  const std::string& get_leaf_token() const { return tokens_.back().second; }

 private:

  /* Private static helper member functions */

  /** Parse the sequence identified as yang node identifier */
  static bool parse_identifer(const char* str, unsigned* length);

  /** Parse the sequence identified as quoted string (single/double) */
  static bool parse_qstring(const char* str, unsigned* length, char q);

  /** Parse the sequence identified as non-negative number */
  static bool parse_number(const char* str, unsigned* length);

  /** Validate the next token */
  static bool validate_next_token(TokenType curr, TokenType next, bool predicate);

  /** Find child msg node */
  const rw_yang_pb_msgdesc_t* find_msg_child_node(
      const rw_yang_pb_msgdesc_t* parent,
      const std::string& prefix,
      const std::string& node);

  /** Find the schema top node */
  const rw_yang_pb_msgdesc_t* find_schema_top_node(
      const rw_yang_pb_schema_t* schema,
      RwSchemaCategory category,
      const std::string& prefix,
      const std::string& node);

  /** Validate the key node */
  static bool validate_key_leaf(const rw_yang_pb_msgdesc_t* parent,
                                const std::string& prefix,
                                const std::string& node);

 private:

  /* private member functions. */

  /** Verifies that all te keys are specified for II*/
  bool verify_and_add_keys(const rw_yang_pb_msgdesc_t* mdesc,
                           size_t index,
                           predicates_t& keys);

 private:

  /** The input xpath instance identifier string */
  const char* xpath_;

  /** How to interpret the xpath string? */
  rw_xpath_type_t xpath_type_;

  /** The depth of the xpath */
  size_t depth_ = 0;

  /** vector to store the parsed tokens */
  token_list_t tokens_;

  /** Flag to indicate whether the parsing was successful */
  bool parse_ok_ = false;

  /** Flag to indicate whether the xpath indicates root */
  bool is_root_ = false;

  /** Flag to indicate whether the xpath is for leaf */
  bool is_leaf_ = false;

  /** Map to store the predicates */
  predicates_map_t keys_map_;

  typedef std::map<int, int> pos_map_t;

  /** Map to store the positional index for keyless lists */
  pos_map_t pos_map_;
};

#endif //  RW_KS_UTIL_HPP_
