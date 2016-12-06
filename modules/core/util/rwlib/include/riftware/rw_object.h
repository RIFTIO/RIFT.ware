
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


/**
 * @file rw_object.h
 * @author Rajesh Ramankutty (rajesh.ramankutty@riftio.com)
 * @date 03/12/14
 * @header file for define RW-OBJECT
 */

#ifndef __RW_OBJECT_H__
#define __RW_OBJECT_H__

__BEGIN_DECLS

#if 1
typedef enum {
    RW_RAW_OBJECT = 0x8F08A,
    RW_CF_OBJECT  = 0x8F0CF,
    RW_MALLOC_OBJECT  = 0x8F030,
} rw_object_type_t;
#else
typedef enum {
    RW_RAW_OBJECT = 0,
    RW_CF_OBJECT  = 1,
    RW_MALLOC_OBJECT  = 2,
} rw_object_type_t;
#endif

typedef struct rw_object_s {
  union {
    unsigned char dummy[64];
  } _base;
  rw_object_type_t _type;
} rw_object_t;

__END_DECLS

#endif /* __RW_OBJECT_H__ */
