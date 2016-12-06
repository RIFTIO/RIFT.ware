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
 *
 */

#ifndef __RWTASKLET_VAR_ROOT_H__
#define __RWTASKLET_VAR_ROOT_H__

#include <rwlib.h>
#include "rwtasklet.h"

__BEGIN_DECLS

/*!
 * Get the value of RIFT_VAR_NAME
 *
 * @param tasklet_info - The tasklet info pointer.
 *         
 * @return  - The value of RIFT_VAR_NAME. 
 *            Caller owns the memory.
 */
///@cond GI_SCANNER
/**
 * rwtasklet_info_get_rift_var_name:
 * Returns: (nullable) (transfer full)
 */
///@endcond
char* rwtasklet_info_get_rift_var_name(rwtasklet_info_t *tasklet_info);

/*!
 * Get the value of RIFT_VAR_ROOT
 *
 * @param tasklet_info - The tasklet info pointer.
 *
 * @return         - The value of RIFT_VAR_ROOT,
 *                   Caller owns the memory.
 */
///@cond GI_SCANNER
/**
 * rwtasklet_info_get_rift_var_root:
 * Returns: (nullable) (transfer full)
 */
///@endcond
char* rwtasklet_info_get_rift_var_root(rwtasklet_info_t *tasklet_info);

/*!
 * Create RIFT_VAR_ROOT directory hierarchy.
 *
 * @param rift_var_root - The RIFT_VAR_ROOT directory
 *                        path to create
 * @return              - RW_STATUS_SUCCESS if the directories are
 *                        created successfully.
 *                        RW_STATUS_FAILURE otherwise.
 */
rw_status_t rwtasklet_create_rift_var_root(const char* rift_var_root);

/*!
 * Destory RIFT_VAR_ROOT directory hierarchy.
 *
 * @param rift_var_root - The RIFT_VAR_ROOT directory
 *                        path to create
 * @return              - RW_STATUS_SUCCESS if the directories are
 *                        created successfully.
 *                        RW_STATUS_FAILURE otherwise.
 */
rw_status_t rwtasklet_destory_rift_var_root(const char* rift_var_root);

__END_DECLS

#endif // __RWTASKLET_VAR_ROOT_H__
