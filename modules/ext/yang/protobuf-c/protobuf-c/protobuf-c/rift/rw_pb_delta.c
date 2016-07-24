/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rw_pb_delta.c
 * @author Sujithra Periasamy
 * @date 2015/08/21
 * @brief Rift PB Delta Implementation
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "rift-protobuf-c.h"
#include "rw_pb_delta.h"

#define PROTOBUF_C_FREF_IS_VALID(fref_) \
    PROTOBUF_C_IS_VALID_REF_HDR(&(fref_)->ref_hdr)

#define PROTOBUF_C_FREF_IS_VALID_FIELD(fref_) \
(\
   PROTOBUF_C_IS_VALID_REF_HDR(&(fref_)->ref_hdr) && \
   (NULL != fref_->message) && \
   (NULL != fref_->fdesc) \
)

#define FIELDREF_GI_RAISE_EXCEPTION(_error, _code, _str, ...) \
do { \
  g_set_error((_error), protobuf_c_field_ref_error_quark(), _code, \
              _str, ##__VA_ARGS__); \
} while (0);

GQuark
protobuf_c_field_ref_error_quark (void)
{
  static volatile gsize quark = 0;

  if (g_once_init_enter (&quark))
    g_once_init_leave (&quark,  g_quark_from_static_string ("ProtobufC_FieldRef_Error"));

  return quark;
}

static const GEnumValue ProtobufCFieldRefGiError_values[] = {
  {FIELDREF_PROTO_GI_ERROR_INVALID_VALUE, "INVALID_VALUE", "INVALID_VALUE"},
  {FIELDREF_PROTO_GI_ERROR_NOT_SUPPORTED, "NOT_SUPPORTED", "NOT_SUPPORTED"},
  {0, NULL, NULL},
};

GType protobuf_c_field_ref_error_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type)
    type = g_enum_register_static("ProtobufC_FieldRef_Error", ProtobufCFieldRefGiError_values);

  return type;
}

static inline void
protobuf_c_delta_mark_field_deleted(ProtobufCInstance* instance,
                                    ProtobufCMessage* msg,
                                    const ProtobufCFieldDescriptor* fdesc)
{
  PROTOBUF_C_ASSERT(PROTOBUF_C_IS_MESSAGE(msg));
  PROTOBUF_C_ASSERT(PROTOBUF_C_MESSAGE_IS_DATA(msg));
  PROTOBUF_C_ASSERT(!msg->unknown_buffer);

  ProtobufCDeleteDelta* delta = 
      protobuf_c_instance_alloc(instance, sizeof(ProtobufCDeleteDelta));

  delta->me = FALSE;
  delta->child_tag = fdesc->id;

  msg->delete_delta = delta;
  PROTOBUF_C_FLAG_SET(
      msg->ref_hdr.magic_flags, UNKNOWN_BUFFER_UNION, PROTOBUF_C_FLAG_UNKNOWN_BUFFER_UNION_DELTA);
}

static inline void
protobuf_c_delta_mark_me_deleted(ProtobufCInstance* instance,
                                 ProtobufCMessage* msg)
{
  PROTOBUF_C_ASSERT(PROTOBUF_C_IS_MESSAGE(msg));
  PROTOBUF_C_ASSERT(PROTOBUF_C_MESSAGE_IS_DATA(msg));
  PROTOBUF_C_ASSERT(!msg->unknown_buffer);

  ProtobufCDeleteDelta* delta = 
      protobuf_c_instance_alloc(instance, sizeof(ProtobufCDeleteDelta));

  delta->me = TRUE;
  delta->child_tag = 0;

  msg->delete_delta = delta;
  PROTOBUF_C_FLAG_SET(
      msg->ref_hdr.magic_flags, UNKNOWN_BUFFER_UNION, PROTOBUF_C_FLAG_UNKNOWN_BUFFER_UNION_DELTA);
}

ProtobufCFieldReference*
protobuf_c_field_ref_alloc(ProtobufCInstance *instance)
{
  ProtobufCFieldReference* fref = NULL;
  instance = protobuf_c_instance_get(instance);

  fref = protobuf_c_instance_alloc(instance, sizeof(ProtobufCFieldReference));
  memset(fref, 0, sizeof(ProtobufCFieldReference));
  protobuf_c_ref_header_create(&fref->ref_hdr, PROTOBUF_C_FLAG_REF_STATE_COUNTED, 0);
  fref->instance = instance;
  return fref;
}

static void
protobuf_c_field_ref_free(ProtobufCReferenceHeader* obj)
{
  ProtobufCFieldReference* fref = (ProtobufCFieldReference *)obj;
  PROTOBUF_C_ASSERT(PROTOBUF_C_FREF_IS_VALID(fref));
  protobuf_c_instance_free(fref->instance, fref);
}

void
protobuf_c_field_ref_unref(ProtobufCFieldReference* fref)
{
  PROTOBUF_C_ASSERT(PROTOBUF_C_FREF_IS_VALID(fref));
  protobuf_c_ref_header_unref(&fref->ref_hdr, protobuf_c_field_ref_free);
}

ProtobufCFieldReference*
protobuf_c_field_ref_ref(ProtobufCFieldReference* fref)
{
  PROTOBUF_C_ASSERT(PROTOBUF_C_FREF_IS_VALID(fref));
  ProtobufCReferenceHeader* ref_hdr = protobuf_c_ref_header_ref(&fref->ref_hdr);
  PROTOBUF_C_ASSERT(ref_hdr == &fref->ref_hdr);
  return fref;
}

ProtobufCFieldReference*
protobuf_c_field_ref_alloc_goto(ProtobufCInstance* instance,
                                ProtobufCMessage* msg,
                                const ProtobufCFieldDescriptor* fdesc,
                                size_t index,
                                uint64_t* status)
{
  PROTOBUF_C_ASSERT(PROTOBUF_C_IS_MESSAGE(msg));
  PROTOBUF_C_ASSERT(fdesc);

  instance = protobuf_c_instance_get(instance);
  ProtobufCFieldReference* fref = protobuf_c_field_ref_alloc(instance);

  *status = protobuf_c_field_ref_goto(fref, msg, fdesc, index);
  return fref;
}

uint64_t
protobuf_c_field_ref_goto(ProtobufCFieldReference* fref,
                          ProtobufCMessage* msg,
                          const ProtobufCFieldDescriptor* fdesc,
                          size_t index)
{
  PROTOBUF_C_ASSERT(PROTOBUF_C_IS_MESSAGE(msg));
  PROTOBUF_C_ASSERT(PROTOBUF_C_FREF_IS_VALID(fref));
  PROTOBUF_C_ASSERT(fdesc);

  fref->message = msg;
  fref->fdesc   = fdesc;
  fref->status  = 0; // Clear all

  /* Trivial implementation of the goto.
   * Will turn into a complex function soon.
   */
  if (PROTOBUF_C_MESSAGE_IS_DELTA(msg)) {
    PROTOBUF_C_ASSERT(msg->delete_delta);
    if (msg->delete_delta->child_tag == fdesc->id) {
      fref->status |= PROTOBUF_C_FIELD_REF_STATUS_DELETED;
      return fref->status;
    }
  }

  ProtobufCFieldInfo finfo = {NULL};
  protobuf_c_boolean fpres = protobuf_c_message_get_field_instance(
      fref->instance, msg, fdesc, index, &finfo);
  if (fpres) {
    fref->status |= PROTOBUF_C_FIELD_REF_STATUS_PRESENT;
  } else {
    fref->status |= PROTOBUF_C_FIELD_REF_STATUS_MISSING;
  }

  return fref->status;
}

uint64_t 
protobuf_c_field_ref_goto_proto_name(ProtobufCFieldReference* fref,
                                     ProtobufCMessage* msg,
                                     const char* fname)
{
  PROTOBUF_C_ASSERT(PROTOBUF_C_IS_MESSAGE(msg));
  PROTOBUF_C_ASSERT(PROTOBUF_C_FREF_IS_VALID(fref));

  const ProtobufCFieldDescriptor* fdesc =
     protobuf_c_message_descriptor_get_field_by_name(msg->descriptor, fname);
  PROTOBUF_C_ASSERT(fdesc);

  return protobuf_c_field_ref_goto(fref, msg, fdesc, 0);
}

uint64_t
protobuf_c_field_ref_goto_tag(ProtobufCFieldReference* fref,
                              ProtobufCMessage* msg,
                              uint32_t tag)
{
  PROTOBUF_C_ASSERT(PROTOBUF_C_IS_MESSAGE(msg));
  PROTOBUF_C_ASSERT(PROTOBUF_C_FREF_IS_VALID(fref));

  const ProtobufCFieldDescriptor* fdesc =
      protobuf_c_message_descriptor_get_field(msg->descriptor, tag);
  PROTOBUF_C_ASSERT(fdesc);

  return protobuf_c_field_ref_goto(fref, msg, fdesc, 0);
}

uint64_t
protobuf_c_field_ref_goto_whole_message(ProtobufCFieldReference* fref,
                                        ProtobufCMessage* msg)
{
  PROTOBUF_C_ASSERT(PROTOBUF_C_IS_MESSAGE(msg));
  PROTOBUF_C_ASSERT(PROTOBUF_C_FREF_IS_VALID(fref));

  fref->message = msg;
  fref->fdesc = NULL;
  fref->status = 0;
  fref->status |= PROTOBUF_C_FIELD_REF_STATUS_WHOLE_MESSAGE;

  if (PROTOBUF_C_MESSAGE_IS_DELTA(msg)) {
    PROTOBUF_C_ASSERT(msg->delete_delta);
    if (msg->delete_delta->me) {
      fref->status |= PROTOBUF_C_FIELD_REF_STATUS_DELETED;
    }
  }

  return fref->status;
}

protobuf_c_boolean
protobuf_c_field_ref_is_field_deleted(const ProtobufCFieldReference* fref)
{
  PROTOBUF_C_ASSERT(PROTOBUF_C_FREF_IS_VALID(fref));
  protobuf_c_boolean is_deleted = !!(fref->status & PROTOBUF_C_FIELD_REF_STATUS_DELETED);
  return is_deleted;
}

protobuf_c_boolean
protobuf_c_field_ref_is_field_present(const ProtobufCFieldReference* fref)
{
  PROTOBUF_C_ASSERT(PROTOBUF_C_FREF_IS_VALID(fref));
  protobuf_c_boolean is_present = !!(fref->status & PROTOBUF_C_FIELD_REF_STATUS_PRESENT);
  return is_present;
}

protobuf_c_boolean
protobuf_c_field_ref_is_field_missing(const ProtobufCFieldReference* fref)
{
  PROTOBUF_C_ASSERT(PROTOBUF_C_FREF_IS_VALID(fref));
  protobuf_c_boolean is_missing = !!(fref->status & PROTOBUF_C_FIELD_REF_STATUS_MISSING);
  return is_missing;
}

void
protobuf_c_field_ref_mark_field_deleted(ProtobufCFieldReference* fref)
{
  PROTOBUF_C_ASSERT(PROTOBUF_C_FREF_IS_VALID(fref));

  bool is_whole_msg = !!(fref->status & PROTOBUF_C_FIELD_REF_STATUS_WHOLE_MESSAGE);
  if (is_whole_msg) {
    PROTOBUF_C_ASSERT(!fref->fdesc);
    protobuf_c_delta_mark_me_deleted(fref->instance, fref->message);
  } else {
    PROTOBUF_C_ASSERT(fref->fdesc);
    protobuf_c_delta_mark_field_deleted(fref->instance, fref->message, fref->fdesc);
  }
  fref->status |= PROTOBUF_C_FIELD_REF_STATUS_DELETED;
}

gboolean
protobuf_c_field_ref_pb_text_set(ProtobufCFieldReference* fref, 
                                 const gchar* strval, 
                                 GError **error)
{
  PROTOBUF_C_ASSERT(PROTOBUF_C_FREF_IS_VALID(fref));
  PROTOBUF_C_ASSERT(fref->message);
  PROTOBUF_C_ASSERT(fref->fdesc);

  PROTOBUF_C_ASSERT (fref->fdesc->type != PROTOBUF_C_TYPE_MESSAGE &&
             fref->fdesc->type != PROTOBUF_C_TYPE_BYTES);

  protobuf_c_boolean ok = protobuf_c_message_set_field_text_value(
      fref->instance, fref->message, fref->fdesc, strval, strlen(strval));

  if (ok) {
    fref->status = PROTOBUF_C_FIELD_REF_STATUS_PRESENT; // ATTN: Check this
    return ok;
  }

  FIELDREF_GI_RAISE_EXCEPTION(error, 
                              FIELDREF_PROTO_GI_ERROR_INVALID_VALUE,
                              "Invalid text for set_pb_text");

  return ok;
}

G_DEFINE_BOXED_TYPE(
  ProtobufCFieldReference,
  protobuf_c_field_ref,
  protobuf_c_field_ref_ref,
  protobuf_c_field_ref_unref);

static const GEnumValue _protobuf_c_field_ref_status_values[] = {
  { PROTOBUF_C_FIELD_REF_STATUS_VALID,         "VALID",    "VALID"    },
  { PROTOBUF_C_FIELD_REF_STATUS_DELETED,       "DELETED",  "DELETED"  },
  { PROTOBUF_C_FIELD_REF_STATUS_PRESENT,       "PRESENT",  "PRESENT"  },
  { PROTOBUF_C_FIELD_REF_STATUS_MISSING,       "MISSING",  "MISSING"  },
  { PROTOBUF_C_FIELD_REF_STATUS_SELF,          "SELF",     "SELF"     },
  { PROTOBUF_C_FIELD_REF_STATUS_WHOLE_MESSAGE, "WHOLE_MESSAGE", "WHOLE_MESSAGE" },
  { PROTOBUF_C_FIELD_REF_STATUS_FIELD,         "FIELD", "FIELD" },
  { 0, NULL, NULL}
};

/* Registers the Enum resulting in a GType,
 * this also tells it the names and values for the enum.
 *
 * This defines protobuf_c_field_ref_status_get_type()
 */
GType protobuf_c_field_ref_status_get_type(void)
{
  static GType type = 0; /* G_TYPE_INVALID */

  if (!type)
    type = g_enum_register_static("ProtobufCFieldRefStatus",
                                  _protobuf_c_field_ref_status_values);
  return type;
}
