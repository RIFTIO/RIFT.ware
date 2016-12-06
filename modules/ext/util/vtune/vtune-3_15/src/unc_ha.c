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
#include "unc_common.h"
#include "ivt_unc_ubox.h"
#include "ivt_unc_ha.h"
#include "hsx_unc_ubox.h"
#include "hsx_unc_ha.h"
#include "ecb_iterators.h"
#include "inc/pci.h"

static DRV_BOOL
unc_ha_ivt_is_Valid(
    U32 device_id
) 
{
    if ((device_id != IVYTOWN_HA_DID) &&
        (device_id != IVYTOWN_HA1_DID)) {
        return FALSE;
    }
    return TRUE;
}

static DRV_BOOL
unc_ha_ivt_is_Valid_For_Write(
    U32 device_id,
    U32 reg_id
) 
{
    return unc_ha_ivt_is_Valid(device_id);
}

static DRV_BOOL
unc_ha_ivt_is_PMON_Ctl(
    U32 pci_addr
) 
{
    return (IS_THIS_HA_PCI_PMON_CTL(pci_addr));
}

static DRV_BOOL
unc_ha_ivt_is_Unit_Ctl(
    U32 pci_addr
) 
{
    return (IS_THIS_BOX_HA_PCI_UNIT_CTL(pci_addr));
}

DEVICE_CALLBACK_NODE  unc_ha_ivt_callback = {
    unc_ha_ivt_is_Valid,
    unc_ha_ivt_is_Valid_For_Write,
    unc_ha_ivt_is_Unit_Ctl,
    unc_ha_ivt_is_PMON_Ctl
};


/*!
 * @fn          static VOID ivytown_ha_Write_PMU(VOID*)
 *
 * @brief       Initial write of PMU registers
 *              Walk through the enties and write the value of the register accordingly.
 *              When current_group = 0, then this is the first time this routine is called,
 *
 * @param       None
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 */
static VOID
ivytown_ha_Write_PMU (
    VOID  *param
)
{
    UNC_COMMON_PCI_Write_PMU(param, 
                            IVYTOWNUNC_SOCKETID_UBOX_DID,
                            IVYTOWN_UBOX_GLOBAL_CONTROL_MSR, 
                            RESET_CTRS_HASWELL_SERVER_HA,
                            &unc_ha_ivt_callback);
    return;
}

/******************************************************************************************
 * @fn         static VOID ivytown_ha_Enable_PMU(PVOID)
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
ivytown_ha_Enable_PMU (
    PVOID   param
)
{
    UNC_COMMON_PCI_Enable_PMU(param, 
                             IVYTOWN_UBOX_GLOBAL_CONTROL_MSR,
                             ENABLE_HA_COUNTERS,
                             DISABLE_HA_COUNTERS,
                             &unc_ha_ivt_callback);
    return;
}

/******************************************************************************************
 * @fn         static VOID ivytownunc_ha_Disable_PMU(PVOID)
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
ivytown_ha_Disable_PMU (
    PVOID  param
)
{
    UNC_COMMON_PCI_Disable_PMU(param, 
                              IVYTOWN_UBOX_GLOBAL_CONTROL_MSR,
                              DISABLE_HA_COUNTERS,
                              &unc_ha_ivt_callback);
    return;
}

/*!
 * @fn          static VOID ivytown_ha_Scan_For_Uncore(VOID*)
 *
 * @brief       Initial write of PMU registers
 *              Walk through the enties and write the value of the register accordingly.
 *              When current_group = 0, then this is the first time this routine is called,
 *
 * @param       None
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 */
static VOID
ivytown_ha_Scan_For_Uncore(
    PVOID  param
)
{
    UNC_COMMON_PCI_Scan_For_Uncore(param, UNCORE_TOPOLOGY_INFO_NODE_HA, &unc_ha_ivt_callback);

    return;
}

/***********************************************
 *  
 *  UNC ha haswell server specific functions
 *
 **********************************************/

static DRV_BOOL
unc_ha_hsx_is_Valid(
    U32 device_id
) 
{
    if ((device_id != HASWELL_SERVER_HA_DID)   &&
        (device_id != HASWELL_SERVER_HA1_DID)) {
        return FALSE;
    }
    return TRUE;
}

static DRV_BOOL
unc_ha_hsx_is_Valid_For_Write (
    U32 device_id,
    U32 reg_id
) 
{
    return unc_ha_hsx_is_Valid(device_id);
}

static DRV_BOOL
unc_ha_hsx_is_PMON_Ctl(
    U32 pci_addr
) 
{
    if (IS_THIS_HASWELL_SERVER_HA_PCI_PMON_CTL(pci_addr)) {
        return TRUE;
    }
    return FALSE;
}

static DRV_BOOL
unc_ha_hsx_is_Unit_Ctl(
    U32 pci_addr
) 
{
    if (IS_THIS_HASWELL_SERVER_BOX_HA_PCI_UNIT_CTL(pci_addr)) {
        return TRUE;
    }
    return FALSE;
}

DEVICE_CALLBACK_NODE  unc_ha_hsx_callback = {
    unc_ha_hsx_is_Valid,
    unc_ha_hsx_is_Valid_For_Write,
    unc_ha_hsx_is_Unit_Ctl,
    unc_ha_hsx_is_PMON_Ctl
};



/*!
 * @fn          static VOID haswell_server_ha_Write_PMU(VOID*)
 *
 * @brief       Initial write of PMU registers
 *              Walk through the enties and write the value of the register accordingly.
 *              When current_group = 0, then this is the first time this routine is called,
 *
 * @param       None
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 */
static VOID
haswell_server_ha_Write_PMU (
    VOID  *param
)
{
    UNC_COMMON_PCI_Write_PMU(param, 
                            HASWELL_SERVER_SOCKETID_UBOX_DID,
                            HASWELL_SERVER_UBOX_GLOBAL_CONTROL_MSR, 
                            RESET_CTRS_HASWELL_SERVER_HA,
                            &unc_ha_hsx_callback);
    return;
}

/******************************************************************************************
 * @fn         static VOID haswell_server_ha_Enable_PMU(PVOID)
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
haswell_server_ha_Enable_PMU (
    PVOID   param
)
{
    UNC_COMMON_PCI_Enable_PMU(param, 
                             HASWELL_SERVER_UBOX_GLOBAL_CONTROL_MSR,
                             ENABLE_HASWELL_SERVER_HA_COUNTERS,
                             DISABLE_HASWELL_SERVER_HA_COUNTERS,
                             &unc_ha_hsx_callback);
    return;
}

/******************************************************************************************
 * @fn         static VOID haswell_server_ha_Disable_PMU(PVOID)
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
haswell_server_ha_Disable_PMU (
    PVOID  param
)
{
    UNC_COMMON_PCI_Disable_PMU(param, 
                              HASWELL_SERVER_UBOX_GLOBAL_CONTROL_MSR,
                              DISABLE_HASWELL_SERVER_HA_COUNTERS,
                              &unc_ha_hsx_callback);
    return;
}

/*!
 * @fn          static VOID haswell_server_ha_Scan_For_Uncore(VOID*)
 *
 * @brief       Initial write of PMU registers
 *              Walk through the enties and write the value of the register accordingly.
 *              When current_group = 0, then this is the first time this routine is called,
 *
 * @param       None
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 */
static VOID
haswell_server_ha_Scan_For_Uncore(
    PVOID  param
)
{
    UNC_COMMON_PCI_Scan_For_Uncore(param, UNCORE_TOPOLOGY_INFO_NODE_HA, &unc_ha_hsx_callback);
    return;
}


/***********************************************
 *  
 *  UNC ha Broadwell DE specific functions
 *
 **********************************************/

static DRV_BOOL
unc_ha_bdx_de_is_Valid(
    U32 device_id
) 
{
    if ((device_id != BROADWELL_DE_HA_DID)   &&
        (device_id != BROADWELL_DE_HA1_DID)) {
        return FALSE;
    }
    return TRUE;
}

static DRV_BOOL
unc_ha_bdx_de_is_Valid_For_Write (
    U32 device_id,
    U32 reg_id
) 
{
    return unc_ha_bdx_de_is_Valid(device_id);
}

DEVICE_CALLBACK_NODE  unc_ha_bdx_de_callback = {
    unc_ha_bdx_de_is_Valid,
    unc_ha_bdx_de_is_Valid_For_Write,
    unc_ha_hsx_is_Unit_Ctl,
    unc_ha_hsx_is_PMON_Ctl
};



/*!
 * @fn          static VOID broadwell_de_ha_Write_PMU(VOID*)
 *
 * @brief       Initial write of PMU registers
 *              Walk through the enties and write the value of the register accordingly.
 *              When current_group = 0, then this is the first time this routine is called,
 *
 * @param       None
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 */
static VOID
broadwell_de_ha_Write_PMU (
    VOID  *param
)
{
    UNC_COMMON_PCI_Write_PMU(param, 
                            BROADWELL_DE_SOCKETID_UBOX_DID,
                            HASWELL_SERVER_UBOX_GLOBAL_CONTROL_MSR, 
                            RESET_CTRS_HASWELL_SERVER_HA,
                            &unc_ha_bdx_de_callback);
    return;
}

/*!
 * @fn          static VOID broadwell_de_ha_Scan_For_Uncore(VOID*)
 *
 * @brief       Initial write of PMU registers
 *              Walk through the enties and write the value of the register accordingly.
 *              When current_group = 0, then this is the first time this routine is called,
 *
 * @param       None
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 */
static VOID
broadwell_de_ha_Scan_For_Uncore(
    PVOID  param
)
{
    UNC_COMMON_PCI_Scan_For_Uncore(param, UNCORE_TOPOLOGY_INFO_NODE_HA, &unc_ha_bdx_de_callback);
    return;
}

/*
 * Initialize the dispatch table
 */
DISPATCH_NODE  ivytown_ha_dispatch =
{
    NULL,                        // initialize
    NULL,                        // destroy
    ivytown_ha_Write_PMU,        // write
    ivytown_ha_Disable_PMU,      // freeze
    ivytown_ha_Enable_PMU,       // restart
    UNC_COMMON_PCI_Read_PMU_Data,     // read
    NULL,                        // check for overflow
    NULL,                        // swap group
    NULL,                        // read lbrs
    NULL,                        // cleanup
    NULL,                        // hw errata
    NULL,                        // read power
    NULL,                        // check overflow errata
    UNC_COMMON_PCI_Read_Counts,       // read counts
    NULL,                        // check overflow gp errata
    NULL,                        // read_ro
    NULL,                        // platform info
    NULL,
    ivytown_ha_Scan_For_Uncore   // scan for uncore
};
/*
 * Initialize the dispatch table
 */
DISPATCH_NODE  haswell_server_ha_dispatch =
{
    NULL,                             // initialize
    NULL,                             // destroy
    haswell_server_ha_Write_PMU,      // write
    haswell_server_ha_Disable_PMU,    // freeze
    haswell_server_ha_Enable_PMU,     // restart
    UNC_COMMON_PCI_Read_PMU_Data,          // read
    NULL,                             // check for overflow
    NULL,                             // swap group
    NULL,                             // read lbrs
    NULL,                             // cleanup
    NULL,                             // hw errata
    NULL,                             // read power
    NULL,                             // check overflow errata
    UNC_COMMON_PCI_Read_Counts,            // read counts
    NULL,                             // check overflow gp errata
    NULL,                             // read_ro
    NULL,                             // platform info
    NULL,
    haswell_server_ha_Scan_For_Uncore // scan for uncore
};
/*
 * Initialize the dispatch table
 */
DISPATCH_NODE  broadwell_de_ha_dispatch =
{
    NULL,                             // initialize
    NULL,                             // destroy
    broadwell_de_ha_Write_PMU,      // write
    haswell_server_ha_Disable_PMU,    // freeze
    haswell_server_ha_Enable_PMU,     // restart
    UNC_COMMON_PCI_Read_PMU_Data,          // read
    NULL,                             // check for overflow
    NULL,                             // swap group
    NULL,                             // read lbrs
    NULL,                             // cleanup
    NULL,                             // hw errata
    NULL,                             // read power
    NULL,                             // check overflow errata
    UNC_COMMON_PCI_Read_Counts,            // read counts
    NULL,                             // check overflow gp errata
    NULL,                             // read_ro
    NULL,                             // platform info
    NULL,
    broadwell_de_ha_Scan_For_Uncore // scan for uncore
};
