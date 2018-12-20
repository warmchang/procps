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

#ifdef __cplusplus
extern "C" {
#endif

enum pids_item {
    PIDS_noop,              //        ( never altered )
    PIDS_extra,             //        ( reset to zero )
                            //  returns        origin, see proc(5)
                            //  -------        -------------------
    PIDS_ADDR_END_CODE,     //   ul_int        stat
    PIDS_ADDR_KSTK_EIP,     //   ul_int        stat
    PIDS_ADDR_KSTK_ESP,     //   ul_int        stat
    PIDS_ADDR_START_CODE,   //   ul_int        stat
    PIDS_ADDR_START_STACK,  //   ul_int        stat
    PIDS_CGNAME,            //      str        cgroup
    PIDS_CGROUP,            //      str        cgroup
    PIDS_CGROUP_V,          //     strv        cgroup
    PIDS_CMD,               //      str        stat or status
    PIDS_CMDLINE,           //      str        cmdline
    PIDS_CMDLINE_V,         //     strv        cmdline
    PIDS_ENVIRON,           //      str        environ
    PIDS_ENVIRON_V,         //     strv        environ
    PIDS_EXE,               //      str        exe
    PIDS_EXIT_SIGNAL,       //    s_int        stat
    PIDS_FLAGS,             //   ul_int        stat
    PIDS_FLT_MAJ,           //   ul_int        stat
    PIDS_FLT_MAJ_C,         //   ul_int        stat
    PIDS_FLT_MAJ_DELTA,     //    s_int        stat
    PIDS_FLT_MIN,           //   ul_int        stat
    PIDS_FLT_MIN_C,         //   ul_int        stat
    PIDS_FLT_MIN_DELTA,     //    s_int        stat
    PIDS_ID_EGID,           //    u_int        status
    PIDS_ID_EGROUP,         //      str      [ EGID based, see: getgrgid(3) ]
    PIDS_ID_EUID,           //    u_int        status
    PIDS_ID_EUSER,          //      str      [ EUID based, see: getpwuid(3) ]
    PIDS_ID_FGID,           //    u_int        status
    PIDS_ID_FGROUP,         //      str      [ FGID based, see: getgrgid(3) ]
    PIDS_ID_FUID,           //    u_int        status
    PIDS_ID_FUSER,          //      str      [ FUID based, see: getpwuid(3) ]
    PIDS_ID_LOGIN,          //    s_int        loginuid
    PIDS_ID_PGRP,           //    s_int        stat
    PIDS_ID_PID,            //    s_int        as: /proc/<pid>
    PIDS_ID_PPID,           //    s_int        stat or status
    PIDS_ID_RGID,           //    u_int        status
    PIDS_ID_RGROUP,         //      str      [ RGID based, see: getgrgid(3) ]
    PIDS_ID_RUID,           //    u_int        status
    PIDS_ID_RUSER,          //      str      [ RUID based, see: getpwuid(3) ]
    PIDS_ID_SESSION,        //    s_int        stat
    PIDS_ID_SGID,           //    u_int        status
    PIDS_ID_SGROUP,         //      str      [ SGID based, see: getgrgid(3) ]
    PIDS_ID_SUID,           //    u_int        status
    PIDS_ID_SUSER,          //      str      [ SUID based, see: getpwuid(3) ]
    PIDS_ID_TGID,           //    s_int        status
    PIDS_ID_TID,            //    s_int        as: /proc/<pid>/task/<tid>
    PIDS_ID_TPGID,          //    s_int        stat
    PIDS_LXCNAME,           //      str        cgroup
    PIDS_MEM_CODE,          //   ul_int        statm
    PIDS_MEM_CODE_PGS,      //   ul_int        statm
    PIDS_MEM_DATA,          //   ul_int        statm
    PIDS_MEM_DATA_PGS,      //   ul_int        statm
    PIDS_MEM_RES,           //   ul_int        statm
    PIDS_MEM_RES_PGS,       //   ul_int        statm
    PIDS_MEM_SHR,           //   ul_int        statm
    PIDS_MEM_SHR_PGS,       //   ul_int        statm
    PIDS_MEM_VIRT,          //   ul_int        statm
    PIDS_MEM_VIRT_PGS,      //   ul_int        statm
    PIDS_NICE,              //    s_int        stat
    PIDS_NLWP,              //    s_int        stat or status
    PIDS_NS_IPC,            //   ul_int        ns/
    PIDS_NS_MNT,            //   ul_int        ns/
    PIDS_NS_NET,            //   ul_int        ns/
    PIDS_NS_PID,            //   ul_int        ns/
    PIDS_NS_USER,           //   ul_int        ns/
    PIDS_NS_UTS,            //   ul_int        ns/
    PIDS_OOM_ADJ,           //    s_int        oom_score_adj
    PIDS_OOM_SCORE,         //    s_int        oom_score
    PIDS_PRIORITY,          //    s_int        stat
    PIDS_PROCESSOR,         //    u_int        stat
    PIDS_PROCESSOR_NODE,    //    s_int        stat
    PIDS_RSS,               //   ul_int        stat
    PIDS_RSS_RLIM,          //   ul_int        stat
    PIDS_RTPRIO,            //    s_int        stat
    PIDS_SCHED_CLASS,       //    s_int        stat
    PIDS_SD_MACH,           //      str      [ PID/TID based, see: sd-login(3) ]
    PIDS_SD_OUID,           //      str         "
    PIDS_SD_SEAT,           //      str         "
    PIDS_SD_SESS,           //      str         "
    PIDS_SD_SLICE,          //      str         "
    PIDS_SD_UNIT,           //      str         "
    PIDS_SD_UUNIT,          //      str         "
    PIDS_SIGBLOCKED,        //      str        status
    PIDS_SIGCATCH,          //      str        status
    PIDS_SIGIGNORE,         //      str        status
    PIDS_SIGNALS,           //      str        status
    PIDS_SIGPENDING,        //      str        status
    PIDS_STATE,             //     s_ch        stat or status
    PIDS_SUPGIDS,           //      str        status
    PIDS_SUPGROUPS,         //      str      [ SUPGIDS based, see: getgrgid(3) ]
    PIDS_TICS_ALL,          //  ull_int        stat
    PIDS_TICS_ALL_C,        //  ull_int        stat
    PIDS_TICS_ALL_DELTA,    //    s_int        stat
    PIDS_TICS_BLKIO,        //  ull_int        stat
    PIDS_TICS_GUEST,        //  ull_int        stat
    PIDS_TICS_GUEST_C,      //  ull_int        stat
    PIDS_TICS_SYSTEM,       //  ull_int        stat
    PIDS_TICS_SYSTEM_C,     //  ull_int        stat
    PIDS_TICS_USER,         //  ull_int        stat
    PIDS_TICS_USER_C,       //  ull_int        stat
    PIDS_TIME_ALL,          //  ull_int        stat
    PIDS_TIME_ELAPSED,      //  ull_int        stat
    PIDS_TIME_START,        //  ull_int        stat
    PIDS_TTY,               //    s_int        stat
    PIDS_TTY_NAME,          //      str        stat
    PIDS_TTY_NUMBER,        //      str        stat
    PIDS_VM_DATA,           //   ul_int        status
    PIDS_VM_EXE,            //   ul_int        status
    PIDS_VM_LIB,            //   ul_int        status
    PIDS_VM_RSS,            //   ul_int        status
    PIDS_VM_RSS_ANON,       //   ul_int        status
    PIDS_VM_RSS_FILE,       //   ul_int        status
    PIDS_VM_RSS_LOCKED,     //   ul_int        status
    PIDS_VM_RSS_SHARED,     //   ul_int        status
    PIDS_VM_SIZE,           //   ul_int        status
    PIDS_VM_STACK,          //   ul_int        status
    PIDS_VM_SWAP,           //   ul_int        status
    PIDS_VM_USED,           //   ul_int        status
    PIDS_VSIZE_PGS,         //   ul_int        stat
    PIDS_WCHAN_NAME         //      str        wchan
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
    int running, sleeping, stopped, zombied, other;
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


#ifdef XTRA_PROCPS_DEBUG
# include <proc/xtra-procps-debug.h>
#endif
#ifdef __cplusplus
}
#endif
#endif
