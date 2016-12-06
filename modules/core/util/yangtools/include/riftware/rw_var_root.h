
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
 * Creation Date: 6/6/16
 * 
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
