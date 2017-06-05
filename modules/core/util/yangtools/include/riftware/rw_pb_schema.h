/* STANDARD_RIFT_IO_COPYRIGHT */

#include "rw_keyspec.h"

/*!
 * @file rw_schema_merge.h
 * @author Sujithra Periasamy
 * @date 2014/08/29
 * @brief Dynamic PB Schema Merge
 */
#ifndef __RW_SCHEMA_MERGE_H__
#define __RW_SCHEMA_MERGE_H__

__BEGIN_DECLS

/*!
 *  Merge two protobuf schema's to create a single unified schema.
 *  If one of the two input schema is a superset of the other, just return
 *  the superset schema. Otherwise create a unified schema that is the 
 *  result of the merge of both the input schemas.
 *
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] schema1 The input schema1. Can be compile time/runtime generated.
 * @param[in] schema2 The input schema2, can be compile tine/runtime generated.
 *
 * @return The unified schema which is the merge of schema1 and schema2.
 */
const rw_yang_pb_schema_t* rw_schema_merge(rw_keyspec_instance_t* instance,
                                           const rw_yang_pb_schema_t* schema1,
                                           const rw_yang_pb_schema_t* schema2);

/*!
 * API to print the dynamic schema to the stdout. Used only in the
 * testcase.
 *
 * @param[in] schema The schema to print.
 */
void rw_schema_print_dynamic_schema(const rw_yang_pb_schema_t* schema);

/*!
 * API to free the dynamic schema. Please dont call this API from the
 * app code. Currently called only from test case.
 *
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the
 * default.
 * @param[in] schema The dynamic schema pointer to free.
 */
void rw_schema_free(rw_keyspec_instance_t* instance,
                    rw_yang_pb_schema_t* schema);


/*!
 * Find the top node in the schema, given the namespace id and the
 * element tag.
 *
 * @param[in] schema The yang pbc schema. Must be valid.
 * @param[in] system_nsid The namespace id of the node.
 * @param[in] element_tag The tag value of the node.
 *
 * @return The yangpbc msgdesc for the node.
 */
const rw_yang_pb_msgdesc_t*
rw_yang_pb_schema_get_ypbc_mdesc(const rw_yang_pb_schema_t *schema,
                                 uint32_t system_nsid,
                                 uint32_t element_tag);

__END_DECLS

#ifdef __cplusplus

struct UniquePtrRwYangPbSchema
{
  rw_keyspec_instance_t* instance_ = nullptr;

  UniquePtrRwYangPbSchema(rw_keyspec_instance_t* instance = nullptr)
  : instance_(instance)
  {}

  void operator()(rw_yang_pb_schema_t* schema) const
  {
    rw_schema_free (instance_, schema);
  }

  typedef std::unique_ptr<rw_yang_pb_schema_t, UniquePtrRwYangPbSchema> uptr_t;
};

#endif


#endif // __RW_SCHEMA_MERGE_H__
