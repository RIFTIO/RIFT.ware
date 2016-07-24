
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */


/**
 * @file rwmc_log.h
 * @author Anil Gunturu (anil.gunturu@riftio.com)
 * @date 08/14/2015
 * @brief Internal logging macros for rwmc
 *
 */

#include "rw-mc-log.pb-c.h"
#include "rw-log.pb-c.h"
#include "rwlog.h"

// logging macros
#define RWMC_LOG_HANDLE(_inst)                         \
      ((_inst)->rwtasklet_info->rwlog_instance)

#define RWMC_LOG_EVENT(__inst__, __evt__, ...) \
      RWLOG_EVENT(RWMC_LOG_HANDLE(__inst__), RwMcLog_notif_##__evt__, __VA_ARGS__) 
