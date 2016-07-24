
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
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
