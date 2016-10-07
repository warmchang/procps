/*
 * libprocps - Library to read proc filesystem
 *
 * Copyright (C) 1998-2005 Albert Cahalan
 * Copyright (C) 2015 Craig Small <csmall@enc.com.au>
 * Copyright (C) 2015 Jim Warner <james.warner@comcast.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef PROCPS_PIDS_H
#define PROCPS_PIDS_H

#include <sys/cdefs.h>

__BEGIN_DECLS

enum pids_item {
    PIDS_noop,              //        ( never altered )
    PIDS_extra,             //        ( reset to zero )
    PIDS_ADDR_END_CODE,     //  ul_int
    PIDS_ADDR_KSTK_EIP,     //  ul_int
    PIDS_ADDR_KSTK_ESP,     //  ul_int
    PIDS_ADDR_START_CODE,   //  ul_int
    PIDS_ADDR_START_STACK,  //  ul_int
    PIDS_CGNAME,            //     str
    PIDS_CGROUP,            //     str
    PIDS_CGROUP_V,          //    strv
    PIDS_CMD,               //     str
    PIDS_CMDLINE,           //     str
    PIDS_CMDLINE_V,         //    strv
    PIDS_ENVIRON,           //     str
    PIDS_ENVIRON_V,         //    strv
    PIDS_EXIT_SIGNAL,       //   s_int
    PIDS_FLAGS,             //  ul_int
    PIDS_FLT_MAJ,           //  ul_int
    PIDS_FLT_MAJ_C,         //  ul_int
    PIDS_FLT_MAJ_DELTA,     //   s_int
    PIDS_FLT_MIN,           //  ul_int
    PIDS_FLT_MIN_C,         //  ul_int
    PIDS_FLT_MIN_DELTA,     //   s_int
    PIDS_ID_EGID,           //   u_int
    PIDS_ID_EGROUP,         //     str
    PIDS_ID_EUID,           //   u_int
    PIDS_ID_EUSER,          //     str
    PIDS_ID_FGID,           //   u_int
    PIDS_ID_FGROUP,         //     str
    PIDS_ID_FUID,           //   u_int
    PIDS_ID_FUSER,          //     str
    PIDS_ID_PGRP,           //   s_int
    PIDS_ID_PID,            //   s_int
    PIDS_ID_PPID,           //   s_int
    PIDS_ID_RGID,           //   u_int
    PIDS_ID_RGROUP,         //     str
    PIDS_ID_RUID,           //   u_int
    PIDS_ID_RUSER,          //     str
    PIDS_ID_SESSION,        //   s_int
    PIDS_ID_SGID,           //   u_int
    PIDS_ID_SGROUP,         //     str
    PIDS_ID_SUID,           //   u_int
    PIDS_ID_SUSER,          //     str
    PIDS_ID_TGID,           //   s_int
    PIDS_ID_TPGID,          //   s_int
    PIDS_LXCNAME,           //     str
    PIDS_MEM_CODE,          //  ul_int
    PIDS_MEM_CODE_PGS,      //  ul_int
    PIDS_MEM_DATA,          //  ul_int
    PIDS_MEM_DATA_PGS,      //  ul_int
    PIDS_MEM_RES,           //  ul_int
    PIDS_MEM_RES_PGS,       //  ul_int
    PIDS_MEM_SHR,           //  ul_int
    PIDS_MEM_SHR_PGS,       //  ul_int
    PIDS_MEM_VIRT,          //  ul_int
    PIDS_MEM_VIRT_PGS,      //  ul_int
    PIDS_NICE,              //   s_int
    PIDS_NLWP,              //   s_int
    PIDS_NS_IPC,            //  ul_int
    PIDS_NS_MNT,            //  ul_int
    PIDS_NS_NET,            //  ul_int
    PIDS_NS_PID,            //  ul_int
    PIDS_NS_USER,           //  ul_int
    PIDS_NS_UTS,            //  ul_int
    PIDS_OOM_ADJ,           //   s_int
    PIDS_OOM_SCORE,         //   s_int
    PIDS_PRIORITY,          //   s_int
    PIDS_PROCESSOR,         //   u_int
    PIDS_RSS,               //  ul_int
    PIDS_RSS_RLIM,          //  ul_int
    PIDS_RTPRIO,            //   s_int
    PIDS_SCHED_CLASS,       //   s_int
    PIDS_SD_MACH,           //     str
    PIDS_SD_OUID,           //     str
    PIDS_SD_SEAT,           //     str
    PIDS_SD_SESS,           //     str
    PIDS_SD_SLICE,          //     str
    PIDS_SD_UNIT,           //     str
    PIDS_SD_UUNIT,          //     str
    PIDS_SIGBLOCKED,        //     str
    PIDS_SIGCATCH,          //     str
    PIDS_SIGIGNORE,         //     str
    PIDS_SIGNALS,           //     str
    PIDS_SIGPENDING,        //     str
    PIDS_STATE,             //    s_ch
    PIDS_SUPGIDS,           //     str
    PIDS_SUPGROUPS,         //     str
    PIDS_TICS_ALL,          // ull_int
    PIDS_TICS_ALL_C,        // ull_int
    PIDS_TICS_ALL_DELTA,    //   s_int
    PIDS_TICS_BLKIO,        // ull_int
    PIDS_TICS_GUEST,        // ull_int
    PIDS_TICS_GUEST_C,      // ull_int
    PIDS_TICS_SYSTEM,       // ull_int
    PIDS_TICS_SYSTEM_C,     // ull_int
    PIDS_TICS_USER,         // ull_int
    PIDS_TICS_USER_C,       // ull_int
    PIDS_TIME_ALL,          // ull_int
    PIDS_TIME_ELAPSED,      // ull_int
    PIDS_TIME_START,        // ull_int
    PIDS_TTY,               //   s_int
    PIDS_TTY_NAME,          //     str
    PIDS_TTY_NUMBER,        //     str
    PIDS_VM_DATA,           //  ul_int
    PIDS_VM_EXE,            //  ul_int
    PIDS_VM_LIB,            //  ul_int
    PIDS_VM_RSS,            //  ul_int
    PIDS_VM_RSS_ANON,       //  ul_int
    PIDS_VM_RSS_FILE,       //  ul_int
    PIDS_VM_RSS_LOCKED,     //  ul_int
    PIDS_VM_RSS_SHARED,     //  ul_int
    PIDS_VM_SIZE,           //  ul_int
    PIDS_VM_STACK,          //  ul_int
    PIDS_VM_SWAP,           //  ul_int
    PIDS_VM_USED,           //  ul_int
    PIDS_VSIZE_PGS,         //  ul_int
    PIDS_WCHAN_NAME         //     str
};

enum pids_fetch_type {
    PIDS_FETCH_TASKS_ONLY,
    PIDS_FETCH_THREADS_TOO
};

enum pids_select_type {
    PIDS_SELECT_PID  = 0x1000,
    PIDS_SELECT_UID  = 0x4000
};

enum pids_sort_order {
    PIDS_SORT_ASCEND   = +1,
    PIDS_SORT_DESCEND  = -1
};


struct pids_result {
    enum pids_item item;
    union {
        signed char         s_ch;
        signed int          s_int;
        unsigned int        u_int;
        unsigned long       ul_int;
        unsigned long long  ull_int;
        char               *str;
        char              **strv;
    } result;
};

struct pids_stack {
    struct pids_result *head;
};

struct pids_counts {
    int total;
    int running, sleeping, stopped, zombied;
};

struct pids_fetch {
    struct pids_counts *counts;
    struct pids_stack **stacks;
};


#define PIDS_VAL( relative_enum, type, stack, info ) \
    stack -> head [ relative_enum ] . result . type


struct pids_info;

int procps_pids_new   (struct pids_info **info, enum pids_item *items, int numitems);
int procps_pids_ref   (struct pids_info  *info);
int procps_pids_unref (struct pids_info **info);

struct pids_stack *fatal_proc_unmounted (
    struct pids_info *info,
    int return_self);

struct pids_stack *procps_pids_get (
    struct pids_info *info,
    enum pids_fetch_type which);

struct pids_fetch *procps_pids_reap (
    struct pids_info *info,
    enum pids_fetch_type which);

int procps_pids_reset (
    struct pids_info *info,
    enum pids_item *newitems,
    int newnumitems);

struct pids_fetch *procps_pids_select (
    struct pids_info *info,
    unsigned *these,
    int numthese,
    enum pids_select_type which);

struct pids_stack **procps_pids_sort (
    struct pids_info *info,
    struct pids_stack *stacks[],
    int numstacked,
    enum pids_item sortitem,
    enum pids_sort_order order);

__END_DECLS
#endif
