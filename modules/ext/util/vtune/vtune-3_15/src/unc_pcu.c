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
#include "ivt_unc_ubox.h"
#include "ivt_unc_pcu.h"
#include "hsx_unc_pcu.h"
#include "ecb_iterators.h"
#include "pebs.h"
#include "inc/pci.h"
#include "unc_common.h"

extern U64           *read_unc_ctr_info;
/*************************************************************************/

static DRV_BOOL
unc_pcu_ivt_is_PMON_Ctl (
    U32 msr_addr
)
{
    return (IS_THIS_PCU_BOX_EVSEL_CTL_MSR(msr_addr));
}

static DRV_BOOL
unc_pcu_ivt_is_Unit_Ctl (
    U32 msr_addr
)
{
    return (msr_addr == IVYTOWN_PCU_MSR_PMON_BOX_CTL);
}

DEVICE_CALLBACK_NODE  unc_pcu_ivt_callback = {
    NULL,
    NULL,
    unc_pcu_ivt_is_Unit_Ctl,
    unc_pcu_ivt_is_PMON_Ctl
};


/*!
 * @fn          static VOID unc_pcu_ivt_Write_PMU(VOID*)
 *
 * @brief       Initial write of PMU registers
 *              Walk through the enties and write the value of the register accordingly.
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 */
static VOID
unc_pcu_ivt_Write_PMU (
    VOID  *param
)
{
    UNC_COMMON_MSR_Write_PMU(param, 
                             IVYTOWN_UBOX_GLOBAL_CONTROL_MSR,
                             0, 
                             0, 
                             NULL);
    return;
}


/*!
 * @fn         static VOID unc_pcu_ivt_Disable_PMU(PVOID)
 *
 * @brief      Set box level control register bit to stop PMU counters
 *
 * @param      None
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 */
static VOID
unc_pcu_ivt_Disable_PMU (
    PVOID  param
)
{
    UNC_COMMON_MSR_Disable_PMU(param,
                               IVYTOWN_UBOX_GLOBAL_CONTROL_MSR,
                               DISABLE_PCU_COUNTERS,
                               0,
                               &unc_pcu_ivt_callback);

    return;
}

/*!
 * @fn         static VOID unc_pcu_ivt_Enable_PMU(PVOID)
 *
 * @brief      Set the enable bit for all the evsel registers
 *
 * @param      None
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 */
static VOID
unc_pcu_ivt_Enable_PMU (
    PVOID   param
)
{
    UNC_COMMON_MSR_Enable_PMU(param, 
                              IVYTOWN_UBOX_GLOBAL_CONTROL_MSR,
                              0,
                              0,
                              ENABLE_PCU_COUNTERS,
                              &unc_pcu_ivt_callback);

    return;
}


/***************************************************************************/
static DRV_BOOL
unc_pcu_hsx_is_Unit_Ctl (
    U32 msr_addr
)
{
    return (IS_THIS_HASWELL_SERVER_PCU_BOX_CTL_MSR(msr_addr));
}

static DRV_BOOL
unc_pcu_hsx_is_PMON_Ctl (
    U32 msr_addr
)
{
    return (IS_THIS_HASWELL_SERVER_PCU_BOX_EVSEL_CTL_MSR(msr_addr));
}


DEVICE_CALLBACK_NODE  unc_pcu_hsx_callback = {
    NULL,
    NULL,
    unc_pcu_hsx_is_Unit_Ctl,
    unc_pcu_hsx_is_PMON_Ctl
};


/*!
 * @fn          static VOID unc_pcu_hsx_Write_PMU(VOID*)
 *
 * @brief       Initial write of PMU registers
 *              Walk through the enties and write the value of the register accordingly.
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 */
static VOID
unc_pcu_hsx_Write_PMU (
    VOID  *param
)
{
    UNC_COMMON_MSR_Write_PMU (param,
                              HASWELL_SERVER_UBOX_UNIT_GLOBAL_CONTROL_MSR,
                              0, 
                              RESET_PCU_CTRS,
                              &unc_pcu_hsx_callback); 
    return;
}
/*!
 * @fn         static VOID unc_pcu_hsx_Disable_PMU(PVOID)
 *
 * @brief      Set box level control register bit to stop PMU counters
 *
 * @param      None
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 */
static VOID
unc_pcu_hsx_Disable_PMU (
    PVOID  param
)
{
    UNC_COMMON_MSR_Disable_PMU(param,
                               HASWELL_SERVER_UBOX_UNIT_GLOBAL_CONTROL_MSR,
                               DISABLE_HASWELL_SERVER_PCU_COUNTERS,
                               0,
                               &unc_pcu_hsx_callback);
    return;
}

/*!
 * @fn         static VOID unc_pcu_hsx_Enable_PMU(PVOID)
 *
 * @brief      Set the enable bit for all the evsel registers
 *
 * @param      None
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 */
static VOID
unc_pcu_hsx_Enable_PMU (
    PVOID   param
)
{
    UNC_COMMON_MSR_Enable_PMU(param,
                              HASWELL_SERVER_UBOX_UNIT_GLOBAL_CONTROL_MSR,
                              0,
                              DISABLE_HASWELL_SERVER_PCU_COUNTERS,
                              ENABLE_HASWELL_SERVER_PCU_COUNTERS,
                              &unc_pcu_hsx_callback);
    return;
}


/*
 * Initialize the dispatch table
 */
DISPATCH_NODE  haswell_server_pcu_dispatch =
{
    NULL,                             // initialize
    NULL,                             // destroy
    unc_pcu_hsx_Write_PMU,            // write
    unc_pcu_hsx_Disable_PMU,          // freeze
    unc_pcu_hsx_Enable_PMU,           // restart
    UNC_COMMON_MSR_Read_PMU_Data,     // read
    NULL,                             // check for overflow
    NULL,                             // swap group
    NULL,                             // read lbrs
    NULL,                             // cleanup
    NULL,                             // hw errata
    NULL,                             // read power
    NULL,                             // check overflow errata
    UNC_COMMON_MSR_Read_Counts,       // read counts
    NULL,                             // check overflow gp errata
    NULL,                             // read_ro
    NULL,                             // platform info
    NULL,
    NULL                              // scan for uncore
};


/*
 * Initialize the dispatch table
 */
DISPATCH_NODE  ivytown_pcu_dispatch =
{
    NULL,                           // initialize
    NULL,                           // destroy
    unc_pcu_ivt_Write_PMU,          // write
    unc_pcu_ivt_Disable_PMU,        // freeze
    unc_pcu_ivt_Enable_PMU,         // restart
    UNC_COMMON_MSR_Read_PMU_Data,   // read
    NULL,                           // check for overflow
    NULL,                           // swap group
    NULL,                           // read lbrs
    NULL,                           // cleanup
    NULL,                           // hw errata
    NULL,                           // read power
    NULL,                           // check overflow errata
    UNC_COMMON_MSR_Read_Counts,     // read counts
    NULL,                           // check overflow gp errata
    NULL,                           // read_ro
    NULL,                           // platform info
    NULL,
    NULL                            // scan for uncore
};
