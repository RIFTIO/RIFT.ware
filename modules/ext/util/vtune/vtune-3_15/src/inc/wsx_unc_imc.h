/*
    Copyright (C) 2009-2014 Intel Corporation.  All Rights Reserved.
 
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

#ifndef _WSMEXUNC_IMC_H_
#define _WSMEXUNC_IMC_H_

/*
 * Local to this architecture: SNB uncore NCU unit 
 * Arch Perf monitoring version 3
 */
// NCU/CBO MSRs
#define IMC_PERF_GLOBAL_CTRL        0xC00
#define IMC_PERF_GLOBAL_STATUS      0x392

#define IMC_GLOBAL_ENABLE           0x10000001
#define IMC_GLOBAL_DISABLE          0x30000001

// To get the correct MSR, need to 
// take a iMC base + corresponding offset
// e.g.: IMC0_PERF_CTR0 = IMC0_BASE_MSR + IMC_PERF_CTR0

// base addr per iMC
#define IMC0_BASE_MSR               0xca0
#define IMC1_BASE_MSR               0xce0

// offsets
#define IMC_PERF_UNIT_CTRL          0x0
#define IMC_PERF_UNIT_STATUS        0x1
#define IMC_PERF_UNIT_OVF_CTRL      0x2
#define IMC_PERF_EVTSEL0            0x10
#define IMC_PERF_EVTSEL1            0x12
#define IMC_PERF_EVTSEL2            0x13
#define IMC_PERF_EVTSEL3            0x16
#define IMC_PERF_EVTSEL4            0x18
#define IMC_PERF_EVTSEL5            0x1A
#define IMC_PERF_CTR0               0x11
#define IMC_PERF_CTR1               0x13
#define IMC_PERF_CTR2               0x15
#define IMC_PERF_CTR3               0x17
#define IMC_PERF_CTR4               0x19
#define IMC_PERF_CTR5               0x1B
#define IMC_TIMESTAMP_UNIT          0x04
#define IMC_DSP                     0x05
#define IMC_ISS                     0x06
#define IMC_MAP                     0x07
#define IMC_MSC_THR                 0x08
#define IMC_PGT                     0x09
#define IMC_PLD                     0x0A
#define IMC_ZDP_CTL_FVC             0x0B

#define IA32_DEBUG_CTRL             0x1D9

extern DISPATCH_NODE  wsmexunc_imc_dispatch;

#endif 
