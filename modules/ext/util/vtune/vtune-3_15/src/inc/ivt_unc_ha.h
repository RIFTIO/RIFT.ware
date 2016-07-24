/*COPYRIGHT**
// -------------------------------------------------------------------------
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of the accompanying license
//  agreement or nondisclosure agreement with Intel Corporation and may not
//  be copied or disclosed except in accordance with the terms< of that
//  agreement.
//        Copyright (C) 2012-2014 Intel Corporation. All Rights Reserved.
// -------------------------------------------------------------------------
**COPYRIGHT*/

#ifndef _IVYTOWN_HA_H_INC_
#define _IVYTOWN_HA_H_INC_

#define IVYTOWN_HA_DID       0x0E30 // --- HA DID -- b:14:1
#define JKT_UNC_HA_ID        0x3c46
#define IVYTOWN_HA1_DID      0x0E38 // --- HA DID -- b:28:1 -- EX

#define HA_UNIT_COUNTER_UNIT_CONTROL_OFFSET    0xf4  
#define HA_UNIT_COUNTER_0_CONTROL_OFFSET       0xd8
#define HA_UNIT_COUNTER_1_CONTROL_OFFSET       0xdc
#define HA_UNIT_COUNTER_2_CONTROL_OFFSET       0xe0
#define HA_UNIT_COUNTER_3_CONTROL_OFFSET       0xe4

#define IVYTOWNUNC_SOCKETID_UBOX_DID           0x0E1E
#define ENABLE_HA_COUNTERS                     0x400000
#define DISABLE_HA_COUNTERS                    0x100

#define IS_THIS_BOX_HA_PCI_UNIT_CTL(x)        (x == HA_UNIT_COUNTER_UNIT_CONTROL_OFFSET)
#define IS_THIS_HA_PCI_PMON_CTL(x)          ( (x == HA_UNIT_COUNTER_0_CONTROL_OFFSET) || (x == HA_UNIT_COUNTER_1_CONTROL_OFFSET) || \
                                              (x == HA_UNIT_COUNTER_2_CONTROL_OFFSET) || (x == HA_UNIT_COUNTER_3_CONTROL_OFFSET))
extern DISPATCH_NODE                          ivytown_ha_dispatch;

#endif
