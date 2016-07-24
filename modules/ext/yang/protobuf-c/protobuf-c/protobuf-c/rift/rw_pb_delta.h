/* STANDARD_RIFT_IO_COPYRIGHT */

/*!
 * @file rw_delta.h
 * @author Sujithra Periasamy
 * @date 2015/08/21
 * @brief RiftWare PB Delta
 */

#ifndef __RW_PB_DELTA_H__
#define __RW_PB_DELTA_H__

PROTOBUF_C_BEGIN_DECLS

struct ProtobufCFieldReference;
typedef struct ProtobufCFieldReference ProtobufCFieldReference;

#ifndef __GI_SCANNER__
/*!
 * A field reference has a status value, which is a bitmask that encodes several
 * boolean indications about the field. The staus can be used as a quick
 * shorthand to replace more expensive function calls; this may be more useful
 * in the GI context than in C, but the bitmaks does enable some advanced C
 * usage. The bitmask will be a 64-bit unsigned integer. The following are some
 * prospective values of the bitmask.
 */
typedef enum {
  /*!
   * The field reference is set to a valid field or message
   */
  PROTOBUF_C_FIELD_REF_STATUS_VALID   = 1 << 0,

  /*!
   *  The field has been marked created.
   */
  PROTOBUF_C_FIELD_REF_STATUS_DELETED = 1 << 1,

  /*!
   *  The field has been marked deleted
   */
  PROTOBUF_C_FIELD_REF_STATUS_PRESENT = 1 << 2,

  /*!
   * The field is a known field.
   */
  PROTOBUF_C_FIELD_REF_STATUS_MISSING = 1 << 3,

  /*!
   * The field is a unknown field
   */
  PROTOBUF_C_FIELD_REF_STATUS_KNOWN = 1 << 4,

  /*!
   * The field is present in the message.
   */
  PROTOBUF_C_FIELD_REF_STATUS_UNKNOWN = 1 << 5,

  /*!
   * The field has been marked created.
   */
  PROTOBUF_C_FIELD_REF_STATUS_CREATED = 1 << 6,

  /*!
   * The current target is the message itself.
   */
  PROTOBUF_C_FIELD_REF_STATUS_SELF = 1 << 7,

  /*!
   * The current target is a single field
   */
  PROTOBUF_C_FIELD_REF_STATUS_OPTIONAL = 1 << 8,

  /*!
   * The current target listy
   */
  PROTOBUF_C_FIELD_REF_STATUS_LISTY = 1 << 9,

  /*!
   * The current target leafy
   */
  PROTOBUF_C_FIELD_REF_STATUS_LEAFY = 1 << 10,

  /*!
   * The current target is a message
   */
  PROTOBUF_C_FIELD_REF_STATUS_WHOLE_MESSAGE = 1 << 11,

  /*!
   * The current target is a single field
   */
  PROTOBUF_C_FIELD_REF_STATUS_FIELD = 1 << 12,

  /*!
   * The current target is a whole list
   */
  PROTOBUF_C_FIELD_REF_STATUS_WHOLE_LIST = 1 << 13,

  /*!
   * The current target is a list entry.
   */
  PROTOBUF_C_FIELD_REF_STATUS_LIST_ENTRY = 1 << 14

} ProtobufCFieldRefStatus;


/*!
 * The `ProtobufCFieldReference` structure.
 *
 *  A field reference object is a type that is able to refer to any kind of
 *  protobuf-c field: a whole message, a sub-message, a whole list, a list
 *  entry, the end of the list (past the last real entry as in C++ container
 *  end()), an optional field, a required field, the unknown fields, a specific
 *  unknown field, or the end of the unknown fields. A field reference can be
 *  used to get, set or copy the field value; get or set the delta status; get
 *  meta-data about the field; or perform a limited set of iterations.
 */
struct ProtobufCFieldReference
{
  /*!
   * The `ProtobufCReferenceHeader` header for reference count
   * management of the field ref objects.
   */
  ProtobufCReferenceHeader ref_hdr;
  /*!
   * The `ProtobufCInstance` structure.
   */
  ProtobufCInstance* instance;
  /*!
   * The ProbufCMessage that the field ref refers to
   */
  ProtobufCMessage* message;
  /*!
   * The field desc of the field that the ref points to
   */
  const ProtobufCFieldDescriptor *fdesc;
  /*!
   * The FieldReference Status bitmask
   */
  uint64_t status;
  /*!
   * The field value. 
   */
  void* value;

  /*!
   * The index or the key if the FieldRef points to a listy entry.
   */
  union {
    size_t index;
  };
};

/*!
 * Macro to initialize the FieldReference object
 */
#define PROTOBUF_C_FIELD_REF_INIT_VALUE {PROTOBUF_C_INIT_REF_HDR(PROTOBUF_C_FLAG_REF_STATE_OWNED, 0), NULL, NULL, NULL, 0, NULL, {0}}

/*!
 * Convenience that combine goto and status tests.
 */
#define PROTOBUF_C_FIELD_REF_IS(pbcfr_, pbcm_, field_name_, status_) \
(\
   (protobuf_c_field_ref_goto((pbcfr_), \
                              &(pbcm_)->base, \
                              (pbcm_)->base_concrete.descriptor->fields.field_name_, \
                              0) \
         & PROTOBUF_C_FIELD_REF_STATUS_##status_) \
)

#endif //__GI_SCANNER__

typedef enum {

  FIELDREF_PROTO_GI_ERROR_INVALID_VALUE = 10001,
  FIELDREF_PROTO_GI_ERROR_NOT_SUPPORTED = 10002,

} ProtobufCFieldRefGiError;

GQuark protobuf_c_field_ref_error_quark(void);
GType protobuf_c_field_ref_error_get_type(void);

/// @cond GI_SCANNER
/**
 * protobuf_c_field_ref_alloc: (constructor)
 * @instance: (optional) (nullable)
 * Returns: (transfer full)
 */
/// @endcond
ProtobufCFieldReference*
protobuf_c_field_ref_alloc(ProtobufCInstance* instance);

ProtobufCFieldReference*
protobuf_c_field_ref_alloc_goto(ProtobufCInstance* instance,
                                ProtobufCMessage* msg,
                                const ProtobufCFieldDescriptor* fdesc,
                                size_t n,
                                uint64_t* status);

uint64_t
protobuf_c_field_ref_goto(ProtobufCFieldReference* fref,
                          ProtobufCMessage* msg,
                          const ProtobufCFieldDescriptor* fdesc,
                          size_t n);
/// @cond GI_SCANNER
/**
 * protobuf_c_field_ref_goto_proto_name:
 * @fref:
 * @msg:
 * @fname:
 * Returns:
 */
/// @endcond
uint64_t 
protobuf_c_field_ref_goto_proto_name(ProtobufCFieldReference* fref,
                                     ProtobufCMessage* msg,
                                     const char* fname);
/// @cond GI_SCANNER
/**
 * protobuf_c_field_ref_goto_tag:
 * @fref:
 * @msg:
 * @tag:
 * Returns:
 */
/// @endcond
uint64_t
protobuf_c_field_ref_goto_tag(ProtobufCFieldReference* fref,
                              ProtobufCMessage* msg,
                              uint32_t tag);
/// @cond GI_SCANNER
/**
 * protobuf_c_field_ref_goto_whole_message:
 * @fref:
 * @msg:
 * Returns:
 */
/// @endcond
uint64_t
protobuf_c_field_ref_goto_whole_message(ProtobufCFieldReference* fref,
                                        ProtobufCMessage* msg);

/// @cond GI_SCANNER
/**
 * protobuf_c_field_ref_is_field_deleted:
 * @fref:
 * Returns:
 */
/// @endcond
protobuf_c_boolean
protobuf_c_field_ref_is_field_deleted(const ProtobufCFieldReference* fref);

/// @cond GI_SCANNER
/**
 * protobuf_c_field_ref_is_field_present:
 * @fref:
 * Returns:
 */
/// @endcond
protobuf_c_boolean
protobuf_c_field_ref_is_field_present(const ProtobufCFieldReference* fref);

/// @cond GI_SCANNER
/**
 * protobuf_c_field_ref_is_field_missing:
 * @fref:
 * Returns:
 */
/// @endcond
protobuf_c_boolean
protobuf_c_field_ref_is_field_missing(const ProtobufCFieldReference* fref);

/// @cond GI_SCANNER
/**
 * protobuf_c_field_ref_mark_field_deleted:
 * @fref:
 * Returns:
 */
/// @endcond
void
protobuf_c_field_ref_mark_field_deleted(ProtobufCFieldReference* fref);

/// @cond GI_SCANNER
/**
 * protobuf_c_field_ref_pb_text_set:
 * @fref:
 * @strval:
 * Returns:
 */
gboolean
protobuf_c_field_ref_pb_text_set(ProtobufCFieldReference* fref, const gchar* strval, GError **error);

GType protobuf_c_field_ref_status_get_type(void);

GType protobuf_c_field_ref_get_type(void);

void
protobuf_c_field_ref_unref(ProtobufCFieldReference* fref);

ProtobufCFieldReference*
protobuf_c_field_ref_ref(ProtobufCFieldReference* fref);

PROTOBUF_C_END_DECLS

#endif
