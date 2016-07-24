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

#ifndef _IVYTOWN_QPILL_H_INC_
#define _IVYTOWN_QPILL_H_INC_

#define IVYTOWN_QPILL0_DID              0x0E32 // --- QPILL0 PerfMon DID --- B:D 1:8:2
#define IVYTOWN_QPILL1_DID              0x0E33 // --- QPILL1 PerfMon DID --- B:D 1:9:2
#define IVYTOWN_QPILL2_DID              0x0E3A // --- QPILL2 PerfMon DID --- B:D 1:24:2 -- EX
#define IVYTOWN_QPILL_MM0_DID           0x0E86 // --- QPILL0 PerfMon MM Config DID --- B:D 1:8:6
#define IVYTOWN_QPILL_MM1_DID           0x0E96 // --- QPILL1 PerfMon MM Config DID --- B:D 1:9:6
#define IVYTOWN_QPILL_MCFG_DID          0x0E28 // --- QPILL1 PerfMon MCFG DID --- B:D 0:5:0

#define ENABLE_QPI_COUNTERS                       0x400000
#define DISABLE_QPI_COUNTERS                      0x100
#define ENABLE_ALL_PMU                            0x20000000
#define DISABLE_ALL_PMU                           0x80000000
#define QPILL_UNIT_COUNTER_UNIT_CONTROL_OFFSET    0xf4  
#define QPILL_UNIT_COUNTER_0_CONTROL_OFFSET       0xd8
#define QPILL_UNIT_COUNTER_1_CONTROL_OFFSET       0xdc
#define QPILL_UNIT_COUNTER_2_CONTROL_OFFSET       0xe0
#define QPILL_UNIT_COUNTER_3_CONTROL_OFFSET       0xe4
#define IS_THIS_QPILL_PCI_UNIT_CTL(x)             (x == QPILL_UNIT_COUNTER_UNIT_CONTROL_OFFSET)
#define IS_THIS_QPILL_PCI_PMON_CTL(x)            ((x == QPILL_UNIT_COUNTER_0_CONTROL_OFFSET) || (x == QPILL_UNIT_COUNTER_1_CONTROL_OFFSET) || \
                                                  (x == QPILL_UNIT_COUNTER_2_CONTROL_OFFSET) || (x == QPILL_UNIT_COUNTER_3_CONTROL_OFFSET))

extern  DISPATCH_NODE                             ivytown_qpill_dispatch;

#endif
