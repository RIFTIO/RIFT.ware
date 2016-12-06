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

#ifndef _HASWELL_SERVER_QPILL_H_INC_
#define _HASWELL_SERVER_QPILL_H_INC_

//Intel QPI Link 0	2F80h, 2F32h, 2F83h	8	0,2,3	Intel QPI Link 0
//Intel QPI Link 1	2F90h, 2F33h, 2F93h	9	0,2,3	Intel QPI Link 1
//Intel QPI Link 2	2F40h, 2F3Ah, 2F43h	10	0,2,3	Intel QPI Link 2
#define HASWELL_SERVER_QPILL0_DID              0x2F32 // --- QPILL0 PerfMon DID --- B:D :8:2
#define HASWELL_SERVER_QPILL1_DID              0x2F33 // --- QPILL1 PerfMon DID --- B:D :9:2
#define HASWELL_SERVER_QPILL2_DID              0x2F3A // --- QPILL2 PerfMon DID --- B:D :10:2 -- EX
#define HASWELL_SERVER_QPILL_MM0_DID           0x2F86 // --- QPILL0 PerfMon MM Config DID --- B:D :8:6
#define HASWELL_SERVER_QPILL_MM1_DID           0x2F96 // --- QPILL1 PerfMon MM Config DID --- B:D :9:6
#define HASWELL_SERVER_QPILL_MM2_DID           0x2F46 // --- QPILL1 PerfMon MM Config DID --- B:D :10:6 -- EX
//#define HASWELL_SERVER_QPILL_MCFG_DID          0x0E28 // --- QPILL1 PerfMon MCFG DID --- B:D 0:5:0

#define HASWELL_SERVER_UBOX_UNIT_GLOBAL_CONTROL_MSR    0x700
#define RESET_HASWELL_SERVER_QPI_CTRS                            0x2
#define ENABLE_HASWELL_SERVER_QPI_COUNTERS                       0x400000
#define DISABLE_HASWELL_SERVER_QPI_COUNTERS                      0x100
#define ENABLE_ALL_HASWELL_SERVER_PMU                            0x20000000
#define DISABLE_ALL_HASWELL_SERVER_PMU                           0x80000000
#define HASWELL_SERVER_QPILL_UNIT_COUNTER_UNIT_CONTROL_OFFSET    0xf4  
#define HASWELL_SERVER_QPILL_UNIT_COUNTER_0_CONTROL_OFFSET       0xd8
#define HASWELL_SERVER_QPILL_UNIT_COUNTER_1_CONTROL_OFFSET       0xdc
#define HASWELL_SERVER_QPILL_UNIT_COUNTER_2_CONTROL_OFFSET       0xe0
#define HASWELL_SERVER_QPILL_UNIT_COUNTER_3_CONTROL_OFFSET       0xe4

#define HASWELL_SERVER_QPILL_UNIT_RX_MASK0_OFFSET                 0x238
#define HASWELL_SERVER_QPILL_UNIT_RX_MASK1_OFFSET                 0x23c
#define HASWELL_SERVER_QPILL_UNIT_TX_MASK0_OFFSET                 0x210
#define HASWELL_SERVER_QPILL_UNIT_TX_MASK1_OFFSET                 0x214

#define HASWELL_SERVER_QPILL_UNIT_RX_MATCH0_OFFSET                0x228
#define HASWELL_SERVER_QPILL_UNIT_RX_MATCH1_OFFSET                0x22c
#define HASWELL_SERVER_QPILL_UNIT_TX_MATCH0_OFFSET                0x200
#define HASWELL_SERVER_QPILL_UNIT_TX_MATCH1_OFFSET                0x204

#define IS_THIS_HASWELL_SERVER_QPILL_PCI_UNIT_CTL(x)              (x == HASWELL_SERVER_QPILL_UNIT_COUNTER_UNIT_CONTROL_OFFSET)
#define IS_THIS_HASWELL_SERVER_QPILL_PCI_PMON_CTL(x)              ((x == HASWELL_SERVER_QPILL_UNIT_COUNTER_0_CONTROL_OFFSET) || (x == HASWELL_SERVER_QPILL_UNIT_COUNTER_1_CONTROL_OFFSET) || \
                                                                  (x == HASWELL_SERVER_QPILL_UNIT_COUNTER_2_CONTROL_OFFSET) || (x == HASWELL_SERVER_QPILL_UNIT_COUNTER_3_CONTROL_OFFSET))

#define HASWELL_SERVER_QPILL_UNIT_RX_MASK0_OFFSET                 0x238
#define HASWELL_SERVER_QPILL_UNIT_RX_MASK1_OFFSET                 0x23c
#define HASWELL_SERVER_QPILL_UNIT_TX_MASK0_OFFSET                 0x210
#define HASWELL_SERVER_QPILL_UNIT_TX_MASK1_OFFSET                 0x214

#define HASWELL_SERVER_QPILL_UNIT_RX_MATCH0_OFFSET                0x228
#define HASWELL_SERVER_QPILL_UNIT_RX_MATCH1_OFFSET                0x22c
#define HASWELL_SERVER_QPILL_UNIT_TX_MATCH0_OFFSET                0x200
#define HASWELL_SERVER_QPILL_UNIT_TX_MATCH1_OFFSET                0x204

#define IS_THIS_HASWELL_SERVER_QPILL_MASK_REG(x)                  ((x == HASWELL_SERVER_QPILL_UNIT_RX_MASK0_OFFSET) || (x == HASWELL_SERVER_QPILL_UNIT_RX_MASK1_OFFSET) || \
                                                                   (x == HASWELL_SERVER_QPILL_UNIT_TX_MASK0_OFFSET) || (x == HASWELL_SERVER_QPILL_UNIT_TX_MASK1_OFFSET))
#define IS_THIS_HASWELL_SERVER_QPILL_MATCH_REG(x)                 ((x == HASWELL_SERVER_QPILL_UNIT_RX_MATCH0_OFFSET) || (x == HASWELL_SERVER_QPILL_UNIT_RX_MATCH1_OFFSET) || \
                                                                   (x == HASWELL_SERVER_QPILL_UNIT_TX_MATCH0_OFFSET) || (x == HASWELL_SERVER_QPILL_UNIT_TX_MATCH1_OFFSET))
#define IS_THIS_HASWELL_SERVER_QPILL_MM_REG(x)                    (IS_THIS_HASWELL_SERVER_QPILL_MASK_REG(x) || IS_THIS_HASWELL_SERVER_QPILL_MATCH_REG(x))

extern  DISPATCH_NODE                                            haswell_server_qpill_dispatch;
#define HASWELL_SERVER_UBOX_GLOBAL_CONTROL_MSR                   0x700
#define RESET_QPI_CTRS                                           0x2
#endif
