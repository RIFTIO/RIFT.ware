/* STANDARD_RIFT_IO_COPYRIGHT */

/*!
 * @file yangmodel.h
 *
 * Top-level include for Rift yang model.
 */

#ifndef RW_YANGTOOLS_YANGMODEL_H_
#define RW_YANGTOOLS_YANGMODEL_H_

// Need this because doxygen parser thinks this is a C file and misses the C++ definitions
#ifdef RW_DOXYGEN_PARSING
#define __cplusplus 201103
#endif

#include <sys/cdefs.h>
#include <protobuf-c/rift-protobuf-c.h>
#include "rwlib.h"
#include "yangpb_gi.h"
#include "yangmodel_gi.h"
#include <stdbool.h>

#if __cplusplus

#include <string>
#include <iterator>
#include <memory>
#include <vector>
#include <unordered_map>

#include "rw_app_data.hpp"
#if __cplusplus < 201103L
#error "Requires C++11"
#endif

#endif

/*!
 * @defgroup YangModel A Yang Object library
 * @{
 */

__BEGIN_DECLS


static inline bool_t rw_yang_stmt_type_is_good(rw_yang_stmt_type_t v)
{
  return    (int)v > (int)RW_YANG_STMT_TYPE_FIRST
         && (int)v < (int)RW_YANG_STMT_TYPE_LAST;
}
static inline bool_t rw_yang_stmt_type_is_good_or_null(rw_yang_stmt_type_t v)
{
  return    v == RW_YANG_STMT_TYPE_NULL
         || (    (int)v > (int)RW_YANG_STMT_TYPE_FIRST
              && (int)v < (int)RW_YANG_STMT_TYPE_LAST);
}
static inline size_t rw_yang_stmt_type_to_index(rw_yang_stmt_type_t v)
{
  return (size_t)((int)v - (int)RW_YANG_STMT_TYPE_FIRST);
}
static inline rw_yang_stmt_type_t rw_yang_stmt_type_from_index(size_t i)
{
  return (rw_yang_stmt_type_t)(i + (size_t)RW_YANG_STMT_TYPE_FIRST);
}

const char* rw_yang_stmt_type_string(rw_yang_stmt_type_t v);




static inline bool_t rw_yang_leaf_type_is_good(rw_yang_leaf_type_t v)
{
  return    (int)v > (int)RW_YANG_LEAF_TYPE_FIRST
         && (int)v < (int)RW_YANG_LEAF_TYPE_LAST;
}

static inline bool_t rw_yang_leaf_type_is_good_or_null(rw_yang_leaf_type_t v)
{
  return    v == RW_YANG_LEAF_TYPE_NULL
         || (    (int)v > (int)RW_YANG_LEAF_TYPE_FIRST
              && (int)v < (int)RW_YANG_LEAF_TYPE_LAST);
}
static inline size_t rw_yang_leaf_type_to_index(rw_yang_leaf_type_t v)
{
  return (size_t)((int)v - (int)RW_YANG_LEAF_TYPE_FIRST);
}
static inline rw_yang_leaf_type_t rw_yang_leaf_type_from_index(size_t i)
{
  return (rw_yang_leaf_type_t)(i + (size_t)RW_YANG_LEAF_TYPE_FIRST);
}

const char* rw_yang_leaf_type_string(rw_yang_leaf_type_t v);



typedef enum rw_yang_pb_msg_type_e {
  RW_YANGPBC_MSG_TYPE_NULL = 0,
  RW_YANGPBC_MSG_TYPE_base = 0x473EF374,

  RW_YANGPBC_MSG_TYPE_ROOT, //!< yang container
  RW_YANGPBC_MSG_TYPE_MODULE_ROOT, //!< yang container
  RW_YANGPBC_MSG_TYPE_CONTAINER, //!< yang container
  RW_YANGPBC_MSG_TYPE_LIST, //!< yang list
  RW_YANGPBC_MSG_TYPE_LEAF_LIST, //!< yang leaf list
  RW_YANGPBC_MSG_TYPE_RPC_INPUT, //!< yang rpc input
  RW_YANGPBC_MSG_TYPE_RPC_OUTPUT, //!< yang rpc output
  RW_YANGPBC_MSG_TYPE_GROUPING, //!< yang grouping
  RW_YANGPBC_MSG_TYPE_NOTIFICATION, //!< yang notification
  RW_YANGPBC_MSG_TYPE_SCHEMA_ENTRY, //!< auto-generated schema entry
  RW_YANGPBC_MSG_TYPE_SCHEMA_PATH, //!< auto-generated schema path message
  RW_YANGPBC_MSG_TYPE_SCHEMA_LIST_ONLY, //!< auto-generated for yang list element

  RW_YANGPBC_MSG_TYPE_end,
  RW_YANGPBC_MSG_TYPE_FIRST = RW_YANGPBC_MSG_TYPE_base+1,
  RW_YANGPBC_MSG_TYPE_LAST = RW_YANGPBC_MSG_TYPE_end-1,
  RW_YANGPBC_MSG_TYPE_COUNT = RW_YANGPBC_MSG_TYPE_end-RW_YANGPBC_MSG_TYPE_base-1,
} rw_yang_pb_msg_type_t;

static inline bool_t rw_yang_pb_msg_type_is_good(rw_yang_pb_msg_type_t v)
{
  return    (int)v > (int)RW_YANGPBC_MSG_TYPE_FIRST
         && (int)v < (int)RW_YANGPBC_MSG_TYPE_LAST;
}
static inline bool_t rw_yang_pb_msg_type_is_good_or_null(rw_yang_pb_msg_type_t v)
{
  return    v == RW_YANGPBC_MSG_TYPE_NULL
         || (    (int)v > (int)RW_YANGPBC_MSG_TYPE_FIRST
              && (int)v < (int)RW_YANGPBC_MSG_TYPE_LAST);
}
static inline size_t rw_yang_pb_msg_type_to_index(rw_yang_pb_msg_type_t v)
{
  return (size_t)((int)v - (int)RW_YANGPBC_MSG_TYPE_FIRST);
}
static inline rw_yang_pb_msg_type_t rw_yang_pb_msg_type_from_index(size_t i)
{
  return (rw_yang_pb_msg_type_t)(i + (size_t)RW_YANGPBC_MSG_TYPE_FIRST);
}

const char* rw_yang_pb_msg_type_string(rw_yang_pb_msg_type_t v);


// ATTN: These structs need some kind of member, such as a magic number.
// because empty struct is not C compliant.
#ifdef __GI_SCANNER__
//! C adapter struct for C++ class YangModel.
typedef struct rw_yang_model_s rw_yang_model_t;

//! C adapter struct for C++ class YangModule.
typedef struct rw_yang_module_s rw_yang_module_t;

//! C adapter struct for C++ class YangNode.
typedef struct rw_yang_node_s rw_yang_node_t;

//! C adapter struct for C++ class YangType.
typedef struct rw_yang_type_s rw_yang_type_t;

//! C adapter struct for C++ class YangKey.
typedef struct rw_yang_key_s rw_yang_key_t;

//! C adapter struct for C++ class YangAugment.
typedef struct rw_yang_augment_s rw_yang_augment_t;

//! C adapter struct for C++ class YangValue.
typedef struct rw_yang_value_s rw_yang_value_t;

//! C adapter struct for C++ class YangBits.
typedef struct rw_yang_bits_s rw_yang_bits_t;

//! C adapter struct for C++ class YangExtension.
typedef struct rw_yang_extension_s rw_yang_extension_t;

//! C adapter struct for C++ class YangTypedEnum.
typedef struct rw_yang_typed_enum_s rw_yang_typed_enum_t;

//! C adapter struct for C++ class YangGrouping.
typedef struct rw_yang_grouping_s rw_yang_grouping_t;

//! C adapter struct for C++ class YangUses.
typedef struct rw_yang_uses_s rw_yang_uses_t;
#else   /* __GI_SCANNER__ */
//! C adapter struct for C++ class YangModel.
typedef struct rw_yang_model_s {} rw_yang_model_t;

//! C adapter struct for C++ class YangModule.
typedef struct rw_yang_module_s {} rw_yang_module_t;

//! C adapter struct for C++ class YangNode.
typedef struct rw_yang_node_s {} rw_yang_node_t;

//! C adapter struct for C++ class YangType.
typedef struct rw_yang_type_s {} rw_yang_type_t;

//! C adapter struct for C++ class YangKey.
typedef struct rw_yang_key_s {} rw_yang_key_t;

//! C adapter struct for C++ class YangAugment.
typedef struct rw_yang_augment_s {} rw_yang_augment_t;

//! C adapter struct for C++ class YangValue.
typedef struct rw_yang_value_s {} rw_yang_value_t;

//! C adapter struct for C++ class YangBits.
typedef struct rw_yang_bits_s {} rw_yang_bits_t;

//! C adapter struct for C++ class YangExtension.
typedef struct rw_yang_extension_s {} rw_yang_extension_t;

//! C adapter struct for C++ class YangTypedEnum.
typedef struct rw_yang_typed_enum_s {} rw_yang_typed_enum_t;

//! C adapter struct for C++ class YangGrouping.
typedef struct rw_yang_grouping_s {} rw_yang_grouping_t;

//! C adapter struct for C++ class YangUses.
typedef struct rw_yang_uses_s {} rw_yang_uses_t;
#endif  /* __GI_SCANNER__ */


struct rw_yang_pb_group_root_t;
struct rw_yang_pb_module_t;
struct rw_yang_pb_rpcdef_t;
union rw_yang_pb_msgdesc_union_t;
struct rw_yang_pb_msgdesc_t;
struct rw_yang_pb_enumdesc_t;
struct rw_yang_pb_flddesc_t;

typedef struct rw_yang_pb_group_root_t rw_yang_pb_group_root_t;
typedef struct rw_yang_pb_module_t rw_yang_pb_module_t;
typedef struct rw_yang_pb_rpcdef_t rw_yang_pb_rpcdef_t;
typedef union rw_yang_pb_msgdesc_union_t rw_yang_pb_msgdesc_union_t;
typedef struct rw_yang_pb_msgdesc_t rw_yang_pb_msgdesc_t;
typedef struct rw_yang_pb_enumdesc_t rw_yang_pb_enumdesc_t;
typedef struct rw_yang_pb_flddesc_t rw_yang_pb_flddesc_t;


/*!
 * Protobuf-c module grouping definitions for YangModel integration.
 * These structs are automatically generated by yangpbc in order to map
 * the top-level grouping statements for a yang module to the
 * corresponding protobuf-c message descriptors.
 *
 * It is a requirement that instances of this struct be shareable by
 * multiple YangModels and ProtobufC*Descriptors.  It should be
 * possible to store these in read-only data sections.
 */
struct rw_yang_pb_group_root_t
{
  //! The module that defines the grouping
  const rw_yang_pb_module_t* module;

  //! The list of top-level grouping elements in the module.
  const rw_yang_pb_msgdesc_t* const* msgs;

  //! The size of msgs.
  size_t msg_count;
};


/*!
 * Protobuf-c RPC definition for YangModel integration.  These structs
 * are automatically generated by yangpbc in order to map a yang rpc
 * statement to the corresponding protobuf-c message descriptors.
 *
 * It is a requirement that instances of this struct be shareable by
 * multiple YangModels and ProtobufC*Descriptors.  It should be
 * possible to store these in read-only data sections.
 */
struct rw_yang_pb_rpcdef_t
{
  //! The module that defines the rpc
  const rw_yang_pb_module_t* module;

  //! The yang RPC name.
  const char* yang_node_name;

  //! The ypbc message descriptor that defines the input.
  //! Must exist, even if the message has no contents.
  const rw_yang_pb_msgdesc_t* input;

  //! The ypbc message descriptor that defines the output.
  //! Must exist, even if the message has no contents.
  const rw_yang_pb_msgdesc_t* output;
};


/*!
 * Protobuf-c module definition for YangModel integration.  These
 * structs are automatically generated by yangpbc in order to map a
 * yang module to the corresponding protobuf-c message descriptors.
 *
 * It is a requirement that instances of this struct be shareable by
 * multiple YangModels and ProtobufC*Descriptors.  It should be
 * possible to store these in read-only data sections.
 */
struct rw_yang_pb_module_t
{
  //! The owning schema.
  const rw_yang_pb_schema_t* schema;

  //! The module name (as defined by yang).
  const char* module_name;

  //! Module revision. ATTN: Not yet supported.
  const char* revision;

  //! Module namespace, as defined in yang.
  const char* ns;

  //! The module prefix defined in the module itself.
  const char* prefix;

  // ATTN: Also define a list of module prefixes used by the importers?
  // ATTN: Or, define a list of the modules this module imported, and
  //   the prefixes used in those import statements.

  //! The module's root data definition.
  const rw_yang_pb_msgdesc_t* data_module;

  //! The module's grouping definitions.
  const rw_yang_pb_group_root_t* group_root;

  //! The module's rpc root input definition.
  const rw_yang_pb_msgdesc_t* rpci_module;

  //! The module's rpc root output definition.
  const rw_yang_pb_msgdesc_t* rpco_module;

  //! The module's notification root definition.
  const rw_yang_pb_msgdesc_t* notif_module;

  //! ATTN: Also define the top-level messages?
};


/*!
 * Protobuf-c global-schema definition for YangModel integration.
 * These structs are automatically generated by yangpbc in order to map
 * a yang global-schema to protobuf-c.  A yang global-schema is the
 * complete set of yang data, from all modules and imports with all
 * augments applied - as viewed through the lens of the single yang
 * module doing the imports.
 *
 * It is a requirement that instances of this struct be shareable by
 * multiple YangModels and ProtobufC*Descriptors.  It must be possible
 * to store these in read-only data sections.
 */
typedef struct rw_yang_pb_schema_t
{
  //! The module that defines the schema.
  const rw_yang_pb_module_t* schema_module;

  //! The schema data root msgdesc
  const rw_yang_pb_msgdesc_t* data_root;

  //! The list of top-level messages.
  //! ATTN: deprecated Should walk the modules to the messages
  const rw_yang_pb_msgdesc_t* const* top_msglist;

  //! The list of modules in the schema.
  const rw_yang_pb_module_t* const* modules;

  //! The number of modules.
  size_t module_count;

} rw_yang_pb_schema_t;



/*!
 * Yang to protobuf translation support for a protobuf message
 * definition.  These structs are automatically generated by yangpbc in
 * order to link a list or container YangNode to a
 * ProtbufCMessageDescriptor.
 *
 * It is a requirement that instances of this struct be shareable by
 * multiple YangModels and ProtobufC*Descriptors.  It should be
 * possible to store these in read-only data sections.
 *
 * As a result of the sharing requirement, please note that it IS NOT
 * POSSIBLE to go directly from protobuf descriptors to the actual
 * Yang* data structures, because there can be multiple YangModel
 * instances in a single running program, each with their own copy of
 * the module metadata.  The design assumes that the lack of direct
 * back-pointer to Yang* is acceptible, given that all XML-pb
 * conversions must use RwXml, and therefore must already perform a
 * rooted traverse of the YangModel.
 */
struct rw_yang_pb_msgdesc_t
{
  /*!
   * The module that defined this element.  This does not reflect
   * augments - some fields may come from augmenting modules.  This
   * pointer can be used to find the full schema used to define the
   * message.
   */
  const rw_yang_pb_module_t* module;

  /*!
   * The yang node name.  Unmangled.
   */
  const char* yang_node_name;

  /*!
   * The tag number used for this message/node in a keyspec.
   */
  uint32_t pb_element_tag;

  /*
   * ATTN: Should include yang prefix here, because it can differ from
   * the module's actual yang prefix, due to the way the node was
   * imported/augmented/used?
   */

  /*!
   * The ypbc field descriptors in the schema order.
   */
  size_t num_fields;
  const rw_yang_pb_flddesc_t* ypbc_flddescs;

  /*!
   * The parent ypbc msg descriptor structure, for messages defined in
   * the global-schema.  This is null for top-level nodes directly
   * under a module, and possibly null for messages in the old-style
   * schema.  This can be used to trace the complete schema path of the
   * node/msg back to the root.
   */
  const rw_yang_pb_msgdesc_t* parent;

  /*!
   * The rift-protobuf-c message descriptor for the message type.
   */
  const ProtobufCMessageDescriptor* pbc_mdesc;

  /*!
   * The rift-protobuf-c message descriptor for the message's concrete
   * schema entry type.
   *
   * ATTN: This field is null in old-style schema messages.
   * ATTN: Technically obtainable from schema_entry_value.
   */
  const rw_yang_pb_msgdesc_t* schema_entry_ypbc_desc;

  /*!
   * The rift-protobuf-c message descriptor for the message's concrete
   * schema path type.
   *
   * ATTN: This field is null in old-style schema messages.
   * ATTN: Technically obtainable from schema_path_value.
   */
  const rw_yang_pb_msgdesc_t* schema_path_ypbc_desc;

  /*!
   * If the proto-c message is a list element in the yang, then this field
   * points to the ypbc message descriptor of the  list-only
   * message that contains only the list element as the field.
   *
   * ATTN: This field is null in old-style schema messages.
   */
  const rw_yang_pb_msgdesc_t* schema_list_only_ypbc_desc;

  /*!
   * A constant copy of a fully-initialized schema entry message,
   * complete with element id and namespace values filled with the
   * correct values.  The keys, if any, are not set.
   *
   * ATTN: This field is null in old-style schema messages.
   */
  const rw_keyspec_entry_t* schema_entry_value;

  /*!
   * A constant copy of a fully-initialized schema path message,
   * complete with element id and namespace values filled with the
   * correct values.  The keys, if any, are not set.
   *
   * ATTN: This field is null in old-style schema messages.
   */
  const rw_keyspec_path_t* schema_path_value;

  /*!
   * List of child msg descriptors.
   *
   * ATTN: This field is null in old-style schema messages.
   */
  const rw_yang_pb_msgdesc_t* const* child_msgs;

  /*!
   * Number of child msgs.
   */
  size_t child_msg_count;

  /*!
   * Message type.
   */
  rw_yang_pb_msg_type_t msg_type;

  /*!
   * Yang statement-type specific data union.  The contents depend on
   * the statement type.
   */
  const rw_yang_pb_msgdesc_union_t* u;
};


/*!
 * Additional data union for the ypbc message descriptor.  The actual
 * contents of this union depends on the stmt_type of the referring
 * ypbc message descriptor.
 *
 * It is a requirement that instances of this struct be shareable by
 * multiple YangModels and ProtobufC*Descriptors.  It should be
 * possible to store these in read-only data sections.
 */
union rw_yang_pb_msgdesc_union_t
{
  /*!
   * The related message's ypbc message descriptor.  Populated only for
   * the path entry, path spec, and list-only descriptors.
   */
  rw_yang_pb_msgdesc_t msg_msgdesc;

  //! The rpcdef structure for the RPC.
  rw_yang_pb_rpcdef_t rpcdef;
};


/*!
 * Yang to protobuf translation support for a protobuf
 * field definition.
 */
struct rw_yang_pb_flddesc_t
{
  /*!
   * The module that defined this node.
   */
  const rw_yang_pb_module_t* module;

  /*!
   * The ypbc_msgdesc containing this field descriptor.
   */
  const rw_yang_pb_msgdesc_t* ypbc_msgdesc;

  /*!
   * Pointer to the protobuf c field descripto for this
   * yang field.
   */
  const ProtobufCFieldDescriptor* pbc_fdesc;

  /*!
   * The yang node name.
   */
  const char* yang_node_name;

  /*!
   * The yang stmt type (leaf, container etc.)
   */
  rw_yang_stmt_type_t stmt_type;

  /*!
   * The yang leaf_type ( string, binary etc.)
   */
  rw_yang_leaf_type_t leaf_type;

  /*!
   * The yang order of the field as it appears inside the enclosing container/list
   */
  unsigned yang_order;

  /*!
   * The protobuf c order of the field inside the field descriptor array of the
   * enclosing message.
   */
  unsigned pbc_order;
};


/*!
 * Yang to protobuf-c translation support for a protobuf enumeration
 * definition.  These structs are automatically generated by yangpbc in
 * order to link a yang enummeration type to a ProtbufCEnumDescriptor.
 *
 * It is a requirement that instances of this struct be shareable by
 * multiple YangModels and ProtobufC*Descriptors.  It should be
 * possible to store these in read-only data sections.
 *
 * As a result of the sharing requirement, please note that it IS NOT
 * POSSIBLE to go directly from protobuf descriptors to the actual
 * Yang* data structures, because there can be multiple YangModel
 * instances in a single running program, each with their own copy of
 * the module metadata.
 */
struct rw_yang_pb_enumdesc_t
{
  /*!
   * The module that defined the enumeration.
   */
  const rw_yang_pb_module_t* module;

  /*!
   * The yang enum name.  Unmangled.  Will be either a yang typedef
   * name or a yang field name, depending on whether the enum was
   * defined in a typedef or at a leaf.
   */
  const char* yang_enum_name;

  /*!
   * The rift-protobuf-c enum descriptor for the enum type.
   */
  const ProtobufCEnumDescriptor* enum_desc;

  /*!
   * The yang enum identifier strings, in enum_desc->values_by_name[]
   * order.  Length is enum_desc->n_value_names.
   */
  const char* const* yang_enums;

  /*!
   * The indicies of the yang enum identifier strings, sorted by yang
   * identifier (in the "C" locale).  This is suitable for use in a
   * binary string search of the yang enum identifiers (indirectly).
   * The indicies in this array index into both yang_enums[] and
   * enum_desc->values_by_name[].  Please note that the order of this
   * array IS NOT THE SAME AS enum->values_by_name[], due to the name
   * mangling going from yang to proto!  Length is
   * enum_desc->n_value_names.
   */
  const uint32_t* values_by_yang_name;
};

/*
 * Functions that enable use of pb schema
 */

/*!
 * Find the rw_yang_pb message descriptor from a schema using the namespace
 * and yang node name from the yang schema.
 *
 * @param schema The schema in which the namespace exists
 * @param ns namespace of the module in the schema
 * @param name name of the yang node in the schema
 *
 * @return The message descriptor if it exists, nullpt otherwise
 */
const rw_yang_pb_msgdesc_t *rw_yang_pb_schema_get_top_level_message (
    const rw_yang_pb_schema_t* schema,
    const char *ns,
    const char *name);

/*!
 * Find the rw_yang_pb message descriptor from a parent descriptor  using the
 * namespace and yang node name from the yang schema.
 *
 * @param parent the parent message descriptor
 * @param ns namespace of the module in the schema
 * @param name name of the yang node in the schema
 *
 * @return The message descriptor if it exists, nullptr otherwise
 */

const rw_yang_pb_msgdesc_t *rw_yang_pb_msg_get_child_message (
    const rw_yang_pb_msgdesc_t *parent,
    const char *ns,
    const char *name);




__END_DECLS

#if __cplusplus

extern const char* RW_ANNOTATIONS_NAMESPACE;


namespace rw_yang {
class YangModel;
class YangModule;
class YangNode;
class YangKey;
class YangAugment; // ATTN: Should be a flavor of YangNode?
class YangType;
class YangValue;
class YangExtension; // ATTN: Should be a flavor of YangNode?
class YangTypedEnum;
class YangUses; // ATTN: Should be a flavor of YangNode?

class YangModuleIter;
class YangNodeIter;
class YangKeyIter;
class YangAugmentIter;
class YangValueIter;
class YangExtensionIter;
class YangTypedEnumIter;
class YangImportIter;
class YangGroupingIter;
class YangGrouping;
// ATTN: class YangBits;
// ATTN: class YangBitsIter;


typedef uint32_t (*ymodel_map_func_ptr_t) (const char *);
typedef std::unordered_map <std::string, uint32_t> namespace_map_t;

extern const char* YANGMODEL_ANNOTATION_KEY;
extern const char* YANGMODEL_ANNOTATION_PBNODE;
extern const char* YANGMODEL_ANNOTATION_SCHEMA_NODE;
extern const char* YANGMODEL_ANNOTATION_SCHEMA_MODULE;
extern const char* YANGMODEL_ANNOTATION_SCHEMA_MODEL;


/*!
 * Yang model.  Contains a description of the yang schema, suitable for
 * use in generating CLI syntax, obtaining help, finding RIFT
 * extensions, and verifying data.
 *
 * When the model returns a pointer to a node, the model owns the
 * returned pointer!
 *
 * Abstract base class.
 */
class YangModel
: public rw_yang_model_t
{
public:
  YangModel();
  virtual ~YangModel() {}

  // Cannot copy
  YangModel(const YangModel&) = delete;
  YangModel& operator=(const YangModel&) = delete;

  typedef std::unique_ptr<YangModel> ptr_t;
  typedef AppDataToken<intptr_t,rw_yang::AppDataTokenDeleterNull> adt_confd_ann_t;
  typedef AppDataToken<const rw_yang_pb_msgdesc_t*,rw_yang::AppDataTokenDeleterNull> adt_schema_node_t;
  typedef AppDataToken<const rw_yang_pb_module_t*,rw_yang::AppDataTokenDeleterNull> adt_schema_module_t;
  typedef AppDataToken<const rw_yang_pb_schema_t*,rw_yang::AppDataTokenDeleterNull> adt_schema_model_t;

public:
  //! Convert filename into module name
  /*!
   * This function takes a (relative) path, with or without a .yang
   * extension, and extracts the yang module name.
   *
   * ATTN: Need to be able to split revision?
   *
   * @param filename - The filename to convert
   * @return The module name.  Could be empty on error.
   */
  static std::string filename_to_module(
    const char* filename //!< The filename
  );

  //! Mangle a yang identifier to a valid C identifier
  /*!
   * This function mangles a yang or other identifier into a valid C
   * identifier.  Non-C characters are converted to underscores, and
   * then consecutive underscores are collapsed into single
   * underscores.  Leading and trailing underscores are suppressed.
   * Leading integer characters are mangled to text number.
   *
   * The mangling is not reversible.
   *
   * @return The mangled identifier.
   */
  static std::string mangle_to_c_identifier(
    const char* id //!< [in] The identifier to mangle
  );

  //! Mangle a yang identifier to camel case.
  /*!
   * This function mangles a yang or C identifier into a CamelCase C
   * identifier.  Non-C and underscore characters are considered word
   * separators; they are removed and the first letter of the following
   * word is capitalized.  Leading and trailing underscores are
   * suppressed.  Leading integer characters are mangled to text
   * number.
   *
   * The mangling is not reversible.
   *
   * @return The mangled identifier.
   */
  static std::string mangle_to_camel_case(
    const char* id //!< [in] The identifier to mangle
  );

  //! Mangle a yang Camelcase identifier to a c string
  /*!
   * This function mangles a camelcase C/C++ identifier into a underscore C/C++
   * identifier.  Characters in the input string that are not allowed in C
   * identfier  will cause this function  to assert;
   * Upper case characters in the passed argument is always converted to lower
   * case and may be prefixed by an "_".
   * The converted string inserts "_" before the converted uppercase character,
   * when the converted character,
   *   1) is not the first character in the string
   *   2) is not preceded by other uppercase characters
   * This function also asserts when the resulting string is empty()
   *
   * The mangling is not reversible.
   *
   * @return The mangled identifier.
   */
  static std::string mangle_camel_case_to_underscore(
    const char* id //!< [in] The identifier to mangle
  );

  /*!
   * This function checks if an identifier is a valid C identifier, and
   * does not lead with underscore.
   *
   * @return true if the identifier is valid in C
   */
  static bool mangle_is_c_identifier(
    const char* id //!< [in] The identifier to check
  );

  /*!
   * This function checks if an identifier is a valid CamelCased
   * identifier, without underscores.
   *
   * @return true if the identifier is CamelCased.
   */
  static bool mangle_is_camel_case(
    const char* id //!< [in] The identifier to check
  );

  /*!
   * This function checks if an identifier is a valid lower-cased
   * identifier, with only single underscores, and does not lead with
   * underscore.
   *
   * @return true if the identifier is valid lower case.
   */
  static bool mangle_is_lower_case(
    const char* id //!< [in] The identifier to check
  );

  /*!
   * Escape a raw UTF8 string to a text JSON string (RFC 7159.7).
   *
   * @return The escaped string.  Suitable for including in JSON text,
   *   after wrapping in quotes.
   */
  static std::string utf8_to_escaped_json_string(
    /*! The string to escape.  UTF8 compliance is assumed. */
    const std::string& string );

  /*!
   * Escape a raw UTF8 string to a RW.REST URI key value string, as per
   * RFC 3986, RESTCONF RFC draft 8, and RW.REST design.
   *
   * @return The escaped string.  Suitable for inclusion into raw URI
   *   text as a path element.
   */
  static std::string utf8_to_escaped_uri_key_string(
    /*! The string to escape.  UTF8 compliance is assumed. */
    const std::string& string );

  /*!
   * Escape a raw UTF8 string XPath expression to libncx-compatible
   * xpath text.  The result in non-normative with any other XPath
   *
   * @return The escaped string.  Suitable for producing XPath text
   *   that will be parsed by libncx.
   */
  static std::string utf8_to_escaped_libncx_xpath_string(
    /*! The string to escape.  UTF8 compliance is assumed. */
    const std::string& string );

  /*!
   * Escape a raw UTF8 string to XML text data (as opposed to XML
   * syntax or tags), as per XML1.1.
   *
   * @return The escaped string.  Suitble for use as data within an XML
   *   document, which when parsed by a compliant XML parser, will
   *   produce the input string as a text node.
   */
  static std::string utf8_to_escaped_xml_data(
    /*! The string to escape.  UTF8 compliance is assumed. */
    const std::string& string );

  /*!
   * Change the indentation on a string.  The string is assumed to come
   * from yang module parsing, where the first line does not
   * necessarily have any padding at all.  The padding on the input
   * string is taken as the minimum padding of all the lines except the
   * first and entirely blank lines.
   *
   * @return The re-indented string.
   */
  static std::string adjust_indent_yang(
    /*! The desired indentation. */
    unsigned indent,
    /*! The string to indent. */
    const std::string& data );

  /*!
   * Change the indentation on a string.  The string is assumed to come
   * with padding, as determined by the number of leading spaces on the
   * first line.  This padding will be removed.
   *
   * @return The re-indented string.
   */
  static std::string adjust_indent_normal(
    /*! The desired indentation. */
    unsigned indent,
    /*! The string to indent. */
    const std::string& data );

  /*!
   * Change the indentation on a string.  The string is assumed to come
   * with padding to be removed.
   *
   * @return The re-indented string.
   */
  static std::string adjust_indent(
    /*! The desired indentation. */
    unsigned indent,
    /* The amount of padding that data already has, which is to be removed. */
    size_t subtract_pad,
    /*! The string to indent. */
    const std::string& data );

public:
  /*!
   * Search for and/or load a yang module into the model.  If the
   * module is not currently loaded, it will be loaded if possible,
   * along with all dependencies.  If the module is already loaded,
   * this function just returns the module.
   *
   * ATTN: revision is currently not supported.
   *
   * THIS FUNCTION MAY TAKE A LOCK AND THEN BLOCK INDEFINITELY ON FILE
   * ACCESSES.  ALL OUTSTANDING REFERENCES TO YangModel DATA STRUCTURES
   * SHUOULD BE CONSIDERED MODIFIED AFTER THIS CALL.
   *
   * ATTN: revision is currently not supported.
   *
   * @return The module, if the load was successful.  Even if the load
   *   was not successful, the model may have changed (due to other
   *   imports, with their associated augments, that occurred while
   *   trying load the target module).
   */
  virtual YangModule* load_module(
    const char* module_name //!< The name of the module to load
  ) = 0;

  /*!
   * Search for and/or load a module needed by a yang-derived protobuf
   * descriptor.  If the module is not currently loaded, it will be
   * loaded if possible, along with all dependencies.  If the module is
   * already loaded, this function just returns the module.
   *
   * THIS FUNCTION MAY TAKE A LOCK AND THEN BLOCK INDEFINITELY ON FILE
   * ACCESSES.  ALL OUTSTANDING REFERENCES TO YangModel DATA STRUCTURES
   * SHUOULD BE CONSIDERED MODIFIED AFTER THIS CALL.  THIS FUNCTION MAY
   * BE O(n) IN THE NUMBER OF MODULES.
   *
   * @return A pointer to the module, if found or loaded successfully.
   *   nullptr if not found or if the load fails.  The module is owned
   *   by the library.  Caller may use the module until model is
   *   destroyed.
   */
  virtual YangModule* load_module_ypbc(
    const rw_yang_pb_module_t* ypbc_mod //!< The module descriptor
  );

  /*!
   * Load a schema needed by a yang-derived protobuf descriptor.  The
   * schema is the set of explicitly imported modules that were used to
   * generate the protobuf descriptor.  If any of the modules are not
   * currently loaded, they will be loaded if possible, along with all
   * dependencies.
   *
   * THIS FUNCTION MAY TAKE A LOCK AND THEN BLOCK INDEFINITELY ON FILE
   * ACCESSES.  ALL OUTSTANDING REFERENCES TO YangModel DATA STRUCTURES
   * SHUOULD BE CONSIDERED MODIFIED AFTER THIS CALL.  THIS FUNCTION MAY
   * BE O(n) IN THE NUMBER OF MODULES.
   *
   * @return RW_STATUS_SUCCESS on success. Other values on failure.
   */
  virtual rw_status_t load_schema_ypbc(
    //! The schema to load.
    const rw_yang_pb_schema_t* schema
  );

  /*!
   * Return the root node for the model.  The root node is the parent
   * of all top-level nodes in all modules.  The root node does not
   * belong to any module.
   *
   * @return A pointer to the root node.  Cannot be nullptr.  The node
   *   is owned by the library.  Caller may use the node until model is
   *   destroyed.  The attributes of the node may change after future
   *   module loads (such as first_child), so the caller should not
   *   cache data from the node.
   */
  virtual YangNode* get_root_node() = 0;

  /*!
   * Search for a loaded module by namespace URI.  The module may not
   * have been loaded explicitly.
   *
   * This function may take a lock.  This function may be O(n) in the
   * number of modules.
   *
   * @return A pointer to the module, if found.  nullptr if not found.
   *   The module is owned by the library.  Caller may use the module
   *   until model is destroyed.
   */
  virtual YangModule* search_module_ns(
    const char* ns //!< The namespace
  );

#if 0
  /*!
   * Search for a loaded module by namespace identifier.  The module may not
   * have been loaded explicitly.
   *
   * This function may take a lock.  This function may be O(n) in the
   * number of modules.
   *
   * @return A pointer to the module, if found.  nullptr if not found.
   *   The module is owned by the library.  Caller may use the module
   *   until model is destroyed.
   */
  virtual YangModule* search_module_ns(
      const uint32_t system_ns_id); //!< The namespace
#endif

  /*!
   * Search for a loaded module by module name and optional revision.
   * The module may not have been loaded explicitly.
   *
   * This function may take a lock.  This function may be O(n) in the
   * number of modules.
   *
   * @return A pointer to the module, if found.  nullptr if not found.
   *   The module is owned by the library.  Caller may use the module
   *   until model is destroyed.
   */
  virtual YangModule* search_module_name_rev(
    const char* name, //!< The module name
    const char* revision //!< The module revision date, may be nullptr if revision doesn't matter
  );

  /*!
   * Search for a top level node in a module that is loaded into this module.
   *
   * This function walks the imported modules. It then checks if any of the top
   * level nodes in the module match the element id that is provided.
   *
   * ATTN: This needs to change to first identifying the module using the module
   * identifier, and then walking just that modules top level nodes to find the
   * corresponding yang node.
   * @return A pointer to the node, if found. nullptr otherwise
   */
  YangNode* search_top_yang_node(
    const uint32_t system_ns_id, /*!< [in] namespace identifier assigned by yangpbc */
    const uint32_t element_tag); /*!< [in] element tag assigned by yangpbc */

  /*!
   * Get a pointer to the first module.  The module may not have been
   * loaded explicitly.
   *
   * @return A pointer to the module, if any.  nullptr if no modules
   *   have been loaded.  Some modules may be loaded automatically upon
   *   creation of the model, so this function might never return
   *   nullptr in practice.  The module is owned by the library.
   *   Caller may use the module until model is destroyed.
   */
  virtual YangModule* get_first_module()
  { return nullptr; }

  /*!
   * Construct a module iterator located at the first module of this
   * model.
   *
   * @return The module iterator.
   */
  virtual YangModuleIter module_begin();

  /*!
   * Construct a module iterator located after the last module of this
   * model.
   *
   * @return The module iterator.  Not suitable for dereferencing to a
   *   YangModule.
   */
  virtual YangModuleIter module_end();

  /*!
   * Public interface to registering or looking up Application Data
   * Tokens.  If there is no slot allocated for the Application Data,
   * one will be created.
   *
   * @return
   *  - true if the token could be initialized
   *  - false if the token could not be initialized
   */
  template <class data_type, class deleter_type=AppDataTokenDeleter<data_type>>
  bool app_data_get_token(
    //! [in] The extension name space.  This name space doesn't have to
    //! map to a yang module, but if it does, then some additional
    //! helper functions become available in YangNode.  Cannot be
    //! nullptr.
    const char* ns,

    //! [in] The extension name.  If ns and ext map to an actual yang
    //! modeul extension, then you can lookup and cache an extension
    //! with a single member function call to a YangNode.  Otherwise,
    //! this does not necessarily need to map to any yang extension
    //! name.
    const char* ext,

    //! [out] The token.  Only valid on success.
    AppDataToken<data_type,deleter_type>* token);

  /*!
   * Base interface to registering or looking up Application Data Base
   * Tokens.  Used by app_data_get_token<>().
   *
   * @return true if the token was successfully initialized
   */
  virtual bool app_data_get_token(
    const char* ns, //!< [in] The extension name space.
    const char* ext, //!< [in] The extension name.
    AppDataTokenBase::deleter_f deleter, //!< [in] the deleter function
    AppDataTokenBase* token //!< [out] The token, if successful
  ) = 0;

  /*!
   * Set the module logging level.
   */
  virtual void set_log_level(
    rw_yang_log_level_t level //!< [in] desired loging level
  ) = 0;

  /*!
   * Retreive the current module logging level.
   *
   * @return the current module logging level
   */
  virtual rw_yang_log_level_t log_level() const = 0;

  /*!
   * Given a namespace identifier and a tag number associated with a top level
   * node in a module, this function finds its associated YangNode in the model.
   *
   * @return YangNode, if it can be found. nllptr otherwise
   * If a yangpbc schema has not been loaded to the model, this function will
   * fail.
   */
  YangNode* search_node(
    /*!
     * [in] the namespace assigned by a namespace manager for a module within
     * this model
     */
    const uint32_t system_ns_id,

    /*!
     * [in] the tag assigned to this node during yangpbc processing. This is
     * the tag used for this node in a protobuf.
     */
    const uint32_t element_id);

  /*!
   * YangModel is used in converting from XML and Protobuf structures
   * that are used extensively in Riftware.  The conversion is based on
   * a protobuf schema.  The protobuf schema is analogous to the Yang
   * schema, and has more data that is useful in Riftware.  In
   * particular, it has information that is generated during the
   * Yang2Protobuf conversion step.  This includes the
   * ProtoCMessageDesc and schema path for each node in the Yang
   * Schema.  During conversions, the YANG model and the associated
   * protobuf model is required.  This function loads a Protobuf schema
   * on to a YangModel.
   *
   * @return RW_STATUS_SUCCESS if no errors are found
   *         RW_STATUS_FAILURE, if there are modules or nodes referred to in the
   *                            protobuf that are not known in the Yang schema.
   */
  rw_status_t register_ypbc_schema(
    //! [in] The schema to load
    const rw_yang_pb_schema_t* ypbc_schema);

  /*!
   * If a model has a yangpbc schema attached, retrieve it.
   *
   * @return the ypbc schema if it has been set on the model. nullptr otherwise.
   */
  const rw_yang_pb_schema_t* get_ypbc_schema();

public:
  //! App Data token for binding Confd Namespace
  adt_confd_ann_t adt_confd_ns_;

  //! App Data token for binding Confd Tag
  adt_confd_ann_t adt_confd_name_;

  //! App Data token for binding pb information for a message
  adt_schema_node_t adt_schema_node_;

  //! App Data token for binding pb information to a module
  adt_schema_module_t adt_schema_module_;

  //! A boolean to ensure that the schema token has been registered.
  bool adt_schema_registered_ = false;

  const rw_yang_pb_schema_t* ypbc_schema_ = nullptr;
};


/*!
 * Yang schema module.  This class describes a single module in the
 * YANG schema, whether loaded explicitly through the YangModel, or
 * implicitly via import statements or the library itself.
 *
 * The module is owned by the model.  The caller can keep a copy of the
 * pointer for as long as the model exists.  The caller is free to
 * assume that the module will not be deleted.  However, the module may
 * be modified due to augments, so the caller should not cache
 * attributes of the module.
 *
 * Abstract base class.
 */
class YangModule
: public rw_yang_module_t
{
public:
  YangModule() {}
  virtual ~YangModule() {}

  // Cannot copy
  YangModule(const YangModule&) = delete;
  YangModule& operator=(const YangModule&) = delete;

public:
  virtual std::string get_location() = 0;
  virtual const char* get_description() = 0;
  virtual const char* get_name() = 0;
  virtual const char* get_prefix() = 0;
  virtual const char* get_ns() = 0;
  // ATTN: virtual const char* get_reference() = 0;
  // ATTN: virtual const char* get_contact() = 0;
  // ATTN: virtual const char* get_revision() = 0;
  virtual YangModule* get_next_module() = 0;
  virtual bool was_loaded_explicitly() = 0;

  virtual YangNode* get_first_node() = 0; // skips choice and case
  virtual YangNodeIter node_begin();
  virtual YangNodeIter node_end();

  virtual YangExtension* get_first_extension() { return nullptr; }
  virtual YangExtensionIter extension_begin();
  virtual YangExtensionIter extension_end();

  virtual YangAugment* get_first_augment() = 0; // skips choice and case
  virtual YangAugmentIter augment_begin();
  virtual YangAugmentIter augment_end();

  virtual YangTypedEnum *get_first_typed_enum () = 0;
  virtual YangTypedEnumIter typed_enum_begin();
  virtual YangTypedEnumIter typed_enum_end();

  virtual YangModule** get_first_import() = 0;
  virtual YangImportIter import_begin();
  virtual YangImportIter import_end();

  virtual YangNode* get_first_grouping() { return nullptr; }
  virtual YangGroupingIter grouping_begin();
  virtual YangGroupingIter grouping_end();

  /*!
   * Check if an attribute is cached.
   *
   * @return
   *  - true: The value was cached.
   *  - false: The value was not cached.
   */
  virtual bool app_data_is_cached(
    const AppDataTokenBase* token //!< [in] Token that defines the attribute.
  ) const noexcept;

  /*!
   * Check if an attribute has been looked-up already.  Explicitly
   * setting the cached value counts as being looked up.
   *
   * @return
   *  - true: The value was looked-up or explicitly cached.
   *  - false: The value was not looked-up or explicitly cached.
   */
  virtual bool app_data_is_looked_up(
    const AppDataTokenBase* token //!< [in] Token that defines the attribute.
  ) const noexcept;

  /*!
   * Check if an attribute is cached, and if it is cached, return the
   * pointer.
   *
   * @return
   *  - true: The value was cached.  *value contains the cached value.
   *  - false: The value was not cached.  *value was not modified.
   */
  template <class data_type, class deleter_type=AppDataTokenDeleter<data_type>>
  bool app_data_check_and_get(
    const AppDataToken<data_type,deleter_type>* token, //!< [in] Token that defines the attribute.
    data_type* data //!< [out] The data value, if cached.
  ) const noexcept;

  /*!
   * Check if an attribute is cached, and if it is cached, return the
   * pointer.  If it is not cached, and the attribute has not yet been
   * looked up in this module's extensions, also look it up in the
   * extensions.  If found, cache the result and return it.
   *
   * @return
   *  - true: The value was cached.  *value contains the cached value.
   *  - false: The value was not cached.  *value was not modified.
   */
  template <class data_type, class deleter_type=AppDataTokenDeleter<data_type>>
  bool app_data_check_lookup_and_get(
    const AppDataToken<data_type,deleter_type>* token, //!< [in] Token that defines the attribute.
    data_type* data //!< [out] The data value, if cached.
  );

  /*!
   * Mark an attribute as having been looked-up already.  Explicitly
   * setting the cached value implicitly marks it as looked up; you
   * might do this when you tried to look it up and didn't find it.
   */
  virtual void app_data_set_looked_up(
    const AppDataTokenBase* token //!< [in] Token that defines the attribute.
  );

  /*!
   * Cache an attribute, giving ownership of the pointer to the
   * YangModule.  If the pointer is still valid when the YangModule gets
   * deleted, the pointer will also be destroyed.
   *
   * @return
   *  - If the value was not previously cached, or if the previous
   *    pointer was not owned by the NcxAppDataCacheLevel, then 0 will
   *    be returned.
   *  - Otherwise, the value was previously cached and owned by
   *    NcxAppDataCacheLevel.  Because the model still exists, we do
   *    not want to make assumptions about the application data's
   *    lifetime; therefore, the previous data will be returned and the
   *    application TAKES OWNERSHIP.  N.B., immediate destruction of
   *    the returned data, without additional application-provided
   *    mutual exclusion, probably CREATES A DATA RACE.
   */
  template <class data_type, class deleter_type=AppDataTokenDeleter<data_type>>
  data_type app_data_set_and_give_ownership(
    const AppDataToken<data_type,deleter_type>* token, //!< [in] the token
    data_type value //!< [in] The data to set
  );

  /*!
   * Cache an attribute, with the application (or some other entity)
   * retaining ownership of the pointer.  When the YangModule gets
   * deleted, nothing will be done with the pointer.
   *
   * @return
   *  - If the value was not previously cached, or if the previous
   *    pointer was not owned by the NcxAppDataCacheLevel, then 0 will
   *    be returned.
   *  - Otherwise, the value was previously cached and owned by
   *    NcxAppDataCacheLevel.  Because the model still exists, we do
   *    not want to make assumptions about the application data's
   *    lifetime; therefore, the previous data will be returned and the
   *    application TAKES OWNERSHIP.  N.B., immediate destruction of
   *    the returned data, without additional application-provided
   *    mutual exclusion, probably CREATES A DATA RACE.
   */
  template <class data_type, class deleter_type=AppDataTokenDeleter<data_type>>
  data_type app_data_set_and_keep_ownership(
    const AppDataToken<data_type,deleter_type>* token, //!< [in] the token
    data_type value //!< [in] The app data item to set
  );

  /*!
   * Check if an attribute is cached, and if it is cached, returns the
   * attribute.
   *
   * @return
   *  - true: The value was cached.  *data contains the cached value.
   *  - false: The value was not cached.  *data was not modified.
   */
  virtual bool app_data_check_and_get(
    const AppDataTokenBase* token, //! [in] Token that defines the attribute.
    intptr_t* data //! [out] The data value, if cached.
  ) const noexcept;

  /*!
   * Check if an attribute is cached, and if it is cached, return the
   * pointer.  If it is not cached, and the attribute has not yet been
   * looked up in this node's extensions, also look it up in the
   * extensions.  If found, cache the result and return it.
   *
   * @return
   *  - true: The value was cached.  *value contains the cached value.
   *  - false: The value was not cached.  *value was not modified.
   */
  virtual bool app_data_check_lookup_and_get(
    const AppDataTokenBase* token, //! [in] Token that defines the attribute.
    intptr_t* data //! [out] The data value, if cached.
  );

  /*!
   * Cache an attribute, giving ownership of the pointer to the
   * YangModule.  If the pointer is still valid when the YangModule gets
   * deleted, the pointer will also be destroyed.
   *
   * @return
   *  - If the value was not previously cached, or if the previous
   *    pointer was not owned by the NcxAppDataCacheLevel, then 0 will
   *    be returned.
   *  - Otherwise, the value was previously cached and owned by
   *    NcxAppDataCacheLevel.  Because the model still exists, we do
   *    not want to make assumptions about the application data's
   *    lifetime; therefore, the previous data will be returned and the
   *    application TAKES OWNERSHIP.  N.B., immediate destruction of
   *    the returned data, without additional application-provided
   *    mutual exclusion, probably CREATES A DATA RACE.
   */
  virtual intptr_t app_data_set_and_give_ownership(
    const AppDataTokenBase* token, //!< [in] the token
    intptr_t data //! [in] The data to cache.
  );

  /*!
   * Cache an attribute, with the application (or some other entity)
   * retaining ownership of the pointer.  When the YangModule gets
   * deleted, nothing will be done with the pointer.
   *
   * @return
   *  - If the value was not previously cached, or if the previous
   *    pointer was not owned by the NcxAppDataCacheLevel, then 0 will
   *    be returned.
   *  - Otherwise, the value was previously cached and owned by
   *    NcxAppDataCacheLevel.  Because the model still exists, we do
   *    not want to make assumptions about the application data's
   *    lifetime; therefore, the previous data will be returned and the
   *    application TAKES OWNERSHIP.  N.B., immediate destruction of
   *    the returned data, without additional application-provided
   *    mutual exclusion, probably CREATES A DATA RACE.
   */
  virtual intptr_t app_data_set_and_keep_ownership(
    const AppDataTokenBase* token, //!< [in] the token
    intptr_t data //! [in] The data to cache.
  );

  /*!
   * Search this node's extensions for the first descriptor that
   * matches the specified namespace and extension names.  This API is
   * ambiguous, because multiple extensions of the same name can be
   * specified in yang.
   *
   * O(n*m) in the number of extensions and the full depth of the uses
   * statements to the nodes originating grouping statement.
   *
   * @return A pointer to the first matching extension descriptor, if
   *   any.  Descriptor is owned by the library.  Caller may use
   *   descriptor until model is destroyed.
   */
  virtual YangExtension* search_extension(
    const char* ns, //!< [in] The extension's namespace.  Cannot be nullptr.
    const char *ext //!< [in] The extension name.  Cannot be nullptr.
  );

  /*!
   * For a module, mark its first level imports as explicity loaded. The
   * management agent loads a composite module, which is a list of imports of
   * all the modules that are needed in the management agent. These modules
   * appear as implicitly loaded modules. For the YANG library to properly
   * work for the management agent, these nodes need to appear as explicitly
   * loaded. This method achieves that
   */
  virtual void mark_imports_explicit() = 0;

  /*!
   * Each YangModule has an associated Protobuf schema. The schema is generated
   * during yangpbc processing. Annotating a module with the protobuf schema
   * allows for seamless conversions from a Yang-valid XML and its associated
   * Protobuf message. This also allows each node in a module have a schema path
   * and other associated information generated during the conversion process.
   */
  rw_status_t register_ypbc_module(
    //! [in] The module schema
    const rw_yang_pb_module_t* ypbc_module);

  /*!
   * If a model has a yangpbc schema attached, retrieve it.
   *
   * @return the ypbc schema if it has been set on the model. nullptr otherwise.
   */
  const rw_yang_pb_module_t* get_ypbc_module();

  /* Get the YangModel that the YangNode is part of
   *
   * @return The YANG Model. Cannot be null. Value owned by Model
   */
  virtual YangModel* get_model() = 0;

  /*!
   * Search this modules's top nodes for a node that matches the specified
   * node name and namespace.
   *
   * O(n) in the number of top-level nodes in the module.
   *
   * @return A pointer to the (first) matching child, if any.  Node is
   *   owned by the library.  Caller may use node until model is
   *   destroyed.
   */
  virtual YangNode* search_node(
    const char* name, //!< [in] Node name to search for.
    const char *ns = nullptr //!< [in] The namespace.  May be nullptr.
  );

  /*!
   * Search the modules to nodes for a node that matches has a protobuf
   * element tag.
   *
   * O(n) in the number of children.
   *
   * @return A pointer to the (first) matching child, if any.  Node is
   *   owned by the library.  Caller may use node until model is
   *   destroyed.
   */
  virtual YangNode* search_node(
    //! [in] tag of the element to find
    const uint32_t element_id
  );
};


/*!
 * Yang model node.  This class describes a single node in the YANG
 * schema model.
 *
 * The node is owned by the model.  The caller can keep a copy of the
 * pointer for as long as the model exists.  The caller is free to
 * assume that the node will not be deleted.  However, the node may be
 * modified, and the attributes (yang configuration, children, type, et
 * cetera...) may change after obtaining the pointer.
 *
 * ATTN: const-correctness!
 *
 * Abstract base class.
 */
class YangNode
: public rw_yang_node_t
{
public:
  //! Trival constructor for abstract base
  YangNode()
  {}

  //! Trival destructor for abstract base
  virtual ~YangNode()
  {}

  // Cannot copy
  YangNode(const YangNode&) = delete;
  YangNode& operator=(const YangNode&) = delete;

public:
  // ATTN: virtual const char* get_reference() = 0;
  // ATTN: virtual const char* get_rift_short_help() = 0;

  /*!
   * Get the yang or yin source file location of the node definition.
   *
   * @return A location string.  Might be empty if location is not
   *   known.
   */
  virtual std::string get_location() = 0;

  /*!
   * Get the yang description for the node.
   *
   * @return The description.  May be empty, but won't be nullptr.
   *   Buffer owned by the model.  Caller may use string until model is
   *   destroyed.
   */
  virtual const char* get_description() = 0;

  /*!
   * Get "short help" for the node.  Short help is intended for a terse
   * display.  It may be defined as a yang extension or automatically
   * generated from the yang description.
   *
   * ATTN: Define extension
   *
   * @return The short help.  May be empty, but won't be nullptr.
   *   Buffer owned by the model.  Caller may use string until model is
   *   destroyed.
   */
  virtual const char* get_help_short();

  /*!
   * Get "full help" for the node.  Full help is intended for a
   * verbose, all-encompassing display.  It may be defined as a yang
   * extension or automatically generated from the yang description,
   * reference, default value, and/or other yang fields.
   *
   * ATTN: Define extension
   *
   * @return The full help.  May be empty, but won't be nullptr.
   *   Buffer owned by the model.  Caller may use string until model is
   *   destroyed.
   */
  virtual const char* get_help_full();

  /*!
   * Get the name of the node (yang identifier).  The name does not
   * include the prefix.
   *
   * @return The name.  Cannot be empty or nullptr.  Buffer owned by
   *   the model.  Caller may use string until model is destroyed.
   */
  virtual const char* get_name() = 0;

  /*!
   * Get the namespace prefix for the node.  The prefix returned is
   * either the prefix actually used in the yang file, or the prefix of
   * the module.
   *
   * @return The name.  Cannot be empty or nullptr.  Buffer owned by
   *   the model.  Caller may use string until model is destroyed.
   */
  virtual const char* get_prefix() = 0;

  /*!
   * Get the namespace for the node.  The namespace is the correct
   * namespace for generating XML output for the node.
   *
     ATTN: This is being output incorrectly in YangNodeNcx for grouping
     fields!
   *
   * @return The name.  Cannot be empty or nullptr.  Buffer owned by
   *   the model.  Caller may use string until model is destroyed.
   */
  virtual const char* get_ns() = 0;

  /*!
   * Get the default value, if there is one.  THIS MAY RETURN nullptr,
   * which indicates that there is no default value.
   *
   * @return The default value.  May be nullptr, which indicates that
   *   there is no default value or that the node cannot have a default
   *   value.  May be empty string, which means the default value is
   *   the empty string.  Buffer owned by the model.  Caller may use
   *   string until model is destroyed.
   */
  virtual const char* get_default_value();

  /*!
   * Get the XML element name, with the default prefix prepended.
   *
   * @return The element name.
   */
  std::string get_xml_element_name();

  /*!
   * Get the XML element name, including namespace declaration if
   * required or if it differs from the parent element (as in the case
   * of an augment).  Does not include the XML square brackets.
   *
   * ATTN: Have an API that produces the XML namespace declarations for
   * a target element and all of its descendents to a certain depth?
   * Should trim the XML sample output quite a bit for heavily
   * augmented trees.
   *
   * @return The element and namespace declaration.
   */
  std::string get_xml_element_start_with_ns(
    /*!
     * If true, require the namespace delcaration regardless of parent
     * namespace.
     */
    bool require_xmlns );

  /*!
   * Get the XPath element name, including prefix.  Might differ from
   * XML element name due to suppression of nodes in XPath.
   *
   * @return The element name.  If empty string, then there is no
   *   element in the XPath for this node.
   */
  std::string get_xpath_element_name();

  /*!
   * Get the XPath element name, including prefix and sample predicates
   * values for listy nodes.
   *
   * @return The element name (and predicates).  If empty string, then
   *   there is no element in the XPath for this node.  This value is a
   *   raw XPath string - it contains appropriate escapes for the
   *   sample predicate values, but does not contain any other escapes.
   */
  std::string get_rw_xpath_element_with_sample_predicate();

  /*!
   * Get a sample XML value for a leaf element.  The sample will be one
   * of the following:
   *  - The default value.
   *  - A sample value for a simple scalar or string.
   *  - An instance of an enum or identity (if one is known).
   *  - A placeholder that is invalid, but serves to identify the kind
   *    of value required.
   *
   * This function will not produce a useful result for empty leafs,
   * anyxml, or complex elements.
   *
   * @return The sample.  Non-leaves will return empty strings, as will
   *   empty leaves.  The value will be the raw XML value, this no XML
   *   escaping applied.  To include in a sample document, XML escaping
   *   must be applied.
   */
  std::string get_xml_sample_leaf_value();

  /*!
   * Get a sample XML document for an element, rooted at the element.
   * The sample will be XML text, with proper escaping applied to the
   * sample values and/or attributes, such that the input could be fed
   * into a XML parser as text.
   *
   * However, this function is intended only to provide a basic
   * example.  Samples might not be valid instance documents, for any
   * of the following reasons:
   *  - Values might not have valid samples (such as
   *    instance-identifier).
   *  - When depth is used, the terminal depth will have elipsis
   *    placeholders.
   *  - When the element contains lists, a second list entry will
   *    always be included, but summarized with an elipsis body.
   *  - All alternatives of a choice or case will be included.
   *  - rpc output elements will have outer tags named after the rpc,
   *    rather than a proper NETCONF rpc response.
   *  - No unique, must, or feature statements are considered.
   *
   * @return A string with the pretty XML instance document, indented
   *   if it contains newlines.
   */
  std::string get_xml_sample_element(
    /*!
     * If the output would contain newlines, use this as the base
     * indentation, in spaces.  Each extra level will add 2 more
     * spaces.
     */
    unsigned indent,
    /*!
     * If greater than zero, recurse into the complex child elements,
     * producing samples for them as well.  Leaves always get sample
     * values.  If zero, complex children are summarized with an
     * elipsis body.
     */
    unsigned to_depth,
    /*!
     * Only include config elements.  If none of the children are
     * config, the sample will be an empty element.
     */
    bool config_only,
    /*!
     * Require the XML namespace declaration on the outer element, even
     * if it matches the parent.  This is most useful on the outer-most
     * element.
     */
    bool require_xmlns );

  /*!
   * Get the REST element name, including module prefix if it differs
   * from the parent element (as in the case of an augment).  Top-level
   * elements always include the module prefix.
   *
   * @return The element name.
   */
  std::string get_rest_element_name();

  /*!
   * Get the RW.REST URI resource path element, including module
   * prefixes where required, and including sample key values for
   * ancestor listy elements.  The sample values will be properly
   * slash-separated and URL escaped, if necessary.
   *
   * @return The path element string.
   */
  std::string get_rwrest_path_element();

  /*!
   * Get a sample RW.REST JSON value for a leaf element.  The sample
   * will be one of the following:
   *  - The default value.
   *  - A sample value for a simple scalar or string.
   *  - An instance of an enum or identity (if one is known).
   *  - A placeholder that is invalid, but serves to identify the kind
   *    of value required.
   *
   * This function will not produce a useful result for empty,
   * instance-identifier, anyxml, or complex elements.
   *
   * @return The sample.  Non-leaves will return empty strings.  In
   *   most other cases, this function will return a properly-typed
   *   JSON value, which can be included in JSON source directly -
   *   i.e., strings will include surrounding quotes.  However,
   *   syntactic validity is not guaranteed - this function's purpose
   *   is simply to provide a relatively reasonable sample.
   */
  std::string get_rwrest_json_sample_leaf_value();

  /*!
   * Get a sample JSON definition for an element, rooted at the
   * element.  The sample will be JSON source code, with proper
   * escaping applied to the sample values, and full wrapping syntax
   * for array and list.
   *
   * However, this function is intended only to provide a basic
   * example.  Samples might not be valid JSON code or valid instances
   * of the element, for any of the following reasons:
   *  - Values might not have valid samples (such as
   *    instance-identifier).
   *  - When depth is used, the terminal depth will have elipsis
   *    placeholders.
   *  - When the element contains lists, a second list entry will
   *    always be included, but summarized with an elipsis.
   *  - All alternatives of a choice or case will be included.
   *  - No unique, must, or feature statements are considered.
   *
   * @return A string with the pretty JSON code sample, indented if it
   *   contains newlines.
   */
  std::string get_rwrest_json_sample_element(
    /*!
     * If the output would contain newlines, use this as the base
     * indentation, in spaces.  Each extra level will add 2 more
     * spaces.
     */
    unsigned indent,
    /*!
     * If greater than zero, recurse into the complex child elements,
     * producing samples for them as well.  Leaves always get sample
     * values.  If zero, complex children are summarized with an
     * elipsis body.
     */
    unsigned to_depth,
    /*!
     * Only include config elements.  If none of the children are
     * config, the sample will be an empty element.
     */
    bool config_only );

  /*!
   * Get the maximum number of elemets for a list or leaf-list.  If the
   * object is not a leaf-list or list, then it will return 1.
   *
   * ATTN: Should other objects return 0?
   *
   * @return The maximum number of elements.  If no maximum was
   *   specified in yang, may return UINT32_MAX.
   */
  virtual uint32_t get_max_elements();

  /*!
   * Get the yang statement type.
   *
   * @return The statement type.  Does not necessarily map directly to
   *   the actual yang statement type, due to processing performed on
   *   the model.
   */
  virtual rw_yang_stmt_type_t get_stmt_type() = 0;

  /*!
   * Determine if the node is a config node.
   *
   * ATTN: What about config-by-descent?
   *
   * @return true if the node is config true.  Otherwise false.
   */
  virtual bool is_config();

  /*!
   * Determine if the node is leafy - if it has one or more value nodes
   * as children.  Yang statement types that are considered leafy by the
   * yang model are:
   *  - anyxml
   *  - leaf
   *  - leaf-list
   *
   * @return true if the node is leafy.  Otherwise false.
   */
  virtual bool is_leafy();

  /*!
   * Determine if the node is listy - if more than one item may be
   * legally accepted for a single node.  Yang statement types that are
   * considered listy by the yang model are:
   *  - list
   *  - leaf-list
   *  - ATTN: leaf of bits type, or with a union member of bits type
   *
   * @return true if the node is listy.  Otherwise false.
   */
  virtual bool is_listy();

  /*!
   * Determine if the node a list key.
   *
   * @return true if the node is a list key.  Otherwise false.
   */
  virtual bool is_key();

  /*!
   * Determine if the node is a list and has keys.
   *
   * @return true if the node has keys.  Otherwise false.
   */
  virtual bool has_keys();

  /*!
   * Determine if the node is a presence container.
   *
   * @return true if the node a presence container.  Otherwise false.
   */
  virtual bool is_presence();

  /*!
   * Determine if the node is mandatory.
   *
   * @return true if the node is mandatory.  Otherwise false.
   */
  virtual bool is_mandatory();

  /*!
   * Determine if the node has a descendnat that has a default value.
   *
   * @return true if the node has a default descendant, false otherwise
   */
  virtual bool has_default();

  /*!
   * Get this node's parent node.  Skips choice and case nodes.
   * Several kinds of nodes may have no node parent: groupings,
   * augments, the root model node.
   *
   * @return A pointer to the parent node, if any.  Node is owned by
   *   the library.  Caller may use node until model is destroyed.  The
   *   children of the node may change after future module loads, due
   *   to augments, so the caller should not cache the children.
   */
  virtual YangNode* get_parent();

  /*!
   * Get this node's first child node, if any.  Skips choice and case
   * nodes.
   *
   * @return A pointer to the first child node, if any.  Node is owned
   *   by the library.  Caller may use node until model is destroyed.
   *   The children of a node may change after future module loads, due
   *   to augments, so the caller should not cache the children.
   */
  virtual YangNode* get_first_child() = 0;

  /*!
   * Get an iterator to the first child of this node, if any.
   *
   * @return The iterator.  The iterator may only be validly compared
   *   to other iterators of this node's children.
   */
  virtual YangNodeIter child_begin();

  /*!
   * Get an iterator to the end of this node's child nodes.  This
   * iterator does not refer to a node.
   *
   * @return The iterator.  The iterator may only be validly compared
   *   to other iterators of this node's children.
   */
  virtual YangNodeIter child_end();

  /*!
   * Get this node's next sibling node, if any.  Skips choice and case
   * nodes, but not their immediate children (the contents of a case
   * node are considered siblings to the choice node's siblings).
   *
   * @return A pointer to the next sibling node, if any.  Node is owned
   *   by the library.  Caller may use node until model is destroyed.
   *   The children of a node may change after future module loads, due
   *   to augments, so the caller should not cache the sibling
   *   relationship.
   */
  virtual YangNode* get_next_sibling() = 0;

  /*!
   * Search this rpc node's children for either "input" or "output".
   *
   * @return A pointer to the child, if any.  Node is owned by the
   *   library.  Caller may use node until model is destroyed.
   */
  virtual YangNode* search_child_rpcio_only(
    /*!
     * [in] Node name to search for.
     */
    const char* name
  );

  /*!
   * Search this node's children for a node that matches the specified
   * node name and namespace.  If the namespace is not specified, then
   * the search can be ambiguous; in this case, the first matching node
   * will be returned.
   *
   * O(n) in the number of children.
   *
   * @return A pointer to the (first) matching child, if any.  Node is
   *   owned by the library.  Caller may use node until model is
   *   destroyed.
   */
  virtual YangNode* search_child(
    /*!
     * [in] Node name to search for.
     */
    const char* name,
    /*!
     * [in] The namespace.  May be nullptr.
     */
    const char *ns = nullptr
  );

  /*!
   * Search this node's children for a node that matches the specified
   * node name and prefix.  If the prefix is not specified, then
   * the search can be ambiguous; in this case, the first matching node
   * will be returned.
   *
   * O(n) in the number of children.
   *
   * @return A pointer to the (first) matching child, if any.  Node is
   *   owned by the library.  Caller may use node until model is
   *   destroyed.
   */
  virtual YangNode* search_child_with_prefix(
    /*!
     * [in] Node name to search for.
     */
    const char* name,
    /*!
     * [in] The XML prefix.  May be nullptr, but then the result is ambiguous.
     */
    const char *prefix
  );

  /*!
   * When a YANG model is decorated with an associated Protobuf schema,
   * each node in the model has an associated tag id. The method of generation
   * of protobuf ensures that the tags of all children for a node are unique.
   * This makes it possible to search the children of a YANG node with a
   * protobuf tag and namespace to find an child yang node
   *
   * @return the child node if found, nullptr otherwise
   */
  virtual YangNode* search_child(
    /*!
     * [in] The numeric namespace ID.
     */
    uint32_t system_ns_id,
    /*!
     * [in] Protobuf tag assigned to the element
     */
    uint32_t element_tag
  );

  /*!
   * Search this node's children for a node that matches the specified
   * node name and namespace, if the nodes original name were mangled
   * according to rules used by Yangpbc methods. The namespace is optional,
   * and  the search can be ambiguous; in this case, the first matching node
   * will be returned.
   *
   * O(n) in the number of children.
   *
   * @return A pointer to the (first) matching child, if any.  Node is
   *   owned by the library.  Caller may use node until model is
   *   destroyed.
   */
  virtual YangNode* search_child_by_mangled_name(
    /*!
     * [in] Node name to search for.
     */
    const char* name,
    /*!
     * [in] The namespace.  May be nullptr.
     */
    const char *ns = nullptr
  );

  /*!
   * Get the type descriptor for the node, if applicable.
   *
   * @return The type descriptor.  nullptr if the node is not leafy.
   *   Type descriptor is owned by the library.  Caller may use the
   *   descriptor until model is destroyed.
   */
  virtual YangType* get_type();

  /*!
   * Get the first value descriptor of the node's type, if the node is
   * leafy.  Non-leafy nodes have no values, and return nullptr.  Even
   * leafy nodes might have no values, such as empty types (ATTN:) or
   * anyxml.
   *
   * @return A pointer to the first value descriptor, if any.
   *   Descriptor is owned by the library.  Caller may use descriptor
   *   until model is destroyed.  The values of a type may change after
   *   future module loads, due to the creation of additional
   *   identities, so the caller should not cache the list.
   */
  virtual YangValue* get_first_value();

  /*!
   * Get an iterator to the first value descriptor of this node's type,
   * if any.
   *
   * @return The iterator.  The iterator may only be validly compared
   *   to other iterators of this node's type's values.
   */
  virtual YangValueIter value_begin();

  /*!
   * Get an iterator to the end of this node's type's value
   * descriptors.  This iterator does not refer to a value descriptor.
   *
   * @return The iterator.  The iterator may only be validly compared
   *   to other iterators of this node's type's values.
   */
  virtual YangValueIter value_end();

  /*!
   * Get the first key descriptor of the node, if it is a list node and
   * if it has keys.  Non-list nodes have no keys, and so return
   * nullptr.
   *
   * @return A pointer to the first key descriptor, if any.  Descriptor
   *   is owned by the library.  Caller may use descriptor until model
   *   is destroyed.
   */
  virtual YangKey* get_first_key();

  /*!
   * Get an iterator to the first key descriptor of this node, if any.
   *
   * @return The iterator.  The iterator may only be validly compared
   *   to other iterators of this node's keys.
   */
  virtual YangKeyIter key_begin();

  /*!
   * Get an iterator to the end of this node's key descriptors.  This
   * iterator does not refer to a key descriptor.
   *
   * @return The iterator.  The iterator may only be validly compared
   *   to other iterators of this node's keys.
   */
  virtual YangKeyIter key_end();

  /*!
   * Get the YangNode corresponding to a particular position, say 2nd key
   * of a listy node. The use of this API is during iteration of trees.
   * Keys that are essentially paths are specified with this implicit
   * position (Confd hkey path, rw keyspec are examples)
   *
   * @return YangNode if the list has at least position keys. nullptr otherwise
   *
   * @param [in] position/index of the key within the list
   */
  virtual YangNode* get_key_at_index(uint8_t position);

  /*!
   * Get the first extension descriptor of the node, if any.
   *
   * ATTN: Currently may also extend to the node's incorporating uses
   * statement, and/or the node's defining grouping statement.  ATTN:
   * This capability will be removed once uses and groupings become
   * first-class ojbects int he yang model.
   *
   * @return A pointer to the first extension descriptor, if any.
   *   Descriptor is owned by the library.  Caller may use descriptor
   *   until model is destroyed.
   */
  virtual YangExtension* get_first_extension();

  /*!
   * Get an iterator to the first extension descriptor of this node, if
   * any.
   *
   * @return The iterator.  The iterator may only be validly compared
   *   to other iterators of this node's extensions.
   */
  virtual YangExtensionIter extension_begin();

  /*!
   * Get an iterator to the end of this node's extension descriptors.
   * This iterator does not refer to a extension descriptor.
   *
   * @return The iterator.  The iterator may only be validly compared
   *   to other iterators of this node's extensions.
   */
  virtual YangExtensionIter extension_end();

  /*!
   * Search this node's extensions for the first descriptor that
   * matches the specified namespace and extension names.  This API is
   * ambiguous, because multiple extensions of the same name can be
   * specified in yang.
   *
   * O(n*m) in the number of extensions and the full depth of the uses
   * statements to the nodes originating grouping statement.
   *
   * @return A pointer to the first matching extension descriptor, if
   *   any.  Descriptor is owned by the library.  Caller may use
   *   descriptor until model is destroyed.
   */
  virtual YangExtension* search_extension(
    const char* ns, //!< [in] The extension's namespace.  Cannot be nullptr.
    const char *ext //!< [in] The extension name.  Cannot be nullptr.
  );

  /*!
   * Get the module this node is instantiated in.  This is the module
   * that contains the object, which may be different from the module
   * where the node was originally defined if the node participates in
   * an augment or uses.
   *
   * @return The module descriptor.  Descriptor is owned by the
   *   library.  Caller may use descriptor until model is destroyed.
   */
  virtual YangModule* get_module() = 0;

  /*!
   * Get the module this node was originally defined in.  This is the
   * module that contains the yang node definition statement,
   * regardless of whether the node was ultimately incorporated in the
   * parent object via a uses or augment.
   *
   * @return The module descriptor.  Descriptor is owned by the
   *   library.  Caller may use descriptor until model is destroyed.
   */
  virtual YangModule* get_module_orig();

  /*!
   * Get this node's augment descriptor, if the node was added via augment.
   *
   * @return A pointer to the augment descriptor, if any.  nullptr for
   *   nodes that were not added via augment.  Owned by the model;
   *   caller may use descriptor until model is destroyed.
   */
  virtual YangAugment* get_augment();

  /*!
   * Get the next grouping descriptor in the module.  Descriptor owned
   * by the library.  Caller may use descriptor until model is
   * destroyed.
   *
   * ATTN: DOES NOT belong in YangNode!  There should be a YangGrouping
   * derived from YangNode.
   */
  virtual YangNode* get_next_grouping();

  /*!
   * Get the uses descriptor, which applies to nodes that were added
   * via the yang uses statement.
   *
   * @return The descriptor is owned by the model.  Will be nullptr if
   *   the node does not come from uses.  Caller may use uses
   *   descriptor until model is destroyed.
   */
  virtual YangUses* get_uses();

  /*!
   * Get this node's grouping
   *
   * @return A pointer to the defining grouping if the node is from a uses
   *   statement and has not been augmented or refined -  nullptr otherwise
   *   Caller may use node until model is destroyed.
   */
  virtual YangNode* get_reusable_grouping();

  /*!
   * Get this node's grouping module
   *
   * @return A pointer to the module of the defining grouping if the node is from a uses
   *   statement  and has not been augmented or refined - nullptr otherwise
   *   Caller may use node until model is destroyed.
   */
  virtual YangModule* get_reusable_grouping_module();

  /*!
   * Determine if a value is a leading substring of the node name.
   * Exact match is considered a valid leading substring.
   */
  virtual bool matches_prefix(
    const char* prefix_string //!< [in] The string to compare
  );

  /*!
   * Get the node's containing case, if it is a member of a case.
   *
   * @return The case, if any.  nullptr if not a member of a case.
   */
  virtual YangNode* get_case();

  /*!
   * Get the node's containing choice, if it is a member of a choice.
   *
   * @return The choice, if any.  nullptr if not a member of a choice.
   */
  virtual YangNode* get_choice();

  /*!
   * Get the choice node's default case.
   *
   * @returns The default case node. nullptr if not a choice or there is no
   * default case.
   */
  virtual YangNode* get_default_case();


  /*!
   * Two YANG nodes can be mutually exclusive. This happens when both
   * nodes are children of case nodes, and the case nodes are mutually
   * exclusive.
   * The case nodes are not part of the schema tree. To test for the exclusivity
   * of two nodes, the nodes have to be walked to the root choice nodes. This
   * is not a problem in the usual case, but in YANG schema where cases and
   * choices recurse, there is no easy algorithm to test for exclusivity.
   * Two nodes are mutually exclusive if both of them are share a common choice
   * ancestor, but use two different cases under that choice.
   * Only immediate children of case nodes are checked for conflict - It is
   * assumed that a parse or XML tree would not traverse further down if there
   * was a conflict. ie, as 2 trees are walked, conflicts will always be
   * discovered at children of case nodes, and checks further down are not
   * required.
   */
  virtual bool is_conflicting_node(YangNode *other);

  /*!
   * Determine if a value is a valid for the leafy node.  A value is
   * valid if it is accepted as valid for any of the node's types'
   * value descriptors.  As per RFC 6020, when a leaf is defined as a
   * union, the first matching type is the defined type, even if
   * multiple types may match.  So, technically, this API cannot be
   * ambiguous.
   *
   * ATTN: Currently unimplemented!  Should pass off to type...
   *
   * @return The status of the parse.
   *  - RW_STATUS_SUCCESS if a match was found.
   *  - RW_STATUS_FAILURE if no match was found, or not leafy.
   */
  virtual rw_status_t parse_value(
    const char* value_string, //!< [in] The value string to check.
    YangValue** matched = nullptr //!< [out] The value descriptor that matched. May be nullptr.
  );


  /*!
   * Get the rift-protoc-c field name for this node.  The result may be
   * name mangled from the yang node name, or may be overridden by a
   * yang extension.  This API does not guarantee uniqueness.
   *
   * ATTN: list extension
   *
   * @return The field name.  Cannot be nullptr or empty string.
   *   Buffer owned by the model.  Caller may use string until model is
   *   destroyed.
   */
  virtual const char* get_pbc_field_name();


  /*!
   * Mark node as on the path to a CLI mode.  This cannot be undone
   *
     ATTN: This should have been entirely replaced by AppData!
   * ATTN: Should there be a way to undo it?
   * ATTN: Should return an error when not supported?
   */
  virtual void set_mode_path();

  /*!
   * Mark node as on the path to a CLI mode, and then all the
   * parents up to the root
   *
     ATTN: This should have been entirely replaced by AppData!
   * ATTN: Should there be a way to undo it?
   * ATTN: Should return an error when not supported?
   */
  virtual void set_mode_path_to_root();

  /*!
   * Determine if a node is on a path to a CLI mode.
   *
     ATTN: This should have been entirely replaced by AppData!
   * ATTN: Is a mode-node itself marked as a mode path?
   *
   * @return true if the node leads to a mode node.
   */
  virtual bool is_mode_path();

  /*!
   * Determine if a node maps to a yang rpc statement.
   *
   * @return true if the node is a rpc.
   */
  virtual bool is_rpc();

  /*!
   * Determine if a node maps to a yang rpc input section.
   *
   * @return true if the node is a rpc input.
   */
  virtual bool is_rpc_input();

  /*!
   * Determine if a node maps to a yang rpc output section.
   *
   * @return true if the node is a rpc output.
   */
  virtual bool is_rpc_output();

  /*!
   * Determine if a node is a descendant of a notification statement.
   *
   * @return true if the node is a descendant of a notification.
   */
  virtual bool is_notification();

  /*!
   * Determine if a node is the root node of the module
   *
   * @return true if the node is the root node of a module
   */
  virtual bool is_root();

  /*!
   * For an enumeration, return the string representation of the
   * integer value
   *
   * @return string representing enumeration value for enumerations
   *   null string for other types
   */
  virtual std::string get_enum_string(
    uint32_t value /*!< Value that is to be converted to string
                       representation */
  );

  /*!
   * For a leaf or leaf-list node, if the type is leafref, this will
   * return the node that the leafref refers to.  The referred to node
   * may also be a leafref - possibly an indeterminately long chain of
   * leafrefs, to the ultimate and final leaf (which must be of a type
   * other than leafref).
   *
   * @return A pointer to the leafref's referecend node.  nullptr if
   *   not a leaf with leafref type.
   */
  virtual YangNode* get_leafref_ref();

  /*!
   * For a leaf or leaf-list node, if the type is leafref, this will
   * return the path of the referenced node as string.
   * It will not try to stringify the referred node if that also is
   * a leafref.
   *
   * @return Referred path as string.
   */
  virtual std::string get_leafref_path_str();

  /*!
   * Converts the yang node to its json representation. This
   * will return the json representation as std::string.
   */
  virtual std::string to_json_schema(bool pretty_print = false);

  /*
   * ATTN: Should the app-data API actually be just an access to get
   * the cache object?  And the the caller can make accesses to the
   * cache directly?  Maybe, also, accessing the cache creates it on
   * demand, instead of always wasting the memory?
   */


  /*!
   * Check if an attribute is cached.
   *
   * @return
   *  - true: The value was cached.
   *  - false: The value was not cached.
   */
  virtual bool app_data_is_cached(
    const AppDataTokenBase* token //!< [in] Token that defines the attribute.
  ) const noexcept;

  /*!
   * Check if an attribute has been looked-up already.  Explicitly
   * setting the cached value counts as being looked up.
   *
   * @return
   *  - true: The value was looked-up or explicitly cached.
   *  - false: The value was not looked-up or explicitly cached.
   */
  virtual bool app_data_is_looked_up(
    const AppDataTokenBase* token //!< [in] Token that defines the attribute.
  ) const noexcept;

  /*!
   * Check if an attribute is cached, and if it is cached, return the
   * pointer.
   *
   * @return
   *  - true: The value was cached.  *value contains the cached value.
   *  - false: The value was not cached.  *value was not modified.
   */
  template <class data_type, class deleter_type=AppDataTokenDeleter<data_type>>
  bool app_data_check_and_get(
    const AppDataToken<data_type,deleter_type>* token, //!< [in] Token that defines the attribute.
    data_type* data //!< [out] The data value, if cached.
  ) const noexcept;

  /*!
   * Check if an attribute is cached, and if it is cached, return the
   * pointer.  If it is not cached, and the attribute has not yet been
   * looked up in this node's extensions, also look it up in the
   * extensions.  If found, cache the result and return it.
   *
   * @return
   *  - true: The value was cached.  *value contains the cached value.
   *  - false: The value was not cached.  *value was not modified.
   */
  template <class data_type, class deleter_type=AppDataTokenDeleter<data_type>>
  bool app_data_check_lookup_and_get(
    const AppDataToken<data_type,deleter_type>* token, //!< [in] Token that defines the attribute.
    data_type* data //!< [out] The data value, if cached.
  );

  /*!
   * Mark an attribute as having been looked-up already.  Explicitly
   * setting the cached value implicitly marks it as looked up; you
   * might do this when you tried to look it up and didn't find it.
   */
  virtual void app_data_set_looked_up(
    const AppDataTokenBase* token //!< [in] Token that defines the attribute.
  );

  /*!
   * Cache an attribute, giving ownership of the pointer to the
   * YangNode.  If the pointer is still valid when the YangNode gets
   * deleted, the pointer will also be destroyed.
   *
   * @return
   *  - If the value was not previously cached, or if the previous
   *    pointer was not owned by the NcxAppDataCacheLevel, then 0 will
   *    be returned.
   *  - Otherwise, the value was previously cached and owned by
   *    NcxAppDataCacheLevel.  Because the model still exists, we do
   *    not want to make assumptions about the application data's
   *    lifetime; therefore, the previous data will be returned and the
   *    application TAKES OWNERSHIP.  N.B., immediate destruction of
   *    the returned data, without additional application-provided
   *    mutual exclusion, probably CREATES A DATA RACE.
   *
   *    In the event this function fails to set the appdata, the passed
   *    appdata will be returned back to the caller.
   */
  template <class data_type, class deleter_type=AppDataTokenDeleter<data_type>>
  data_type app_data_set_and_give_ownership(
    const AppDataToken<data_type,deleter_type>* token, //!< [in] the token
    data_type value //!< [in] The data to set
  );

  /*!
   * Cache an attribute, with the application (or some other entity)
   * retaining ownership of the pointer.  When the YangNode gets
   * deleted, nothing will be done with the pointer.
   *
   * @return
   *  - If the value was not previously cached, or if the previous
   *    pointer was not owned by the NcxAppDataCacheLevel, then 0 will
   *    be returned.
   *  - Otherwise, the value was previously cached and owned by
   *    NcxAppDataCacheLevel.  Because the model still exists, we do
   *    not want to make assumptions about the application data's
   *    lifetime; therefore, the previous data will be returned and the
   *    application TAKES OWNERSHIP.  N.B., immediate destruction of
   *    the returned data, without additional application-provided
   *    mutual exclusion, probably CREATES A DATA RACE.
   */
  template <class data_type, class deleter_type=AppDataTokenDeleter<data_type>>
  data_type app_data_set_and_keep_ownership(
    const AppDataToken<data_type,deleter_type>* token, //!< [in] the token
    data_type value //!< [in] The app data item to set
  );

  /*!
   * Check if an attribute is cached, and if it is cached, returns the
   * attribute.
   *
   * @return
   *  - true: The value was cached.  *data contains the cached value.
   *  - false: The value was not cached.  *data was not modified.
   */
  virtual bool app_data_check_and_get(
    const AppDataTokenBase* token, //! [in] Token that defines the attribute.
    intptr_t* data //! [out] The data value, if cached.
  ) const noexcept;

  /*!
   * Check if an attribute is cached, and if it is cached, return the
   * pointer.  If it is not cached, and the attribute has not yet been
   * looked up in this node's extensions, also look it up in the
   * extensions.  If found, cache the result and return it.
   *
   * @return
   *  - true: The value was cached.  *value contains the cached value.
   *  - false: The value was not cached.  *value was not modified.
   */
  virtual bool app_data_check_lookup_and_get(
    const AppDataTokenBase* token, //! [in] Token that defines the attribute.
    intptr_t* data //! [out] The data value, if cached.
  );

  /*!
   * Cache an attribute, giving ownership of the pointer to the
   * YangNode.  If the pointer is still valid when the YangNode gets
   * deleted, the pointer will also be destroyed.
   *
   * @return
   *  - If the value was not previously cached, or if the previous
   *    pointer was not owned by the NcxAppDataCacheLevel, then 0 will
   *    be returned.
   *  - Otherwise, the value was previously cached and owned by
   *    NcxAppDataCacheLevel.  Because the model still exists, we do
   *    not want to make assumptions about the application data's
   *    lifetime; therefore, the previous data will be returned and the
   *    application TAKES OWNERSHIP.  N.B., immediate destruction of
   *    the returned data, without additional application-provided
   *    mutual exclusion, probably CREATES A DATA RACE.
   */
  virtual intptr_t app_data_set_and_give_ownership(
    const AppDataTokenBase* token, //!< [in] the token
    intptr_t data //! [in] The data to cache.
  );

  /*!
   * Cache an attribute, with the application (or some other entity)
   * retaining ownership of the pointer.  When the YangNode gets
   * deleted, nothing will be done with the pointer.
   *
   * @return
   *  - If the value was not previously cached, or if the previous
   *    pointer was not owned by the NcxAppDataCacheLevel, then 0 will
   *    be returned.
   *  - Otherwise, the value was previously cached and owned by
   *    NcxAppDataCacheLevel.  Because the model still exists, we do
   *    not want to make assumptions about the application data's
   *    lifetime; therefore, the previous data will be returned and the
   *    application TAKES OWNERSHIP.  N.B., immediate destruction of
   *    the returned data, without additional application-provided
   *    mutual exclusion, probably CREATES A DATA RACE.
   */
  virtual intptr_t app_data_set_and_keep_ownership(
    const AppDataTokenBase* token, //!< [in] the token
    intptr_t data //! [in] The data to cache.
  );

  /*!
   * Get the YangModel that the YangNode is part of
   *
   * @return The YANG Model. Cannot be null. Value owned by Model
   */
  virtual YangModel* get_model() = 0;

  /*!
   * Gets a unique tag number for this node  that is unique across all loaded modules/namespaces.
   * ATTN: Specify details of namespace + ncx encoding
   *
   * @return A unique tag number for this node.
   */
  virtual uint32_t get_node_tag()  = 0;

  /*!
   * Decorate a YANG Node with data from yangpbc conversions
   */
  virtual rw_status_t register_ypbc_msgdesc(
    const rw_yang_pb_msgdesc_t* ypbc_msgdesc);

  /*!
   * If the node has a yangpbc schema attached, retrieve it.
   *
   * @return the ypbc schema if it has been set on the model. nullptr otherwise.
   */
  const rw_yang_pb_msgdesc_t* get_ypbc_msgdesc();
};





/*!
 * Yang key descriptor.  This class describes a single key in the YANG
 * schema model.
 *
 * The descriptor is owned by the model.  The caller can keep a pointer
 * to the descriptor for as long as the model exists.  The caller is
 * free to assume that the descriptor will not be deleted.
 *
 * Abstract base class.
 */
class YangKey
: public rw_yang_key_t
{
public:
  YangKey() {}
  virtual ~YangKey() {}

  // Cannot copy
  YangKey(const YangKey&) = delete;
  YangKey& operator=(const YangKey&) = delete;

public:
  virtual YangNode* get_list_node() = 0;
  virtual YangNode* get_key_node() = 0;
  virtual YangKey* get_next_key() = 0;
};


/*!
 * Yang augment descriptor.  This class describes a single module
 * augment in the yang model.
 *
 * The descriptor is owned by the model.  The caller can keep a pointer
 * to the descriptor for as long as the model exists.  The caller is
 * free to assume that the descriptor will not be deleted.
 *
 * Abstract base class.
 */
class YangAugment
: public rw_yang_augment_t
{
public:
  YangAugment() {}
  virtual ~YangAugment() {}

  // Cannot copy
  YangAugment(const YangAugment&) = delete;
  YangAugment& operator=(const YangAugment&) = delete;

public:

  /*!
   * Get the source file location of the augment statement.
   */
  virtual std::string get_location() = 0;

  /*!
   * Get the owning module's descriptor.  Descriptor owned by the
   * library.  Caller may use the descriptor until model is destroyed.
   */
  virtual YangModule* get_module() = 0;

  /*!
   * Get the augment's target node.  Node owned by the library.  Caller
   * may use node until model is destroyed.  THE NODE MAY BE A choice
   * OR case!
   */
  virtual YangNode* get_target_node() = 0;

  /*!
   * Get the next augment descriptor in the module.  Descriptor owned by
   * the library.  Caller may use descriptor until model is destroyed.
   */
  virtual YangAugment* get_next_augment() = 0;

  /*!
   * Get the first extension descriptor.  The descriptor is owned by the
   * model.  Will be nullptr if there are no extensions.  Caller may use
   * extension descriptor until model is destroyed.
   */
  virtual YangExtension* get_first_extension();

  /*!
   * Return an iterator pointing at the first extension descriptor.
   */
  virtual YangExtensionIter extension_begin();

  /*!
   * Return an iterator pointing to the end of the extension list.
   */
  virtual YangExtensionIter extension_end();
};


/*!
 * Yang Uses descriptor.  This class describes a single module
 * uses in the yang model.
 *
 * The descriptor is owned by the model.  The caller can keep a pointer
 * to the descriptor for as long as the model exists.  The caller is
 * free to assume that the descriptor will not be deleted.
 *
 * Abstract base class.
 */
class YangUses
: public rw_yang_uses_t
{
public:
  YangUses() {}
  virtual ~YangUses() {}

  // Cannot copy
  YangUses(const YangUses&) = delete;
  YangUses& operator=(const YangUses&) = delete;

public:
  virtual std::string get_location() = 0;

  virtual YangNode* get_grouping() = 0;
  virtual bool is_augmented() = 0;
  virtual bool is_refined() = 0;
};


/*!
 * Yang type descriptor.  This class describes a single leafy type in
 * the YANG schema model - leaf, leaf-list or anyxml.  The type may be
 * shared across multiple leaf or leaf-list entries in the model.
 *
 * A type may accept multiple values as follows.  A leaf's yang type
 * may be identityref, bits, or enumeration, which means that the
 * accepted values are a list of specific strings.  A leaf's yang type
 * may be a union, in which case it may accept multiple types.  A
 * leaf's yang type may be empty, in which case it will not take any
 * value, per se.
 *
 * The descriptor is owned by the model.  The caller can keep a pointer
 * to the descriptor for as long as the model exists.  The caller is
 * free to assume that the descriptor will not be deleted.
 *
 * Abstract base class.
 */
class YangType
: public rw_yang_type_t
{
public:
  YangType() {}
  virtual ~YangType() {}

  // Cannot copy
  YangType(const YangType&) = delete;
  YangType& operator=(const YangType&) = delete;

public:
  virtual std::string get_location() = 0;
  virtual const char* get_description() { return ""; /* ATTN */ }
  virtual const char* get_help_short() { return ""; /* ATTN */ }
  virtual const char* get_help_full() { return ""; /* ATTN */ }
  virtual const char* get_prefix() = 0;
  virtual const char* get_ns() = 0;
  virtual rw_yang_leaf_type_t get_leaf_type() = 0;
  virtual bool is_imported() {return false;}
  virtual YangValue* get_first_value() = 0;
  virtual YangValueIter value_begin();
  virtual YangValueIter value_end();
  virtual rw_status_t parse_value(const char* value_string, YangValue** matched);
  virtual unsigned count_matches(const char* value_string);
  virtual unsigned count_partials(const char* value_string);

  virtual YangExtension* get_first_extension() { return nullptr; }
  virtual YangExtensionIter extension_begin();
  virtual YangExtensionIter extension_end();
  virtual YangTypedEnum* get_typed_enum() { return nullptr; }
  // ATTN: virtual YangModule* get_module() = 0;

  // Get the fraction-digits value if the type is decimal64
  virtual uint8_t get_dec64_fraction_digits() { return 0; }
};


/*!
 * Yang value node.  This class describes a single value node in the
 * YANG schema model.  A value node is an anyxml, leaf, or leaf-list
 * entry.
 *
 * A leaf's yang type may be identityref, bits, or enumeration, which
 * means that the accepted values are also keywords.  Those values will
 * be available in the model as value nodes, so it is possible for
 * value nodes to have siblings.
 *
 * The node is owned by the model.  The caller can keep a copy of the
 * pointer for as long as the model exists.  The caller is free to
 * assume that the node will not be deleted.  However, the node may be
 * modified, and the attributes (yang configuration, children, type, et
 * cetera...) may change after obtaining the pointer.
 *
 * ATTN: identities can be extended by loading additional models.  Will
 * that change the typedef here?
 *
 * ATTN: for leafref and instance-identifer, need to be able to walk
 * the dom (or, at least the model), possible backwards (through
 * parents).
 *
 * ATTN: it would be nice for tab-completion in CLI for leafref and
 * instance-identifier to be able to complete on the known leafs or
 * model+keys
 *
 * Abstract base class.
 */
class YangValue
: public rw_yang_value_t
{
public:
  YangValue() {}
  virtual ~YangValue() {}

  // Cannot copy
  YangValue(const YangValue&) = delete;
  YangValue& operator=(const YangValue&) = delete;

public:
  virtual const char* get_name() = 0;
  virtual std::string get_location() { return ""; /* ATTN */ }
  virtual const char* get_description() = 0;
  virtual const char* get_help_short() { return ""; /* ATTN */ }
  virtual const char* get_help_full() { return ""; /* ATTN */ }
  // ATTN: virtual const char* get_syntax() = 0;
  // ATTN: Get min, max, units...
  // ATTN: virtual const char* get_rift_short_help() = 0;
  virtual const char* get_prefix() = 0;
  virtual const char* get_ns() = 0;
  virtual rw_yang_leaf_type_t get_leaf_type() = 0;
  virtual uint64_t get_max_length() { return 1; }

  virtual YangValue* get_next_value() = 0; // Does not include unions - they are expanded
  virtual rw_status_t parse_value(const char* value_string) = 0;
  virtual rw_status_t parse_partial(const char* value_string) = 0;
  virtual bool is_keyword() { return false; }
  // ATTN: Need a way to distinguish which bits belong together

  virtual YangExtension* get_first_extension() { return nullptr; }
  virtual YangExtensionIter extension_begin();
  virtual YangExtensionIter extension_end();

  // for a enumeration, get the integer value specified in the YANG model
  // for bits type, get the position value specified in the YANG model
  virtual uint32_t get_position() {return 0;};

  // ATTN: virtual YangModule* get_module() = 0;
};


/*!
 * Yang extension descriptor.  This class describes a single extension
 * associated with a YangNode, YangType, or YangValue.
 *
 * For nodes imported as part of a group, the list of extensions will
 * include those defined in the node itself, the uses statement that
 * imported the node, and the node's original grouping statement.
 * ATTN: augments?
 *
 * The descriptor is owned by the model.  The caller can keep a pointer
 * to the descriptor for as long as the model exists.  The caller is
 * free to assume that the descriptor will not be deleted.
 *
 * Abstract base class.
 */
class YangExtension
: public rw_yang_extension_t
{
public:
  YangExtension() {}
  virtual ~YangExtension() {}

  // Cannot copy
  YangExtension(const YangExtension&) = delete;
  YangExtension& operator=(const YangExtension&) = delete;

public:
  // ATTN: virtual const char* get_reference() = 0;
  virtual std::string get_location() = 0;
  virtual std::string get_location_ext() = 0;
  virtual const char* get_value() = 0;
  virtual const char* get_description_ext() = 0;
  virtual const char* get_name() = 0;
  virtual const char* get_module_ext() = 0;
  virtual const char* get_prefix() = 0;
  virtual const char* get_ns() = 0;
  virtual YangExtension* get_next_extension() = 0;
  virtual YangExtension* search(const char* ns, const char* ext);

  //!! Matching extension?
  /*!
   * Determine if this extension is the specified extension.
   * @return true if the extension matches
   */
  virtual bool is_match(
    const char* ns, //!< The namespace.
    const char* ext //! The extension name.
  );

  // ATTN: virtual YangModule* get_module() = 0;
};


/*!
 * Yang typedef-ed enum descriptor.  This class describes a single
 * named, typedefed eumeration at the module level.
 *
 * The descriptor is owned by the model.  The caller can keep a pointer
 * to the descriptor for as long as the model exists.  The caller is
 * free to assume that the descriptor will not be deleted.
 *
   ATTN: This really should derive from YangType!
 *
 * Abstract base class.
 */
class YangTypedEnum
: public rw_yang_typed_enum_t
{
public:
  YangTypedEnum() {}
  virtual ~YangTypedEnum() {}

  // Cannot copy
  YangTypedEnum(const YangTypedEnum&) = delete;
  YangTypedEnum& operator=(const YangTypedEnum&) = delete;

public:
  // ATTN: virtual const char* get_reference() = 0;

  virtual const char* get_description() = 0;
  virtual const char* get_name() = 0;
  virtual std::string get_location() = 0;

  virtual const char* get_ns() = 0;
  virtual YangTypedEnum* get_next_typed_enum() = 0;

  virtual YangValue* get_first_enum_value() { return nullptr; }
  virtual YangExtension* get_first_extension() { return nullptr; }
  virtual YangValueIter enum_value_begin();
  virtual YangValueIter enum_value_end();
};


/*!
 * YangModule iterator type.  Iterates over modules.  Module iterators
 * may be safely compared ONLY when created by the same YangModel!
 */
class YangModuleIter
{
  private:
    //! The underlying pointer
    YangModule* p_;

  public:
    //! Create an module iterator pointing at nothing.
    YangModuleIter() : p_(nullptr) {}

    //! Create a module iterator located at the given module.
    explicit YangModuleIter(YangModule* module) : p_(module) {}

    //! Copy a module iterator.
    YangModuleIter(const YangModuleIter& other) : p_(other.p_) {}

    //! Assign a module iterator
    YangModuleIter& operator=(const YangModuleIter& other) { p_ = other.p_; return *this; }

    //! Compare two module iterators.
    bool operator==(const YangModuleIter& other) const { return p_ == other.p_; }

    //! Compare two module iterators.
    bool operator!=(const YangModuleIter& other) const { return !(p_ == other.p_); }

    //! Pre-increment module iterator
    YangModuleIter& operator++() { RW_ASSERT(p_); p_ = p_->get_next_module(); return *this; }

    //! Post-increment module iterator
    YangModuleIter operator++(int) { RW_ASSERT(p_); YangModuleIter old(*this); p_ = p_->get_next_module(); return old; }

    //! Dereference iterator.
    YangModule& operator*() { return *p_; }

    //! Dereference iterator.
    YangModule* operator->() { return p_; }

    typedef YangModule value_type;
    typedef void difference_type;
    typedef std::forward_iterator_tag iterator_category;
    typedef YangModule* pointer;
    typedef YangModule& reference;
};


/*!
 * YangAugment iterator.  Iterates over lists of augments.  Augment
 * iterators may be safely compared ONLY when created by the same
 * YangModule!
 */
class YangAugmentIter
{
  private:
    //! The underlying pointer
    YangAugment* p_;

  public:
    //! Create an augment iterator pointing at nothing.
    YangAugmentIter() : p_(nullptr) {}

    //! Create a augment iterator located at the first augment
    explicit YangAugmentIter(YangModule* module)
    : p_(nullptr)
    {
      if (module) {
        p_ = module->get_first_augment();
      }
    }

    //! Create a augment iterator located at the given module iterator.
    explicit YangAugmentIter(YangModuleIter ymi)
    : p_(nullptr)
    {
      if (ymi != YangModuleIter()) {
        p_ = ymi->get_first_augment();
      }
    }

    //! Create a augment iterator located at the given augment.
    explicit YangAugmentIter(YangAugment* augment) : p_(augment) {}

    //! Copy a augment iterator.
    YangAugmentIter(const YangAugmentIter& other) : p_(other.p_) {}

    //! Assign a augment iterator
    YangAugmentIter& operator=(const YangAugmentIter& other) { p_ = other.p_; return *this; }

    //! Compare two augment iterators.
    bool operator==(const YangAugmentIter& other) const { return p_ == other.p_; }

    //! Compare two augment iterators.
    bool operator!=(const YangAugmentIter& other) const { return !(p_ == other.p_); }

    //! Pre-increment augment iterator
    YangAugmentIter& operator++() { RW_ASSERT(p_); p_ = p_->get_next_augment(); return *this; }

    //! Post-increment augment iterator
    YangAugmentIter operator++(int) { RW_ASSERT(p_); YangAugmentIter old(*this); p_ = p_->get_next_augment(); return old; }

    //! Dereference iterator.
    YangAugment& operator*() { return *p_; }

    //! Dereference iterator.
    YangAugment* operator->() { return p_; }

    typedef YangAugment value_type;
    typedef void difference_type;
    typedef std::forward_iterator_tag iterator_category;
    typedef YangAugment* pointer;
    typedef YangAugment& reference;
};


/*!
 * YangNode iterator type.  Iterates over siblings.  Node iterators may
 * be safely compared ONLY when created by the same YangModule or
 * parent YangNode.
 */
class YangNodeIter
{
  private:
    //! The underlying pointer
    YangNode* p_;

  public:
    //! Create an node iterator pointing at nothing.
    YangNodeIter() : p_(nullptr) {}

    //! Create a node iterator located at the given node.
    explicit YangNodeIter(YangNode* node) : p_(node) {}

    //! Copy a node iterator.
    YangNodeIter(const YangNodeIter& other) : p_(other.p_) {}

    //! Assign a node iterator
    YangNodeIter& operator=(const YangNodeIter& other) { p_ = other.p_; return *this; }

    //! Compare two node iterators.
    bool operator==(const YangNodeIter& other) const { return p_ == other.p_; }

    //! Compare two node iterators.
    bool operator!=(const YangNodeIter& other) const { return !(p_ == other.p_); }

    //! Pre-increment node iterator
    YangNodeIter& operator++() { RW_ASSERT(p_); p_ = p_->get_next_sibling(); return *this; }

    //! Post-increment node iterator
    YangNodeIter operator++(int) { RW_ASSERT(p_); YangNodeIter old(*this); p_ = p_->get_next_sibling(); return old; }

    //! Dereference iterator.
    YangNode& operator*() { return *p_; }

    //! Dereference iterator.
    YangNode* operator->() { return p_; }

    //! Get at the underlying pointer.
    YangNode* raw() { return p_; }

    typedef YangNode value_type;
    typedef void difference_type;
    typedef std::forward_iterator_tag iterator_category;
    typedef YangNode* pointer;
    typedef YangNode& reference;
};


/*!
 * YangKey iterator.  Iterates over lists of keys.  Key iterators may
 * be safely compared ONLY when created by the same YangNode.
 */
class YangKeyIter
{
  private:
    //! The underlying pointer
    YangKey* p_;

  public:
    //! Create an key iterator pointing at nothing.
    YangKeyIter() : p_(nullptr) {}

    //! Create a key iterator located at the first key
    explicit YangKeyIter(YangNode* node)
    : p_(nullptr)
    {
      if (node) {
        p_ = node->get_first_key();
      }
    }

    //! Create a key iterator located at the given key.
    explicit YangKeyIter(YangKey* key) : p_(key) {}

    //! Create a key iterator located at the given node iterator.
    explicit YangKeyIter(YangNodeIter yni)
    : p_(nullptr)
    {
      if (yni != YangNodeIter()) {
        p_ = yni->get_first_key();
      }
    }

    //! Copy a key iterator.
    YangKeyIter(const YangKeyIter& other) : p_(other.p_) {}

    //! Assign a key iterator
    YangKeyIter& operator=(const YangKeyIter& other) { p_ = other.p_; return *this; }

    //! Compare two key iterators.
    bool operator==(const YangKeyIter& other) const { return p_ == other.p_; }

    //! Compare two key iterators.
    bool operator!=(const YangKeyIter& other) const { return !(p_ == other.p_); }

    //! Pre-increment key iterator
    YangKeyIter& operator++() { RW_ASSERT(p_); p_ = p_->get_next_key(); return *this; }

    //! Post-increment key iterator
    YangKeyIter operator++(int) { RW_ASSERT(p_); YangKeyIter old(*this); p_ = p_->get_next_key(); return old; }

    //! Dereference iterator.
    YangKey& operator*() { return *p_; }

    //! Dereference iterator.
    YangKey* operator->() { return p_; }

    typedef YangKey value_type;
    typedef void difference_type;
    typedef std::forward_iterator_tag iterator_category;
    typedef YangKey* pointer;
    typedef YangKey& reference;
};


/*!
 * YangValue iterator type.  Iterates over all the valid values in a
 * type.  There may be multiple values in a type because of:
 *  - bits leaf type: multiple bits can be defined
 *  - enumeration leaf type: multiple values can be defined
 *  - union: multiple simple types may be accepted
 *  - identityref: multiple identities may be defined
 *
 * Value iterators may be safely compared ONLY when created by the same
 * YangType.
 */
class YangValueIter
{
  private:
    //! The underlying pointer
    YangValue* p_;

  public:
    //! Create an value iterator pointing at nothing.
    YangValueIter() : p_(nullptr) {}

    //! Create a value iterator located at the first value
    explicit YangValueIter(YangNode* node)
    : p_(nullptr)
    {
      if (node) {
        YangType* type = node->get_type();
        if (type) {
          p_ = type->get_first_value();
        }
      }
    }

    //! Create a value iterator located at the first value
    explicit YangValueIter(YangType* type)
    : p_(nullptr)
    {
      if (type) {
        p_ = type->get_first_value();
      }
    }

    //! Create a value iterator located at the given value.
    explicit YangValueIter(YangValue* value) : p_(value) {}

    //! Create a value iterator located at the given value.
    explicit YangValueIter(YangNodeIter yni)
    : p_(nullptr)
    {
      if (yni != YangNodeIter()) {
        YangType* type = yni->get_type();
        if (type) {
          p_ = type->get_first_value();
        }
      }
    }

    //! Copy a value iterator.
    YangValueIter(const YangValueIter& other) : p_(other.p_) {}

    //! Assign a value iterator
    YangValueIter& operator=(const YangValueIter& other) { p_ = other.p_; return *this; }

    //! Compare two value iterators.
    bool operator==(const YangValueIter& other) const { return p_ == other.p_; }

    //! Compare two value iterators.
    bool operator!=(const YangValueIter& other) const { return !(p_ == other.p_); }

    //! Pre-increment value iterator
    YangValueIter& operator++() { RW_ASSERT(p_); p_ = p_->get_next_value(); return *this; }

    //! Post-increment value iterator
    YangValueIter operator++(int) { RW_ASSERT(p_); YangValueIter old(*this); p_ = p_->get_next_value(); return old; }

    //! Dereference iterator.
    YangValue& operator*() { return *p_; }

    //! Dereference iterator.
    YangValue* operator->() { return p_; }

    typedef YangValue value_type;
    typedef void difference_type;
    typedef std::forward_iterator_tag iterator_category;
    typedef YangValue* pointer;
    typedef YangValue& reference;
};


/*!
 * YangExtension iterator.  Iterates over lists of extensions.
 *
 * Extension iterators may be safely compared ONLY when created by the
 * same YangModule, YangAugment, YangNode, YangType, or YangValue.
 */
class YangExtensionIter
{
  private:
    //! The underlying pointer
    YangExtension* p_;

  public:
    //! Create an extension iterator pointing at nothing.
    YangExtensionIter() : p_(nullptr) {}

    //! Create a extension iterator located at the first extension
    explicit YangExtensionIter(YangNode* node)
    : p_(nullptr)
    {
      if (node) {
        p_ = node->get_first_extension();
      }
    }

    //! Create a extension iterator located at the first extension
    explicit YangExtensionIter(YangModule* module)
    : p_(nullptr)
    {
      if (module) {
        p_ = module->get_first_extension();
      }
    }

    //! Create a extension iterator located at the given extension.
    explicit YangExtensionIter(YangExtension* extension) : p_(extension) {}

    //! Create a extension iterator located at the given node iterator.
    explicit YangExtensionIter(YangNodeIter yni)
    : p_(nullptr)
    {
      if (yni != YangNodeIter()) {
        p_ = yni->get_first_extension();
      }
    }

    //! Copy a extension iterator.
    YangExtensionIter(const YangExtensionIter& other) : p_(other.p_) {}

    //! Assign a extension iterator
    YangExtensionIter& operator=(const YangExtensionIter& other) { p_ = other.p_; return *this; }

    //! Compare two extension iterators.
    bool operator==(const YangExtensionIter& other) const { return p_ == other.p_; }

    //! Compare two extension iterators.
    bool operator!=(const YangExtensionIter& other) const { return !(p_ == other.p_); }

    //! Pre-increment extension iterator
    YangExtensionIter& operator++() { RW_ASSERT(p_); p_ = p_->get_next_extension(); return *this; }

    //! Post-increment extension iterator
    YangExtensionIter operator++(int) { RW_ASSERT(p_); YangExtensionIter old(*this); p_ = p_->get_next_extension(); return old; }

    //! Dereference iterator.
    YangExtension& operator*() { return *p_; }

    //! Dereference iterator.
    YangExtension* operator->() { return p_; }

    typedef YangExtension value_type;
    typedef void difference_type;
    typedef std::forward_iterator_tag iterator_category;
    typedef YangExtension* pointer;
    typedef YangExtension& reference;
};

/*!
 * YangTypedEnum iterator.  Iterates over lists of typed enumerations at module level
 */
class YangTypedEnumIter
{
  private:
    //! The underlying pointer
    YangTypedEnum* p_;

  public:
    //! Create an typed enum iterator pointing at nothing.
    YangTypedEnumIter() : p_(nullptr) {}

    //! Create a typed enum iterator located at the first typed enum
    explicit YangTypedEnumIter(YangModule* module)
    : p_(nullptr)
    {
      if (module) {
        p_ = module->get_first_typed_enum();
      }
    }

    //! Create a typed enum iterator located at the given module iterator.
    explicit YangTypedEnumIter(YangModuleIter ymi)
    : p_(nullptr)
    {
      if (ymi != YangModuleIter()) {
        p_ = ymi->get_first_typed_enum();
      }
    }

    //! Create a typed enum iterator located at the given typed enum.
    explicit YangTypedEnumIter(YangTypedEnum* typed_enum) : p_(typed_enum) {}

    //! Copy a typed enum iterator.
    YangTypedEnumIter(const YangTypedEnumIter& other) : p_(other.p_) {}

    //! Assign a typed enum iterator
    YangTypedEnumIter& operator=(const YangTypedEnumIter& other) { p_ = other.p_; return *this; }

    //! Compare two typed enum iterators.
    bool operator==(const YangTypedEnumIter& other) const { return p_ == other.p_; }

    //! Compare two typed enum iterators.
    bool operator!=(const YangTypedEnumIter& other) const { return !(p_ == other.p_); }

    //! Pre-increment typed enum iterator
    YangTypedEnumIter& operator++() { RW_ASSERT(p_); p_ = p_->get_next_typed_enum(); return *this; }

    //! Post-increment typed enum iterator
    YangTypedEnumIter operator++(int) { RW_ASSERT(p_); YangTypedEnumIter old(*this); p_ = p_->get_next_typed_enum(); return old; }

    //! Dereference iterator.
    YangTypedEnum& operator*() { return *p_; }

    //! Dereference iterator.
    YangTypedEnum* operator->() { return p_; }

    typedef YangTypedEnum value_type;
    typedef void difference_type;
    typedef std::forward_iterator_tag iterator_category;
    typedef YangTypedEnum* pointer;
    typedef YangTypedEnum& reference;
};

/*!
 * YangImport iterator.  Iterates over imports in a module
 */
class YangImportIter
{
  private:
    //! The underlying pointer
    YangModule** p_;

  public:
    //! Create an import iterator pointing at nothing.
    YangImportIter() : p_(nullptr) {}

    //! Create a import iterator  for the module's imports
    explicit YangImportIter(YangModule** module)
    : p_(nullptr)
    {
        p_ = module;
    }

    //! Copy ian import iterator.
    YangImportIter(const YangImportIter& other) : p_(other.p_) {}

    //! Assign an import iterator
    YangImportIter& operator=(const YangImportIter& other) { p_ = other.p_; return *this; }

    //! Compare two import iterators.
    bool operator==(const YangImportIter& other) const { return p_ == other.p_; }

    //! Compare two import iterators.
    bool operator!=(const YangImportIter& other) const { return !(p_ == other.p_); }

    //! Pre-increment import iterator
    YangImportIter& operator++() { RW_ASSERT(p_); p_++; if ((*p_) == nullptr) p_ = nullptr; return *this; }

    //! Post-increment import itertaor
    YangImportIter operator++(int) { RW_ASSERT(p_); YangImportIter old(*this); p_++;  if ((*p_) == nullptr) p_ = nullptr; return old; }

    //! Dereference iterator.
    YangModule& operator*() { return **p_; }

    //! Dereference iterator.
    YangModule* operator->() { return *p_; }

    typedef YangModule  value_type;
    typedef void difference_type;
    typedef std::forward_iterator_tag iterator_category;
    typedef YangModule* pointer;
    typedef YangModule& reference;
};

/*!
 * YangGrouping iterator.  Iterates over lists of groupings at module level
 */
class YangGroupingIter
{
  private:
    //! The underlying pointer
    YangNode* p_;

  public:

    //! Create a grouping iterator pointing at nothing.
    YangGroupingIter() : p_(nullptr) {}

    //! Create a grouping iterator located at the first grouping in the module
    explicit YangGroupingIter(YangModule* module)
    : p_(nullptr)
    {
      if (module) {
        p_ = module->get_first_grouping();
      }
    }

    //! Create a typed enum iterator located at the given module iterator.
    explicit YangGroupingIter(YangModuleIter ymi)
    : p_(nullptr)
    {
      if (ymi != YangModuleIter()) {
        p_ = ymi->get_first_grouping();
      }
    }

    //! Create a grouping iterator located at the given grouping
    explicit YangGroupingIter(YangNode* grouping) : p_(grouping) {}

    //! Copy a grouping iterator.
    YangGroupingIter(const YangGroupingIter& other) : p_(other.p_) {}

    //! Assign a grouping iterator
    YangGroupingIter& operator=(const YangGroupingIter& other) { p_ = other.p_; return *this; }

    //! Compare two grouping iterators.
    bool operator==(const YangGroupingIter& other) const { return p_ == other.p_; }

    //! Compare two grouping iterators.
    bool operator!=(const YangGroupingIter& other) const { return !(p_ == other.p_); }

    //! Pre-increment typed grouping iter
    YangGroupingIter& operator++() { RW_ASSERT(p_); p_ = p_->get_next_grouping(); return *this; }

    //! Post-increment typed grouping iter
    YangGroupingIter operator++(int) { RW_ASSERT(p_); YangGroupingIter old(*this); p_ = p_->get_next_grouping(); return old; }

    //! Dereference iterator.
    YangNode& operator*() { return *p_; }

    //! Dereference iterator.
    YangNode* operator->() { return p_; }

    typedef YangNode value_type;
    typedef void difference_type;
    typedef std::forward_iterator_tag iterator_category;
    typedef YangNode* pointer;
    typedef YangTypedEnum& reference;
};


template <class data_type, class deleter_type>
inline bool YangModel::app_data_get_token(
  const char* ns,
  const char* ext,
  AppDataToken<data_type,deleter_type>* token)
{
  RW_STATIC_ASSERT(sizeof(data_type) == sizeof(intptr_t));
  AppDataTokenBase base(ns,ext);
  bool retval = this->app_data_get_token(
    ns,
    ext,
    static_cast<AppDataTokenBase::deleter_f>(&deleter_type::deleter),
    &base);
  *static_cast<AppDataTokenBase*>(token) = base;
  return retval;
}

template <class data_type, class deleter_type>
bool YangNode::app_data_check_and_get(
  const AppDataToken<data_type,deleter_type>* token,
  data_type* data
) const noexcept
{
  intptr_t intptr_data = 0;
  bool retval = this->app_data_check_and_get(static_cast<const AppDataTokenBase*>(token), &intptr_data);
  if (retval) {
   *data = reinterpret_cast<data_type>(intptr_data);
  }
  return retval;
}

template <class data_type, class deleter_type>
bool YangNode::app_data_check_lookup_and_get(
  const AppDataToken<data_type,deleter_type>* token,
  data_type* data
)
{
  intptr_t intptr_data = 0;
  bool retval = this->app_data_check_lookup_and_get(static_cast<const AppDataTokenBase*>(token), &intptr_data);
  if (retval) {
   *data = reinterpret_cast<data_type>(intptr_data);
  }
  return retval;
}

template <class data_type, class deleter_type>
data_type YangNode::app_data_set_and_give_ownership(
  const AppDataToken<data_type,deleter_type>* token,
  data_type data
)
{
  intptr_t intptr_data = this->app_data_set_and_give_ownership(
    static_cast<const AppDataTokenBase*>(token),
    reinterpret_cast<intptr_t>(data));
  return reinterpret_cast<data_type>(intptr_data);
}

template <class data_type, class deleter_type>
data_type YangNode::app_data_set_and_keep_ownership(
  const AppDataToken<data_type,deleter_type>* token,
  data_type data
)
{
  intptr_t intptr_data = this->app_data_set_and_keep_ownership(
    static_cast<const AppDataTokenBase*>(token),
    reinterpret_cast<intptr_t>(data));
  return reinterpret_cast<data_type>(intptr_data);
}

template <class data_type, class deleter_type>
bool YangModule::app_data_check_and_get(
  const AppDataToken<data_type,deleter_type>* token,
  data_type* data
) const noexcept
{
  intptr_t intptr_data = 0;
  bool retval = this->app_data_check_and_get(static_cast<const AppDataTokenBase*>(token), &intptr_data);
  if (retval) {
   *data = reinterpret_cast<data_type>(intptr_data);
  }
  return retval;
}

template <class data_type, class deleter_type>
bool YangModule::app_data_check_lookup_and_get(
  const AppDataToken<data_type,deleter_type>* token,
  data_type* data
)
{
  intptr_t intptr_data = 0;
  bool retval = this->app_data_check_lookup_and_get(static_cast<const AppDataTokenBase*>(token), &intptr_data);
  if (retval) {
   *data = reinterpret_cast<data_type>(intptr_data);
  }
  return retval;
}

template <class data_type, class deleter_type>
data_type YangModule::app_data_set_and_give_ownership(
  const AppDataToken<data_type,deleter_type>* token,
  data_type data
)
{
  intptr_t intptr_data = this->app_data_set_and_give_ownership(
    static_cast<const AppDataTokenBase*>(token),
    reinterpret_cast<intptr_t>(data));
  return reinterpret_cast<data_type>(intptr_data);
}

template <class data_type, class deleter_type>
data_type YangModule::app_data_set_and_keep_ownership(
  const AppDataToken<data_type,deleter_type>* token,
  data_type data
)
{
  intptr_t intptr_data = this->app_data_set_and_keep_ownership(
    static_cast<const AppDataTokenBase*>(token),
    reinterpret_cast<intptr_t>(data));
  return reinterpret_cast<data_type>(intptr_data);
}


} /* namespace rw_yang */

namespace std {
  // needed for unordered std containers

  template <> struct hash<rw_yang::YangNode*>
  {
    size_t operator()(rw_yang::YangNode* const & x) const
    {
      return reinterpret_cast<size_t>(x);
    }
  };

  template <> struct equal_to<rw_yang::YangNode*>
  {
    bool operator()(rw_yang::YangNode* const & left, rw_yang::YangNode* const & right) const
    {
      return left == right;
    }
  };
}


#endif /* __cplusplus */

__BEGIN_DECLS

/*****************************************************************************/
// Static strings exported by the library

// Protobuf extension string constants.
extern const char* RW_YANG_PB_MODULE;
extern const char* RW_YANG_PB_EXT_MOD_PACKAGE;
extern const char* RW_YANG_PB_EXT_MOD_GLOBAL;

extern const char* RW_YANG_PB_EXT_MSG_NEW;
extern const char* RW_YANG_PB_EXT_MSG_NAME;
extern const char* RW_YANG_PB_EXT_MSG_FLAT;
extern const char* RW_YANG_PB_EXT_MSG_PROTO_MAX_SIZE;
extern const char* RW_YANG_PB_EXT_MSG_TAG_BASE;
extern const char* RW_YANG_PB_EXT_MSG_TAG_RESERVE;

extern const char* RW_YANG_PB_EXT_FLD_NAME;
extern const char* RW_YANG_PB_EXT_FLD_INLINE;
extern const char* RW_YANG_PB_EXT_FLD_INLINE_MAX;
extern const char* RW_YANG_PB_EXT_FLD_STRING_MAX;
extern const char* RW_YANG_PB_EXT_FLD_TAG;
extern const char* RW_YANG_PB_EXT_FLD_TYPE;
extern const char* RW_YANG_PB_EXT_FLD_C_TYPE;

extern const char* RW_YANG_PB_EXT_PACKAGE_NAME;

extern const char* RW_YANG_PB_EXT_ENUM_NAME;
extern const char* RW_YANG_PB_EXT_ENUM_TYPE;
extern const char* RW_YANG_PB_EXT_FILE_PBC_INCLUDE;

extern const char* RW_YANG_PB_EXT_FIELD_MERGE_BEHAVIOR;


// RW.CLI extension string constants.
extern const char* RW_YANG_CLI_MODULE;

extern const char* RW_YANG_CLI_EXT_NEW_MODE;
extern const char* RW_YANG_CLI_EXT_PLUGIN_API;
extern const char* RW_YANG_CLI_EXT_CLI_PRINT_HOOK;
extern const char* RW_YANG_CLI_EXT_SHOW_KEY_KW;


// RW.UtCli extension string constants.
extern const char* RW_YANG_UTCLI_MODULE;

extern const char* RW_YANG_UTCLI_EXT_CALLBACK_ARGV;

// RW.Notify extensions for log events.
extern const char* RW_YANG_NOTIFY_MODULE;
extern const char* RW_YANG_NOTIFY_EXT_LOG_COMMON;
extern const char* RW_YANG_NOTIFY_EXT_LOG_EVENT_ID;



__END_DECLS


#endif // RW_YANGTOOLS_YANGMODEL_H_

/*! @} */
