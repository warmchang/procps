/*
 * pids.h - task/thread/process related declarations for libproc
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

#ifndef _PROC_PIDS_H
#define _PROC_PIDS_H

#include <features.h>
__BEGIN_DECLS

enum pids_item {
    PROCPS_PIDS_noop,              //        ( never altered )
    PROCPS_PIDS_extra,             //        ( reset to zero )
    PROCPS_PIDS_ADDR_END_CODE,     //  ul_int
    PROCPS_PIDS_ADDR_KSTK_EIP,     //  ul_int
    PROCPS_PIDS_ADDR_KSTK_ESP,     //  ul_int
    PROCPS_PIDS_ADDR_START_CODE,   //  ul_int
    PROCPS_PIDS_ADDR_START_STACK,  //  ul_int
    PROCPS_PIDS_ALARM,             //  sl_int
    PROCPS_PIDS_CGNAME,            //     str
    PROCPS_PIDS_CGROUP,            //     str
    PROCPS_PIDS_CGROUP_V,          //    strv
    PROCPS_PIDS_CMD,               //     str
    PROCPS_PIDS_CMDLINE,           //     str
    PROCPS_PIDS_CMDLINE_V,         //    strv
    PROCPS_PIDS_ENVIRON,           //     str
    PROCPS_PIDS_ENVIRON_V,         //    strv
    PROCPS_PIDS_EXIT_SIGNAL,       //   s_int
    PROCPS_PIDS_FLAGS,             //  ul_int
    PROCPS_PIDS_FLT_MAJ,           //  sl_int
    PROCPS_PIDS_FLT_MAJ_C,         //  sl_int
    PROCPS_PIDS_FLT_MAJ_DELTA,     //  sl_int
    PROCPS_PIDS_FLT_MIN,           //  sl_int
    PROCPS_PIDS_FLT_MIN_C,         //  sl_int
    PROCPS_PIDS_FLT_MIN_DELTA,     //  sl_int
    PROCPS_PIDS_ID_EGID,           //   u_int
    PROCPS_PIDS_ID_EGROUP,         //     str
    PROCPS_PIDS_ID_EUID,           //   u_int
    PROCPS_PIDS_ID_EUSER,          //     str
    PROCPS_PIDS_ID_FGID,           //   u_int
    PROCPS_PIDS_ID_FGROUP,         //     str
    PROCPS_PIDS_ID_FUID,           //   u_int
    PROCPS_PIDS_ID_FUSER,          //     str
    PROCPS_PIDS_ID_PGRP,           //   s_int
    PROCPS_PIDS_ID_PID,            //   s_int
    PROCPS_PIDS_ID_PPID,           //   s_int
    PROCPS_PIDS_ID_RGID,           //   u_int
    PROCPS_PIDS_ID_RGROUP,         //     str
    PROCPS_PIDS_ID_RUID,           //   u_int
    PROCPS_PIDS_ID_RUSER,          //     str
    PROCPS_PIDS_ID_SESSION,        //   s_int
    PROCPS_PIDS_ID_SGID,           //   u_int
    PROCPS_PIDS_ID_SGROUP,         //     str
    PROCPS_PIDS_ID_SUID,           //   u_int
    PROCPS_PIDS_ID_SUSER,          //     str
    PROCPS_PIDS_ID_TGID,           //   s_int
    PROCPS_PIDS_ID_TPGID,          //   s_int
    PROCPS_PIDS_LXCNAME,           //     str
    PROCPS_PIDS_MEM_CODE,          //  sl_int
    PROCPS_PIDS_MEM_CODE_KIB,      //  sl_int
    PROCPS_PIDS_MEM_DATA,          //  sl_int
    PROCPS_PIDS_MEM_DATA_KIB,      //  sl_int
    PROCPS_PIDS_MEM_DT,            //  sl_int
    PROCPS_PIDS_MEM_LRS,           //  sl_int
    PROCPS_PIDS_MEM_RES,           //  sl_int
    PROCPS_PIDS_MEM_RES_KIB,       //  sl_int
    PROCPS_PIDS_MEM_SHR,           //  sl_int
    PROCPS_PIDS_MEM_SHR_KIB,       //  sl_int
    PROCPS_PIDS_MEM_VIRT,          //  sl_int
    PROCPS_PIDS_MEM_VIRT_KIB,      //  sl_int
    PROCPS_PIDS_NICE,              //  sl_int
    PROCPS_PIDS_NLWP,              //   s_int
    PROCPS_PIDS_NS_IPC,            //  ul_int
    PROCPS_PIDS_NS_MNT,            //  ul_int
    PROCPS_PIDS_NS_NET,            //  ul_int
    PROCPS_PIDS_NS_PID,            //  ul_int
    PROCPS_PIDS_NS_USER,           //  ul_int
    PROCPS_PIDS_NS_UTS,            //  ul_int
    PROCPS_PIDS_OOM_ADJ,           //   s_int
    PROCPS_PIDS_OOM_SCORE,         //   s_int
    PROCPS_PIDS_PRIORITY,          //   s_int
    PROCPS_PIDS_PROCESSOR,         //   u_int
    PROCPS_PIDS_RSS,               //  sl_int
    PROCPS_PIDS_RSS_RLIM,          //  ul_int
    PROCPS_PIDS_RTPRIO,            //  ul_int
    PROCPS_PIDS_SCHED_CLASS,       //  ul_int
    PROCPS_PIDS_SD_MACH,           //     str
    PROCPS_PIDS_SD_OUID,           //     str
    PROCPS_PIDS_SD_SEAT,           //     str
    PROCPS_PIDS_SD_SESS,           //     str
    PROCPS_PIDS_SD_SLICE,          //     str
    PROCPS_PIDS_SD_UNIT,           //     str
    PROCPS_PIDS_SD_UUNIT,          //     str
    PROCPS_PIDS_SIGBLOCKED,        //     str
    PROCPS_PIDS_SIGCATCH,          //     str
    PROCPS_PIDS_SIGIGNORE,         //     str
    PROCPS_PIDS_SIGNALS,           //     str
    PROCPS_PIDS_SIGPENDING,        //     str
    PROCPS_PIDS_STATE,             //    s_ch
    PROCPS_PIDS_SUPGIDS,           //     str
    PROCPS_PIDS_SUPGROUPS,         //     str
    PROCPS_PIDS_TICS_ALL,          // ull_int
    PROCPS_PIDS_TICS_ALL_C,        // ull_int
    PROCPS_PIDS_TICS_DELTA,        //  sl_int
    PROCPS_PIDS_TICS_SYSTEM,       // ull_int
    PROCPS_PIDS_TICS_SYSTEM_C,     // ull_int
    PROCPS_PIDS_TICS_USER,         // ull_int
    PROCPS_PIDS_TICS_USER_C,       // ull_int
    PROCPS_PIDS_TIME_ALL,          // ull_int
    PROCPS_PIDS_TIME_ELAPSED,      // ull_int
    PROCPS_PIDS_TIME_START,        // ull_int
    PROCPS_PIDS_TTY,               //   s_int
    PROCPS_PIDS_TTY_NAME,          //     str
    PROCPS_PIDS_TTY_NUMBER,        //     str
    PROCPS_PIDS_VM_DATA,           //  sl_int
    PROCPS_PIDS_VM_EXE,            //  sl_int
    PROCPS_PIDS_VM_LIB,            //  sl_int
    PROCPS_PIDS_VM_RSS,            //  sl_int
    PROCPS_PIDS_VM_RSS_ANON,       //  sl_int
    PROCPS_PIDS_VM_RSS_FILE,       //  sl_int
    PROCPS_PIDS_VM_RSS_LOCKED,     //  sl_int
    PROCPS_PIDS_VM_RSS_SHARED,     //  sl_int
    PROCPS_PIDS_VM_SIZE,           //  sl_int
    PROCPS_PIDS_VM_STACK,          //  sl_int
    PROCPS_PIDS_VM_SWAP,           //  sl_int
    PROCPS_PIDS_VM_USED,           //  sl_int
    PROCPS_PIDS_VSIZE_PGS,         //  ul_int
    PROCPS_PIDS_WCHAN_ADDR,        //  ul_int
    PROCPS_PIDS_WCHAN_NAME         //     str
};

enum pids_fetch_type {
    PROCPS_FETCH_TASKS_ONLY,
    PROCPS_FETCH_THREADS_TOO
};

enum pids_select_type {
    PROCPS_SELECT_PID  = 0x1000,
    PROCPS_SELECT_UID  = 0x4000
};

enum pids_sort_order {
    PROCPS_PIDS_ASCEND   = +1,
    PROCPS_PIDS_DESCEND  = -1
};


struct pids_result {
    enum pids_item item;
    union {
        signed char         s_ch;
        signed int          s_int;
        unsigned int        u_int;
        signed long         sl_int;
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
    struct pids_stack **stacks;
    struct pids_counts counts;
};


#define PROCPS_PIDS_VAL( relative_enum, type, stack ) \
    stack -> head [ relative_enum ] . result . type


struct procps_pidsinfo;

int procps_pids_new   (struct procps_pidsinfo **info, enum pids_item *items, int numitems);
int procps_pids_ref   (struct procps_pidsinfo  *info);
int procps_pids_unref (struct procps_pidsinfo **info);

struct pids_stack *fatal_proc_unmounted (
    struct procps_pidsinfo *info,
    int return_self);

struct pids_stack *procps_pids_get (
    struct procps_pidsinfo *info,
    enum pids_fetch_type which);

struct pids_fetch *procps_pids_reap (
    struct procps_pidsinfo *info,
    enum pids_fetch_type which);

int procps_pids_reset (
    struct procps_pidsinfo *info,
    enum pids_item *newitems,
    int newnumitems);

struct pids_fetch *procps_pids_select (
    struct procps_pidsinfo *info,
    unsigned *these,
    int numthese,
    enum pids_select_type which);

struct pids_stack **procps_pids_sort (
    struct procps_pidsinfo *info,
    struct pids_stack *stacks[],
    int numstacked,
    enum pids_item sortitem,
    enum pids_sort_order order);

__END_DECLS
#endif /* _PROC_PIDS_H */
