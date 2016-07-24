
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



/*!
 * @file rw_pb.h
 * @author Tom Seidenberg
 * @date 2014/12/03
 * @brief RiftWare protobuf support macros
 */

#ifndef RW_YANGTOOLS_PB_H_
#define RW_YANGTOOLS_PB_H_

#include <sys/cdefs.h>
#include <protobuf-c/rift-protobuf-c.h>
#include <stdbool.h>


/*!
 * @defgroup RWPB RWPB: Access to yang and proto data
 * @{
 */

/*!
 * @addtogroup RWPB
 * Each meta-data identifier follows a consistent naming convention.
 * The identifier starts with (or contains) a single lowercase
 * character separated with underscores that defines the "kind" of
 * identifier.  "Kind" roughly defines how the identifier gets used and
 * what it can stand for.  The defined kinds are:
 *
 *  - c - Numeric or symbolic constant.  Can be used generally where a
 *        C numeric constant can be used.  These things are suitable
 *        for use in initialization expressions.
 *  - f - A function or function-like macro.  These can be called, but
 *        you probably cannot take their address.
 *  - g - A pointer to some global data.  May or may not be const.  May
 *        or may not be static.
 *  - i - RESERVED to avoid name collisions with RWPB_i_* macros.
 *  - m - A special-purpose, metadata, or multi-statement macro.  Don't
 *        use this for things that are technically macros, but
 *        semantically one of the other things in this list.
 *  - n - Identifier name (such as struct field, but not a typedef) or
 *        an identifier fragment intended for token-pasting.  All
 *        things in this categories must be suitable for token-pasting.
 *  - s - String constant.  Must include the outer double quotes.  Must
 *        be suitable for string concatenation, so it cannot be a
 *        pointer to a string constant.
 *  - t - A C typename or typedef.  Might be a complex type (such as
 *        pointer to function), so you cannot assume that this can be
 *        token-pasted.
 */

/*!
 * @addtogroup RWPB
 * General macro arguments:
 *
 * - var_
 *   A local variable name containing a whole struct.  Depending on
 *   the macro, the variable may be defined and initialized, or just
 *   used as an argument.
 *
 * - ptr_
 *   A local variable name or expression that evaluates to a pointer
 *   of the appropriate struct type.
 *
 * - path_
 *   The yangpbc message path identifier name.  The identifier starts
 *   with the camel-cased yang module name, followed by a classifier
 *   type, and finally the rooted path to the target element.
 *
 *   There are 3 forms of schema identifier:
 *    - \c SCHEMA_MODULE_CATEGORY_TOP...
 *    - \c SCHEMA_CATEGORY_TOP...
 *    - \c SCHEMA_MSGNEW...
 *    .
 *
 *   SCHEMA is the CamelCased yang module name of the yang module that
 *   was compiled to obtain the global schema.  This may be a module
 *   that defines the application's actual schema, or a dummy module
 *   that simply imports multiple modules, that together, make up the
 *   application's actual schema.
 *
 *   MODULE is the CamelCased yang module name of the yang module that
 *   defines the TOP element.  It may be SCHEMA or one of the SCHEMA
 *   module's (transitive) imports.  Two levels of module are needed
 *   because different modules may define the same element names -
 *   disambiguation is needed.  When SCHEMA and MODULE are the same,
 *   an extra alias, with the redundant MODULE suppressed, will be
 *   available.
 *
 *   CATEGORY identifies the element type of TOP.  CATEGORY may be one
 *   of the following strings:
 *    - \c notif - TOP is a notification element.
 *    - \c input - TOP is a rpc's input element.  TOP is named after the rpc.
 *    - \c output - TOP is a rpc's output element.  TOP is named after the rpc.
 *    - \c data - TOP is a data or config element.
 *    - \c config - TOP is just the config portion of the tree.  ATTN: Not supported.
 *    - \c group - TOP is the name of a module-level grouping.
 *    .
 *
 *   TOP... is the CamelCased module-level element, followed by the
 *   entire yang element path (all elements CamelCased) to the target,
 *   with each element separated by underscores.
 *
 *   MSGNEW is a rwpb:msg-new extension.  If there is exactly one such
 *   element in the schema (across the config, data, rpc, and
 *   notification hierarchies), then SCHEMA_MSGNEW...  will be an
 *   alias for that one element's full form
 *   (SCHEMA_MODULE_CLASS_TOP...).  This is a convenience to the
 *   programmer (to get shorter names).
 *
 *   Examples:
 *    - \c SchemaMod_ImportedMod_data_Top_Mid_Low
 *    - \c ImportedMod_data_Top_Mid_Low
 *    - \c ImportedMod_MsgNew_Low
 *    .
 */

__BEGIN_DECLS


/*****************************************************************************/
/*!
 * @defgroup RWPB_Schema Schema Information
 * @{
 */

/*!
 * Get the yangpbc schema descriptor for schema-root module schema_.
 * The result is pointer to the the global const variable.
 *
 * @param schema_ - The yangpbc schema identifier name.
 */
#define RWPB_G_SCHEMA_YPBCSD(schema_) \
  (&(RWPB_i_g_schema(schema_)))

/*! @} */



/*****************************************************************************/
/*!
 * @defgroup RWPB_Module Module Information
 * @{
 */

/*!
 * Get the yangpbc module descriptor for schema module mod_.  The
 * result is pointer to the the global const variable.
 *
 * @param module_ - The yangpbc module identifier name or an alias.
 */
#define RWPB_G_MODULE_YPBCMD(module_) \
  (&(RWPB_i_g_module(module_)))

/*! @} */



/*****************************************************************************/
/*!
 * @defgroup RWPB_Enum Enum Information
 * @{
 */

/*!
 * Get the typename for an enum of type path_.  The result can be used
 * anywhere where an enum type can be used.
 *
 * @param path_ - The yangpbc enum path identifier name.
 */
#define RWPB_E(path_) \
  RWPB_i_t_enum(path_)

/*! @} */



/*****************************************************************************/
/*!
 * @defgroup RWPB_Message Message Information
 * @{
 *
 * Message identifiers:
 *
 * - t_msg_pbcm
 *   The rift-protobuf-c message typedef.
 *
 * - t_msg_pbcc
 *   The rift-protobuf-c message closure typedef.
 *
 * - t_msg_msgdesc
 *   The yangpbc message descriptor typedef.
 *
 * - g_msg_msgdesc
 *   The yangpbc message ypbc_msgdesc global variable name.
 *
 * - g_msg_pbcmd
 *   The rift-protobuf-c message ProtobufCMessageDescriptor global
 *   variable name.
 *
 * - f_msg_init
 *   The name of the init function for t_msg_pbcm.  It has the
 *   prototype:
 *
 *       t_msg_pbcm* f_msg_init(
 *         t_msg_pbcm* msg)
 *
 *   If msg is not NULL, then the initialization will initialize the
 *   specified message.  If msg is NULL, then a message will be
 *   allocated and initialized.  In either case, f_msg_init returns
 *   the message.
 *
 * - f_msg_safecast
 *   The name of the safe-cast function for t_msg_pbcm.  It has the
 *   prototype:
 *
 *       t_msg_pbcm* f_msg_safecast(
 *         void* msg)
 *
 *   The input msg will be validated that it is not NULL and that it
 *   has the correct descriptor pointer.  If valid, the pointer will
 *   be returned.  Otherwise, it will assert.
 */

/*!
 * Get the typename for the message of type path_.  The result can be
 * used anywhere where a type can be used.  The type will not be const
 * and will not be a pointer.
 *
 * @param path_ - The yangpbc message path identifier name.
 */
#define RWPB_T_MSG(path_) \
  RWPB_i_t_message_pbcm(path_)

/*!
 * Get the rift-protobuf-c message descriptor for type path_.  The
 * result is pointer to the the global const variable.
 *
 * @param path_ - The yangpbc message path identifier name.
 */
#define RWPB_G_MSG_PBCMD(path_) \
  (&(RWPB_i_g_message_pbcmd(path_)))

/*!
 * Get the yangpbc message descriptor for type path_.  The result is
 * pointer to the the global const variable.
 *
 * @param path_ - The yangpbc message path identifier name.
 */
#define RWPB_G_MSG_YPBCD(path_) \
  (&(RWPB_i_g_message_msgdesc(path_)))

/*!
 * Get the yangpbc rpc descriptor for input or output type path_.  The
 * result is pointer to the the global const variable.
 *
 * @param path_ - The yangpbc message path identifier name.
 */
#define RWPB_G_MSG_YANG_RPCDEF(path_) \
  (&(RWPB_i_g_message_msgdesc(path_).ypbc_rpcdef))

/*!
 * (Re)Initialize a message of type path_.  If still initialized from a
 * previous call, the previous contents will be ignored and no memory
 * will be freed.  Upon return, the descriptor pointer will have been
 * set and the message will contain all default values.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param ptr_ - A pointer of the message type.  Must be the
 *   fully-qualified type; it cannot be just a ProtobufCMessage
 *   pointer.
 */
#define RWPB_F_MSG_INIT(path_, ptr_) \
  (RWPB_i_f_message_init(path_)((ptr_)))

/*!
 * Free the contents of a message of type path_, but not the message
 * itself.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param ptr_ - A pointer of the message type.
 * @param alloc_ - A pointer to a protobuf-c allocation.  Use NULL for
 *   the default.
 */
#define RWPB_F_MSG_FREE_KEEP_BODY(path_, ptr_, alloc_) \
  (protobuf_c_message_free_unpacked_usebody((alloc_), &(ptr_)->base))

/*!
 * Free a message of type path_, and the message's contents.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param ptr_ - A pointer of the message type.
 * @param alloc_ - A pointer to a protobuf-c allocation.  Use NULL for
 *   the default.
 */
#define RWPB_F_MSG_FREE_AND_BODY(path_, ptr_, alloc_) \
  (protobuf_c_message_free_unpacked((alloc_), &(ptr_)->base))

/*!
 * Declare, define, and initialize a local message variable of type
 * path_.  After this statement, a variable with the name var_ will
 * exist.  The variable will have the descriptor pointer set correctly.
 * All the fields will be set to their default values.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param var_ - The variable to declare and initialize.
 */
#define RWPB_M_MSG_DECL_INIT(path_, var_) \
  RWPB_i_t_message_pbcm(path_) var_; \
  (RWPB_i_f_message_init(path_)(&var_))

/*!
 * Declare, define, and allocate a message pointer variable of type
 * path_.  After this statement, a pointer with the name var_ will
 * exist.  The variable will point to a newly allocated copy of the
 * message.  All the fields will be set to their default values.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param var_ - The variable to declare and initialize.
 */
#define RWPB_M_MSG_DECL_PTR_ALLOC(path_, var_) \
  RWPB_i_t_message_pbcm(path_)* var_ \
    = (RWPB_i_f_message_init(path_)(NULL))

/*!
 * Creates a GI boxed type for path, initializes it to hold message,
 * and saves the result as the new variable ptr.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param message_ - The message to hold
 * @param ptr_ - The pointer to the result
 */
#define RWPB_M_MSG_DECL_UPTR_ALLOC(path_, var_) \
  UniquePtrProtobufCFree<RWPB_i_t_message_pbcm(path_)>::uptr_t var_( \
    RWPB_i_f_message_init(path_)(NULL) ); \
  RW_ASSERT(var_.get())

/*!
 * Declare a local message pointer variable and cast a generic
 * ProtobufCMessage pointer to it.  The macro asserts if message is not
 * of the specified type.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param var_ - The variable to declare and initialize.
 * @param msg_ - The ProtobufCMessage pointer.
 */
#define RWPB_M_MSG_DECL_CAST(path_, var_, msg_) \
  RWPB_i_t_message_pbcm(path_)* var_ = (RWPB_T_MSG(path_) *)(msg_); \
  RW_ASSERT(var_->base.descriptor == RWPB_G_MSG_PBCMD(path_))

/*!
 * Declare a local message pointer variable and cast a generic
 * ProtobufCMessage pointer to it.  The macro asserts if message is not
 * of the specified type.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param var_ - The variable to declare and initialize.
 * @param msg_ - The ProtobufCMessage pointer.
 */
#define RWPB_M_MSG_DECL_SAFE_CAST(path_, var_, msg_) \
  RWPB_i_t_message_pbcm(path_)* var_ = (RWPB_T_MSG(path_) *)(msg_); \
  if (var_->base.descriptor != RWPB_G_MSG_PBCMD(path_)) { \
    var_ = NULL; \
  }

/*!
   Allocate and return a message of type path_.  The return value will
   be a newly allocated copy of the message.  All the fields will be
   set to their default values.
 *
 * @param path_ - The yangpbc message path identifier name.
 */
#define RWPB_F_MSG_ALLOC(path_) \
  (RWPB_i_f_message_init(path_)(NULL))

/*!
 * Cast a generic ProtobufCMessage pointer to ProtobufCMessage pointer
 * of type path_.  The macro asserts if the protoc message is not of
 * the specified type.  For a more safer cast, the SAFE_CAST version of the
 * macro should be used.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param msg_ - The ProtobufCMessage pointer.
 */
#define RWPB_M_MSG_CAST(path_, msg_) \
  ({ \
    RW_ASSERT(msg_); \
    RW_ASSERT(msg_->descriptor == RWPB_G_MSG_PBCMD(path_)); \
    (RWPB_T_MSG(path_) *)(msg_); \
  })

/*!
 * Cast a generic ProtobufCMessage pointer to ProtobufCMessage pointer
 * of type path_.  The safer version of the above macro.  If the msg_
 * is not of type path_, the cast is not attemped and NULL is returned.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param msg_ - The ProtobufCMessage pointer.
 */
#define RWPB_M_MSG_SAFE_CAST(path_, msg_) \
  ({ \
    RWPB_T_MSG(path_)* local_ = NULL; \
     if ((msg_) && ((msg_)->descriptor == RWPB_G_MSG_PBCMD(path_))) \
       local_ = (RWPB_T_MSG(path_) *)(msg_);\
     local_; \
   })

/*!
 * Unpack the given bytes/serialzied data to ProtobufCMessage of type
 * path_.  A ProtobufCMessage pointer of type path_ is returned on
 * success.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param alloc_ - A pointer to a protobuf-c allocation.  Use NULL for
 *   the default.
 * @len_ - Length of the serialized data.
 * @data_ - The serialized buffer pointer.
 */
#define RWPB_F_MSG_UNPACK(path_, alloc_, len_, data_) \
  ((RWPB_T_MSG(path_) *)(protobuf_c_message_unpack((alloc_), RWPB_G_MSG_PBCMD(path_), len_, data_)))

/*!
 * Unpack the given bytes/serialized data into msg_, a ProtobufCMessage
 * pointer.  Returns the unpacked protoc message of type path_.  On
 * success, the returned pointer is same as msg_.  Returns NULL if
 * there is an unpack failure.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param alloc_ - A pointer to a protobuf-c allocation.  Use NULL for
 *   the default.
 * @len_ - Length of the serialized data.
 * @data_ - The serialized data pointer.
 * @msg_ - A valid ProtobufCMessage pointer where the unpacked data goes into.
 */
#define RWPB_F_MSG_UNPACK_USE_BODY(path_, alloc_, len_, data_, msg_) \
  ((RWPB_T_MSG(path_) *)(protobuf_c_message_unpack_usebody((alloc_), RWPB_G_MSG_PBCMD(path_), len_, data_, msg_)))

/*!
 * Duplicate the given ProtobufCMessage msg_ to a new ProtobufCMessage
 * of type path_.  A ProtobufCMessage pointer of type path_ is returned
 * on success.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param alloc_ - A pointer to a protobuf-c allocation.  Use NULL for
 *   the default.
 * @msg_ - A valid ProtobufCMessage pointer.
 */
#define RWPB_F_MSG_DUPLICATE(path_, alloc_, msg_) \
  ((RWPB_T_MSG(path_) *)(protobuf_c_message_duplicate((alloc_), msg_, RWPB_G_MSG_PBCMD(path_))))

/*!
 * Creates a GI boxed type for path, initializes it to hold message,
 * and saves the result as the new variable ptr.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param message_ - The message to hold
 * @param ptr_ - The pointer to the result
 */
#define RWPB_M_MSG_GI_DECL_INIT(path_, message_, ptr_) \
  RWPB_i_message_mmd_pbc(path_, t_gi_typename) *ptr_; \
  if (message_) { \
    ptr_ = RWPB_i_sym_2(RWPB_i_message_mmd_pbc(path_, s_gi_identifier_base),new_adopt)(message_, NULL); \
  } else { \
    ptr_ = RWPB_i_sym_2(RWPB_i_message_mmd_pbc(path_, s_gi_identifier_base),new)(); \
  } \
  RW_ASSERT(ptr_);

/*!
 * Creates a GI boxed type for path, initializes it to hold message,
 * and saves the result in the passed variable ptr.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param message_ The message to hold
 * @param ptr_ - The pointer to the result
 */
#define RWPB_M_MSG_GI_INIT(path_, message_, ptr_) \
  if (message_) { \
    ptr_ = RWPB_i_sym_2(RWPB_i_message_mmd_pbc(path_, s_gi_identifier_base),new_adopt)(message_, NULL); \
  } else { \
    ptr_ = RWPB_i_sym_2(RWPB_i_message_mmd_pbc(path_, s_gi_identifier_base),new)(); \
  } \
  RW_ASSERT(ptr_);

/*! @} */



/*****************************************************************************/
/*!
 * @defgroup RWPB_Message Message Fields
 * @{
 */

/*!
 * Obtain a constant for the yang maximum number of elements of a listy
 * field.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param field_ - The field name.
 */
#define RWPB_C_MSG_FIELD_MAX_ELEMENTS(path_, field_) \
  RWPB_i_c_message_field_max_elements(path_, field_)

/*!
 * Obtain a constant for the protobuf-c inline_max extension of a listy
 * field.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param field_ - The field name.
 */
#define RWPB_C_MSG_FIELD_INLINE_MAX(path_, field_) \
  RWPB_i_c_message_field_inline_max(path_, field_)

/*!
 * Obtain a constant for the protobuf-c string_max extension of a
 * stringy field.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param field_ - The field name.
 */
#define RWPB_C_MSG_FIELD_STRING_MAX(path_, field_) \
  RWPB_i_c_message_field_string_max(path_, field_)

/*!
 * Obtain the constant protobuf-c field tag for the given field
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param field_ - The proto field name.
 */
#define RWPB_C_MSG_FIELD_TAG(path_, field_) \
    RWPB_i_c_message_field_tag(path_, field_)

/*!
 * Obtain a pointer to the field-descriptor for a message field, by
 * message type and field name.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param field_ - The field name.
 */
#define RWPB_G_MSG_FIELD_PBCFD(path_, field_) \
  RWPB_i_g_message_field_pbcfd(path_, field_)

/*!
   Obtain a pointer to the field-descriptor for a message field, by
   message pointer and field name.
 *
 * @param msg_ - A pointer to a live instance of the concrete message type.
 * @param field_ - The field name.
 */
#define rwpb_g_msg_field_pbcfd(msg_, field_) \
  ( \
    (msg_)->base_concrete.descriptor->fields.field_ \
  )


/*! @} */



/*****************************************************************************/
/*!
 * @defgroup RWPB_PathEntry Path Entry Message Information
 * @{
 *
 * Path entry identifiers:
 *
 * - t_entry_pbcm
 *   The rift-protobuf-c path entry message typedef.
 *
 * - g_entry_pbcmd
 *   The rift-protobuf-c path entry message ProtobufCMessageDescriptor
 *   global variable name.
 *
 * - g_entry_msgdesc
 *   The yangpbc path entry message ypbc_msgdesc global variable name.
 *
 * - f_entry_init
 *   The name of the init function for t_entry_pbcm.  It has the
 *   prototype:
 *
 *       t_entry_pbcm f_entry_init(
 *         const t_entry_pbcm* in_pe,
 *         t_entry_pbcm* out_pe)
 *
 *   f_entry_init creates a path entry and returns it by value (under
 *   the assumption that the optimizer will eliminate extraneous
 *   copies).  If in_pe is not NULL, then the returned entry will be
 *   initialized from in_pe.  If out_pe is not NULL, then the
 *   initialized entry will be assigned to it.
 *
 * - f_entry_init_set
 *   The name of the init-and-set function for t_entry_pbcm.  It has the
 *   prototype:
 *
 *       t_entry_pbcm f_entry_init_set(
 *         const t_entry_pbcm* in_pe,
 *         t_entry_pbcm* out_pe,
 *         ARGS...)
 *
 *   f_entry_init_set creates a path entry and returns it by value (under
 *   the assumption that the optimizer will eliminate extraneous
 *   copies).  If in_pe is not NULL, then the returned entry will be
 *   initialized from in_pe.  If out_pe is not NULL, then the
 *   initialized entry will be assigned to it.  If the entry
 *   corresponds to a yang list with keys, then ARGS will contain the
 *   keys in yang key order.  The keys will be initialized to the
 *   values provided.
 *
 * - f_entry_safecast
 *   The name of the safe-cast function for t_entry_pbcm.  It has the
 *   prototype:
 *
 *       t_entry_pbcm* f_entry_safecast(
 *         void* pe)
 *
 *   The input pe will be validated that it is not NULL and that it
 *   has the correct descriptor pointer.  If valid, the pointer will
 *   be returned.  Otherwise, it will assert.
 *
 * - g_entry_value
 *   The yangpbc global path entry constant, which is fully
 *   initialized with the correct schema values.  It has type const
 *   t_entry_pbcm.
 */

/*!
 * Get the typename for the path entry of type path_.  The result can be
 * used anywhere where a type can be used.  The type will not be const
 * and will not be a pointer.
 *
 * @param path_ - The yangpbc message path identifier name.
 */
#define RWPB_T_PATHENTRY(path_) \
  RWPB_i_t_pathentry_pbcm(path_)

/*!
 * Get the rift-protobuf-c message descriptor for path entry type
 * path_.  The result is pointer to the the global const variable.
 *
 * @param path_ - The yangpbc message path identifier name.
 */
#define RWPB_G_PATHENTRY_PBCMD(path_) \
  (&(RWPB_i_g_pathentry_pbcmd(path_)))

/*!
 * Get the yangpbc path entry descriptor for type path_.  The result is
 * pointer to the the global const variable.
 *
 * @param path_ - The yangpbc message path identifier name.
 */
#define RWPB_G_PATHENTRY_YPBCD(path_) \
  (&(RWPB_i_g_pathentry_msgdesc(path_)))

/*!
 * Get the yangpbc pre-filled path entry type path_.  The result is
 * pointer to the the global const variable.
 *
 * @param path_ - The yangpbc message path identifier name.
 */
#define RWPB_G_PATHENTRY_VALUE(path_) \
  (&(RWPB_i_g_pathentry_value(path_)))

/*!
 * Declare, define, and initialize a path entry local variable of type
 * path_.  After this statement, a variable with the name var_ will
 * exist.  The variable will have the descriptor pointer set correctly,
 * and the schema data set to the correct values.  The keys, if any,
 * will have the default values.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param var_ - The variable to declare and initialize.
 */
#define RWPB_M_PATHENTRY_DECL_INIT(path_, var_) \
  RWPB_i_t_pathentry_pbcm(path_) var_ = {{PROTOBUF_C_MESSAGE_INIT(RWPB_G_PATHENTRY_PBCMD(path_), PROTOBUF_C_FLAG_REF_STATE_OWNED, 0)}};\
  protobuf_c_message_memcpy(&var_.base, &RWPB_i_g_pathentry_value(path_).base)

/*!
 * Declare, define, and initialize a path entry local variable of type
 * path_.  After this statement, a variable with the name var_ will
 * exist.  The variable will have the descriptor pointer set correctly,
 * and the schema data set to the correct values.  The keys will be
 * initialized according to __VA_ARGS__, which must match in type and
 * order.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param var_ - The variable to declare and initialize.
 * @param __VA_ARGS__ - The initialization arguments, if any.
 */
#define RWPB_M_PATHENTRY_DECL_SET(path_, var_, ...) \
  RWPB_i_t_pathentry_pbcm(path_) var_ \
    = RWPB_i_f_pathentry_init_set(path_)(&RWPB_i_g_pathentry_value(path_),NULL,__VA_ARGS__)

/*!
   Creates a GI boxed pathentry type for path, initializes it to the
   default values, and saves the result as the new variable ptr.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param pe_ - The source path entry.  Will be duplicated and cast.
 * @param ptr_ - The pointer to the result
 */
#define RWPB_M_PATHENTRY_GI_DECL_CREATE_DUP(path_, pe_, ptr_) \
    RWPB_i_message_mmd_ypbc(path_, t_gi_typename) *ptr_; \
    if (message_) { \
      ptr_ = RWPB_i_sym_2(RWPB_i_message_mmd_pbc(path_, s_gi_identifier_base),new_adopt)(message_, NULL); \
    } else { \
      ptr_ = RWPB_i_sym_2(RWPB_i_message_mmd_pbc(path_, s_gi_identifier_base),new)(); \
    } \
    RW_ASSERT(ptr_);

#define RWPB_F_PATHENTRY_CAST_OR_CRASH(path_, msg_) \
({ \
    RW_ASSERT(msg_); \
    RW_ASSERT(msg_->base.descriptor == RWPB_G_PATHENTRY_PBCMD(path_)); \
    (RWPB_T_PATHENTRY(path_)*) msg_;\
})

#define RWPB_F_PATHENTRY_CAST_OR_NULL(path_, msg_) \
({ \
    RWPB_T_PATHENTRY(path_)* local_ = nullptr; \
    if ((msg_) && msg_->base.descriptor == RWPB_G_PATHENTRY_PBCMD(path_))\
        local_ = (RWPB_T_PATHENTRY(path_)*) msg_; \
    local_; \
})


/*! @} */



/*****************************************************************************/
/*!
 * @defgroup RWPB_PathSpec Path Spec Message Information
 * @{
 *
 * Path spec identifiers:
 *
 * - t_path_pbcm
 *   The rift-protobuf-c path spec message typedef.
 *
 * - g_path_pbcmd
 *   The rift-protobuf-c path spec message ProtobufCMessageDescriptor
 *   global variable name.
 *
 * - g_path_msgdesc
 *   The yangpbc path spec message ypbc_msgdesc global variable name.
 *
 * - f_path_init
 *   The name of the init function for t_path_pbcm.  It has the
 *   prototype:
 *
 *       t_path_pbcm f_path_init(
 *         const t_path_pbcm* in_ps,
 *         t_path_pbcm* out_ps)
 *
 *   f_path_init creates a path spec and returns it by value (under
 *   the assumption that the optimizer will eliminate extraneous
 *   copies).  If in_ps is not NULL, then the returned spec will be
 *   initialized from in_ps.  If out_ps is not NULL, then the
 *   initialized spec will be assigned to it.
 *
 * - f_path_init_set
 *   The name of the init-and-set function for t_path_pbcm.  It has the
 *   prototype:
 *
 *       t_path_pbcm f_path_init_set(
 *         const t_path_pbcm* in_ps,
 *         t_path_pbcm* out_ps,
 *         RwSchemaCategory category)
 *
 *   f_path_init_set creates a path spec and returns it by value (under
 *   the assumption that the optimizer will eliminate extraneous
 *   copies).  If in_ps is not NULL, then the returned spec will be
 *   initialized from in_ps.  If out_ps is not NULL, then the
 *   initialized spec will be assigned to it.  The path spec category
 *   will be set to the provided category.
 *
 * - f_path_safecast
 *   The name of the safe-cast function for t_path_pbcm.  It has the
 *   prototype:
 *
 *       t_path_pbcm* f_path_safecast(
 *         void* ps)
 *
 *   The input ps will be validated that it is not NULL and that it
 *   has the correct descriptor pointer.  If valid, the pointer will
 *   be returned.  Otherwise, it will assert.
 *
 * - g_path_value
 *   The yangpbc global path spec constant, which is fully
 *   initialized with the correct schema values.  It has type const
 *   t_path_pbcm.
 */

/*!
 * Get the typename for the path spec of type path_.  The result can be
 * used anywhere where a type can be used.  The type will not be const
 * and will not be a pointer.
 *
 * @param path_ - The yangpbc message path identifier name.
 */
#define RWPB_T_PATHSPEC(path_) \
  RWPB_i_t_pathspec_pbcm(path_)

/*!
 * Get the rift-protobuf-c message descriptor for path spec type
 * path_.  The result is pointer to the the global const variable.
 *
 * @param path_ - The yangpbc message path identifier name.
 */
#define RWPB_G_PATHSPEC_PBCMD(path_) \
  (&(RWPB_i_g_pathspec_pbcmd(path_)))

/*!
 * Get the yangpbc path spec descriptor for type path_.  The result is
 * pointer to the the global const variable.
 *
 * @param path_ - The yangpbc message path identifier name.
 */
#define RWPB_G_PATHSPEC_YPBCD(path_) \
  (&(RWPB_i_g_pathspec_msgdesc(path_)))

/*!
 * Get the yangpbc pre-filled path spec type path_.  The result is
 * pointer to the the global const variable.
 *
 * @param path_ - The yangpbc message path identifier name.
 */
#define RWPB_G_PATHSPEC_VALUE(path_) \
  (&(RWPB_i_g_pathspec_value(path_)))

/*!
 * (Re)Initialize a path spec local variable of type path_, using the
 * protobuf-c init function.  If the path spec is initialized from a
 * previous call or use, the previous contents will be ignored and no
 * memory will be freed.  Upon return, the descriptor pointer will have
 * been set and the message will contain all default values.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param ptr_ - A pointer of the path spec type.  Must be the
 *   fully-qualified type; it cannot be just a ProtobufCMessage
 *   pointer.
 */
#define RWPB_F_PATHSPEC_INIT(path_, ptr_) \
  RWPB_i_f_pathspec_pbcm_init(path_)((ptr_))

/*!
 * Declare, define, and initialize a path spec local variable of type
 * path_.  After this statement, a variable with the name var_ will
 * exist.  The variable will have the descriptor pointer set correctly,
 * and the schema data set to the correct values.  The keys, if any,
 * will have the default values.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param var_ - The variable to declare and initialize.
 */
#define RWPB_M_PATHSPEC_DECL_INIT(path_, var_) \
  RWPB_i_t_pathspec_pbcm(path_) var_ = {{PROTOBUF_C_MESSAGE_INIT(RWPB_G_PATHSPEC_PBCMD(path_), PROTOBUF_C_FLAG_REF_STATE_OWNED, 0)}};\
  protobuf_c_message_memcpy(&var_.base, &RWPB_i_g_pathspec_value(path_).base);

/*!
 * Declare, define, and initialize a path spec local variable of type
 * path_.  After this statement, a variable with the name var_ will
 * exist.  The variable will have the descriptor pointer set correctly,
 * and the schema data set to the correct values.  The keys will be
 * initialized according to __VA_ARGS__, which must match in type and
 * order.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param var_ - The variable to declare and initialize.
 * @param cat_ - The keyspec category.
 */
#define RWPB_M_PATHSPEC_DECL_SET(path_, var_, cat_) \
  RWPB_i_t_pathspec_pbcm(path_) var_ \
    = RWPB_i_f_pathspec_init_set(path_)(&RWPB_i_g_pathspec_value(path_), NULL, cat_)

/*!
 * Cast a generic keyspec pointer to keyspec pointer
 * of type path_. The macro asserts if the protoc message is not of
 * the specified type.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param msg_ - The keyspec pointer.
 */
#define RWPB_M_PATHSPEC_CONST_CAST(path_, ks_) \
  ({ \
    RW_ASSERT(ks_); \
    RW_ASSERT(ks_->base.descriptor == RWPB_G_PATHSPEC_PBCMD(path_)); \
    (const RWPB_T_PATHSPEC(path_) *)(ks_); \
  })

#define RWPB_F_PATHSPEC_CAST_OR_CRASH(path_, msg_) \
  ({ \
      RW_ASSERT(msg_); \
      RW_ASSERT(msg_->base.descriptor == RWPB_G_PATHSPEC_PBCMD(path_)); \
      (RWPB_T_PATHSPEC(path_)*) msg_;\
  })

#define RWPB_F_PATHSPEC_CAST_OR_NULL(path_, msg_) \
  ({ \
      RWPB_T_PATHSPEC(path_)* local_ = nullptr; \
      if ((msg_) && msg_->base.descriptor == RWPB_G_PATHSPEC_PBCMD(path_))\
          local_ = (RWPB_T_PATHSPEC(path_)*) msg_; \
      local_; \
  })


#define RWPB_F_PATHSPEC_UNSAFE_MORPH(path_, msg_) \
  ({ \
      RW_ASSERT(msg_); \
      (RWPB_T_PATHSPEC(path_)*) msg_; \
  })


/*! @} */



/*****************************************************************************/
/*!
 * @defgroup RWPB_ListOnly List-Only Message Information
 * @{
 */

/*!
 * Get the typename for the list only of type path_.  The result can be
 * used anywhere where a type can be used.  The type will not be const
 * and will not be a pointer.
 *
 * @param path_ - The yangpbc message path identifier name.
 */
#define RWPB_T_LISTONLY(path_) \
  RWPB_i_t_list_pbcm(path_)

/*!
 * (Re)Initialize a list only local variable of type path_, using the
 * protobuf-c init function.  If the list only is initialized from a
 * previous call or use, the previous contents will be ignored and no
 * memory will be freed.  Upon return, the descriptor pointer will have
 * been set and the message will contain all default values.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param ptr_ - A pointer of the list only type.  Must be the
 *   fully-qualified type; it cannot be just a ProtobufCMessage
 *   pointer.
 */
#define RWPB_F_LISTONLY_INIT(path_, ptr_) \
  RWPB_i_f_list_pbcm_init(path_)((ptr_))

/*!
 * Declare, define, and initialize a list only local variable of type
 * path_.  After this statement, a variable with the name var_ will
 * exist.  The variable will have the descriptor pointer set correctly,
 * but will be empty.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param var_ - The variable to declare and initialize.
 */
#define RWPB_M_LISTONLY_DECL_INIT(path_, var_) \
  RWPB_i_t_list_pbcm(path_) var_; \
  (RWPB_i_f_list_pbcm_init(path_)(&var_))

/*!
 * Get the rift-protobuf-c listonly message descriptor for type path_.  The
 * result is pointer to the the global const variable.
 *
 * @param path_ - The yangpbc message path identifier name.
 */
#define RWPB_G_LISTONLY_PBCMD(path_) \
  (&(RWPB_i_g_list_pbcmd(path_)))

/*! @} */



/*****************************************************************************/
/*!
 * @defgroup RWPB_Minikey Minikey Information
 * @{
 *
 * Minikey identifiers:
 *
 * - t_mk
 *   The minikey typedef.  Applicable only to lists.  For non-list
 *   elements, this will be defined to something that produces a
 *   compile error.
 *
 * - g_mk_desc
 *   The minikey descriptor global variable name.
 *
 * - f_mk_init
 *   The name of the init function for t_mk.  It has the
 *   prototype:
 *
 *       t_mk f_mk_init(
 *         const t_mk* in_mk,
 *         t_mk* out_mk)
 *
 *   f_mk_init creates a minikey and returns it by value (under the
 *   assumption that the optimizer will eliminate extraneous copies).
 *   If in_mk is not NULL, then the returned minikey will be
 *   initialized from in_mk.  If out_mk is not NULL, then the
 *   initialized minikey will be assigned to it.
 *
 * - f_mk_init_set
 *   The name of the init-and-set function for t_mk.  It has the
 *   prototype:
 *
 *       t_mk f_mk_init_set(
 *         const t_mk* in_mk,
 *         t_mk* out_mk,
 *         ARGS...)
 *
 *   f_mk_init_set creates a minikey and returns it by value (under
 *   the assumption that the optimizer will eliminate extraneous
 *   copies).  If in_mk is not NULL, then the returned minikey will be
 *   initialized from in_mk.  If out_mk is not NULL, then the
 *   initialized minikey will be assigned to it.  ARGS will contain
 *   the keys in yang key order.  The keys will be initialized to the
 *   values provided.
 *
 * - f_mk_safecast
 *   The name of the safe-cast function for t_mk.  It has the
 *   prototype:
 *
 *       t_mk* f_mk_safecast(
 *         void* mk)
 *
 *   The input mk will be validated that it is not NULL and that it
 *   has the correct descriptor pointer.  If valid, the pointer will
 *   be returned.  Otherwise, it will assert.
 *
 * - g_mk_value
 *   The yangpbc global minikey constant, which is fully
 *   initialized with the correct schema values.  It has type const
 *   t_mk.
 */

/*!
 * Get the typename for the minikey of type path_.  The result can be
 * used anywhere where a type can be used.  The type will not be const
 * and will not be a pointer.
 *
 * @param path_ - The yangpbc message path identifier name.
 */
#define RWPB_T_MK(path_) \
  RWPB_i_t_minikey(path_)

/*!
 * Declare, define, and initialize a minikey local variable of type
 * path_.  After this statement, a variable with the name var_ will
 * exist.  The variable will have the descriptor pointer set correctly,
 * and the schema data set to the correct values.  The keys, if any,
 * will have the default values.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param var_ - The variable to declare and initialize.
 */
#define RWPB_M_MK_DECL_INIT(path_, var_) \
  RWPB_i_t_minikey(path_) var_; \
  (RWPB_i_f_minikey_init(path_)(&var_))

/*!
 * Declare, define, and initialize a minikey local variable of type
 * path_.  After this statement, a variable with the name var_ will
 * exist.  The variable will have the descriptor pointer set correctly,
 * and the schema data set to the correct values.  The keys will be
 * initialized according to __VA_ARGS__, which must match in type and
 * order.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param var_ - The variable to declare and initialize.
 * @param __VA_ARGS__ - The initialization arguments, if any.
 */
#define RWPB_M_MK_DECL_SET(path_, var_, ...) \
  RWPB_i_m_minikey_decl_init(path_)(var_,__VA_ARGS__)

/*! @} */



/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*!
 * @defgroup RWPB_Impl RWPB Implementation Details
 * @{
 */

/*!
 * Get a global symbol from a path.
 */
#define RWPB_i_path(path_) \
  rw_ypbc_##path_

/*!
 * Get a global symbol from a path.
 */
#define RWPB_i_sym(path_, token_) \
  RWPB_i_sym_2(RWPB_i_path(path_), token_)

#define RWPB_i_sym_2(realpath_, token_) \
  RWPB_i_paste2(realpath_, token_)

/*!
 * Get a global field symbol from a path.
 */
#define RWPB_i_fsym(path_, field_, token_) \
  RWPB_i_fsym_2(RWPB_i_path(path_), field_, token_)

#define RWPB_i_fsym_2(realpath_, field_, token_) \
  RWPB_i_paste3(realpath_, token_, field_)


/*!
 * Paste 2 tokens together
 */
#define RWPB_i_paste2(a_,b_) \
  a_ ## _ ## b_

/*!
 * Paste 3 tokens together
 */
#define RWPB_i_paste3(a_,b_,c_) \
  a_ ## _ ## b_ ## _ ## c_



/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*!
 * @defgroup RWPB_ImplMdIx Meta-data selection macros
 * @{
 *
 * All of these macros are considered internal implementation to RWPB.
 * These macros extract a specific piece of meta-data from one of the
 * meta-data macros associated with a message or field.
 *
 * All of the macros follow the same pattern:
 *  - token-paste the short meta-data name into the indexing macro.
 *  - expand the path into the meta-data macro name.
 *  - expand the meta-data macro by appending parentheses.
 *
 * The protobuf-c meta-data gets expanded indirectly, via the ypbc
 * meta-data.  And the field meta-data requires an extra step of
 * pasting the field name.  However, ultimately, the process is
 * basically the same for all types of meta-data.
 */


/* --- messages --- */

#define RWPB_i_mdix_ypbc_msg_s_yang_element         00 /* ATTN: superflous: get from msgdesc */
#define RWPB_i_mdix_ypbc_msg_m_msg_pbc_metadata     01
#define RWPB_i_mdix_ypbc_msg_t_msg_msgdesc          02 /* ATTN: superflous: get with RWPB_i_sym */
#define RWPB_i_mdix_ypbc_msg_t_path_msgdesc         03 /* ATTN: superflous: get with RWPB_i_sym */
#define RWPB_i_mdix_ypbc_msg_t_entry_msgdesc        04 /* ATTN: superflous: get with RWPB_i_sym */
#define RWPB_i_mdix_ypbc_msg_t_msg_pbcm             05 /* ATTN: superflous: get from pbc_metadata */
#define RWPB_i_mdix_ypbc_msg_t_path_pbcm            06 /* ATTN: superflous: get from pbc_metadata */
#define RWPB_i_mdix_ypbc_msg_t_entry_pbcm           07 /* ATTN: superflous: get from pbc_metadata */
#define RWPB_i_mdix_ypbc_msg_t_msg_pbcc             08 /* ATTN: superflous: get from pbc_metadata */
#define RWPB_i_mdix_ypbc_msg_g_msg_msgdesc          09 /* ATTN: superflous: get with RWPB_i_sym */
#define RWPB_i_mdix_ypbc_msg_g_path_msgdesc         10 /* ATTN: superflous: get from msgdesc */
#define RWPB_i_mdix_ypbc_msg_g_entry_msgdesc        11 /* ATTN: superflous: get from msgdesc */
#define RWPB_i_mdix_ypbc_msg_g_msg_pbcmd            12 /* ATTN: superflous: get from pbc_metadata */
#define RWPB_i_mdix_ypbc_msg_g_path_pbcmd           13 /* ATTN: superflous: get from pbc_metadata */
#define RWPB_i_mdix_ypbc_msg_g_entry_pbcmd          14 /* ATTN: superflous: get from pbc_metadata */
#define RWPB_i_mdix_ypbc_msg_g_path_value           15 /* ATTN: superflous: get from msgdesc or RWPB_i_sym */
#define RWPB_i_mdix_ypbc_msg_g_entry_value          16 /* ATTN: superflous: get from msgdesc or RWPB_i_sym */
#define RWPB_i_mdix_ypbc_msg_m_entry_pbc_metadata   17
#define RWPB_i_mdix_ypbc_msg_m_path_pbc_metadata    18
#define RWPB_i_mdix_ypbc_msg_m_list_pbc_metadata    19
/* ATTN: yang leaf type? */
/* ATTN: yang statement type? */

#define RWPB_i_mdix_pbc_msg_t_pbcm                  00
#define RWPB_i_mdix_pbc_msg_g_pbcmd                 01
#define RWPB_i_mdix_pbc_msg_f_pbc_init              02
#define RWPB_i_mdix_pbc_msg_g_module                03
#define RWPB_i_mdix_pbc_msg_t_gi_typename           04
#define RWPB_i_mdix_pbc_msg_s_gi_identifier_base    05
#define RWPB_i_mdix_pbc_msg_n_fields                06
#define RWPB_i_mdix_pbc_msg_l_field_name            07

/*!
 * Extract a specific piece of ypbc message meta-data.  This macro
 * expands into the piece of meta-data.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param md_ - The name of the desired meta-data.
 */
#define RWPB_i_message_mmd_ypbc(path_, md_) \
  RWPB_i_extract_n( \
    RWPB_i_paste2(RWPB_i_mdix_ypbc_msg, md_), \
    RWPB_i_sym(path_, m_msg)() )

/*!
 * Extract a specific piece of protobuf-c meta-data for a ypbc message.
 * This macro expands into the piece of meta-data.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param md_ - The name of the desired meta-data.
 */
#define RWPB_i_message_mmd_pbc(path_, md_) \
  RWPB_i_extract_n( \
    RWPB_i_paste2(RWPB_i_mdix_pbc_msg, md_), \
    RWPB_i_message_mmd_ypbc(path_, m_msg_pbc_metadata)() )

/*!
 * Extract a specific piece of protobuf-c meta-data for a ypbc path
 * entry.  This macro expands into the piece of meta-data.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param md_ - The name of the desired meta-data.
 */
#define RWPB_i_pathentry_mmd_pbc(path_, md_) \
  RWPB_i_extract_n( \
    RWPB_i_paste2(RWPB_i_mdix_pbc_msg, md_), \
    RWPB_i_message_mmd_ypbc(path_, m_entry_pbc_metadata)() )

/*!
 * Extract a specific piece of protobuf-c meta-data for a ypbc path
 * spec.  This macro expands into the piece of meta-data.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param md_ - The name of the desired meta-data.
 */
#define RWPB_i_pathspec_mmd_pbc(path_, md_) \
  RWPB_i_extract_n( \
    RWPB_i_paste2(RWPB_i_mdix_pbc_msg, md_), \
    RWPB_i_message_mmd_ypbc(path_, m_path_pbc_metadata)() )



/* --- fields --- */

#define RWPB_i_mdix_ypbc_fld_n_msg_path             00
#define RWPB_i_mdix_ypbc_fld_s_yang_element         01
#define RWPB_i_mdix_ypbc_fld_g_module               02 /* ATTN: superfluous: can get from path::pbcmd */
#define RWPB_i_mdix_ypbc_fld_m_msg_pbc_metadata     03
#define RWPB_i_mdix_ypbc_fld_c_tag                  04 /* ATTN: superfluous: can get from pbcfd */
#define RWPB_i_mdix_ypbc_fld_c_max_elements         05
#define RWPB_i_mdix_ypbc_fld_m_entry_pbc_metadata   ATTN_NOT_YET_DEFINED
#define RWPB_i_mdix_ypbc_fld_m_path_pbc_metadata    ATTN_NOT_YET_DEFINED
#define RWPB_i_mdix_ypbc_fld_m_list_pbc_metadata    ATTN_NOT_YET_DEFINED

#define RWPB_i_mdix_pbc_fld_t_msg_pbcm              00
#define RWPB_i_mdix_pbc_fld_s_pbc_field             01 /* ATTN: superfluous: can get from pbcfd */
#define RWPB_i_mdix_pbc_fld_n_pbc_field             02
#define RWPB_i_mdix_pbc_fld_n_opt_req_rep           03
#define RWPB_i_mdix_pbc_fld_n_type_kind             04
#define RWPB_i_mdix_pbc_fld_n_flat_pointy           05
#define RWPB_i_mdix_pbc_fld_n_has_or_n              06
#define RWPB_i_mdix_pbc_fld_g_pbcfd                 07
#define RWPB_i_mdix_pbc_fld_t_pbc_element_ptr       08
#define RWPB_i_mdix_pbc_fld_c_string_max            09
#define RWPB_i_mdix_pbc_fld_c_inline_max            10
#define RWPB_i_mdix_pbc_fld_c_primitive_type        11
#define RWPB_i_mdix_pbc_fld_c_default_value         12

/*!
 * Extract a specific piece of ypbc message field meta-data.  This
 * macro expands into the piece of meta-data.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param field_ - The field name.  Cannot be a has or n field.
 * @param md_ - The name of the desired meta-data.
 */
#define RWPB_i_message_fmd_ypbc(path_, field_, md_) \
  RWPB_i_extract_n( \
    RWPB_i_paste2(RWPB_i_mdix_ypbc_fld, md_), \
    RWPB_i_fsym(path_, field_, m_field)() )

/*!
 * Extract a specific piece of protobuf-c meta-data for a ypbc message
 * field.  This macro expands into the piece of meta-data.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param field_ - The field name.  Cannot be a has or n field.
 * @param md_ - The name of the desired meta-data.
 */
#define RWPB_i_message_fmd_pbc(path_, field_, md_) \
  RWPB_i_extract_n( \
    RWPB_i_paste2(RWPB_i_mdix_pbc_fld, md_), \
    RWPB_i_message_fmd_ypbc(path_, field_, m_msg_pbc_metadata)() )

/*!
 * Extract a specific piece of protobuf-c meta-data for a ypbc path
 * entry field.  This macro expands into the piece of meta-data.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param field_ - The field name.  Cannot be a has or n field.
 * @param md_ - The name of the desired meta-data.
 */
#define RWPB_i_pathentry_fmd_pbc(path_, field_, md_) \
  RWPB_i_extract_n( \
    RWPB_i_paste2(RWPB_i_mdix_pbc_fld, md_), \
    RWPB_i_message_fmd_ypbc(path_, field_, m_entry_pbc_metadata)() )

/*!
 * Extract a specific piece of protobuf-c meta-data for a ypbc path
 * spec field.  This macro expands into the piece of meta-data.
 *
 * @param path_ - The yangpbc message path identifier name.
 * @param field_ - The field name.  Cannot be a has or n field.
 * @param md_ - The name of the desired meta-data.
 */
#define RWPB_i_pathspec_fmd_pbc(path_, field_, md_) \
  RWPB_i_extract_n( \
    RWPB_i_paste2(RWPB_i_mdix_pbc_fld, md_), \
    RWPB_i_message_fmd_ypbc(path_, field_, m_path_pbc_metadata)() )


/* --- minikeys --- */
/* ATTN: implement minikey metadata */


/* --- enums --- */
/* ATTN: implement enum metadata */


/* --- typedefs --- */
/* ATTN: implement yang typedef metadata */


/* --- modules --- */
/* ATTN: implement module metadata */


/* --- schemas --- */
/* ATTN: implement schema metadata */

/*! @} */



/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*!
 * @defgroup RWPB_ImplMdabstr Meta-data abstraction macros
 * @{
 *
 * Meta-data abstraction macros.  These macros extract a specific piece
 * of meta-data from the appropriate source, whatever that source might
 * be.  These should be used in the main RWPB implementation macros, in
 * lieu of making sym, fsym, mmd, or fmd expansions directly.  Doing
 * direct expansions will result in more code changes if/when the
 * implementation changes.
 *
 * All of the macros expand into one of the following:
 *  - A RWPB_i_sym() macro
 *  - A RWPB_i_*_mmd_*() macro
 *  - A RWPB_i_fsym() macro
 *  - A RWPB_i_*_fmd_*() macro
 *  - One of the above with extra syntax designed to get a particular
 *    descriptor field
 *  - One of the above, and some additional token pasting.
 */

/* --- message --- */

#define RWPB_i_f_message_init(path_) \
  RWPB_i_sym(path_, f_msg_init)

#define RWPB_i_g_message_msgdesc(path_) \
  RWPB_i_sym(path_, g_msg_msgdesc)

#define RWPB_i_g_message_pbcmd(path_) \
  RWPB_i_sym(path_, g_msg_pbcmd)

#define RWPB_i_t_message_pbcm(path_) \
  RWPB_i_sym(path_, t_msg_pbcm)

#define RWPB_i_m_message_metadata(path_) \
  RWPB_i_sym(path_, m_msg_metadata)


/* --- message fields --- */

#define RWPB_i_c_message_field_max_elements(path_, field_) \
  RWPB_i_message_fmd_ypbc(path_, field_, c_max_elements)

#define RWPB_i_c_message_field_string_max(path_, field_) \
  RWPB_i_message_fmd_pbc(path_, field_, c_string_max)

#define RWPB_i_c_message_field_inline_max(path_, field_) \
  RWPB_i_message_fmd_pbc(path_, field_, c_inline_max)

#define RWPB_i_c_message_field_tag(path_, field_) \
  RWPB_i_message_fmd_ypbc(path_, field_, c_tag)

#define RWPB_i_g_message_field_pbcfd(path_, field_) \
  RWPB_i_message_fmd_pbc(path_, field_, g_pbcfd)


/* --- path entry message --- */

#define RWPB_i_f_pathentry_init_set(path_) \
  RWPB_i_sym(path_, f_entry_init_set)

#define RWPB_i_g_pathentry_msgdesc(path_) \
  RWPB_i_sym(path_, g_entry_msgdesc)

#define RWPB_i_g_pathentry_pbcmd(path_) \
  RWPB_i_sym(path_, g_entry_pbcmd)

#define RWPB_i_g_pathentry_value(path_) \
  RWPB_i_sym(path_, g_entry_value)

#define RWPB_i_t_pathentry_pbcm(path_) \
  RWPB_i_sym(path_, t_entry_pbcm)

#define RWPB_i_m_pathentry_metadata(path_) \
  RWPB_i_sym(path_, m_entry_metadata)


/* --- path spec message --- */

#define RWPB_i_f_pathspec_init_set(path_) \
  RWPB_i_sym(path_, f_path_init_set)

#define RWPB_i_f_pathspec_pbcm_init(path_) \
  RWPB_i_sym(path_, f_path_pbcm_init)

#define RWPB_i_g_pathspec_msgdesc(path_) \
  RWPB_i_sym(path_, g_path_msgdesc)

#define RWPB_i_g_pathspec_pbcmd(path_) \
  RWPB_i_sym(path_, g_path_pbcmd)

#define RWPB_i_g_pathspec_value(path_) \
  RWPB_i_sym(path_, g_path_value)

#define RWPB_i_t_pathspec_pbcm(path_) \
  RWPB_i_sym(path_, t_path_pbcm)

#define RWPB_i_m_pathspec_metadata(path_) \
  RWPB_i_sym(path_, m_path_metadata)


/* --- list-only message --- */

#define RWPB_i_f_list_pbcm_init(path_) \
  RWPB_i_sym(path_, f_list_pbcm_init)

#define RWPB_i_t_list_pbcm(path_) \
  RWPB_i_sym(path_, t_list_pbcm)

#define RWPB_i_m_list_metadata(path_) \
  RWPB_i_sym(path_, m_list_metadata)

#define RWPB_i_g_list_pbcmd(path_) \
  RWPB_i_sym(path_, g_list_pbcmd)


/* --- yang typedef --- */
/* ATTN: Not yet implemented */


/* --- yang enum type --- */
/* ATTN: also: proto enum */

#define RWPB_i_t_enum(path_) \
  RWPB_i_sym(path_, t_enum)

#define RWPB_i_m_enum_metadata(path_) \
  RWPB_i_sym(path_, m_enum_metadata)


/* --- minikey --- */

#define RWPB_i_f_minikey_init(path_) \
  RWPB_i_sym(path_, f_mk_init)

#define RWPB_i_m_minikey_decl_init(path_) \
  RWPB_i_sym(path_, m_mk_decl_init)

#define RWPB_i_t_minikey(path_) \
  RWPB_i_sym(path_, t_mk)

#define RWPB_i_m_minikey_metadata(path_) \
  RWPB_i_sym(path_, m_mk_metadata)


/* --- yang module --- */

#define RWPB_i_g_module(module_) \
  RWPB_i_sym(module_, g_module)

#define RWPB_i_m_module_metadata(module_) \
  RWPB_i_sym(module_, m_module_metadata)


/* --- yang schema --- */
/* ATTN: also: proto package */

#define RWPB_i_g_schema(schema_) \
  RWPB_i_sym(schema_, g_schema)

#define RWPB_i_m_schema_metadata(schema_) \
  RWPB_i_sym(schema_, m_schema_metadata)

/*! @} */


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*!
 * @defgroup RWPB_ImplExtractN Extract Nth element
 * @{
 *
 * Extract the "Nth" element from a macro that contains a list of up to
 * 50 comma-separated elements.  The index is 0-based, in the form
 * "XX", with leading 0 for 0-9.  It IS NOT octal!  The index may be a
 * macro itself.
 *
 * - Step 1:
 *    - Cause the expansion of index_, if it is a macro.  index_ will
 *      eventually be used in a token paste: extract needs the
 *      fully-expanded contents of the macro, rather than the macro name
 *      and arguments themselves.
 *    - Cause the expansion of list_, if it was a macro that contained
 *      commas.
 *
 * - Step 2:
 *    - Finish the expansion of index_, if it was a macro.
 *    - Create a macro name that will select the correct index.
 *
 * - Step 3:
 *    - Extra expansion to allow the paste from step 2 to expand fully.
 *
 * - Step 4:
 *    - Expand the index-extraction macro on the arguments, which selects
 *      the desired element.  This will be a preprocessor compile error
 *      if list_ contains 2 fewer elements than the index.  This will
 *      produce a probable syntax error if list_ contains 1 fewer
 *      elements than the index.
 *
 * - Support macros:
 *    - A set of 50 macros, one for each index, which expands to just
 *      that selected index.
 *
 * - How to make the maximum N bigger:
 *    - First, reconsider what you are doing.  Do you really need more
 *      than 50?  At some point, it is probably too much and maybe you
 *      should find another way.
 *    - Only add 10 at a time.
 *    - Update all the '50's in this comment to the new N.
 *    - Add another block of 10 args whereever the full N is coded.
 *    - Add another 10 extraction macros.
 */

#define RWPB_i_extract_n(index_, list_, ...) \
  RWPB_i_extract_n_2( \
    index_, \
    list_, \
    __VA_ARGS__, \
    RWPB_i_ERROR_IN_extract_n_TOO_FEW_ARGUMENTS)

#define RWPB_i_extract_n_2(index_, ...) \
  RWPB_i_extract_n_3( \
    RWPB_i_paste2(RWPB_i_extract_n_index, index_), \
    __VA_ARGS__)

#define RWPB_i_extract_n_3(macro_, ...) \
  RWPB_i_extract_n_4(macro_, __VA_ARGS__)

#define RWPB_i_extract_n_4(macro_, ...) \
  macro_(__VA_ARGS__)


#define RWPB_i_extract_n_index_00( \
    a00, \
    junk, ... ) \
  a00

#define RWPB_i_extract_n_index_01( \
    a00, a01, \
    junk, ... ) \
  a01

#define RWPB_i_extract_n_index_02( \
    a00, a01, a02, \
    junk, ... ) \
  a02

#define RWPB_i_extract_n_index_03( \
    a00, a01, a02, a03, \
    junk, ... ) \
  a03

#define RWPB_i_extract_n_index_04( \
    a00, a01, a02, a03, a04, \
    junk, ... ) \
  a04

#define RWPB_i_extract_n_index_05( \
    a00, a01, a02, a03, a04, a05, \
    junk, ... ) \
  a05

#define RWPB_i_extract_n_index_06( \
    a00, a01, a02, a03, a04, a05, a06, \
    junk, ... ) \
  a06

#define RWPB_i_extract_n_index_07( \
    a00, a01, a02, a03, a04, a05, a06, a07, \
    junk, ... ) \
  a07

#define RWPB_i_extract_n_index_08( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, \
    junk, ... ) \
  a08

#define RWPB_i_extract_n_index_09( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    junk, ... ) \
  a09

#define RWPB_i_extract_n_index_10( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, \
    junk, ... ) \
  a10

#define RWPB_i_extract_n_index_11( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, \
    junk, ... ) \
  a11

#define RWPB_i_extract_n_index_12( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, \
    junk, ... ) \
  a12

#define RWPB_i_extract_n_index_13( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, \
    junk, ... ) \
  a13

#define RWPB_i_extract_n_index_14( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, \
    junk, ... ) \
  a14

#define RWPB_i_extract_n_index_15( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, \
    junk, ... ) \
  a15

#define RWPB_i_extract_n_index_16( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, \
    junk, ... ) \
  a16

#define RWPB_i_extract_n_index_17( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, \
    junk, ... ) \
  a17

#define RWPB_i_extract_n_index_18( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, \
    junk, ... ) \
  a18

#define RWPB_i_extract_n_index_19( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    junk, ... ) \
  a19

#define RWPB_i_extract_n_index_20( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, \
    junk, ... ) \
  a20

#define RWPB_i_extract_n_index_21( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, \
    junk, ... ) \
  a21

#define RWPB_i_extract_n_index_22( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, \
    junk, ... ) \
  a22

#define RWPB_i_extract_n_index_23( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, \
    junk, ... ) \
  a23

#define RWPB_i_extract_n_index_24( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, \
    junk, ... ) \
  a24

#define RWPB_i_extract_n_index_25( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, \
    junk, ... ) \
  a25

#define RWPB_i_extract_n_index_26( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, \
    junk, ... ) \
  a26

#define RWPB_i_extract_n_index_27( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, a27, \
    junk, ... ) \
  a27

#define RWPB_i_extract_n_index_28( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, a27, a28, \
    junk, ... ) \
  a28

#define RWPB_i_extract_n_index_29( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, \
    junk, ... ) \
  a29

#define RWPB_i_extract_n_index_30( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, \
    a30, \
    junk, ... ) \
  a30

#define RWPB_i_extract_n_index_31( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, \
    a30, a31, \
    junk, ... ) \
  a31

#define RWPB_i_extract_n_index_32( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, \
    a30, a31, a32, \
    junk, ... ) \
  a32

#define RWPB_i_extract_n_index_33( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, \
    a30, a31, a32, a33, \
    junk, ... ) \
  a33

#define RWPB_i_extract_n_index_34( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, \
    a30, a31, a32, a33, a34, \
    junk, ... ) \
  a34

#define RWPB_i_extract_n_index_35( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, \
    a30, a31, a32, a33, a34, a35, \
    junk, ... ) \
  a35

#define RWPB_i_extract_n_index_36( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, \
    a30, a31, a32, a33, a34, a35, a36, \
    junk, ... ) \
  a36

#define RWPB_i_extract_n_index_37( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, \
    a30, a31, a32, a33, a34, a35, a36, a37, \
    junk, ... ) \
  a37

#define RWPB_i_extract_n_index_38( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, \
    a30, a31, a32, a33, a34, a35, a36, a37, a38, \
    junk, ... ) \
  a38

#define RWPB_i_extract_n_index_39( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, \
    a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, \
    junk, ... ) \
  a39

#define RWPB_i_extract_n_index_40( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, \
    a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, \
    a40, \
    junk, ... ) \
  a40

#define RWPB_i_extract_n_index_41( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, \
    a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, \
    a40, a41, \
    junk, ... ) \
  a41

#define RWPB_i_extract_n_index_42( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, \
    a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, \
    a40, a41, a42, \
    junk, ... ) \
  a42

#define RWPB_i_extract_n_index_43( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, \
    a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, \
    a40, a41, a42, a43, \
    junk, ... ) \
  a43

#define RWPB_i_extract_n_index_44( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, \
    a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, \
    a40, a41, a42, a43, a44, \
    junk, ... ) \
  a44

#define RWPB_i_extract_n_index_45( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, \
    a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, \
    a40, a41, a42, a43, a44, a45, \
    junk, ... ) \
  a45

#define RWPB_i_extract_n_index_46( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, \
    a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, \
    a40, a41, a42, a43, a44, a45, a46, \
    junk, ... ) \
  a46

#define RWPB_i_extract_n_index_47( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, \
    a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, \
    a40, a41, a42, a43, a44, a45, a46, a47, \
    junk, ... ) \
  a47

#define RWPB_i_extract_n_index_48( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, \
    a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, \
    a40, a41, a42, a43, a44, a45, a46, a47, a48, \
    junk, ... ) \
  a48

#define RWPB_i_extract_n_index_49( \
    a00, a01, a02, a03, a04, a05, a06, a07, a08, a09, \
    a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, \
    a20, a21, a22, a23, a24, a25, a26, a27, a28, a29, \
    a30, a31, a32, a33, a34, a35, a36, a37, a38, a39, \
    a40, a41, a42, a43, a44, a45, a46, a47, a48, a49, \
    junk, ... ) \
  a49

/*! @} */

/*! @} */

__END_DECLS


/*****************************************************************************/
/*
 * C++ helper classes
 */

#ifdef RW_DOXYGEN_PARSING
#define __cplusplus 201103
#endif

#ifdef __cplusplus

#if __cplusplus < 201103L
#error "Requires C++11"
#endif

#include <memory>

/*!
 * @defgroup RWPB_Uptr C++ Message Deleters
 * @{
 */

/*!
 * Protobuf-c whole-message deleter template.  This template is
 * designed to provide automagic memory management for protobuf-c
 * messages in RW code.
 */
template <class pointer_type_t = ProtobufCMessage>
struct UniquePtrProtobufCMessage
{
  ProtobufCInstance* instance_ = nullptr;
  UniquePtrProtobufCMessage(ProtobufCInstance* instance = nullptr)
  : instance_(instance)
  {}

  void operator()(ProtobufCMessage* obj) const
  {
    protobuf_c_message_free_unpacked(instance_, obj);
  }
  template <class msg_t>
  void operator()(msg_t* obj) const
  {
    protobuf_c_message_free_unpacked(instance_, &obj->base);
  }

  typedef std::unique_ptr<pointer_type_t,UniquePtrProtobufCMessage> uptr_t;
};

/*!
 * Protobuf-c keep-message deleter template.  This template is designed
 * to provide automagic memory management for protobuf-c messages in RW
 * code.
 *
 *     RWPB_T_MSG(Message_Type)* msg;
 *     msg = alloc...();
 *     UniquePtrProtobufCMessageUseBody<decltype(msg)>::uptr_t
 *       msg_uptr(&msg);
 *     // msg will be now be freed upon exit
 */
template <class pointer_type_t = ProtobufCMessage>
struct UniquePtrProtobufCMessageUseBody
{
  ProtobufCInstance* instance_ = nullptr;
  UniquePtrProtobufCMessageUseBody(ProtobufCInstance* instance = nullptr)
  : instance_(instance)
  {}

  void operator()(ProtobufCMessage* obj) const
  {
    protobuf_c_message_free_unpacked_usebody(instance_, obj);
  }
  template <class msg_t>
  void operator()(msg_t* obj) const
  {
    protobuf_c_message_free_unpacked_usebody(instance_, &obj->base);
  }

  typedef std::unique_ptr<pointer_type_t,UniquePtrProtobufCMessageUseBody> uptr_t;
};

/*!
 * Protobuf-c instance-based deleter.  This template is designed to
 * provide automagic memory management for memory allocated by
 * protobuf-c in RW code, when that memory is not a message.
 *
 *     uint8_t* data = protobuf_c_...();
 *     UniquePtrProtobufCFree<uint8_t>::uptr_t
 *       data_uptr(data, instance);
 *     // data will be now be freed upon exit
 */
template <class pointer_type_t = uint8_t>
struct UniquePtrProtobufCFree
{
  ProtobufCInstance* instance_ = nullptr;
  UniquePtrProtobufCFree(ProtobufCInstance* instance = nullptr)
  : instance_(instance)
  {}

  void operator()(pointer_type_t* data) const
  {
    protobuf_c_instance_free(instance_, data);
  }

  typedef std::unique_ptr<pointer_type_t, UniquePtrProtobufCFree> uptr_t;
};

/*! @} */

#endif /* __cplusplus */

/*! @} */

#endif // RW_YANGTOOLS_PB_H_
