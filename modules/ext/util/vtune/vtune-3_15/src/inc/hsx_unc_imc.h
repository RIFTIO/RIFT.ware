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

#ifndef _HASWELL_SERVER_IMC_H_INC_
#define _HASWELL_SERVER_IMC_H_INC_

//Integrated Memory Controller 0	2FB0h, 2FB1h, 2FB4h, 2FB5h	20, 21	0, 1	Channel 0 -3 Thermal Control

// Channel 0
#define HASWELL_SERVER_IMC0_DID             0x2FB0 // --- IMC0 DID -- 1:20:0
#define HASWELL_SERVER_IMC1_DID             0x2FB1 // --- IMC1 DID -- 1:20:1
#define HASWELL_SERVER_IMC2_DID             0x2FB4 // --- IMC2 DID -- 1:21:0
#define HASWELL_SERVER_IMC3_DID             0x2FB5 // --- IMC3 DID -- 1:21:1
// Channel 1
#define HASWELL_SERVER_IMC4_DID             0x2FD0 // --- IMC0 DID -- 1:23:0
#define HASWELL_SERVER_IMC5_DID             0x2FD1 // --- IMC1 DID -- 1:23:1
#define HASWELL_SERVER_IMC6_DID             0x2FD4 // --- IMC2 DID -- 1:24:0
#define HASWELL_SERVER_IMC7_DID             0x2FD5 // --- IMC3 DID -- 1:24:1

#define BROADWELL_DE_IMC0_DID               0x6FD0
#define BROADWELL_DE_IMC1_DID               0x6FD1 
#define BROADWELL_DE_IMC2_DID               0x6FD4 
#define BROADWELL_DE_IMC3_DID               0x6FD5

#define HASWELL_SERVER_IMC_UNIT_COUNTER_UNIT_CONTROL_OFFSET    0xf4  
#define HASWELL_SERVER_IMC_UNIT_COUNTER_0_CONTROL_OFFSET       0xd8
#define HASWELL_SERVER_IMC_UNIT_COUNTER_1_CONTROL_OFFSET       0xdc
#define HASWELL_SERVER_IMC_UNIT_COUNTER_2_CONTROL_OFFSET       0xe0
#define HASWELL_SERVER_IMC_UNIT_COUNTER_3_CONTROL_OFFSET       0xe4
#define HASWELL_SERVER_IMC_UNIT_FIXED_COUNTER_CONTROL_OFFSET   0xf0

#define ENABLE_HASWELL_SERVER_IMC_COUNTERS       0x400000
#define DISABLE_HASWELL_SERVER_IMC_COUNTERS      0x100

#define IS_THIS_HASWELL_SERVER_BOX_MC_PCI_UNIT_CTL(x)     (x == HASWELL_SERVER_IMC_UNIT_COUNTER_UNIT_CONTROL_OFFSET) 
#define IS_THIS_HASWELL_SERVER_MC_PCI_PMON_CTL(x)        (   (x == HASWELL_SERVER_IMC_UNIT_COUNTER_0_CONTROL_OFFSET)  \
                                                          || (x == HASWELL_SERVER_IMC_UNIT_COUNTER_1_CONTROL_OFFSET) \
                                                          || (x == HASWELL_SERVER_IMC_UNIT_COUNTER_2_CONTROL_OFFSET) \
                                                          || (x == HASWELL_SERVER_IMC_UNIT_COUNTER_3_CONTROL_OFFSET) \
                                                          || (x == HASWELL_SERVER_IMC_UNIT_FIXED_COUNTER_CONTROL_OFFSET))
extern DISPATCH_NODE  haswell_server_imc_dispatch;
extern DISPATCH_NODE  broadwell_de_imc_dispatch;
#define HASWELL_SERVER_UBOX_GLOBAL_CONTROL_MSR            0x700
#define RESET_IMC_CTRS                                    0x2
#endif
