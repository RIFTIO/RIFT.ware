
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#ifndef __RW_SCHEMA_GI_H__ 
#define __RW_SCHEMA_GI_H__

#include <sys/cdefs.h>
#include <glib-object.h>
#include "yangpb_gi.h"
#include "rwtypes_gi.h"
#include "yangmodel_gi.h"
#include <protobuf-c/rift-protobuf-c.h>

#ifndef __GI_SCANNER__ 
#include "yangmodel.h"
#include "rwlib.h"
#include "rwtypes.h"
#endif

__BEGIN_DECLS

typedef struct rw_keyspec_path_t rw_keyspec_path_t;
typedef struct rw_keyspec_entry_t rw_keyspec_entry_t;
typedef struct rw_keyspec_instance_t rw_keyspec_instance_t;
/* This structure will be used only by functions that needs Gi support */
typedef struct rw_schema_minikey_t rw_schema_minikey_t;

GType rw_keyspec_entry_get_type(void);
GType rw_keyspec_path_get_type(void);
GType rw_keyspec_instance_get_type(void);
GType rw_schema_minikey_get_type(void);

typedef enum {
  RW_XPATH_KEYSPEC,
  RW_XPATH_INSTANCEID,
} rw_xpath_type_t;

GType rw_keyspec_xpath_type_get_type(void);


/*!
 * Implement keyspec from xpath string. According to RFC 6020 rule
 * "instance-identifier", as described in 9.13.  The purpose of this
 * task is to allow a string to be converted into a keyspec, such 
 * as would be used on the CLI or in python.
 *
 * @param schema The schema
 * @param xpath The xpath string
 * @patam keyspec The return keyspec
 */
/// @cond GI_SCANNER
/**
 * rw_keyspec_path_from_xpath:
 * @schema:
 * @xpath:
 * @type:
 * @instance: (optional) (nullable)
 * Returns: (transfer full) (nullable)
 */
/// @endcond
rw_keyspec_path_t* rw_keyspec_path_from_xpath(const rw_yang_pb_schema_t* schema,
                                              const char* xpath,
                                              rw_xpath_type_t type,
                                              rw_keyspec_instance_t* instance);

/*!
 * Create rw_keyspec_path_to_xpath(), to be the inverse of from_xpath(). 
 * Reimplement rw_keyspec_path_get_strpath() and rw_keyspec_path_update_strpath() 
 * to be in terms of rw_keyspec_path_to_xpath().
 * 
 * @param schema The scheme
 * @param keyspec The keyspec
 * @param xpath The xpath string
 */
/// @cond GI_SCANNER
/**
 * rw_keyspec_path_to_xpath:
 * @keyspec:
 * @schema:
 * @instance: (optional) (nullable)
 * Returns: (transfer full) (nullable)
 */
/// @endcond
char* rw_keyspec_path_to_xpath(const rw_keyspec_path_t* keyspec,
                               const rw_yang_pb_schema_t* schema,
                               rw_keyspec_instance_t* instance);

/*!
 * Get a displayable string representation of the keyspec entry path.
 * The caller owns the allocated string.   When accessed through 
 * GI, the managed language will free the buffer when  the last 
*  reference goes away.   
 * C users must be careful to free  the allocated string after use.
 * 
 * @param keyspec The keyspec
 * @param instance Keyspsec instance if present.
 * @param schema  The  schema to use to convert the keypsec to string
 */
/// @cond GI_SCANNER
/**
 * rw_keyspec_path_create_string:
 * @keyspec:
 * @instance:(nullable)(optional)
 * @schema: (nullable) (optional)
 * Returns: (transfer full) (nullable)
 */
/// @endcond
char* 
rw_keyspec_path_create_string(const rw_keyspec_path_t*   keyspec,
                              const rw_yang_pb_schema_t* schema,
                              rw_keyspec_instance_t*     instance);

/*!
 * Given the xpath string, get the ProtobufCMessageDescriptor
 * for the corresponding pbcm.
 * 
 * @param xpath The xpath string
 * @param schema  The  schema to use to get descriptor for the message.
 * @param instance Keyspsec instance pass NULL to use the default.
 */
/// @cond GI_SCANNER
/**
 * rw_keyspec_get_pbcmd_from_xpath:
 * @xpath:
 * @schema: (type RwYangPb.Schema)
 * @instance:(nullable)(optional)
 * Returns: (type ProtobufC.MessageDescriptor)
 */
/// @endcond
const ProtobufCMessageDescriptor*
rw_keyspec_get_pbcmd_from_xpath(const char* xpath,
                                const rw_yang_pb_schema_t* schema,
                                rw_keyspec_instance_t* instance);


/*!
 * Given the pathEntry, get the minikey
 * 
 * @param path_entry Path-entry
 * @param mk  Corresponding generated minikey.
 */
/// @cond GI_SCANNER
/**
 * rw_schema_minikey_get_from_path_entry:
 * @path_entry: (type RwKeyspec.Entry)
 * @mk: (out) (transfer full)
 * Returns: (type RwTypes.RwStatus) (transfer none)
 */
/// @endcond
rw_status_t
rw_schema_minikey_get_from_path_entry(const rw_keyspec_entry_t *path_entry,
                                      rw_schema_minikey_t **mk);

__END_DECLS


#endif //__RW_SCHEMA_GI_H__

