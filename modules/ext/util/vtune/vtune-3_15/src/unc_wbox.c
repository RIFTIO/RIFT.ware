/*COPYRIGHT**
    Copyright (C) 2010-2014 Intel Corporation.  All Rights Reserved.

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
#include "wsx_unc_wbox.h"
#include "ecb_iterators.h"
#include "pebs.h"
#include "unc_common.h"

extern EVENT_CONFIG   global_ec;
extern U64           *read_counter_info;
extern LBR            lbr;
extern DRV_CONFIG     pcfg;
extern PWR            pwr;

/*!
 * @fn          static VOID unc_wbox_wsmex_Write_PMU(VOID*)
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
unc_wbox_wsmex_Write_PMU (
    VOID  *param
)
{
    UNC_COMMON_MSR_Write_PMU(param, WBOX_PERF_GLOBAL_CTRL, WBOX_GLOBAL_DISABLE, 0, NULL);
    return;
}

/*!
 * @fn         static VOID unc_wbox_wsmex_Disable_PMU(PVOID)
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
unc_wbox_wsmex_Disable_PMU (
    PVOID  param
)
{
    SYS_Write_MSR(WBOX_PERF_GLOBAL_CTRL, (long long)WBOX_GLOBAL_DISABLE);

    return;
}

/*!
 * @fn         static VOID unc_wbox_wsmex_Enable_PMU(PVOID)
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
unc_wbox_wsmex_Enable_PMU (
    PVOID   param
)
{
    /*
     * Get the value from the event block
     *   0 == location of the global control reg for this block.
     *   Generalize this location awareness when possible
     */
    if (GLOBAL_STATE_current_phase(driver_state) == DRV_STATE_RUNNING) {
        SYS_Write_MSR(WBOX_PERF_GLOBAL_CTRL, (long long)WBOX_GLOBAL_ENABLE);
    }

    return;
}


/*!
 * @fn         static VOID unc_wbox_wsmex_Read_PMU_Data(PVOID)
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
unc_wbox_wsmex_Read_PMU_Data(
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



/*
 * Initialize the dispatch table
 */
DISPATCH_NODE  wsmexunc_wbox_dispatch =
{
    NULL,                         // initialize
    NULL,                         // destroy
    unc_wbox_wsmex_Write_PMU,     // write
    unc_wbox_wsmex_Disable_PMU,   // freeze
    unc_wbox_wsmex_Enable_PMU,    // restart
    unc_wbox_wsmex_Read_PMU_Data, // read
    NULL,                         // check for overflow
    NULL,
    NULL,
    UNC_COMMON_MSR_Clean_Up,
    NULL,
    NULL,
    NULL,
    UNC_COMMON_MSR_Read_Counts,
    NULL,                         // check_overflow_gp_errata
    NULL,                         // read_ro
    NULL,                         // platform_info
    NULL,
    NULL                          // scan for uncore
};
