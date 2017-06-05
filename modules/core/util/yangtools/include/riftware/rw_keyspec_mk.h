/* STANDARD_RIFT_IO_COPYRIGHT */

/**
 * @file rw_keyspec_mk.h
 * @author Sujithra Periasamy
 * @date 2014/12/18
 * @brief RiftWare schema basic minikey types.
 */

#ifndef RW_YANGTOOLS_SCHEMA_MINIKEY_H_
#define RW_YANGTOOLS_SCHEMA_MINIKEY_H_

#include <sys/cdefs.h>
#include <protobuf-c/rift-protobuf-c.h>
#include "rwlib.h"
#include "rw_keyspec.h"


// Need this because doxygen parser thinks this is a C file and misses the C++ definitions
#ifdef RW_DOXYGEN_PARSING
#define __cplusplus 201103
#endif

#if __cplusplus
#if __cplusplus < 201103L
#error "Requires C++11"
#endif
#endif

/**
 * @defgroup RwKeyspec basic minikey types
 * @{
 */

__BEGIN_DECLS

#ifndef __GI_SCANNER__
/*
 * Opaque struct representing any kind of minikey, regardless of 
 * static type.
 */
typedef char rw_schema_minikey_opaque_t[RWPB_MINIKEY_OPAQ_LEN];

/* This type/structure will be used in case Gi support is needed */
typedef struct rw_schema_minikey_t rw_schema_minikey_t;

struct rw_schema_minikey_t {
  rw_schema_minikey_opaque_t minikey;
};

#endif /* __GI_SCANNER__ */

/*
 * Minikey message descriptor
 */
typedef struct rw_minikey_desc_s {
  const ProtobufCMessageDescriptor* pbc_mdesc;
  RwSchemaElementType key_type;
} rw_minikey_desc_t;


/*
 * key type int32
 */
typedef union rw_minikey_basic_int32_u {
  struct {
    const rw_minikey_desc_t *desc;
    int32_t key;
  } k;
  rw_schema_minikey_opaque_t opaque;
} rw_minikey_basic_int32_t;

extern const rw_minikey_desc_t rw_var_minikey_basic_int32_desc;

static inline
void rw_minikey_basic_int32_init(rw_minikey_basic_int32_t *mk)
{
  memset(mk, 0, sizeof(rw_minikey_basic_int32_t));
  mk->k.desc = &rw_var_minikey_basic_int32_desc;
}

#define RWPB_MINIKEY_DEF_INIT_INT32(var_, key_) \
  rw_minikey_basic_int32_t var_; \
  rw_minikey_basic_int32_init(&var_); \
  var_.k.key = key_;

/*
 * key type int64
 */
typedef union rw_minikey_basic_int64_u {
  struct {
    const rw_minikey_desc_t *desc;
    int64_t key;
  } k;
  rw_schema_minikey_opaque_t opaque;
} rw_minikey_basic_int64_t;

extern const rw_minikey_desc_t rw_var_minikey_basic_int64_desc;

static inline
void rw_minikey_basic_int64_init(rw_minikey_basic_int64_t *mk)
{
  memset(mk, 0, sizeof(rw_minikey_basic_int64_t));
  mk->k.desc = &rw_var_minikey_basic_int64_desc;
}

#define RWPB_MINIKEY_DEF_INIT_INT64(var_, key_) \
  rw_minikey_basic_int64_t var_; \
  rw_minikey_basic_int64_init(&var_); \
  var_.k.key = key_;

/*
 * key type uint32
 */
typedef union rw_minikey_basic_uint32_u {
  struct {
    const rw_minikey_desc_t *desc;
    uint32_t key;
  } k;
  rw_schema_minikey_opaque_t opaque;
} rw_minikey_basic_uint32_t;

extern const rw_minikey_desc_t rw_var_minikey_basic_uint32_desc;

static inline
void rw_minikey_basic_uint32_init(rw_minikey_basic_uint32_t *mk)
{
  memset(mk, 0, sizeof(rw_minikey_basic_uint32_t)); 
  mk->k.desc = &rw_var_minikey_basic_uint32_desc;
}

#define RWPB_MINIKEY_DEF_INIT_UINT32(var_, key_) \
  rw_minikey_basic_uint32_t var_; \
  rw_minikey_basic_uint32_init(&var_); \
  var_.k.key = key_;

/*
 * key type uint64
 */
typedef union rw_minikey_basic_uint64_u {
  struct {
    const rw_minikey_desc_t *desc;
    uint64_t key;
  } k;
  rw_schema_minikey_opaque_t opaque;
} rw_minikey_basic_uint64_t;

extern const rw_minikey_desc_t rw_var_minikey_basic_uint64_desc;

static inline
void rw_minikey_basic_uint64_init(rw_minikey_basic_uint64_t *mk)
{
  memset(mk, 0, sizeof(rw_minikey_basic_uint64_t)); 
  mk->k.desc = &rw_var_minikey_basic_uint64_desc;
}

#define RWPB_MINIKEY_DEF_INIT_UINT64(var_, key_) \
  rw_minikey_basic_uint64_t var_; \
  rw_minikey_basic_uint64_init(&var_); \
  var_.k.key = key_;

/*
 * key type float
 */
typedef union rw_minikey_basic_float_u {
  struct {
    const rw_minikey_desc_t *desc;
    float key;
  } k;
  rw_schema_minikey_opaque_t opaque;
} rw_minikey_basic_float_t;

extern const rw_minikey_desc_t rw_var_minikey_basic_float_desc;

static inline
void rw_minikey_basic_float_init(rw_minikey_basic_float_t *mk)
{
  memset(mk, 0, sizeof(rw_minikey_basic_float_t)); 
  mk->k.desc = &rw_var_minikey_basic_float_desc;
}

#define RWPB_MINIKEY_DEF_INIT_FLOAT(var_, key_) \
  rw_minikey_basic_float_t var_; \
  rw_minikey_basic_float_init(&var_); \
  var_.k.key = key_;

/*
 * key type double
 */
typedef union rw_minikey_basic_double_u {
  struct {
    const rw_minikey_desc_t *desc;
    double key;
  } k;
  rw_schema_minikey_opaque_t opaque;
} rw_minikey_basic_double_t;

extern const rw_minikey_desc_t rw_var_minikey_basic_double_desc;

static inline
void rw_minikey_basic_double_init(rw_minikey_basic_double_t *mk)
{
  memset(mk, 0, sizeof(rw_minikey_basic_double_t)); 
  mk->k.desc = &rw_var_minikey_basic_double_desc;
}

#define RWPB_MINIKEY_DEF_INIT_DOUBLE(var_, key_) \
  rw_minikey_basic_double_t var_; \
  rw_minikey_basic_double_init(&var_); \
  var_.k.key = key_;

/*
 * key type pointy strings.
 */
typedef union rw_minikey_basic_string_pointy_u {
  struct {
    const rw_minikey_desc_t *desc;
    char *key;
  } k;
  rw_schema_minikey_opaque_t opaque;
} rw_minikey_basic_string_pointy_t;

extern const rw_minikey_desc_t rw_var_minikey_basic_string_pointy_desc;

static inline
void rw_minikey_basic_string_pointy_init(rw_minikey_basic_string_pointy_t *mk) 
{
  memset(mk, 0, sizeof(rw_minikey_basic_string_pointy_t)); 
  mk->k.desc = &rw_var_minikey_basic_string_pointy_desc;
}

#define RWPB_MINIKEY_DEF_INIT_STRING_POINTY(var_, key_) \
    rw_minikey_basic_string_pointy_t var_;\
    rw_minikey_basic_string_pointy_init(&var_); \
    var_.k.key = key_;

/*
 * key type fixed length strings.
 */
typedef union rw_minikey_basic_string_u {
  struct {
    const rw_minikey_desc_t *desc;
    char key[RWPB_MINIKEY_MAX_SKEY_LEN];
  } k;
  rw_schema_minikey_opaque_t opaque;
} rw_minikey_basic_string_t;

extern const rw_minikey_desc_t rw_var_minikey_basic_string_desc;

static inline
void rw_minikey_basic_string_init(rw_minikey_basic_string_t *mk) 
{
  memset(mk, 0, sizeof(rw_minikey_basic_string_t)); 
  mk->k.desc = &rw_var_minikey_basic_string_desc;
}

#define RWPB_MINIKEY_DEF_INIT_STRING(var_, key_) \
  rw_minikey_basic_string_t var_; \
  rw_minikey_basic_string_init(&var_); \
  RW_ASSERT(strlen(key_) < RWPB_MINIKEY_MAX_SKEY_LEN);\
  strcpy(var_.k.key, key_);

/*
 * key type bool.
 */
typedef union rw_minikey_basic_bool_u {
  struct {
    const rw_minikey_desc_t *desc;
    int key;
  } k;
  rw_schema_minikey_opaque_t opaque;
} rw_minikey_basic_bool_t;

extern const rw_minikey_desc_t rw_var_minikey_basic_bool_desc;

static inline
void rw_minikey_basic_bool_init(rw_minikey_basic_bool_t *mk) 
{
  memset(mk, 0, sizeof(rw_minikey_basic_bool_t)); 
  mk->k.desc = &rw_var_minikey_basic_bool_desc;
}

#define RWPB_MINIKEY_DEF_INIT_BOOL(var_, key_) \
  rw_minikey_basic_bool_t var_; \
  rw_minikey_basic_bool_init(&var_); \
  var_.k.key = key_;

/*
 * key type bytes pointy
 */
typedef union rw_minikey_basic_bytes_pointy_u {
  struct {
    const rw_minikey_desc_t *desc;
    uint8_t *key;
    int key_len;
  } k;
  rw_schema_minikey_opaque_t opaque;
} rw_minikey_basic_bytes_pointy_t;

extern const rw_minikey_desc_t rw_var_minikey_basic_bytes_pointy_desc;

static inline
void rw_minikey_basic_bytes_pointy_init(rw_minikey_basic_bytes_pointy_t *mk) 
{
  memset(mk, 0, sizeof(rw_minikey_basic_bytes_pointy_t)); 
  mk->k.desc = &rw_var_minikey_basic_bytes_pointy_desc;
}

#define RWPB_MINIKEY_DEF_INIT_BYTES_POINTY(var_, key_, keylen_) \
    RW_CRASH(); /* Not implemented for now. */

/*
 * key type bytes with fixed max length
 */
typedef union rw_minikey_basic_bytes_u {
  struct {
    const rw_minikey_desc_t *desc;
    uint8_t key[RWPB_MINIKEY_MAX_BKEY_LEN];
    int key_len;
  } k;
  rw_schema_minikey_opaque_t opaque;
} rw_minikey_basic_bytes_t;

extern const rw_minikey_desc_t rw_var_minikey_basic_bytes_desc;

static inline
void rw_minikey_basic_bytes_init(rw_minikey_basic_bytes_t *mk) 
{
  memset(mk, 0, sizeof(rw_minikey_basic_bytes_t)); 
  mk->k.desc = &rw_var_minikey_basic_bytes_desc;
}

#define RWPB_MINIKEY_DEF_INIT_BYTES(var_, key_, keylen_) \
  rw_minikey_basic_bytes_t var_; \
  rw_minikey_basic_bytes_init(&var_); \
  RW_ASSERT(keylen_ <= RWPB_MINIKEY_MAX_BKEY_LEN); \
  memcpy(var_.k.key, key_, keylen_); \
  var_.k.key_len = keylen_;

/*
 * Minikey APIs
 */

rw_status_t
rw_schema_mk_get_from_message(const ProtobufCMessage *msg,
                              rw_schema_minikey_opaque_t *mk);

rw_status_t
rw_schema_mk_get_from_path_entry(const rw_keyspec_entry_t *path_entry,
                                 rw_schema_minikey_opaque_t *mk);

rw_status_t
rw_schema_mk_copy_to_path_entry(const rw_schema_minikey_opaque_t *mk,
                                rw_keyspec_entry_t *path_entry);

rw_status_t
rw_schema_mk_copy_to_keyspec(rw_keyspec_path_t *ks,
                             rw_schema_minikey_opaque_t *mk,
                             size_t path_index);

bool
rw_schema_mk_is_equal(const rw_schema_minikey_opaque_t *mk1,
                      const rw_schema_minikey_opaque_t *mk2);

bool
rw_schema_mk_path_entry_is_equal(const rw_schema_minikey_opaque_t *mk,
                                 const rw_keyspec_entry_t *path_entry);

bool
rw_schema_mk_message_is_equal(const rw_schema_minikey_opaque_t *mk,
                              const ProtobufCMessage *msg);

int
rw_schema_mk_get_print_buffer(const rw_schema_minikey_opaque_t *mk,
                              char *buff,
                              size_t buff_len);
__END_DECLS

/** @} */

#endif // RW_YANGTOOLS_SCHEMA_MINIKEY_H_
