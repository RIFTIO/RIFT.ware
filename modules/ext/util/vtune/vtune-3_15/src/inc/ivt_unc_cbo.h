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

#ifndef _IVTUNC_CBO_H_INC_
#define _IVTUNC_CBO_H_INC_

#define CBO0_BOX_CTL_MSR                     0xd04
#define CBO1_BOX_CTL_MSR                     0xd24
#define CBO2_BOX_CTL_MSR                     0xd44
#define CBO3_BOX_CTL_MSR                     0xd64
#define CBO4_BOX_CTL_MSR                     0xd84
#define CBO5_BOX_CTL_MSR                     0xdA4
#define CBO6_BOX_CTL_MSR                     0xdC4
#define CBO7_BOX_CTL_MSR                     0xdE4
#define CBO8_BOX_CTL_MSR                     0xE04
#define CBO9_BOX_CTL_MSR                     0xE24
#define CBO10_BOX_CTL_MSR                    0xE44
#define CBO11_BOX_CTL_MSR                    0xE64
#define CBO12_BOX_CTL_MSR                    0xE84
#define CBO13_BOX_CTL_MSR                    0xEA4
#define CBO14_BOX_CTL_MSR                    0xEC4

#define CBO14_PMON_CTL0_MSR                  0xED0
#define CBO14_PMON_CTL3_MSR                  0xED3
#define CBO13_PMON_CTL0_MSR                  0xEB0
#define CBO13_PMON_CTL3_MSR                  0xEB3
#define CBO12_PMON_CTL0_MSR                  0xE90
#define CBO12_PMON_CTL3_MSR                  0xE93
#define CBO11_PMON_CTL0_MSR                  0xE70
#define CBO11_PMON_CTL3_MSR                  0xE73
#define CBO10_PMON_CTL0_MSR                  0xE50
#define CBO10_PMON_CTL3_MSR                  0xE53
#define CBO9_PMON_CTL0_MSR                   0xE30
#define CBO9_PMON_CTL3_MSR                   0xE33
#define CBO8_PMON_CTL0_MSR                   0xE10
#define CBO8_PMON_CTL3_MSR                   0xE13
#define CBO7_PMON_CTL0_MSR                   0xDF0
#define CBO7_PMON_CTL3_MSR                   0xDF3
#define CBO6_PMON_CTL0_MSR                   0xDD0
#define CBO6_PMON_CTL3_MSR                   0xDD3
#define CBO5_PMON_CTL0_MSR                   0xDB0
#define CBO5_PMON_CTL3_MSR                   0xDB3
#define CBO4_PMON_CTL0_MSR                   0xD90
#define CBO4_PMON_CTL3_MSR                   0xD93
#define CBO3_PMON_CTL0_MSR                   0xD70
#define CBO3_PMON_CTL3_MSR                   0xD73
#define CBO2_PMON_CTL0_MSR                   0xD50
#define CBO2_PMON_CTL3_MSR                   0xD53
#define CBO1_PMON_CTL0_MSR                   0xD30
#define CBO1_PMON_CTL3_MSR                   0xD33
#define CBO0_PMON_CTL0_MSR                   0xD10
#define CBO0_PMON_CTL3_MSR                   0xD13

#define IS_THIS_BOX_CTL_MSR(x)              ( x == CBO0_BOX_CTL_MSR    || x == CBO1_BOX_CTL_MSR  || x == CBO2_BOX_CTL_MSR  || x == CBO3_BOX_CTL_MSR \
                                             || x == CBO4_BOX_CTL_MSR  || x == CBO5_BOX_CTL_MSR  || x == CBO6_BOX_CTL_MSR  || x == CBO7_BOX_CTL_MSR \
                                             || x == CBO8_BOX_CTL_MSR  || x == CBO9_BOX_CTL_MSR  || x == CBO10_BOX_CTL_MSR || x == CBO11_BOX_CTL_MSR \
                                             || x == CBO12_BOX_CTL_MSR || x == CBO12_BOX_CTL_MSR || x == CBO13_BOX_CTL_MSR || x == CBO14_BOX_CTL_MSR )

#define  DISABLE_CBO_COUNTERS               0x100
#define  ENABLE_CBO_COUNTERS                0x400000
#define  ENABLE_ALL_PMU                     0x20000000
#define  DISABLE_ALL_PMU                    0x80000000

#define IS_THIS_EVSEL_PMON_CTL_MSR(x)       (   (x>= CBO0_PMON_CTL0_MSR && x <= CBO0_PMON_CTL3_MSR) || (x>= CBO1_PMON_CTL0_MSR && x <= CBO1_PMON_CTL3_MSR) \
                                             || (x>= CBO2_PMON_CTL0_MSR && x <= CBO2_PMON_CTL3_MSR) || (x>= CBO3_PMON_CTL0_MSR && x <= CBO3_PMON_CTL3_MSR) \
                                             || (x>= CBO4_PMON_CTL0_MSR && x <= CBO4_PMON_CTL3_MSR) || (x>= CBO5_PMON_CTL0_MSR && x <= CBO5_PMON_CTL3_MSR) \
                                             || (x>= CBO6_PMON_CTL0_MSR && x <= CBO6_PMON_CTL3_MSR) || (x>= CBO6_PMON_CTL0_MSR && x <= CBO6_PMON_CTL3_MSR) \
                                             || (x>= CBO7_PMON_CTL0_MSR &&  x <= CBO7_PMON_CTL3_MSR) ||   (x>= CBO8_PMON_CTL0_MSR && x <= CBO8_PMON_CTL3_MSR) \
                                             || (x>= CBO9_PMON_CTL0_MSR &&  x <= CBO9_PMON_CTL3_MSR) ||   (x>= CBO10_PMON_CTL0_MSR && x <= CBO10_PMON_CTL3_MSR) \
                                             || (x>= CBO11_PMON_CTL0_MSR && x <= CBO11_PMON_CTL3_MSR) || (x>= CBO12_PMON_CTL0_MSR && x <= CBO12_PMON_CTL3_MSR) \
                                             || (x>= CBO13_PMON_CTL0_MSR && x <= CBO13_PMON_CTL3_MSR) || (x>= CBO14_PMON_CTL0_MSR && x <= CBO14_PMON_CTL3_MSR) )

extern  DISPATCH_NODE                       ivtunc_cbo_dispatch;

#endif
