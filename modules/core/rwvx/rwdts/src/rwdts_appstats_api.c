
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
 * @file   rwdts_appstats_api.c
 * @author Rajesh Velandy(rajesh.velandy@riftio.com)
 * @date   02/12/2015
 * @brief  Implements the  DTSAppStats framework
 *
 */

#include <rwdts.h>
#include <rwdts_int.h>

/*
 * static proto types
 */


/*
 * Public APIs
 */

//LCOV_EXCL_START
/*
 * Register with the DTSAppstat framework
 */
rwdts_member_reg_handle_t
rwdts_appstats_register(const rw_keyspec_path_t*               keyspec,
                        const ProtobufCMessageDescriptor* pbdesc,
                        rwdts_shard_info_detail_t*        shard,
                        void*                             rawpb,
                        size_t                            rawpb_size,
                        rwdts_appstat_reg_cb_t*           appstat)
{
  // ATTN need implementation;
  return NULL;
}

/*
 * Register with the DTSAppstat framework with a rawpb
 */
rwdts_member_reg_handle_t
rwdts_appstats_register_rawpb(const rw_keyspec_path_t*               keyspec,
                              const ProtobufCMessageDescriptor* pbdesc,
                              rwdts_shard_info_detail_t*        shard,
                              void*                             rawpb,
                              size_t                            rawpb_size,
                              rwdts_appstat_reg_cb_t*           appstat)
{
  // ATTN need implementation;
  return NULL;
}

/*
 * Register with the DTSAppstat framework with a callback
 *
 */
rwdts_member_reg_handle_t
rwdts_appstats_register_callback(const rw_keyspec_path_t*               keyspec,
                                 const ProtobufCMessageDescriptor* pbdesc,
                                 rwdts_shard_info_detail_t*        shard,
                                 void*                             rawpb,
                                 size_t                            rawpb_size,
                                 rwdts_appstat_reg_cb_t*           appstat)
{
  // ATTN need implementation;
  return NULL;
}
//LCOV_EXCL_STOP
 /*
 * Deregister from the DTSAppStats framework
 */
rw_status_t rwdts_appstats_deregister(rwdts_member_reg_handle_t);
