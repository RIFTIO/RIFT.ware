/*
    Copyright (C) 2013-2014 Intel Corporation.  All Rights Reserved.

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

#ifndef _PERFVER4_H_
#define _PERFVER4_H_

#include "msrdefs.h"

extern DISPATCH_NODE  perfver4_dispatch;
extern DISPATCH_NODE  perfver4_dispatch_htoff_mode;

#define PERFVER4_UNC_BLBYPASS_BITMASK      0x00000001
#define PERFVER4_UNC_DISABLE_BL_BYPASS_MSR 0x39C

#if defined(DRV_IA32)
#define LBR_DATA_BITS                      32
#else
#define LBR_DATA_BITS                      48
#endif

#define LBR_BITMASK                        ((1ULL << LBR_DATA_BITS) -1)

#define PERFVER4_FROZEN_BIT_MASK           0xc00000000000000ULL
#define PERFVER4_OVERFLOW_BIT_MASK_HT_ON   0x600000070000000FULL
#define PERFVER4_OVERFLOW_BIT_MASK_HT_OFF  0x60000007000000FFULL
#endif
