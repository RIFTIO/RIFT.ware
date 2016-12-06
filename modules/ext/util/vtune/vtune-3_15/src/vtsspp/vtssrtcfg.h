/*
  Copyright (C) 2010-2014 Intel Corporation.  All Rights Reserved.

  This file is part of SEP Development Kit

  SEP Development Kit is free software; you can redistribute it
  and/or modify it under the terms of the GNU General Public License
  version 2 as published by the Free Software Foundation.

  SEP Development Kit is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with SEP Development Kit; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

  As a special exception, you may use this file as part of a free software
  library without restriction.  Specifically, if other files instantiate
  templates or use macros or inline functions from this file, or you compile
  this file and link it with other files to produce an executable, this
  file does not by itself cause the resulting executable to be covered by
  the GNU General Public License.  This exception does not however
  invalidate any other reasons why the executable file might be covered by
  the GNU General Public License.
*/
#ifndef _VTSSRTCFG_H_
#define _VTSSRTCFG_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
//
// VTSS Run-Time Configuration Support
//
*/

#define VTSS_CFG_SPACE_SIZE 0x2000
#define VTSS_CFG_CHAIN_SIZE 0x0200

// Formats of request structures
#define VTSS_CFGREQ_CANCEL      0x80000000  // ORed with reqtype to indicate the cancelation of
                                            // the related profiling activity

#define VTSS_CFGREQ_VOID        0x00    // indicates the end of configuration space
#define VTSS_CFGREQ_CPUEVENT    0x01
#define VTSS_CFGREQ_CHIPEVENT   0x02
#define VTSS_CFGREQ_OSEVENT     0x03
#define VTSS_CFGREQ_TRIGGER     0x04
#define VTSS_CFGREQ_APPSTATE    0x05
#define VTSS_CFGREQ_APISTATE    0x06
#define VTSS_CFGREQ_TRACE       0x07
#define VTSS_CFGREQ_INJECT      0x08
#define VTSS_CFGREQ_ROOT        0x09
#define VTSS_CFGREQ_BTS         0x0a
#define VTSS_CFGREQ_EXECTX      0x0b
#define VTSS_CFGREQ_TBS         0x0c
#define VTSS_CFGREQ_LBR         0x0d
#define VTSS_CFGREQ_STKSTATE    0x0e
#define VTSS_CFGREQ_CPUEVENT_V1 0x0f

#define VTSS_CFGREASON_UNCOND   0x00    // start profiling immediately
#define VTSS_CFGREASON_APPSTATE 0x01    // start when the specified application state is encountered
#define VTSS_CFGREASON_APISTATE 0x02    // start when the specified API state is encountered
#define VTSS_CFGREASON_EVTSTATE 0x04    // start when the specified event state is encountered
#define VTSS_CFGREASON_TRIGGER  0x08    // start when triggered by a trigger point
#define VTSS_CFGREASON_STKSTATE 0x10    // start when the specified call stack sate is encountered
#define VTSS_CFGREASON_EXCPLUS  0x20    // start when encountered a specified state and generate an exception
#define VTSS_CFGREASON_EXCONLY  0x60    // generate an exception and continue monitoring the specified states
#define VTSS_CFGREASON_SUSPEND  0x80    // start when explicitly resumed by controlling software

#define VTSS_CFGTRACE_CTX       0x0001  // trace context switches
#define VTSS_CFGTRACE_CPUEV     0x0002  // trace processor event counts
#define VTSS_CFGTRACE_SWCFG     0x0004  // trace profiling configuration (event chain parameters)
#define VTSS_CFGTRACE_HWCFG     0x0008  // trace system configuration (pack:core:thread number/map/id; chipset id)
#define VTSS_CFGTRACE_CHIPEV    0x0010  // trace chipset event counts
#define VTSS_CFGTRACE_SAMPLE    0x0020  // trace samples
#define VTSS_CFGTRACE_TP        0x0040  // trace trigger point information
#define VTSS_CFGTRACE_OSEV      0x0080  // trace operating system event counts
#define VTSS_CFGTRACE_MODULE    0x0100  // trace module load information
#define VTSS_CFGTRACE_PROCTHR   0x0200  // trace thread and process creation/destruction information
#define VTSS_CFGTRACE_STACKS    0x0400  // trace stack samples
#define VTSS_CFGTRACE_BRANCH    0x0800  // trace all taken branches
#define VTSS_CFGTRACE_EXECTX    0x1000  // trace execution context
#define VTSS_CFGTRACE_TBS       0x2000  // trace time-based samples
#define VTSS_CFGTRACE_LASTBR    0x4000  // trace last taken branches
#define VTSS_CFGTRACE_TREE      0x8000  // trace spawned processes
#define VTSS_CFGTRACE_SYNCARG   0x10000 // trace synchronization function parameters
#define VTSS_CFGTRACE_TAGGRA    0x20000 // aggravate the behavior of threads
#define VTSS_CFGTRACE_DBGSAMP   0x40000 // generate debug exception upon event samples
#define VTSS_CFGTRACE_THRNORM   0x80000 // normalize thread-to-processor subscription
#define VTSS_CFGTRACE_LBRCSTK   0x100000 // collect LBR call stacks

#define VTSS_CFGSTATE_SYS       0x80000000  // system function ID space

#define VTSS_CFGMUX_NONE        0   // multiplexion is disabled
#define VTSS_CFGMUX_TIME        1   // time-based event multiplexion algorithm
#define VTSS_CFGMUX_SEQ         2   // event alternation algorithm
#define VTSS_CFGMUX_MST         3   // master event in event alternation algorithm
#define VTSS_CFGMUX_SLV         4   // slave event in event alternation algorithm
#define VTSS_CFGMUX_PEBS       -1   // event should be counted via PEBS mechanism

#define VTSS_CFGEVST_IGNORED    0   // the event is not used for state determination
#define VTSS_CFGEVST_EXCLUSIVE  1   // immediate triggering mode when event state is determined
#define VTSS_CFGEVST_COMBINED   2   // event state comprises trends of multiple events

#pragma pack(push, 1)

// event configuration
typedef struct
{
    int reqtype;            // serves both to pass the request to the driver and 
    // to indicate a present cfg parameter within the driver
    int event_id;
    int interval;

    int modifier;           // user-supplied event bit-mask

    int mux_grp;            // multiplexion group
    int mux_alg;            // multiplexion algorithm
    int mux_arg;            // multiplexion algorithm's argument

    int trend;              // the trend of event distribution function (degrees, +/-90)
    int trigger_mode;       // indicates how the state of this event triggers the monitoring:
                            //   Exclusive mode triggers monitoring immediately
                            //   Combined mode enables monitoring if all other events have an appropriate trend

    int extra_msr;          // an extra MSR to be programmed in conjunction with the event
    long long extra_msk;    // a mask the extra MSR is to be programmed with

    int dbg_samples;        // sample count to generate a debug exception

} cpuevent_cfg_t;

typedef struct
{
    unsigned idx;           // MSR index
    unsigned long long val; // MSR value
    unsigned long long msk; // MSR AND-mask

} msrcfg_t;

typedef struct
{
    int reqtype;
    int reqsize;            // size of record including event name and description
    int event_id;           // unique event ID for the current tracing session
    int interval;           // sampling interval (non-negative number)

    int mux_grp;            // multiplexion group
    int mux_alg;            // multiplexion algorithm
    int mux_arg;            // multiplexion algorithm's argument

    int name_off;           // offset of the event name within the buffer
    int name_len;           // event name length including '\0'
    int desc_off;           // offset of the event description within the buffer
    int desc_len;           /// event description length including '\0'

    msrcfg_t selmsr;        // event selection MSR
    msrcfg_t cntmsr;        // counter MSR
    msrcfg_t extmsr;        // extra event configuration MSR

} cpuevent_cfg_v1_t;

typedef struct
{
    int reqtype;
    int event_id;
    int interval;
    int modifier;           // user-supplied event bit-mask
    int mux_grp;            // multiplexion group
    int mux_alg;            // multiplexion algorithm
    int mux_arg;            // multiplexion algorithm's argument
    int trend;              // the trend of event distribution function (degrees, +/-90)
    int trigger_mode;       // indicates how the state of this event triggers the monitoring:
                            //   Exclusive mode triggers monitoring immediately
                            //   Combined mode enables monitoring if all other events have an appropriate trend

    int extra_msr;          // an extra MSR to be programmed in conjunction with the event
    long long extra_msk;    // a mask the extra MSR is to be programmed with

} chipevent_cfg_t;

typedef struct
{
    int reqtype;
    int event_id;

    /// all fields below are currently unused
    int interval;
    int modifier;           // user-supplied event bit-mask
    int mux_grp;            // multiplexion group
    int mux_alg;            // multiplexion algorithm
    int mux_arg;            // multiplexion algorithm's argument

} osevent_cfg_t;

// branch tracing configuration
typedef struct
{
    int reqtype;
    int brcount;            // number of branches to record
    int modifier;           // user-supplied bit-mask (shares some bits with those of event modifier)

} bts_cfg_t;

// last branch tracing configuration
typedef struct
{
    int reqtype;
    int brcount;            // number of branches to record
    int modifier;           // user-supplied bit-mask (shares some bits with those of event modifier)

} lbr_cfg_t;

// tame-based sampling configuration
typedef struct
{
    int reqtype;
    int interval;           // in system time units

} tbs_cfg_t;

// execution context tracing configuration
typedef struct
{
    int reqtype;
    int base;               // base register
    int index;              // index register
    int scale;              // multiplier
    int imm;                // offset
    int size;               // data size
    long long ips;          // starting instruction pointer of a code region to read memory contents for
    long long ipe;          // ending instruction pointer of a code region to to read memory contents for

    /// IMPORTANT NOTICE:
    ///   Set MSB in base and index register fields indictes that the remaining bits of the fields
    ///     encode an index to another exectx_cfg structure whose simulated results should be used 
    ///     instead of a register to read memory contents
    ///   All set bits in base and index fields indicate that a corresponding field is not involved
    //      in memory address calculation
} exectx_cfg_t;

// collection triggering configuration
typedef struct
{
    int reqtype;
    int reason;
    int duration;           /// profiling duration, seconds

} coltrigger_cfg_t;

// application state configuration
typedef struct
{
    int reqtype;
    int sim_thread_count;
    int range_count;
    struct
    {
        long long start;
        long long end;

    } range[1];

} appstate_cfg_t;

// API-based application state configuration
typedef struct
{
    int reqtype;
    int sim_thread_count;
    int range_count;
    struct
    {
        int function_id;
        int entry_count;

    } range[1];

} apistate_cfg_t;

// Callstack-based application state configuration
typedef struct
{
    int reqtype;
    int sim_thread_count;
    int stack_count;
    struct
    {
        int size;
        long long sample;
        struct
        {
            long long caller;

        } stack[1];

    } chain[1];

} stkstate_cfg_t;

// tracing configuration
typedef struct
{
    int reqtype;
    int trace_flags;
    int namelen;
    char name[0];

} trace_cfg_t;

// injection configuration
typedef struct
{
    int reqtype;
    void *libaddr;
    int namelen;
    char name[0];

} inject_cfg_t;

// supervisor configuration
typedef struct
{
    int reqtype;
    long long root_id;

} root_cfg_t;

// main per process configuration
typedef struct
{
    // CPU event version 1
    int cpuevent_count_v1;
    cpuevent_cfg_v1_t cpuevent_cfg_v1[VTSS_CFG_CHAIN_SIZE];
    unsigned char cpuevent_namespace_v1[VTSS_CFG_SPACE_SIZE * 16];

    // OS event configuration
    int osevent_count;
    osevent_cfg_t osevent_cfg[VTSS_CFG_CHAIN_SIZE];

    // branch tracing configuration
    bts_cfg_t bts_cfg;

    // last branch tracing configuration
    lbr_cfg_t lbr_cfg;

    // tracing configuration
    trace_cfg_t trace_cfg;
    unsigned char trace_space[VTSS_CFG_SPACE_SIZE];

} process_cfg_t;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif /* _VTSSRTCFG_H_ */
