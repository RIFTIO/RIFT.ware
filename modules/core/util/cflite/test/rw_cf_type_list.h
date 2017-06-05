/* STANDARD_RIFT_IO_COPYRIGHT */
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
