
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
 * @file rw_cf_type_list.h
 * @author Rajesh Ramankutty (rajesh.ramankutty@riftio.com)
 * @date 02/12/14
 * @header file to add a RW TYPE to CF-like validation, allocation, etc.
 */

#ifndef __RW_CF_TYPE_LIST_H__
#define __RW_CF_TYPE_LIST_H__

#include "rw_cf_type_validate.h"

__BEGIN_DECLS

#define RW_CF_TYPE_LIST                                   \
  RW_CF_TYPE("rwtype_01_ptr_t", rwtype_01_ptr_t)                  \
  RW_CF_TYPE("rwtype_02_ptr_t", rwtype_02_ptr_t)                  \
  RW_CF_TYPE("rwtype_03_ptr_t", rwtype_03_ptr_t)                  \
  RW_CF_TYPE("rwtype_04_ptr_t", rwtype_04_ptr_t)

#undef RW_CF_TYPE
#define RW_CF_TYPE(a, b) RW_CF_TYPE_DECLARE(b)

RW_CF_TYPE_LIST

void rw_cf_register_types();

__END_DECLS

#endif //__RW_CF_TYPE_LIST_H_
