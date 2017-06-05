/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rwdts_shard.c
 * @date Wed Aug 19 2015
 * @brief DTS sharding related APIs 
 */
#include <stdlib.h>
#include <rwtypes.h>
#include <rwdts_int.h>
#include <rw_dts_int.pb-c.h>
#include <rwdts_keyspec.h>
#include <rwdts.h>
#include <protobuf-c/rift-protobuf-c.h>
#include <rwlib.h>
#include <rwdts_kv_light_api.h>
#include <rwdts_kv_light_api_gi.h>
#include <rw-dts.pb-c.h>
#include <rwdts_int.h>

void
rwdts_shard_chunk_unref(rwdts_shard_t *s , const char *file, int line);

//
// Internal function invoked by rwdts_shard_handle_unref
// always all rwdts_shard_handle_unref to free the shard
// Should not call rwdts_shard_deinit directly.
//
void
rwdts_shard_deinit(rwdts_shard_t* shard_ptr)
{
  rwdts_shard_t *s = shard_ptr;
  RW_ASSERT_TYPE(s, rwdts_shard_t);

  // Lets unref the parent if present
  rwdts_shard_t *parent = s->parent;
  RW_ASSERT(!s->wildcard_shard);
  if (parent) {
    if (s->key) { // not wildcard 
      HASH_DELETE(hh_shard, parent->children, s);
    }
    else {
      RW_ASSERT(parent->wildcard_shard == s);
      parent->wildcard_shard = NULL;
    }
    rwdts_shard_handle_unref(parent, __PRETTY_FUNCTION__, __LINE__);
  }
  if (s->key) {
    RW_FREE(s->key);
  }
  /* Delete the appdata related data */
  rwdts_shard_handle_appdata_delete(s);

  DTS_APIH_FREE_TYPE(s->apih, RW_DTS_DTS_MEMORY_TYPE_SHARD, s, rwdts_shard_t);
}

void
rwdts_chunk_deinit(rwdts_shard_chunk_info_t* s)
{
  rwdts_api_t *apih = s->parent->apih;
  RW_ASSERT_TYPE(s, rwdts_shard_chunk_info_t);
  // remove this chunk from parent shard's skip list
  HASH_DELETE(hh_chunk, s->parent->shard_chunks, s);
  rwdts_shard_handle_unref(s->parent,__PRETTY_FUNCTION__, __LINE__);
  DTS_APIH_FREE_TYPE(apih,
                     RW_DTS_DTS_MEMORY_TYPE_SHARD_CHUNK, s, rwdts_shard_chunk_info_t);
}

//
// takes a ref count for the shard
//
rwdts_shard_t
*rwdts_shard_handle_ref(rwdts_shard_t *s, const char *file, int line)
{
  RW_ASSERT_TYPE(s, rwdts_shard_t);
  g_atomic_int_inc(&s->ref_cnt);
  return s;
}

//
// take a ref count for the chunk
//
rwdts_shard_chunk_info_t
*rwdts_chunk_ref(rwdts_shard_chunk_info_t *s, const char *file, int line)
{
  RW_ASSERT_TYPE(s, rwdts_shard_chunk_info_t);
  g_atomic_int_inc(&s->ref_cnt);
  return s;
}

//
// take ref count for all chunks for this shard
//
void
rwdts_shard_chunk_ref(rwdts_shard_t *s , const char *file, int line)
{
  rwdts_shard_chunk_info_t *shard_chunk, *tmp_chunk;
  int max_ref_cnt = 0;
 
  if (s->shard_chunks == NULL) {
    return;
  }

  HASH_ITER(hh_chunk, s->shard_chunks, shard_chunk, tmp_chunk) {
    if ( shard_chunk->ref_cnt > max_ref_cnt) {
      max_ref_cnt = shard_chunk->ref_cnt;
    }
  }
  max_ref_cnt++;
  HASH_ITER(hh_chunk, s->shard_chunks, shard_chunk, tmp_chunk) {
    shard_chunk->ref_cnt = max_ref_cnt; // TBD atomic set?
  }

}

//
// unref the shard. this will automatically free the shard struct
// when the ref count reaches zero and decrement the ref count
// of parent in this case. If the parent ref count also become zero
// the same steps repeated for the parent until all the shards with 
// zero ref count is freed up.
//
void
rwdts_shard_handle_unref(rwdts_shard_t *shard_ptr, const char *file, int line)
{
  rwdts_shard_t *s = shard_ptr;

  RW_ASSERT_TYPE(s, rwdts_shard_t);
  RW_ASSERT(s->ref_cnt>0);
  if (!g_atomic_int_dec_and_test(&s->ref_cnt)) {
    return;
  }
  rwdts_shard_deinit(shard_ptr);
}

void
rwdts_chunk_unref(rwdts_shard_chunk_info_t *s, const char *file, int line)
{
  RW_ASSERT_TYPE(s, rwdts_shard_chunk_info_t);
  RW_ASSERT(s->ref_cnt > 0);
  if (!g_atomic_int_dec_and_test(&s->ref_cnt)) {
    return;
  }
  rwdts_chunk_deinit(s);
}

//
// unref all chunks in this shard
//
void
rwdts_shard_chunk_unref(rwdts_shard_t *s , const char *file, int line)
{
  rwdts_shard_chunk_info_t *shard_chunk, *tmp_chunk;

  HASH_ITER(hh_chunk, s->shard_chunks, shard_chunk, tmp_chunk) {
    rwdts_chunk_unref(shard_chunk, file, line);
  }
}

static rwdts_shard_handle_t*
rwdts_shard_handle_ref_gi(rwdts_shard_handle_t *boxed)
{
  rwdts_shard_handle_ref(boxed, "from Gi", 0);
  return boxed;
}

static void
rwdts_shard_handle_unref_gi(rwdts_shard_handle_t *boxed)
{
  rwdts_shard_handle_unref(boxed, "from Gi", 0);
}

/* Registers the Boxed struct resulting in a GType,
 * this also tells it how to manage the memory using either
 * a ref/unref pair or a copy/free pair.
 *
 * This defines rwdts_shard_handle_get_type()
 */
G_DEFINE_BOXED_TYPE(rwdts_shard_handle_t,
                    rwdts_shard_handle,
                    rwdts_shard_handle_ref_gi,
                    rwdts_shard_handle_unref_gi);

/*
 *  README notes for rwdts_shard_key_init_keyspec() 
 *  Convert the reg->keyspec into a lookup tree, or expand existing tree
 *  if nodes already exists.
 *  NOTE:This function only creates the lookup shard tree.
 *  This should be followed by shard_chunk add before using this shard
 *  for any query operations
 *  Each keyspec path element is split into element-id shard and keys shards.
 *  the primary key is  element id shard, followed by secondary key etc.
 *  path element with wildcard key is also represented by a child.
 *  Shards with chunks at any level represents a registered keyspec in shard tree
 *  from the top. 
 *  For example /a[key1=1]/b[key2=2] is represented by 5 shards.
 *  Root-Shard->a-Shard->key1-Shard->b-Shard->key2-Shard.
 *  When we add /a[key1=1]/c[key3=3] to this shard tree key1-shard will have 
 *  two children b-shard and c-shard.
 */
rwdts_shard_handle_t *
rwdts_shard_init_keyspec(rwdts_api_t *apih, rw_keyspec_path_t *keyspec,
                         int idx,
                         rwdts_shard_t **parent,
                         rwdts_shard_flavor flavor,
                         union rwdts_shard_flavor_params_u *flavor_params,
                         enum rwdts_shard_keyfunc_e hashfunc,
                         union rwdts_shard_keyfunc_params_u *keyfunc_params,
                         enum rwdts_shard_anycast_policy_e anypolicy)
{
  int i, depth, num_keys, key_index; 
  size_t length;
  rwdts_shard_t *shard_tab = NULL, *parent_shard;
  const rw_keyspec_entry_t *pe;
  rwdts_shard_elemkey_t elemvalue;
  uint8_t *keyptr; 
  char *ks_str = NULL;
  rw_status_t rs;
  // local vars  end

  RW_ASSERT(keyspec);
  
  if (!keyspec || !parent) { return NULL; } 

  depth = rw_keyspec_path_get_depth(keyspec);
  RW_ASSERT(depth);
  if (!depth) { return NULL; }
  
  if (idx >= 0 && (flavor != RW_DTS_SHARD_FLAVOR_NULL)) {
    depth = idx;
  }

  if (*parent == NULL) {
    *parent = DTS_APIH_MALLOC0_TYPE(apih,RW_DTS_DTS_MEMORY_TYPE_SHARD,
                                    sizeof(rwdts_shard_t), rwdts_shard_t);

    RW_ASSERT(*parent);
    if (!*parent) { return NULL; } 
    (*parent)->key_index = 0;
    (*parent)->pe_index = 0;
    (*parent)->key_type = RWDTS_SHARD_ELMID;
  }

  parent_shard = *parent;
  // start building the shard lookup tree.
  // for each keyspec path element, lookup/create shard child
  // for each key in  path element, lookup/create shard child

  for (i=0; i<depth; i++) { // loop for each path element in keyspec
    // get next path element from keyspec 
    pe = rw_keyspec_path_get_path_entry(keyspec, i);
    RW_ASSERT(pe);
    if (!pe) { return NULL; }

    // get element-id - key is ns-id,tag,type for next level
    rw_schema_ns_and_tag_t element = 
          rw_keyspec_entry_get_schema_id(pe);

    memcpy(&elemvalue.key[0], &element.ns,4);
    memcpy(&elemvalue.key[4], &element.tag,4);
 
    // lookup next level using element id of path element 
    HASH_FIND(hh_shard, parent_shard->children, &elemvalue.key, 
              sizeof(elemvalue.key), shard_tab);

    // create if not present
    if (!shard_tab) {
      // alloc shard
      shard_tab = (rwdts_shard_t*) DTS_APIH_MALLOC0_TYPE(apih,RW_DTS_DTS_MEMORY_TYPE_SHARD,
                                                         sizeof(rwdts_shard_t), rwdts_shard_t);
      RW_ASSERT(shard_tab);
      if (!shard_tab) {return NULL; }

      // alloc key 
      shard_tab->key = (uint8_t *)RW_MALLOC0(sizeof(elemvalue.key));
      RW_ASSERT(shard_tab->key);

      if (!shard_tab->key) { return NULL; };
      shard_tab->key_len = sizeof(elemvalue.key);
      memcpy(shard_tab->key, &elemvalue.key, sizeof(elemvalue.key));
    
      HASH_ADD_KEYPTR(hh_shard, parent_shard->children, shard_tab->key, 
                      shard_tab->key_len, shard_tab);
      shard_tab->parent = parent_shard;
      rwdts_shard_handle_ref(parent_shard, __PRETTY_FUNCTION__, __LINE__);
      parent_shard->pe_index = i;
      parent_shard->key_type = RWDTS_SHARD_ELMID;
      shard_tab->pe_index = i; /* init the pe index for now */
    }
    // go down one level advance parent_shard ptr
    parent_shard = shard_tab;
    // traverse down the tree through each key, 
    // some of the keys might be wildcard
    // 
    num_keys = rw_keyspec_entry_num_keys(pe);
    for (key_index=0;key_index<num_keys;key_index++) {
      // get the key ptr and length from path element
     //  keyptr = rw_keyspec_entry_get_key_ptr(pe, NULL, key_index, &length);
      keyptr = rw_keyspec_entry_get_key_packed(pe, NULL, key_index, &length);
      // if keyptr is not NULL this key is not wildcard
      // search for next level for this key,create if not present
      if (keyptr) {
        HASH_FIND(hh_shard, parent_shard->children, keyptr, 
              length, shard_tab);
        if (!shard_tab) {
          shard_tab = DTS_APIH_MALLOC0_TYPE(apih,RW_DTS_DTS_MEMORY_TYPE_SHARD,
                                            sizeof(rwdts_shard_t), rwdts_shard_t);
          RW_ASSERT(shard_tab);
          if (!shard_tab) { return NULL; } // TBD go to undo what we did so far
          shard_tab->key = keyptr; 
          shard_tab->key_len = length; 
          HASH_ADD_KEYPTR(hh_shard, parent_shard->children, shard_tab->key, 
                      shard_tab->key_len, shard_tab);
          shard_tab->parent = parent_shard; 
          rwdts_shard_handle_ref(parent_shard, __PRETTY_FUNCTION__, __LINE__);
    
          if (parent_shard->key_index) {
            RW_ASSERT(parent_shard->key_index == key_index);
          }
          parent_shard->key_index = key_index;
          parent_shard->pe_index = i;
          parent_shard->key_type = RWDTS_SHARD_KEY;
          shard_tab->pe_index = i; /* init the pe index for now */
        } // if (!shard_tab)
        else { 
          RW_FREE(keyptr);
        }
      } // if (keyptr)
      else {// key is null - wildcard key
        shard_tab = parent_shard->wildcard_shard;
        if (!shard_tab) { // allocate if not already present
          shard_tab = DTS_APIH_MALLOC0_TYPE(apih,RW_DTS_DTS_MEMORY_TYPE_SHARD,
                                            sizeof(rwdts_shard_t), rwdts_shard_t);
          RW_ASSERT(shard_tab);
          if (!shard_tab) { return NULL; } // TBD go to undo what we did so far
          parent_shard->wildcard_shard = shard_tab;
          shard_tab->parent = parent_shard; 
          rwdts_shard_handle_ref(parent_shard, __PRETTY_FUNCTION__, __LINE__);
          shard_tab->key = NULL;
          shard_tab->key_len = 0;
          // add a chunk with empty records for wildcard matching
          // at this level for NULL flavor. 
          shard_tab->flavor = flavor;
          shard_tab->hash_func = hashfunc;
          if (flavor_params) {
            shard_tab->flavor_params = *flavor_params;
          }
          if (keyfunc_params) {
            shard_tab->keyfunc_params = *keyfunc_params;
          }
          shard_tab->anycast_policy = anypolicy;
          shard_tab->pe_index = i;
          if (shard_tab->flavor == RW_DTS_SHARD_FLAVOR_NULL) {
            rwdts_shard_chunk_info_t *chunk = 
                rwdts_shard_add_chunk(shard_tab, NULL);
            RW_ASSERT(chunk);
          }
        }
        if (idx < 0 || (flavor != RW_DTS_SHARD_FLAVOR_NULL)) {
          goto ret;
        }
      }
      // traverse dwon the tree until we process all the keys in this path element
      parent_shard = shard_tab;
    } // for(key_index=0...
  } // for each path element 

  // Return the leaf shard take ref 
  // updates the parameters. We can update flavor only if it is NULLL shard 
ret:
  if (shard_tab->flavor != flavor) {
    rs = rwdts_shard_update(shard_tab, flavor, flavor_params);
    RW_ASSERT(rs == RW_STATUS_SUCCESS);
  }
  shard_tab->flavor = flavor;
  shard_tab->hash_func = hashfunc;
  if (flavor_params) {
    shard_tab->flavor_params = *flavor_params;
  }
  if (keyfunc_params) {
    shard_tab->keyfunc_params = *keyfunc_params;
  }
  shard_tab->anycast_policy = anypolicy;
  // take ref
  rwdts_shard_handle_ref(shard_tab, __PRETTY_FUNCTION__, __LINE__);
  UNUSED(ks_str);
//
// debug stuff start
//rw_keyspec_path_get_new_print_buffer(keyspec, NULL , NULL, &ks_str);
//printf("Shard Init root=%p,ks=%s\n",*parent,ks_str);
//free(ks_str); 
// debug stuff end

  return shard_tab;
}

rwdts_shard_handle_t*
rwdts_shard_key_init(rwdts_member_reg_handle_t reg,
                     rwdts_xact_t *xact,
                     uint32_t flags,
                     int      idx,
                     rwdts_shard_t **parent, 
                     rwdts_shard_flavor flavor,
                     union rwdts_shard_flavor_params_u *flavor_params,
                     enum rwdts_shard_keyfunc_e hashfunc,
                     union rwdts_shard_keyfunc_params_u *keyfunc_params,
                     enum rwdts_shard_anycast_policy_e anypolicy,
                     rwdts_member_getnext_cb prepare)
{
  rwdts_member_registration_t* r = (rwdts_member_registration_t *)reg;
  rwdts_api_t* apih = r->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t);
  rwdts_shard_t *shard = rwdts_shard_init_keyspec(r->apih,
                                                  ((rwdts_member_registration_t*)reg)->keyspec,
                                                  idx, parent, flavor, flavor_params,
                                                  hashfunc, keyfunc_params, anypolicy);
  RW_ASSERT(shard);

#if 0
  if (shard) {
    shard->flags |= flags;
    if (flags & RWDTS_FLAG_SUBSCRIBER) {
     shard->sub_reg = realloc(shard->sub_reg, ((shard->n_sub_reg+1)*sizeof(rwdts_member_reg_handle_t)));
     RW_ASSERT(shard->sub_reg);
     shard->sub_reg[shard->n_sub_reg]=reg;
     shard->n_sub_reg += 1;
    }
    else {
     shard->pub_reg = realloc(shard->pub_reg, ((shard->n_pub_reg+1)*sizeof(rwdts_member_reg_handle_t)));
     RW_ASSERT(shard->pub_reg);
     shard->pub_reg[shard->n_pub_reg]=reg;
     shard->n_pub_reg += 1;
    }
  }
#endif
  shard->apih = apih;
  return shard;
} 

rwdts_shard_handle_t * 
rwdts_shard_handle_new(rwdts_member_reg_handle_t reg,
                                       uint32_t flags,
                                       rwdts_shard_flavor flavor,
                                       union rwdts_shard_flavor_params_u *flavor_params,
                                       enum rwdts_shard_keyfunc_e hashfunc,
                                       union rwdts_shard_keyfunc_params_u *keyfunc_params,
                                       enum rwdts_shard_anycast_policy_e anypolicy,
                                       rwdts_member_getnext_cb prepare,
                                       GDestroyNotify dtor)
{
  rwdts_member_registration_t* r = (rwdts_member_registration_t *)reg;
  rwdts_api_t* apih = r->apih;
  RW_ASSERT_TYPE(apih, rwdts_api_t); 
  rwdts_shard_handle_t *shard_handle = rwdts_shard_key_init(reg, NULL, flags, -1, &apih->rootshard,
                                                            flavor, flavor_params,
                                                            hashfunc, keyfunc_params, anypolicy, prepare);

  RW_ASSERT(!r->shard);
  r->shard = shard_handle;
  return shard_handle;
}

rw_status_t
rwdts_shard_handle_print(rwdts_shard_handle_t* shard_handle)
{
  return RW_STATUS_SUCCESS;
}

//
// Add ident flavor chunk with supplied key. TBD:minikey
// to the specific shard handle passed. 
//

rwdts_shard_chunk_info_t *
rwdts_shard_add_chunk_ident(rwdts_shard_handle_t *shard,
                      uint8_t *keyin, int keylen)
{
  rwdts_shard_t *shard_tab = shard;
  rwdts_shard_chunk_info_t *shard_chunk = NULL;

  RW_ASSERT(shard_tab);
  if (!shard_tab) { return NULL; }  
  
  // IDENT flavor key for the chunk will be just minikey 
  HASH_FIND(hh_chunk, shard_tab->shard_chunks, keyin, (size_t)keylen, shard_chunk);
  if (!shard_chunk) {
    shard_chunk = DTS_APIH_MALLOC0_TYPE(shard->apih, RW_DTS_DTS_MEMORY_TYPE_SHARD_CHUNK,
                                        sizeof(rwdts_shard_chunk_info_t), rwdts_shard_chunk_info_t);
    RW_ASSERT(shard_chunk);
    if (!shard_chunk) {
      return NULL;
    }
    memcpy(&shard_chunk->chunk_key.identkey.key, keyin, keylen);
    shard_chunk->chunk_key.identkey.keylen = keylen;
    HASH_ADD_KEYPTR(hh_chunk, shard_tab->shard_chunks, 
      &shard_chunk->chunk_key.identkey.key, keylen, shard_chunk);
    shard_chunk->parent = shard;
    rwdts_shard_handle_ref(shard_tab, __PRETTY_FUNCTION__, __LINE__);
    rwdts_shard_chunk_ref(shard_tab, __PRETTY_FUNCTION__, __LINE__);
  }
  return shard_chunk;
}

//
// Add null flavor chunk. autogenerated chunk-id is the key
// for null flavor chunk 
// to the specific shard handle passed. 
//

rwdts_shard_chunk_info_t *
rwdts_shard_add_chunk_null(rwdts_shard_handle_t *shard)
{
  rwdts_shard_t *shard_tab = shard;
  rwdts_shard_chunk_info_t *shard_chunk = NULL;
  rwdts_chunk_id_t chunk_id;

  RW_ASSERT(shard_tab);
  if (!shard_tab) { return NULL; }  
  chunk_id = shard->next_chunk_id; 
  shard->next_chunk_id++;

  shard_chunk = DTS_APIH_MALLOC0_TYPE(shard->apih, RW_DTS_DTS_MEMORY_TYPE_SHARD_CHUNK,
                                      sizeof(rwdts_shard_chunk_info_t), rwdts_shard_chunk_info_t);
  RW_ASSERT(shard_chunk);
  if (!shard_chunk) {
    return NULL;
  }
  shard_chunk->chunk_id = chunk_id;
  HASH_ADD_KEYPTR(hh_chunk, shard_tab->shard_chunks, 
           &shard_chunk->chunk_id, sizeof(shard_chunk->chunk_id), shard_chunk);
  shard_chunk->parent = shard;
  rwdts_shard_handle_ref(shard_tab, __PRETTY_FUNCTION__, __LINE__);
  rwdts_shard_chunk_ref(shard_tab, __PRETTY_FUNCTION__, __LINE__);
  return shard_chunk;
}

//
// add chunk range : TBD replace with interval tree
// 
rwdts_shard_chunk_info_t *
rwdts_shard_add_chunk_range(rwdts_shard_handle_t *shard, int64_t start, int64_t end)
{
  rwdts_shard_t *shard_tab = shard;
  rwdts_shard_chunk_info_t *shard_chunk = NULL;
  rwdts_chunk_id_t chunk_id;

  RW_ASSERT(shard_tab);
  if (!shard_tab) { return NULL; }  
  chunk_id = shard->next_chunk_id; 
  shard->next_chunk_id++;

  shard_chunk = DTS_APIH_MALLOC0_TYPE(shard->apih, RW_DTS_DTS_MEMORY_TYPE_SHARD_CHUNK,
                                      sizeof(rwdts_shard_chunk_info_t), rwdts_shard_chunk_info_t);
  RW_ASSERT(shard_chunk);
  if (!shard_chunk) {
    return NULL;
  }
  shard_chunk->chunk_id = chunk_id;
  shard_chunk->chunk_key.rangekey.start_range = start;
  shard_chunk->chunk_key.rangekey.end_range = end;
  HASH_ADD_KEYPTR(hh_chunk, shard_tab->shard_chunks, 
           &shard_chunk->chunk_id, sizeof(shard_chunk->chunk_id), shard_chunk);
  shard_chunk->parent = shard;
  rwdts_shard_handle_ref(shard_tab, __PRETTY_FUNCTION__, __LINE__);
  rwdts_shard_chunk_ref(shard_tab, __PRETTY_FUNCTION__, __LINE__);
  return shard_chunk;
}


//
// delete specific chunk id based on the minikey passed
//

rw_status_t
rwdts_shard_delete_chunk_ident(rwdts_shard_handle_t *shard,
                      union rwdts_shard_flavor_params_u *params)
{
  rwdts_shard_t *shard_tab = shard;
  rwdts_shard_chunk_info_t *shard_chunk = NULL;

  RW_ASSERT(shard_tab);
  if (!shard_tab) {
    return RW_STATUS_FAILURE;
  }  
  RW_ASSERT(params);
  if (!params) {
    return RW_STATUS_FAILURE;
  }

  // IDENT flavor key for the chunk will be just minikey 
  HASH_FIND(hh_chunk, shard_tab->shard_chunks, params->identparams.keyin, 
            params->identparams.keylen, shard_chunk);
  if (!shard_chunk) {
    return RW_STATUS_FAILURE;
  }  

  HASH_DELETE(hh_chunk, shard_tab->shard_chunks, shard_chunk);
  rwdts_shard_handle_unref(shard_tab, __PRETTY_FUNCTION__, __LINE__);
  return RW_STATUS_SUCCESS;
}

//
// delete specific chunk id for null flavor
//

rw_status_t
rwdts_shard_delete_chunk_null(rwdts_shard_handle_t *shard,  union rwdts_shard_flavor_params_u *params)
{
  rwdts_shard_t *shard_tab = shard;
  rwdts_shard_chunk_info_t *shard_chunk = NULL;

  RW_ASSERT(shard_tab);
  if (!shard_tab) {
    return RW_STATUS_FAILURE;
  }  

  // NULL flavor key for the chunk is chunk_id;
  HASH_FIND(hh_chunk, shard_tab->shard_chunks, &(params->nullparams.chunk_id), 
            sizeof(params->nullparams.chunk_id), shard_chunk);
  if (!shard_chunk) {
    return RW_STATUS_FAILURE;
  }  

  HASH_DELETE(hh_chunk, shard_tab->shard_chunks, shard_chunk);
  rwdts_shard_handle_unref(shard_tab, __PRETTY_FUNCTION__, __LINE__);
  return RW_STATUS_SUCCESS;
}

//
// rwdts_range_key_cmpr compare the incomping key with a range flavor chunk key range
// TBD interval tree 
//
bool
rwdts_range_key_cmpr(rwdts_shard_chunk_info_t *chunk, union rwdts_shard_flavor_params_u *params)
{

  RW_ASSERT(chunk); 
  RW_ASSERT(params);
  if (chunk == NULL)  { return FALSE;}
  if (params == NULL) { return FALSE;}

  if ((chunk->chunk_key.rangekey.start_range == params->range.start_range) &&
      (chunk->chunk_key.rangekey.end_range == params->range.end_range)) {
    return TRUE;
  }
  return FALSE;
}


bool
rwdts_range_key_cmpr_ks(rwdts_shard_chunk_info_t *chunk, rw_keyspec_path_t *ks)
{
  rw_schema_minikey_opaque_t minikey;

  RW_ASSERT(chunk);
  if (chunk == NULL)  { return FALSE;}
  if (ks == NULL) { return FALSE;}

  rw_keyspec_entry_t* pe = (rw_keyspec_entry_t*)rw_keyspec_path_get_path_entry(ks, chunk->parent->pe_index);
  rw_status_t rs = rw_schema_mk_get_from_path_entry(pe, &minikey);
  RW_ASSERT(rs == RW_STATUS_SUCCESS);

  return (rwdts_shard_range_key_compare(pe, &minikey, &chunk->chunk_key));
}

//
// TBD replace with interval tree implementation 
//
rw_status_t
rwdts_shard_delete_chunk_range(rwdts_shard_handle_t *shard,  union rwdts_shard_flavor_params_u *params)
{
  rwdts_shard_t *shard_tab = shard;
  rwdts_shard_chunk_info_t *shard_chunk = NULL, *tmp_chunk = NULL;

  RW_ASSERT(shard_tab);
  if (!shard_tab) {
    return RW_STATUS_FAILURE;
  }  
  RW_ASSERT(params);
  if (!params) {
    return RW_STATUS_FAILURE;
  }

  // implement interval tree?
  HASH_ITER(hh_chunk, shard_tab->shard_chunks, shard_chunk, tmp_chunk) {
    if (rwdts_range_key_cmpr(shard_chunk, params) == TRUE) {
      HASH_DELETE(hh_chunk, shard_tab->shard_chunks, shard_chunk);
      rwdts_shard_handle_unref(shard_tab, __PRETTY_FUNCTION__, __LINE__);
      return RW_STATUS_SUCCESS;
    }
  }
  return RW_STATUS_FAILURE;
}


//
// add chunk function - calls respective flavor add chunk 
//
rwdts_shard_chunk_info_t *
rwdts_shard_add_chunk(rwdts_shard_handle_t *shard, union rwdts_shard_flavor_params_u *params) 
{
  rwdts_shard_chunk_info_t *info = NULL;
  
  RW_ASSERT(shard);
  if (!shard) { return NULL; }

  switch (shard->flavor) {
    case RW_DTS_SHARD_FLAVOR_IDENT:
     if (params->identparams.keylen) {
       info = rwdts_shard_add_chunk_ident(shard, 
                                          (uint8_t*)&params->identparams.keyin[0], params->identparams.keylen);
     }
     break;
    case RW_DTS_SHARD_FLAVOR_NULL:
      info = rwdts_shard_add_chunk_null(shard);
      if (info
          && params){
        params->nullparams.chunk_id = info->chunk_id;
      }
      break;
    case RW_DTS_SHARD_FLAVOR_RANGE:
      info = rwdts_shard_add_chunk_range(shard, params->range.start_range, params->range.end_range);
      break;
    default:
      RW_ASSERT_MESSAGE(0, "flavor %d not implemented\n", shard->flavor); 
  }
  return info;
}

rw_status_t
rwdts_shard_delete_chunk(rwdts_shard_handle_t *shard,
                          union rwdts_shard_flavor_params_u *params)
{
  if (!shard) { return RW_STATUS_FAILURE;}

  switch (shard->flavor) {
    case RW_DTS_SHARD_FLAVOR_IDENT:
      if (params) {
        return(rwdts_shard_delete_chunk_ident(shard, params));
      }
     break;
    case RW_DTS_SHARD_FLAVOR_NULL:
      return(rwdts_shard_delete_chunk_null(shard,params)); 
     break;
    case RW_DTS_SHARD_FLAVOR_RANGE:
      return(rwdts_shard_delete_chunk_range(shard,params)); 
     break;
   default:
    RW_ASSERT_MESSAGE(0, "flavor %d not implemented\n", shard->flavor); 
  }
  return RW_STATUS_FAILURE;
}
    
rw_status_t 
rwdts_shard_ident_key(rw_keyspec_path_t *ks, rwdts_shard_t *shard, union rwdts_shard_flavor_params_u *params)
{
  const rw_keyspec_entry_t *pe;
  int peindex;
  size_t keylen;
  uint8_t *key;

    peindex = shard->pe_index;
    pe = rw_keyspec_path_get_path_entry(ks, peindex);
    if (pe) {
      key = rw_keyspec_entry_get_keys_packed(pe, NULL, &keylen);
      RW_ASSERT(key); // TBD remove debugging only
      if (key) {
        memcpy(&params->identparams.keyin[0], key, keylen);
        params->identparams.keylen = keylen;
        RW_FREE(key);
        return RW_STATUS_SUCCESS;
      }
    }
  return RW_STATUS_FAILURE;
}

//
// Take shard handle and match the chunk.
//
rwdts_shard_t *
rwdts_shard_chunk_match(rwdts_shard_handle_t *shard_handle,
                  rw_keyspec_path_t *ks,
                  union rwdts_shard_flavor_params_u *params, 
                  rwdts_shard_chunk_info_t **chunk_out,
                  rwdts_chunk_id_t *chunk_id_out)

{
  rwdts_shard_t *shard_tab = (rwdts_shard_t *) shard_handle;
  rwdts_shard_chunk_info_t *shard_chunk = NULL, *tmp_chunk;
  
  RW_ASSERT(shard_handle);
  if (chunk_out)    { *chunk_out = (rwdts_shard_chunk_info_t*)NULL; }
  if (chunk_id_out) { *chunk_id_out = 0; }

  if (!shard_handle) { return NULL; }


  switch (shard_tab->flavor) {
     union rwdts_shard_flavor_params_u local_params;
     rw_status_t st;
    case RW_DTS_SHARD_FLAVOR_IDENT:
      st = rwdts_shard_ident_key(ks, shard_tab, &local_params);
      if (st == RW_STATUS_SUCCESS) {
        HASH_FIND(hh_chunk, shard_tab->shard_chunks, &local_params.identparams.keyin, 
                  local_params.identparams.keylen, shard_chunk);
      }
    break;
    case RW_DTS_SHARD_FLAVOR_NULL:
      HASH_ITER(hh_chunk, shard_tab->shard_chunks, shard_chunk, tmp_chunk) {
        if (shard_chunk) {
          break; /* now return first chunk, iter should be used to get all */
        }
      }
    break;
    case RW_DTS_SHARD_FLAVOR_RANGE:
      HASH_ITER(hh_chunk, shard_tab->shard_chunks, shard_chunk, tmp_chunk) {
        if (shard_chunk) {
          /* compare the range */
          if (rwdts_range_key_cmpr_ks(shard_chunk, ks)) {
            break;
          }
        }
      }
    break;
    default:
      RW_CRASH();
      return NULL;
  }

  if (shard_chunk) {
    if (chunk_out)    { *chunk_out = shard_chunk;}
    if (chunk_id_out) { *chunk_id_out = shard_chunk->chunk_id;}
    return shard_tab;
  }
  return NULL;
}

//
// We can search the keyspec from root if we have the full keyspec
// Walk through the shard tree and return the matching shard
// and chunk- key_in used for matching the chunk for IDENT flavor
// TBD make key_in as union of keys based on flavor.
//
rwdts_shard_t *
rwdts_shard_match_keyspec(rwdts_shard_handle_t *rootshard,
                          rw_keyspec_path_t *keyspec,
                          union rwdts_shard_flavor_params_u * params,
                          rwdts_shard_chunk_info_t **chunk_out,
                          rwdts_chunk_id_t *chunk_id_out)
{

  int i, depth, num_keys, key_index;
  size_t length;
  rwdts_shard_t *shard_tab = NULL, *parent_shard, *matched_shard = NULL;
  const rw_keyspec_entry_t *pe;
  rwdts_shard_elemkey_t elemvalue;
  uint8_t *keyptr; 
  char *ks_str;
  // local vars  end

  RW_ASSERT(keyspec);
  
  if (!keyspec || !rootshard) { goto match_failed; } 

  depth = rw_keyspec_path_get_depth(keyspec);
  RW_ASSERT(depth);
  if (!depth) { goto match_failed; }
  parent_shard = rootshard;
  // for each path element in the keyspec, lookup shard child
  // for each key in  path element, lookup shard child

  for (i=0; i<depth; i++) { // loop for each path element in keyspec
    // get next path element from keyspec 
    pe = rw_keyspec_path_get_path_entry(keyspec, i);
    RW_ASSERT(pe);
    if (!pe) { goto match_failed; }

    // get element-id - key is ns-id,tag,type for next level
    rw_schema_ns_and_tag_t element = 
          rw_keyspec_entry_get_schema_id(pe);

    memcpy(&elemvalue.key[0], &element.ns,4);
    memcpy(&elemvalue.key[4], &element.tag,4);
 
    // lookup next level using element id of path element 
    HASH_FIND(hh_shard, parent_shard->children, &elemvalue.key, 
              sizeof(elemvalue.key), shard_tab);

    // Return NULL not present
    if (shard_tab == NULL) {
      goto match_failed; 
    }

    // If there are chunks in this shard, try to match them
    if (shard_tab->shard_chunks) {
      matched_shard = rwdts_shard_chunk_match(shard_tab, keyspec, params,
                        chunk_out, chunk_id_out);
    }
    if (matched_shard) {
      return matched_shard;
    }

    // go down one level advance parent_shard ptr
    parent_shard = shard_tab;
    // traverse down the tree through each key, 
    // some of the keys might be wildcard
    // 
    num_keys = rw_keyspec_entry_num_keys(pe);
    for (key_index=0;key_index<num_keys;key_index++) {
      // get the key ptr and length from path element
      // keyptr = rw_keyspec_entry_get_key_ptr(pe, NULL, key_index, &length);
      keyptr = rw_keyspec_entry_get_key_packed(pe, NULL, key_index, &length);
      if (!keyptr) { // wildcard 
        if (parent_shard->wildcard_shard &&
            parent_shard->wildcard_shard->shard_chunks) {
            matched_shard = rwdts_shard_chunk_match(parent_shard->wildcard_shard,
                            keyspec, params,chunk_out, chunk_id_out); // TBD make it work for all flavors
        }
        if (matched_shard) { 
          return matched_shard; 
        }

      }
      // search for next level for this key
      HASH_FIND(hh_shard, parent_shard->children, keyptr,length, shard_tab);
      RW_FREE(keyptr);
      keyptr = NULL;
      if (!shard_tab) {
      // search for any wildcard match first
        if (parent_shard->wildcard_shard &&
            parent_shard->wildcard_shard->shard_chunks) {
            matched_shard = rwdts_shard_chunk_match(parent_shard->wildcard_shard,
                            keyspec, params, chunk_out, chunk_id_out); // TBD make it work for all flavors
        }
        if (matched_shard) { 
          return matched_shard; 
        }
        else {
          goto match_failed;
        }
      }
     

      // search for specific match in chunks
      if (shard_tab && shard_tab->shard_chunks) {
        matched_shard = rwdts_shard_chunk_match(shard_tab,
                          keyspec, params, chunk_out, chunk_id_out); // TBD make it work for all flavors
        if (matched_shard) { return matched_shard; }
      }
      parent_shard = shard_tab;// go to next path element
    } // traverse dwon the tree until we process all the keys in this path element
      parent_shard = shard_tab;// go to next path element
  }

  if (shard_tab) { return shard_tab;}

match_failed:
  UNUSED(ks_str);
//rw_keyspec_path_get_new_print_buffer(keyspec, NULL , NULL, &ks_str);
//printf("Shard match FAILED root=%p,ks=%s\n",rootshard,ks_str);
//free(ks_str); 
  return NULL;
}
//
// internal routine to match most specific shard like longest prefix match
// 
rwdts_shard_t *
rwdts_shard_match_full_keyspec(rwdts_shard_handle_t *rootshard,
                          rw_keyspec_path_t *keyspec,
                          union rwdts_shard_flavor_params_u *params,
                          rwdts_shard_chunk_info_t **chunk_out,
                          rwdts_chunk_id_t *chunk_id_out)
{

  int i, depth, num_keys, key_index;
  size_t length;
  rwdts_shard_t *shard_tab = NULL, *parent_shard;
  const rw_keyspec_entry_t *pe;
  rwdts_shard_elemkey_t elemvalue;
  uint8_t *keyptr; 
  char *ks_str;
  rwdts_shard_t *last_match = NULL;
  // local vars  end

  RW_ASSERT(keyspec);
  
  if (!keyspec || !rootshard) { goto match_failed; } 

  depth = rw_keyspec_path_get_depth(keyspec);
  RW_ASSERT(depth);
  if (!depth) { goto match_failed; }

  parent_shard = rootshard;
  // for each path element in the keyspec, lookup shard child
  // for each key in  path element, lookup shard child

  for (i=0; i<depth; i++) { // loop for each path element in keyspec
    // get next path element from keyspec 
    pe = rw_keyspec_path_get_path_entry(keyspec, i);
    RW_ASSERT(pe);
    if (!pe) { goto match_failed; }

    // get element-id - key is ns-id,tag,type for next level
    rw_schema_ns_and_tag_t element = 
          rw_keyspec_entry_get_schema_id(pe);

    memcpy(&elemvalue.key[0], &element.ns,4);
    memcpy(&elemvalue.key[4], &element.tag,4);
 
    // lookup next level using element id of path element 
    HASH_FIND(hh_shard, parent_shard->children, &elemvalue.key, 
              sizeof(elemvalue.key), shard_tab);

    // Return NULL not present
    if (shard_tab == NULL) {
      goto match_failed; 
    }


    // go down one level advance parent_shard ptr
    parent_shard = shard_tab;
    // traverse down the tree through each key, 
    // some of the keys might be wildcard
    // 
    num_keys = rw_keyspec_entry_num_keys(pe);
    for (key_index=0;key_index<num_keys;key_index++) {
      // get the key ptr and length from path element
      // keyptr = rw_keyspec_entry_get_key_ptr(pe, NULL, key_index, &length);
      keyptr = rw_keyspec_entry_get_key_packed(pe, NULL, key_index, &length);

      if (!keyptr && parent_shard->wildcard_shard) { // wildcard 
        shard_tab =  parent_shard->wildcard_shard;
        last_match = shard_tab;
      }
      else if (!keyptr && !parent_shard->wildcard_shard) { 
        goto match_failed; 
      }
      else {
      // search for next level for this key
        HASH_FIND(hh_shard, parent_shard->children, keyptr,length, shard_tab);
        RW_FREE(keyptr);
        keyptr = NULL;
        if (!shard_tab) {
          goto match_failed;
        }
      }
      parent_shard = shard_tab;// go to next path element
    } // traverse dwon the tree until we process all the keys in this path element
    parent_shard = shard_tab;// go to next path element
  }

  if (shard_tab) { return shard_tab;}
  UNUSED(ks_str);
match_failed:
  if (last_match) return last_match;
//rw_keyspec_path_get_new_print_buffer(keyspec, NULL , NULL, &ks_str);
//printf("Shard match FAILED root=%p,ks=%s\n",rootshard,ks_str);
//free(ks_str); 
  return NULL;
}

rw_status_t
rwdts_shard_member_delete_member_null(rwdts_shard_t *shard,
                                      bool publisher, rwdts_chunk_id_t chunk_id,
                                      char *msgpath)
{
  rwdts_shard_chunk_info_t *chunk;
  rwdts_chunk_member_info_t *mbr_elem = NULL;

  RW_ASSERT_TYPE(shard, rwdts_shard_t);

  HASH_FIND(hh_chunk, shard->shard_chunks, &chunk_id, sizeof(chunk_id), chunk);
  if (!chunk) { // kick the bucket
    return RW_STATUS_FAILURE;
  }
  
  if (publisher) {
    HASH_FIND(hh_mbr_record, chunk->elems.member_info.pub_mbr_info, msgpath, strlen(msgpath), mbr_elem);
    if (mbr_elem) {
      HASH_DELETE(hh_mbr_record, chunk->elems.member_info.pub_mbr_info, mbr_elem);
      RW_FREE(mbr_elem);
      RW_ASSERT(chunk->elems.member_info.n_pub_reg);
      chunk->elems.member_info.n_pub_reg--;
    }
  } else {
    HASH_FIND(hh_mbr_record, chunk->elems.member_info.sub_mbr_info, msgpath, strlen(msgpath), mbr_elem);
    if (mbr_elem) {
      HASH_DELETE(hh_mbr_record, chunk->elems.member_info.sub_mbr_info, mbr_elem);
      RW_FREE(mbr_elem);
      RW_ASSERT(chunk->elems.member_info.n_sub_reg);
      chunk->elems.member_info.n_sub_reg--;
    }
  }
  
  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_shard_rtr_delete_member_null(rwdts_shard_t *shard, char*member, bool pub) 
{
  rwdts_shard_chunk_info_t *chunk=NULL, *tmp_chunk;
  rwdts_chunk_rtr_info_t *rtr_elem = NULL;

  HASH_ITER(hh_chunk, shard->shard_chunks, chunk, tmp_chunk) {
    if (pub) {
      rtr_elem = NULL;
      HASH_FIND(hh_rtr_record, chunk->elems.rtr_info.pub_rtr_info, member, strlen(member), rtr_elem);
      if (rtr_elem) {
        HASH_DELETE(hh_rtr_record, chunk->elems.rtr_info.pub_rtr_info, rtr_elem);
        RW_FREE(rtr_elem);
        RW_ASSERT(chunk->elems.rtr_info.n_pubrecords);
        chunk->elems.rtr_info.n_pubrecords--;
      }
    }
    else {
      rtr_elem = NULL;
      HASH_FIND(hh_rtr_record, chunk->elems.rtr_info.sub_rtr_info, member, strlen(member), rtr_elem);
      if (rtr_elem) {
        HASH_DELETE(hh_rtr_record, chunk->elems.rtr_info.sub_rtr_info, rtr_elem);
        RW_FREE(rtr_elem);
        RW_ASSERT(chunk->elems.rtr_info.n_subrecords);
        chunk->elems.rtr_info.n_subrecords--;
      }
    }
  }
  return RW_STATUS_SUCCESS;
}

rw_status_t
rwdts_shard_member_delete_element(rwdts_shard_t *shard,
                                  bool publisher, rwdts_chunk_id_t chunk_id, char *msgpath)
{
  if (!shard) {
    return RW_STATUS_FAILURE;
  }
  RW_ASSERT(msgpath);
  switch(shard->flavor) {
    case RW_DTS_SHARD_FLAVOR_NULL:
    case RW_DTS_SHARD_FLAVOR_IDENT:
    case RW_DTS_SHARD_FLAVOR_RANGE: // TBD make more efficient delete function for other flavors
      return(rwdts_shard_member_delete_member_null(shard, publisher,
                                                   chunk_id,
                                                   msgpath));
      break;
    default:
      RW_ASSERT_MESSAGE(0,"delete element not implemented for flavor = %d\n", shard->flavor);
  }
  return RW_STATUS_FAILURE;
}

rw_status_t
rwdts_shard_rtr_delete_element(rwdts_shard_t *shard, char *member, bool pub)
{
  switch(shard->flavor) {
    case RW_DTS_SHARD_FLAVOR_NULL:
    case RW_DTS_SHARD_FLAVOR_IDENT:
     return(rwdts_shard_rtr_delete_member_null(shard, member,pub));
    break;
    default:
    //RW_ASSERT_MESSAGE(0,"delete element not implemented for flavor = %d\n", shard->flavor);
    break; 
  }
  return RW_STATUS_FAILURE;
}

//
// removes ref from shard when we have shard ptr available.
// Otherwise use rwdts_shard_deinit_keyspec if we have only
// keyspec and root of shard available
//
rw_status_t
rwdts_shard_deref(rwdts_shard_t *shard)
{
  if (!shard ) {
    return RW_STATUS_FAILURE;
  }
  rwdts_shard_chunk_unref(shard, __PRETTY_FUNCTION__, __LINE__);
  rwdts_shard_handle_unref(shard, __PRETTY_FUNCTION__, __LINE__);
  return RW_STATUS_SUCCESS;
}

//
// deref the shard when we do not have direct pointer to shard
// and only keyspec and parent available
//
rw_status_t
rwdts_shard_deinit_keyspec(rw_keyspec_path_t *keyspec,
                             rwdts_shard_t **parent)
{
  rwdts_shard_t *shard;
  rwdts_shard_chunk_info_t *chunk = NULL;
  rwdts_chunk_id_t chunk_id;

  shard = 
#if 1
    rwdts_shard_match_full_keyspec(*parent, keyspec,
                           NULL, &chunk, &chunk_id);
#else
    rwdts_shard_match_keyspec(*parent, keyspec,
                           NULL, &chunk, &chunk_id);
#endif

  if (!shard ) {
    return RW_STATUS_FAILURE;
  }
  rwdts_shard_chunk_unref(shard, __PRETTY_FUNCTION__, __LINE__);
  rwdts_shard_handle_unref(shard, __PRETTY_FUNCTION__, __LINE__);
  return RW_STATUS_SUCCESS;
}

static rw_status_t
rwdts_shard_member_create_element_ident(rwdts_shard_t *shard, 
                                        rwdts_shard_chunk_info_t *shard_chunk, 
                                        rwdts_chunk_member_info_t *mbr_info,
                                        bool publisher, 
                                        rwdts_chunk_id_t *chunkid, 
                                        char *msgpath)
{
  rwdts_chunk_member_info_t *mbr_record;
  RW_ASSERT(mbr_info);
  RW_ASSERT(msgpath);
  if (!mbr_info) { return RW_STATUS_FAILURE; }
  RW_ASSERT_TYPE(shard, rwdts_shard_t);
  RW_ASSERT_TYPE(shard_chunk, rwdts_shard_chunk_info_t);

#if 0 //TBD:implement RW_VALID_TYPE macro 
  if (!RW_VALID_TYPE(shard, rwdts_shard_t)) { return RW_STATUS_FAILURE; }
#endif

  // chunk not added? return failure
  if (!shard_chunk) { return RW_STATUS_FAILURE; }

  uint32_t n_records = shard_chunk->elems.member_info.n_pub_reg + shard_chunk->elems.member_info.n_sub_reg;

  RW_ASSERT(n_records < RWDTS_SHARD_CHUNK_SZ);
  if (n_records >= RWDTS_SHARD_CHUNK_SZ) {
    return RW_STATUS_FAILURE;; 
  } 

  *chunkid = shard_chunk->chunk_id; 
  //Memory leak below.. if there is an app which does multiple registrations to the same path then we
  //would be adding elements to the hash below without looking up. We need reference counting for number
  //adds..

  mbr_record = RW_MALLOC0(sizeof(rwdts_chunk_member_info_t));
  RW_ASSERT(mbr_info);
  if (mbr_info == NULL) { 
    return RW_STATUS_FAILURE; 
  }
  mbr_record->member =  mbr_info->member;
  mbr_record->flags =  mbr_info->flags;
  strncpy(mbr_record->msgpath, msgpath, sizeof(mbr_record->msgpath));
    
  size_t keylen = strlen(mbr_record->msgpath);
  if (keylen > sizeof(mbr_record->msgpath)) {
    keylen = sizeof(mbr_record->msgpath);
  }
  RW_ASSERT(keylen);
  if (publisher) {
    HASH_ADD_KEYPTR(hh_mbr_record, shard_chunk->elems.member_info.pub_mbr_info, 
                    mbr_record->msgpath, keylen, mbr_record);
   shard_chunk->elems.member_info.n_pub_reg++;
  } else {
    HASH_ADD_KEYPTR(hh_mbr_record, shard_chunk->elems.member_info.sub_mbr_info, 
                    mbr_record->msgpath, keylen, mbr_record);
   shard_chunk->elems.member_info.n_sub_reg++;
  } 
  return RW_STATUS_SUCCESS; 
}

//
// create an element for IDENT flavor in the passed chunk.
// TBD when chunk has max element ?
// 

rw_status_t
rwdts_shard_rts_create_element_ident(rwdts_shard_t *shard, 
                                     rwdts_shard_chunk_info_t *shard_chunk, 
                                     rwdts_chunk_rtr_info_t *rtr_info,
                                     bool publisher, 
                                     rwdts_chunk_id_t *chunkid, 
                                     uint32_t *membid, char *msgpath)
{
  rwdts_chunk_rtr_info_t *rtr_record;
  RW_ASSERT(rtr_info);
  RW_ASSERT(msgpath);
  if (!rtr_info) { return RW_STATUS_FAILURE; }
  RW_ASSERT_TYPE(shard, rwdts_shard_t);
  RW_ASSERT_TYPE(shard_chunk, rwdts_shard_chunk_info_t);

  // chunk not added? return failure
  if (!shard_chunk) { return RW_STATUS_FAILURE; }

  uint32_t n_records = shard_chunk->elems.rtr_info.n_pubrecords + shard_chunk->elems.rtr_info.n_subrecords;

  RW_ASSERT(n_records < RWDTS_SHARD_CHUNK_SZ);
  if (n_records >= RWDTS_SHARD_CHUNK_SZ) {
    return RW_STATUS_FAILURE;; 
  } 

  *chunkid = shard_chunk->chunk_id; 
  //Memory leak below.. if there is an app which does multiple registrations to the same path then we
  //would be adding elements to the hash below without looking up. We need reference counting for number
  //adds..
  rtr_record = RW_MALLOC0(sizeof(rwdts_chunk_rtr_info_t));
  RW_ASSERT(rtr_info);
  if (rtr_info == NULL) { 
    return RW_STATUS_FAILURE; 
  }
  rtr_record->member =  rtr_info->member;
  rtr_record->flags =  rtr_info->flags;
  strncpy(rtr_record->msgpath, msgpath, sizeof(rtr_record->msgpath));
    
  size_t keylen = strlen(rtr_record->msgpath);
  if (keylen > sizeof(rtr_record->msgpath)) {
    keylen = sizeof(rtr_record->msgpath);
  }
  RW_ASSERT(keylen);
  if (publisher) {
    HASH_ADD_KEYPTR(hh_rtr_record, shard_chunk->elems.rtr_info.pub_rtr_info, 
                    rtr_record->msgpath, keylen, rtr_record);
   *membid = shard_chunk->elems.rtr_info.n_pubrecords;
   shard_chunk->elems.rtr_info.n_pubrecords++;
  } else {
    HASH_ADD_KEYPTR(hh_rtr_record, shard_chunk->elems.rtr_info.sub_rtr_info, 
                    rtr_record->msgpath, keylen, rtr_record);
   *membid = shard_chunk->elems.rtr_info.n_subrecords;
   shard_chunk->elems.rtr_info.n_subrecords++;
  } 
  return RW_STATUS_SUCCESS; 
}

static rw_status_t
rwdts_shard_member_create_element_null(rwdts_shard_t *shard, rwdts_chunk_member_info_t *mbr_info,
                                              bool publisher, rwdts_chunk_id_t *chunkid, char *msgpath)
{
  rwdts_shard_chunk_info_t *shard_chunk;

  RW_ASSERT(mbr_info);
  if (!mbr_info) { return RW_STATUS_FAILURE; }
  RW_ASSERT_TYPE(shard, rwdts_shard_t);

  // chunk not added? return failure
  if (!shard->next_chunk_id) { return RW_STATUS_FAILURE; }

  // add record to last chunk, if last chunk is full add new chunk 
  rwdts_chunk_id_t chunk_id = shard->next_chunk_id-1;
  HASH_FIND(hh_chunk, shard->shard_chunks, &chunk_id, sizeof(chunk_id), shard_chunk);

  if (!shard_chunk) { // kick the bucket
    RW_ASSERT_MESSAGE(0, "\nchunk not found for id = %lu]\n", chunk_id);
    return RW_STATUS_FAILURE;
  }

  // add new chunk if the last one is full

  uint32_t n_records = shard_chunk->elems.member_info.n_sub_reg + shard_chunk->elems.member_info.n_pub_reg;

  if (n_records >= RWDTS_SHARD_CHUNK_SZ) {
    shard_chunk = rwdts_shard_add_chunk_null(shard);
    if (shard_chunk == NULL) {
    // log event - max chunk per shard limit exceeded?
      return RW_STATUS_FAILURE;
    }
  } 

  *chunkid = chunk_id;

  //Memory leak below.. if there is an app which does multiple registrations to the same path then we
  //would be adding elements to the hash below without looking up. We need reference counting for number
  //adds..AKKI tbd
  rwdts_chunk_member_info_t *mbr_record = RW_MALLOC0(sizeof(rwdts_chunk_member_info_t));
  RW_ASSERT(mbr_record);
  if (mbr_record == NULL) {
    return RW_STATUS_FAILURE;
  }
  mbr_record->member =  mbr_info->member;
  mbr_record->flags =   mbr_info->flags;
  strncpy(mbr_record->msgpath, msgpath, sizeof(mbr_record->msgpath));

  size_t keylen = strlen(mbr_record->msgpath);
  if (keylen > sizeof(mbr_record->msgpath)) {
    keylen = sizeof(mbr_record->msgpath);
  }

  RW_ASSERT(keylen);
  if (publisher) {
    HASH_ADD_KEYPTR(hh_mbr_record, shard_chunk->elems.member_info.pub_mbr_info,
                    mbr_record->msgpath, keylen, mbr_record);
   shard_chunk->elems.member_info.n_pub_reg++;
  } else {
    HASH_ADD_KEYPTR(hh_mbr_record, shard_chunk->elems.member_info.sub_mbr_info,
                    mbr_record->msgpath, keylen, mbr_record);
   shard_chunk->elems.member_info.n_sub_reg++;
  }

  return RW_STATUS_SUCCESS; 
}


//
// Add an elemnet to null shard flavor chunk.
// accepts shard in which we are adding (not root shard)
// and the rtr record struct
//
static rw_status_t
rwdts_shard_rts_create_element_null(rwdts_shard_t *shard, rwdts_chunk_rtr_info_t *rtr_info,
                                    bool publisher, rwdts_chunk_id_t *chunkid, uint32_t *membid, char *msgpath)
{
  rwdts_shard_chunk_info_t *shard_chunk;

  RW_ASSERT(rtr_info);
  if (!rtr_info) { return RW_STATUS_FAILURE; }
  RW_ASSERT_TYPE(shard, rwdts_shard_t);

  // chunk not added? return failure
  if (!shard->next_chunk_id) { return RW_STATUS_FAILURE; }

  // add record to last chunk, if last chunk is full add new chunk 
  rwdts_chunk_id_t chunk_id = shard->next_chunk_id-1;
  HASH_FIND(hh_chunk, shard->shard_chunks, &chunk_id, sizeof(chunk_id), shard_chunk);

  if (!shard_chunk) { // kick the bucket
    RW_ASSERT_MESSAGE(0, "\nchunk not found for id = %lu]\n", chunk_id);
    return RW_STATUS_FAILURE;
  }

  // add new chunk if the last one is full

  uint32_t n_records = shard_chunk->elems.rtr_info.n_pubrecords + shard_chunk->elems.rtr_info.n_subrecords;

  if (n_records >= RWDTS_SHARD_CHUNK_SZ) {
    shard_chunk = rwdts_shard_add_chunk_null(shard);
    if (shard_chunk == NULL) {
    // log event - max chunk per shard limit exceeded?
      return RW_STATUS_FAILURE;
    }
  } 

  *chunkid = chunk_id;
  //Memory leak below.. if there is an app which does multiple registrations to the same path then we
  //would be adding elements to the hash below without looking up. We need reference counting for number
  //adds..
  rwdts_chunk_rtr_info_t *rtr_record = RW_MALLOC0(sizeof(rwdts_chunk_rtr_info_t));
  RW_ASSERT(rtr_record);
  if (rtr_record == NULL) {
    return RW_STATUS_FAILURE;
  }
  rtr_record->member =  rtr_info->member;
  rtr_record->flags =  rtr_info->flags;
  strncpy(rtr_record->msgpath, msgpath, sizeof(rtr_record->msgpath));

  size_t keylen = strlen(rtr_record->msgpath);
  if (keylen > sizeof(rtr_record->msgpath)) {
    keylen = sizeof(rtr_record->msgpath);
  }

  RW_ASSERT(keylen);
  if (publisher) {
    HASH_ADD_KEYPTR(hh_rtr_record, shard_chunk->elems.rtr_info.pub_rtr_info,
                    rtr_record->msgpath, keylen, rtr_record);
   *membid = shard_chunk->elems.rtr_info.n_pubrecords;
   shard_chunk->elems.rtr_info.n_pubrecords++;
  } else {
    HASH_ADD_KEYPTR(hh_rtr_record, shard_chunk->elems.rtr_info.sub_rtr_info,
                    rtr_record->msgpath, keylen, rtr_record);
   *membid = shard_chunk->elems.rtr_info.n_subrecords;
   shard_chunk->elems.rtr_info.n_subrecords++;
  }

  return RW_STATUS_SUCCESS; 
}


rw_status_t
rwdts_shard_member_update_element_null(rwdts_shard_t *shard, rwdts_chunk_member_info_t *mbr_info,
                                       bool publisher, rwdts_chunk_id_t chunk_id, char *msgpath)
{
  rwdts_shard_chunk_info_t *shard_chunk;
  rwdts_chunk_member_info_t *mbr_record = NULL; 

  RW_ASSERT(mbr_info);
  if (!mbr_info) { return RW_STATUS_FAILURE; }
  RW_ASSERT_TYPE(shard, rwdts_shard_t);

  // add record to last chunk, if last chunk is full add new chunk 
  HASH_FIND(hh_chunk, shard->shard_chunks, &chunk_id, sizeof(chunk_id), shard_chunk);

  if (!shard_chunk) { // kick the bucket
    return RW_STATUS_FAILURE;
  }

  if (publisher) {
    HASH_FIND(hh_mbr_record, shard_chunk->elems.member_info.pub_mbr_info, msgpath, strlen(msgpath), mbr_record);
    if (!mbr_record) {
      return RW_STATUS_FAILURE;
    }
    mbr_record->flags |= mbr_info->flags;
  } else {
    HASH_FIND(hh_mbr_record, shard_chunk->elems.member_info.sub_mbr_info, msgpath, strlen(msgpath), mbr_record);
    if (!mbr_record) {
      return RW_STATUS_FAILURE;
    }
    mbr_record->flags |= mbr_info->flags;
  }
  return RW_STATUS_SUCCESS;
}

//
// Update the elemnet to null shard flavor chunk.
// accepts shard in which we are adding (not root shard)
// and the rtr record struct
//
rw_status_t
rwdts_shard_rts_update_element_null(rwdts_shard_t *shard, rwdts_chunk_rtr_info_t *rtr_info,
                               bool publisher, rwdts_chunk_id_t chunk_id, uint32_t membid, char *msgpath)
{
  rwdts_shard_chunk_info_t *shard_chunk;
  rwdts_chunk_rtr_info_t *rtr_record = NULL; 

  RW_ASSERT(rtr_info);
  if (!rtr_info) { return RW_STATUS_FAILURE; }
  RW_ASSERT_TYPE(shard, rwdts_shard_t);

  RW_ASSERT(membid <= RWDTS_SHARD_CHUNK_SZ);

  // add record to last chunk, if last chunk is full add new chunk 
  HASH_FIND(hh_chunk, shard->shard_chunks, &chunk_id, sizeof(chunk_id), shard_chunk);

  if (!shard_chunk) { // kick the bucket
    return RW_STATUS_FAILURE;
  }

  if (publisher) {
    HASH_FIND(hh_rtr_record, shard_chunk->elems.rtr_info.pub_rtr_info, msgpath, strlen(msgpath), rtr_record);
    if (!rtr_record) {
      return RW_STATUS_FAILURE;
    }
    rtr_record->flags |= rtr_info->flags;
  } else {
    HASH_FIND(hh_rtr_record, shard_chunk->elems.rtr_info.sub_rtr_info, msgpath, strlen(msgpath), rtr_record);
    if (!rtr_record) {
      return RW_STATUS_FAILURE;
    }
    rtr_record->flags |= rtr_info->flags;
  }
  return RW_STATUS_SUCCESS;
}

static rw_status_t
rwdts_shard_rts_create_element_range(rwdts_shard_t *shard, rwdts_chunk_rtr_info_t *rtr_info,
                                     bool publisher, rwdts_chunk_id_t *chunkid, uint32_t *membid, char *msgpath)
{
  rwdts_shard_chunk_info_t *shard_chunk;

  RW_ASSERT(rtr_info);
  if (!rtr_info) { return RW_STATUS_FAILURE; }
  RW_ASSERT_TYPE(shard, rwdts_shard_t);

  // chunk not added? return failure
  if (!shard->next_chunk_id) { return RW_STATUS_FAILURE; }

  // add record to last chunk, if last chunk is full add new chunk 
  rwdts_chunk_id_t chunk_id = shard->next_chunk_id-1;
  HASH_FIND(hh_chunk, shard->shard_chunks, &chunk_id, sizeof(chunk_id), shard_chunk);

  if (!shard_chunk) { // kick the bucket
    RW_ASSERT_MESSAGE(0, "\nchunk not found for id = %lu]\n", chunk_id);
    return RW_STATUS_FAILURE;
  }

  // add new chunk if the last one is full

  uint32_t n_records = shard_chunk->elems.rtr_info.n_pubrecords + shard_chunk->elems.rtr_info.n_subrecords;

  if (n_records >= RWDTS_SHARD_CHUNK_SZ) {
    shard_chunk = rwdts_shard_add_chunk_range(shard, 
                       shard_chunk->chunk_key.rangekey.start_range, 
                       shard_chunk->chunk_key.rangekey.end_range);
    if (shard_chunk == NULL) {
    // log event - max chunk per shard limit exceeded?
      return RW_STATUS_FAILURE;
    }
  } 

  *chunkid = chunk_id;

  rwdts_chunk_rtr_info_t *rtr_record = RW_MALLOC0(sizeof(rwdts_chunk_rtr_info_t));
  RW_ASSERT(rtr_info);
  if (rtr_info == NULL) {
    return RW_STATUS_FAILURE;
  }
  rtr_record->member =  rtr_info->member;
  rtr_record->flags =  rtr_info->flags;
  strncpy(rtr_record->msgpath, msgpath, sizeof(rtr_record->msgpath));

  size_t keylen = strlen(rtr_record->msgpath);
  if (keylen > sizeof(rtr_record->msgpath)) {
    keylen = sizeof(rtr_record->msgpath);
  }

  RW_ASSERT(keylen);
  if (publisher) {
    HASH_ADD_KEYPTR(hh_rtr_record, shard_chunk->elems.rtr_info.pub_rtr_info,
                    rtr_record->msgpath, keylen, rtr_record);
   *membid = shard_chunk->elems.rtr_info.n_pubrecords;
   shard_chunk->elems.rtr_info.n_pubrecords++;
  } else {
    HASH_ADD_KEYPTR(hh_rtr_record, shard_chunk->elems.rtr_info.sub_rtr_info,
                    rtr_record->msgpath, keylen, rtr_record);
   *membid = shard_chunk->elems.rtr_info.n_subrecords;
   shard_chunk->elems.rtr_info.n_subrecords++;
  }

  return RW_STATUS_SUCCESS; 
}


static rw_status_t
rwdts_shard_member_create_element_range(rwdts_shard_t *shard, rwdts_chunk_member_info_t *mbr_info,
                                        bool publisher, rwdts_chunk_id_t *chunkid, char *msgpath)
{
  rwdts_shard_chunk_info_t *shard_chunk;

  RW_ASSERT(mbr_info);
  if (!mbr_info) { return RW_STATUS_FAILURE; }
  RW_ASSERT_TYPE(shard, rwdts_shard_t);

#if 0 //TBD:implement RW_VALID_TYPE macro 
  if (!RW_VALID_TYPE(shard, rwdts_shard_t)) { return NULL; }
#endif

  // chunk not added? return failure
  if (!shard->next_chunk_id) { return RW_STATUS_FAILURE; }

  // add record to last chunk, if last chunk is full add new chunk 
  rwdts_chunk_id_t chunk_id = shard->next_chunk_id-1;
  HASH_FIND(hh_chunk, shard->shard_chunks, &chunk_id, sizeof(chunk_id), shard_chunk);

  if (!shard_chunk) { 
    RW_ASSERT_MESSAGE(0, "\nchunk not found for id = %lu]\n", chunk_id);
    return RW_STATUS_FAILURE;
  }

  // add new chunk if the last one is full

  uint32_t n_records = shard_chunk->elems.member_info.n_pub_reg + shard_chunk->elems.member_info.n_sub_reg;

  if (n_records >= RWDTS_SHARD_CHUNK_SZ) {
    shard_chunk = rwdts_shard_add_chunk_range(shard, 
                       shard_chunk->chunk_key.rangekey.start_range, 
                       shard_chunk->chunk_key.rangekey.end_range);
    if (shard_chunk == NULL) {
    // log event - max chunk per shard limit exceeded?
      return RW_STATUS_FAILURE;
    }
  } 

  *chunkid = chunk_id;

  rwdts_chunk_member_info_t *mbr_record = RW_MALLOC0(sizeof(rwdts_chunk_member_info_t));
  RW_ASSERT(mbr_record);
  if (mbr_record == NULL) {
    return RW_STATUS_FAILURE;
  }
  mbr_record->member =  mbr_info->member;
  mbr_record->flags =  mbr_info->flags;
  strncpy(mbr_record->msgpath, msgpath, sizeof(mbr_record->msgpath));

  size_t keylen = strlen(mbr_record->msgpath);
  if (keylen > sizeof(mbr_record->msgpath)) {
    keylen = sizeof(mbr_record->msgpath);
  }

  RW_ASSERT(keylen);
  if (publisher) {
    HASH_ADD_KEYPTR(hh_mbr_record, shard_chunk->elems.member_info.pub_mbr_info,
                    mbr_record->msgpath, keylen, mbr_record);
   shard_chunk->elems.member_info.n_pub_reg++;
  } else {
    HASH_ADD_KEYPTR(hh_mbr_record, shard_chunk->elems.member_info.sub_mbr_info,
                    mbr_record->msgpath, keylen, mbr_record);
   shard_chunk->elems.member_info.n_sub_reg++;
  }

  return RW_STATUS_SUCCESS; 
}

rw_status_t
rwdts_shard_rts_update_element(rwdts_shard_t *shard, rwdts_chunk_rtr_info_t *rtr_info,
                               bool publisher, rwdts_chunk_id_t chunk_id, uint32_t membid, char *msgpath)
{
  switch(shard->flavor) {
    case RW_DTS_SHARD_FLAVOR_NULL:
      return(rwdts_shard_rts_update_element_null(shard, rtr_info, publisher, chunk_id, membid, msgpath)); 
    break;
    default:
    RW_CRASH();
  }
  return RW_STATUS_FAILURE;
}
//
// create a router record element in the chunk.
// 
rw_status_t
rwdts_shard_rts_create_element(rwdts_shard_t *shard, rwdts_shard_chunk_info_t *chunk, 
                               rwdts_chunk_rtr_info_t *rtr_info, bool publisher, 
                               rwdts_chunk_id_t *chunk_id, uint32_t *membid, char *msgpath)
{
  RW_ASSERT_TYPE(shard, rwdts_shard_t);

  switch (shard->flavor) {
    case RW_DTS_SHARD_FLAVOR_NULL:
      return(rwdts_shard_rts_create_element_null(shard, rtr_info, publisher, chunk_id, membid, msgpath));
      break;

    case RW_DTS_SHARD_FLAVOR_IDENT:
      return(rwdts_shard_rts_create_element_ident(shard, chunk, rtr_info, publisher, chunk_id, membid, msgpath));
      break;

    case RW_DTS_SHARD_FLAVOR_RANGE:
      return(rwdts_shard_rts_create_element_range(shard, rtr_info, publisher, chunk_id, membid, msgpath));
      break;

   default:
    RW_ASSERT_MESSAGE(0, "create element for flavor %d not implemented", shard->flavor);
  }
  return RW_STATUS_FAILURE;
} 
  
rw_status_t
rwdts_shard_member_create_element(rwdts_shard_t *shard, rwdts_shard_chunk_info_t *chunk, 
                                  rwdts_chunk_member_info_t *mbr_info, bool publisher, 
                                  rwdts_chunk_id_t *chunk_id, char *msgpath)
{
  RW_ASSERT_TYPE(shard, rwdts_shard_t);

  switch (shard->flavor) {
    case RW_DTS_SHARD_FLAVOR_NULL:
      return(rwdts_shard_member_create_element_null(shard, mbr_info, publisher, chunk_id, msgpath));
      break;

    case RW_DTS_SHARD_FLAVOR_IDENT:
      return(rwdts_shard_member_create_element_ident(shard, chunk, mbr_info, publisher, chunk_id,  msgpath));
      break;

    case RW_DTS_SHARD_FLAVOR_RANGE:
      return(rwdts_shard_member_create_element_range(shard, mbr_info, publisher, chunk_id,  msgpath));
      break;

   default:
    RW_ASSERT_MESSAGE(0, "create element for flavor %d not implemented", shard->flavor);
  }
  return RW_STATUS_FAILURE;
} 


rwdts_shard_chunk_iter_t *
rwdts_shard_chunk_iter_create(rwdts_shard_t *shard, rw_keyspec_path_t *keyspec)
{
  rwdts_shard_chunk_iter_t *iter = NULL;
  const rw_keyspec_entry_t *pe;
  uint8_t *keyptr;
  size_t length;

  iter = DTS_APIH_MALLOC0_TYPE(shard->apih, RW_DTS_DTS_MEMORY_TYPE_SHARD_CHUNK_ITER,
                               sizeof(rwdts_shard_chunk_iter_t),
                               rwdts_shard_chunk_iter_t);
  RW_ASSERT(iter);
  if (!iter) { return NULL; }
 
  iter->shard = shard;
  rwdts_shard_handle_ref(shard, __PRETTY_FUNCTION__, __LINE__);

  switch(shard->flavor) {
    case RW_DTS_SHARD_FLAVOR_NULL:
      iter->chunk_id = 0;
      iter->chunk = shard->shard_chunks;
      break;
    case RW_DTS_SHARD_FLAVOR_IDENT:

      pe = rw_keyspec_path_get_path_entry(keyspec, shard->pe_index);
      if (!pe) { 
        iter->shard = NULL;
        rwdts_shard_handle_unref(shard, __PRETTY_FUNCTION__, __LINE__);
        DTS_APIH_FREE_TYPE(shard->apih, RW_DTS_DTS_MEMORY_TYPE_SHARD_CHUNK_ITER,
                           iter, rwdts_shard_chunk_iter_t);
        return NULL; 
      }
      keyptr = rw_keyspec_entry_get_key_packed(pe, NULL, shard->pe_index, &length);
      if (keyptr == NULL) {
        iter->shard = NULL;
        rwdts_shard_handle_unref(shard, __PRETTY_FUNCTION__, __LINE__);
        DTS_APIH_FREE_TYPE(shard->apih, RW_DTS_DTS_MEMORY_TYPE_SHARD_CHUNK_ITER,
                           iter, rwdts_shard_chunk_iter_t);
        return NULL; 
      }
      HASH_FIND(hh_chunk, shard->shard_chunks, 
          keyptr, length, iter->chunk);
      RW_FREE(keyptr);
      keyptr = NULL;
      if (iter->chunk == NULL) {
        iter->shard = NULL;
        rwdts_shard_handle_unref(shard, __PRETTY_FUNCTION__, __LINE__);
        DTS_APIH_FREE_TYPE(shard->apih, RW_DTS_DTS_MEMORY_TYPE_SHARD_CHUNK_ITER,
                           iter, rwdts_shard_chunk_iter_t);
        return NULL; 
      }
      break;
     default:
       RW_ASSERT_MESSAGE(0, "flavor iter %d not implemented\n", shard->flavor);
  } 
  return iter;
}


//
// chunk iterate create - to iterate through all the chunks in a shard
//
rwdts_shard_chunk_iter_t *
rwdts_shard_chunk_iter_create_keyspec(rwdts_shard_t *rootshard, rw_keyspec_path_t *keyspec, union rwdts_shard_flavor_params_u *params)
{
  rwdts_shard_chunk_iter_t *iter;
  rwdts_shard_t *shard;
  rwdts_shard_chunk_info_t *chunk;
  rwdts_chunk_id_t chunk_id;

  shard = rwdts_shard_match_keyspec(rootshard,
                          keyspec,
                          params,
                          &chunk,
                          &chunk_id);

  if (!shard || !chunk) { return NULL; }
  
  iter = DTS_APIH_MALLOC0_TYPE(shard->apih, RW_DTS_DTS_MEMORY_TYPE_SHARD_CHUNK_ITER,
                               sizeof(rwdts_shard_chunk_iter_t), rwdts_shard_chunk_iter_t);
  RW_ASSERT(iter);
  if (!iter) { return NULL; }
 
  iter->shard = shard;
  rwdts_shard_handle_ref(shard, __PRETTY_FUNCTION__, __LINE__);
  iter->chunk_id = chunk_id;
  iter->chunk = chunk;
  return iter;
}

//
// chunk iterate next, to iterate all the chunks in a shard
// iter will be freed from this function if the return 
// value is FALSE
//
bool rwdts_rtr_shard_chunk_iter_next(rwdts_shard_chunk_iter_t *iter, 
            rwdts_chunk_rtr_info_t **records_out, int32_t *n_records_out)
{
  rwdts_shard_chunk_info_t *shard_chunk = NULL;
  rwdts_api_t *apih = iter->shard->apih;
  
  RW_ASSERT_TYPE(iter, rwdts_shard_chunk_iter_t);
  RW_ASSERT(records_out);
  RW_ASSERT(n_records_out);

// implement RW_VALID_TYPE and enable below line
//if (!RW_VALID_TYPE(iter, rwdts_shard_chunk_iter_t)) { return FALSE;}

  if (!records_out || !n_records_out) {
    if (iter->shard) {
      rwdts_shard_handle_unref(iter->shard, __PRETTY_FUNCTION__, __LINE__);
    }
    DTS_APIH_FREE_TYPE(apih, RW_DTS_DTS_MEMORY_TYPE_SHARD_CHUNK_ITER,
                       iter, rwdts_shard_chunk_iter_t);
    return FALSE;
  } 
  switch (iter->shard->flavor) {
    case RW_DTS_SHARD_FLAVOR_IDENT:
      if (iter->chunk) {
       *records_out = iter->chunk->elems.rtr_info.pub_rtr_info;
       *n_records_out = iter->chunk->elems.rtr_info.n_pubrecords;
       iter->chunk = NULL;
       return TRUE;
      }
      else {
         rwdts_shard_handle_unref(iter->shard, __PRETTY_FUNCTION__, __LINE__);
         DTS_APIH_FREE_TYPE(apih, RW_DTS_DTS_MEMORY_TYPE_SHARD_CHUNK_ITER,
                            iter, rwdts_shard_chunk_iter_t);
         return FALSE;
      }
      break;
    case RW_DTS_SHARD_FLAVOR_NULL:
       *records_out = iter->chunk->elems.rtr_info.pub_rtr_info;
       *n_records_out = iter->chunk->elems.rtr_info.n_pubrecords;

       // advance cursor to next chunk in the shard.
       iter->chunk_id++;
       HASH_FIND(hh_chunk, iter->shard->shard_chunks, 
          &iter->chunk_id, sizeof(iter->chunk_id), shard_chunk);

       if (!shard_chunk) { // last chunk end of iteration
         rwdts_shard_handle_unref(iter->shard, __PRETTY_FUNCTION__, __LINE__);
         DTS_APIH_FREE_TYPE(apih, RW_DTS_DTS_MEMORY_TYPE_SHARD_CHUNK_ITER,
                            iter, rwdts_shard_chunk_iter_t);
         return FALSE;
       }

      iter->chunk = shard_chunk;
      return TRUE;
      break;

    default:
      RW_ASSERT_MESSAGE(0, "iter for shard flavor %d not implemented", iter->shard->flavor);
  }
  if (!shard_chunk) {
    rwdts_shard_handle_unref(iter->shard, __PRETTY_FUNCTION__, __LINE__);
    DTS_APIH_FREE_TYPE(apih, RW_DTS_DTS_MEMORY_TYPE_SHARD_CHUNK_ITER,
                       iter, rwdts_shard_chunk_iter_t);
  }
  return FALSE;
}

//
// shard iterate create - to iterate through all the shards to 
// retrive the data
//
rwdts_shard_iter_t *
rwdts_shard_iter_create(rwdts_shard_t *rootshard, rw_keyspec_path_t *keyspec,
                        union rwdts_shard_flavor_params_u *params)
{
  rwdts_shard_iter_t *iter;
  rw_status_t status;

  iter = DTS_APIH_MALLOC0_TYPE(rootshard->apih, RW_DTS_DTS_MEMORY_TYPE_SHARD_ITER,
                               sizeof(rwdts_shard_iter_t), rwdts_shard_iter_t);
  RW_ASSERT(iter);
  if (!iter) { return NULL; }
 
  iter->chunk_iter = rwdts_shard_chunk_iter_create_keyspec(rootshard, keyspec, params);
  if (iter->chunk_iter == NULL) {
    DTS_APIH_FREE_TYPE(rootshard->apih, RW_DTS_DTS_MEMORY_TYPE_SHARD_ITER,
                       iter, rwdts_shard_iter_t);
    return NULL;
  }
  iter->shard = iter->chunk_iter->shard; // store the current active shard
  iter->params  = *params;
  status = rw_keyspec_path_create_dup(keyspec, NULL , &iter->keyspec); 
  RW_ASSERT(status == RW_STATUS_SUCCESS);
  if (status != RW_STATUS_SUCCESS) {
    DTS_APIH_FREE_TYPE(rootshard->apih, RW_DTS_DTS_MEMORY_TYPE_SHARD_ITER,
                       iter, rwdts_shard_iter_t);
    return NULL;
  }

  rwdts_shard_handle_ref(iter->shard, __PRETTY_FUNCTION__, __LINE__);
  return iter;
}

// shard iterate next, to iterate through next shard/chunk data matched 
// iter will be freed from this function if the return 
// value is FALSE
//
bool rwdts_rtr_shard_iter_next(rwdts_shard_iter_t *iter, 
            rwdts_chunk_rtr_info_t **records_out, int32_t *n_records_out)
{
  rwdts_shard_t *shard;
  rwdts_shard_chunk_info_t *chunk;
  rwdts_chunk_id_t chunk_id;
  bool match;
  rwdts_api_t *apih;
  
  RW_ASSERT_TYPE(iter, rwdts_shard_iter_t);
  RW_ASSERT(records_out);
  RW_ASSERT(n_records_out);

// implement RW_VALID_TYPE and enable below line
//if (!RW_VALID_TYPE(iter, rwdts_shard_iter_t)) { return FALSE;}

  match = rwdts_rtr_shard_chunk_iter_next(iter->chunk_iter, records_out, n_records_out);
  if (match ) {
    return TRUE;
  }
  //All chunks in current shard iterated, advance to next shard
  shard = rwdts_shard_match_keyspec(iter->shard,
                          iter->keyspec,
                          &iter->params,
                          &chunk,
                          &chunk_id);
  if (iter->shard) {
    rwdts_shard_handle_unref(iter->shard, __PRETTY_FUNCTION__, __LINE__);
  }
  iter->shard = shard;

  if (!shard) {
    apih = NULL;
    // done with matching all records
    DTS_APIH_FREE_TYPE(apih, RW_DTS_DTS_MEMORY_TYPE_SHARD_ITER,
                       iter, rwdts_shard_iter_t);
    return FALSE;
  }
  apih = shard->apih;
  
  rwdts_shard_handle_ref(iter->shard, __PRETTY_FUNCTION__, __LINE__);

  iter->chunk_iter = rwdts_shard_chunk_iter_create_keyspec(iter->shard, 
                     iter->keyspec, &iter->params);
  if (!iter->chunk_iter) {
    if (iter->shard) {
      rwdts_shard_handle_unref(iter->shard, __PRETTY_FUNCTION__, __LINE__);
    }
    DTS_APIH_FREE_TYPE(apih, RW_DTS_DTS_MEMORY_TYPE_SHARD_ITER,
                       iter, rwdts_shard_iter_t);
    return FALSE;
  }
  
  match = rwdts_rtr_shard_chunk_iter_next(iter->chunk_iter, records_out, n_records_out);
  if (match ) {
    return TRUE;
  }
 
  if (iter->shard) {
    rwdts_shard_handle_unref(iter->shard, __PRETTY_FUNCTION__, __LINE__);
  }
  DTS_APIH_FREE_TYPE(apih, RW_DTS_DTS_MEMORY_TYPE_SHARD_ITER,iter, rwdts_shard_iter_t);
  return FALSE;
 
}


// 
// return the shards with data for the path element children
//
bool
rwdts_shard_match_pathelem(rwdts_shard_t **head, const rw_keyspec_entry_t *pe, rwdts_shard_t ***shards, uint32_t *n_shards) 
{
  rwdts_shard_t *shard_tab, *shard_ptr, *wd_shard;
  rwdts_shard_elemkey_t elemvalue;
  int key_index, num_keys, shard_index = 0;
  uint8_t *keyptr;
  size_t length;

  shard_ptr = *head;
  *n_shards = 0;
  // match the child shard based on the path element 
  // get element-id - key is ns-id,tag,type for next level
  rw_schema_ns_and_tag_t element = 
        rw_keyspec_entry_get_schema_id(pe);

  memcpy(&elemvalue.key[0], &element.ns,4);
  memcpy(&elemvalue.key[4], &element.tag,4);
 
  // lookup next level using element id of path element 
  HASH_FIND(hh_shard, shard_ptr->children, &elemvalue.key, 
            sizeof(elemvalue.key), shard_tab);
  // no more matches then return. No changes for head
  if (!shard_tab) {
    return false;
  }

  wd_shard = shard_tab->wildcard_shard;
  // If there is a wildcard shard with data add this to the result
  if ((wd_shard != NULL)  && wd_shard->shard_chunks) { 
     *shards = realloc(*shards, ((shard_index+1) * sizeof(rwdts_shard_t *)));
     (*shards)[shard_index] = wd_shard;  
     shard_index += 1;
  } 
  // if chunk is present add the result
  if (shard_tab->shard_chunks) {
     *shards = realloc(*shards, ((shard_index+1) * sizeof(rwdts_shard_t *)));
     (*shards)[shard_index] = shard_tab;
     shard_index += 1;
  }
 // lookup the shards for each key  
  num_keys = rw_keyspec_entry_num_keys(pe);
  shard_ptr = shard_tab;

  for (key_index=0;key_index<num_keys;key_index++) {
    // get the key ptr and length from path element
    keyptr = rw_keyspec_entry_get_key_packed(pe, NULL, key_index, &length);

    if (!keyptr) {
      *head = shard_ptr;
      *n_shards = shard_index;
      if (!shard_index) {
        return false;
      } else {
        return true;
      }
    }
    // go to next key shard
    HASH_FIND(hh_shard, shard_ptr->children, keyptr, length, shard_tab);
    RW_FREE(keyptr);

    if (!shard_tab) { 
      *head = shard_ptr;
      *n_shards = shard_index;
      if (!shard_index) {
        return false;
      } else {
        return true;
      }
    }
 
    wd_shard = shard_tab->wildcard_shard;
   // add wildcard shard to results if present
   if ((wd_shard != NULL) && wd_shard->shard_chunks) { 
      *shards = realloc(*shards, ((shard_index+1) * sizeof(rwdts_shard_t *)));
      (*shards)[shard_index] = wd_shard;
      shard_index += 1;
    } 
    // add shard_tab to result if data present
    if (shard_tab->shard_chunks) { 
      *shards = realloc(*shards, ((shard_index+1) * sizeof(rwdts_shard_t *)));
      (*shards)[shard_index] = shard_tab;
      shard_index += 1;
    }
    shard_ptr = shard_tab;
  }

  *head = shard_ptr;
  *n_shards = shard_index;
  if (*head) {
    return true;
   }
   return false;
}

void
rwdts_shard_match_all_recur(rwdts_shard_t        *root, 
                            rw_keyspec_path_t    *keyspec,
                            rwdts_shard_chunk_info_t       ***chunks,
                            uint32_t             *n_chunks,
                            union rwdts_shard_flavor_params_u *params) 
{
  rwdts_shard_t *shard_tab = NULL, *tmp_shard = NULL, *wd_shard = NULL;
  rwdts_shard_chunk_info_t *chunk, *tmp_chunk;
  int chunk_index = 0;

  if (root == NULL) { return; }

  chunk_index = *n_chunks;
  wd_shard = root->wildcard_shard;
  if ((wd_shard != NULL)  && wd_shard->shard_chunks) {
    rwdts_shard_match_all_recur(wd_shard, keyspec, chunks, n_chunks, params);
    chunk_index = *n_chunks;
  }
  // if chunk is present add the result
  if (root->shard_chunks) {
    HASH_ITER(hh_chunk, root->shard_chunks, chunk, tmp_chunk) {
      *chunks = realloc(*chunks, ((chunk_index+1) * sizeof(rwdts_shard_chunk_info_t *)));
      RW_ASSERT(*chunks);
      (*chunks)[chunk_index] = chunk;
      chunk_index += 1;
    }
  }

  *n_chunks = chunk_index;

  // Match the child
   HASH_ITER(hh_shard, root->children, shard_tab, tmp_shard) {
     rwdts_shard_match_all_recur(shard_tab, keyspec, chunks, n_chunks, params);
   }
}

void
rwdts_shard_match_pathelm_recur(rw_keyspec_path_t        *keyspec,
                                rwdts_shard_t            *root,
                                rwdts_shard_chunk_info_t ***chunks,
                                uint32_t                 *n_chunks,
                                union rwdts_shard_flavor_params_u *params)
{
  const rw_keyspec_entry_t *pe;
  rwdts_shard_t *shard_tab, *wd_shard, *tmp_shard;
  rwdts_shard_chunk_info_t *chunk, *tmp_chunk;
  rwdts_shard_elemkey_t elemvalue;
  uint8_t *keyptr;
  int chunk_index;
  size_t length;

  if (root == NULL) { return; }

  pe = rw_keyspec_path_get_path_entry(keyspec, root->pe_index);
  if (pe == NULL) { 
    rwdts_shard_match_all_recur(root, keyspec, chunks, n_chunks, params);
    return; 
  }

  chunk_index = *n_chunks;
  wd_shard = root->wildcard_shard;
  if (wd_shard) {
    rwdts_shard_match_pathelm_recur(keyspec, wd_shard, chunks, n_chunks, params);
    chunk_index = *n_chunks;
  }
  // if chunk is present add the result
  if (root->shard_chunks) {
    if (root->flavor == RW_DTS_SHARD_FLAVOR_NULL) {
      HASH_ITER(hh_chunk, root->shard_chunks, chunk, tmp_chunk) {
        *chunks = realloc(*chunks, ((chunk_index+1) * sizeof(rwdts_shard_chunk_info_t *)));
        RW_ASSERT(*chunks);
        (*chunks)[chunk_index] = chunk;
        chunk_index += 1;
      }
    }
    else {
      if (rwdts_shard_chunk_match(root, keyspec, params, &chunk, NULL)) {
        *chunks = realloc(*chunks, ((chunk_index+1) * sizeof(rwdts_shard_chunk_info_t *)));
        RW_ASSERT(*chunks);
        (*chunks)[chunk_index] = chunk;
        chunk_index += 1;
      }
    }
  }

  *n_chunks = chunk_index;

  // Match the child
  if (root->key_type == RWDTS_SHARD_ELMID ) {
    // get element-id - key is ns-id,tag,type for next level
    rw_schema_ns_and_tag_t element =
        rw_keyspec_entry_get_schema_id(pe);

    memcpy(&elemvalue.key[0], &element.ns,4);
    memcpy(&elemvalue.key[4], &element.tag,4);

    // lookup next level using element id of path element
    HASH_FIND(hh_shard, root->children, &elemvalue.key,
            sizeof(elemvalue.key), shard_tab);

    if (shard_tab) {
      rwdts_shard_match_pathelm_recur(keyspec, shard_tab, 
                                      chunks, n_chunks, params);
    }
  }
  else if (root->key_type == RWDTS_SHARD_KEY) {
    keyptr = rw_keyspec_entry_get_key_packed(pe, NULL, 
                               root->key_index, &length);
    if (keyptr) {
      HASH_FIND(hh_shard, root->children, keyptr, length, shard_tab);
      RW_FREE(keyptr);
      if (shard_tab) {
        rwdts_shard_match_pathelm_recur(keyspec, shard_tab, 
                                      chunks, n_chunks, params);
      }
    }
    else { // wildcard match all children
      HASH_ITER(hh_shard, root->children, shard_tab, tmp_shard) {
        rwdts_shard_match_pathelm_recur(keyspec, shard_tab, 
                                      chunks, n_chunks, params);
      }
    }
  }
}


#if 1
//
// We are doing keyspec registration and shard-init/add-chunk in one single API
// TBD: split them into 2 where shard init takes registered reg handle
// reg ready will have shard handle in the registration handler for now.
//

rw_status_t
rwdts_member_api_shard_key_init(const rw_keyspec_path_t*        keyspec,
                              rwdts_api_t*                      apih,
                              const ProtobufCMessageDescriptor* desc,

                              uint32_t                          flags,
                              int                               idx,
                              rwdts_shard_flavor              flavor,
                              union rwdts_shard_flavor_params_u *flavor_params,
                              enum rwdts_shard_keyfunc_e         hashfunc,
                              union rwdts_shard_keyfunc_params_u *keyfunc_params,
                              enum rwdts_shard_anycast_policy_e   anypolicy,
                              rwdts_member_event_cb_t             *cb)

{
  rwdts_member_reg_handle_t reg_handle;
  rwdts_shard_info_detail_t shard_detail;
  rw_status_t status;

  flags |= RWDTS_FLAG_SHARDING;
  RW_ZERO_VARIABLE(&shard_detail);

  shard_detail.shard_flavor = flavor;
  shard_detail.params = *flavor_params;
  reg_handle = 
    rwdts_member_register(NULL, apih, keyspec, cb, desc, flags, &shard_detail);
  status = reg_handle?RW_STATUS_SUCCESS:RW_STATUS_FAILURE;
  return(status);
}

rw_status_t
rwdts_member_api_shard_key_update(const rw_keyspec_path_t*      keyspec,
                              rwdts_api_t*                      apih,
                              rwdts_member_reg_handle_t         regh,
                              const ProtobufCMessageDescriptor* desc,
                              int                               idx,
                              rwdts_shard_flavor               flavor,
                              union rwdts_shard_flavor_params_u *flavor_params,
                              enum rwdts_shard_keyfunc_e         hashfunc,
                              union rwdts_shard_keyfunc_params_u *keyfunc_params,
                              enum rwdts_shard_anycast_policy_e   anypolicy,
                              rwdts_member_event_cb_t             *cb)

{
  rwdts_member_reg_handle_t reg_handle;
  rwdts_shard_info_detail_t shard_detail;
  rw_status_t status;

  RW_ZERO_VARIABLE(&shard_detail);

  shard_detail.shard_flavor = flavor;
  shard_detail.params = *flavor_params;
  reg_handle = 
      rwdts_member_register_int(NULL, apih, regh, NULL,
                                keyspec, cb, desc, RWDTS_FLAG_SHARDING, &shard_detail);
  status = reg_handle?RW_STATUS_SUCCESS:RW_STATUS_FAILURE;
  return(status);
}
#endif

//
// Gi the function to extract serialized key from a path entry
// for IDENT shard 
//
uint8_t *
rwdts_api_ident_key_gi( rwdts_api_t *apih, char *xpath, int index, size_t *keylen)
{
  const rw_keyspec_entry_t *pe;
  rw_keyspec_path_t *keyspec;
  *keylen = 0;
  uint8_t *retval;

  keyspec = rw_keyspec_path_from_xpath(rwdts_api_get_ypbc_schema(apih),
                                  xpath, RW_XPATH_KEYSPEC, &apih->ksi);
  if (keyspec == NULL) {return NULL;} 
  pe = rw_keyspec_path_get_path_entry(keyspec, index);
  if (pe == NULL) { return NULL; }

  retval = rw_keyspec_entry_get_keys_packed(pe, NULL, keylen);
  return(retval);
}

rwdts_shard_handle_t *
rwdts_member_reg_handle_get_shard(rwdts_member_reg_handle_t regh)
{
  RW_ASSERT(regh);
  rwdts_member_registration_t* reg = (rwdts_member_registration_t *)regh;
  return reg->shard;
}

#define RANGE_KEY_COMP(x) \
{\
  rw_minikey_basic_##x##_t *mk_basic = \
    (rw_minikey_basic_##x##_t *)mk; \
  if ((mk_basic->k.key >= params->rangekey.start_range ) && \
      (mk_basic->k.key <= params->rangekey.end_range)) { \
    return TRUE; \
  } \
  return FALSE; \
}

//
// Returns true if minikey falls within the range param
//

bool
rwdts_shard_range_key_compare(const rw_keyspec_entry_t *pe, 
                              rw_schema_minikey_opaque_t *mk, 
                              union rwdts_shard_chunk_key_u *params)
{
  ProtobufCType ctype = rw_keyspec_entry_get_ctype(pe);

  switch (ctype) {
    case PROTOBUF_C_TYPE_FLOAT: RANGE_KEY_COMP(float); break;
    case PROTOBUF_C_TYPE_DOUBLE: RANGE_KEY_COMP(double); break;

    case PROTOBUF_C_TYPE_INT32:
    case PROTOBUF_C_TYPE_SINT32:
    case PROTOBUF_C_TYPE_SFIXED32:
      RANGE_KEY_COMP(int32); break;

    case PROTOBUF_C_TYPE_UINT32:
    case PROTOBUF_C_TYPE_FIXED32:
      RANGE_KEY_COMP(uint32); break;

    case PROTOBUF_C_TYPE_SINT64:
    case PROTOBUF_C_TYPE_INT64:
    case PROTOBUF_C_TYPE_SFIXED64:
      RANGE_KEY_COMP(int64); break;

    case PROTOBUF_C_TYPE_UINT64:
    case PROTOBUF_C_TYPE_FIXED64:
      RANGE_KEY_COMP(uint64); break;

    case PROTOBUF_C_TYPE_BYTES:
    case PROTOBUF_C_TYPE_BOOL:
    case PROTOBUF_C_TYPE_STRING:
    default: return FALSE;
  }
  return FALSE;
}

//
// update the flavor for the shard. While registering keyspec
// default null shard flavor is created. Later member can call the 
// shard api init to update the shard flavor and add chunk for that flavor
// existing chunks will be freed up when we update flavor
//

rw_status_t 
rwdts_shard_update(rwdts_shard_t *shard, 
                   rwdts_shard_flavor flavor,
                   union rwdts_shard_flavor_params_u *flavor_params)
{
  rwdts_shard_chunk_info_t *shard_chunk, *tmp_chunk;
 
  if (shard->flavor == flavor) {
    return RW_STATUS_SUCCESS;
  }
  if (!shard->flavor) {
    shard->flavor = flavor;
    return RW_STATUS_SUCCESS;
  }
// TBD should prevent arbitrary shard flavor updates?
//if (shard->flavor != RW_DTS_SHARD_FLAVOR_NULL) {
//  return RW_STATUS_READONLY;
//}

  HASH_ITER(hh_chunk, shard->shard_chunks, shard_chunk, tmp_chunk) {
    HASH_DELETE(hh_chunk, shard->shard_chunks, shard_chunk);
  }
  shard->flavor = flavor;
  return RW_STATUS_SUCCESS;
}

void
rwdts_member_reg_handle_update_shard(rwdts_member_reg_handle_t regh,
                                     rwdts_shard_handle_t* shard)
{
  RW_ASSERT(regh);
  RW_ASSERT(shard);

  rwdts_member_registration_t *reg = (rwdts_member_registration_t *)regh;
  RW_ASSERT(!reg->shard);

  reg->shard = shard;

  return;
}

rwdts_shard_t**
rwdts_shard_get_root_parent(rwdts_api_t* apih)
{
  return &apih->rootshard;
}

void
rwdts_shard_del(rwdts_shard_t** parent)
{
  RW_ASSERT_TYPE(*parent, rwdts_shard_t);

  rwdts_shard_t* child, *tmp_child;
  HASH_ITER(hh_shard, (*parent)->children, child, tmp_child) {
    RW_ASSERT_TYPE(child, rwdts_shard_t);
    rwdts_shard_del(&child);
  }
  if ((*parent)->wildcard_shard) {
    RW_ASSERT_TYPE((*parent)->wildcard_shard, rwdts_shard_t);
    rwdts_shard_del(&(*parent)->wildcard_shard);
  }
  if ((*parent)->children || (*parent)->wildcard_shard) {
    return;
  }
  rwdts_shard_chunk_info_t* chunk, *tmp_chunk;
  HASH_ITER(hh_chunk, (*parent)->shard_chunks, chunk, tmp_chunk) {
    rwdts_chunk_rtr_info_t *rtr, *tmp_rtr;
    HASH_ITER(hh_rtr_record, chunk->elems.rtr_info.pub_rtr_info,
              rtr, tmp_rtr) {
      if (!strstr(rtr->msgpath, "DTSRouter") && 
          !(rtr->flags & RWDTS_FLAG_INTERNAL_REG)) {
        HASH_DELETE(hh_rtr_record, chunk->elems.rtr_info.pub_rtr_info, rtr);
        RW_FREE(rtr);
        chunk->elems.rtr_info.n_pubrecords--;
      }
    }
    HASH_ITER(hh_rtr_record, chunk->elems.rtr_info.sub_rtr_info,
              rtr, tmp_rtr) {
      if (!strstr(rtr->msgpath, "DTSRouter") &&
          !(rtr->flags & RWDTS_FLAG_INTERNAL_REG)) {
        HASH_DELETE(hh_rtr_record, chunk->elems.rtr_info.sub_rtr_info, rtr);
        RW_FREE(rtr);
        chunk->elems.rtr_info.n_subrecords--;
      } 
    }

    if (!(chunk->elems.rtr_info.n_subrecords || chunk->elems.rtr_info.n_pubrecords)) {
      HASH_DELETE(hh_chunk, (*parent)->shard_chunks, chunk);
      DTS_APIH_FREE_TYPE((*parent)->apih, RW_DTS_DTS_MEMORY_TYPE_SHARD_CHUNK,
                         chunk, rwdts_shard_chunk_info_t);
    }
  }
  if (!HASH_CNT(hh_chunk, (*parent)->shard_chunks)) {
    if ((*parent)->parent) {
      if ((*parent)->parent->wildcard_shard != (*parent)) {
        HASH_DELETE(hh_shard, (*parent)->parent->children, (*parent));
        if ((*parent)->key) {
          RW_FREE((*parent)->key);
        }
        DTS_APIH_FREE_TYPE((*parent)->apih, RW_DTS_DTS_MEMORY_TYPE_SHARD,
                           *parent, rwdts_shard_t);
        *parent = NULL;
      } else {
        rwdts_shard_t* tmp_wd_par = (*parent)->parent;
        if ((*parent)->key) {
          RW_FREE((*parent)->key);
        }
        DTS_APIH_FREE_TYPE((*parent)->apih,
                           RW_DTS_DTS_MEMORY_TYPE_SHARD,
                           *parent, rwdts_shard_t);
        tmp_wd_par->wildcard_shard = NULL;
        *parent = NULL;
      }
    }
  }
}

rw_status_t
rwdts_shard_promote_to_publisher(rwdts_member_reg_handle_t regh)
{
  RW_ASSERT(regh);
  rwdts_shard_chunk_info_t *chunk=NULL, *tmp_chunk;
  rwdts_chunk_member_info_t *mbr_elem = NULL;

  rwdts_member_registration_t *reg = (rwdts_member_registration_t *)regh;
  RW_ASSERT (reg && reg->shard);

  const char* member = reg->apih->client_path;
  RW_ASSERT (member);

  HASH_ITER(hh_chunk, reg->shard->shard_chunks, chunk, tmp_chunk) {
    mbr_elem = NULL;
    HASH_FIND(hh_mbr_record, chunk->elems.member_info.sub_mbr_info, member, strlen(member), mbr_elem);
    if (mbr_elem) {
      // Remove the member-record from subscriber list
      HASH_DELETE(hh_mbr_record, chunk->elems.member_info.sub_mbr_info, mbr_elem); 

      // Reset SUBSCRIBER flag and set PUBLISHER flag
      mbr_elem->flags &= ~RWDTS_FLAG_SUBSCRIBER;
      mbr_elem->flags |= RWDTS_FLAG_PUBLISHER;

      // Insert the member record into publisher list
      HASH_ADD_KEYPTR(hh_mbr_record, chunk->elems.member_info.pub_mbr_info, member, strlen(member), mbr_elem); 
      chunk->elems.member_info.n_sub_reg--;
      chunk->elems.member_info.n_pub_reg++;
    }
  }

  return RW_STATUS_SUCCESS;
}


rw_status_t
rwdts_shard_rts_promote_element(rwdts_shard_t *shard, rwdts_chunk_id_t chunk_id, 
                                uint32_t membid, char *msgpath)
{
  rwdts_shard_chunk_info_t *shard_chunk;
  rwdts_chunk_rtr_info_t *rtr_record = NULL;

  RW_ASSERT_TYPE(shard, rwdts_shard_t);

  RW_ASSERT(membid <= RWDTS_SHARD_CHUNK_SZ);

  // add record to last chunk, if last chunk is full add new chunk 
  HASH_FIND(hh_chunk, shard->shard_chunks, &chunk_id, sizeof(chunk_id), shard_chunk);

  if (!shard_chunk) { // kick the bucket
    return RW_STATUS_FAILURE;
  }

  HASH_FIND(hh_rtr_record, shard_chunk->elems.rtr_info.sub_rtr_info, msgpath, strlen(msgpath), rtr_record);

  if (!rtr_record) {
    return RW_STATUS_FAILURE;
  }

  HASH_DELETE(hh_rtr_record, shard_chunk->elems.rtr_info.sub_rtr_info, rtr_record);

  // Reset SUBSCRIBER flag and set PUBLISHER flag
  rtr_record->flags &= ~RWDTS_FLAG_SUBSCRIBER;
  rtr_record->flags |= RWDTS_FLAG_PUBLISHER;

 
  // Insert the member record into publisher list
  HASH_ADD_KEYPTR(hh_rtr_record, shard_chunk->elems.rtr_info.pub_rtr_info, msgpath, strlen(msgpath), rtr_record);

  shard_chunk->elems.member_info.n_sub_reg--;
  shard_chunk->elems.member_info.n_pub_reg++;

  return RW_STATUS_SUCCESS;
 
}
