
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
 * @file rwmemlog_c_test.c
 * @date 2015/09/17
 * @brief C testing for rwmemlog
 */

#include <rwmemlog.h>
#include <rwmemlog_mgmt.h>
#include "rwmemlog_c_test.h"

#include <inttypes.h>


static const char* dummy_c_enum_func( int enumval )
{
  return "dummy";
}

rw_status_t rwmemlog_c_test_macros(
  rwmemlog_buffer_t** buf )
{
  rwmemlog_buffer_header_t* header = &(*buf)->header;

  RWMEMLOG( buf, RWMEMLOG_MEM2, "intptr test",
            RWMEMLOG_ARG_PRINTF_INTPTR("%12" PRIdPTR, 1) );
  if (header->used_units != 4) return RW_STATUS_FAILURE;

  RWMEMLOG( buf, RWMEMLOG_MEM2, "strncpy test",
            RWMEMLOG_ARG_STRNCPY(12,"yellow dog") );
  if (header->used_units != 9) return RW_STATUS_FAILURE;

  enum { junk = 1 } k = junk;
  RWMEMLOG( buf, RWMEMLOG_MEM2, "enum test",
            RWMEMLOG_ARG_ENUM_FUNC(dummy_c_enum_func,"junk",k) );
  if (header->used_units != 13) return RW_STATUS_FAILURE;

  RWMEMLOG( buf, RWMEMLOG_MEM2, "arg count test",
             RWMEMLOG_ARG_CONST_STRING("first two are implied"),
             RWMEMLOG_ARG_CONST_STRING("4th"),
             RWMEMLOG_ARG_CONST_STRING("5th"),
             RWMEMLOG_ARG_CONST_STRING("6th"),
             RWMEMLOG_ARG_CONST_STRING("7th"),
             RWMEMLOG_ARG_CONST_STRING("8th"),
             RWMEMLOG_ARG_CONST_STRING("9th"),
             RWMEMLOG_ARG_CONST_STRING("tenth") );
  if (header->used_units != 16) return RW_STATUS_FAILURE;

  RWMEMLOG( buf, RWMEMLOG_MEM2, "extra comma", );
  if (header->used_units != 19) return RW_STATUS_FAILURE;

  RWMEMLOG( buf, RWMEMLOG_MEM2, "simple no comma" );
  if (header->used_units != 22) return RW_STATUS_FAILURE;

  RWMEMLOG( buf, RWMEMLOG_MEM2, "arg count test",
            RWMEMLOG_ARG_STRNCPY( 6,"123456789_1234567890"),
            RWMEMLOG_ARG_STRNCPY( 7,"123456789_1234567890"),
            RWMEMLOG_ARG_STRNCPY( 8,"123456789_1234567890"),
            RWMEMLOG_ARG_STRNCPY( 9,"123456789_1234567890"),
            RWMEMLOG_ARG_STRNCPY(14,"123456789_1234567890"),
            RWMEMLOG_ARG_STRNCPY(15,"123456789_1234567890"),
            RWMEMLOG_ARG_STRNCPY(16,"123456789_1234567890"),
            RWMEMLOG_ARG_STRNCPY(17,"123456789_1234567890") );
  if (header->used_units != 39) return RW_STATUS_FAILURE;

  RWMEMLOG_TIME_CODE(
    (int i = 3; usleep(123);),
    buf, RWMEMLOG_MEM2, "time code" );
  if (3 != i) return RW_STATUS_FAILURE;

  RWMEMLOG_TIME_CODE_RTT(
    buf, RWMEMLOG_MEM2, "time code rtt",
    (usleep(34);) );

  return RW_STATUS_SUCCESS;
}
