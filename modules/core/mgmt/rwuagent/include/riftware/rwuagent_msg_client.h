
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwuagent_msg_client.h
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @date 05/23/2014
 * @brief uagent client functions
 * @details uagent client functions
 */

#ifndef __RWUAGENT_CLIENT_H__
#define __RWUAGENT_CLIENT_H__

#include "rwtrace.h"
#include "rwmsg.h"
#include "rwtasklet.h"
#include "rwuagent.pb-c.h"
#include "rw_xml.h"

#define RWUAGENT_PATH "/R/RW.uAgent/1"

struct rwuagent_client_info_s {
  CFRuntimeBase _base;
  rwmsg_clichan_t *cc;
  RwUAgent_Client rwuacli;
  rwtasklet_info_ptr_t rwtasklet_info;
  rwmsg_destination_t *dest;
  void *user_context;
  uint32_t registration_retry_attempt;
};

RW_TYPE_DECL(rwuagent_client_info);
RW_CF_TYPE_EXTERN(rwuagent_client_info_ptr_t);


/**
 * This function registers with the uagent as client. RWCLI
 * calls this function during the tasklet initalization.
 *
 * @param[in] rwuagent_info Pointer to the microagent info structure
 * tasklet instance id is used to register a unique name with the broker
 */
rw_status_t rwtasklet_register_uagent_client(rwuagent_client_info_ptr_t rwuagent_info);
#endif //__RWUAGENT_CLIENT_H__


