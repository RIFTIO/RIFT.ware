/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file yangpbc.hpp
 *
 * Top level include for yangpbc utility
 */

#ifndef RW_YANGTOOLS_YANGPBC_HPP__
#define RW_YANGTOOLS_YANGPBC_HPP__

#include "yangncx.hpp"

#include <iosfwd>
#include <list>
#include <set>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <rw_namespace.h>

#define EVENT_ID_LEAF_NAME "event-id"
namespace rw_yang {

class PbEnumValue;
class PbEnumType;
class PbField;
class PbMessage;
class PbNode;
class PbModule;
class PbModel;
class PbEqMsgSet;
class PbEqMgr;
class PbModelVisitor;

/**
 * @defgroup yangpbc A tool to build protobuf from YANG
 * @{
 */

/// Parser control.  Parser functions return these values to indicate
/// how to continue processing.
enum class parser_control_t {
  ILLEGAL = 0,

  DO_REDO, ///< Redo the current parsing step
  DO_NEXT, ///< Go to the next parsing step
  DO_SKIP_TO_SIBLING, ///< Skip this yang subtree
};

enum class doc_file_t {
  TEXT = 0,
  HTML = 1,
};

/**
 * Define special message types.  The message types determine how the
 * proto message, proto field, ypbc descriptors, and other meta-data
 * get generated, independently of other aspects of the message (such
 * as yang node type).
 */
enum class PbMsgType
{
  illegal_ = 0,       ///< none/illegal value
  unset,              ///< default value, has not been set
  automatic,          ///< choose the correct type automatically, based on yang type

  /*** ORDER IS IMPORTANT ***/
  package,            ///< package                        # currently no PbMessage.
  top_root,           ///< package.YangMsgNew                # currently no PbMessage.
  data_root,          ///< package.YangData
  data_module,        ///< package.YangData.MODULE
  message,            ///< descendant of data_module
  rpci_root,          ///< package.YangInput           # ATTN: no PbMessage.
  rpci_module,        ///< package.YangInput.MODULE
  rpc_input,          ///< package.YangInput.MODULE.RPC
  rpco_root,          ///< package.YangOutput          # ATTN: no PbMessage.
  rpco_module,        ///< package.YangOutput.MODULE
  rpc_output,         ///< package.YangOutput.MODULE.RPC
  notif_root,         ///< package.YangNotif              # ATTN: no PbMessage.
  notif_module,       ///< package.YangNotif.MODULE
  notif,              ///< package.YangNotif.MODULE.NOTIF
  group_root,         ///< package.YangGroup
  group,              ///< package.YangGroup.GROUP

  /*
   * ATTN: grouping is temporarily ordered after everything else.  This
   * has the effect of forcing grouping instantiation to occur only
   * rarely, which is considered desireable only from the point of view
   * of rw_xml/yangmodel.  XML conversion currently requires a rooted
   * path in order to find the yang metadata.  This requirement will
   * eventually change, at which point groupings will move up, after
   * top-level, but before data, rpc, and notif.
   */

  // Last, for error detection
  last_
};

static inline bool is_in_range(PbMsgType v) {
  return v >= PbMsgType::illegal_ && v <= PbMsgType::last_;
}

static inline bool is_good(PbMsgType v) {
  return v > PbMsgType::automatic && v < PbMsgType::last_;
}


/**
 * Select the protobuf field type.
 */
enum class PbFieldType
{
  illegal_ = 0, ///< none/illegal value
  unset, ///< has not been determined yet
  automatic, ///< Automatically determine the field type

  pbdouble, ///< double
  pbfloat, ///< float
  pbint32, ///< int32
  pbuint32, ///< uint32
  pbsint32, ///< sint32
  pbfixed32, ///< fixed32
  pbsfixed32, ///< sfixed32
  pbint64, ///< int64
  pbuint64, ///< uint64
  pbsint64, ///< sint64
  pbfixed64, ///< fixed64
  pbsfixed64, ///< sfixed64
  pbbool, ///< bool
  pbbytes, ///< bytes
  pbstring, ///< string

  hack_bits, ///< Hack for yang bits. ATTN: Should convert to bytes.
  hack_union, ///< Hack for yang union. ATTN: Should convert to a message.

  pbenum, ///< enumeration
  pbmessage, ///< The field is a message
};


/// Enum casting hash function.
template <class T>
struct enum_hash {
  size_t operator()(const T& k) const;
};

/// Enum-pair casting hash function.
template <class P>
struct enum_pair_hash {
  size_t operator()(const P& k) const;
};

/// Pair hash function
template <class P, class A=typename P::first_type, class B=typename P::first_type>
struct pair_hash {
  size_t operator()(const P& k) const;
};


/**
 * A symbol table.  Maps a string to a pointer type, detects
 * collisions, and provides a facility for generating unique names.
 */
template <typename value_type>
class PbSymTab
{
public:
  // Utility types.

  /// The type of symbol table entry
  typedef value_type value_t;

  /// The symbol table.
  typedef std::unordered_map<std::string,value_t> table_t;

  /// A const table iterator.
  typedef typename table_t::const_iterator citer_t;

public:
  // Uses all default constructors/destructors

public:
  /**
   * Check if a symbol exists, and if not, add it to the table.
   *
   * @return nullptr if the symbol was added successfully.  Otherwise,
   *   a pointer to the previously added symbol.
   */
  value_t add_symbol(
    /// The identifier name
    std::string symbol,
    /// The value
    value_t value
  );

  /**
   * Check if a symbol exists in the table, and if not, add it to the
   * table.  If it already exists in the table, append a numeric suffix
   * until the generated name is unique, and then add it to the table.
   *
   * @return The empty string if no unique name can be generated.
   *   Otherwise, the unique name (possibly equal to the original)
   */
  std::string add_symbol_make_unique(
    /// The identifier name
    std::string symbol,
    /// The value
    value_t value
  );

  /**
   * Add a symbol to the table, replacing any previous value.
   *
   * @return The previous value, if any.
   */
  value_t add_symbol_replace(
    /// The identifier name
    std::string symbol,
    /// The value
    value_t value
  );

  /**
   * Check if a symbol exists in the table.
   */
  bool has_symbol(
    /// The identifier name
    std::string symbol
  ) const;

  /**
   * Check if a symbol exists in the table.
   *
   * @return nullptr if the symbol does not exist.
   */
  value_t get_symbol(
    /// The identifier name
    std::string symbol
  ) const;

  /**
   * Delete a symbol.
   *
   * @return false if the symbol did not exist.
   */
  bool delete_symbol(
    /// The identifier name
    std::string symbol
  );

  /**
   * Get a begin iterator.
   */
  citer_t begin() const
  {
    return table_.begin();
  }

  /**
   * Get an end iterator.
   */
  citer_t end() const
  {
    return table_.end();
  }


private:
  /// The symbnol table
  table_t table_;
};


/**
 * An enum value.  One of these objects gets created for every yang
 * enum enumeration identifier.  These objects are owned by the enum
 * types.
 */
class PbEnumValue
{
public:
  // Utility types.

  /// unique pointer for memory management.
  typedef std::unique_ptr<PbEnumValue> enum_value_uptr_t;

  /**
   * For storing sets of enum values.  Enum types will use this to
   * store the values in the order they are defined by yang.
   */
  typedef std::list<enum_value_uptr_t> enum_value_list_t;

  /// Symbol table for value constants.
  typedef PbSymTab<PbEnumValue*> enum_value_stab_t;

  /// Ordered names
  typedef std::map<std::string,PbEnumValue*> enum_value_sorted_t;

  /// For looking up integer values, for alias detection
  typedef std::unordered_set<int> enum_value_int_set_t;

public:
  /// Constructor
  PbEnumValue(
    /// The owning model.
    PbEnumType* pbenum,
    /// The yang value
    YangValue* yvalue
  );

  // Cannot copy
  PbEnumValue(const PbEnumValue&) = delete;
  PbEnumValue& operator=(const PbEnumValue&) = delete;

public:
  // Accessors

  PbModel* get_pbmodel() const;
  PbModule* get_pbmod() const;
  PbEnumType* get_pbenum() const;
  YangValue* get_yvalue() const;
  int get_int_value() const;
  std::string get_yang_enumerator() const;
  std::string get_raw_enumerator() const;
  void set_raw_enumerator(std::string raw_enumerator);
  std::string get_proto_enumerator() const;
  std::string get_pbc_enumerator() const;

  unsigned get_index_by_proto_name() const
  {
    RW_ASSERT(index_by_proto_name_ != UINT_MAX);
    return index_by_proto_name_;
  }
  void set_index_by_proto_name(unsigned index_by_proto_name)
  {
    index_by_proto_name_ = index_by_proto_name;
  }

  /* Finds any extension specified in the
   * enum statement. */
  YangExtension* find_ext(
    const char* module,
    const char* ext
  );

  /// Determine if two enumerators are equivalent
  bool is_equivalent(
    /// The other enumerator
    PbEnumValue* other
  ) const;

public:
  /// Read all the enum values, determine conversions.
  void parse();

private:
  /// The owning enum type.
  PbEnumType* pbenum_ = nullptr;

  /// The yang value for the enumerator.
  YangValue* yvalue_ = nullptr;

  /**
   * The raw mangled enumerator name tail for the enum constant.  Not
   * necessarily ready for direct use in other identifiers.
   */
  std::string raw_enumerator_;

  /**
   * The proto enumerator name for the enum constant, as written to the
   * proto file.
   */
  mutable std::string proto_enumerator_;

  /**
   * The rift-protobuf-c enumerator name, as generated.
   */
  mutable std::string pbc_enumerator_;

  /// The enumerator index in ProtobufCEnumDescriptor.values_by_name[] order.
  unsigned index_by_proto_name_ = UINT_MAX;
};


/**
 * One of these objects gets created for every yang enum that gets
 * converted to a protobuf enum, whether top-level or embedded.
 * These objects refer to the owning protobuf field.
 *
 * This object owns the list of its enum values.
 *
 * ATTN: Future support for creating enums from yang identity
 * statements.
 *  - Would need to "subclass" based on namespace.
 */
class PbEnumType
{
public:
  // Utility types.

  typedef PbEnumValue::enum_value_stab_t enum_value_stab_t;
  typedef PbEnumValue::enum_value_list_t enum_value_list_t;
  typedef enum_value_list_t::const_iterator enum_value_iter_t;

  // For storing the enumeration types.  Owned by the module.
  typedef std::unique_ptr<PbEnumType> enum_type_uptr_t;
  typedef std::list<enum_type_uptr_t> enum_type_list_t;

  /// For looking up enum types.
  typedef std::unordered_map<YangType*,PbEnumType*> enum_ytype_map_t;

  /// For looking up enum types.
  typedef std::unordered_map<YangTypedEnum*,PbEnumType*> enum_ytenum_map_t;

  /// Lookup table for finding enum redefinitions.
  typedef std::multimap<std::string,PbEnumType*> enum_typename_map_t;

  /// Lookup table for mapping equivalent enums.
  typedef std::unordered_map<PbEnumType*,PbEnumType*> enum_type_equivalent_t;

  /// Path string list.  Useful for generating manglings.
  typedef std::list<std::string> path_list_t;


public:
  /// Constructor
  PbEnumType(
    /// The enum's yang type.
    YangType* ytype,
    /// The node where the enum was encountered.
    PbNode* pbnode
  );

  /// Constructor
  PbEnumType(
    /// The enum's yang typedef type.
    YangTypedEnum* ytenum,
    /// The module where the enum was encountered.
    PbModule* pbmod
  );

  // Cannot copy
  PbEnumType(const PbEnumType&) = delete;
  PbEnumType& operator=(const PbEnumType&) = delete;

public:
  // accessors
  PbModel* get_pbmodel() const;
  PbModule* get_pbmod() const;
  PbNode* get_scoped_pbnode() const;
  YangType* get_ytype() const;
  YangTypedEnum* get_ytenum() const;
  bool has_aliases() const;
  enum_value_iter_t begin() const;
  enum_value_iter_t end() const;

  PbEnumType* get_equivalent_pbenum() const;
  void set_equivalent_pbenum(PbEnumType* equivalent_pbenum);

  bool is_suppressed() const;
  bool is_typed() const;

  void make_proto_path_list() const;

  std::string get_location() const;
  std::string get_proto_enclosing_typename() const;
  std::string get_proto_type_path() const;
  std::string get_proto_c_prefix() const;
  std::string get_pbc_typename() const;
  std::string get_pbc_enum_desc() const;
  std::string get_ypbc_var_enumdesc() const;
  std::string get_ypbc_var_enumsort() const;
  std::string get_ypbc_var_enumstr() const;
  PbEnumValue* get_value(const char* val) const;

  /// Create a rift-protoc-c generated C variable or function name
  std::string make_pbc_name(const char* suffix) const;

  /// Create a yangpbc generated C variable or function name
  std::string get_ypbc_global(const char* discriminator) const;
  std::string get_ypbc_global(const std::string& discriminator) const;

  /// Create a yangpbc generated, GI-related, C variable or function name
  std::string make_gi_name(const char* discriminator) const;
  std::string make_gi_name(const std::string& discriminator) const;

  /* Finds any extension specified in the
   * enumeration type statement. */
  YangExtension* find_ext(
    const char* module,
    const char* ext
  );

  /// Determine if two enum types are equivalent
  bool is_equivalent(
    /// The other type
    PbEnumType* other
  ) const;

public:
  /// Read all the enum values, determine conversions.
  void parse();

public:
  /// Output proto enum definition
  void output_proto_enum(
    std::ostream& os,
    unsigned indent);

  /// Output C header enum declarations
  void output_h_enum(
    std::ostream& os,
    unsigned indent);

  /// Output C++ header definitions
  void output_cpp_enum(
    std::ostream& os,
    unsigned indent);

private:
  /// The owning module.
  PbModule* pbmod_ = nullptr;

  /**
   * If the yang enumeration is not defined at the top-level, this node
   * defines the enum type's scope.  Will be nullptr if top-level.
   */
  PbNode* scoped_pbnode_ = nullptr;

  /// The yang type.
  YangType* ytype_ = nullptr;

  /**
   * The defined enum type.  If the enum is defined inline in yang,
   * this will be nullptr, in which case the enum is scoped to a
   * specific field.
   */
  YangTypedEnum* ytenum_ = nullptr;

  /// The equivalent type (to use instead of this one).
  PbEnumType* equivalent_pbenum_ = nullptr;

  /**
   * The enum's enclosing proto message type name (pathless).
   */
  std::string proto_enclosing_type_;

  /**
   * The full proto enum type path, suitable for use as protobuf field
   * type specifier.
   */
  mutable std::string proto_type_path_;

  /**
   * The proto c_prefix option value.  This will be prepended to all
   * enum values to create the C enumerator identifier.
   */
  mutable std::string proto_c_prefix_;

  /**
   * The rift-protobuf-c type name.
   */
  mutable std::string pbc_type_;

  /**
   * The rift-protobuf-c function/variable base name.
   */
  mutable std::string pbc_basename_;

  /// The GI base identifier name
  mutable std::string gi_basename_;

  /// The ypbc enum descriptor variable name.
  mutable std::string ypbc_var_enumdesc_;

  /// The ypbc yang enumerators sort order variable name.
  mutable std::string ypbc_var_enumsort_;

  /// The ypbc yang enumerators yang enumerators variable name.
  mutable std::string ypbc_var_enumstr_;

  /**
   * Flag indicating that one or more enumerators are aliased.  Used to
   * generate the proper option in proto to suppress alias warnings.
   */
  bool has_aliases_ = false;

  /// The list of enumerator values, in yang order, and owned by this object.
  enum_value_list_t value_list_;

  /// The list of enumerator values.
  enum_value_stab_t value_stab_;

  /// The most-recently tried message for a unique proto type name.
  bool parse_complete_ = false;

  /// The protobuf path list.
  mutable path_list_t proto_path_list_;
};


/**
 * One of these objects gets created for every yang node that gets
 * converted to a protobuf field.  These objects refer to the owning
 * protobuf parser, the owning protobuf module, the owning protobuf
 * node, the owning protobuf message.  This object may also refer to
 * related protobuf encoding yang extension objects.
 *
 * The protobuf message descriptors own these objects.  These objects
 * live until the owning message is destroyed.  The message destruction
 * order is undefined, so if this class has a destructor, it should not
 * rely on any of the member pointers being valid!
 */
class PbField
{
public:
  // Utility types.

  // For storing sets of fields.  Messages will use this.
  typedef std::unique_ptr<PbField> field_uptr_t;
  typedef std::list<field_uptr_t> field_list_t;

  // For looking up fields.  Extension resolution will need these types.
  typedef PbSymTab<PbField*> field_stab_t;

  typedef std::map<unsigned, PbField*> field_by_tag_t;

  /**
   * Select how to output a field.
   */
  enum class proto_output_style_t
  {
    ILLEGAL = 0, ///< none/illegal value

    SEL_SCHEMA, ///< use the new global-schema style
    SEL_OLD_STYLE, ///< use the old-style
    SEL_SCHEMA_KEY, ///< just output as a schema key value
  };

  struct field_size_info_t
  {
    uint32_t field_size;
    uint32_t align_size;
    uint32_t has_size;
  };

public:
  /**
   * Initialize the static parsing tables.
   */
  static void init_tables();

public:
  /// Constructor
  PbField(
    /// The owning message.
    PbMessage* owning_pbmsg,
    /// The field's defining node.
    PbNode* pbnode,
    /// The field's message.
    PbMessage* field_pbmsg = nullptr
  );

  // Cannot copy
  PbField(const PbField&) = delete;
  PbField& operator=(const PbField&) = delete;

public:
  // accessors
  PbModel* get_pbmodel() const;
  PbModule* get_pbmod() const;
  PbModule* get_schema_defining_pbmod() const;
  PbNode* get_pbnode() const;
  void set_field_pbmsg(PbMessage* pbmsg);
  PbMessage* get_field_pbmsg() const;
  PbMessage* get_field_pbmsg_eq() const;
  PbMessage* get_field_pbmsg_eq_local_only() const;
  PbMessage* get_owning_pbmsg() const;

  YangNode* get_ynode() const;
  rw_yang_stmt_type_t get_yang_stmt_type() const;
  YangValue* get_yvalue() const;
  unsigned get_tag_value() const;

  /// Is this field from a message?
  bool is_message() const;

  /// Is this field an inline message?
  bool is_inline_message() const;

  /// Is this field  inline
  bool is_field_inline() const;

  /// Is this field stringy
  bool is_stringy() const;

  /// Is this field bytes
  bool is_bytes() const;

  void set_proto_fieldname(std::string proto_fieldname);
  std::string get_proto_fieldname() const;
  std::string get_pbc_fieldname() const;

  std::string get_proto_typename() const;
  std::string get_pbc_typename() const;
  std::string get_gi_typename(bool use_const = true) const;

  std::string get_pbc_schema_key_descriptor();


public:

  /**
   * Determine if another field is equivalent to this field.  Parsing
   * must be complete for both fields.
   *
   * @return true if both fields are equivalent.
   */
  bool is_equivalent(
    /// [in] The field to compare with this one.
    PbField* other_pbfld,
    /**
     * [out] The reason why the fields are not equivalent.  May be
     * nullptr if the caller does not care about the reason.  Not
     * meaningful if the fields are equivalent - may be stomped even if
     * the fields are equivalent.
     */
    const char** reason,
    /// The schema in which to compare messages
    PbModule* schema_pbmod = nullptr
  ) const;

  /// get the size of the field in the generated structure
  field_size_info_t get_pbc_size() const;

  /// Does this field's generated protoc  has a quantifier
  bool has_pbc_quantifier_field() const;

  /// Get this field's protoc quantifier - returns empty string when has_pbc_quantifier() returns false
  std::string get_pbc_quantifier_field() const;


public:
  /**
   * Start protobuf field parsing.
   */
  void parse_start();

  /**
   * Complete protobuf field parsing.  This parsing phase updates the
   * tag.
   */
  void parse_complete();


private:
  /// No-op parser function.
  parser_control_t parse_noop();

  /// Find the tag extension and determine how to handle it.
  parser_control_t parse_tag_unset();

  /// Handle automatic tag numbering.
  parser_control_t parse_tag_auto();

  /// Handle field-relative tag numbering.
  parser_control_t parse_tag_field();

  /// Handle base-relative tag numbering.
  parser_control_t parse_tag_base();

  /**
   * Final parser step for tag - validate ordering raltive to the
   * enclosing message.
   */
  parser_control_t parse_tag_value();


public:

  /// Output a proto key field message definition
  void output_proto_key_message(std::ostream& os, unsigned indent);

  /// Output a proto key field
  void output_proto_key_field(unsigned n, std::ostream& os, unsigned indent);

  /// Output a field definition, in global-schema format.
  void output_proto_schema_field(std::ostream& os, unsigned indent);

  /// Output a field definition.
  void output_proto_field(std::ostream& os, unsigned indent);

  /// Output a field definition, common implementation.
  void output_proto_field_impl(proto_output_style_t style, std::ostream& os, unsigned indent);

  /// Output field metadata macro.
  void output_h_meta_data_macro(std::ostream& os, unsigned indent);

  /// Output ypbc field descs.
  void output_cpp_ypbc_flddesc_body(std::ostream& os, unsigned indent, unsigned pbc_index, unsigned yang_order);

  /// Output a C++ schema key initialization
  void output_cpp_schema_key_init(std::ostream& os, unsigned indent, std::string offset);

  /// Output a C++ field initialization (to the default value)
  void output_cpp_schema_default_init(std::ostream& os, unsigned indent);


private:
  // Parser tables.

  /**
   * Select how a tag value is determined.
   */
  enum class tag_t
  {
    ILLEGAL = 0, ///< none/illegal value
    UNSET, ///< has not been determined yet

    SEL_AUTO, ///< tag is auto-generated sequentially relative to previous field's tag
    SEL_VALUE, ///< an explicit tag has been specified
    SEL_FIELD, ///< tag value is relative to a specified field
    SEL_BASE, ///< tag value is relative to the enclosing object
  };

  typedef parser_control_t (PbField::*parse_field_pmf_t)();
  typedef std::unordered_map<tag_t,parse_field_pmf_t,enum_hash<tag_t>> map_parse_tag_t;

  /// Parser table for tag selection state.
  static map_parse_tag_t map_parse_tag;


private:

  /// The corresponding pb-node.
  PbNode* pbnode_ = nullptr;

  /// The owning pb-message.
  PbMessage* owning_pbmsg_ = nullptr;

  /// Is the field also a message?
  PbMessage* field_pbmsg_ = nullptr;

  /// The proto field name.  May differ from the default.
  mutable std::string proto_fieldname_;

  /// ATTN: Deprecate this?
  /// If a key, the type of the key's message.
  std::string proto_key_type_;

  /// ATTN
  std::string pbc_schema_key_descriptor_;

  /// The state of tag value selection.
  // ATTN: This is deprecated with module-based tag numbering!
  tag_t tag_ = tag_t::UNSET;

  /// The field that a relative tag will be calculated from
  std::string tag_field_;

  /// The actual tag value.  0 until calculated.
  unsigned tag_value_ = 0;

  /// Parsing has been started for this field
  bool parse_started_ = false;

  /// Parsing has been completed for this field
  bool parse_complete_ = false;
};


/**
 * A message equivalence set.  An equivalence set is the set of all
 * messages that would otherwise share the exact same proto definition.
 * When the output files are generated by yangpbc, only one message out
 * of an equivalence set will be output - the preferred message.  The
 * remaining messages in the equivalence set will be suppressed, and
 * their referring fields' types replaced with the preferred message's
 * type.
 *
 * Equivalences are created by the yang uses and augment statements.
 * There are two problems equivalences are designed to solve.
 *
 * The first problem is one of duplication reduction.  The uses
 * statement can create duplication of message contents.  We would like
 * to ensure that such duplication is suppressed, so that that those
 * messages share the exact same type (and are therefore type
 * compatible in strongly-typed languages like C).  All of the
 * equivalent copies of a message must be replaced with a single
 * message; with the putative duplicates referring to the one selected
 * message via proto dotted-name notation.  Duplication reduction is
 * achieved by assigning all duplicates to the same equivalence set.
 *
 * The second problem is one of conflict detection and resolution.
 * This problem is created by the yang augment statement, which can
 * modify the contents of a message.  In such cases, duplication
 * reduction must not be allowed to collapse both resulting messages
 * into a single type, even though they otherwise share the same
 * "original type".  Instead, the augmented messages must be output as
 * a different type than the unaugmented version or differently
 * augmented version(s).  Even though augments create conflicts, they
 * can also create duplication (as augmented messages may also
 * participate in uses statements).  So conflicts do not preclude
 * duplication reduction.  Conflict resolution is acheived by creating
 * a new equivalence set for each (unique) conflict.
 *
 * Equivalence set construction occurs during parse, as each message
 * parse completes (equivalence cannot be determined until message
 * parse completes).  Once parsing has completed for all messages in
 * the schema, a global operation is required to compare all the
 * equivalence sets and determine the optimal selection for the
 * preferred messages.  For equivalence sets that have only one
 * message, that message is the preferred message (obviously).  When
 * there is more than one message in the set, once the preferred
 * message is selected, all of the suppressed messages' descendants
 * must be removed from their respective equivalence sets.
 *
 * There is one additional complication that this class must take care
 * of.  Although it would be quite easy to determine the equivalence
 * sets across the entire global schema, this is insufficient.  Each
 * module imported into the current global schema has also had an
 * independent calculation of message equivalence sets during a
 * previous yangpbc invocation.  Therefore, each yangpbc invocation
 * must recreate the independent message set determinations made by the
 * parsing of all of the module imports.  Furthermore, such recreated
 * determinations must be stable - they must calculate the same exact
 * set of duplications and conflicts regardless of whether a module is
 * being parsed as the primary module, or as an import (possibly many
 * imports removed from the target module).  This is a giant
 * complication.
 */
class PbEqMsgSet
{
public:
  // Utility types.

  /**
   * Unique pointer for memory management.  Equivalence sets are owned
   * by the set manager.
   */
  typedef std::unique_ptr<PbEqMsgSet> eq_uptr_t;

  /// Module set for tracking related modules by module pointer.
  typedef std::unordered_set<PbModule*> module_set_t;

  /// Lookup and ownership table.
  typedef std::multimap<std::string,eq_uptr_t> eq_map_t;

  /// List of equivalence sets.
  typedef std::list<PbEqMsgSet*> eq_list_t;

  /// List of messages for tracking the mesages in the set.
  typedef std::list<PbMessage*> msg_list_t;

  /// Iterator for the messages in the set.
  typedef msg_list_t::const_iterator msg_citer_t;

  /// Mapping of module to preferred message.
  typedef std::unordered_map<PbModule*,PbMessage*> preferences_t;

  /// Sets equivalence sets.
  typedef std::unordered_set<PbEqMsgSet*> eq_set_t;

  /// Map for generating dependencies.
  typedef std::unordered_map<PbEqMsgSet*, eq_set_t> depends_map_t;

  /// Helper type for generating dependencies.
  typedef depends_map_t::iterator depends_map_iter_t;


public:

  PbEqMsgSet(
    /// The first message in the set.
    PbMessage* pbmsg
  );

  // Cannot copy
  PbEqMsgSet(const PbEqMsgSet&) = delete;
  PbEqMsgSet& operator=(const PbEqMsgSet&) = delete;

public:
  /// Get the owning model.
  PbModel* get_pbmodel() const;

  PbModule* get_base_pbmod() const;

  /// Get the key for the equivalence set.
  std::string get_key() const;

  /// Get a representative message.  Any one will do.
  PbMessage* get_any_pbmsg() const;

  /// Get a representative module.  Any one will do.
  PbModule* get_any_pbmod() const;

  /**
   * Determine if a message belongs to the set, possibly as viewed
   * through a particular schema module.
   */
  bool is_equivalent(
    /// The message to compare.
    PbMessage* pbmsg,
    /// The schema in which to compare the message
    PbModule* schema_pbmod = nullptr
  ) const;

  /**
   * Add an equivalence.
   * @return true if the message represents a new defining module.
   */
  bool add_equivalence(
    /// The message to add.
    PbMessage* pbmsg
  );

  /**
   * Check if this equivalence set is equivalent to a "lower"
   * equivalence set, when viewed through the lens of the "lower"
   * equivalence set's defining module(s).  Such a situation exists
   * when this equivalence set augments a message in a schema that
   * imports the schema that defines the "lower" equivalence set.
   * ("Lower" in this case is in terms of module import relationships,
   * with lower meaning is-imported-by the "higher" module.)
   *
   * In effect, this function checks to see if two equivalence sets
   * define equivalent messages, but only when viewed through a
   * fragment of the global schema.  This information is needed in
   * order to reconstruct a picture of the global schema as seen by all
   * the other modules.
   */
  void parse_schema_lower_equivalence(
    /// The equivalence set to check.
    PbEqMsgSet* lower_pbeq
  );

  /**
   * Reorder a set of message equivalence sets, based on message
   * dependencies.
   */
  static void order_dependencies(
    eq_list_t* order_list);

  /**
   * Determine the message children's equivalence set dependencies.
   * These dependencies may be used during code generation to order the
   * code in a definition-then-use order, particularly in regards to C
   * structs for flat/inline message definitions.
   */
  void parse_find_dependencies(
    depends_map_t* depends_map,
    depends_map_iter_t this_dmi
  ) const;

  /**
   * Determine the message children's equivalence set dependencies.
   * These dependencies may be used during code generation to order the
   * code in a definition-then-use order, particularly in regards to C
   * structs for flat/inline message definitions.
   */
  void parse_order_dependencies(
    depends_map_t* depends_map,
    depends_map_iter_t this_dmi,
    eq_list_t* order_list
  );

  /**
   * Pick a preferred message to output, for each of the defining
   * modules.  This operation results in the suppression of duplicate
   * message definitions.  It must only be called once parse completes.
   */
  void pick_preferences();

  /**
   * Suppress an equivalence.  This removes the message from output as
   * a separate message type.  Fields that would otherwise reference
   * the suppressed message type will instead refer to (one of) the
   * unsuppressed message(s).
   */
  void suppress_msg(
    /// The message to suppress.
    PbMessage* pbmsg
  );

  /**
   * Mark a message name conflict in a particular schema module.  When
   * a conflict exists, more than one message equivalence set exists in
   * the module's global schema that would have the same name.
   */
  void mark_conflict(
    /// The module to mark.
    PbModule* schema_pbmod
  );

  /**
   * Determine if the equivalence set has a conflict, when viewed from
   * the given module's global schema.
   */
  bool has_conflict(
    /// The schema module to check.
    PbModule* schema_pbmod
  ) const;

  /// Get the preferred message, as viewed in a particular schema.
  PbMessage* get_preferred_msg(
    /// The schema in which to compare the message
    PbModule* pbmod = nullptr
  );

  /// Get an iterator to the first message in the set.
  msg_citer_t msg_begin() const;

  /// Get an iterator after the last message in the set.
  msg_citer_t msg_end() const;


private:
  /// The module that defines the original base message.
  PbModule* base_pbmod_ = nullptr;

  /**
   * The modules that contain an equivalence.  Must be the same as the
   * LCI of all the messages(s) in the set.
   */
  module_set_t reference_pbmods_;

  /**
   * The modules that independently define the equivalence.  These
   * equivalences are not visible to each other - the occur because
   * modules independently created the same effect message type.  All
   * members of this set are mutually non-(transitive)-importing.
   */
  module_set_t defining_pbmods_;

  /**
   * The set of modules in which this equivalence set has a
   * rwpb:msg-new naming conflict with a related (same base message) or
   * unrelated (different base message) equivalence set.  When
   * conflicts occur, special handling is needed.
   */
  module_set_t conflict_pbmods_;

  /**
   * Mapping of this equivalence set (as viewed by the modules in
   * defining_pbmods_) to decayed equivalences in all of the other
   * modules in which the message appears.  A decayed equivalence is an
   * equivalence of a message as viewed in another modules' schema -
   * not the defining module's schema - when that equivalence does not
   * have the same exact message definition.  This set may be empty if
   * all such modules use this equivalence.  May have multiple entries
   * if this equivalence decays in different ways in the other modules.
   */
  eq_set_t other_pbeqs_;

  /// The list of equivalent messages.
  msg_list_t pbmsgs_;

  /// The list of suppressed messages.
  msg_list_t suppressed_pbmsgs_;

  /// The selected preferences (by schema module).
  preferences_t preferences_;


  friend class PbEqMgr;
};


/**
 * Equivalence set manager.
 */
class PbEqMgr
{
public:
  // Utility types.
  typedef PbEqMsgSet::eq_map_t eq_map_t;

public:
  PbEqMgr(
    /// The owning model.
    PbModel* pbmodel
  );

  // Cannot copy
  PbEqMgr(const PbEqMgr&) = delete;
  PbEqMgr& operator=(const PbEqMgr&) = delete;

public:
  // accessors
  PbModel* get_pbmodel() const;

  /**
   * Lookup/allocate the equivalence set for a message, and save it in
   * the message.
   *
   * @return The found equivalence set, or a newly allocated one.
   *   Never nullptr.
   */
  void parse_equivalence(
    PbMessage* pbmsg
  );

  /**
   * Select the preferred messages across all equivalence sets.  Must
   * be done after parsing all messages.
   */
  void find_preferred_msgs();

private:
  static std::string make_key(PbMessage* pbmsg);
  static std::string make_key(PbEqMsgSet* pbeq);

  /**
   * Save an equivalence in a message.  Check for conflicts and lower
   * equivalences.
   */
  void add_equivalence(
    /// The equivalence set to save in the message.
    PbEqMsgSet* pbeq,
    /// The message to save in the equivalence set.
    PbMessage* pbmsg
  );

  /**
   * Check an equivalences set for conflicts and lower equivalences,
   * using a newly added message.
   */
  void check_conflicts(
    /// The equivalence set to check.
    PbEqMsgSet* pbeq,
    /// The new message.
    PbMessage* pbmsg
  );

private:
  /// The owning model.
  PbModel* pbmodel_ = nullptr;

  /// The equivalences map.
  eq_map_t eq_map_;

  friend class PbEqMsgSet;
};


/**
 * One of these objects gets created for every yang node that gets
 * converted to a protobuf message, whether top-level or embedded.
 * These objects refer to the owning protobuf parser, the owning
 * protobuf module, the owning protobuf node, and the parent protobuf
 * message (if there is one).  This object will also refer to related
 * protobuf encoding yang extension objects.
 *
 * This object owns the list of its member fields.  The fields are kept
 * in two orders - in declaration order (for yang and XML
 * correspondence), and mapped by field name in order to do field
 * lookup during tag resolution.
 *
 * The field that uses this message (if any) will refer back to this
 * object.  If this is a top-level definition, multiple fields within
 * the protobuf package may refer to this object.
 *
 * The corresponding protobuf nodes own these objects.  These objects
 * live until the owning node is destroyed.  The node destruction order
 * is undefined, so if this class has a destructor, it should not rely
 * on any of the member pointers being valid!
 */
class PbMessage
{
public:
  /**
   * Initialize the static parsing tables.
   */
  static void init_tables();

public:
  // Utility types.

  typedef PbField::field_list_t field_list_t;
  typedef field_list_t::const_iterator field_citer_t;
  typedef PbField::field_stab_t field_stab_t;

  /**
   * Unique pointer for memory management.  Messages are owned by the
   * corresponding PbNode.
   */
  typedef std::unique_ptr<PbMessage> msg_uptr_t;

  /// Message list. Typically ordered by yang definition, or parser encounter.
  typedef std::list<PbMessage*> msg_list_t;

  /// Owned message list.
  typedef std::list<msg_uptr_t> msg_owned_list_t;

  /// Path string list.  Useful for generating manglings.
  typedef std::list<std::string> path_list_t;

  /**
   * Select how to output a ypbc message descriptor
   */
  enum class output_t
  {
    illegal_ = 0, ///< none/illegal value
    message, ///< message definitions
    path_entry, ///< schema path entry
    dom_path, ///< dom path definition
    path_spec, ///< full path specification
    list_only, ///< list-only definition
  };

  /**
   * Select how to output documentation
   */
  enum class doc_t
  {
    illegal_ = 0, ///< none/illegal value
    api_entry, ///< API documentation entry
    api_toc, ///< API documentation table of contents
    user_entry, ///< User documentation entry
    user_toc, ///< User documentation table of contents
  };

public:
  /**
   * Create a pb-message.
   */
  PbMessage(
    /// [in] The associated node.
    PbNode* pbnode,
    /**
     * [in] The owning message.  May be nullptr, but that should only
     * be for messages owned by the module itself.
     */
    PbMessage* parent_pbmsg,
    /**
     * [in] The type of message.  This controls parsing and output.
     */
    PbMsgType pb_msg_type
  );

  // Cannot copy
  PbMessage(const PbMessage&) = delete;
  PbMessage& operator=(const PbMessage&) = delete;

public:
  // accessors
  PbModel* get_pbmodel() const;
  PbModule* get_pbmod() const;
  PbModule* get_schema_defining_pbmod() const;
  PbModule* get_lci_pbmod() const;
  PbNode* get_pbnode() const;
  PbMessage* get_parent_pbmsg() const;
  PbMessage* get_rpc_input_pbmsg() const;
  PbMessage* get_rpc_output_pbmsg() const;
  YangNode* get_ynode() const;

  std::string get_proto_msg_new_typename();
  std::string get_proto_typename();

  path_list_t get_proto_message_path_list();
  std::string get_proto_message_path();
  path_list_t get_proto_schema_path_list(
    output_t output_style);
  std::string get_proto_schema_path(
    output_t output_style);

  std::string get_pbc_message_typename();
  std::string get_pbc_message_global(
    const char* suffix);
  std::string get_pbc_message_upper(
    output_t output_style,
    const char* suffix);

  std::string get_pbc_schema_typename(
    output_t output_style);
  std::string get_pbc_schema_desc_typename(
    output_t output_style);
  std::string get_pbc_schema_global(
    output_t output_style,
    const char* suffix);
  std::string get_pbc_schema_desc_global(
    output_t output_style,
    const char* suffix);

  path_list_t get_ypbc_path_list();
  path_list_t get_ypbc_msg_new_path_list();
  std::string get_rwpb_long_alias();
  std::string get_ypbc_long_alias();
  std::string get_rwpb_short_alias();
  std::string get_ypbc_short_alias();
  std::string get_rwpb_msg_new_alias();
  std::string get_ypbc_msg_new_alias();
  std::string get_proto_msg_new_path();
  std::string get_rwpb_imported_alias();
  std::string get_ypbc_imported_alias();
  std::string get_rwpb_imported_msg_new_alias();
  std::string get_ypbc_imported_msg_new_alias();
  std::string get_ypbc_global(
    const char* discriminator = nullptr);

  path_list_t get_gi_typename_path_list(
    output_t output_style = output_t::message);
  std::string get_gi_typename(
    output_t output_style = output_t::message);
  std::string get_gi_desc_typename(
    output_t output_style = output_t::message);
  std::string get_gi_python_typename(
    output_t output_style = output_t::message);

  std::string get_gi_c_identifier(
    const char* operation = nullptr,
    output_t output_style = output_t::message,
    const char* field = nullptr);
  std::string get_gi_c_desc_identifier(
    const char* operation = nullptr,
    output_t output_style = output_t::message,
    const char* field = nullptr);

  path_list_t get_xpath_path_list();
  std::string get_xpath_path();
  path_list_t get_xpath_keyed_path_list();
  std::string get_xpath_keyed_path();

  path_list_t get_rwrest_uri_path_list();
  std::string get_rwrest_uri_path();

  PbField* get_field_by_name(std::string fieldname) const;
  field_citer_t field_begin() const;
  field_citer_t field_end() const;
  unsigned has_fields() const;
  unsigned has_messages() const;
  rw_yang_stmt_type_t get_yang_stmt_type() const;

  void set_pbeq(PbEqMsgSet* pbeq);
  PbEqMsgSet* get_pbeq() const;
  PbMessage* get_eq_pbmsg();
  void set_chosen_eq();
  bool has_conflict() const;

  bool is_parse_complete();
  bool is_root_message() const;
  bool is_top_level_message() const;
  bool is_local_message() const;
  bool is_real_top_level_message() const;
  bool is_local_top_level_message() const;
  bool is_scoped_message();
  bool is_local_scoped_message();
  bool is_local_defining_message();
  bool is_local_schema_defining_message() const;
  bool is_pbc_suppressed(output_t output_style = output_t::message) const;
  bool is_schema_suppressed() const;
  bool generate_field_descs() const;

  PbMsgType get_pb_msg_type() const;
  PbMsgType get_category_msg_type() const;
  uint32_t get_event_id() const;

  /// Walks through the PbMessage hiearchy and invkes visitor callbacks on every
  /// PbMessage visit.
  bool traverse(
    /// Visitor on which the visit callbacks to be invoked
    PbModelVisitor* visitor
  );

public:

  /// Determine if two messages are equivalent.
  bool is_equivalent(
    /// The message to compare to
    PbMessage* msg,
    /// [out] If incompatible, a short description of why.  May be nullptr.
    const char** reason,
    /// [out] If incompatible, the field that is incompatible.  May be nullptr.  Result may be nullptr.
    PbField** field,
    /// The schema in which to compare messages
    PbModule* schema_pbmod = nullptr
  ) const;

  /// Determine which of 2 equivalent messages appears first in the schema.
  PbMessage* equivalent_which_comes_first(
    PbMessage* other_pbmsg,
    PbModule* schema_pbmod
  );

  /// get the size of the message in the generated structure
  uint32_t get_pbc_size();

  /// Validate the maximum size of the protobuf-c struct
  void validate_pbc_size();

  /**
   * Add a field to the message.  The field's node has already been
   * parsed, and its parent node is this message's pbnode.  Create the
   * field object, link it to the other objects, and then parse it.
   */
  PbField* add_field(
    /// The node that defines the field to add
    PbNode* field_pbnode,
    /// If the node is a message, set the message parsing type
    PbMsgType pb_msg_type = PbMsgType::automatic
  );


public:
  /**
   * Perform message parsing.
   */
  void parse();

  /// Find required schema imports.
  void find_schema_imports();


private:

  /// Parse regular message.
  void parse_pb_msg_type_message();

  /// Parse grouping message.
  void parse_pb_msg_type_group_root();

  /// Parse rpc-input message.
  void parse_pb_msg_type_rpci_module();

  /// Parse rpc-output message.
  void parse_pb_msg_type_rpco_module();

  /// Parse notification message.
  void parse_pb_msg_type_notif_module();

  /// Start parsing a message.
  void parse_start();

  /// Filter the child nodes, take only particular kind of node.
  void parse_add_children_filter(
    uint64_t filter_mask);

  ///  Filter the child nodes, take only rpc child input or output.
  void parse_add_children_rpc(
    const char* type);

  /// Complete parsing a message.
  void parse_complete();


public:

  /// Output proto message definition
  void output_proto_message(output_t output_style, std::ostream& os, unsigned indent);

  /// Output C header file struct tag declarations
  void output_h_struct_tags(std::ostream& os, unsigned indent);

  /// Output C header file struct definitions
  void output_h_structs(std::ostream& os, unsigned indent);

  /// Output C header file ypbc_msg type-punned struct body
  void output_h_ypbc_msgdesc_body(output_t output_style, std::ostream& os, unsigned indent);

  /// Output C header file children descriptors list
  void output_h_struct_children(output_t output_style, std::ostream& os, unsigned indent);

  /// Output message meta data macro
  void output_h_meta_data_macro(std::ostream& os, unsigned indent);

  /// Output a C++ schema path element variable.
  void output_cpp_path_element(std::ostream& os, unsigned indent);

  /// Output a C++ schema path entry initialization
  void output_cpp_schema_path_entry_init(char eoinit, std::ostream& os, unsigned indent, std::string offset);

  /// Output a C++ schema dom path initialization
  void output_cpp_schema_dompath_init(char eoinit, std::ostream& os, unsigned indent);

  /// Output a C++ schema path spec initialization
  void output_cpp_schema_path_spec_init(char eoinit, std::ostream& os, unsigned indent);

  /// Output C++ file message yang-field-order array.
  void output_cpp_field_order(std::ostream& os, unsigned indent);

  /// Output C++ file yang field descriptors.
  void output_cpp_ypbc_field_descs(std::ostream& os, unsigned indent);

  /// Output C++ file ypbc_msg struct initialization values
  void output_cpp_ypbc_msgdesc_body(output_t output_style, std::ostream& os, unsigned indent);

  /// Output C++ file constants data definitions
  void output_cpp_msgdesc_define(std::ostream& os, unsigned indent);

  /**
   * Output C++ file schema constants data definitions for rpc input,
   * to tie input and output together.
   */
  void output_cpp_msgdesc_define_rpc_input(std::ostream& os, unsigned indent);

  /// Output C++ file schema constants data definitions for the group root
  void output_cpp_msgdesc_define_group_root(std::ostream& os, unsigned indent);

  /// Output C++ file schema constants data definitions for simple message
  void output_cpp_msgdesc_define_message(std::ostream& os, unsigned indent);

  /// Output gi h definitions
  void output_gi_h_decls(std::ostream& os, unsigned indent);
  void output_gi_to_xml_method_decl(std::ostream& os, unsigned indent);
  void output_gi_to_xml_v2_method_decl(std::ostream& os, unsigned indent);
  void output_gi_from_xml_method_decl(std::ostream& os, unsigned indent);
  void output_gi_from_xml_v2_method_decl(std::ostream& os, unsigned indent);
  void output_gi_to_pb_cli_method_decl(std::ostream& os, unsigned indent);

  void output_gi_desc_keyspec_to_entry_method_decl(std::ostream& os, unsigned indent);
  void output_gi_desc_keyspec_leaf_method_decl(std::ostream& os, unsigned indent);
  void output_gi_desc_retrieve_descriptor_method_decl(std::ostream& os, unsigned indent);

  /// Output gi C definitions
  void output_gi_c_funcs(std::ostream& os, unsigned indent);
  void output_gi_to_xml_method(std::ostream& os, unsigned indent);
  void output_gi_to_xml_v2_method(std::ostream& os, unsigned indent);
  void output_gi_from_xml_method(std::ostream& os, unsigned indent);
  void output_gi_from_xml_v2_method(std::ostream& os, unsigned indent);
  void output_gi_to_pb_cli_method(std::ostream& os, unsigned indent);

  void output_gi_desc_keyspec_to_entry_method(std::ostream& os, unsigned indent);
  void output_gi_desc_keyspec_leaf_method(std::ostream& os, unsigned indent);
  void output_gi_desc_retrieve_descriptor_method(std::ostream& os, unsigned indent);

  /// Output programmer's guide
  void output_doc_message(
    const doc_file_t file_type,    
    std::ostream& os,
    unsigned indent,
    doc_t doc_style,
    const std::string& chapter );

private:

  /// The owning module.
  PbModule* pbmod_ = nullptr;

  /// The owning pbnode.
  PbNode* pbnode_ = nullptr;

  /// The parent pbmsg.
  PbMessage* parent_pbmsg_ = nullptr;

  /// The message equivalence set - all the messages with the same definition.
  PbEqMsgSet* pbeq_ = nullptr;

  /// The message has been chosen as the keeper in an equivalence set.
  bool is_chosen_eq_ = false;

  /// The rwpb:msg-new typename, if any.  Empty string if not applicable.
  std::string proto_msg_new_typename_;

  /// The proto typename (as modified by rwpb:msg-name).
  std::string proto_typename_;

  /// The full proto path to the message.
  std::string proto_message_path_;

  /// The protobuf-c message C typedef.
  std::string pbc_message_typename_;

  /// The protobuf-c message C function/variable identifier stem
  std::string pbc_message_basename_;

  /**
   * The ypbc identifier stem.  This stem is used to generate symbols
   * for ypbc.
   */
  std::string ypbc_path_;

  /// The original rwpb path name.
  std::string rwpb_path_;

  /// The local alias rwpb path name, for messages that are not local defining.
  std::string rwpb_alias_path_;

  /// The example XPath without keys.
  std::string xpath_path_;

  /// The example XPath, with sample keys.
  std::string xpath_keyed_path_;

  /// The example RW.REST URI, with sample keys.
  std::string rwrest_uri_path_;

  /// Path element variable has been output.
  bool ypbc_pathelem_done_ = false;

  /// Path definition array variable has been output.
  bool ypbc_pathdef_done_ = false;

  /// The last assigned field tag.
  unsigned last_tag_ = 0;

  /// The list of owned field in this message.  In declaration order.
  field_list_t field_list_;

  /// Lookup table for fields by name.
  field_stab_t field_stab_;

  /// The size of the rift-protobuf-c message struct
  uint32_t pbc_size_ = 0;

  /// The number of children nodes that are messages themselves
  unsigned message_children_ = 0;

  /// The ypbc message type
  rw_yang_pb_msg_type_t ypbc_msg_type_ = RW_YANGPBC_MSG_TYPE_NULL;

  /// Parsing has started.
  bool parse_started_ = false;

  /// Parsing has completed.
  bool parse_complete_ = false;

  /**
   * The message type, which broadly defines how the message gets
   * output as proto message(s), proto field(s), ypbc descriptor(s),
   * and other meta-data.
   */
  PbMsgType pb_msg_type_ = PbMsgType::unset;
};


/**
 * One of these objects gets created for every yang node in the schema.
 * These exist whether or not there is a protobuf conversion for the
 * corresponding yang node.  These nodes refer to the yang node, the
 * owning protobuf parser, the owning protobuf module, the parent
 * protobuf node (if there is one), and the containig protobuf message
 * (if there is one).  This object will also refer to several
 * protobuf encoding yang extension objects.
 *
 * The node does not know its own children (unless it is converted to
 * protobuf, and then only indirectly through the protobuf message
 * object).  This object does not need to know its children because the
 * yang schema tree is only walked once, and there is no need to refer
 * to children if not doing protobuf conversion.
 *
 * Whenever a complex node must be converted into protobuf, this object
 * will contain (and own) the protobuf message object.  The message
 * object will refer back to this object.
 *
 * Whenever this object represents a field in a protobuf message, the
 * protobuf field object will refer back to this object.  The protobuf
 * message object owns its fields.
 *
 * The root PbModel parser object owns all these objects.  These
 * objects live until the parser is destroyed.  The node destruction
 * order is undefined, so if this class has a destructor, it should not
 * rely on any of the member pointers being valid!
 */
class PbNode
{
private:
  static const uint32_t PBC_SIZE_MAX_DEFAULT = 512*1024;
  static const uint32_t PBC_SIZE_MAX_WARN = 128*1024;

public:
  // Utility types.
  typedef PbMessage::msg_uptr_t msg_uptr_t;

  /// Unique pointer for memory management
  typedef std::unique_ptr<PbNode> node_uptr_t;

  /// List of nodes for memory management
  typedef std::list<node_uptr_t> node_owned_list_t;
  typedef node_owned_list_t::const_iterator node_owned_citer_t;

  /// List of nodes referenced by other objects
  typedef std::list<PbNode*> node_list_t;
  typedef node_list_t::const_iterator node_citer_t;

  /// Yang to PB node mapping
  typedef std::unordered_map<YangNode*,PbNode*> node_ynode_lookup_t;

  /// Module set: used for recording module relationships that have no ordering
  typedef std::unordered_set<PbModule*> module_set_t;


public:
  /**
   * Initialize the static parsing tables.
   */
  static void init_tables();

public:
  /**
   * Create a pb-node.  This may or not map to a field in another
   * message.  This may or may not map to a top-level pbmsg definition.
   */
  PbNode(
    /// The owning module.
    PbModule* pbmod,
    /// The corresponding yang node
    YangNode* ynode,
    /// The parent pbnode, if any
    PbNode* parent = nullptr
  );

  // Cannot copy
  PbNode(const PbNode&) = delete;
  PbNode& operator=(const PbNode&) = delete;


public:
  PbModel* get_pbmodel() const;
  PbModule* get_pbmod() const;
  PbModule* get_schema_defining_pbmod() const;
  PbModule* get_lci_pbmod() const;
  PbNode* get_parent_pbnode() const;
  PbEnumType* get_pbenum() const;

  YangNode* get_ynode() const;
  YangValue* get_yvalue() const;
  YangType* get_effective_ytype() const;
  YangExtension* get_yext_msg_new() const;
  YangExtension* get_yext_msg_tag_base() const;
  YangExtension* get_yext_field_tag() const;
  YangExtension* get_yext_field_c_type() const;
  YangExtension* get_yext_notif_log_common() const;
  YangExtension* get_yext_notif_log_event_id() const;
  YangExtension* get_yext_field_merge_behavior() const;
  rw_yang_stmt_type_t get_yang_stmt_type() const;
  rw_yang_leaf_type_t get_effective_yang_leaf_type() const;

  std::string get_schema_element_type() const;

  std::string get_yang_description() const;
  std::string get_yang_fieldname() const;  
  std::string get_proto_fieldname() const;
  std::string get_pbc_fieldname() const;

  std::string get_xml_element_name() const;
  std::string get_xpath_element_name() const;
  std::string get_xpath_element_with_predicate() const;
  std::string get_rwrest_path_element() const;

  std::string get_proto_msg_new_typename() const;
  std::string get_proto_msg_typename() const;
  std::string get_proto_simple_typename() const;
  std::string get_pbc_simple_typename() const;
  std::string get_gi_simple_typename() const;

  bool is_message() const;
  bool is_field_inline() const;
  bool is_stringy() const;
  bool is_listy() const;
  bool is_leaf_list() const;
  bool is_optional() const;
  bool is_key() const;
  bool is_flat() const;
  bool has_list_descendants() const;
  bool has_op_data() const;
  bool has_keys() const;
  unsigned get_num_keys() const;

  uint32_t get_max_elements() const;
  uint32_t get_inline_max() const;
  uint64_t get_string_max() const;

  uint32_t get_event_id() const;

  bool has_default() const;
  std::string get_proto_default() const;
  std::string get_c_default_init() const;

  uint32_t get_pbc_size_warning_limit() const;
  uint32_t get_pbc_size_error_limit() const;

  PbFieldType get_pb_field_type() const;

  void child_add(PbNode* child_pnode);
  node_citer_t child_begin() const;
  node_citer_t child_end() const;
  node_citer_t child_last() const;

  /// Get this node's yang-derived tag number
  uint32_t get_tag_number();

  /**
   * Determine if another node is equivalent to this node.  Parsing
   * must be complete for both nodes.
   *
   * @return true if both nodes are equivalent.
   */
  bool is_equivalent(
    /// [in] The node to compare with this one.
    PbNode* other_pbnode,
    /**
     * [out] The reason why the nodes are not equivalent.  May be
     * nullptr if the caller does not care about the reason.  Not
     * meaningful if the nodes are equivalent - may be stomped even if
     * the nodes are equivalent.
     */
    const char** reason,
    /// The schema in which to compare messages
    PbModule* schema_pbmod = nullptr
  ) const;

  /// Add a module as expressing a direct augment of this node.
  void add_augment_mod(
    PbModule* pbmod);

  /**
   * Search for an extension.  Starts in the value, then tries the
   * type, and finally tries the node.
   *
   * @return The extension, if found. Otherwise nullptr.
   */
  YangExtension* find_ext(
    /// The extension's defining module
    const char* module,
    /// The extension name
    const char* ext
  );


public:

  /**
   * Populate the first-level of nodes, under the (module) root.  This
   * is a seprate step so that when augments get populated, the order
   * of the PbNodes will not differ from yang.
   */
  void populate_module_root();

  /**
   * Parse a root node.
   */
  void parse_root();

  /**
   * Parse a non-root node (a node that is a child of another node,
   * possibly the yang root node).
   */
  void parse_nonroot();

  /**
   * Parse rwpb yang extensions.  These are not processed for
   * yang root nodes.
   */
  void parse_extensions();

  /**
   * Parse the yang element.
   */
  void parse_element();

  /**
   * Parse the node's children (if any).
   */
  void parse_children();

  /**
   * Complete node parsing.
   */
  void parse_complete();


private:

  /// No-op parser function.
  parser_control_t parse_noop();

  /**
   * The status of the RW_YANG_EXT_PROTOBUF_FLD_INLINE extension is not
   * known.  Attempt to look it up.
   */
  parser_control_t parse_inline_unset();

  /// Select field-inline automatically.
  parser_control_t parse_inline_auto();

  /**
   * The status of the RW_YANG_EXT_PROTOBUF_FLD_INLINE_MAX extension is
   * not known.  Attempt to look it up.
   */
  parser_control_t parse_inline_max_unset();

  /**
   * The inline-max extension should be determined by yang.  Use the
   * max-elements specification, if any, to determine inline-max.
   */
  parser_control_t parse_inline_max_yang();

  /**
   * Attempt to detemine the maximum string length from the schema.  If
   * the encoding has not already been chosen, this will also select a
   * default encoding based on the leaf type.
   */
  parser_control_t parse_string_max_unset();

  /// Determine string length from yang.
  parser_control_t parse_string_max_yang();

  /// Verify a string max.
  parser_control_t parse_string_max_octet_value();

  /**
   * Handle conversion of a UTF-8 sized string-max to a char sized
   * string-max.
   */
  parser_control_t parse_string_max_utf8_value();

  /// Determine the protobuf field type.
  void parse_pb_field_type();

  /// Parser state handler for unset flat extension.
  parser_control_t parse_flat_unset();

  /**
   * Select the flat message extension automatically.  Basically, look
   * in the parent.  Defaults to bumpy if the parent has nothing to
   * say.
   */
  parser_control_t parse_flat_auto();


private:
  // Parser tables.

  /**
   * Determine the status of the inline extension.
   */
  enum class inline_t
  {
    ILLEGAL = 0, ///< none/illegal value
    UNSET, ///< has not been determined yet

    SEL_AUTO, ///< unspecified, or automatic
    SEL_PARENT_FLAT, ///< the parent node is flat
    SEL_TRUE, ///< make inline
    SEL_FALSE, ///< make pointy, dynamically allocated in C
  };

  /**
   * Select how a repeated element's repeat limit is determined.
   */
  enum class inline_max_t
  {
    ILLEGAL = 0, ///< none/illegal value
    UNSET, ///< has not been determined yet

    SEL_YANG, ///< use the limits found in yang
    SEL_NONE, ///< no limits, dynamically allocated in C
    SEL_VALUE, ///< a value was specified
  };

  /**
   * Select how a string-like leaf's length limit is determined.
   */
  enum class string_max_t
  {
    ILLEGAL = 0, ///< none/illegal value
    UNSET, ///< has not been determined yet

    SEL_YANG, ///< use the limits found in yang
    SEL_NONE, ///< no limits, dynamically allocated in C
    SEL_OCTET, ///< length is in terms of octets
    SEL_UTF8, ///< length is in terms of UTF8 characters
    SEL_VALUE, ///< an explicit length has been specified
    SEL_OCTET_VALUE, ///< value is specified, in terms of octets
    SEL_UTF8_VALUE, ///< value is specified, in terms of UTF8 characters
    SEL_OCTET_YANG, ///< take value from yang, in terms of octets
    SEL_UTF8_YANG, ///< take value from yang, in terms of UTF8 characters
  };

  /**
   * Determine the status of the flat extension.
   */
  enum class flat_t
  {
    ILLEGAL = 0, ///< none/illegal value
    UNSET, ///< has not been determined yet

    SEL_AUTO, ///< unspecified, or automatic
    SEL_TRUE, ///< make flat
    SEL_FALSE, ///< make bumpy, dynamically allocated in C
  };


  typedef parser_control_t (PbNode::*parse_node_pmf_t)();

  typedef std::unordered_map<std::string,inline_t> map_value_inline_t;
  typedef std::unordered_map<inline_t,parse_node_pmf_t,enum_hash<inline_t>> map_parse_inline_t;

  typedef std::unordered_map<std::string,inline_max_t> map_value_inline_max_t;
  typedef std::unordered_map<inline_max_t,parse_node_pmf_t,enum_hash<inline_max_t>> map_parse_inline_max_t;

  typedef std::unordered_map<std::string,string_max_t> map_value_string_max_t;
  typedef std::unordered_map<string_max_t,parse_node_pmf_t,enum_hash<string_max_t>> map_parse_string_max_t;

  typedef std::unordered_map<rw_yang_leaf_type_t,PbFieldType,std::hash<int>> map_yang_leaf_type_t;
  typedef std::unordered_map<std::string,PbFieldType> map_value_type_t;
  typedef std::pair<rw_yang_leaf_type_t,PbFieldType> type_allowed_pair_t;
  typedef std::unordered_set<type_allowed_pair_t,enum_pair_hash<type_allowed_pair_t>> type_allowed_t;
  typedef std::unordered_map<PbFieldType,std::string,enum_hash<PbFieldType>> map_string_pbftype_t;

  typedef std::unordered_map<std::string,flat_t> map_flat_value_t;
  typedef std::unordered_map<flat_t,parse_node_pmf_t,enum_hash<flat_t>> map_parse_flat_t;

  /// Lookup table for mapping inline extension values.
  static map_value_inline_t map_value_inline;

  /// Parser table for inline selection state.
  static map_parse_inline_t map_parse_inline;

  /// Lookup table for mapping inline-max extension values.
  static map_value_inline_max_t map_value_inline_max;

  /// Parser table for inline-max selection state.
  static map_parse_inline_max_t map_parse_inline_max;

  /// Lookup table for mapping string-max extension values.
  static map_value_string_max_t map_value_string_max;

  /// Parser table for string-max selection state.
  static map_parse_string_max_t map_parse_string_max;

  /// Parser table for mapping yang leaf type to protobuf field type.
  static map_yang_leaf_type_t map_yang_leaf_type;

  /// Lookup table for mapping protobuf field type name.
  static map_value_type_t map_value_type;

  /// Validation table for yang leaf and protobuf field type compatibility.
  static type_allowed_t type_allowed;

  /// Lookup table for mapping protobuf type to type name.
  static map_string_pbftype_t map_string_pbftype;

  /// Lookup table for mapping protobuf type to "C" type name.
  static map_string_pbftype_t map_string_pbctype;

  /// Lookup table for mapping protobuf type to default value.
  static map_string_pbftype_t map_default_pbftype;

  /// Table for mapping the value of the flat extension to a parser action
  static map_flat_value_t map_flat_value;

  /// Table for parsing the flat extension state
  static map_parse_flat_t map_parse_flat;


private:

  /// The owning parser.
  PbModel* pbmodel_ = nullptr;

  /**
   * The owning module.  The module where this node's declaring yang
   * statement exists.  For root nodes, this will be set to a
   * particular module that restricts children to be part of the
   * module.
   */
  PbModule* pbmod_ = nullptr;

  /**
   * The module most responsible for incorporating this node into the
   * schema at this node's specific path.  This module is either the
   * module that defined the node into the schema, the module that
   * contains the (more recent) uses that brought the node into the
   * schema, or the module that augmented the node into the schema.
   * Without schema_defining_pbmod_ having been imported, no node would
   * exist at the node's schema path.  This is in contrast to pbmod_,
   * which is the module that defined the base node (but not
   * necessarily added this specific path in the schema).  This also
   * contrasts with lci_pbmod_ - which is the module that minimally
   * includes all of this node's descendant nodes.
   */
  PbModule* schema_defining_pbmod_ = nullptr;

  /**
   * The Least-Common-Import module of all the modules in
   * augment_pbmods_.  This is the attribute that ultimately determines
   * whether or not a message gets (re)generated in the current schema
   * module, or whether the global schema refers to another schema's
   * definition.  This module will contain a (re)definition iff the LCI
   * is equal to the schema module.
   *
   * A message will be (re)generated in a schema where:
   *  - the message is originally defined in the schema module
   *  - the message is defined as a grouping by the schema module
   *  - the message is augmented by the schema module
   *  - a child message of the message is augmented by the schema module
   *  - the schema is the least common module where all the descendant
   *    nodes are visible together (the union of independent augment
   *    chains)
   */
  PbModule* lci_pbmod_ = nullptr;

  /**
   * The source modules of all fields, submessages, and submessage
   * fields (ATTN: but not types!), whether through direct expression,
   * uses, or augments.  Includes this node's defining module and the
   * using module (if applicable).
   */
  module_set_t reference_pbmods_;

  /**
   * All the modules that directly or indirectly augment this yang
   * node, through the module-level augment statement on this node, or
   * any of this node's descendant nodes.  Will not contain
   * get_pbmod(), even if it does augment, because in-the-same-module
   * augments are considered to be plain definitions by yangpbc (such
   * augments do not have proto-visible effects).  Will not contain any
   * transitive imports of get_pbmod(), even if an attempt is made to
   * add them.  All modules in this set will be added to the parent
   * node's set, as appropriate to that node's native pbmod.
   *
   * It makes no sense to track this for groupings.  In practice, for a
   * node directly part of a grouping definition, this set will only
   * contain the grouping's defining module.  The groupings' uses'
   * augments are not checked in the same way as module-level augments,
   * because they become a natural part of the data tree.  When a
   * grouping gets used, its contents will either be determined to be
   * equivalent to the groupings' contents (and therefore the message
   * will be reused), or they will be determined to be different (and
   * the grouping will not be reused).
   *
   * Does not really apply to leaf nodes.  The information will be
   * initialized when the node gets created, but it will never really
   * be used.
   */
  module_set_t augment_pbmods_;

  /**
   * The parent node, if any.  Does not necessary map to the same yang
   * node as ynode_->get_parent().
   */
  PbNode* parent_pbnode_ = nullptr;

  /**
   * The list of child nodes.
   */
  node_list_t child_pbnodes_;

  /// The corresponding yang schema node.
  YangNode* ynode_ = nullptr;

  /// Yang rwpb:msg-new extension, if any.
  YangExtension* yext_msg_new_ = nullptr;

  /// Yang rwpb:msg-name extension, if any.
  YangExtension* yext_msg_name_ = nullptr;

  /// Yang rwpb:msg-flat extension, if any.
  YangExtension* yext_msg_flat_ = nullptr;

  /// Yang rwpb:msg-proto-max-size extension, if any.
  YangExtension* yext_msg_proto_max_size_ = nullptr;

  /// Yang rwpb:sg-tag-base extension, if any.
  YangExtension* yext_msg_tag_base_ = nullptr;

  /// Yang rwpb:sg-tag-reserve extension, if any.
  YangExtension* yext_msg_tag_reserve_ = nullptr;

  /// Yang rwpb:fld-name extension, if any.
  YangExtension* yext_field_name_ = nullptr;

  /// Yang rwpb:fld-inline extension, if any.
  YangExtension* yext_field_inline_ = nullptr;

  /// Yang rwpb:fld-inline-max extension, if any.
  YangExtension* yext_field_inline_max_ = nullptr;

  /// Yang rwpb:fld-string-max extension, if any.
  YangExtension* yext_field_string_max_ = nullptr;

  /// Yang rwpb:fld-tag extension, if any.
  YangExtension* yext_field_tag_ = nullptr;

  /// Yang rwpb:fld-type extension, if any.
  YangExtension* yext_field_type_ = nullptr;

  /// Yang rwpb:fld-c-type extension, if any.
  YangExtension* yext_field_c_type_ = nullptr;

  /// Yang rw-notify-ext:log-common extension, if any.
  YangExtension* yext_notif_log_common_ = nullptr;

  /// Yang rw-notify-ext:log-event-id extension, if any.
  YangExtension* yext_notif_log_event_id_ = nullptr;

  /// Yang rwpb:field-merge-behavior extension, if any.
  YangExtension* yext_field_merge_behavior_ = nullptr;

  /// The node parse has started.
  bool parse_started_ = false;

  /// Parsing has been completed for this node.
  bool parse_complete_ = false;

  /// Is or has descendant nodes that are operational data (non-config).
  bool has_op_data_ = false;

  /// Is or has descendant nodes that are yang lists.
  bool has_list_descendants_ = false;

  /// The protobuf field type (if not a message).
  PbFieldType pb_field_type_ = PbFieldType::unset;

  /// If the field is an enumeration type
  PbEnumType* pbenum_ = nullptr;

  /// The state of inline extension selection.
  inline_t inline_ = inline_t::UNSET;

  /// The maximum number of elements, as determined from yang.
  uint32_t max_elements_ = 0;

  /// The state of inline extension selection.
  inline_max_t inline_max_ = inline_max_t::UNSET;

  /// The inline max limit.  Only valid when using the inline-max extension.
  uint32_t inline_max_limit_ = 0;

  /// The state of string extension selection.
  string_max_t string_max_ = string_max_t::UNSET;

  /// The string max limit.  Only valid when using the string-max extension.
  uint64_t string_max_limit_ = 0;

  /// Is the field optional?
  bool optional_ = true;

  /// The flat extension parser state.
  flat_t flat_ = flat_t::UNSET;

  /// The maximum size detection state.
  bool pbc_size_max_set_ = false;

  /// The maximum allowable protobuf-c struct size.
  uint32_t pbc_size_limit_ = PBC_SIZE_MAX_DEFAULT;

  /// The log event ID. ATTN: TGS: I don't think the implementation makes much sense
  uint32_t notif_log_event_id_ = 0;
};


/**
 * This class defines a yang module being parsed for conversion to
 * protobuf.
 */
class PbModule
{
public:
  // Utility types

  typedef PbEnumType::enum_type_uptr_t enum_type_uptr_t;
  typedef PbEnumType::enum_type_list_t enum_type_list_t;

  typedef PbEqMsgSet::eq_list_t eq_list_t;

  typedef PbMessage::msg_uptr_t msg_uptr_t;

  /// Unique pointer for memory management
  typedef std::unique_ptr<PbModule> module_uptr_t;

  /// List of modules for memory management; owned by PbModel.
  typedef std::list<module_uptr_t> module_owned_list_t;

  /// Yang to PB module mapping
  typedef std::unordered_map<YangModule*,PbModule*> module_ymod_lookup_t;

  /// Symbol table for modules. Used for ns and name lookups
  typedef PbSymTab<PbModule*> module_stab_t;

  /// Module set: used for recording module relationships that have no ordering
  typedef std::unordered_set<PbModule*> module_set_t;

  /// Module list: used for recording module relationships that have an order
  typedef std::list<PbModule*> module_list_t;

  /// The include file list to be added in the generated pb-c.h
  typedef std::list<std::string> pbc_include_files_t;


public:
  /**
   * Construct a protobuf module.
   */
  PbModule(
    /// The owning parser
    PbModel* pbmodel,
    /// The corresponding yang module
    YangModule* ymod
  );

  // Cannot copy
  PbModule(const PbModule&) = delete;
  PbModule& operator=(const PbModule&) = delete;


public:

  PbModel* get_pbmodel() const;
  YangModule* get_ymod() const;
  rw_namespace_id_t get_nsid() const;

  /**
   * Calculates the local least common importer for
   * the module. Does not take into account the
   * augmenting modules
   */
  PbModule* get_local_lci_pbmod();
  PbModule* get_lci_pbmod();
  PbNode* get_root_pbnode() const;

  PbMessage* get_data_module_pbmsg() const;
  unsigned has_data_module_fields() const;
  unsigned has_data_module_messages() const;
  PbMessage* get_rpci_module_pbmsg() const;
  PbMessage* get_rpco_module_pbmsg() const;
  unsigned has_rpc_messages() const;
  PbMessage* get_notif_module_pbmsg() const;
  unsigned has_notif_module_messages() const;
  PbMessage* get_group_root_pbmsg() const;
  unsigned has_group_root_messages() const;

  bool is_import_needed() const;
  void need_to_import();

  bool has_top_level() const;
  bool has_schema() const;
  bool is_local();

  const std::string& get_proto_package_name() const;
  std::string get_proto_typename();
  std::string get_proto_fieldname();

  void enum_add(enum_type_uptr_t pbenum);

  void save_top_level_grouping(
    PbNode* pbnode,
    PbMessage* pbmsg);
  void find_grouping_order();

  /// Walks through the Module hierarchy and invokes visitor callbacks on every
  /// PbMessage/Module visit.
  bool traverse(
    /// Visitor on which the visit callbacks will be invoked
    PbModelVisitor* visitor
  );

public:
  /**
   * Populate the list of modules directly imported by this module.
   */
  void populate_direct_imports();

  /**
   * Populate the list of modules directly or indirectly imported by
   * this module.
   */
  void populate_transitive_imports(
    /**
     * [in] The next module to populate.  Populate the imports of this
     * module that have not already been populated, recursively.
     */
    PbModule* other_pbmod
  );

  /**
   * Determine the Least-Common-Importer.  The LCI is the/a module that
   * imports both this module and the other module.  It is least common
   * in that the answer excludes modules that also import other modules
   * that import both this and other.  The answer is not unique -
   * multiple modules can be considered LCI for a pair of modules.
   */
  PbModule* least_common_importer(
    /**
     * [in] The other module to check.
     */
    PbModule* other
  );

  bool imports_direct(PbModule* pbmod);
  bool imports_transitive(PbModule* pbmod);
  bool equals_or_imports_direct(PbModule* pbmod);
  bool equals_or_imports_transitive(PbModule* pbmod);
  bool is_ordered_before(
    PbModule* other_pbmod,
    PbModule* schema_pbmod = nullptr);

  /**
   * Add a module to the list of the other protobuf modules this module
   * must import in protobuf, because a type is used from the import.
   */
  void import_proto(
    /**
     * [in] The other module to import.
     */
    PbModule* other
  );

  /**
   * Add the augmenting module to the
   * augment_pbmods_ set
   */
  void add_augment_pbmod(
    PbModule* other
  );

  /// Create a yangpbc generated C variable or function name
  std::string get_ypbc_global(
    /// Identifier fragment to discriminate symbols
    const char* discriminator);

  /// Find all the nodes in the module that have been augmented.
  void find_augmented_nodes();

  /// Find and mark all the equivalent enum types.
  void find_equivalent_enums();

  /// Parse the module-level statements
  void parse_start();

  /**
   * Parse all the nodes in the yang model.
   */
  void parse_nodes();

  /**
   * Parse all the nodes into messages.
   */
  void parse_messages();

  /**
   * Find all the schema imports needed for the module.
   */
  void find_schema_imports();


public:
  /// Output the proto file options.
  void output_proto_file_options(std::ostream& os, unsigned indent);

  /// Output the proto file top-level enums.
  void output_proto_enums(std::ostream& os, unsigned indent);

  /// Output the proto module data message and hierarchy.
  void output_proto_data_module(std::ostream& os, unsigned indent);

  /// Output the proto rpc input message definitions and hierarchy.
  void output_proto_rpci_module(std::ostream& os, unsigned indent);

  /// Output the proto rpc output message definitions and hierarchy.
  void output_proto_rpco_module(std::ostream& os, unsigned indent);

  /// Output the proto notification message definitions and hierarchy.
  void output_proto_notif_module(std::ostream& os, unsigned indent);

  /// Output the proto grouping message definitions and hierarchy.
  void output_proto_group_root(std::ostream& os, unsigned indent);

  /// Output the proto schema message definitions and hierarchy.
  void output_proto_schema(PbMessage::output_t output_style, std::ostream& os, unsigned indent);


  /// Output the C header file top-level enums.
  void output_h_enums(std::ostream& os, unsigned indent);

  /// Output the C header file struct tags.
  void output_h_struct_tags(std::ostream& os, unsigned indent);

  /// Output the C header file schema constants struct definitions.
  void output_h_structs(std::ostream& os, unsigned indent);

  /// Output meta data macros.
  void output_h_meta_data_macros(std::ostream& os, unsigned indent);


  /// Output the C++ file forward declarations.
  void output_cpp_forward_decls(std::ostream& os, unsigned indent);

  /// Output the C++ file top-level enums.
  void output_cpp_enums(std::ostream& os, unsigned indent);

  /// Output the C++ file schema constants definitions.
  void output_cpp_descs(std::ostream& os, unsigned indent);


  /// Output the GI header declarations.
  void output_gi_h(std::ostream& os, unsigned indent);


  /// Output the GI C definitions.
  void output_gi_c(std::ostream& os, unsigned indent);

  /// Output the proto data module msg as field in schema root msg
  void output_proto_data_module_as_field(std::ostream& os, unsigned indent);

  /// Output the proto rpci module msg as field in schema root msg
  void output_proto_rpci_module_as_field(std::ostream& os, unsigned indent);

  /// Output the proto rpco module msg as field in schema root msg
  void output_proto_rpco_module_as_field(std::ostream& os, unsigned indent);

  /// Output the proto notif module msg as field in schema root msg
  void output_proto_notif_module_as_field(std::ostream& os, unsigned indent);


  /// Output the documentation for a module.
  bool output_doc(
    const doc_file_t file_type,
    std::ostream& os,
    unsigned indent,
    PbMessage::doc_t doc_style,
    unsigned chapter );


private:

  /// The owning parser.
  PbModel* pbmodel_ = nullptr;

  /// The corresponding yang module.
  YangModule* ymod_ = nullptr;

  /**
   * The module root node.  Contains all top-level nodes, plus all
   * top-level groupings.
   */
  PbNode::node_uptr_t root_pbnode_;

  /// The package.YangData.MODULE message.
  PbMessage* data_module_pbmsg_;

  /// The package.YangInput.MODULE message.
  PbMessage* rpci_module_pbmsg_;

  /// The package.YangOutput.MODULE message.
  PbMessage* rpco_module_pbmsg_;

  /// The package.YangNotif.MODULE message.
  PbMessage* notif_module_pbmsg_;

  /// The MODULE.YangGroup message.
  PbMessage* group_root_pbmsg_;

  /// The package name, as output in the package statement (if any).
  std::string proto_package_name_;

  /// The ypbc global identifier base.
  std::string ypbc_global_base_;

  /// Yang Module has package name extension, if any.
  YangExtension* yext_package_name_ = nullptr;

  /// The set of modules that this module imports.
  module_set_t direct_imports_;

  /// The set of modules that this module imports, whether directly or not.
  module_set_t transitive_imports_;

  /// The set of modules that import this module, whether directly or not.
  module_set_t transitive_importers_;

  /// The set of augmenting modules
  module_set_t augment_pbmods_;

  /// The global namespace id for this module.
  rw_namespace_id_t nsid_ = RW_NAMESPACE_ID_NULL;

  /**
   * All the enums owned by this module, they will be defined in the
   * module's proto package
   */
  enum_type_list_t enum_type_list_;

  /**
   * The list of groupings in this module.  During primary node
   * parsing, this list is unordered.  After all the nodes have been
   * parsed, this list will be reordered such that groupings appear in
   * a reference-safe order.  When ordered, no grouping will appear in
   * the list before any of the groupings it references (directly or
   * indirectly).  As such, it would be suitable for use in C struct
   * definitions without needing forward declarations.
   */
  eq_list_t ordered_grouping_pbeqs_;

  /**
   * List of files to be included in the generated pb-c.h
   */
  pbc_include_files_t pbc_inc_files_;

  /**
   * If true, need to import the proto module into the schema proto
   * module.
   */
  bool need_to_import_ = false;

  PbModule* lci_pbmod_ = nullptr;

  PbModule* lci_local_pbmod_ = nullptr;
};


/**
 * This class converts a yang schema to protobuf, by using hints stored
 * in yang extensions.  The parser walks the entire module, searching
 * for objects to convert to protobufs.
 *
 * This object builds two parallel trees, parallel to the yang model
 * tree.  First, it builds a node tree that maps directly to the yang
 * model tree.  The node tree keeps track of general state information
 * that may or may not be needed if a protobuf conversion is needed.
 * If a protobuf conversion extension is found in the node tree, the
 * converter will also build a message tree, parallel to the node tree,
 * but only including those nodes that belong in the protobuf.  The
 * message tree contains an additional set of leaves for the fields
 * within the protobufs - these map to yang values, or to other yang
 * nodes.
 *
 * The parser object ultimately owns all the other conversion objects,
 * whether directly, or indirectly.  However, it does not own the yang
 * model (which allows the loaded yang modules to be be reused without
 * having to reparse them).  All conversion data structures will live
 * until this object is destroyed.
 */
class PbModel
{
public:
  // Utility types

  // Unique pointer for memory management
  typedef std::unique_ptr<PbModel> ptr_t;

  typedef PbModule::module_list_t::const_iterator module_citer_t;

  // For generating unique static variable names.
  typedef std::set<std::string> static_vars_t;

  typedef std::unordered_map<std::string, uint32_t> name_instance_map_t;

  typedef AppDataToken<PbNode*,AppDataTokenDeleterNull> adt_pbnode_t;

public:
  /**
   * Mangle a raw yang name into a raw protobuf name.  Basically
   * equates to replacing '-' and '.' with '_'.
   *
   * @return The mangled name.
   */
  static std::string mangle_identifier(
    /// The yang identifier
    const char* id
  );
  static std::string mangle_identifier(const std::string& id) { return mangle_identifier(id.c_str()); }

  /**
   * Mangle a raw yang name into a protobuf typename.  Removes '-',
   * '.', and '_', and camel-cases the resulting identifier.
   *
   * @return The mangled name.
   */
  static std::string mangle_typename(
    /// The yang identifier
    const char* id
  );
  static std::string mangle_typename(const std::string& id) { return mangle_typename(id.c_str()); }

  /**
   * Expects a Camelcase C/C++ identifier as argument.
   * Mangle a Camel case identifier to an underscore identifier with lower case.
   * All uppercase letters in the passed string are converted to lowercase
   * and prefixed with '_'. The fisrt character when uppercase is converted to
   * lower without the '_' prefix. Other valid characters are unchanged.
   *
   * @return The mangled name.
   */
  static std::string mangle_to_underscore(
    /// The C Camelcase identifier
    const char* id
  );
  static std::string mangle_to_underscore(const std::string& id) { return mangle_to_underscore(id.c_str()); }

  /**
   * Mangle to uppercase.
   *
   * @return The mangled name.
   */
  static std::string mangle_to_uppercase(
    /// The C/C++ identifier to mangle
    const char* id
  );
  static std::string mangle_to_uppercase(const std::string& id) { return mangle_to_uppercase(id.c_str()); }

  /**
   * Mangle to lowercase.
   *
   * @return The mangled name.
   */
  static std::string mangle_to_lowercase(
    /// The C/C++ identifier to mangle
    const char* id
  );
  static std::string mangle_to_lowercase(const std::string& id) { return mangle_to_lowercase(id.c_str()); }

  /**
   * Check that a string is a valid protobuf identifier.
   * @return true if a valid identifier. Otherwise false.
   */
  static bool is_identifier(
    /// The identifier to check
    const char* id
  );
  static bool is_identifier(const std::string& id) { return is_identifier(id.c_str()); }

  /**
   * Check that a string is a valid protobuf typename.
   * @return true if a valid typename. Otherwise false.
   */
  static bool is_proto_typename(
    /// The typename to check
    const char* id
  );
  static bool is_proto_typename(const std::string& id) { return is_proto_typename(id.c_str()); }

  /**
   * Check that a string is a valid protobuf fieldname.
   * @return true if a valid fieldname. Otherwise false.
   */
  static bool is_proto_fieldname(
    /// The fieldname to check
    const char* id
  );
  static bool is_proto_fieldname(const std::string& id) { return is_proto_fieldname(id.c_str()); }

  /**
   * Initialize the static parsing tables.
   */
  static void init_tables();

public:
  /// Create a yang to protobuf converter.
  PbModel(
    /// The yang model to use.
    YangModel* model
  );

  // cannot copy
  PbModel(const PbModel&) = delete;
  PbModel& operator=(const PbModel&) = delete;

public:
  // accessors

  YangModel* get_ymodel() const;
  PbModule* get_schema_pbmod() const;
  NamespaceManager& get_ns_mgr() const;
  const adt_pbnode_t get_pbnode_adt() const;

  PbEqMgr* get_pbeqmgr();

  const std::string& get_proto_schema_package_name() const;

  unsigned get_errors() const;
  unsigned get_warnings() const;
  bool is_failed() const;

  module_citer_t modules_begin() const;
  module_citer_t modules_end() const;

  std::string get_gi_c_identifier_base();
  std::string get_gi_c_identifier(
    const char* operation = nullptr);
  std::string get_gi_c_global_func_name(
    const char* operation = nullptr);

  /// Walks through the Model hierarchy and invokes visitor callbacks on every
  /// PbMessage/Module visit.
  bool traverse(
    /// Visitor on which the visit callbacks will be invoked
    PbModelVisitor* visitor
  );

public:

  /// Set the package name.  Must be valid protobuf package name
  /// @return true on success, false on failure
  void set_schema(
    /// The module that is being compiled
    PbModule* pbmod
  );

  /// Load a module.
  /// @return The module pointer.  nullptr on error.
  PbModule* module_load(
    /// The path to the yang file (path is stripped, uses libncx search path).
    const char* yang_filename
  );

  /**
   * Find a pbmod, given the YangModule.
   *
   * @return The module, if found.  nullptr if not found.
   */
  PbModule* module_lookup(
    /// [in] The corresponding yang module
    YangModule* ymod
  );

  /**
   * Find a pbmod, given the ns string
   *
   * @return The module, if found.  nullptr if not found.
   */
  PbModule* module_lookup(
    /// [in] The namespace string
    std::string ns
  );

  /// Determine if a is ordered before b in the schema.
  bool module_is_ordered_before(
    PbModule* a_pbmod,
    PbModule* b_pbmod) const;

  /// Walk an augment node back to its module root, creating a proper
  /// PbNode hierarchy
  void populate_augment_root(
    /// The yang node to walk back
    YangNode* ynode,
    /// [out] The node found/created
    PbNode** pbnode
  );

  /**
   * Find a pbnode, given the YangNode, allocating it if it cannot be
   * found.  The node is owned by the model and will be destroyed when
   * the model is destroyed.
   *
   * @return The node.  Throws on failure.
   */
  PbNode* get_node(
    /// [in] The corresponding yang node
    YangNode* ynode,
    /// [in] The parent node, if any.  nullptr if ynode's parent is root
    PbNode* pbparent
  );

  /**
   * Find a pbnode, given the YangNode.  In contrast to get_node(),
   * this function does not allocate the node if it does not exist.
   *
   * @return The node, if found.  nullptr if not found.
   */
  PbNode* lookup_node(
    /// [in] The corresponding yang node
    YangNode* ynode
  );

  /**
   * Allocate a message descriptor.  The message is owned by the model.
   *
   * @return The new message.
   */
  PbMessage* alloc_pbmsg(
    /// [in] The associated node.
    PbNode* pbnode,
    /**
     * [in] The owning message.  May be nullptr, but that should only
     * be for messages owned by the module itself.
     */
    PbMessage* parent_pbmsg,
    /**
     * [in] The type of message.  This controls parsing and output.
     */
    PbMsgType pb_msg_type = PbMsgType::automatic
  );

  /**
   * Search for a enum type by yang typedef'ed enum type.  If it is not
   * yet known, create it.
   */
  PbEnumType* enum_get(
    /// The typedef'ed enum type to lookup.
    YangTypedEnum* ytenum,
    /// The module that defines the typedef.
    PbModule* pbmod
  );

  /**
   * Search for a enum type by yang type.  If it is not yet known,
   * create it.
   */
  PbEnumType* enum_get(
    /// The enum type to lookup
    YangType* ytype,
    /// The field associated with the enum.
    PbNode* pbnode
  );

  /// Save a enum type by the yang type
  void enum_add(
    /// The enum type to save
    PbEnumType* pbenum
  );

  /// Handle an error.
  void error(
    /// The source location of the error
    const std::string& location,
    /// The text of the message
    const std::string& text
  );

  /// Handle a warning.
  void warning(
    /// The source location of the warning
    const std::string& location,
    /// The text of the message
    const std::string& text
  );

  /// Create a yangpbc generated C variable or function name
  std::string get_ypbc_global(
    /// Identifier fragment to discriminate symbols
    const char* discriminator
  );

  /// Generate a C identifier name.  If not unique, a number of the
  /// form _X will be appended.
  /// @return The unique variable name.
  std::string register_c_global(
    /// The preferred name.
    std::string name
  );

  /**
   * Parse all the eligible nodes, looking for protobuf conversions.
   * Generated protobuf messages for any nodes that are found.
   */
  void parse_nodes();

  /// Find and mark all the equivalent enum types.
  void find_equivalent_enums();

  /**
   * Save a potential top-level message equivalence set.  It is
   * possible that the set will be suppressed in the end.
   */
  void save_top_level_equivalence(
    /// The message equivalence set to save.
    PbEqMsgSet* pbeq);

  /// Find and mark all the equivalent enum types.
  void find_top_level_order();



public:

  /// Output the protobuf file
  parser_control_t output_proto(
    /// The output base filename.  Must be able to create or write basename.proto.
    const char* basename
  );

  /// Output the proto-based schema hiearchy.
  parser_control_t output_proto_schema(
    /// The stream to emit the schema on.
    std::ostream& os
  );

  /// Output the C source file
  parser_control_t output_cpp_file(
    /// The output base filename.  Must be able to create or write basename.ypbc.c.
    const char* basename
  );

  /// Output the schema struct hiearchy to the c file
  parser_control_t output_cpp_schema(
    /// The stream to emit the schema on.
    std::ostream& os
  );

  // Ouput the C header file
  parser_control_t output_h_file(
    /// The output base filename.  Must be able to create or write basename.h.
      const char* basename
  );

  /// Output the GI C source file
  parser_control_t output_gi_c_file(
    /// The output base filename.  Must be able to create or write basename.gi.c.
    const char* basename
  );

  /// Outputs a enumeration gi helper decl
  void output_gi_enumeration_helper_decl (
    /// [in] stream to emit the enumeration on
    std::ostream& os,
    /// [in] intendation for the enumeration in the proto file
    unsigned indent
  );

  /// Outputs enumeration gi helper decl
  void output_gi_enumeration_helper_decl (
    /// stream to emit enumeration on
    std::ostream&os,
    /// a pointer to the named type
    const char *name,
    /// start of value iterator
    YangValueIter start,
    /// end of value iterator
    YangValueIter end,
    /// required intendation
    unsigned indent
  );

  /// Outputs a enumeration gi helper def
  void output_gi_enumeration_helper_def (
    /// [in] stream to emit the enumeration on
    std::ostream& os,
    /// [in] intendation for the enumeration in the proto file
    unsigned indent
  );

  /// Outputs enumeration gi helper def
  void output_gi_enumeration_helper_def (
    /// stream to emit enumeration on
    std::ostream&os,
    /// a pointer to the named type
    const char *name,
    /// start of value iterator
    YangValueIter start,
    /// end of value iterator
    YangValueIter end,
    /// required intendation
    unsigned indent
  );

  /// Gets the unique GI namespace prefix
  std::string get_gi_prefix(const char* discriminator) const;

  /// Output the schema root msg descs
  void output_cpp_descs(std::ostream& os, unsigned indent, PbMessage::output_t output_style);

  /// Output the schema root msg desc structs
  void output_h_structs(std::ostream& os, unsigned indent, PbMessage::output_t output_style);

  /// Support function to get the pbc typename/identifier name
  std::string get_pbc_message_global(const char* discriminator, PbMessage::output_t output_style);

  /// Output the keyspec and path entry global values
  void output_cpp_schema(std::ostream& os, unsigned indent, PbMessage::output_t output_style);

  /// Support function to get the ypbc typename/identifier name
  std::string get_pbc_schema_typename(PbMessage::output_t output_style);


  /// Output the user documentation file
  parser_control_t output_doc_user_file(
    /// The output file type
    const doc_file_t file_type,
    /// The output base filename.  Must be able to create or write basename.user.txt
    const char* basename
  );

  /// Output the API documentation file
  parser_control_t output_doc_api_file(
    /// The output file type
    const doc_file_t file_type,
    /// The output base filename.  Must be able to create or write basename.api.txt
    const char* basename
  );

  /// Output a documentation table of contents or heading.
  void output_doc_heading(
    const doc_file_t file_type,
    std::ostream& os,
    unsigned indent,
    PbMessage::doc_t doc_style,
    const std::string& chapter,
    const std::string& title );

  /// Output a documentation entry.
  void output_doc_field(
    const doc_file_t file_type,
    std::ostream& os,
    unsigned indent,
    const std::string& chapter,
    const std::string& doc_key,
    const std::string& entry_type,
    const std::string& entry_data );

private:

  /// The yang model to use when loading modules
  YangModel* ymodel_ = nullptr;

  /// The owned modules
  PbModule::module_owned_list_t modules_;

  /// The mapping of YangModule to module
  PbModule::module_ymod_lookup_t modules_by_ymod_;

  /// The mapping of module namespace to module
  PbModule::module_stab_t modules_by_ns_;

  /// The modules in dependency order; modules with no dependencies come first
  PbModule::module_list_t modules_by_dependencies_;

  /// The proto package name
  std::string proto_schema_package_name_;

  /// The proto module name converted to a proto message name
  std::string proto_module_typename_;

  /// The stem of a proto path to a global-schema message/enum
  std::string proto_schema_path_stem_;

  /// The C global message list variable name
  // ATTN: deprecated: remove me in favor of schemaconst_schema
  std::string msglist_var_;

  /// Fail parse on warnings.
  bool werror_ = false; // ATTN: Need argv support

  /// Ignore warnings.
  bool nowarn_ = false; // ATTN: Need argv support

  /// The conversion has failed
  bool failed_ = false;

  /// The number of errors detected.
  unsigned errors_ = 0;

  /// The number of warnings detected.
  unsigned warnings_ = 0;

  /// A mapping of yang-node to pb-node, for lookups when walking the schema tree backwards.
  PbNode::node_ynode_lookup_t node_map_;

  /// The list of top-level message equivalence sets.
  PbEqMsgSet::eq_list_t top_level_pbeqs_;

  /// The set of already-generated static variable names.
  static_vars_t static_vars_;

  /// Maps yang type to protobuf enum type
  /// ATTN: This should be AppData.
  PbEnumType::enum_ytype_map_t enum_ytype_map_;

  /// Maps yang typedefed enum to protobuf enum type
  /// ATTN: This should be AppData.
  PbEnumType::enum_ytenum_map_t enum_ytenum_map_;

  /// name space manager reference initiliazed in  constructor
  NamespaceManager& ns_mgr_;

  /// App data token for binding a PbNode to a YangNode.
  adt_pbnode_t pbnode_adt_;

  /// The module that represents the schema.
  PbModule* schema_pbmod_ = nullptr;

  /// The equivalence set manager
  PbEqMgr pbeqmgr_;

  /// The equivalence set manager
  PbMessage::msg_owned_list_t owned_pbmsgs_;

  /// The equivalence set manager
  PbNode::node_owned_list_t owned_pbnodes_;

  /// There are modules with data.
  bool has_data_ = false;

  /// There are modules with RPCs.
  bool has_rpcs_ = false;

  /// There are modules with notifications.
  bool has_notif_ = false;
};

/// Visitor Interface pattern, to walk through the Model hierarchy.
/// Implementation can use the callbacks to generate code or documentation out
/// of the Yang hierarchy.
class PbModelVisitor
{
 public:
  virtual ~PbModelVisitor() {};

  /// Invoked when a Module is traversed.
  virtual bool visit_module_enter(PbModule* module) = 0;

  // Invoked when a Module traversal is done.
  virtual bool visit_module_exit(PbModule* module) = 0;

  // Invoked when a PbMessage is traversed.
  virtual bool visit_message_enter(PbMessage* msg) = 0;

  // Invoked when a PbMessage traversal is done.
  virtual bool visit_message_exit(PbMessage* msg) = 0;
};


template <class T>
inline size_t enum_hash<T>::operator()(const T& k) const
{
  std::hash<int> h;
  return h(static_cast<int>(k));
}

template <class P>
inline size_t enum_pair_hash<P>::operator()(const P& k) const
{
  std::hash<int> h;
  return h(static_cast<int>(k.first)) ^ h(static_cast<int>(k.second));
}

template <class P, class A, class B>
inline size_t pair_hash<P,A,B>::operator()(const P& k) const
{
  return   std::hash<A>()(*static_cast<const A*>(&k.first))
         ^ std::hash<B>()(*static_cast<const B*>(&k.second));
}


template <typename value_type>
value_type PbSymTab<value_type>::add_symbol(
  std::string symbol,
  value_t value)
{
  RW_ASSERT(symbol.length());

  auto status = table_.emplace(std::make_pair(symbol, value));
  if (status.second) {
    return value_t{};
  }
  return status.first->second;
}

template <typename value_type>
std::string PbSymTab<value_type>::add_symbol_make_unique(
  std::string symbol,
  value_t value)
{
  RW_ASSERT(symbol.length());
  std::string unique_symbol = symbol;

  unsigned suffix = 0;
  while(1) {
    auto symi = table_.find(unique_symbol);
    if (symi == table_.end()) {
      table_.emplace(std::make_pair(unique_symbol, value));
      return unique_symbol;
    }

    // Append a suffix for uniqueness
    char buf[16];
    int bytes = snprintf(buf, sizeof(buf), "_%u", ++suffix);
    RW_ASSERT(bytes>1 && (unsigned)bytes<sizeof(buf));
    unique_symbol = symbol + &buf[0];
    RW_ASSERT(suffix < 10000); // for sanity
  }
}

template <typename value_type>
value_type PbSymTab<value_type>::add_symbol_replace(
  std::string symbol,
  value_t value)
{
  RW_ASSERT(symbol.length());

  value_t retval{};
  auto symi = table_.find(symbol);
  if (symi != table_.end()) {
    retval = symi->second;
    table_.erase(symi);
  }

  auto status = table_.emplace(std::make_pair(symbol, value));
  RW_ASSERT(status.second);
  return retval;
}

template <typename value_type>
bool PbSymTab<value_type>::has_symbol(
  std::string symbol) const
{
  RW_ASSERT(symbol.length());
  auto symi = table_.find(symbol);
  return symi != table_.end();
}

template <typename value_type>
value_type PbSymTab<value_type>::get_symbol(
  std::string symbol) const
{
  auto symi = table_.find(symbol);
  if (symi == table_.end()) {
    return value_t{};
  }
  return symi->second;
}

template <typename value_type>
bool PbSymTab<value_type>::delete_symbol(
  std::string symbol)
{
  return 1 == table_.erase(symbol);
}


static inline void PARSE_CONTROL_SKIP_TO_RETVAL_NO_REDO(parser_control_t pc, parser_control_t& retval)
{
  switch (pc) {
    case parser_control_t::DO_NEXT:
      break;
    case parser_control_t::DO_SKIP_TO_SIBLING:
      retval = pc;
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }
}

static inline void PARSE_CONTROL_ERR_TO_RETVAL(parser_control_t pc, parser_control_t& retval)
{
  switch (pc) {
    case parser_control_t::DO_NEXT:
      break;
    case parser_control_t::DO_SKIP_TO_SIBLING:
    case parser_control_t::DO_REDO:
      retval = pc;
      break;
    default:
      RW_ASSERT_NOT_REACHED();
  }
}

#define PARSE_CONTROL_SKIP_RETURN_NEXT_NO_REDO(pc_) \
  do { \
    switch (pc_) { \
      case parser_control_t::DO_NEXT: \
        break; \
      case parser_control_t::DO_SKIP_TO_SIBLING: \
        return parser_control_t::DO_NEXT; \
      default: \
        RW_ASSERT_NOT_REACHED(); \
    } \
  } while(0)

#define PARSE_CONTROL_REDO_CONT_NEXT_BREAK_SKIP_TO_RETVAL(pc_, retval_) \
  do { \
    switch (pc_) { \
      case parser_control_t::DO_REDO: \
        continue; \
      case parser_control_t::DO_NEXT: \
        break; \
      case parser_control_t::DO_SKIP_TO_SIBLING: \
        retval_ = pc_; \
        break; \
      default: \
        RW_ASSERT_NOT_REACHED(); \
    } \
  } while(0)

static inline uint64_t stmt_mask_data()
{
  return
      (uint64_t(1)<<rw_yang_stmt_type_to_index(RW_YANG_STMT_TYPE_ANYXML))
    | (uint64_t(1)<<rw_yang_stmt_type_to_index(RW_YANG_STMT_TYPE_CONTAINER))
    | (uint64_t(1)<<rw_yang_stmt_type_to_index(RW_YANG_STMT_TYPE_LEAF))
    | (uint64_t(1)<<rw_yang_stmt_type_to_index(RW_YANG_STMT_TYPE_LEAF_LIST))
    | (uint64_t(1)<<rw_yang_stmt_type_to_index(RW_YANG_STMT_TYPE_LIST))
    | (uint64_t(1)<<rw_yang_stmt_type_to_index(RW_YANG_STMT_TYPE_CHOICE))
    | (uint64_t(1)<<rw_yang_stmt_type_to_index(RW_YANG_STMT_TYPE_CASE))
    | (uint64_t(1)<<rw_yang_stmt_type_to_index(RW_YANG_STMT_TYPE_RPCIO));
}

static inline uint64_t stmt_mask_grouping()
{
  return (uint64_t(1)<<rw_yang_stmt_type_to_index(RW_YANG_STMT_TYPE_GROUPING));
}


static inline uint64_t stmt_mask_rpc()
{
  return (uint64_t(1)<<rw_yang_stmt_type_to_index(RW_YANG_STMT_TYPE_RPC));
}

static inline uint64_t stmt_mask_notification()
{
  return (uint64_t(1)<<rw_yang_stmt_type_to_index(RW_YANG_STMT_TYPE_NOTIF));
}


/** @} */

} // namespace rw_yang

#endif // RW_YANGTOOLS_YANGPBC_HPP__
