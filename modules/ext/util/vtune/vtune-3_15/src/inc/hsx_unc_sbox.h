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

#ifndef _HASWELL_SERVER_SBOX_H_INC_
#define _HASWELL_SERVER_SBOX_H_INC_


#define HASWELL_SERVER_SBOX0_BOX_CTL_MSR         0x720
#define HASWELL_SERVER_SBOX1_BOX_CTL_MSR         0x72A
#define HASWELL_SERVER_SBOX2_BOX_CTL_MSR         0x734
#define HASWELL_SERVER_SBOX3_BOX_CTL_MSR         0x73E



#define HASWELL_SERVER_SBOX3_PMON_CTL0_MSR       0x73F
#define HASWELL_SERVER_SBOX3_PMON_CTL3_MSR       0x742
#define HASWELL_SERVER_SBOX2_PMON_CTL0_MSR       0x735
#define HASWELL_SERVER_SBOX2_PMON_CTL3_MSR       0x738
#define HASWELL_SERVER_SBOX1_PMON_CTL0_MSR       0x72B
#define HASWELL_SERVER_SBOX1_PMON_CTL3_MSR       0x72E
#define HASWELL_SERVER_SBOX0_PMON_CTL0_MSR       0x721
#define HASWELL_SERVER_SBOX0_PMON_CTL3_MSR       0x724

#define IS_THIS_HASWELL_SERVER_SBOX_BOX_CTL_MSR(x)            (   (x) == HASWELL_SERVER_SBOX0_BOX_CTL_MSR  \
                                               || (x) == HASWELL_SERVER_SBOX1_BOX_CTL_MSR  \
                                               || (x) == HASWELL_SERVER_SBOX2_BOX_CTL_MSR  \
                                               || (x) == HASWELL_SERVER_SBOX3_BOX_CTL_MSR  )

#define  DISABLE_HASWELL_SERVER_SBOX_COUNTERS     0x100
#define  ENABLE_HASWELL_SERVER_SBOX_COUNTERS      0x400000
#define  ENABLE_ALL_HASWELL_SERVER_SBOX_PMU       0x20000000
#define  DISABLE_ALL_HASWELL_SERVER_SBOX_PMU      0x80000000

#define IS_THIS_HASWELL_SERVER_SBOX_EVSEL_PMON_CTL_MSR(x)   (   ((x) >= HASWELL_SERVER_SBOX0_PMON_CTL0_MSR  && (x) <= HASWELL_SERVER_SBOX0_PMON_CTL3_MSR)  \
                                                             || ((x) >= HASWELL_SERVER_SBOX1_PMON_CTL0_MSR &&  (x) <= HASWELL_SERVER_SBOX1_PMON_CTL3_MSR)   \
                                                             || ((x) >= HASWELL_SERVER_SBOX2_PMON_CTL0_MSR  && (x) <= HASWELL_SERVER_SBOX2_PMON_CTL3_MSR)  \
                                                             || ((x) >= HASWELL_SERVER_SBOX3_PMON_CTL0_MSR  && (x) <= HASWELL_SERVER_SBOX3_PMON_CTL3_MSR) )

extern  DISPATCH_NODE                                  haswell_server_sbox_dispatch;
#define HASWELL_SERVER_UBOX_GLOBAL_CONTROL_MSR         0x700
#define RESET_SBOX_CTRS                                  0x2
#endif
