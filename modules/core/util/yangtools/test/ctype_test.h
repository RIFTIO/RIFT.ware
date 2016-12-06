
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
 * @file ctype_test.h
 * @author Tom Seidenberg
 * @date 2014/12/31
 */

#ifndef CTYPE_TEST_H_
#define CTYPE_TEST_H_

#include <rwtypes.h>
#include <inttypes.h>

__BEGIN_DECLS

typedef struct {
  size_t n;
  uint16_t x[8];
} binary_ctype_t;

extern const ProtobufCCTypeDescriptor binary_ctype_t_helper;
#define binary_ctype_t_STATIC_INIT {}


typedef struct {
  size_t n;
  uint16_t* x; // dynamically allocated
} dynalloc_ctype_t;

extern const ProtobufCCTypeDescriptor dynalloc_ctype_t_helper;
#define dynalloc_ctype_t_STATIC_INIT {}


extern unsigned ctype_calls_to_alloc;
extern unsigned ctype_calls_to_check;
extern unsigned ctype_calls_to_compare;
extern unsigned ctype_calls_to_deep_copy;
extern unsigned ctype_calls_to_error;
extern unsigned ctype_calls_to_free;
extern unsigned ctype_calls_to_free_usebody;
extern unsigned ctype_calls_to_from_string;
extern unsigned ctype_calls_to_get_packed_size;
extern unsigned ctype_calls_to_init_usebody;
extern unsigned ctype_calls_to_pack;
extern unsigned ctype_calls_to_realloc;
extern unsigned ctype_calls_to_to_string;
extern unsigned ctype_calls_to_unpack;
extern unsigned ctype_calls_to_zalloc;

extern void ctype_clear_call_counters();
extern void ctype_error(ProtobufCInstance* instance, const char* errmsg);
extern void ctype_free(ProtobufCInstance* instance, void *data);
extern void* ctype_alloc(ProtobufCInstance* instance, size_t size);
extern void* ctype_realloc(ProtobufCInstance* instance, void* data, size_t size);
extern void* ctype_zalloc(ProtobufCInstance* instance, size_t size);

__END_DECLS

#endif /* CTYPE_TEST_H_ */
