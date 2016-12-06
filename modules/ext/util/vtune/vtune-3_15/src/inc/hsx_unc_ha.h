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

#ifndef _HASWELL_SERVER_HA_H_INC_
#define _HASWELL_SERVER_HA_H_INC_
//Home Agent (HA) 0	2FA0h, 2F30h	18	0-1	Processor Home Agent 0
#define HASWELL_SERVER_HA_DID       0x2F30 // --- HA DID -- b:18:1
#define JKT_UNC_HA_ID               0x3c46
//Home Agent (HA) 1	2F60h, 2F38h	18	4-5	Processor Home Agent 1
#define HASWELL_SERVER_HA1_DID      0x2F38 // --- HA DID -- b:18:5 -- EX

#define BROADWELL_DE_HA_DID         0x6FA0
#define BROADWELL_DE_HA1_DID        0x6F38

#define HASWELL_SERVER_HA_UNIT_COUNTER_UNIT_CONTROL_OFFSET    0xf4  
#define HASWELL_SERVER_HA_UNIT_COUNTER_0_CONTROL_OFFSET       0xd8
#define HASWELL_SERVER_HA_UNIT_COUNTER_1_CONTROL_OFFSET       0xdc
#define HASWELL_SERVER_HA_UNIT_COUNTER_2_CONTROL_OFFSET       0xe0
#define HASWELL_SERVER_HA_UNIT_COUNTER_3_CONTROL_OFFSET       0xe4

#define ENABLE_HASWELL_SERVER_HA_COUNTERS      0x400000
#define DISABLE_HASWELL_SERVER_HA_COUNTERS     0x100
#define RESET_CTRS_HASWELL_SERVER_HA           0x2

#define IS_THIS_HASWELL_SERVER_BOX_HA_PCI_UNIT_CTL(x)        ((x) == HASWELL_SERVER_HA_UNIT_COUNTER_UNIT_CONTROL_OFFSET)
#define IS_THIS_HASWELL_SERVER_HA_PCI_PMON_CTL(x)          ( ((x) == HASWELL_SERVER_HA_UNIT_COUNTER_0_CONTROL_OFFSET) || ((x) == HASWELL_SERVER_HA_UNIT_COUNTER_1_CONTROL_OFFSET) || \
                                                             ((x) == HASWELL_SERVER_HA_UNIT_COUNTER_2_CONTROL_OFFSET) || ((x) == HASWELL_SERVER_HA_UNIT_COUNTER_3_CONTROL_OFFSET))
extern DISPATCH_NODE                                         haswell_server_ha_dispatch;
extern DISPATCH_NODE                                         broadwell_de_ha_dispatch;
#define HASWELL_SERVER_UBOX_GLOBAL_CONTROL_MSR               0x700

#endif
