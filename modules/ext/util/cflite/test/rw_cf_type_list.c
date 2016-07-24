/* STANDARD_RIFT_IO_COPYRIGHT */                                                                                                                   
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
