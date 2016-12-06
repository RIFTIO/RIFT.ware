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

#ifndef _HSX_CBO_H_INC_
#define _HSX_CBO_H_INC_


#define HSX_CBO0_BOX_CTL_MSR                     0xE00
#define HSX_CBO1_BOX_CTL_MSR                     0xE10
#define HSX_CBO2_BOX_CTL_MSR                     0xE20
#define HSX_CBO3_BOX_CTL_MSR                     0xE30
#define HSX_CBO4_BOX_CTL_MSR                     0xE40
#define HSX_CBO5_BOX_CTL_MSR                     0xE50
#define HSX_CBO6_BOX_CTL_MSR                     0xE60
#define HSX_CBO7_BOX_CTL_MSR                     0xE70
#define HSX_CBO8_BOX_CTL_MSR                     0xE80
#define HSX_CBO9_BOX_CTL_MSR                     0xE90
#define HSX_CBO10_BOX_CTL_MSR                    0xEA0
#define HSX_CBO11_BOX_CTL_MSR                    0xEB0
#define HSX_CBO12_BOX_CTL_MSR                    0xEC0
#define HSX_CBO13_BOX_CTL_MSR                    0xED0
#define HSX_CBO14_BOX_CTL_MSR                    0xEE0

#define HSX_CBO14_PMON_CTL0_MSR                  0xEE1
#define HSX_CBO14_PMON_CTL3_MSR                  0xEE4
#define HSX_CBO13_PMON_CTL0_MSR                  0xED1
#define HSX_CBO13_PMON_CTL3_MSR                  0xED4
#define HSX_CBO12_PMON_CTL0_MSR                  0xEC1
#define HSX_CBO12_PMON_CTL3_MSR                  0xEC4
#define HSX_CBO11_PMON_CTL0_MSR                  0xEB1
#define HSX_CBO11_PMON_CTL3_MSR                  0xEB4
#define HSX_CBO10_PMON_CTL0_MSR                  0xEA1
#define HSX_CBO10_PMON_CTL3_MSR                  0xEA4
#define HSX_CBO9_PMON_CTL0_MSR                   0xE91
#define HSX_CBO9_PMON_CTL3_MSR                   0xE94
#define HSX_CBO8_PMON_CTL0_MSR                   0xE81
#define HSX_CBO8_PMON_CTL3_MSR                   0xE84
#define HSX_CBO7_PMON_CTL0_MSR                   0xE71
#define HSX_CBO7_PMON_CTL3_MSR                   0xE74
#define HSX_CBO6_PMON_CTL0_MSR                   0xE61
#define HSX_CBO6_PMON_CTL3_MSR                   0xE64
#define HSX_CBO5_PMON_CTL0_MSR                   0xE51
#define HSX_CBO5_PMON_CTL3_MSR                   0xE54
#define HSX_CBO4_PMON_CTL0_MSR                   0xE41
#define HSX_CBO4_PMON_CTL3_MSR                   0xE44
#define HSX_CBO3_PMON_CTL0_MSR                   0xE31
#define HSX_CBO3_PMON_CTL3_MSR                   0xE34
#define HSX_CBO2_PMON_CTL0_MSR                   0xE21
#define HSX_CBO2_PMON_CTL3_MSR                   0xE24
#define HSX_CBO1_PMON_CTL0_MSR                   0xE11
#define HSX_CBO1_PMON_CTL3_MSR                   0xE14
#define HSX_CBO0_PMON_CTL0_MSR                   0xE01
#define HSX_CBO0_PMON_CTL3_MSR                   0xE04

#define IS_THIS_HASWELL_SERVER_BOX_CTL_MSR(x)   ( x == HSX_CBO0_BOX_CTL_MSR  || x == HSX_CBO1_BOX_CTL_MSR  || x == HSX_CBO2_BOX_CTL_MSR  || x == HSX_CBO3_BOX_CTL_MSR \
                                               || x == HSX_CBO4_BOX_CTL_MSR  || x == HSX_CBO5_BOX_CTL_MSR  || x == HSX_CBO6_BOX_CTL_MSR  || x == HSX_CBO7_BOX_CTL_MSR \
                                               || x == HSX_CBO8_BOX_CTL_MSR  || x == HSX_CBO9_BOX_CTL_MSR  || x == HSX_CBO10_BOX_CTL_MSR || x == HSX_CBO11_BOX_CTL_MSR \
                                               || x == HSX_CBO12_BOX_CTL_MSR || x == HSX_CBO12_BOX_CTL_MSR || x == HSX_CBO13_BOX_CTL_MSR || x == HSX_CBO14_BOX_CTL_MSR )

#define  DISABLE_HASWELL_SERVER_CBO_COUNTERS               0x100
#define  ENABLE_HASWELL_SERVER_CBO_COUNTERS                0x400000
#define  ENABLE_ALL_HASWELL_SERVER_PMU                     0x20000000
#define  DISABLE_ALL_HASWELL_SERVER_PMU                    0x80000000

#define IS_THIS_HASWELL_SERVER_EVSEL_PMON_CTL_MSR(x)   ( (x>= HSX_CBO0_PMON_CTL0_MSR  && x <= HSX_CBO0_PMON_CTL3_MSR)  || (x>= HSX_CBO1_PMON_CTL0_MSR && x <= HSX_CBO1_PMON_CTL3_MSR) \
                                                      || (x>= HSX_CBO2_PMON_CTL0_MSR  && x <= HSX_CBO2_PMON_CTL3_MSR)  || (x>= HSX_CBO3_PMON_CTL0_MSR && x <= HSX_CBO3_PMON_CTL3_MSR) \
                                                      || (x>= HSX_CBO4_PMON_CTL0_MSR  && x <= HSX_CBO4_PMON_CTL3_MSR)  || (x>= HSX_CBO5_PMON_CTL0_MSR && x <= HSX_CBO5_PMON_CTL3_MSR) \
                                                      || (x>= HSX_CBO6_PMON_CTL0_MSR  && x <= HSX_CBO6_PMON_CTL3_MSR)  || (x>= HSX_CBO6_PMON_CTL0_MSR && x <= HSX_CBO6_PMON_CTL3_MSR) \
                                                      || (x>= HSX_CBO7_PMON_CTL0_MSR  &&  x <= HSX_CBO7_PMON_CTL3_MSR) || (x>= HSX_CBO8_PMON_CTL0_MSR && x <= HSX_CBO8_PMON_CTL3_MSR) \
                                                      || (x>= HSX_CBO9_PMON_CTL0_MSR  &&  x <= HSX_CBO9_PMON_CTL3_MSR) || (x>= HSX_CBO10_PMON_CTL0_MSR && x <= HSX_CBO10_PMON_CTL3_MSR) \
                                                      || (x>= HSX_CBO11_PMON_CTL0_MSR && x <= HSX_CBO11_PMON_CTL3_MSR) || (x>= HSX_CBO12_PMON_CTL0_MSR && x <= HSX_CBO12_PMON_CTL3_MSR) \
                                                      || (x>= HSX_CBO13_PMON_CTL0_MSR && x <= HSX_CBO13_PMON_CTL3_MSR) || (x>= HSX_CBO14_PMON_CTL0_MSR && x <= HSX_CBO14_PMON_CTL3_MSR) )

extern  DISPATCH_NODE                                  haswell_server_cbo_dispatch;
#define HASWELL_SERVER_UBOX_GLOBAL_CONTROL_MSR         0x700

#endif
