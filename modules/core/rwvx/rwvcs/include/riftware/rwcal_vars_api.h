
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
 * @file rwcal_api.h
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @date 06/06/2014
 * @brief Top level API include for RW.CAL component
 */

#ifndef __RWCAL_VARS_API_H__
#define __RWCAL_VARS_API_H__

__BEGIN_DECLS

struct rwvcs_instance_s;

rw_status_t rwvcs_variable_evaluate_str(
    struct rwvcs_instance_s *instance,
    const char * variable,
    char * result,
    int result_len);

rw_status_t rwvcs_variable_evaluate_int(struct rwvcs_instance_s *instance,
                                        char *variable,
                                        int *result);

rw_status_t rwvcs_variable_list_evaluate(struct rwvcs_instance_s * instance,
                                         size_t n_variable,
                                         char **variable);

rw_status_t rwvcs_evaluate_python_loop_variables(struct rwvcs_instance_s *instance,
                                                 char *variable);

__END_DECLS

#endif // __RWCAL_VARS_API_H__
