/*COPYRIGHT**
    Copyright (C) 2005-2014 Intel Corporation.  All Rights Reserved.

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
#include "jkt_unc_ubox.h"
#include "jkt_unc_qpill.h"
#include "ivt_unc_ubox.h"
#include "ivt_unc_qpill.h"
#include "hsx_unc_ubox.h"
#include "hsx_unc_qpill.h"
#include "ivt_unc_r3qpi.h"
#include "wsx_unc_qpi.h"
#include "hsx_unc_r3qpi.h"
#include "ecb_iterators.h"
#include "pebs.h"
#include "inc/pci.h"

extern U64           *read_counter_info;
extern DRV_CONFIG     pcfg;


/************************************************************************/
/*  
 *  UNC QPILL JKT specific function
 *
 ************************************************************************/

static DRV_BOOL
unc_jkt_qpill_is_Valid (
    U32 device_id
)
{
    if ((device_id != JKTUNC_QPILL0_DID) &&
        (device_id != JKTUNC_QPILL1_DID) &&
        (device_id != JKTUNC_QPILL_MM0_DID) &&
        (device_id != JKTUNC_QPILL_MM1_DID)) {
        return FALSE;
    }
    return TRUE;
}

static DRV_BOOL
unc_jkt_qpill_is_Valid_For_Write (
    U32 device_id,
    U32 req_id
)
{
    return unc_jkt_qpill_is_Valid (device_id);
}


DEVICE_CALLBACK_NODE  unc_jkt_qpill_callback = {
    unc_jkt_qpill_is_Valid,   
    unc_jkt_qpill_is_Valid_For_Write,   
    NULL,
    NULL
};
    

static VOID
unc_qpill_jkt_Write_PMU (
    VOID  *param
)
{
    U32  control_msr = 0;
    U32  control_val = 0;
    UNC_COMMON_PCI_Write_PMU(param, 
                             JKTUNC_SOCKETID_UBOX_DID, 
                             control_msr, 
                             control_val, 
                             &unc_jkt_qpill_callback);
}


/******************************************************************************************
 *
 * UNC QPILL IVT specific functions
 *
 * ***************************************************************************************/


static DRV_BOOL
unc_qpill_ivt_is_Valid(
   U32 device_id
)
{ 
    if ((device_id != IVYTOWN_QPILL0_DID) &&
        (device_id != IVYTOWN_QPILL1_DID) &&
        (device_id != IVYTOWN_QPILL2_DID)) {
        return FALSE;
    }
    return TRUE;
}

static DRV_BOOL
unc_qpill_ivt_is_Valid_For_Write(
   U32 device_id,
   U32 reg
)
{ 
     if ((device_id != IVYTOWN_QPILL0_DID) &&
         (device_id != IVYTOWN_QPILL_MM0_DID) &&
         (device_id != IVYTOWN_QPILL1_DID) &&
         (device_id != IVYTOWN_QPILL_MM1_DID) &&
         (device_id != IVYTOWN_QPILL2_DID)) {
        return FALSE;
    }
    return TRUE;
}

static DRV_BOOL
unc_qpill_ivt_is_PMON_Ctl(
    U32 reg_id
)
{
    return (IS_THIS_QPILL_PCI_PMON_CTL(reg_id));
}

static DRV_BOOL
unc_qpill_ivt_is_Unit_Ctl(
    U32 reg_id
)
{
    return (IS_THIS_QPILL_PCI_UNIT_CTL(reg_id));
}

DEVICE_CALLBACK_NODE  unc_qpill_ivt_callback = {
    unc_qpill_ivt_is_Valid,
    unc_qpill_ivt_is_Valid_For_Write,
    unc_qpill_ivt_is_Unit_Ctl,
    unc_qpill_ivt_is_PMON_Ctl
};

/*!
 * @fn          static VOID unc_qpill_ivtWrite_PMU(VOID*)
 *
 * @brief       Initial write of PMU registers
 *              Walk through the enties and write the value of the register accordingly.
 *              When current_group = 0, then this is the first time this routine is called,
 *
 * @param       VOID *param - this should be the device_id of the box
 *
 * @return      None
 *
 * <I>Special Notes:</I>
 */
static VOID
unc_qpill_ivt_Write_PMU (
    VOID  *param
)
{
    UNC_COMMON_PCI_Write_PMU (param, 
                              IVYTOWN_SOCKETID_UBOX_DID,
                              IVYTOWN_UBOX_GLOBAL_CONTROL_MSR,
                              RESET_QPI_CTRS,
                              &unc_qpill_ivt_callback);

}

static VOID
unc_qpill_ivt_Enable_PMU (
    PVOID   param
)
{
    UNC_COMMON_PCI_Enable_PMU(param, 
                              IVYTOWN_UBOX_GLOBAL_CONTROL_MSR,
                              ENABLE_QPI_COUNTERS,
                              DISABLE_QPI_COUNTERS,
                              &unc_qpill_ivt_callback);
}

static VOID
unc_qpill_ivt_Disable_PMU (
    PVOID  param
)
{
    UNC_COMMON_PCI_Disable_PMU(param, 
                               IVYTOWN_UBOX_GLOBAL_CONTROL_MSR,
                               DISABLE_QPI_COUNTERS,
                               &unc_qpill_ivt_callback);

    return;
}


static VOID
unc_qpill_ivt_Scan_For_Uncore(
    PVOID  param

)
{
    UNC_COMMON_PCI_Scan_For_Uncore(param, 
                                   UNCORE_TOPOLOGY_INFO_NODE_QPILL,
                                   &unc_qpill_ivt_callback);
    return;
}
    


/********************************************************************************/
/*
 *  UNC qpill HSX specific functions
 *
 *******************************************************************************/

static DRV_BOOL
unc_qpill_hsx_is_Valid(
   U32 device_id
)
{ 
    if ((device_id != HASWELL_SERVER_QPILL0_DID) &&
        (device_id != HASWELL_SERVER_QPILL1_DID) &&
        (device_id != HASWELL_SERVER_QPILL2_DID)) {
        return FALSE;
    }
    return TRUE;
}

static DRV_BOOL
unc_qpill_hsx_is_Valid_For_Write(
    U32 device_id,
    U32 reg_id
)
{ 
    if ((device_id != HASWELL_SERVER_QPILL0_DID) &&
        (device_id != HASWELL_SERVER_QPILL_MM0_DID) &&
        (device_id != HASWELL_SERVER_QPILL1_DID) &&
        (device_id != HASWELL_SERVER_QPILL_MM1_DID) &&
        (device_id != HASWELL_SERVER_QPILL2_DID) &&
        (device_id != HASWELL_SERVER_QPILL_MM2_DID)) {
        return FALSE;
    }
    if ((device_id == HASWELL_SERVER_QPILL0_DID) ||
        (device_id == HASWELL_SERVER_QPILL1_DID) ||
        (device_id == HASWELL_SERVER_QPILL2_DID)) {
        if ((IS_THIS_HASWELL_SERVER_QPILL_MM_REG(reg_id))) {
            return FALSE;
        }
    } 
    else {
        if (!(IS_THIS_HASWELL_SERVER_QPILL_MM_REG(reg_id))) {
            return FALSE;
        }
    }
    return TRUE;
}

static DRV_BOOL
unc_qpill_hsx_is_PMON_Ctl(
    U32 reg_id
)
{
    return (IS_THIS_HASWELL_SERVER_QPILL_PCI_PMON_CTL(reg_id));
}

static DRV_BOOL
unc_qpill_hsx_is_Unit_Ctl(
    U32 reg_id
)
{
    return (IS_THIS_HASWELL_SERVER_QPILL_PCI_UNIT_CTL(reg_id));
}


DEVICE_CALLBACK_NODE unc_qpill_hsx_callback = {
    unc_qpill_hsx_is_Valid,
    unc_qpill_hsx_is_Valid_For_Write,
    unc_qpill_hsx_is_Unit_Ctl,
    unc_qpill_hsx_is_PMON_Ctl
};

/*!
 * @fn          static VOID unc_qpill_hsx_Write_PMU(VOID*)
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
unc_qpill_hsx_Write_PMU (
    VOID  *param
)
{
    UNC_COMMON_PCI_Write_PMU(param, 
                             HASWELL_SERVER_SOCKETID_UBOX_DID,
                             HASWELL_SERVER_UBOX_UNIT_GLOBAL_CONTROL_MSR,
                             RESET_QPI_CTRS,
                             &unc_qpill_hsx_callback);
    return;
}


/*!
 * @fn         static VOID unc_qpill_hsx_Enable_PMU(PVOID)
 *
 * @brief      Set the enable bit for all the EVSEL registers
 *
 * @param      Device Index of this PMU unit
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 */
static VOID
unc_qpill_hsx_Enable_PMU (
    PVOID   param
)
{
    UNC_COMMON_PCI_Enable_PMU(param, 
                              HASWELL_SERVER_UBOX_GLOBAL_CONTROL_MSR,
                              ENABLE_HASWELL_SERVER_QPI_COUNTERS,
                              DISABLE_HASWELL_SERVER_QPI_COUNTERS,
                              &unc_qpill_hsx_callback);
    return;
}

/*!
 * @fn         static VOID haswell_server_qpi_Disable_PMU(PVOID)
 *
 * @brief      Disable the per unit global control to stop the PMU counters.
 *
 * @param      Device Index of this PMU unit
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 */
static VOID
unc_qpill_hsx_Disable_PMU (
    PVOID  param
)
{
    UNC_COMMON_PCI_Disable_PMU(param, 
                              HASWELL_SERVER_UBOX_UNIT_GLOBAL_CONTROL_MSR,
                              DISABLE_HASWELL_SERVER_QPI_COUNTERS,
                              &unc_qpill_hsx_callback);
    return;
}


/*!
 * @fn          static VOID unc_qpill_hsx_Scan_For_Uncore(VOID*)
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
unc_qpill_hsx_Scan_For_Uncore(
    PVOID  param
)
{
    UNC_COMMON_PCI_Scan_For_Uncore(param, 
                                   UNCORE_TOPOLOGY_INFO_NODE_QPILL,
                                   &unc_qpill_hsx_callback);
    return;
}


/********************************************************************************/
/*
 *  UNC qpi wsmex specific functions
 *
 *******************************************************************************/

/*!
 * @fn          static VOID wsmexunc_qpi_Write_PMU(VOID*)
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
wsmexunc_qpi_Write_PMU (
    VOID  *param
)
{
    UNC_COMMON_MSR_Write_PMU(param, QPI_PERF_GLOBAL_CTRL, (unsigned long long)QPI_GLOBAL_DISABLE, 0, NULL);
    return;
}

/*!
 * @fn         static VOID wsmexunc_qpi_Disable_PMU(PVOID)
 *
 * @brief      Zero out the global control register.  This automatically disables the
 *             PMU counters.
 *
 * @param      None
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 */
static VOID
wsmexunc_qpi_Disable_PMU (
    PVOID  param
)
{
    SYS_Write_MSR(QPI_PERF_GLOBAL_CTRL, (unsigned long long)QPI_GLOBAL_DISABLE);
    return;
}

/*!
 * @fn         static VOID wsmexunc_qpi_Enable_PMU(PVOID)
 *
 * @brief      Set the enable bit for all the CCCR registers
 *
 * @param      None
 *
 * @return     None
 *
 * <I>Special Notes:</I>
 */
static VOID
wsmexunc_qpi_Enable_PMU (
    PVOID   param
)
{
    /*
     * Get the value from the event block
     *   0 == location of the global control reg for this block.
     *   Generalize this location awareness when possible
     */
    if (GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_RUNNING) {
        SYS_Write_MSR(QPI_PERF_GLOBAL_CTRL, (unsigned long long)QPI_GLOBAL_ENABLE);
    }
    return;
}

/*!
 * @fn         static VOID wsmexunc_qpi_Read_PMU_Data(PVOID)
 *
 * @brief      Read all the data MSR's into a buffer.
 *             Called by the interrupt handler
 *
 * @param      buffer      - pointer to the output buffer
 *             start       - position of the first entry
 *             stop        - last possible entry to this buffer
 *
 * @return     None
 */
static VOID
wsmexunc_qpi_Read_PMU_Data (
    PVOID   param
)
{
    S32       start_index, j;
    U64      *buffer    = read_counter_info;
    U32       this_cpu  = CONTROL_THIS_CPU();
 
    start_index = DRV_CONFIG_num_events(pcfg) * this_cpu;
    SEP_PRINT_DEBUG("PMU control_data 0x%p, buffer 0x%p, j = %d\n", PMU_register_data, buffer, j);
    FOR_EACH_DATA_REG(pecb_unc,i) {
        j = start_index + ECB_entries_event_id_index(pecb_unc,i);
        buffer[j] = SYS_Read_MSR(ECB_entries_reg_id(pecb_unc,i));
        SEP_PRINT_DEBUG("this_cpu %d, event_id %d, value 0x%llx\n", this_cpu, i, buffer[j]);
    } END_FOR_EACH_DATA_REG;
 
    return;
}

 /************************************************************
 ************************************************************/
/*
 * Initialize the dispatch table
 */
DISPATCH_NODE  jktunc_qpill_dispatch =
{
    NULL,                        // initialize
    NULL,                        // destroy
    unc_qpill_jkt_Write_PMU,     // write
    NULL,                        // freeze
    NULL,                        // restart
    UNC_COMMON_PCI_Read_PMU_Data,  // read
    NULL,                        // check for overflow
    NULL,                        // swap group
    NULL,                        // read lbrs
    UNC_COMMON_Dummy_Func,       // clean up
    NULL,                        // hw errata
    NULL,                        // read power
    NULL,                        // check overflow errata
    UNC_COMMON_PCI_Read_Counts,  // read_counts
    NULL,                        // check overflow gp errata
    NULL,                        // read_ro
    NULL,                        // platform_info
    NULL
};


/*
 * Initialize the dispatch table
 */
DISPATCH_NODE  haswell_server_qpill_dispatch =
{
    NULL,                            // initialize
    NULL,                            // destroy
    unc_qpill_hsx_Write_PMU,         // write
    unc_qpill_hsx_Disable_PMU,       // freeze
    unc_qpill_hsx_Enable_PMU,        // restart
    UNC_COMMON_PCI_Read_PMU_Data,    // read_counts
    NULL,                            // check for overflow
    NULL,                            // swap group
    NULL,                            // read lbrs
    NULL,                            // cleanup
    NULL,                            // hw errata
    NULL,                            // read power
    NULL,                            // check overflow errata
    UNC_COMMON_PCI_Read_Counts,      // read_counts
    NULL,                            // check overflow gp errata
    NULL,                            // read_ro
    NULL,                            // platform info
    NULL,
    unc_qpill_hsx_Scan_For_Uncore    // scan for uncore
};
/*
 * Initialize the dispatch table
 */
DISPATCH_NODE  ivytown_qpill_dispatch =
{
    NULL,                            // initialize
    NULL,                            // destroy
    unc_qpill_ivt_Write_PMU,         // write
    unc_qpill_ivt_Disable_PMU,       // freeze
    unc_qpill_ivt_Enable_PMU,        // restart
    UNC_COMMON_PCI_Read_PMU_Data,    // read_counts
    NULL,                            // check for overflow
    NULL,                            // swap group
    NULL,                            // read lbrs
    NULL,                            // cleanup
    NULL,                            // hw errata
    NULL,                            // read power
    NULL,                            // check overflow errata
    UNC_COMMON_PCI_Read_Counts,      // read_counts
    NULL,                            // check overflow gp errata
    NULL,                            // read_ro
    NULL,                            // platform info
    NULL,
    unc_qpill_ivt_Scan_For_Uncore     // scan for uncore
};

/*
 * Initialize the dispatch table
 */
DISPATCH_NODE  wsmexunc_qpi_dispatch =
{
    NULL,                              // initialize
    NULL,                              // destroy
    wsmexunc_qpi_Write_PMU,            // write
    wsmexunc_qpi_Disable_PMU,          // freeze
    wsmexunc_qpi_Enable_PMU,           // restart
    wsmexunc_qpi_Read_PMU_Data,        // read
    NULL,                              // check for overflow
    NULL,
    NULL,
    UNC_COMMON_MSR_Clean_Up,
    NULL,
    NULL,
    NULL,
    UNC_COMMON_MSR_Read_Counts,
    NULL,                          // check_overflow_gp_errata
    NULL,                          // read_ro
    NULL,                          // platform_info
    NULL,
    NULL                           // scan for uncore
};
