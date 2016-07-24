
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/*!
 * @file rwdts_keyspec.h
 * @brief RW.DTS Keyspec API
 * @author Tom Seidenberg
 * @date 2014/008/28
 */

#ifndef __RWDTS_KEYSPEC_H
#define __RWDTS_KEYSPEC_H

#include <uthash.h>
#include <rw_keyspec.h>

__BEGIN_DECLS

/*! RW Keyspec handle */
typedef int rwdts_keyspec_t;

/* For sharding purposes, most key types are (externally) turned into
   a uint64.  A few remain as literal opaque ptr+len values which get
   exact-matched.  Probably we need typed-key-savvy APIs atop this. */
typedef uint64_t rwdts_shard_keyint_t;

/* There can be 2**32 chunks */
typedef uint32_t rwdts_shard_chunk_t;

/* For RANGE; IDENT is just a degenerate range for now with first==last */
struct rwdts_shard_key_range_s {
  rw_minikey_basic_int32_t key_first;
  rw_minikey_basic_int32_t key_last;
};

/* Actual mapping function */
enum rwdts_shard_mapping_e {
  RWDTS_SHARD_MAPPING_IDENT=0,  /* tab[kint(key)] */
  RWDTS_SHARD_MAPPING_STRKEY, /* tab[key] */
  RWDTS_SHARD_MAPPING_RANGE,  /* tab[@first<kint<last], sorted tab+binary search or similar */
  RWDTS_SHARD_MAPPING_HASH, /* tab[kint%sz] */
};

typedef struct rwdts_shard_key_range_s rwdts_shard_key_range_t;
typedef enum rwdts_shard_mapping_e rwdts_shard_mapping_t;

#define RWDTS_PUBLISH 0x01
#define RWDTS_SUBSCRIBE 0x02
#define RWDTS_KV_DB_UPDATE 0x04

#define RWDTS_SET_PUBLISH(flags)\
    (flags |= RWDTS_PUBLISH)

#define RWDTS_IS_PUBLISH(flags)\
    (flags & RWDTS_PUBLISH)

#define RWDTS_SET_SUBSCRIBE(flags)\
    (flags |= RWDTS_SUBSCRIBE)

#define RWDTS_IS_SUBSCRIBE(flags)\
    (flags & RWDTS_SUBSCRIBE)

#define RWDTS_SET_KV_DB_UPDATE(flags)\
    (flags |= RWDTS_KV_DB_UPDATE)

#define RWDTS_IS_KV_DB_UPDATE(flags)\
    (flags & RWDTS_KV_DB_UPDATE)

typedef struct rwdts_shard_db_num_s {
  int db_number;
  int shard_chunk_id;
} rwdts_shard_db_num_t;

struct rwdts_shard_db_num_info_s {
  rwdts_shard_db_num_t *shard_db_num;
  int shard_db_num_cnt;
};

#if 0
typedef struct rwdts_shard_key_detail_s {
  union {
    struct {
      void *key;
      uint32_t key_len;
    } strkey;
    rwdts_shard_key_range_t range;

    //#error need more info than range.first for HASH case(s), there
    //#are lotsa hashes, for example, and the app should be able to
    //#say which chunk(s) it wants and/or how many auto-assigned
    //#chunk(s) it wants and/or to just take what the router/resource
    //#stuff assigns

  }u;
} rwdts_shard_key_detail_t;
#endif

typedef struct rwdts_shard_key_detail_s {
  union {
    rw_minikey_basic_bytes_pointy_t byte_key;

    rwdts_shard_key_range_t range;

    //#error need more info than range.first for HASH case(s), there
    //#are lotsa hashes, for example, and the app should be able to
    //#say which chunk(s) it wants and/or how many auto-assigned
    //#chunk(s) it wants and/or to just take what the router/resource
    //#stuff assigns

  }u;
} rwdts_shard_key_detail_t;


/* These details are associated with a shard-to-member binding.  This,
   encapsulated with msgpath and a verb (add/del/etc), will need to
   become a Protobuf message for communication with the broker.  */
typedef struct rwdts_shard_registration_detail_s {
  rwdts_shard_key_detail_t shard_key_detail;
  //#error why here?
  uint8_t kv_database_use; /* Key stored in KV database */
  //#error why here?
  uint8_t flags; /* Indicates publish or subscriber for a member w.r.t a key */
  uint32_t shard_chunk_id;
} rwdts_shard_registration_detail_t;

/* Member record, exists once per sharding table, not once per member globally */
typedef struct rwdts_shard_member_s {
  uint32_t empty:1;        /* entries may need to stay and be marked empty so we can use indices */
  uint32_t defmember:1;        /* there may be some default members */
  uint32_t _pad:30;
  uint32_t refct;        /* there will be many tab_ents pointing at us (by index) */

  char *msgpath;         /* actual rwmsg path for the member */

  rwdts_shard_registration_detail_t *registration_detail_tab;
  int registration_detail_len;
} rwdts_shard_member_t;

/* Shard chunk entry.  One or more members for this chunk; all members
   sharing identical details */
typedef struct rwdts_shard_tab_ent_s {

  //#error this tab should be of pointers or just indices, each with a refct taken on the member, at the one table of rwdts_shard_member_t's in shard_tab_t
  rwdts_shard_member_t *member_tab;
  union {
    uint32_t member_tab_len;        /* list of members for this chunk */
    uint32_t member_idx;        /* the one and only member if !member_tab */
  } u;

  //#error shard_chunk_id is already in map.shard_chunk_id, why two?
  uint32_t shard_chunk_id;

  uint32_t empty:1;                /* table entry can be empty for partly-registered hashes */

  rwdts_shard_registration_detail_t map;
} rwdts_shard_tab_ent_t;

//const uint32_t rwdts_shard_tab_sz_default = 8192;

/* Reg: /X/{k1 chunks 1 2 3 }/Y/{k2 chunks 1,1 1,2 1,3 2,1 2,2 2,3 3,1 3,2 3,3} */

/* ! */

// single table Y, one or multiple results
//  /X/11/Y/22      ; key 22 in table x11y, single destination
//  /X/11/Y/{s3}    ; chunk 3 entries in table x11y, single dest mult result
//  /X/11/Y/{*}     ; all entries in table x11y, mult dest must result

// single destination, multiple key-values from multiple Y tables, probably we won't bother with the brute force lookups for now
//  /X/{s1}/Y/22    ; not ambiguous, intersect y chunk(keyint(22)) and x chunk s1
//  /X/{s1}/Y/{s3}  ; not ambiguous, (X chunk 1, Y chunk 3) is probably a specific tab entry
//  /X/{s1}/Y/{*}   ; ambiguous, to all Sx1 members

// multiple destinations, multiple KVs from all tables Y
//  /X/{*}/Y/22     ; ", maybe intersect y 22 members with X members
//  /X/{*}/Y/{s3}   ; ", maybe intersect y shard 3 with X members
//  /X/{*}/Y/{*}    ; "

/* Shard mapping table, maps from key[int] to chunk number.  Each slot
   in the shard_tab is a chunk.  Each member operates one or more
   chunks.  Chunks may map to any number of members, or to zero
   members to trigger default route behavior.  Together the chunks for
   one member comprise a shard.  Member workload may be reduced or
   increased by reassigning chunks to some other member.  It is
   suggested to start with a healthy number of chunks per member to
   enable splitting easily after the fact.  Individual chunks may
   theoretically be split (by introducing a table that is, say, twice
   a long) or unsplit (by introducing a table that is half as long);
   or an explicit split-chunk mechanism might be introduced, however
   any of this requires lots more work.

   For common sense to prevail, a shard_tab with a parent (ie any
   sharded key(s) higher up in the keyspec) can only contain members
   found in that parent.  This has no immediate runtime use other than
   validation.

   The implementation of heirarchical delivery is likely to be driven
   at least in part by shard topology. */
typedef struct rwdts_shard_tab_s {
  UT_hash_handle hh_shard;
  rwdts_shard_mapping_t mapping;

  uint32_t table_id;                /* unique integer ID of this table, from Router or ZK? */
  uint32_t next_chunkno;        /* dynamically assigned chunk numbering */
  uint32_t chunkct;                /* fixed number of chunks in tab for hash-type tabs */

  void *parent; /* parent sharding table, also visible by going "up" in the keyspec/path */
  rwdts_shard_chunk_t parent_chunk; /* our chunk in the parent */

  rwdts_shard_tab_ent_t *tab;        /* Actual hash/lookup table, we work from tab[f(key)] */
  uint32_t tab_len;                /* length filled in for index/hash purposes, also allocation size of this overall tab_t */

  rwdts_shard_tab_ent_t def_ent; /* Misses go to this entry, which gets populated with references to all defmember members */

  rwdts_shard_member_t *member;        /* Table of members, referred to by index. */
  uint32_t member_len;                /* Number of members */
} rwdts_shard_tab_t;

rw_status_t
rwdts_shard_add_member_registration(rwdts_shard_tab_t *shtab,
                                    const char *msgpath,
                                    const rwdts_shard_key_detail_t *shard_reg,
                                    uint32_t *shard_chunk_id);

__END_DECLS


#endif /* __RWDTS_KEYSPEC_H */

