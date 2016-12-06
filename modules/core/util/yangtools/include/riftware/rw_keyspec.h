
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
 * @file rw_keyspec.h
 * @author Tom Seidenberg
 * @date 2014/08/29
 * @brief RiftWare keyspecs - virtual DOM schema path identifiers
 */

#ifndef RW_KEYSPEC_H_
#define RW_KEYSPEC_H_

#include <sys/cdefs.h>
#include <protobuf-c/rift-protobuf-c.h>
#include <stdbool.h>
#include <rwlib.h>
#include <yangmodel.h>
#include <rw_pb.h>
#include <rw_yang_pb.h>
#include <rw_protobuf_c.h>

#include <rw_schema.pb-c.h>
#include <rw_keyspec_gi.h>


// Need this because doxygen parser thinks this is a C file and misses the C++ definitions
#ifdef RW_DOXYGEN_PARSING
#define __cplusplus 201103
#endif

#if __cplusplus
#if __cplusplus < 201103L
#error "Requires C++11"
#endif
#endif

/*!
 * @defgroup RwKeyspec RiftWare virtual DOM schema path specifiers
 * @{
 */

__BEGIN_DECLS
#ifndef __GI_SCANNER__

// forward declarations
struct rw_yang_model_s;


/*
   ATTN: leftover notes from design. Delete me or implement:


   ATTN: If we can keep namespace numbering relative, then is it possible
   to apply simple offsets in app-specific schemas?
    - probably not: it would be hard to keep namespaces relative in a
      global sense?  Probably not if that global numbering includes all
      known namespaces, even unrelated ones.
    - However, is it possible to keep namespaces relative in a relative
      sense.  How would such a relative namespace counting strategy work?
       - For any given, schema-rooted, augmented node:
          - Sort the list of modules traversed in getting to the node, and
          module of the node's type.
    - Data structures:
        struct rw_ns_version_t {
          bool is_local; # 1 bit
          rw_ns_id_t ns;
          rw_ns_rev_t rev; # given that revs only occur on whole days, not many bits are needed (certainly 25 or less)
        }
        rw_ns_id_t ns_system_map[local_ns] => local_ns
        rw_ns_id_t ns_local_map{sys_ns} => system_ns
        generated_ns_state_struct {
          const rw_ns_id_t local_ns_map;
          atomic rw_ns_id_t system_ns_map;
        }


   In any given schema, there are a limited, enumerable, number of uses of
   any source grouping, possibly changing with yang file version.  All
   such uses can be uniquely identified with the following tuple:
     <Schema-uses-statement-index, Schema-namespace-index, Schema-date>
       - Ultimate-uses-statement-index: This is an orginal numbering of
         the outer-most enclosing uses statement, as determined by a
         compiled yangpbc schema of a collection of yang modules, where
         index 1 is defined as the un-augmented un-refined definition, and
         all augmented and/or refined instances have their own index
         values.  Code that uses the grouping  ATTN

   What happens when someone violates the restrictions and delete/adds
   keys, or illegally reorganizes the yang structure?
    - need to be able to remap from one module into another?
    - need to be able to translate fields from one type to another?


   Additions to a schema due to revisions are more-or-less equivalent to
   augments.  But deletions and attribute changes are not - they can have
   serious consequences.  Therefore, the yangpbc compiler needs to compare
   differences between schemas and react accordingly.  With annotations,
   it may be possible to define translations.

   Without annotations, the best thing we can do is require that the
   previous version be avilable for inspection, so that yangpbc can detect
   the changes and blow up?  What about detecting changes against all
   revisions?

   Can each new revision obtain a unique starting tag number?
   Programmatically, or by annotation?


   App understanding rooted keyspec
   App understanding relative keyspec
   App registering for rooted keyspec
   App registering for relative keyspec

   When app's registration is relative to a grouping, DTS needs to convert
   from rooted path to grouping path, and vice-versa, at member API
   boundary
    - in theory, a complete, rooted, keyspec can be walked, relatively
      efficiently, by generated code, into something the app can
      understand.  The app can get the fully rooted path, or the
      grouping-relative path, at its option.



   ATTN: Augments could be added into protoc-c structs with minimal extra
   syntax at point of use, if the augment struct's fields are included
   inline in the parent struct, including base pointer, after all normal
   fields.

     struct base {
       PBCMD* base_pbcmd;
       type_t base_field_1;
       type_t base_field_2;
       # more base fields

       # now revision rev_2... #
       bool has_rev_2;
       PBCMD* rev_2_pbcmd;
       type_t rev_2_field_1;
       # ...more rev_2 fields

       #  now augment aug_a... #
       bool has_aug_a;
       PBCMD* aug_a_pbcmd;
       aug_ns_t* aug_a_ns_data; # not intended to be used by app code
       type_t aug_a_field_1;
       # ... more aug_a fields

       #  now augment aug_b... #
       #
          ATTN: Ideally, the has_augment_b is really spread across the
          various augment fields...  The app doesn't want to care about
          augment_b; it just wants to care about the fields.  However, it
          would "just work" is the augment's has is simply always true.
       #
       bool has_aug_b;
       PBCMD* aug_b_bpbcmd;
       aug_ns_t* aug_b_ns_data; # not intended to be used by app code
       type_t aug_b_bfield_1;
       # ... more aug_b fields
     };


   ATTN: If I'm an app coded in terms of a grouping, and some module in my
   schema modifies that grouping, can I get notificatins in terms of the
   modification?  The app will have two mutually-distinct struct
   definitions; the registration must occur with the context of the
   desired structure format.  That registration can occur for the
   augmented case only with a rooted registration, or enclosing
   grouping-uses registration.


   yang char mappings:
    -[_-.]    => _dashX
    -X        => _dash_X
    .X        => _dot_X
    _[^1ydu]  => _1_X
    _d        => _1_X
    __X  => __2_X
    ___X => __3_X
    ...


   ATTN: Another possible way to uniquely identify all fields in a message
   is to assign each field a module-unique tag number, that it always
   uses.  Perhaps assigned by a total file ordering of all identifiers.
    - global by file
    - global by level
    - globally numbered by complex object, then offset by field
       - each augment, container, list, grouping, choice, rpc-io, and
         notification, gets its own complex-object number, and each field
         within the complex object is offset form the object.
       - By default, segregate the space into categories, so that fewer
         renumberings are required.


   2^29 tags:
    - 256M reserved for future use
    - reserve the first M for ns (and protobuf reserved)
    - reserve the second M for extensions
    - reserve 4M for escapes
    - leaves 250 M for splitting the ranges
       - 64 depth, at 2 M ids per depth, in encounter order
         (this schema would break if a revision pushes levels, but doesn't
         actually change the XML mapping
       - 100 container/list depth, at 1 M ids per depth, in encounter order
         (this schema would break if a revision pushes levels, but doesn't
         actually change the XML mapping


   ATTN: key access APIs
   ATTN: key modification APIs
    - change keys
    - clear/wildcard keys
    - snprintf
 */


__END_DECLS

__BEGIN_DECLS

#define RW_SCHEMA_PB_MAX_PATH_ENTRIES (RW_SCHEMA_TAG_PATH_ENTRY_END - RW_SCHEMA_TAG_PATH_ENTRY_START + 1)
#define RW_SCHEMA_PB_MAX_KEY_VALUES (RW_SCHEMA_TAG_ELEMENT_KEY_END - RW_SCHEMA_TAG_ELEMENT_KEY_START + 1)

typedef struct rw_keyspec_instance_t rw_keyspec_instance_t;
typedef struct rw_keyspec_stats_t rw_keyspec_stats_t;

struct rw_keyspec_stats_t {
  struct {
    uint64_t total_errors;
  } error;

  struct {
    struct {
      uint64_t create_dup;
      uint64_t create_dup_type;
      uint64_t create_dup_type_trunc;
      uint64_t find_spec_ks;
      uint64_t get_binpath;
      uint64_t set_category;
      uint64_t has_entry;
      uint64_t is_equal;
      uint64_t is_equal_detail;
      uint64_t is_match_detail;
      uint64_t is_path_detail;
      uint64_t is_branch_detail;
      uint64_t append_entry;
      uint64_t trunc_suffix;
      uint64_t pack_dompath;
      uint64_t create_with_binpath;
      uint64_t create_with_dompath;
      uint64_t free;
      uint64_t find_mdesc;
      uint64_t is_sub_ks;
      uint64_t reroot;
      uint64_t reroot_iter;
      uint64_t reroot_and_merge;
      uint64_t reroot_and_merge_op;
      uint64_t add_keys;
      uint64_t delete_proto_field;
      uint64_t get_print_buff;
      uint64_t from_xpath;
      uint64_t matches_msg;
    } path;
    struct {
      uint64_t create_from_proto;
      uint64_t create_dup;
      uint64_t create_dup_type;
      uint64_t pack;
      uint64_t unpack;
    } entry;
  } fcall;
};

struct rw_keyspec_instance_t {
  ProtobufCInstance* pbc_instance;
  rw_yang_model_t* ymodel;
  void* ylib_data;
  void* user_data;
  void (*error)(rw_keyspec_instance_t*, const char* msg);
  rw_keyspec_stats_t stats;
};

/*!
 * Get the pointer to the default keyspec instance.
 *
 * @return pointer to the default global keyspec instance.
 */

rw_keyspec_instance_t* rw_keyspec_instance_default();


/*!
 * Set the protobuf-instance pointer in the keyspec instance.
 *
 * @param[in] ks_inst The keyspec instance pointer.
 * @param[in] pbc_inst The new protobuf instance pointer to set in the
 * instance.
 *
 * @return The old protobuf instance pointer already set in the
 * keyspec instance. NULL if none is set earlier.
 */
ProtobufCInstance* rw_keyspec_instance_set_pbc_instance(rw_keyspec_instance_t* ks_inst,
                                                        ProtobufCInstance* pbc_inst);

/*!
 * Set the yangmodel pointer in the keyspec instance.
 *
 * @param[in] ks_inst The keyspec instance pointer.
 * @param[in] ymodel The new yangmodel pointer to set in the instance.
 *
 * @return The old yangmodel pointer already set in the
 * keyspec instance. NULL if none is set earlier.
 */
rw_yang_model_t* rw_keyspec_instance_set_yang_model(rw_keyspec_instance_t* ks_inst,
                                                    rw_yang_model_t* ymodel);

/*!
 * Set the userdata pointer in the keyspec instance.
 *
 * @param[in] ks_inst The keyspec instance pointer.
 * @param[in] user_data The new userdata pointer to set in the instance.
 *
 * @return The old userdata pointer already set in the
 * keyspec instance. NULL if none is set earlier.
 */
void* rw_keyspec_instance_set_user_data(rw_keyspec_instance_t* ks_inst,
                                        void* user_data);

/*!
 * Get the userdata pointer set in the keyspec instance.
 *
 * @param[in] ks_inst The  keyspec instance pointer.
 *
 * @return The userdata pointer present in the keyspec instance.
 */
void* rw_keyspec_instance_get_user_data(rw_keyspec_instance_t* ks_inst);

/*!
 * Duplicate input to output, if possible, allocating a new keyspec in
 * the process.  Upon success, rw_keyspec_path_is_equal(input,output)
 * will be true.
 *
 * @param[in] input The keyspec to duplicate. Will not be modified.
 * Must be valid
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[out] output Will be set to the newly created duplicate. The duplicate
 * will end up with the same dynamic type as input (if you need to
 * end up with a generic or different specific type, then use copy
 * instrad of dup).  Target pointer must be NULL upon input; this
 * forces the caller to properly dispose of any previous value,
 * because this function cannot do that correctly in all cases.  If
 * the function fails, the value will be undefined, but will not need
 * to be freed.
 *
 * @return RW_STATUS_FAILURE if the keyspec cannot be duplicated. This
 * could happen if output is a specific type and flat.
 */
rw_status_t rw_keyspec_path_create_dup(const rw_keyspec_path_t* input,
                                       rw_keyspec_instance_t* instance,
                                       rw_keyspec_path_t** output);

/*!
 * Duplicate input to output, if possible, allocating a new keyspec in
 * the process. The output keyspec will be set to the given category.
 *
 * @param[in] input The keyspec to duplicate. Will not be modified.
 * Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[out] output Will be set to the newly created duplicate. The duplicate
 * will end up with the same dynamic type as input (if you need to
 * end up with a generic or different specific type, then use copy
 * instrad of dup).  Target pointer must be NULL upon input; this
 * forces the caller to properly dispose of any previous value,
 * because this function cannot do that correctly in all cases.  If
 * the function fails, the value will be undefined, but will not need
 * to be freed.
 * @param[in] category The output keyspec will be set to
 * the specified category.
 *
 * @return RW_STATUS_FAILURE if the keyspec cannot be duplicated.  This
 *   could happen if output is a specific type and flat.
 */
rw_status_t rw_keyspec_path_create_dup_category(const rw_keyspec_path_t* input,
                                                rw_keyspec_instance_t* instance,
                                                rw_keyspec_path_t** output,
                                                RwSchemaCategory category);

/*!
 * Duplicate input to output of specified type, if possible, allocating
 * a new keyspec in the process.  Upon success,
 * rw_keyspec_path_is_equal(input,output) will be true.
 *
 * @param[in] input The keyspec to duplicate. Will not be modified.
 *   Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] out_pbcmd The type to duplicate to. Will not be modified.
 *   Must be valid.
 *
 * @return The new keypsec of the input type.  NULL if the keyspec
 *   cannot be duplicated to the specified type.  This could happen if
 *   output is a specific type and flat.
 */
rw_keyspec_path_t* rw_keyspec_path_create_dup_of_type(const rw_keyspec_path_t* input,
                                                      rw_keyspec_instance_t* instance,
                                                      const ProtobufCMessageDescriptor* out_pbcmd);

/*!
 * Duplicate input to output of specified type, if possible, allocating
 * a new keyspec in the process.  Discard all unknown fields and
 * generic path elements - leaving only fields understood in the target
 * concrete type.  Upon success, rw_keyspec_path_is_equal(input,output) MAY
 * NOT be true!
 *
 * @param[in] input The keyspec to duplicate. Will not be modified.
 *   Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] out_pbcmd The type to duplicate to. Will not be modified.
 *   Must be valid.
 *
 * @return The new keypsec of the input type.  NULL if the keyspec
 *   cannot be duplicated to the specified type.  This could happen if
 *   output is a specific type and flat.
 */
rw_keyspec_path_t* rw_keyspec_path_create_dup_of_type_trunc(const rw_keyspec_path_t* input,
                                                            rw_keyspec_instance_t* instance,
                                                            const ProtobufCMessageDescriptor* out_pbcmd);

/*!
 * Copy input to output, if possible.  Upon success,
 * rw_keyspec_path_is_equal(input,output) will be true.
 *
 * @param[in] input The keyspec to copy. Will not be modified.
 * Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in/out] output The keyspec to initialize. The original value will be
 * overwritten, if there is one. If the old value contains pointers,
 * they will leak! If the copy fails, contents are undefined (but no
 * additional frees will be required).
 *
 * @return RW_STATUS_FAILURE if the keyspec cannot be copied.  This
 *   could happen if output is a specific type and flat.
 */
rw_status_t rw_keyspec_path_copy(const rw_keyspec_path_t* input,
                                 rw_keyspec_instance_t* instance,
                                 rw_keyspec_path_t* output);


/*!
 * Copy input to a new message type, if possible.  Upon success,
 * rw_keyspec_path_is_equal(input,return value) will be true.
 *
 * @param[in] input The keyspec to copy. Will not be modified.
 * Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 *
 * @param[in] out_pbcmd The keyspec type to initialize to.
 * The message type must be tag-compatible with RwSchemaPathSpec.
 * If it is not tag-compatible, the function might (or might not) assert.
 *
 * @return NULL if the keyspec cannot be copied.  This could happen if
 *   output is a specific type and flat.
 */
rw_keyspec_path_t* rw_keyspec_path_copy_to_type(const rw_keyspec_path_t* input,
                                                rw_keyspec_instance_t* instance,
                                                const ProtobufCMessageDescriptor* out_pbcmd);


/*!
 * Move input to output, if possible.  Upon success, output will be
 * equivalent to what input used to be.
 *
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] input The keyspec to move from. Must be valid. Regardless of
 * success, contents are undefined upon return (but no additional
 * frees will be required, except for the pointer itself, if it was
 * dynamically allocated [which only the caller knows]).
 *
 * @param[in/out] output The keyspec to move to. The original value will be
 * overwritten, if there is one.  If the old value contains pointers,
 * they will leak!  If the copy fails, contents are undefined (but no
 * additional frees will be required).
 *
 * @return RW_STATUS_FAILURE if the keyspec cannot be moved.  This
 *   could happen if output is a specific type and flat.
 */
rw_status_t rw_keyspec_path_move(rw_keyspec_instance_t* instance,
                                 rw_keyspec_path_t* input,
                                 rw_keyspec_path_t* output);

/*!
 * Swap keyspecs a and b, if possible.  Upon success, keyspecs will be
 * swapped.
 *
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in/out] a Must be valid. The original value will be freed, if
 * there is one. If the swap fails, contents are undefined (but no
 * additional frees will be required).
 * @param[in/out] b Must be valid. The original value will be freed, if
 * there is one. If the swap fails, contents are undefined (but no
 * additional frees will be required).
 *
 * @return RW_STATUS_FAILURE if the keyspecs cannot be swapped.  This
 *   could happen if either a or b are a specific keyspec type and are
 *   flat.
 */
rw_status_t rw_keyspec_path_swap(rw_keyspec_instance_t* instance,
                                 rw_keyspec_path_t* a,
                                 rw_keyspec_path_t* b);

/*!
 * Update binpath from dompath, and save the result in the keyspec.
 *
 * @param[in/out] k The keyspec to update. Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 *
 * @return RW_STATUS_FAILURE if binpath cannot be updated.  Will not
 *   fail because of an empty path.
 */
rw_status_t rw_keyspec_path_update_binpath(rw_keyspec_path_t* k,
                                           rw_keyspec_instance_t* instance);

/*!
 * Delete the dompath in the keyspec.
 * @param[in/out] k The keyspec. Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 *
 * @return RW_STATUS_FAILURE if binpath cannot be deleted.
 *         RW_STATUS_SUCCESS on success.
 */
rw_status_t rw_keyspec_path_delete_binpath(rw_keyspec_path_t* k,
                                           rw_keyspec_instance_t* instance);

/*!
 * Update dompath from binpath, and save the result in the keyspec.
 *
 * @param[in/out] k The keyspec to update. Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 *
 * @return RW_STATUS_FAILURE if dompath cannot be updated.  This could
 *   happen if k is a specific keyspec type and is flat.
 */
rw_status_t rw_keyspec_path_update_dompath(rw_keyspec_path_t* k,
                                           rw_keyspec_instance_t* instance);

/*!
 * Delete the dompath in the keyspec
 *
 * @param[in/out] k The keyspec, must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 *
 * @return RW_STATUS_FAILURE if dompath cannot be deleted.
 *         RW_STATUS_SUCCESS on success.
 */
rw_status_t rw_keyspec_path_delete_dompath(rw_keyspec_path_t* k,
                                           rw_keyspec_instance_t* instance);

/*!
 * Get the keyspec string path. If not present,
 * the function internally updates the strpath.
 *
 * @param[in] k The keyspec. Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] schema Optional schema pointer
 * @param[out] str_path Pointer to the keyspec string path. The caller should
 * not should not free the pointer. It points to the keyspec strpath.
 *
 * @return:
 *    RW_STATUS_SUCCESS: If the function was able
 *    to get the strpath of the keyspec successfully.
 *    RW_STATUS_FAILURE: If there was an error.
 */
rw_status_t rw_keyspec_path_get_strpath(rw_keyspec_path_t* k,
                                        rw_keyspec_instance_t* instance,
                                        const rw_yang_pb_schema_t* schema,
                                        const char** str_path);

/*!
 * Update the string path in the keyspec. The schema pointer is optional.
 * The old strpath if present will be deleted and a new strpath is
 * created based on the current contents of keyspec. If the keyspec is changed
 * outside of the schema APIs, then keyspec user must call update strpath
 * to keep the strpath updated with the current keyspec contents.
 *
 * @param[in] k The keyspec. Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] schema Optional schema pointer to use for generating a strpath.
 *
 * @return:
 *   RW_STATUS_SUCCESS: If the function was
 *   able to update the keyspec successfully.
 *   RW_STATUS_FAILURE: If there was an error.
 */

rw_status_t rw_keyspec_path_update_strpath(rw_keyspec_path_t* k,
                                           rw_keyspec_instance_t* instance,
                                           const rw_yang_pb_schema_t* schema);


/*!
 * Obtain a reference to binpath for the keyspec.  If necessary,
 * creates binpath from dompath.
 *
 * @param[in] k The keyspec to convert. Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[out] buf Will be set to the start of the binpath buffer. Undefined
 * if the function fails.
 * @param[out] len Will be set to the length of the binpath buffer. Undefined
 * if the function fails.
 *
 * @return RW_STATUS_FAILURE if binpath cannot be updated.  Will not
 *   fail because of an empty path.
 */
rw_status_t rw_keyspec_path_get_binpath(rw_keyspec_path_t* k,
                                        rw_keyspec_instance_t* instance,
                                        const uint8_t** buf,
                                        size_t* len);

/*!
 * Get the number of levels in the keyspec.
 *
 * @param[in] keyspec The keyspec to check. Must be valid.
 */
size_t rw_keyspec_path_get_depth(const rw_keyspec_path_t* keyspec);

/*!
 * Get the keyspec category (config/data/rpc, et cetera).
 * @param[in] keyspec The keyspec to check. Must be valid.
 */
RwSchemaCategory rw_keyspec_path_get_category(const rw_keyspec_path_t* keyspec);

/*!
 * Set the keyspec category (config/data/rpc, et cetera).
 *
 * @param[in/out] keyspec The keyspec to check. Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] category The category to change to. Must be valid.
 */
void rw_keyspec_path_set_category(rw_keyspec_path_t* keyspec,
                                  rw_keyspec_instance_t* instance,
                                  RwSchemaCategory category);

/*!
 * Get a pointer to the path entry at a given index.
 *
 * @param[in/out] keyspec The keyspec to inspect. Must be valid.
 * @param[in] entry_index The path entry index to get.
 *
 * @return NULL if entry_index is larger than the path length.  Dynamic
 *   type might not be RwSchemaPathEntry; it might be some other type
 *   that is tag-compatible.
 */
const rw_keyspec_entry_t* rw_keyspec_path_get_path_entry(const rw_keyspec_path_t* keyspec,
                                                         size_t entry_index);


/*!
 * Get the type of primary key to the path entry
 *
 * @param[in] path_entry The path entry input.
 *
 * @return ProtobufCType  of the primary key of the input path entry
 */
ProtobufCType rw_keyspec_entry_get_ctype(const rw_keyspec_entry_t *path_entry);


/*
 * A structure that allows convenient return of the tag and name space id of a
 * path element
 */
typedef struct rw_schema_ns_and_tag_s {
  uint32_t ns;
  uint32_t tag;
} rw_schema_ns_and_tag_t;

/*!
 * Get the tag id and namespace of  of this path entry
 * @param[in] path_entry The path entry to inspect
 *
 * @return the tag id and namespace of this path
 */

rw_schema_ns_and_tag_t rw_keyspec_entry_get_schema_id(
    const rw_keyspec_entry_t *path_entry);


/*!
 * Determine if a keyspec is rooted.
 * @param[in] keyspec The keyspec to inspect. Must be valid.
 */
bool rw_keyspec_path_is_rooted(const rw_keyspec_path_t* keyspec);

/*!
 * Determine if a keyspec is a module root ks.
 * @param[in] ks The keyspec to inspect. Must be valid.
 */
bool rw_keyspec_path_is_module_root(
    const rw_keyspec_path_t* ks);

/*!
 * Determine if a keyspec is a root ks.
 * @param[in] ks The keyspec to inspect. Must be valid.
 */
bool rw_keyspec_path_is_root(
    const rw_keyspec_path_t* ks);

/*!
 * Determine if k has a particular path entry at the tip.  If it does,
 * optionally determine the path entry index of the entry, and whether
 * the keys also match.
 *
 * @param[in/out] k The keyspec to inspect. Must be valid and concrete
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] entry The entry to look for. Must be valid.
 * @param[out] entry_index Undefined if the entry was not found. Will be set to
 * the path entry index into k of the entry.  May be NULL if the caller
 * does not want this information.
 * @param[out] key_index Undefined if the entry was not found. Will be set to 1
 * if the entry is found and all the keys match.  Will be set to the
 * 0-based index of the first mismatched key if the entry is found,
 * but has mismatched keys.  Keys left unspecified, or specified only
 * in the keyspec or only in entry are considered matching.
 * Therefore, mismatches can only occur if a key is specified in
 * both, and the values do not match.  May be NULL if the caller does
 * not want this information.
 *
 * O(len) or O(len*keys) operation, as determined by arguments.
 */
bool rw_keyspec_path_is_entry_at_tip(const rw_keyspec_path_t* k,
                                     rw_keyspec_instance_t* instance,
                               const rw_keyspec_entry_t* entry,
                               size_t* entry_index,
                               int* key_index);


/*!
 * Determine if k contains a particular path element.
 *
 * @param[in/out] k The keyspec to inspect. Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] element The element to look for. Must be valid.
 * @param[out] element_index Undefined if the element was not found. Will be
 * set to the path entry index into k of the element. May be NULL if
 * the caller does not want this information.
 *
 * O(len) operation.
 */
bool rw_keyspec_path_has_element(const rw_keyspec_path_t* k,
                                 rw_keyspec_instance_t* instance,
                                 const RwSchemaElementId* element,
                                 size_t* element_index);

/*!
 * Find the number of keys in a path spec.
 *
 * @return the number of keys - 0 for containers
 *
 * @param [in] path_entry the path entry for the operation
 */

size_t rw_keyspec_entry_num_keys(const rw_keyspec_entry_t *path_entry);

/*!
 * Determine if a keyspec path entry has a key at index key_index.
 * @param[in] entry The path entry to inspect
 * @param[in] key_index the index for the key. An index of 0 will find KEY00 etc
 *
 * @return true if found, false otherwise
 */

bool rw_keyspec_entry_has_key(const rw_keyspec_entry_t* entry,
                              int key_index);

/*!
 * Find the key at a specified index in a keyspec
 * @param[in] entry The path entry to inspect
 * @param[in] key_index The index for the key. An index of 0 will find KEY00 etc
 * @param[out] value The value of the key as a ProtobufCFieldInfo type
 * @return RW_STATUS_SUCCESS if the key is present with a valid value, status
 *  otherwise
 */

rw_status_t rw_keyspec_entry_get_key_value(const rw_keyspec_entry_t* entry,
                                           int key_index,
                                           ProtobufCFieldInfo *value);


/*!
 * Find if there is a key present at or after the index. The index is
 * updated to the found index if the status is success.
 * @param[in] entry The path entry to inspect.
 * @param[in/out] the index for the key.  Updated to the first found key
 * An index of 0 will find KEY00 and later etc
 *
 * @return RW_STATUS_SUCCESS if a key is present at this path at or after the
 * index specified, RW_STATUS_FAILURE otherwise
 */

rw_status_t  rw_keyspec_entry_get_key_at_or_after(const rw_keyspec_entry_t* entry,
                                                  size_t *key_index);

/*!
 * Determine if two keyspecs are equal.  Two keyspecs are equal if
 * their binpaths are the same length and all bytes match.
 *
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in/out] a Keyspec to compare. Must be valid.
 * @param[in/out] Keyspec to compare. Must be valid.
 *
 * O(len) operation.
 */
bool rw_keyspec_path_is_equal(rw_keyspec_instance_t* instance,
                              const rw_keyspec_path_t* a,
                              const rw_keyspec_path_t* b);


/*!
 * Determine if two keyspecs are equal.  This API is much slower than
 * plain equal if both keyspecs already have binpaths, because it must
 * interpret the keyspec.  Equality requires:
 *  - The paths have the same length.
 *  - The path entries name the same elements.
 *  - Any specified keys must be specified in both keyspecs, and be
 *    equal.
 *  - Any unknown fields in one keyspec must be specified the other
 *    keyspec (whether known or unknown).  Such keys must also be
 *    equal.
 *  - The specific protobuf message types do not need to match.
 *
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in/out] a Keyspec to compare. Must be valid.
 * @param[in/out b Keyspec to compare. Must be valid.
 * @param[out] entry_index If the keyspecs are not equal,
 * will be set to the 0-based
 * path entry index of the first unequal element.  May be NULL if the
 * caller does not want this information.  Will be 0 if the keyspecs
 * are not equal for reasons other than the path (rooted, category,
 * et cetera).
 * @param[out] key_index If the keyspecs are not equal, will be set to the
 * 0-based key index of the first unequal key.  Will be set to -1 if the
 * element id fails comparison rather than a key.  May be NULL if the
 * caller does not want this information.
 *
 * O(len*keys) operation.
 */
bool rw_keyspec_path_is_equal_detail(rw_keyspec_instance_t* instance,
                                     const rw_keyspec_path_t* a,
                                     const rw_keyspec_path_t* b,
                                     size_t* entry_index,
                                     int* key_index);


/*!
 * Determine if two keyspecs match.  Matching requires:
 *  - The paths have the same length.
 *  - The path entries name the same elements.
 *  - For keys specified in both keyspecs, the keys are equal.
 *  - Keys specified just one keyspec are considered to match by virtue
 *    of wildcarding.
 *  - For undecoded keys specified in both keyspecs, the keys are
 *    equal.
 *  - Undecoded keys specified in one keyspec are considered to match
 *    by virtue of wildcarding.
 *  - Non-key differences are ignored.
 *  - The specific protobuf message types do not need to match.
 *
 *  @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 *  @param[in/out] a Keyspec to compare.  Must be valid.
 *  @param[in/out] b Keyspec to compare.  Must be valid.
 *  @param[out] entry_index If the keyspecs do not match, will be set to the
 *  0-based path entry index of the first mismatched element.
 *  May be NULL if the caller does not want this information.
 *  @param[out] key_index If the keyspecs do not match, will be set to the
 *  0-based key index of the first mismatched key. Will be set to -1 if the
 *  element id fails comparison rather than a key. May be NULL if the
 *  caller does not want this information.
 *
 * O(len*keys) operation.
 */
bool rw_keyspec_path_is_match_detail(rw_keyspec_instance_t* instance,
                                     const rw_keyspec_path_t* a,
                                     const rw_keyspec_path_t* b,
                                     size_t* entry_index,
                                     int* key_index);


/*!
 * Determine if two keyspecs have the same path.
 *  - The paths have the same length.
 *  - The path entries name the same elements.
 *  - Other key and non-key differences are ignored.
 *  - The specific protobuf message types do not need to match.
 *
 *  @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 *  @param[in/out] a Keyspec to compare. Must be valid.
 *  @param[in/out] b Keyspec to compare. Must be valid.
 *  @param[out] entry_index If the keyspecs do not match, will be set to the
 *  0-based path entry index of the first mismatched element. May be NULL if
 *  the caller does not want this information.
 *
 * O(len*keys) operation.
 */
bool rw_keyspec_path_is_same_path_detail(rw_keyspec_instance_t* instance,
                                         const rw_keyspec_path_t* a,
                                         const rw_keyspec_path_t* b,
                                         size_t* entry_index);


/*!
 * Determine if suffix is an alternate branch of, or overlapping
 * extension to, prefix, by virtue of starting with one or more
 * overlapping path entries.  If there is an overlap, determine where
 * the overlap begins, and how long it is.  For an overlap to be found,
 * the first path entry in suffix must appear somewhere in prefix.
 * This function will not find an overlap that starts in the middle of
 * suffix.  This function will only find the first of multiple
 * overlaps.  This function is primarily intended for determining if
 * and how a relative keyspec relates to a rooted keyspec.
 *
 * O(len+len) operation.
 *
 * <pre><tt>
 *    a/b/c/d, c     => 2, 1
 *    a/b/c/d, c/d   => 2, 2
 *    a/b/c/d, c/e   => 2, 1
 *    a/b/c/d, c/d/e => 2, 2
 *    a/b/c/d, b/x/d => 1, 1 - only the b overlap can be detected
 *    a/b/c/d, z/a/b => false - cannot detect this, because suffix starts with z
 *    a/b/c/d, z     => false - no overlap
 * </tt></pre>
 *
 * @param[in/out] prefix Prefix keyspec. Must be valid.
 * @param[in/out] suffix Suffix keyspec. For an overlap to be found, the first
 * element of this keyspec must be in prefix. Must be valid.
 * @param[in] ignore_keys Do not consider the keys.
 * @param[out] start_index Will be set to the path entry index into prefix of
 * the start of the overlap. Undefined if there is no overlap. May be NULL if
 * the caller does not want this information.
 * @param[out] len Will be set to the length of the overlap. Undefined if
 * there is no overlap. May be NULL if the caller does not want this
 * information.
 */
bool rw_keyspec_path_is_branch_detail(rw_keyspec_instance_t* instance,
                                      const rw_keyspec_path_t* prefix,
                                      const rw_keyspec_path_t* suffix,
                                      bool ignore_keys,
                                      size_t* start_index,
                                      size_t* len);


/*!
 * Extend a keyspec by appending another path entry.
 *
 * @param[in/out] k Keyspec to extend. Must be valid. If binpath was set
 * prior to this call, it will be cleared.
 * @param[in] entry The entry to append to the keyepc. Will be copied. Must be
 * valid.
 *
 * @return RW_STATUS_FAILURE if dompath cannot be updated.  This could
 *   happen if k is a specific keyspec type and is flat.
 */
rw_status_t rw_keyspec_path_append_entry(rw_keyspec_path_t* k,
                                         rw_keyspec_instance_t* instance,
                                         const rw_keyspec_entry_t* entry);


/*!
 * Extend a keyspec with an array of entries.
 *
 * @paeam[in/out] k Keyspec to extend. Must be valid. If binpath was set prior
 * to this call, it will be cleared.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] entries The entry to append to the keyepc. Will be copied.
 * Must be valid.
 * @param[in] len The number of entries.
 * @return RW_STATUS_FAILURE if dompath cannot be updated.  This could
 *   happen if k is a specific keyspec type and is flat.
 */
rw_status_t rw_keyspec_path_append_entries(rw_keyspec_path_t* k,
                                           rw_keyspec_instance_t* instance,
                                           const rw_keyspec_entry_t* const* entries,
                                           size_t len);


/*!
 * Extend a keyspec with another keyspec.
 *
 * @param[in/out] k Keyspec to extend. Must be valid. If binpath was set prior
 * to this call, it will be cleared.
 * @param[in/out] other The other keyspec. Keys and elements will be copied.
 * Must be valid.
 *
 * @return RW_STATUS_FAILURE if dompath cannot be updated.  This could
 *   happen if k is a specific keyspec type and is flat.
 */
rw_status_t rw_keyspec_path_append_keyspec(rw_keyspec_path_t* k,
                                           rw_keyspec_instance_t* instance,
                                           const rw_keyspec_path_t* other);


/*!
 * Extend a keyspec and/or replace the suffix of a keyspec, with all or
 * part of a second keyspec.
 *
 * @param[in/out] Keyspec to extend. Must be valid. If binpath was set prior
 * to this call, it will be cleared.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in/out] other The other keyspec. Keys and elements will be copied.
 * Must be valid.
 * @param[in] k_index The 0-based path entry index into k where other will be
 * inserted. The entry at k_index will be replaced. All existing
 * entries from k_index, to the end, inclusive, will be deleted from
 * k. Use k's length or -1 to indicate insertion past the current
 * end of k. Values larger than length(k), or less than -1 will
 * produce an error.
 * ATTN: Define a constant that means at the end: e.g. _KEYSPEC_AT_END
 * @param[in] other_index The 0-based path entry index into other where the copying
 * will begin. Values outside of the path will produce an error.
 * @param[in] other_len The other keyspec. Keys and elements will be copied. Must
 * be valid.
 *
 * @return
 *  - RW_STATUS_FAILURE if dompath cannot be updated.  This could
 *    happen if k is a specific keyspec type and is flat.
 *  - RW_STATUS_OUT_OF_BOUNDS if any indicies are out of bounds.
 */
rw_status_t rw_keyspec_path_append_replace(rw_keyspec_path_t* k,
                                           rw_keyspec_instance_t* instance,
                                           const rw_keyspec_path_t* other,
                                           int k_index,
                                           size_t other_index,
                                           int other_len);

/*!
 * Truncate a keyspec to a specified depth.
 *
 * @param[in/out] k Keyspec to truncate.  Must be valid.  If binpath
 *   was set prior to this call, it will be cleared.
 * @param[in] index The 0-based path entry index to truncate at.
 *   Values outside of the path will produce an error.
 * @return
 *  - RW_STATUS_FAILURE: Probably not possible, as truncation cannot be
 *    prevented by keyspec dynamic type.
 *  - RW_STATUS_OUT_OF_BOUNDS if any indicies are out of bounds.
 */
rw_status_t rw_keyspec_path_trunc_suffix_n(rw_keyspec_path_t* k,
                                           rw_keyspec_instance_t* instance,
                                           size_t index);

/*!
 * Truncate a keyspec to the concrete types only.  All unknown fields
 * will be deleted.  All generic path elements will be deleted.  If
 * this function is called on a generic keyspec, then ALL path elements
 * will be deleted (because they are all generic)!
 *
 * @param[in/out] k Keyspec to truncate.  Must be valid.  If binpath
 *   was set prior to this call, it will be cleared.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] index The 0-based path entry index to truncate at.
 *   Values outside of the path will produce an error.
 * @return
 *  - RW_STATUS_FAILURE: Probably not possible, as truncation cannot be
 *    prevented by keyspec dynamic type.
 *  - RW_STATUS_OUT_OF_BOUNDS if any indicies are out of bounds.
 */
rw_status_t rw_keyspec_path_trunc_concrete(rw_keyspec_path_t* k,
                                           rw_keyspec_instance_t* instance);

/*!
 * Serializes the dompath or copies the binpath to a ProtobufCbinaryData
 * buffer. If the keyspec k already has binpath path, it is just copied.
 * Otherwise the dompath is serialized to the input buffer.
 * The input data buffer must be empty and the API will allocate
 * the data buffer based on the serialized buffer size.
 *
 * @param[in] k The keyspec k whos dom path is to be serialized. Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[out] data The data buffer to copy the serialized dompath.
 * Must be valid.
 *
 * @return
 *  - RW_STATUS_SUCCESS: if the copying of serialized dompath
 *  to the data buffer was successful.
 *  - RW_STATUS_FAILURE: if the copying was not successful.This could happen,
 *  if the keyspec doesnt have dompath or the input data buffer was not
 *  empty.
 *
 */
rw_status_t rw_keyspec_path_serialize_dompath(const rw_keyspec_path_t* k,
                                              rw_keyspec_instance_t* instance,
                                              ProtobufCBinaryData* data);

/*!
 * Allocate a new keyspec and initialize binpath from a copy of a buffer of
 * bytes.
 *
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] len length of the buffer. Must be non zero.
 * @param[in] Buffer containing the serialized data. Must be valid.
 *
 * @return
 * - A valid keypsec pointer of type RwSchemaPathSpec with the
 *   binpath initialized and dompath populated from the binpath.
 * - NULL on failure.
 */
rw_keyspec_path_t* rw_keyspec_path_create_with_binpath_buf(rw_keyspec_instance_t* instance,
                                                           size_t len, const void* data);

/*!
 * Allocate a keyspec and initialize binpath from a copy of a
 * ProtobufCBinaryData struct
 *
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] data The input struct, must be valid. The data pointer
 * should also be valid and the len must be non zero.
 *
 * @return
 * - A valid keypsec pointer of type RwSchemaPathSpec with the
 *   binpath initialized and dompath populated from the binpath.
 * - NULL on failure.
 */
rw_keyspec_path_t*
rw_keyspec_path_create_with_binpath_binary_data(rw_keyspec_instance_t* instance,
                                                const ProtobufCBinaryData* data);

/*!
 * Allocate a keyspec and populate the dompath from the
 * serialized binpath.
 *
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] data The input struct, must be valid. The data pointer
 * should also be valid and the len must be non zero.
 *
 * @return
 * - A valid keyspec pointer of type RwSchemaPathSpec with
 *   the dompath populated.
 * - NULL on failure.
 */
rw_keyspec_path_t*
rw_keyspec_path_create_with_dompath_binary_data(rw_keyspec_instance_t* instance,
                                                const ProtobufCBinaryData* data);

/*!
 * Allocate a concerete keyspec and populate the dompath from
 * the serialized binpath.
 *
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] data The input struct, must be valid. The data pointer
 * should also be valid and the len must be non zero.
 * @param[in] mdesc The conceret keyspec message descriptor. Must be valid.
 *
 * @return
 * - A valid keyspec of the input message descriptor type
 *   with the dompath populated.
 * - NULL on failure.
 */
rw_keyspec_path_t* rw_keyspec_path_create_concrete_with_dompath_binary_data(
    rw_keyspec_instance_t* instance,
    const ProtobufCBinaryData* data,
    const ProtobufCMessageDescriptor* mdesc);

/*!
 * Free a keypsec and all its submessages.
 *
 * @param[in/out] Keypsec to be freed. Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 *
 * @return
 * - RW_STATUS_SUCCESS: Always returns success.
 *   After the function returns k is no longer valid.
 */
rw_status_t rw_keyspec_path_free(rw_keyspec_path_t* k,
                                 rw_keyspec_instance_t* instance);

/*!
 * Checks whether a keyspec has wildcards for
 * any of its list elements.
 *
 * @param[in] k The keypsec to check. Must be a valid keyspec.
 *
 * @return
 * - TRUE: If the keyspec contains wildcards for list elements.
 * - FALSE: If the keypsec doesnt contain wildcards for any of its list elements.
 */
bool rw_keyspec_path_has_wildcards(const rw_keyspec_path_t *k);

/*!
 * Checks whether a keyspec has wildcards for
 * any of its first n(depth) elements.
 *
 * @param[in] k The keyspec to check. Must be a valid keyspec.
 * @param[in] depth The first 'n' path elements to check for wildcards.
 *
 * @return
 * - TRUE: If the keyspec contains wildcards for any of the first 'n' elements.
 * - FALSE: If the keypsec doesnt contain wildcards for any of its first 'n'  elements.
 */
bool rw_keyspec_path_has_wildcards_for_depth(const rw_keyspec_path_t *k, size_t depth);

/*!
 * Checks whether a keyspec has wildcards for
 * its last path element.
 *
 * @param[in] k The keyspec to check. Must be a valid keyspec.
 *
 * @return
 * - TRUE: If the keyspec contains wildcards for its last path entry.
 * - FALSE: If the keypsec doesnt contain wildcards for its last path entry.
 */
bool rw_keyspec_path_has_wildcards_for_last_entry(const rw_keyspec_path_t *k);

/*!
 * Checks whether the given keyspec is listy
 * at the last element in the path.
 *
 * @param[in] ks The input keyspec to check. Must be valid.
 *
 * @return
 * - TRUE: If the keyspec is listy at the last path element.
 * - FALSE: Otherwise.
 */
bool rw_keyspec_path_is_listy(const rw_keyspec_path_t *ks);

/*!
 * Checks whether the keyspec is generic
 *
 * @param[in] ks The input keyspec to check. Must be valid.
 *
 * @return
 * - TRUE: If the keyspec is generic
 * - FALSE: Otherwise.
 */
bool rw_keyspec_path_is_generic(const rw_keyspec_path_t *ks);

/*!
 * Checks whether the last entry is of leaf type in the keyspec.
 *
 * @param[in] ks The input keyspec to check. Must be valid.
 *
 * @return
 * - TRUE: If the last entry is of leaf type.
 * - FALSE: Otherwise.
 */
bool rw_keyspec_path_is_leafy(const rw_keyspec_path_t *ks);

/*!
 * Gets the Protobuf field descriptor for the leaf element
 * in the keyspec
 *
 * @param[in] ks The input keyspec to check. Must be valid.
 *
 * @return
 * - Returns ProtobufCFieldDescriptior pointer
 */
const ProtobufCFieldDescriptor *rw_keyspec_path_get_leaf_field_desc(const rw_keyspec_path_t *ks);

/*!
 * Checks whether the last element is a leaf.
 *
 * @param[in]  The Pathentry to check. Must be valid.
 *
 * @return
 * - TRUE: If the path entry is of listy type.
 * - FALSE: Otherwise.
 */
bool rw_keyspec_entry_is_listy(const rw_keyspec_entry_t *entry);

/*!
 * Get a print buffer for the keyspec
 * to be used in debug logs. If the strpath of the
 * keyspec is populated, the functions just copies
 * the strpath to the input buffer otherwise the function
 * goes over the dom path and fills a printable
 * string in the input buffer. The function takes
 * optional schema pointer as an argument. If present,
 * the schema pointer is used to generate a more readble
 * string; the element tags are converted to their respective
 * yang node names.
 *
 * @param[in] k The input keyspec. Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] schema The schema pointer. If this is valid,
 * the actual field names are obtained from the schema and
 * a better, readable print buffer is generated.
 * @param[in/out] buf The input buffer to copy the pritable output.
 * Must be valid. On success the buf is always
 * NULL terminated.
 * @param[in] buflen The input buffer length. If the target string
 * doesnt fit within this length, then it is truncated
 * and only buflen-1 bytes are copied to the buf.
 *
 * @return
 *   The number of bytes successfully copied to the input buffer.
 *   -1 if there was an error. The input buffer is always NULL
 *   terminated on success.
 */
int rw_keyspec_path_get_print_buffer(const rw_keyspec_path_t *k,
                                     rw_keyspec_instance_t* instance,
                                     const rw_yang_pb_schema_t *schema,
                                     char *buf,
                                     size_t buflen);

/*!
 * Same as rw_keyspec_path_get_print_buffer(). When the print buffer exceeds
 * the output buffer size, the left portion is truncated instead of the
 * right
 *
 * @param[in] k The input keyspec. Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] schema The schema pointer. If this is valid,
 * the actual field names are obtained from the schema and
 * a better, readable print buffer is generated.
 * @param[in/out] buf The input buffer to copy the pritable output.
 * Must be valid. On success the buf is always
 * NULL terminated.
 * @param[in] buflen The input buffer length. If the target string
 * doesnt fit within this length, then it is truncated
 * and only buflen-1 bytes are copied to the buf.
 *
 * @return
 *   The number of bytes successfully copied to the input buffer.
 *   -1 if there was an error. The input buffer is always NULL
 *   terminated on success.
 */
int rw_keyspec_path_get_print_buffer_ltruncate(const rw_keyspec_path_t *k,
                                     rw_keyspec_instance_t* instance,
                                     const rw_yang_pb_schema_t *schema,
                                     char *buf,
                                     size_t buflen);

/*!
 * Get a print buffer for the keyspec
 * to be used in debug logs. If the strpath of the
 * keyspec is populated, the functions just copies
 * the strpath to the input buffer otherwise the function
 * goes over the dom path and fills a printable
 * string in the input buffer. The function takes
 * optional schema pointer as an argument. If present,
 * the schema pointer is used to generate a more readble
 * string; the element tags are converted to their respective
 * yang node names.
 *
 * @param[in] k The input keyspec. Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] schema The schema pointer. If this is valid,
 * the actual field names are obtained from the schema and
 * a better, readable print buffer is generated.
 * @param[out] buf Buffer containing the printable string. Allocated
 * by this method and it is the responsibility of the caller to free
 * the allocated buffer using free().
 * On success the buf is always NULL terminated.
 *
 * @return
 *   The number of bytes successfully copied to the input buffer.
 *   -1 if there was an error. The input buffer is always NULL
 *   terminated on success.
 */
int rw_keyspec_path_get_new_print_buffer(const rw_keyspec_path_t *k,
                                     rw_keyspec_instance_t* instance,
                                     const rw_yang_pb_schema_t *schema,
                                     char **buf);

/*!
 * Walks a schema and an arbitary keyspec to find a ypbc descriprtor that
 * matches the keyspecs target. A specific keypsec for that ypbc descriptor
 * is then generated.
 *
 * @param[in] schema The input schema const. Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] ks The input keyspec to walk through the schema. Must be valid.
 * @param[out] spec_ks The specific keyspec derived from this generic keyspec
 * Caller is responsible for freeing it.
 *
 * @return
 * - RW_STATUS_SUCCESS: If the keyspec matched the schema. The result_msg
 *   contains the ypbc msg descriptor of the concerete keyspec. The
 *   result keyspec container the specific keyspec
 * - RW_STATUS_FAILURE: If the keyspec doesnt match the schema (even partial
 *   match) or there was an error during processing.
 * - RW_STATUS_OUT_OF_BOUNDS: There was a partial match, and the result_msg
 *   contains the y2pd msg descriptor of the deepest match keyspec from the
 *   schema. If remainder is valid, it points to a new keyspec, which contains
 *   the unmatched or the unparsed portion of the keyspec ks.
 */
rw_status_t rw_keyspec_path_find_spec_ks(const rw_keyspec_path_t* ks,
                                         rw_keyspec_instance_t* instance,
                                         const rw_yang_pb_schema_t* schema,
                                         rw_keyspec_path_t** spec_ks);

/*!
 * Walks a schema and an arbitrary keyspec to a ypbc descriptor that matches
 * the keyspec's target. This can be used to convert a generic keyspec to the
 * keyspec's concrete rift-protobuf-c conversion.
 *
 * @param[in] schema The input schema const. Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] ks The input keyspec to walk through the schema. Must be valid.
 * @param[out] result_msg The ypbc msg descriptor of the complete or the partial
 * deepest match pathspec in the schema.
 * @param[out] remainder If there was a partial match, this contains the portion
 * of the keyspec ks, that didnot match. The type it points to is
 * RwSchemaPathSpec. Caller is responsible for freeing it.
 *
 * @return
 * - RW_STATUS_SUCCESS: If the keyspec matched the schema. The result_msg
 *   contains the ypbc msg descriptor of the concerete keyspec.
 * - RW_STATUS_FAILURE: If the keyspec doesnt match the schema (even partial
 *   match) or there was an error during processing.
 * - RW_STATUS_OUT_OF_BOUNDS: There was a partial match, and the result_msg
 *   contains the y2pd msg descriptor of the deepest match keyspec from the
 *   schema. If remainder is valid, it points to a new keyspec, which contains
 *   the unmatched or the unparsed portion of the keyspec ks.
 */
rw_status_t rw_keyspec_path_find_msg_desc_schema(const rw_keyspec_path_t* ks,
                                                 rw_keyspec_instance_t* instance,
                                                 const rw_yang_pb_schema_t* schema,
                                                 const rw_yang_pb_msgdesc_t** result_msg,
                                                 rw_keyspec_path_t** remainder);

/*!
 * Walks a ypbc descriptor msg and an arbitrary keyspec to a ypbc descriptor that matches
 * the keyspec's target. This can be used to convert a generic keyspec to the
 * keyspec's concrete rift-protobuf-c conversion.
 *
 * @param[in] start_msg The ypbc msg descriptor to walk along.
 * Must be valid.
 * @param[in] ks The keyspec to match. Must be valid.
 * @param[in] instance instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] ks_depth The index in the keyspec that the start_msg
 * matched. Must be valid.
 * @param[out] result_msg The ypbc msg descriptor of the deepest match
 * pathspec along the start_msg.
 * @param[out] remainder If there is a partial match, this contains the
 * portion of the keyspec ks, that didnot match. The keyspec is
 * of type RwSchemaPathSpec. The caller is responsible for
 * freeing it.
 *
 * @return
 * - RW_STATUS_SUCCESS: If the keyspec matched a path along the start_msg. The result_msg
 *   contains the ypbc msg descriptor of the concrete keyspec.
 * - RW_STATUS_FAILURE: If the keyspec doesnt match any path(even partial
 *   match) or there was an error during processing.
 * - RW_STATUS_OUT_OF_BOUNDS: There was a partial match, and the result_msg
 *   contains the y2pd msg descriptor of the deepest match keyspec from the
 *   start_msg. If remainder is valid, it points to a new keyspec, which contains
 *   the unmatched or the unparsed portion of the keyspec ks.
 */
rw_status_t rw_keyspec_path_find_msg_desc_msg(const rw_keyspec_path_t* ks,
                                              rw_keyspec_instance_t* instance,
                                              const rw_yang_pb_msgdesc_t* start_msg,
                                              size_t ks_depth,
                                              const rw_yang_pb_msgdesc_t** result_msg,
                                              rw_keyspec_path_t** remainder);

/*!
 * The API takes a keyspec and corresponding message, and converts
 * them into a new message rooted at a different keyspec.
 * The reroot keyspec may be shallower or deeper than the original keyspec.
 * If the new keyspec is shallower, then the new message will be deeper than the
 * original message and the key values will be populated from the original
 * keyspec.
 * If the new keyspec is deeper, then the new message will just be extracted
 * as a portion of the original message. The extraction will obey the key values
 * from the new keyspec, which could possibly result in no message at all (due
 * to key mismatch).
 *
 * @param[in] in_k The in_keyspec. Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] in_msg The in_msg corresponding to the in_keyspec.
 * Must be valid.
 * @param[in] out_k The out_keyspec; deeper or shallower or equal to in_keyspec.
 * Must be valid.
 *
 * @return
 *  - The protobuf-c message corresponding to the out_keyspec.
 *    The returned message is always a new copy, and the caller
 *    is responsible for freeing the memory.
 *  - NULL, if the keys in the out_keypsec and in_msg do not match
 *    or there was any other error.
 */
ProtobufCMessage* rw_keyspec_path_reroot(const rw_keyspec_path_t *in_k,
                                         rw_keyspec_instance_t* instance,
                                         const ProtobufCMessage *in_msg,
                                         const rw_keyspec_path_t *out_k);

/*!
 * The API takes a keyspec and the schema that it is defined in, and converts
 * them into a new message rooted at a different keyspec.
 * The reroot keyspec must be shallower than the original keyspec.
 * The new message will be deeper than the
 * original message and the key values will be populated from the original
 * keyspec.
 *
 * @param[in] schema The schema that the keyspecs belong to
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] in_k The in_keyspec. Must be valid.
 * @param[in] out_k The out_keyspec; Shallower than in_keyspec.
 * Must be valid.
 *
 * @return
 *  - The protobuf-c message corresponding to the out_keyspec.
 *    The caller is responsible for freeing the memory.
 *  - NULL, if the keys in the out_keypsec and in_msg do not match
 *    or there was any other error.
 */
ProtobufCMessage* rw_keyspec_path_schema_reroot(const rw_keyspec_path_t *in_k,
                                                rw_keyspec_instance_t* instance,
                                                const rw_yang_pb_schema_t* schema,
                                                const rw_keyspec_path_t *out_k);

/*!
 * Determine whether either keyspec is a matching prefix of the other
 * keyspec, allowing wildcards.
 *
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] a The first keyspec. Must be valid.
 * @param[in] b The second keyspec. Must be valid.
 *
 * @return
 * - True if all the path entries match (same key values, or either key
 *   is wildcarded, for all keys) for all entries in the shorted
 *   keyspec.
 * - False otherwise.
 */
bool rw_keyspec_path_is_a_sub_keyspec(rw_keyspec_instance_t* instance,
    const rw_keyspec_path_t *a, const rw_keyspec_path_t *b);

/*!
 * Determine whether the shorter keyspec is a matching prefix of the longer
 * keyspec, allowing wildcards.
 *
 * @param[in] shorter_ks - The shorter keyspec.  Must be valid.  It is
 *   not a strict precondition that shorter_ks actually be shorter than
 *   longer_ks.  However if shorter_ks is longer than longer_ks, then
 *   this function will return false, regardless of whether the
 *   keyspecs match over their common prefix.  This behaviour contrasts
 *   with rw_keyspec_path_is_a_sub_keyspec(), which would return true in
 *   that case.
 * @param[in] longer_ks - The longer keyspec. Must be valid.
 *
 * @return
 * - True if all the path entries in shorter_ks match the corresponding
 *   path entries in longer_ks (same key values, or either key is
 *   wildcarded, for all keys).
 * - False otherwise.
 */
bool rw_keyspec_path_is_match_or_prefix(const rw_keyspec_path_t *shorter_ks, const rw_keyspec_path_t *longer_ks);


/*!
 * The API takes a conceret protobuf message,
 * and creates a path entry corresponding to the
 * message and fills it with the key values
 * if any from the message.
 *
 * @param[in] msg The in msg. Must be valid.
 *
 * @return
 * - The concerete path entry of the message
 *   with the key values filled in.
 * - NULL, if there are errors.
 */
rw_keyspec_entry_t*
rw_keyspec_entry_create_from_proto(rw_keyspec_instance_t* instance,
                                   const ProtobufCMessage *msg);

/*!
 * The API takes a keyspec,
 * and creates a protobuf message corresponding
 * to the keyspec
 *
 * @param[in] msg The in keyspec. Must be valid.
 *
 * @return
 * - The protobuf message corresponding
 *   to the keyspec
 * - NULL, if there are errors.
 */
ProtobufCMessage*
rw_keyspec_pbcm_create_from_keyspec(rw_keyspec_instance_t* instance,
                                    const rw_keyspec_path_t *ks);

/*!
 * The keyspec API that can merge DTS query results.
 * The input is a pair of keyspecs, ks1 and ks2, and a pair
 * of matching messages, msg1 and msg2. The output is a new ks3
 * containing the common prefix of ks1 and ks2, and msg3
 * that merges msg1 and msg2, rerooting the result as
 * necessary to conform to ks3
 *
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] ks1 The first keyspec.
 * @param[in] ks2 The second keyspec.
 * @param[in] msg1 The msg1 corresponding to ks1.
 * @param[in] msg2 The msg2 corresponding to ks2.
 * @param[out] ks_out The out keyspec. Common prefix of ks1 and ks2.
 *
 *
 * @return
 *   - On success, returns the new keyspec ks3 and the re-rooted msg, whic
 *     the caller owns and is responsible for freeing it.
 *   - On failure, returns NULL.
 */
ProtobufCMessage* rw_keyspec_path_reroot_and_merge(rw_keyspec_instance_t* instance,
                                                   const rw_keyspec_path_t *ks1,
                                                   const rw_keyspec_path_t *ks2,
                                                   const ProtobufCMessage *msg1,
                                                   const ProtobufCMessage *msg2,
                                                   rw_keyspec_path_t **ks_out);

/*!
 * Make a specific keyspec and message from two serialized buffers.
 * In Riftware, a keyspec and message form base of many operations.
 * When both the keyspec and message are in serialized form, and a schema
 * is available, this function can be used to generate a specific message
 * for both the keyspec and message
 *
 * @param [in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param [in] schema The message schema that defines the message and keyspecs
 * @param [in] msg_b Serialized form of the message
 * @param [in] ks_b Serialized form of the keyspec
 * @param [out] msg Specialized message created from serialized msg_b, Owned
 *                  by caller
 * @param [out] ks Specialized ks created from serialized ks_b, Owned by
 *                 caller
 */

rw_status_t rw_keyspec_path_deserialize_msg_ks_pair (rw_keyspec_instance_t* instance,
                                                     const rw_yang_pb_schema_t* schema,
                                                     const ProtobufCBinaryData* msg_b,
                                                     const ProtobufCBinaryData* ks_b,
                                                     ProtobufCMessage** msg,
                                                     rw_keyspec_path_t** ks);

/*!
 * Copy keys from the path entry to the keyspec.
 * This API can be used to add keys to the
 * keyspec.
 *
 * @param[in/out] ks The keyspec to copy keys.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] path_entry The path entry from which to copy
 * keys to the corresponding or matching path entry in
 * the keyspec
 * @param[in] index The index of the path entry in the
 * keypsec that matches the input path entry.
 *
 * @return
 *   - TRUE if the keys were successfully copied to
 *   the keyspec.
 *   - FALSE if there was an error.
 */
rw_status_t
rw_keyspec_path_add_keys_to_entry(rw_keyspec_path_t *ks,
                                  rw_keyspec_instance_t* instance,
                                  const rw_keyspec_entry_t *path_entry,
                                  size_t index);

/*
 * The following set of APIs are for reroot iterate
 * functionality. (RIFT-3631)
 */

/*!
 * Opaque type for the reroot_iterate state. This type will
 * be passed for the reroot API calls. This is mainly to hide
 * the users of the API about the internal details of the API
 * and the state data.
 */
#define RW_SCHEMA_REROOT_STATE_SIZE 928
typedef char rw_keyspec_path_reroot_iter_state_t[RW_SCHEMA_REROOT_STATE_SIZE];

/*!
 * Initalizes the reroot iterate state.
 * The previous contents of the state are ignored.
 * The init API must be called before calling any of
 * the reroot_iter APIs. The following set of APIs
 * must be used only when the out_ks is equal or deeper
 * than in_ks. For shallower case, the normal reroot API
 * must be used.
 *
 * @param[in] state The local/valid state pointer
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] in_ks The input keyspec. Must be valid.
 * @param[in] in_msg The input message corresponding to
 * the input keyspec.
 * @param[in] out_ks The out keyspec to which the input message
 * must be rerooted. Must be valid.
 *
 * @return
 *   - None.
 */
void rw_keyspec_path_reroot_iter_init(const rw_keyspec_path_t *in_ks,
                                      rw_keyspec_instance_t* instance,
                                      rw_keyspec_path_reroot_iter_state_t *state,
                                      const ProtobufCMessage *in_msg,
                                      const rw_keyspec_path_t *out_ks);

/*!
 *
 * Iterates over the in_msg creating a new keyspec and msg
 * for each matching submessage. When a matching submessage
 * is found, it returns true, return false otherwise. The matched
 * submessage and the keyspec are stored in the state variable
 * which can be retrived later using get_ks/get_msg/take_ks/take_msg
 * functions. Only pointer to the submsg in the original message
 * is stored. Copying is done only when user calls take_msg.
 * A new keyspec is created only when the out_ks is wildcarded.
 * Otherwise only an alias to the out_ks is stored.
 *
 * @param[in] state The iterate state variable
 * Should have been be initialized by calling init().
 *
 * @return
 *   - True: If there was a matching submessage.
 *   - False: If there is no more matching submessage.
 */
bool rw_keyspec_path_reroot_iter_next(rw_keyspec_path_reroot_iter_state_t *state);

/*!
 * Returns the pointer to the ks created/stored
 * during the iter_next. If the out_ks is not wildcarded
 * the returned pointer is equal to out_ks. Otherwise
 * the returned pointer is a new keyspec matching the curr
 * sub message. The caller should not free the pointer.
 * The caller can only access the pointer. Get can be
 * called multiple times as long as the previous iter_next call
 * returned true.  Also get_ks should not be called after calling
 * take_ks. See below.
 *
 * @param[in] state The reroot_iter state. Should have
 * been initialized by the init function.
 *
 * @return:
 *     - The cur_ks created by the previous iter_next call.
 *     - If called without calling iter_next or when iter_next
 *       returned false or when take_ks already took the cur ks,
 *       this function would assert.
 */
const rw_keyspec_path_t*
rw_keyspec_path_reroot_iter_get_ks(rw_keyspec_path_reroot_iter_state_t *state);

/*!
 * Returns the pointer to the submessage in the in_msg
 * that was found during iter_next. The iter_next function
 * always stores only the pointer of the
 * submessage inside the in_msg.
 * The caller should not free the pointer.
 * The caller can only access the pointer. Get can be
 * called multiple times as long as the previous iter_next call
 * returned true.  Also get should not be called after calling
 * take_msg. See below.
 *
 * @param[in] state The reroot_iter state. Should have
 * been initialized by the init function.
 *
 * @return:
 *     - The current matching submessage found by the previous iter_next call.
 *     - If called without calling iter_next or when iter_next
 *       returned false or when take_msg already took the cur-msg
 *       this function would assert.
 */
const ProtobufCMessage*
rw_keyspec_path_reroot_iter_get_msg(rw_keyspec_path_reroot_iter_state_t *state);

/*!
 * Returns the cur ks found by the previous
 * iter_next call. If the cur ks was an alias
 * to the out_ks, this function duplicates and returns
 * a new ks. Whatever is the case, the caller is
 * responsible for the returned ks pointer
 * and the caller should free it after its usage.
 * A call to get_ks after take_ks in the same iteration
 * would assert. Also call to take_ks twice within the same
 * iteration will assert.
 *
 * @param[in] state The reroot_iter state. Should have
 * been initalized by the init function.
 *
 * @return:
 *    - A copy of the cur_ks. The caller is responsible for
 *      freeing it.
 *    - Any error, the function will assert.
 */
rw_keyspec_path_t*
rw_keyspec_path_reroot_iter_take_ks(rw_keyspec_path_reroot_iter_state_t *state);

/*!
 * Returns a copy of the submsg in the in_msg
 * found during the iter_next call. The caller is
 * owner of the returned proto message and the caller
 * is responsible for freeing it. A call to get_msg after
 * take_msg in the same iteration would assert. Also,
 * call to take_ks twice within the same iteration
 * will assert.
 *
 * @param[in] state The reroot_iter state. Should have
 * been initalized by the init function.
 *
 * @return:
 *    - A copy of the matched submessage. The caller
 *    is responsible for freeing it.
 *    - Any error, the function will assert.
 *
 */
ProtobufCMessage*
rw_keyspec_path_reroot_iter_take_msg(rw_keyspec_path_reroot_iter_state_t *state);

/*!
 * Cleanup routine to be called when the reroot iteration
 * finishes. It resets the sate, additionally freeing up the
 * allocated memory if any. This function is
 * optional and need not be called when iter_next returned
 * false. If the caller want to terminate the reroot_iter
 * early, before the iter_next returns false,
 * then this function should be called to cleanup the state.
 * Otherwise there will be leaks. None of the other reroot_iter
 * APIs except init() should be called after calling done().
 *
 * @param[in] state The reroot_iter state. Should have
 * been initalized by the init function.
 *
 * @return:
 *    - None.
 *
 */
void rw_keyspec_path_reroot_iter_done(rw_keyspec_path_reroot_iter_state_t *state);

/*!
 * Duplicate input to output, if possible, allocating a new path entry in
 * the process.  Upon success, protobuf_is_equal(input,output)
 * will be true.
 *
 * @param[in] input The path entry to duplicate. Will not be modified.
 * Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[out] output Will be set to the newly created duplicate. The duplicate
 * will end up with the same dynamic type as input (if you need to
 * end up with a generic or different specific type, then use copy
 * instead of dup). Target pointer must be NULL upon input; this
 * forces the caller to properly dispose of any previous value,
 * because this function cannot do that correctly in all cases. If
 * the function fails, the value will be undefined, but will not need
 * to be freed.
 *
 * @return RW_STATUS_FAILURE if the path entry cannot be duplicated.  This
 *   could happen if output is a specific type and flat.
 */
rw_status_t
rw_keyspec_entry_create_dup(const rw_keyspec_entry_t* input,
                            rw_keyspec_instance_t* instance,
                            rw_keyspec_entry_t** output);

/*!
 * Duplicate input to output of specified type, if possible,
 * allocating a new pathentry in  the process.
 *
 * @param[in] input The pathentry to duplicate. Will not be modified.
 * Must be valid.
 *
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 *
 * @param[in] out_pbcmd The type to duplicate to. Will not be modified.
 * Must be valid.
 *
 * @return The new pathentry of the input type. NULL if the keyspec
 * cannot be duplicated to the specified type.  This
 * could happen if output is a specific type and flat.
 */
rw_keyspec_entry_t*
rw_keyspec_entry_create_dup_of_type(const rw_keyspec_entry_t* input,
                                    rw_keyspec_instance_t* instance,
                                    const ProtobufCMessageDescriptor* out_pbcmd);

/*!
 * Free the path entry. After the function returns
 * the pointer is no longer valid.
 *
 * @param[in] path_entry The path entry to free. Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 *
 * @return RW_STATUS_SUCCESS.
 */
rw_status_t rw_keyspec_entry_free(rw_keyspec_entry_t *path_entry,
                                  rw_keyspec_instance_t* instance);

/*!
 * Free the contents of path entry without freeing
 * the path entry pointer.
 *
 * @param[in] path_entry The path entry to free. Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 *
 * @return RW_STATUS_SUCCESS.
 */
rw_status_t
rw_keyspec_entry_free_usebody(rw_keyspec_entry_t *path_entry,
                              rw_keyspec_instance_t* instance);

/*!
 * Generate a print buffer of the path_entry
 * for output in the debug logs.
 *
 * @param[in] path_entry The input path_entry
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in/out] buf The input buffer to copy the pritable output.
 * Must be valid. On success the buf is always NULL terminated.
 * @param[in] buflen The input buffer length. If the target string
 * doesnt fit within this length, then it is truncated
 * and only buflen-1 bytes are copied to the buf.
 *
 * @return:
 *    The number of bytes successfully copied to the
 *    input buffer.
 *    -1 is there was an error. The input buffer is always NULL
 *    terminated on success.
 */
int rw_keyspec_entry_get_print_buffer(const rw_keyspec_entry_t *path_entry,
                                      rw_keyspec_instance_t* instance,
                                      char *buf,
                                      size_t buflen);

/*!
 * Create a pathentry of type mdesc from the serialized/packed data.
 *
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] mdesc The ProtobufCMessage descriptor used to unpack.
 * @param[in] data Pointer to the serialized buffer.
 * @param[in] len The len of the serialized buffer.
 *
 * @return:
 *    On success returns path entry of type mdesc.
 *    On failure returns NULL.
 */
rw_keyspec_entry_t*
rw_keyspec_entry_unpack(rw_keyspec_instance_t* instance,
                        const ProtobufCMessageDescriptor* mdesc,
                        uint8_t *data,
                        size_t len);

/*!
 *  Serialize the path entry.
 *
 * @param[in] path_entry The path entry to serialize. Must be valid.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in/out] buf The buffer to write the serialized data. Must be valid.
 * @param[in] len The length of the buffer.
 *
 * @return:
 *    The number of bytes of the serialized data.
 *    -1 on failure. Failure can occur if the target buffer doesn't
 *    contain enough space to hold the serialized data.
 */
size_t rw_keyspec_entry_pack(const rw_keyspec_entry_t* path_entry,
                             rw_keyspec_instance_t* instance,
                             uint8_t *buf,
                             size_t len);

/*!
 * Create a Generic PathEntry that refers to a leaf
 * node in the schema.
 *
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] the yang_pb_msd_desc of the containing container/list
 * @param[in] The yang leaf node name.
 *
 * @return:
 *    -A generic path entry refering to the leaf node.
 *    The element_tag is set to the protobuf tag value of
 *    the leaf node. Keys in the path entry are not applicable.
 *    -NULL, if the leaf is not found in the enclosing container
 *    or list.
 */
rw_status_t rw_keyspec_path_create_leaf_entry(rw_keyspec_path_t* k,
                                              rw_keyspec_instance_t* instance,
                                              const rw_yang_pb_msgdesc_t *ypb_mdesc,
                                              const char *leaf);

/*!
 * Return the yang leaf name, if the keyspec refers to a leaf node
 * in the yang.
 *
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] ks The input keyspec must be valid.
 * @param[in] The yangpbc message descriptor of the container/list enclosing
 * the leaf node.
 *
 * @return:
 *     The yang leaf node name in the container/list who's tag
 *     matches with that of the last path entry's element-tag.
 *     NULL, if there was no match.
 */
const char*
rw_keyspec_path_leaf_name(const rw_keyspec_path_t* ks,
                          rw_keyspec_instance_t* instance,
                          const ProtobufCMessageDescriptor *pbcmd);


/*!
 * The keyspec API that can merge DTS query results.
 * The input is a pair of keyspecs, ks1 and ks2, and a pair
 * of matching packed messages, msg1 and msg2. The output is a
 * new ks3 containing the common prefix of ks1 and ks2, and msg3
 * that merges msg1 and msg2, rerooting the result as
 * necessary to conform to ks3
 *
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] ks1 The first keyspec.
 * @param[in] ks2 The second keyspec.
 * @param[in] msg1 The packed proto message corresponding to ks1.
 * @param[in] msg2 The packed proto message corresponding to ks2.
 * @param[out] ks_out The out keyspec. Common prefix of ks1 and ks2.
 * @param[out] msg_out. The packed proto containing the merged result of msg1
 * and msg2.
 *
 *
 * @return
 *   - on success returns RW_STATUS_SUCCESS.
 *   - On failure, returns RW_STATUS_FAILURE
 */
rw_status_t
rw_keyspec_path_reroot_and_merge_opaque(
    rw_keyspec_instance_t* instance,
    const rw_keyspec_path_t *ks1,
    const rw_keyspec_path_t *ks2,
    const ProtobufCBinaryData *msg1,
    const ProtobufCBinaryData *msg2,
    rw_keyspec_path_t **ks_out,
    ProtobufCBinaryData *msg_out);

/*!
 * Merge two input keyspecs ks1 qnd ks2.
 * ks1 and ks2 can be of same depth or different depth;can be concrete or generic ks.
 * The relation between ks1 and ks2 is that one is a sub-keyspec of the other.
 * The shorter depth keyspec must match the longer keyspec for all the
 * path-entires (keys are matched according to the wildcard matching rules.)
 * On success the API returns the new merged keyspec which is of generic type.
 * The input keyspec ks1 and ks2 are not modified and the caller is
 * responsible for freeing the returned keyspec.
 *
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] ks1 The input keypsec. Must be valid.
 * @param[in] ks2 The input keyspec. Must be valid.
 *
 * @return
 *   - The merged keyspec which is always of generic type. The caller owns the returned keyspec.
 *   - NULL, if there is a failure to merge.
 */
rw_keyspec_path_t *
rw_keyspec_path_merge(rw_keyspec_instance_t* instance,
                      const rw_keyspec_path_t *ks1,
                      const rw_keyspec_path_t *ks2);

/*
 * Given a keyspec, create a new GI boxed path entry for the given
 * schema context location, if the schema context matches one of the
 * path entries in the keyspec.
 *
 * @return
 *  - A new, GI boxed, concrete path entry if ks contains a path entry
 *    that corresponds to msgdesc.  The caller becomes responsible for
 *    freeing this object, or handing it off to another owner.  It will
 *    already have 1 reference (for the caller).
 *  - nullptr otherwise.
 */
ProtobufCGiMessageBox* rw_keyspec_entry_check_and_create_gi_dup_of_type(
  /*!
   * [in] The keyspec.
   */
  const rw_keyspec_path_t *ks,

  /*!
   * [in] The keyspec instance pointer.
   */
  rw_keyspec_instance_t* instance,

  /*!
   * [in] The protobuf message descriptor for any schema-aware message
   * at the the desired schema location.
   */
  const ProtobufCMessageDescriptor* pbcmd);

/*!
 * For iterator based conversions from other types to a keyspec, there is a need
 * to find the right key field in a path spec from a yang node name. Both levels
 * are needed, the descriptor at the key level, and the description of the feild at
 * in the key submessage.
 *
 * @param[in] path The path entry to search
 * @param[in] name The yang node name
 * @param[in] ns The namespace that the yang node belongs to
 * @param[out] k_desc The descriptor for the key
 * @param[out] f_desc The descriptor of the field within the key submessage
 *
 * @return RW_STATUS_SUCCESS if the field can be found, RW_STATUS_FAILURE
 *                           otherwise
 */

rw_status_t
rw_keyspec_path_key_descriptors_by_yang_name (const rw_keyspec_entry_t* entry,
                                              const char *name,
                                              const char *ns,
                                              const ProtobufCFieldDescriptor** k_desc,
                                              const ProtobufCFieldDescriptor** f_desc);

/*!
 * Given a keyspec, its corresponding proto message and a deeper ks,
 * delete the field in the proto message that the deeper ks points to.
 * The deeper keyspec can point to a leaf, entry in leaf-list, a list
 * element, complete container or a complete list.
 *
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the default.
 * @param[in] msg_ks The keyspec that corresponds to the message.
 * @param[in] del_ks The delete keyspec that points to the field to be deleted.
 * @param[in/out] The protomessage corresponding to msg_ks.
 *
 * @return RW_STATUS_SUCCESS if the field is present and successfully deleted.
 *         RW_STATUS_FAILURE if the field to delete is not present in the msg.
 */
rw_status_t
rw_keyspec_path_delete_proto_field(rw_keyspec_instance_t* instance,
                                   const rw_keyspec_path_t* msg_ks,
                                   const rw_keyspec_path_t* del_ks,
                                   ProtobufCMessage *msg);

/*!
 * Given a ProtobufCMessage and a keyspec, validate if the message is one which
 * matches the keyspec.
 *
 * @param[in] ks - Keyspec
 * @param[in] instance The keyspec instance pointer. Pass NULL to use the
 * default.
 * @param[in] msg - Message
 * @param[in] match_c_struct - if true, check for strict c struct compatibility
 *            else allow msg-to-path compatibility.
 *
 * @return true if the keyspec and msg match, false otherwise
 */

bool rw_keyspec_path_matches_message (const rw_keyspec_path_t *ks,
                                      rw_keyspec_instance_t* instance,
                                      const ProtobufCMessage *msg,
                                      bool match_c_struct);
/*!
 * API to export keyspec error records.
 * This is a kind of hack to avoid making the error buffer itself
 * public. Here the API assumes that the passed protobuf has a message field
 * which contains two fields, one for time-stamp and other for error-string.
 * This is a bad idea, but it is ok for now as it is a debug code.
 *
 * @param[in] instance The keyspec instance pointer. Pass NULL to use default.
 * @param[in/out] msg The protobuf C message. Some assumption is made about the
 * message structure here!
 * @param[in] record_fname The name of the field inside the protobuf msg which
 * indicates a error record (msg type)
 * @param[in] ts_fname The name of the field inside the error record that
 * specifies the time-stamp field.
 * @param[in] es_fname The name of the field inside the error record that
 * specifies the error-string field.
 */
int rw_keyspec_export_error_records(rw_keyspec_instance_t* instance,
                                    ProtobufCMessage* msg,
                                    const char* record_fname,
                                    const char* ts_fname,
                                    const char* es_fname);


/*!
 * Create an RPC Output KS from an RPC Input/Output KS.
 *
 * @param[in] instance The keyspec instance pointer. Pass NULL to use default.
 * @param[in] input The input keyspec
 * @param[in] The schema, if the input keyspec is a generic keyspec
 *
 * @return The RPC Output Keypsec corresponding to the input keyspec.
 * The caller owns the returned keyspec. NULL if there was any error.
 */
rw_keyspec_path_t*
rw_keyspec_path_create_output_from_input(const rw_keyspec_path_t* input,
                                         rw_keyspec_instance_t* instance,
                                         const rw_yang_pb_schema_t* schema);


/*!
 * API to create a PBCM delete delta based on the delete ks and the
 * registration ks.
 *
 * @param[in] del_ks The incoming delete keyspec.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use default.
 * @param[in] reg_ks The registration keyspec.
 *
 * @return The PBCM containing the delete delta. NULL if there is any error.
 */
ProtobufCMessage*
rw_keyspec_path_create_delete_delta(const rw_keyspec_path_t* del_ks,
                                    rw_keyspec_instance_t* instance,
                                    const rw_keyspec_path_t* reg_ks);

//ATTN: doxy
uint8_t*
rw_keyspec_entry_get_key_packed(const rw_keyspec_entry_t* entry,
                                rw_keyspec_instance_t* instance,
                                int key_index,
                                size_t* len);
/*!
 * API to take all keys in a path element, serialize it and return the buffer 
 *
 * @param[in] entry The incoming path element.
 * @param[in] instance The keyspec instance pointer. Pass NULL to use default.
 * @param[out] len  Length of the serialized buffer.
 *
 * @return Pionter to serialized buffer. NULL if there is any error.
 */
uint8_t*
rw_keyspec_entry_get_keys_packed(const rw_keyspec_entry_t* entry,
                                rw_keyspec_instance_t* instance,
                                size_t* len);
//ATTN: doxy
ProtobufCMessage*
rw_keyspec_entry_get_key(const rw_keyspec_entry_t *path_entry,
                         int index);

#endif /* ndef __GI_SCANNER__ */
__END_DECLS


/*****************************************************************************/
/*
 * C++ helper classes
 */

#ifdef __cplusplus
#ifndef __GI_SCANNER__

struct UniquePtrKeySpecPath
{
  void operator()(rw_keyspec_path_t* path) const
  {
     rw_keyspec_path_free (path, NULL);
  }

  typedef std::unique_ptr<rw_keyspec_path_t,UniquePtrKeySpecPath> uptr_t;
};

struct UniquePtrKeySpecEntry
{
  rw_keyspec_instance_t* instance_ = nullptr;

  UniquePtrKeySpecEntry(rw_keyspec_instance_t* instance = nullptr)
  : instance_(instance)
  {}

  void operator()(rw_keyspec_entry_t* entry) const
  {
    rw_keyspec_entry_free (entry, instance_);
  }

  typedef std::unique_ptr<rw_keyspec_entry_t, UniquePtrKeySpecEntry> uptr_t;
};

#endif /* ndef __GI_SCANNER__ */
#endif /* def __cplusplus */

/*! @} */

#endif // RW_KEYSPEC_H_
