/*
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
*/

#ifndef _IVTUNC_IMC_H_INC_
#define _IVTUNC_IMC_H_INC_

#define IVTUNC_IMC0_DID             0x0EB4 // --- IMC0 DID -- 1:16:4
#define IVTUNC_IMC1_DID             0x0EB5 // --- IMC1 DID -- 1:16:5
#define IVTUNC_IMC2_DID             0x0EB0 // --- IMC2 DID -- 1:16:0
#define IVTUNC_IMC3_DID             0x0EB1 // --- IMC3 DID -- 1:16:1
#define IVTUNC_IMC4_DID             0x0EF4 // --- IMC0 DID -- 1:30:4 -- EX
#define IVTUNC_IMC5_DID             0x0EF5 // --- IMC1 DID -- 1:30:5 -- EX
#define IVTUNC_IMC6_DID             0x0EF0 // --- IMC2 DID -- 1:30:0 -- EX
#define IVTUNC_IMC7_DID             0x0EF1 // --- IMC3 DID -- 1:30:1 -- EX

#define IMC_UNIT_COUNTER_UNIT_CONTROL_OFFSET    0xf4  
#define IMC_UNIT_COUNTER_0_CONTROL_OFFSET       0xd8
#define IMC_UNIT_COUNTER_1_CONTROL_OFFSET       0xdc
#define IMC_UNIT_COUNTER_2_CONTROL_OFFSET       0xe0
#define IMC_UNIT_COUNTER_3_CONTROL_OFFSET       0xe4
#define IMC_UNIT_FIXED_COUNTER_CONTROL_OFFSET   0xf0

#define IVTUNC_SOCKETID_UBOX_DID                0x0E1E
#define ENABLE_IMC_COUNTERS                     0x400000
#define DISABLE_IMC_COUNTERS                    0x100

#define IS_THIS_BOX_MC_PCI_UNIT_CTL(x)     (x == IMC_UNIT_COUNTER_UNIT_CONTROL_OFFSET)
#define IS_THIS_MC_PCI_PMON_CTL(x)         (   (x == IMC_UNIT_COUNTER_0_CONTROL_OFFSET) || (x == IMC_UNIT_COUNTER_1_CONTROL_OFFSET) \
                                            || (x == IMC_UNIT_COUNTER_2_CONTROL_OFFSET) || (x == IMC_UNIT_COUNTER_3_CONTROL_OFFSET) \
                                            || (x == IMC_UNIT_FIXED_COUNTER_CONTROL_OFFSET))
extern  DISPATCH_NODE  ivtunc_imc_dispatch;

#endif
