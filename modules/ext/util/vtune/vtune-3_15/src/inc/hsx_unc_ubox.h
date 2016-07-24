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

#ifndef _HASWELL_SERVER_UBOX_H_INC_
#define _HASWELL_SERVER_UBOX_H_INC_

#define HASWELL_SERVER_SOCKETID_UBOX_DID                0x2F1E
#define BROADWELL_DE_SOCKETID_UBOX_DID                  0x6F1E

#define HASWELL_SERVER_UBOX_GLOBAL_CONTROL_MSR          0x700
#define ENABLE_ALL_HASWELL_SERVER_PMU                   0x20000000
#define DISABLE_ALL_HASWELL_SERVER_PMU                  0x80000000

#define DISABLE_HASWELL_SERVER_UBOX_COUNTER             0xFF4FFFFF    // disable (stop) counters
#define ENABLE_HASWELL_SERVER_UBOX_COUNTER              0x00400000    // enable (start) counters

#define HASWELL_SERVER_UBOX_MSR_PMON_CTL0               0x705
#define HASWELL_SERVER_UBOX_MSR_PMON_CTL1               0x706
#define HASWELL_SERVER_UBOX_MSR_FIXED_CTL               0x703

#define HASWELL_SERVER_MAX_CLR_RATIO_MASK               0x7F

#define IS_THIS_HASWELL_SERVER_UBOX_EVSEL_CTL_MSR(x)    (  x == HASWELL_SERVER_UBOX_MSR_FIXED_CTL   \
                                                        || x == HASWELL_SERVER_UBOX_MSR_PMON_CTL0   \
                                                        || x == HASWELL_SERVER_UBOX_MSR_PMON_CTL1   )

extern  DISPATCH_NODE                                   haswell_server_ubox_dispatch;

#endif
