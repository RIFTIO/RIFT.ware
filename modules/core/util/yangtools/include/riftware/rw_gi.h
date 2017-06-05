/* STANDARD_RIFT_IO_COPYRIGHT */

/*!
 * @file rw_gi.h
 * @author Sujithra Periasamy
 * @date 2015/03/10
 * @brief RiftWare protobuf gi common structures/functions.
 *
 * ATTN: This file properly belongs to protobuf-c?
 */

#ifndef RW_YANGTOOLS_GI_H_
#define RW_YANGTOOLS_GI_H_

typedef struct ProtobufCGiMessageBase ProtobufCGiMessageBase;
typedef struct ProtobufCGiMessageBox ProtobufCGiMessageBox;

#ifndef __GI_SCANNER__
#include <protobuf-c/rift-protobuf-c.h>
#endif  /* __GI_SCANNER__ */

__BEGIN_DECLS

#define PROTOBUF_C_GI_GET_PROTO(_boxed, __boxed_type, __proto_type) \
(G_GNUC_EXTENSION ({ \
 __boxed_type *__boxed = (_boxed); \
 __proto_type *__proto = (__boxed->s.message); \
\
 __proto; \
}))

#define PROTOBUF_C_GI_GET_ERROR_DOMAIN(_domain) \
    _domain##_PROTO_GI_ERROR

#define PROTOBUF_C_GI_CHECK_FOR_ERROR(_boxed, _error, _domain) \
G_STMT_START { \
  if ((_boxed)->s.message == NULL) { \
    g_set_error((_error), PROTOBUF_C_GI_GET_ERROR_DOMAIN(_domain), _domain##_PROTO_GI_ERROR_STALE_REFERENCE, \
                "Protobuf reference is stale."); \
  } \
} G_STMT_END

#define PROTOBUF_C_GI_CHECK_FOR_ERROR_RETURN(_boxed, _error, _domain) \
G_STMT_START { \
  if ((_boxed)->s.message == NULL) { \
    g_set_error((_error), PROTOBUF_C_GI_GET_ERROR_DOMAIN(_domain), _domain##_PROTO_GI_ERROR_STALE_REFERENCE, \
                "Protobuf reference is stale."); \
    return; \
  } \
} G_STMT_END

#define PROTOBUF_C_GI_RAISE_EXCEPTION(_error, _domain, _code, _str, ...) \
G_STMT_START { \
    g_set_error((_error), PROTOBUF_C_GI_GET_ERROR_DOMAIN(_domain), (_domain##_##_code), \
                _str, __VA_ARGS__); \
} G_STMT_END

#define PROTOBUF_C_GI_CHECK_FOR_ERROR_RETVAL(_boxed, _error, _domain, _retval) \
G_STMT_START { \
  if ((_boxed)->s.message == NULL) { \
    g_set_error((_error), PROTOBUF_C_GI_GET_ERROR_DOMAIN(_domain), _domain##_PROTO_GI_ERROR_STALE_REFERENCE, \
                "Protobuf reference is stale."); \
\
    return (_retval); \
  } \
} G_STMT_END

#define PROTOBUF_C_GI_CHECK_FOR_PARENT_ERROR(boxed_parent, _boxed, _error, _domain) \
 G_STMT_START { \
 if (((ProtobufCGiMessageBase *) (_boxed))->parent != NULL) { \
   if ((ProtobufCGiMessageBase *)(((ProtobufCGiMessageBase *) (_boxed))->parent) != (ProtobufCGiMessageBase *)boxed_parent) { \
     g_set_error((_error), PROTOBUF_C_GI_GET_ERROR_DOMAIN(_domain), _domain##_PROTO_GI_ERROR_HAS_PARENT, \
                 "Protobuf reference already has a parent."); \
   } \
 } \
} G_STMT_END

#define PROTOBUF_C_GI_CHECK_FOR_PARENT_ERROR_RETVAL(boxed_parent, _boxed, _error, _domain, _retval) \
 G_STMT_START { \
   if (((ProtobufCGiMessageBase *) (_boxed))->parent != NULL) { \
     if ((ProtobufCGiMessageBase *)(((ProtobufCGiMessageBase *) (_boxed))->parent) != (ProtobufCGiMessageBase *)boxed_parent) { \
       g_set_error((_error), PROTOBUF_C_GI_GET_ERROR_DOMAIN(_domain), _domain##_PROTO_GI_ERROR_HAS_PARENT, \
                   "Protobuf reference already has a parent."); \
\
     return (_retval); \
   } \
 } G_STMT_END


#define PROTOBUF_C_GI_IS_ZOMBIE(gi_base_) \
  ( \
      ((gi_base_)->parent != NULL) \
   && ((gi_base_)->parent_unref == NULL) \
  )

__END_DECLS

#endif // RW_YANGTOOLS_GI_H_
