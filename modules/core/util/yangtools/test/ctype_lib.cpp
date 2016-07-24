
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file ctype_test.cpp
 * @author Tom Seidenberg
 * @date 2014/12/31
 * @brief Test rwpb:field-c-type
 */

#include "rwut.h"
#include "rwlib.h"
#include "ctype_test.h"

#include <limits.h>
#include <cstdlib>
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

unsigned ctype_calls_to_alloc = 0;
void* ctype_alloc(ProtobufCInstance* instance, size_t size)
{
  (void)instance;
  ++ctype_calls_to_alloc;
  return malloc(size);
}

unsigned ctype_calls_to_zalloc = 0;
void* ctype_zalloc(ProtobufCInstance* instance, size_t size)
{
  (void)instance;
  ++ctype_calls_to_zalloc;
  return calloc(size,1);
}

unsigned ctype_calls_to_realloc = 0;
void* ctype_realloc(ProtobufCInstance* instance, void* data, size_t size)
{
  (void)instance;
  ++ctype_calls_to_realloc;
  return realloc(data, size);
}

unsigned ctype_calls_to_free = 0;
void ctype_free(ProtobufCInstance* instance, void *data)
{
  (void)instance;
  ++ctype_calls_to_free;
  free(data);
}

unsigned ctype_calls_to_error = 0;
void ctype_error(ProtobufCInstance* instance, const char* errormsg)
{
  (void)instance;
  ++ctype_calls_to_error;
  fprintf(stderr, errormsg);
}

ProtobufCInstance ctype_pbc_instance = {
  .alloc = &ctype_alloc,
  .zalloc = &ctype_zalloc,
  .realloc = &ctype_realloc,
  .free = &ctype_free,
  .error = &ctype_error,
  .data = NULL,
};

unsigned ctype_calls_to_get_packed_size = 0;
unsigned ctype_calls_to_pack = 0;
unsigned ctype_calls_to_unpack = 0;
unsigned ctype_calls_to_to_string = 0;
unsigned ctype_calls_to_from_string = 0;
unsigned ctype_calls_to_init_usebody = 0;
unsigned ctype_calls_to_free_usebody = 0;
unsigned ctype_calls_to_check = 0;
unsigned ctype_calls_to_deep_copy = 0;
unsigned ctype_calls_to_compare = 0;

void ctype_clear_call_counters()
{
  ctype_calls_to_get_packed_size = 0;
  ctype_calls_to_pack = 0;
  ctype_calls_to_unpack = 0;
  ctype_calls_to_to_string = 0;
  ctype_calls_to_from_string = 0;
  ctype_calls_to_init_usebody = 0;
  ctype_calls_to_free_usebody = 0;
  ctype_calls_to_check = 0;
  ctype_calls_to_deep_copy = 0;
  ctype_calls_to_compare = 0;

  ctype_calls_to_alloc = 0;
  ctype_calls_to_free = 0;
}


/*****************************************************************************/
/*
 * binary_ctype_t is just a way to ensure that the packed encoding is
 * different than the C and string encodings.  They are all mutually
 * distinct, so bugs are easier to detect.
 *  - semantics: up to 8 unsisgned chars
 *  - packed encoding: binary, up to 8 unsigned chars.
 *  - C encoding: array of uint16_6, with each index i shifted by i
 *     "23:ab" => { 0x23<<0, 0xab<<1 }
 */
static size_t binary_ctype_t_get_packed_size(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member)
{
  ++ctype_calls_to_get_packed_size;
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &binary_ctype_t_helper);
  RW_ASSERT(void_member);
  const binary_ctype_t* binary = (const binary_ctype_t*)void_member;
  return binary->n;
}

static size_t binary_ctype_t_pack(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member,
  uint8_t* out)
{
  ++ctype_calls_to_pack;
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &binary_ctype_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(out);
  const binary_ctype_t* binary = (const binary_ctype_t*)void_member;
  for (unsigned i = 0; i < binary->n; ++i) {
    uint16_t v = (uint16_t)(binary->x[i] >> i);
    RW_ASSERT(v < 256);
    out[i] = (uint8_t)v;
  }
  return binary->n;
}

static protobuf_c_boolean binary_ctype_t_unpack(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  size_t in_size,
  const uint8_t* in,
  protobuf_c_boolean maybe_clear,
  void* void_member)
{
  ++ctype_calls_to_unpack;
  UNUSED(fielddesc);
  UNUSED(maybe_clear);
  RW_ASSERT(ctypedesc == &binary_ctype_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(in);
  RW_ASSERT(instance);
  binary_ctype_t* binary = (binary_ctype_t*)void_member;
  for (unsigned i = 0; i < in_size; ++i) {
    binary->x[i] = (uint16_t)in[i] << i;
  }
  binary->n = in_size;
  return TRUE;
}

static protobuf_c_boolean binary_ctype_t_to_string(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member,
  size_t* str_size,
  char* str)
{
  ++ctype_calls_to_to_string;
  UNUSED(fielddesc);
  RW_ASSERT(str_size);
  RW_ASSERT(ctypedesc == &binary_ctype_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(str);

  size_t packed = binary_ctype_t_get_packed_size(
    ctypedesc,
    fielddesc,
    void_member);
  if (*str_size < packed) {
    return FALSE;
  }
  size_t actual = binary_ctype_t_pack(
    ctypedesc,
    fielddesc,
    void_member,
    (uint8_t*)str);
  RW_ASSERT(packed == actual);
  *str_size = actual;
  return TRUE;
}

static protobuf_c_boolean binary_ctype_t_from_string(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const char* str,
  size_t str_len,
  protobuf_c_boolean maybe_clear,
  void* void_member)
{
  ++ctype_calls_to_from_string;
  return binary_ctype_t_unpack(instance,
                               ctypedesc,
                               fielddesc,
                               str_len,
                               (const uint8_t*)str,
                               maybe_clear,
                               void_member);
}

static protobuf_c_boolean binary_ctype_t_init_usebody(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  void* void_member)
{
  ++ctype_calls_to_init_usebody;
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &binary_ctype_t_helper);
  RW_ASSERT(void_member);
  binary_ctype_t* binary = (binary_ctype_t*)void_member;
  memset(binary->x,0,sizeof(binary->x));
  return TRUE;
}

static void binary_ctype_t_free_usebody(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  void* void_member)
{
  ++ctype_calls_to_free_usebody;
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &binary_ctype_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(instance);
}

static protobuf_c_boolean binary_ctype_t_check(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member)
{
  ++ctype_calls_to_check;
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &binary_ctype_t_helper);
  RW_ASSERT(void_member);
  return TRUE;
}

static protobuf_c_boolean binary_ctype_t_deep_copy(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_copy_from,
  protobuf_c_boolean maybe_clear,
  void* void_copy_to)
{
  ++ctype_calls_to_deep_copy;
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &binary_ctype_t_helper);
  RW_ASSERT(void_copy_from);
  RW_ASSERT(void_copy_to);
  RW_ASSERT(instance);
  const binary_ctype_t* copy_from = (const binary_ctype_t*)void_copy_from;
  binary_ctype_t* copy_to = (binary_ctype_t*)void_copy_to;
  memcpy(copy_to, copy_from, sizeof(*copy_to));
  return TRUE;
}

static int binary_ctype_t_compare(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_a,
  const void* void_b)
{
  ++ctype_calls_to_compare;
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &binary_ctype_t_helper);
  RW_ASSERT(void_a);
  RW_ASSERT(void_b);
  const binary_ctype_t* a = (const binary_ctype_t*)void_a;
  const binary_ctype_t* b = (const binary_ctype_t*)void_b;
  for (unsigned i = 0; i < a->n && i < b->n; ++i) {
    if (a->x[i] < b->x[i]) {
      return -1;
    } else if (a->x[i] > b->x[i]) {
      return 1;
    }
  }
  if (a->n < b->n) {
    return -1;
  } else if (a->n > b->n) {
    return 1;
  }
  return 0;
}

const ProtobufCCTypeDescriptor binary_ctype_t_helper = {
  &binary_ctype_t_get_packed_size,
  &binary_ctype_t_pack,
  &binary_ctype_t_unpack,
  &binary_ctype_t_to_string,
  &binary_ctype_t_from_string,
  &binary_ctype_t_init_usebody,
  &binary_ctype_t_free_usebody,
  &binary_ctype_t_check,
  &binary_ctype_t_deep_copy,
  &binary_ctype_t_compare,
};



/*****************************************************************************/
/*
 * dynalloc_ctype_t is just a way to ensure that the packed encoding is
 * different than the C and string encodings.  They are all mutually
 * distinct, so bugs are easier to detect.
 *  - semantics: up to 8 unsisgned chars
 *  - packed encoding: dynalloc, up to 8 unsigned chars.
 *  - C encoding: array of uint16_6, with each index i shifted by i
 *     "23:ab" => { 0x23<<0, 0xab<<1 }
 */
static void dynalloc_ctype_t_realloc(
  ProtobufCInstance *instance,
  dynalloc_ctype_t* dynalloc,
  size_t in_size)
{
  if (in_size != dynalloc->n) {
    if (nullptr == dynalloc->x) {
      ++ctype_calls_to_alloc;
    }
    dynalloc->x = (uint16_t*)realloc(dynalloc->x, sizeof(dynalloc->x[0]) * in_size);
    if (nullptr == dynalloc->x) {
      ++ctype_calls_to_free;
    }
    RW_ASSERT(in_size == 0 || dynalloc->x);
  }
}

static size_t dynalloc_ctype_t_get_packed_size(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member)
{
  ++ctype_calls_to_get_packed_size;
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &dynalloc_ctype_t_helper);
  RW_ASSERT(void_member);
  const dynalloc_ctype_t* dynalloc = (const dynalloc_ctype_t*)void_member;
  return dynalloc->n;
}

static size_t dynalloc_ctype_t_pack(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member,
  uint8_t* out)
{
  ++ctype_calls_to_pack;
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &dynalloc_ctype_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(out);
  const dynalloc_ctype_t* dynalloc = (const dynalloc_ctype_t*)void_member;
  if (dynalloc->n) {
    RW_ASSERT(dynalloc->x);
  }
  for (unsigned i = 0; i < dynalloc->n; ++i) {
    uint16_t v = (uint16_t)(dynalloc->x[i] >> (i&7));
    RW_ASSERT(v < 256);
    out[i] = (uint8_t)v;
  }
  return dynalloc->n;
}

static protobuf_c_boolean dynalloc_ctype_t_unpack(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  size_t in_size,
  const uint8_t* in,
  protobuf_c_boolean maybe_clear,
  void* void_member)
{
  ++ctype_calls_to_unpack;
  UNUSED(fielddesc);
  UNUSED(maybe_clear);
  RW_ASSERT(ctypedesc == &dynalloc_ctype_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(in);
  RW_ASSERT(instance);
  dynalloc_ctype_t* dynalloc = (dynalloc_ctype_t*)void_member;
  if (!maybe_clear) {
    dynalloc->x = NULL;
  }
  dynalloc_ctype_t_realloc(instance, dynalloc, in_size);
  for (unsigned i = 0; i < in_size; ++i) {
    dynalloc->x[i] = (uint16_t)in[i] << (i&7);
  }
  dynalloc->n = in_size;
  return TRUE;
}

static protobuf_c_boolean dynalloc_ctype_t_to_string(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member,
  size_t* str_size,
  char* str)
{
  ++ctype_calls_to_to_string;
  UNUSED(fielddesc);
  RW_ASSERT(str_size);
  RW_ASSERT(ctypedesc == &dynalloc_ctype_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(str);

  size_t packed = dynalloc_ctype_t_get_packed_size(
    ctypedesc,
    fielddesc,
    void_member);
  if (*str_size < packed) {
    return FALSE;
  }
  size_t actual = dynalloc_ctype_t_pack(
    ctypedesc,
    fielddesc,
    void_member,
    (uint8_t*)str);
  RW_ASSERT(packed == actual);
  *str_size = actual;
  return TRUE;
}

static protobuf_c_boolean dynalloc_ctype_t_from_string(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const char* str,
  size_t str_len,
  protobuf_c_boolean maybe_clear,
  void* void_member)
{
  ++ctype_calls_to_from_string;
  return dynalloc_ctype_t_unpack(instance,
                                 ctypedesc,
                                 fielddesc,
                                 str_len,
                                 (const uint8_t*)str,
                                 maybe_clear,
                                 void_member);
}

static protobuf_c_boolean dynalloc_ctype_t_init_usebody(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  void* void_member)
{
  ++ctype_calls_to_init_usebody;
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &dynalloc_ctype_t_helper);
  RW_ASSERT(void_member);
  dynalloc_ctype_t* dynalloc = (dynalloc_ctype_t*)void_member;
  dynalloc->x = NULL;
  dynalloc->n = 0;
  return TRUE;
}

static void dynalloc_ctype_t_free_usebody(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  void* void_member)
{
  ++ctype_calls_to_free_usebody;
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &dynalloc_ctype_t_helper);
  RW_ASSERT(void_member);
  RW_ASSERT(instance);
  dynalloc_ctype_t* dynalloc = (dynalloc_ctype_t*)void_member;
  if (dynalloc->x) {
    instance->free(instance, dynalloc->x);
    dynalloc->x = NULL;
  }
}

static protobuf_c_boolean dynalloc_ctype_t_check(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_member)
{
  ++ctype_calls_to_check;
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &dynalloc_ctype_t_helper);
  RW_ASSERT(void_member);
  const dynalloc_ctype_t* dynalloc = (const dynalloc_ctype_t*)void_member;
  if (dynalloc->n && !dynalloc->x) {
    return FALSE;
  }
  return TRUE;
}

static protobuf_c_boolean dynalloc_ctype_t_deep_copy(
  ProtobufCInstance *instance,
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_copy_from,
  protobuf_c_boolean maybe_clear,
  void* void_copy_to)
{
  ++ctype_calls_to_deep_copy;
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &dynalloc_ctype_t_helper);
  RW_ASSERT(void_copy_from);
  RW_ASSERT(void_copy_to);
  RW_ASSERT(instance);
  const dynalloc_ctype_t* copy_from = (const dynalloc_ctype_t*)void_copy_from;
  dynalloc_ctype_t* copy_to = (dynalloc_ctype_t*)void_copy_to;
  if (!maybe_clear) {
    copy_to->x = NULL;
  }
  dynalloc_ctype_t_realloc(instance, copy_to, copy_from->n);
  memcpy(copy_to->x, copy_from->x, copy_from->n*sizeof(copy_to->x[0]));
  copy_to->n = copy_from->n;
  return TRUE;
}

static int dynalloc_ctype_t_compare(
  const ProtobufCCTypeDescriptor* ctypedesc,
  const ProtobufCFieldDescriptor* fielddesc,
  const void* void_a,
  const void* void_b)
{
  ++ctype_calls_to_compare;
  UNUSED(fielddesc);
  RW_ASSERT(ctypedesc == &dynalloc_ctype_t_helper);
  RW_ASSERT(void_a);
  RW_ASSERT(void_b);
  const dynalloc_ctype_t* a = (const dynalloc_ctype_t*)void_a;
  const dynalloc_ctype_t* b = (const dynalloc_ctype_t*)void_b;
  RW_ASSERT(a->n == 0 || a->x);
  RW_ASSERT(b->n == 0 || b->x);
  for (unsigned i = 0; i < a->n && i < b->n; ++i) {
    if (a->x[i] < b->x[i]) {
      return -1;
    } else if (a->x[i] > b->x[i]) {
      return 1;
    }
  }
  if (a->n < b->n) {
    return -1;
  } else if (a->n > b->n) {
    return 1;
  }
  return 0;
}

const ProtobufCCTypeDescriptor dynalloc_ctype_t_helper = {
  &dynalloc_ctype_t_get_packed_size,
  &dynalloc_ctype_t_pack,
  &dynalloc_ctype_t_unpack,
  &dynalloc_ctype_t_to_string,
  &dynalloc_ctype_t_from_string,
  &dynalloc_ctype_t_init_usebody,
  &dynalloc_ctype_t_free_usebody,
  &dynalloc_ctype_t_check,
  &dynalloc_ctype_t_deep_copy,
  &dynalloc_ctype_t_compare,
};

