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

#ifndef _IVYTOWN_R3QPI_H_INC_
#define _IVYTOWN_R3QPI_H_INC_

#define IVYTOWN_R3QPI0_DID      0x0E36 // --- R3QPI0 PerfMon DID --- B:D 1:19:5
#define IVYTOWN_R3QPI1_DID      0x0E37 // --- R3QPI1 PerfMon DID --- B:D 1:19:6
#define IVYTOWN_R3QPI2_DID      0x0E3E // --- R3QPI2 PerfMon DID --- B:D 1:18:5 -- EX
#define IVYTOWN_R3QPI3_DID      0x0E3F // --- R3QPI3 PerfMon DID --- B:D 1:18:6 -- EX

#define R3QPI_COUNTER_UNIT_CONTROL_OFFSET      0xf4  
#define R3QPI_COUNTER_0_CONTROL_OFFSET         0xd8
#define R3QPI_COUNTER_1_CONTROL_OFFSET         0xdc
#define R3QPI_COUNTER_2_CONTROL_OFFSET         0xe0

#define IVYTOWNUNC_SOCKETID_UBOX_DID           0x0E1E
#define ENABLE_R3QPI_COUNTERS                  0x400000
#define DISABLE_R3QPI_COUNTERS                 0x100

#define IS_THIS_R3QPI_UNIT_CTL(x)             (x == R3QPI_COUNTER_UNIT_CONTROL_OFFSET)
#define IS_THIS_R3QPI_PMON_CTL(x)             ( (x == R3QPI_COUNTER_0_CONTROL_OFFSET) || (x == R3QPI_COUNTER_1_CONTROL_OFFSET) || \
                                                (x == R3QPI_COUNTER_2_CONTROL_OFFSET))
extern DISPATCH_NODE                          ivytown_r3qpi_dispatch;

#endif
