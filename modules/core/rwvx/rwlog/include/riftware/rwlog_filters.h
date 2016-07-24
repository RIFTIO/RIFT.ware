
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 *
 */

#ifndef __RWLOG_FILTERS_H__
#define __RWLOG_FILTERS_H__

#include "rwlog.h"
#include <sys/mman.h>   /* shared memory and mmap() */

#define MAX_ATTRIBUTE_LEN 256
#define RWLOG_FILTER_STRING_SIZE MAX_ATTRIBUTE_LEN
#define RWLOG_FILTER_HASH_SIZE 1024
#define RWLOG_INTRVL 60
#define RWLOG_REPEAT_COUNT 5
#define RWLOG_BINARY_FIELD "pkt_info"
extern void *rwlog_filter_malloc(int sz);
extern void rwlog_filter_free(void *ptr);
//typedef RwLog__YangEnum__LogSeverity__E rwlog_severity_t;
typedef RWPB_E(RwLog_LogSeverity) rwlog_severity_t;


typedef struct name_rec {
   char attribute_value_string[MAX_ATTRIBUTE_LEN];
   UT_hash_handle hh;
   int next_call_flag;
   int magic;
} filter_attribute;

typedef struct {
  uint32_t n_elements;
}rwlog_filter_hash_header_t;

typedef struct {
  uint32_t n_entries:12;
  uint32_t table_offset:20;
}filter_array_hdr;


typedef struct {
  unsigned int magic;
  uint32_t log_serial_no; /* Serial no to denote if any config changed */
  uint32_t category_offset; 
  uint32_t num_categories;
  pid_t rwlogd_pid;
  /* IMPORTANT NOTE: Do not add new fields above this and any changes requires
   * changes in rwlogger.py that assumes above order */
  int rotation_serial;
  uint32_t rwlogd_ticks;
  uint32_t protocol_filter[4];
  uint32_t packet_direction; /* Inbound/Outbound/Both */
  uint64_t static_bitmap; // Values for attributes.
  uint32_t skip_L1:1;		/* better be one writer */
  uint32_t skip_L2:1;
  uint32_t next_call_filter:1;
  uint32_t failed_call_filter:1;
  uint32_t rwlogd_flow:1;
  uint32_t bootstrap:1;
  uint32_t allow_dup:1;
  uint32_t _pad:25;
  int num_filters; // Only for Show logs
} filter_memory_header;

typedef struct {
  /* Do not add any variables before this. hdr should be first vairable */
  filter_memory_header hdr; // common for sinks and sources
  filter_array_hdr fieldvalue_entries[RWLOG_FILTER_HASH_SIZE];
  uint32_t last_location;
}rwlogd_shm_ctrl_t;

#define RWLOG_TRACE printf
#define RWLOG_FILTER_SHM_PATH "RW.Log.Filter.SHM"
#define RWLOG_FILTER_SHM_SIZE (1024*1024)
#define RWLOG_FILTER_MAGIC 0xfeeddeed
#define IS_VALID_RWLOG_FILTER(x) ((x)&&(x)->magic == RWLOG_FILTER_MAGIC)

#define RWLOG_MAX_NUM_CATEGORIES   128

#define NUM_HASH_BITS (64)
#define RWLOG_DENY_ID_HASH_BITS (64)

#define BLOOM_IS_SET(bitmap,value,found) ({ \
  uint64_t _hf_bkt,_hf_hashv; \
  HASH_FCN(value,strlen(value), NUM_HASH_BITS,_hf_hashv,_hf_bkt); \
  found = !!(bitmap&((uint64_t)1<<(_hf_bkt-1))); \
  RWLOG_FILTER_DEBUG_PRINT("hash after test for value:%s hash: %lu bitmap:%lx, found: %d\n", value,_hf_bkt,bitmap, found);\
  _hf_hashv; \
})

#define BLOOM_IS_SET_UINT64(bitmap,value,found) \
do { \
  uint64_t _hf_bkt,_hf_hashv; \
  HASH_FCN(value,sizeof(uint64_t), NUM_HASH_BITS,_hf_hashv,_hf_bkt); \
  found =!!(bitmap&((uint64_t)1<<(_hf_bkt-1))); \
} while (0)

#define BLOOM_SET(bitmap,value) ({ \
  uint64_t _hf_bkt,_hf_hashv; \
  HASH_FCN(value,strlen(value), NUM_HASH_BITS,_hf_hashv,_hf_bkt); \
  RWLOG_FILTER_DEBUG_PRINT("bitmap before set for value %s is %lu %lx,%lx\n", value,_hf_bkt,bitmap,((uint64_t)1<<(_hf_bkt-1))); \
  bitmap |= ((uint64_t)1<<(_hf_bkt-1)); \
  RWLOG_FILTER_DEBUG_PRINT("bitmap after set for value %s is %lu %lx\n", value,_hf_bkt,bitmap);\
  _hf_hashv; \
})

#define BLOOM_SET_UINT64(bitmap,value) \
do { \
  uint64_t _hf_bkt,_hf_hashv; \
  HASH_FCN(value,sizeof(uint64_t), NUM_HASH_BITS,_hf_hashv,_hf_bkt); \
  bitmap |= ((uint64_t)1<<(_hf_bkt-1)); \
} while (0)

#define BLOOM_RESET(bitmap,value) \
do { \
  uint64_t _hf_bkt,_hf_hashv; \
  HASH_FCN(value,strlen(value), NUM_HASH_BITS,_hf_hashv,_hf_bkt); \
  RWLOG_FILTER_DEBUG_PRINT("bitmap before reset for value %s is %lu %lx\n", value,_hf_bkt,bitmap); \
  bitmap ^= ((uint64_t)1<<(_hf_bkt-1)); \
  RWLOG_FILTER_DEBUG_PRINT("bitmap after reset for value %s is %lu %lx\n", value,_hf_bkt,bitmap);\
} while (0)

#define BLOOM_RESET_UINT64(bitmap,value) \
do { \
  uint64_t _hf_bkt,_hf_hashv; \
  HASH_FCN(value,sizeof(uint64_t), NUM_HASH_BITS,_hf_hashv,_hf_bkt); \
  bitmap ^= ((uint64_t)1<<(_hf_bkt-1)); \
} while (0)


#define RWLOG_SET_DENY_ID_HASH(bitmap,value) ({ \
  uint64_t _hf_bkt; \
  _hf_bkt = (value) % RWLOG_DENY_ID_HASH_BITS; \
  bitmap |= ((uint64_t)1<<(_hf_bkt)); \
})

#define RWLOG_RESET_DENY_ID_HASH(bitmap,value) \
do { \
  uint64_t _hf_bkt; \
  _hf_bkt = (value) % RWLOG_DENY_ID_HASH_BITS; \
  bitmap ^= ((uint64_t)1<<(_hf_bkt)); \
} while (0)

#define RWLOG_IS_SET_DENY_ID_HASH(bitmap,value,found) ({ \
  uint64_t _hf_bkt; \
  _hf_bkt = (value) % RWLOG_DENY_ID_HASH_BITS; \
  found = !!(bitmap&((uint64_t)1<<(_hf_bkt))); \
})

#define RWLOG_UPDATE_PROTOCOL_FILTER_FLAG(_filter,_set) {\
  for(unsigned int _i=0;_i<sizeof((_filter)->protocol_filter)/sizeof((_filter)->protocol_filter[0]);_i++) {\
    if((_filter)->protocol_filter[_i]) {\
      _set = TRUE;\
      break;\
    }\
    _set = FALSE;\
  }\
}

#define RWLOG_FILTER_SET_PACKET_DIRECTION(_filter_hdr,_pkt_direction) \
    (_filter_hdr)->packet_direction |= _pkt_direction;

#define RWLOG_FILTER_IS_PACKET_DIRECTION_SET(_filter_hdr,_pkt_direction) \
    ((_filter_hdr)->packet_direction?!!((_filter_hdr)->packet_direction & _pkt_direction):TRUE);

#define RWLOG_FILTER_SET_PROTOCOL(_filter_hdr,_protocol) \
  (_filter_hdr)->protocol_filter[((_protocol)/sizeof((_filter_hdr)->protocol_filter[0]))] |= 1<<(sizeof((_filter_hdr)->protocol_filter[0])*8 - ((_protocol)%sizeof((_filter_hdr)->protocol_filter[0])) -1)

#define RWLOG_FILTER_RESET_PROTOCOL(_filter_hdr,_protocol) \
  (_filter_hdr)->protocol_filter[((_protocol)/sizeof((_filter_hdr)->protocol_filter[0]))] &= ~(1<<(sizeof((_filter_hdr)->protocol_filter[0])*8 - ((_protocol)%sizeof((_filter_hdr)->protocol_filter[0])) -1))

#define RWLOG_FILTER_IS_PROTOCOL_SET(_filter_hdr,_protocol) \
    !!(((_filter_hdr)->protocol_filter[((_protocol)/sizeof((_filter_hdr)->protocol_filter[0]))]) & (1<<(sizeof((_filter_hdr)->protocol_filter[0])*8 - ((_protocol)%sizeof((_filter_hdr)->protocol_filter[0])) -1)))

#endif
