/* STANDARD_RIFT_IO_COPYRIGHT */
/**
 * @file rw_cf_type_validate_test.h
 * @author Rajesh Ramankutty (rajesh.ramankutty@riftio.com)
 * @date 02/12/14
 * @header file to add a RW TYPE to CF-like validation, allocation, etc.
 */

#ifndef _RW_CF_TYPE_VALIDATE_TEST_H___
#define _RW_CF_TYPE_VALIDATE_TEST_H___

typedef struct rwtype_01 {
#ifdef _CF_
    CFRuntimeBase _base;
#endif // _CF_
    gboolean abcd;
    gboolean efgh;
} *rwtype_01_ptr_t;

typedef struct rwtype_02 {
#ifdef _CF_
    //rw_object_t ro;
    CFRuntimeBase _base;
#endif // _CF_
    gboolean abcd;
    gboolean efgh;
} *rwtype_02_ptr_t;

struct rwtype_05 {
#ifdef _CF_
    //rw_object_t ro;
    CFRuntimeBase _base;
#endif // _CF_
    gboolean abcd;
    gboolean efgh;
};
typedef struct rwtype_05 *rwtype_05_ptr_t;

RW_CF_TYPE_EXTERN(rwtype_05_ptr_t);

#endif // _RW_CF_TYPE_VALIDATE_TEST_H___
