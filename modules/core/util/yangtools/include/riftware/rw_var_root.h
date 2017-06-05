/* STANDARD_RIFT_IO_COPYRIGHT */

/*
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(BEGIN)
 * Creation Date: 6/6/16
 * RIFT_IO_STANDARD_CMAKE_COPYRIGHT_HEADER(END)
 */

#ifndef RW_VAR_ROOT_H_
#define RW_VAR_ROOT_H_

#include <rwlib.h>

#if __cplusplus
#if __cplusplus < 201103L
#error "Requires C++11"
#endif
#endif

__BEGIN_DECLS

/*!
 * Get the value of RIFT_VAR_NAME 
 *
 * @param[in]  uid The user id. Can be NULL.
 * @param[in]  vnf The vnf name. Can be NULL.
 * @param[in]  vm The vm name. Can be NULL.
 * @param[out] buf_out The out buffer to fill RVN.
 * @param[in]  buf_len The length of buf_out
 *
 * @return RW_STATUS_SUCCESS on success, RW_STATUS_FAILURE otherwise
 */
rw_status_t rw_util_get_rift_var_name(const char* uid,
                                      const char* vnf,
                                      const char* vm,
                                      char* buf_out,
                                      int buf_len);

/*!
 * Get the value of RIFT_VAR_ROOT
 *
 * @param[in]  uid The user id. Can be NULL.
 * @param[in]  vnf The vnf name. Can be NULL.
 * @param[in]  vm The vm name. Can be NULL.
 * @param[out] buf_out The out buffer to fill RVN.
 * @param[in]  buf_len The length of buf_out
 *
 * @return RW_STATUS_SUCCESS on success, RW_STATUS_FAILURE otherwise
 */
rw_status_t rw_util_get_rift_var_root(const char* uid,
                                      const char* vnf,
                                      const char* vm,
                                      char *buf_out,
                                      int buf_len);

/*!
 * Create RIFT_VAR_ROOT directory hierarchy
 *
 * @param[in] rift_var_root The RVR path value.
 *
 * @return RW_STATUS_SUCCESS if the directories are created
 *         successfully, RW_STATUS_FAILURE otherwise.
 */
rw_status_t rw_util_create_rift_var_root(const char* rift_var_root);

/*!
 * Destroy RIFT_VAR_ROOT directory hierarchy
 *
 * @param[in] rift_var_root The RVR path value.
 *
 * @return RW_STATUS_SUCCESS if the directories are destroyed
 *         successfully, RW_STATUS_FAILURE otherwise.
 */
rw_status_t rw_util_remove_rift_var_root(const char* rift_var_root);

__END_DECLS

#endif // RW_VAR_ROOT_H_
