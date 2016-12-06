/*COPYRIGHT**
    Copyright (C) 2012-2014 Intel Corporation.  All Rights Reserved.

    This file is part of SEP Development Kit

    SEP Development Kit is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    SEP Development Kit is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with SEP Development Kit; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    As a special exception, you may use this file as part of a free software
    library without restriction.  Specifically, if other files instantiate
    templates or use macros or inline functions from this file, or you compile
    this file and link it with other files to produce an executable, this
    file does not by itself cause the resulting executable to be covered by
    the GNU General Public License.  This exception does not however
    invalidate any other reasons why the executable file might be covered by
    the GNU General Public License.
**COPYRIGHT*/


#include "lwpmudrv_defines.h"
#include <linux/version.h>
#include <linux/wait.h>
#include <linux/fs.h>

#include "lwpmudrv_types.h"
#include "rise_errors.h"
#include "lwpmudrv_ecb.h"
#include "lwpmudrv_struct.h"

#include "lwpmudrv.h"
#include "utility.h"
#include "control.h"
#include "output.h"
#include "ecb_iterators.h"
#include "hsx_unc_sbox.h"
#include "unc_common.h"

static DRV_BOOL
unc_sbox_hsx_is_Unit_Ctl (
    U32 msr_addr
)
{
    return (IS_THIS_HASWELL_SERVER_SBOX_BOX_CTL_MSR(msr_addr));
}

static DRV_BOOL
unc_sbox_hsx_is_PMON_Ctl (
    U32 msr_addr
)
{
    return (IS_THIS_HASWELL_SERVER_SBOX_EVSEL_PMON_CTL_MSR(msr_addr));
}

DEVICE_CALLBACK_NODE  unc_sbox_hsx_callback = {
    NULL,
    NULL,
    unc_sbox_hsx_is_Unit_Ctl,
    unc_sbox_hsx_is_PMON_Ctl
};
 
/******************************************************************************************
 * @fn          static VOID unc_sbox_hsx_Write_PMU(VOID*)
 *
 * @brief       Initial write of PMU registers
 *              Walk through the enties and write the value of the register accordingly.
 *
 * @param       None
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 ******************************************************************************************/
static VOID
unc_sbox_hsx_Write_PMU (
    VOID  *param
)
{
    UNC_COMMON_MSR_Write_PMU(param, 
                             HASWELL_SERVER_UBOX_GLOBAL_CONTROL_MSR,
                             0,
                             RESET_SBOX_CTRS,
                             &unc_sbox_hsx_callback);

    return;
}

/******************************************************************************************
 * @fn         static VOID unc_sbox_hsx_Disable_PMU(PVOID)
 *
 * @brief      Disable the per unit global control to stop the PMU counters.
 *
 * @param      Device Index of this PMU unit
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 ******************************************************************************************/
static VOID
unc_sbox_hsx_Disable_PMU (
    PVOID  param
)
{
    UNC_COMMON_MSR_Disable_PMU(param, 
                               HASWELL_SERVER_UBOX_GLOBAL_CONTROL_MSR,
                               DISABLE_HASWELL_SERVER_SBOX_COUNTERS,
                               0,
                               &unc_sbox_hsx_callback);
    return;
}

/******************************************************************************************
 * @fn         static VOID unc_sbox_hsx_Enable_PMU(PVOID)
 *
 * @brief      Set the enable bit for all the EVSEL registers
 *
 * @param      Device Index of this PMU unit
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 ******************************************************************************************/
static VOID
unc_sbox_hsx_Enable_PMU (
    PVOID   param
)
{
    UNC_COMMON_MSR_Enable_PMU(param, 
                              HASWELL_SERVER_UBOX_GLOBAL_CONTROL_MSR,
                              ENABLE_ALL_HASWELL_SERVER_SBOX_PMU, 
                              ENABLE_HASWELL_SERVER_SBOX_COUNTERS, 
                              DISABLE_HASWELL_SERVER_SBOX_COUNTERS,
                              &unc_sbox_hsx_callback);

    return;
}

/*
 * Initialize the dispatch table
 */
DISPATCH_NODE  haswell_server_sbox_dispatch =
{
    NULL,                                // initialize
    NULL,                                // destroy
    unc_sbox_hsx_Write_PMU,              // write
    unc_sbox_hsx_Disable_PMU,            // freeze
    unc_sbox_hsx_Enable_PMU,             // restart
    UNC_COMMON_MSR_Read_PMU_Data,        // read
    NULL,                                // check for overflow
    NULL,                                //swap group
    NULL,                                //read lbrs
    NULL,                                //cleanup
    NULL,                                //hw errata
    NULL,                                //read power
    NULL,                                //check overflow errata
    UNC_COMMON_MSR_Read_Counts,          //read counts
    NULL,                                //check overflow gp errata
    NULL,                                // read_ro
    NULL,                                //platform info
    NULL,
    NULL                                 // scan for uncore
};
