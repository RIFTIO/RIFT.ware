
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
 * @file rw_cf_type_list.c
 * @author Rajesh Ramankutty (rajesh.ramankutty@riftio.com)
 * @date 02/12/14
 * @file to define data-structures and reister RW types with CF
 * @RW types that need to be registered are define in rw_cf_type_list.h
 */

#include "rw_cf_type_list.h"

#undef RW_CF_TYPE
#define RW_CF_TYPE(a, b) RW_CF_TYPE_DEFINE(a, b)

RW_CF_TYPE_LIST

Boolean rw_cf_registerered = 0;

void
rw_cf_register_types()
{
  if (!rw_cf_registerered) {
    rw_cf_registerered = 1;
#undef RW_CF_TYPE
#define RW_CF_TYPE(a, b) _RW_CF_TYPE_ID(b) = _CFRuntimeRegisterClass(&_RW_CF_CF_RUNTIME(b));

RW_CF_TYPE_LIST
  }
}

void
rw_cf_deregister_types()
{
  rw_cf_registerered = 0;
}
