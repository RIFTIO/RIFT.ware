
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



/*!
 * @file rw_yang_pb.h
 * @date 2015/09/28
 * @brief RIFT.ware yang/protobuf integration.
 */

#ifndef RW_YANG_PB_H_
#define RW_YANG_PB_H_

#include <sys/cdefs.h>
#include <rw_pb.h>
#include <yangmodel.h>
#include <protobuf-c/rift-protobuf-c.h>


struct rw_keyspec_instance_t;

// Need this because doxygen parser thinks this is a C file and misses the C++ definitions
#ifdef RW_DOXYGEN_PARSING
#define __cplusplus 201103
#endif

#if __cplusplus
#if __cplusplus < 201103L
#error "Requires C++11"
#endif
#endif

__BEGIN_DECLS
#ifndef __GI_SCANNER__


/*!
 * The rw_pb_to_cli will take proto buf and convert to a string with CLI
 * commands using the yangmodel. The fields are output in the yang order, and
 * not proto buf order. They key name is output first irrespective of the order
 * in which it appears in the proto buf.
 *
 * @param yang_model - The Yang Model
 * @param pbcmsg - The proto buf message
 *
 * @return
 *   the string
 */
char* rw_pb_to_cli(
  rw_yang_model_t* yang_model,
  const ProtobufCMessage *pbcmsg);

/*!
 * Given a schema descriptor of any kind, find the schema descriptor
 * for the protobuf message type.
 *
 * @return The protobuf message schema descriptor, if found.
 */
const rw_yang_pb_msgdesc_t* rw_yang_pb_msgdesc_get_msg_msgdesc(
  /*!
   * [in] in_msgdesc - Any related schema descriptor.  Cannot be NULL.
   */
  const rw_yang_pb_msgdesc_t* in_msgdesc);

/*!
 * Given a schema descriptor of any kind, find the schema descriptor
 * for the protobuf path spec type.
 *
 * @return The protobuf path spec schema descriptor, if found.
 */
const rw_yang_pb_msgdesc_t* rw_yang_pb_msgdesc_get_path_msgdesc(
  /*!
   * [in] in_msgdesc - Any related schema descriptor.  Cannot be NULL.
   */
  const rw_yang_pb_msgdesc_t* in_msgdesc);

/*!
 * Given a schema descriptor of any kind, find the schema descriptor
 * for the protobuf path entry type.
 *
 * @return The protobuf path entry schema descriptor, if found.
 */
const rw_yang_pb_msgdesc_t* rw_yang_pb_msgdesc_get_entry_msgdesc(
  /*!
   * [in] in_msgdesc - Any related schema descriptor.  Cannot be NULL.
   */
  const rw_yang_pb_msgdesc_t* in_msgdesc);

/*!
 * Given a concrete message of any type, or a generic path spec (ATTN:
 * or, in the future, any message of any kind that contains embedded,
 * rooted schema context), find the schema descriptor for the protobuf
 * message type.  This API will fail to find a descriptor if:
 *  - The message is not derived form yang
 *  - The message is a generic path entry, because they are not
 *    globally unique.  (ATTN: However, it is possible to identify
 *    top-level nodes and group deifnitions?).
 *  - The message is a generic dom path.  (ATTN: We could, because it
 *    could be converted to path spec, but the function doesn't do that
 *    because there should be no users of plain dom path, generic or
 *    not.
 *  - The message is a generic ProtobufCMessage, even if yang derived.
 *
 * @return The protobuf message schema descriptor, if found.
 */
const rw_yang_pb_msgdesc_t* rw_schema_pbcm_get_msg_msgdesc(
  /*!
   *[in] The keyspec instance pointer.
   */
  struct rw_keyspec_instance_t* instance,
  /*!
   * [in] The message you have.  If it is a generic path spec, it may
   * be modified, in order to populate the dom path.
   */
  const ProtobufCMessage* msg,
  /*!
   * [in] The schema to use for a lookup, if msg contains generic
   * schema that needs to be walked.  If msg contains concrete schema,
   * then this argument will be unused and the returned descriptor
   * might come from another schema!
   */
  const rw_yang_pb_schema_t* schema);

/*!
 * Given a concrete message of any type, or a generic path spec (ATTN:
 * or, in the future, any message of any kind that contains embedded,
 * rooted schema context), find the schema descriptor for the protobuf
 * path type.  This API will fail to find a descriptor if:
 *  - The message is not derived form yang
 *  - The message is a generic path entry, because they are not
 *    globally unique.  (ATTN: However, it is possible to identify
 *    top-level nodes and group deifnitions?).
 *  - The message is a generic dom path.  (ATTN: We could, because it
 *    could be converted to path spec, but the function doesn't do that
 *    because there should be no users of plain dom path, generic or
 *    not.
 *  - The message is a generic ProtobufCMessage, even if yang derived.
 *
 * @return The protobuf path schema descriptor, if found.
 */
const rw_yang_pb_msgdesc_t* rw_schema_pbcm_get_path_msgdesc(
  /*!
   * [in] The keyspec instance pointer.
   */
  struct rw_keyspec_instance_t* instance,
  /*!
   * [in] The message you have.  If it is a generic path spec, it may
   * be modified, in order to populate the dom path.
   */
  const ProtobufCMessage* msg,
  /*!
   * [in] The schema to use for a lookup, if msg contains generic
   * schema that needs to be walked.  If msg contains concrete schema,
   * then this argument will be unused and the returned descriptor
   * might come from another schema!
   */
  const rw_yang_pb_schema_t* schema);

/*!
 * Given a concrete message of any type, or a generic path spec (ATTN:
 * or, in the future, any message of any kind that contains embedded,
 * rooted schema context), find the schema descriptor for the protobuf
 * entry type.  This API will fail to find a descriptor if:
 *  - The message is not derived form yang
 *  - The message is a generic path entry, because they are not
 *    globally unique.  (ATTN: However, it is possible to identify
 *    top-level nodes and group deifnitions?).
 *  - The message is a generic dom path.  (ATTN: We could, because it
 *    could be converted to path spec, but the function doesn't do that
 *    because there should be no users of plain dom path, generic or
 *    not.
 *  - The message is a generic ProtobufCMessage, even if yang derived.
 *
 * @return The protobuf entry schema descriptor, if found.
 */
const rw_yang_pb_msgdesc_t* rw_schema_pbcm_get_entry_msgdesc(
  /*!
   * [in] The keyspec instance pointer
   */
  struct rw_keyspec_instance_t* instance,
  /*!
   * [in] The message you have.  If it is a generic path spec, it may
   * be modified, in order to populate the dom path.
   */
  const ProtobufCMessage* msg,
  /*!
   * [in] The schema to use for a lookup, if msg contains generic
   * schema that needs to be walked.  If msg contains concrete schema,
   * then this argument will be unused and the returned descriptor
   * might come from another schema!
   */
  const rw_yang_pb_schema_t* schema);

const rw_yang_pb_msgdesc_t* rw_yang_pb_get_module_root_msgdesc(
    const rw_yang_pb_msgdesc_t* in_msgdesc);

#endif /* ndef __GI_SCANNER__ */
__END_DECLS


#endif // RW_YANG_PB_H_
