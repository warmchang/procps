/*
 * pids.c - task/thread/process related declarations for libproc
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

//efine _GNU_SOURCE             // for qsort_r

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <proc/pids.h>
#include <proc/sysinfo.h>
#include <proc/uptime.h>
#include "procps-private.h"

#include "devname.h"                   // and a few headers for our
#include "readproc.h"                  // bridged libprocps support
#include "wchan.h"                     // ( maybe just temporary? )

//#define UNREF_RPTHASH                // report on hashing, at uref time

#define FILL_ID_MAX  255               // upper limit for pid/uid fills
#define MEMORY_INCR  128               // amt by which allocations grow

    // these represent the proc_t fields whose storage cannot be managed
    // optimally if they are ever referenced more than once in any stack
enum rel_ref {
    ref_CGROUP,   ref_CMD,     ref_CMDLINE,   ref_ENVIRON,  ref_SD_MACH,
    ref_SD_OUID,  ref_SD_SEAT, ref_SD_SESS,   ref_SD_SLICE, ref_SD_UNIT,
    ref_SD_UUNIT, ref_SUPGIDS, ref_SUPGROUPS,
    MAXIMUM_ref
};

struct stacks_extent {
    struct pids_stack **stacks;
    int ext_numstacks;
    struct stacks_extent *next;
};

struct fetch_support {
    struct pids_stack **anchor;        // reap/select consolidated extents
    int n_alloc;                       // number of above pointers allocated
    int n_inuse;                       // number of above pointers occupied
    int n_alloc_save;                  // last known summary.stacks allocation
    struct pids_fetch summary;         // counts + stacks for return to caller
};

struct procps_pidsinfo {
    int refcount;
    int maxitems;                      // includes 'logical_end' delimiter
    int curitems;                      // includes 'logical_end' delimiter
    enum pids_item *items;             // includes 'logical_end' delimiter
    struct stacks_extent *extents;     // anchor for all resettable extents
    struct stacks_extent *otherexts;   // anchor for single stack invariant extents
    struct fetch_support fetch;        // support for procps_pids_reap & select
    int history_yes;                   // need historical data
    struct history_info *hist;         // pointer to historical support data
    int dirty_stacks;                  // extents need dynamic storage clean
    proc_t*(*read_something)(PROCTAB*, proc_t*); // readproc/readeither via which
    unsigned pgs2k_shift;              // to convert some proc vaules
    unsigned oldflags;                    // the old library PROC_FILL flagss
    PROCTAB *PT;                       // the old library essential interface
    unsigned long hertz;               // for TIME_ALL & TIME_ELAPSED calculations
    unsigned long long boot_seconds;   // for TIME_ELAPSED calculation
    int ref_counts[MAXIMUM_ref];       // ref counts for special string fields
    PROCTAB *get_PT;                   // old library interface for active 'get'
    struct stacks_extent *get_ext;     // an extent used for active 'get'
    enum pids_fetch_type get_type;     // last known type of 'get' request
};


// ___ Results 'Set' Support ||||||||||||||||||||||||||||||||||||||||||||||||||

#define setNAME(e) set_results_ ## e
#define setDECL(e) static void setNAME(e) \
    (struct procps_pidsinfo *I, struct pids_result *R, proc_t *P)

// convert pages to kib
#define CVT_set(e,t,x) setDECL(e) { \
    R->result. t = (long)(P-> x) << I -> pgs2k_shift; }
// strdup of a static char array
#define DUP_set(e,x) setDECL(e) { \
    (void)I; R->result.str = strdup(P-> x); }
// regular assignment copy
#define REG_set(e,t,x) setDECL(e) { \
    (void)I; R->result. t = P-> x; }
// take ownership of a regular char* string if possible, else duplicate
#define STR_set(e,x) setDECL(e) { \
    if (I->ref_counts[ref_ ## e] > 1) R->result.str = strdup(P-> x); \
    else { R->result.str = P-> x; P-> x = NULL; } }
// take ownership of a vectorized single string if possible, else duplicate
#define STV_set(e,x) setDECL(e) { \
    if (I->ref_counts[ref_ ## e] > 1) R->result.str = strdup(*P-> x); \
    else { R->result.str = *P-> x; P-> x = NULL; } }
/*
   take ownership of true vectorized strings if possible, else return NULL
   [ if there's a source field ref_count, then those true string vectors ]
   [ have already been converted into a single string so we return NULL. ]
   [ otherwise, the first result struct now gets ownership of those true ]
   [ string vectors and any duplicate structures will then receive NULL. ]
*/
#define VEC_set(e1,e2,x) setDECL(e1) { \
    if (I->ref_counts[ref_ ## e2]) R->result.strv = NULL; \
    else { R->result.strv = P-> x; P-> x = NULL; } }

setDECL(noop)         { (void)I; (void)R; (void)P; return; }
setDECL(extra)        { (void)I; (void)R; (void)P; return; }
REG_set(ADDR_END_CODE,    ul_int,  end_code)
REG_set(ADDR_KSTK_EIP,    ul_int,  kstk_eip)
REG_set(ADDR_KSTK_ESP,    ul_int,  kstk_esp)
REG_set(ADDR_START_CODE,  ul_int,  start_code)
REG_set(ADDR_START_STACK, ul_int,  start_stack)
REG_set(ALARM,            sl_int,  alarm)
setDECL(CGNAME)       { char *name = strstr(*P->cgroup, ":name="); if (name && *(name+6)) name += 6; else name = *P->cgroup; R->result.str = strdup(name); }
STV_set(CGROUP,                    cgroup)
VEC_set(CGROUP_V,     CGROUP,      cgroup)
STR_set(CMD,                       cmd)
STV_set(CMDLINE,                   cmdline)
VEC_set(CMDLINE_V,    CMDLINE,     cmdline)
STV_set(ENVIRON,                   environ)
VEC_set(ENVIRON_V,    ENVIRON,     environ)
REG_set(EXIT_SIGNAL,      s_int,   exit_signal)
REG_set(FLAGS,            ul_int,  flags)
REG_set(FLT_MAJ,          sl_int,  maj_flt)
REG_set(FLT_MAJ_C,        sl_int,  cmaj_flt)
REG_set(FLT_MAJ_DELTA,    sl_int,  maj_delta)
REG_set(FLT_MIN,          sl_int,  min_flt)
REG_set(FLT_MIN_C,        sl_int,  cmin_flt)
REG_set(FLT_MIN_DELTA,    sl_int,  min_delta)
REG_set(ID_EGID,          u_int,   egid)
REG_set(ID_EGROUP,        str,     egroup)
REG_set(ID_EUID,          u_int,   euid)
REG_set(ID_EUSER,         str,     euser)
REG_set(ID_FGID,          u_int,   fgid)
REG_set(ID_FGROUP,        str,     fgroup)
REG_set(ID_FUID,          u_int,   fuid)
REG_set(ID_FUSER,         str,     fuser)
REG_set(ID_PGRP,          s_int,   pgrp)
REG_set(ID_PID,           s_int,   tid)
REG_set(ID_PPID,          s_int,   ppid)
REG_set(ID_RGID,          u_int,   rgid)
REG_set(ID_RGROUP,        str,     rgroup)
REG_set(ID_RUID,          u_int,   ruid)
REG_set(ID_RUSER,         str,     ruser)
REG_set(ID_SESSION,       s_int,   session)
REG_set(ID_SGID,          u_int,   sgid)
REG_set(ID_SGROUP,        str,     sgroup)
REG_set(ID_SUID,          u_int,   suid)
REG_set(ID_SUSER,         str,     suser)
REG_set(ID_TGID,          s_int,   tgid)
REG_set(ID_TPGID,         s_int,   tpgid)
setDECL(LXCNAME)      { (void)I; R->result.str = (char *)P->lxcname; }
REG_set(MEM_CODE,         sl_int,  trs)
CVT_set(MEM_CODE_KIB,     sl_int,  trs)
REG_set(MEM_DATA,         sl_int,  drs)
CVT_set(MEM_DATA_KIB,     sl_int,  drs)
REG_set(MEM_DT,           sl_int,  dt)
REG_set(MEM_LRS,          sl_int,  lrs)
REG_set(MEM_RES,          sl_int,  resident)
CVT_set(MEM_RES_KIB,      sl_int,  resident)
REG_set(MEM_SHR,          sl_int,  share)
CVT_set(MEM_SHR_KIB,      ul_int,  share)
REG_set(MEM_VIRT,         sl_int,  size)
CVT_set(MEM_VIRT_KIB,     sl_int,  size)
REG_set(NICE,             sl_int,  nice)
REG_set(NLWP,             s_int,   nlwp)
REG_set(NS_IPC,           ul_int,  ns.ns[0])
REG_set(NS_MNT,           ul_int,  ns.ns[1])
REG_set(NS_NET,           ul_int,  ns.ns[2])
REG_set(NS_PID,           ul_int,  ns.ns[3])
REG_set(NS_USER,          ul_int,  ns.ns[4])
REG_set(NS_UTS,           ul_int,  ns.ns[5])
REG_set(OOM_ADJ,          s_int,   oom_adj)
REG_set(OOM_SCORE,        s_int,   oom_score)
REG_set(PRIORITY,         s_int,   priority)
REG_set(PROCESSOR,        u_int,   processor)
REG_set(RSS,              sl_int,  rss)
REG_set(RSS_RLIM,         ul_int,  rss_rlim)
REG_set(RTPRIO,           ul_int,  rtprio)
REG_set(SCHED_CLASS,      ul_int,  sched)
STR_set(SD_MACH,                   sd_mach)
STR_set(SD_OUID,                   sd_ouid)
STR_set(SD_SEAT,                   sd_seat)
STR_set(SD_SESS,                   sd_sess)
STR_set(SD_SLICE,                  sd_slice)
STR_set(SD_UNIT,                   sd_unit)
STR_set(SD_UUNIT,                  sd_uunit)
DUP_set(SIGBLOCKED,                blocked)
DUP_set(SIGCATCH,                  sigcatch)
DUP_set(SIGIGNORE,                 sigignore)
DUP_set(SIGNALS,                   signal)
DUP_set(SIGPENDING,                _sigpnd)
REG_set(STATE,            s_ch,    state)
STR_set(SUPGIDS,                   supgid)
STR_set(SUPGROUPS,                 supgrp)
setDECL(TICS_ALL)     { (void)I; R->result.ull_int = P->utime + P->stime; }
setDECL(TICS_ALL_C)   { (void)I; R->result.ull_int = P->utime + P->stime + P->cutime + P->cstime; }
REG_set(TICS_DELTA,       sl_int,  pcpu)
REG_set(TICS_SYSTEM,      ull_int, stime)
REG_set(TICS_SYSTEM_C,    ull_int, cstime)
REG_set(TICS_USER,        ull_int, utime)
REG_set(TICS_USER_C,      ull_int, cutime)
setDECL(TIME_ALL)     { R->result.ull_int = (P->utime + P->stime) / I->hertz; }
setDECL(TIME_ELAPSED) { R->result.ull_int = (I->boot_seconds >= (P->start_time / I->hertz)) ? I->boot_seconds - (P->start_time / I->hertz) : 0; }
REG_set(TIME_START,       ull_int, start_time)
REG_set(TTY,              s_int,   tty)
setDECL(TTY_NAME)     { char buf[64]; (void)I; dev_to_tty(buf, sizeof(buf), P->tty, P->tid, ABBREV_DEV); R->result.str = strdup(buf); }
setDECL(TTY_NUMBER)   { char buf[64]; (void)I; dev_to_tty(buf, sizeof(buf), P->tty, P->tid, ABBREV_DEV|ABBREV_TTY|ABBREV_PTS); R->result.str = strdup(buf); }
REG_set(VM_DATA,          sl_int,  vm_data)
REG_set(VM_EXE,           sl_int,  vm_exe)
REG_set(VM_LIB,           sl_int,  vm_lib)
REG_set(VM_RSS,           sl_int,  vm_rss)
REG_set(VM_RSS_ANON,      sl_int,  vm_rss_anon)
REG_set(VM_RSS_FILE,      sl_int,  vm_rss_file)
REG_set(VM_RSS_LOCKED,    sl_int,  vm_lock)
REG_set(VM_RSS_SHARED,    sl_int,  vm_rss_shared)
REG_set(VM_SIZE,          sl_int,  vm_size)
REG_set(VM_STACK,         sl_int,  vm_stack)
REG_set(VM_SWAP,          sl_int,  vm_swap)
setDECL(VM_USED)      { (void)I; R->result.sl_int = P->vm_swap + P->vm_rss; }
REG_set(VSIZE_PGS,        ul_int,  vsize)
REG_set(WCHAN_ADDR,       ul_int,  wchan)
setDECL(WCHAN_NAME)   { (void)I; R->result.str = strdup(lookup_wchan(P->tid)); }

#undef setDECL
#undef CVT_set
#undef DUP_set
#undef REG_set
#undef STR_set
#undef STV_set
#undef VEC_set


// ___ Free Storage Support |||||||||||||||||||||||||||||||||||||||||||||||||||

#define freNAME(e) free_results_ ## e

static void freNAME(str) (struct pids_result *R) {
    if (R->result.str) free(R->result.str);
}

static void freNAME(strv) (struct pids_result *R) {
    if (R->result.strv && *R->result.strv) free(*R->result.strv);
}


// ___ Sorting Support ||||||||||||||||||||||||||||||||||||||||||||||||||||||||

struct sort_parms {
    int offset;
    enum pids_sort_order order;
};

#define srtNAME(e) sort_results_ ## e

#define NUM_srt(T) static int srtNAME(T) ( \
  const struct pids_stack **A, const struct pids_stack **B, struct sort_parms *P) { \
    const struct pids_result *a = (*A)->head + P->offset; \
    const struct pids_result *b = (*B)->head + P->offset; \
    return P->order * (a->result. T - b->result. T); }

#define REG_srt(T) static int srtNAME(T) ( \
  const struct pids_stack **A, const struct pids_stack **B, struct sort_parms *P) { \
    const struct pids_result *a = (*A)->head + P->offset; \
    const struct pids_result *b = (*B)->head + P->offset; \
    if ( a->result. T > b->result. T ) return P->order > 0 ?  1 : -1; \
    if ( a->result. T < b->result. T ) return P->order > 0 ? -1 :  1; \
    return 0; }

NUM_srt(s_ch)
NUM_srt(s_int)
NUM_srt(sl_int)

REG_srt(u_int)
REG_srt(ul_int)
REG_srt(ull_int)

static int srtNAME(str) (
  const struct pids_stack **A, const struct pids_stack **B, struct sort_parms *P) {
    const struct pids_result *a = (*A)->head + P->offset;
    const struct pids_result *b = (*B)->head + P->offset;
    return P->order * strcoll(a->result.str, b->result.str);
}

static int srtNAME(strv) (
  const struct pids_stack **A, const struct pids_stack **B, struct sort_parms *P) {
    const struct pids_result *a = (*A)->head + P->offset;
    const struct pids_result *b = (*B)->head + P->offset;
    if (!a->result.strv || !b->result.strv) return 0;
    return P->order * strcoll((*a->result.strv), (*b->result.strv));
}

static int srtNAME(strvers) (
  const struct pids_stack **A, const struct pids_stack **B, struct sort_parms *P) {
    const struct pids_result *a = (*A)->head + P->offset;
    const struct pids_result *b = (*B)->head + P->offset;
    return P->order * strverscmp(a->result.str, b->result.str);
}

static int srtNAME(noop) (
  const struct pids_stack **A, const struct pids_stack **B, enum pids_item *O) {
    (void)A; (void)B; (void)O;
    return 0;
}

#undef NUM_srt
#undef REG_srt


// ___ Controlling Table ||||||||||||||||||||||||||||||||||||||||||||||||||||||

   // from either 'stat' or 'status' (preferred)
#define f_either   PROC_SPARE_1
#define f_grp      PROC_FILLGRP
#define f_lxc      PROC_FILL_LXC
#define f_ns       PROC_FILLNS
#define f_oom      PROC_FILLOOM
#define f_stat     PROC_FILLSTAT
#define f_statm    PROC_FILLMEM
#define f_status   PROC_FILLSTATUS
#define f_systemd  PROC_FILLSYSTEMD
#define f_usr      PROC_FILLUSR
   // these next three will yield true verctorized strings
#define v_arg      PROC_FILLARG
#define v_cgroup   PROC_FILLCGROUP
#define v_env      PROC_FILLENV
   // remaining are compound flags, yielding a single string (maybe vectorized)
#define x_cgroup   PROC_EDITCGRPCVT | PROC_FILLCGROUP           // just 1 str
#define x_cmdline  PROC_EDITCMDLCVT | PROC_FILLARG              // just 1 str
#define x_environ  PROC_EDITENVRCVT | PROC_FILLENV              // just 1 str
#define x_ogroup   PROC_FILLSTATUS  | PROC_FILLGRP
#define x_ouser    PROC_FILLSTATUS  | PROC_FILLUSR
#define x_supgrp   PROC_FILLSTATUS  | PROC_FILLSUPGRP

typedef void (*SET_t)(struct procps_pidsinfo *, struct pids_result *, proc_t *);
typedef void (*FRE_t)(struct pids_result *);
typedef int  (*QSR_t)(const void *, const void *, void *);

#define RS(e) (SET_t)setNAME(e)
#define FF(e) (FRE_t)freNAME(e)
#define QS(t) (QSR_t)srtNAME(t)


        /*
         * Need it be said?
         * This table must be kept in the exact same order as
         * those 'enum pids_item' guys ! */
static struct {
    SET_t    setsfunc;            // the actual result setting routine
    unsigned oldflags;            // PROC_FILLxxxx flags for this item
    FRE_t    freefunc;            // free function for strings storage
    QSR_t    sortfunc;            // sort cmp func for a specific type
    int      needhist;            // a result requires history support
    int      refcount;            // the result needs reference counts
} Item_table[] = {
/*    setsfunc               oldflags    freefunc   sortfunc      needhist  refcount
      ---------------------  ----------  ---------  ------------  --------  ------------- */
    { RS(noop),              0,          NULL,      QS(noop),     0,        -1            }, // user only, never altered
    { RS(extra),             0,          NULL,      QS(ull_int),  0,        -1            }, // user only, reset to zero
    { RS(ADDR_END_CODE),     f_stat,     NULL,      QS(ul_int),   0,        -1            },
    { RS(ADDR_KSTK_EIP),     f_stat,     NULL,      QS(ul_int),   0,        -1            },
    { RS(ADDR_KSTK_ESP),     f_stat,     NULL,      QS(ul_int),   0,        -1            },
    { RS(ADDR_START_CODE),   f_stat,     NULL,      QS(ul_int),   0,        -1            },
    { RS(ADDR_START_STACK),  f_stat,     NULL,      QS(ul_int),   0,        -1            },
    { RS(ALARM),             f_stat,     NULL,      QS(sl_int),   0,        -1            },
    { RS(CGNAME),            x_cgroup,   FF(str),   QS(str),      0,        ref_CGROUP    }, // refcount: diff result, same source
    { RS(CGROUP),            x_cgroup,   FF(str),   QS(str),      0,        ref_CGROUP    }, // refcount: diff result, same source
    { RS(CGROUP_V),          v_cgroup,   FF(strv),  QS(strv),     0,        -1            },
    { RS(CMD),               f_either,   FF(str),   QS(str),      0,        ref_CMD       },
    { RS(CMDLINE),           x_cmdline,  FF(str),   QS(str),      0,        ref_CMDLINE   },
    { RS(CMDLINE_V),         v_arg,      FF(strv),  QS(strv),     0,        -1            },
    { RS(ENVIRON),           x_environ,  FF(str),   QS(str),      0,        ref_ENVIRON   },
    { RS(ENVIRON_V),         v_env,      FF(strv),  QS(strv),     0,        -1            },
    { RS(EXIT_SIGNAL),       f_stat,     NULL,      QS(s_int),    0,        -1            },
    { RS(FLAGS),             f_stat,     NULL,      QS(ul_int),   0,        -1            },
    { RS(FLT_MAJ),           f_stat,     NULL,      QS(sl_int),   0,        -1            },
    { RS(FLT_MAJ_C),         f_stat,     NULL,      QS(sl_int),   0,        -1            },
    { RS(FLT_MAJ_DELTA),     f_stat,     NULL,      QS(sl_int),   +1,       -1            },
    { RS(FLT_MIN),           f_stat,     NULL,      QS(sl_int),   0,        -1            },
    { RS(FLT_MIN_C),         f_stat,     NULL,      QS(sl_int),   0,        -1            },
    { RS(FLT_MIN_DELTA),     f_stat,     NULL,      QS(sl_int),   +1,       -1            },
    { RS(ID_EGID),           0,          NULL,      QS(u_int),    0,        -1            }, // oldflags: free w/ simple_read...
    { RS(ID_EGROUP),         f_grp,      NULL,      QS(str),      0,        -1            },
    { RS(ID_EUID),           0,          NULL,      QS(u_int),    0,        -1            }, // oldflags: free w/ simple_read...
    { RS(ID_EUSER),          f_usr,      NULL,      QS(str),      0,        -1            },
    { RS(ID_FGID),           f_status,   NULL,      QS(u_int),    0,        -1            },
    { RS(ID_FGROUP),         x_ogroup,   NULL,      QS(str),      0,        -1            },
    { RS(ID_FUID),           f_status,   NULL,      QS(u_int),    0,        -1            },
    { RS(ID_FUSER),          x_ouser,    NULL,      QS(str),      0,        -1            },
    { RS(ID_PGRP),           f_stat,     NULL,      QS(s_int),    0,        -1            },
    { RS(ID_PID),            0,          NULL,      QS(s_int),    0,        -1            }, // oldflags: free w/ simple_nextpid
    { RS(ID_PPID),           f_either,   NULL,      QS(s_int),    0,        -1            },
    { RS(ID_RGID),           f_status,   NULL,      QS(u_int),    0,        -1            },
    { RS(ID_RGROUP),         x_ogroup,   NULL,      QS(str),      0,        -1            },
    { RS(ID_RUID),           f_status,   NULL,      QS(u_int),    0,        -1            },
    { RS(ID_RUSER),          x_ouser,    NULL,      QS(str),      0,        -1            },
    { RS(ID_SESSION),        f_stat,     NULL,      QS(s_int),    0,        -1            },
    { RS(ID_SGID),           f_status,   NULL,      QS(u_int),    0,        -1            },
    { RS(ID_SGROUP),         x_ogroup,   NULL,      QS(str),      0,        -1            },
    { RS(ID_SUID),           f_status,   NULL,      QS(u_int),    0,        -1            },
    { RS(ID_SUSER),          x_ouser,    NULL,      QS(str),      0,        -1            },
    { RS(ID_TGID),           0,          NULL,      QS(s_int),    0,        -1            }, // oldflags: free w/ simple_nextpid
    { RS(ID_TPGID),          f_stat,     NULL,      QS(s_int),    0,        -1            },
    { RS(LXCNAME),           f_lxc,      NULL,      QS(str),      0,        -1            },
    { RS(MEM_CODE),          f_statm,    NULL,      QS(sl_int),   0,        -1            },
    { RS(MEM_CODE_KIB),      f_statm,    NULL,      QS(sl_int),   0,        -1            },
    { RS(MEM_DATA),          f_statm,    NULL,      QS(sl_int),   0,        -1            },
    { RS(MEM_DATA_KIB),      f_statm,    NULL,      QS(sl_int),   0,        -1            },
    { RS(MEM_DT),            f_statm,    NULL,      QS(sl_int),   0,        -1            },
    { RS(MEM_LRS),           f_statm,    NULL,      QS(sl_int),   0,        -1            },
    { RS(MEM_RES),           f_statm,    NULL,      QS(sl_int),   0,        -1            },
    { RS(MEM_RES_KIB),       f_statm,    NULL,      QS(sl_int),   0,        -1            },
    { RS(MEM_SHR),           f_statm,    NULL,      QS(sl_int),   0,        -1            },
    { RS(MEM_SHR_KIB),       f_statm,    NULL,      QS(sl_int),   0,        -1            },
    { RS(MEM_VIRT),          f_statm,    NULL,      QS(sl_int),   0,        -1            },
    { RS(MEM_VIRT_KIB),      f_statm,    NULL,      QS(sl_int),   0,        -1            },
    { RS(NICE),              f_stat,     NULL,      QS(sl_int),   0,        -1            },
    { RS(NLWP),              f_either,   NULL,      QS(s_int),    0,        -1            },
    { RS(NS_IPC),            f_ns,       NULL,      QS(ul_int),   0,        -1            },
    { RS(NS_MNT),            f_ns,       NULL,      QS(ul_int),   0,        -1            },
    { RS(NS_NET),            f_ns,       NULL,      QS(ul_int),   0,        -1            },
    { RS(NS_PID),            f_ns,       NULL,      QS(ul_int),   0,        -1            },
    { RS(NS_USER),           f_ns,       NULL,      QS(ul_int),   0,        -1            },
    { RS(NS_UTS),            f_ns,       NULL,      QS(ul_int),   0,        -1            },
    { RS(OOM_ADJ),           f_oom,      NULL,      QS(s_int),    0,        -1            },
    { RS(OOM_SCORE),         f_oom,      NULL,      QS(s_int),    0,        -1            },
    { RS(PRIORITY),          f_stat,     NULL,      QS(s_int),    0,        -1            },
    { RS(PROCESSOR),         f_stat,     NULL,      QS(u_int),    0,        -1            },
    { RS(RSS),               f_stat,     NULL,      QS(sl_int),   0,        -1            },
    { RS(RSS_RLIM),          f_stat,     NULL,      QS(ul_int),   0,        -1            },
    { RS(RTPRIO),            f_stat,     NULL,      QS(ul_int),   0,        -1            },
    { RS(SCHED_CLASS),       f_stat,     NULL,      QS(ul_int),   0,        -1            },
    { RS(SD_MACH),           f_systemd,  FF(str),   QS(str),      0,        ref_SD_MACH   },
    { RS(SD_OUID),           f_systemd,  FF(str),   QS(str),      0,        ref_SD_OUID   },
    { RS(SD_SEAT),           f_systemd,  FF(str),   QS(str),      0,        ref_SD_SEAT   },
    { RS(SD_SESS),           f_systemd,  FF(str),   QS(str),      0,        ref_SD_SESS   },
    { RS(SD_SLICE),          f_systemd,  FF(str),   QS(str),      0,        ref_SD_SLICE  },
    { RS(SD_UNIT),           f_systemd,  FF(str),   QS(str),      0,        ref_SD_UNIT   },
    { RS(SD_UUNIT),          f_systemd,  FF(str),   QS(str),      0,        ref_SD_UUNIT  },
    { RS(SIGBLOCKED),        f_status,   FF(str),   QS(str),      0,        -1            },
    { RS(SIGCATCH),          f_status,   FF(str),   QS(str),      0,        -1            },
    { RS(SIGIGNORE),         f_status,   FF(str),   QS(str),      0,        -1            },
    { RS(SIGNALS),           f_status,   FF(str),   QS(str),      0,        -1            },
    { RS(SIGPENDING),        f_status,   FF(str),   QS(str),      0,        -1            },
    { RS(STATE),             f_either,   NULL,      QS(s_ch),     0,        -1            },
    { RS(SUPGIDS),           f_status,   FF(str),   QS(str),      0,        ref_SUPGIDS   },
    { RS(SUPGROUPS),         x_supgrp,   FF(str),   QS(str),      0,        ref_SUPGROUPS },
    { RS(TICS_ALL),          f_stat,     NULL,      QS(ull_int),  0,        -1            },
    { RS(TICS_ALL_C),        f_stat,     NULL,      QS(ull_int),  0,        -1            },
    { RS(TICS_DELTA),        f_stat,     NULL,      QS(sl_int),  +1,        -1            },
    { RS(TICS_SYSTEM),       f_stat,     NULL,      QS(ull_int),  0,        -1            },
    { RS(TICS_SYSTEM_C),     f_stat,     NULL,      QS(ull_int),  0,        -1            },
    { RS(TICS_USER),         f_stat,     NULL,      QS(ull_int),  0,        -1            },
    { RS(TICS_USER_C),       f_stat,     NULL,      QS(ull_int),  0,        -1            },
    { RS(TIME_ALL),          f_stat,     NULL,      QS(ull_int),  0,        -1            },
    { RS(TIME_ELAPSED),      f_stat,     NULL,      QS(ull_int),  0,        -1            },
    { RS(TIME_START),        f_stat,     NULL,      QS(ull_int),  0,        -1            },
    { RS(TTY),               f_stat,     NULL,      QS(s_int),    0,        -1            },
    { RS(TTY_NAME),          f_stat,     FF(str),   QS(strvers),  0,        -1            },
    { RS(TTY_NUMBER),        f_stat,     FF(str),   QS(strvers),  0,        -1            },
    { RS(VM_DATA),           f_status,   NULL,      QS(sl_int),   0,        -1            },
    { RS(VM_EXE),            f_status,   NULL,      QS(sl_int),   0,        -1            },
    { RS(VM_LIB),            f_status,   NULL,      QS(sl_int),   0,        -1            },
    { RS(VM_RSS),            f_status,   NULL,      QS(sl_int),   0,        -1            },
    { RS(VM_RSS_ANON),       f_status,   NULL,      QS(sl_int),   0,        -1            },
    { RS(VM_RSS_FILE),       f_status,   NULL,      QS(sl_int),   0,        -1            },
    { RS(VM_RSS_LOCKED),     f_status,   NULL,      QS(sl_int),   0,        -1            },
    { RS(VM_RSS_SHARED),     f_status,   NULL,      QS(sl_int),   0,        -1            },
    { RS(VM_SIZE),           f_status,   NULL,      QS(sl_int),   0,        -1            },
    { RS(VM_STACK),          f_status,   NULL,      QS(sl_int),   0,        -1            },
    { RS(VM_SWAP),           f_status,   NULL,      QS(sl_int),   0,        -1            },
    { RS(VM_USED),           f_status,   NULL,      QS(sl_int),   0,        -1            },
    { RS(VSIZE_PGS),         f_stat,     NULL,      QS(ul_int),   0,        -1            },
    { RS(WCHAN_ADDR),        f_stat,     NULL,      QS(ul_int),   0,        -1            },
    { RS(WCHAN_NAME),        0,          FF(str),   QS(str),      0,        -1            }, // oldflags: tid already free
   // dummy entry corresponding to PROCPS_PIDS_logical_end ...
    { NULL,                  0,          NULL,      NULL,         0,        -1            }
};

   // next MUST be kept in sync with highest value enum
enum pids_item PROCPS_PIDS_logical_end  = PROCPS_PIDS_WCHAN_NAME + 1;

#undef RS
#undef FF
#undef QS
#undef setNAME
#undef freNAME
#undef srtNAME

//#undef f_either                 // needed later
#undef f_grp
#undef f_lxc
#undef f_ns
#undef f_oom
//#undef f_stat                   // needed later
#undef f_statm
//#undef f_status                 // needed later
#undef f_systemd
#undef f_usr
#undef v_arg
#undef v_cgroup
#undef v_env
#undef x_cgroup
#undef x_cmdline
#undef x_environ
#undef x_ogroup
#undef x_ouser
#undef x_supgrp


// ___ History Support Private Functions ||||||||||||||||||||||||||||||||||||||
//   ( stolen from top when he wasn't looking ) -------------------------------

#define HHASH_SIZE  1024
#define _HASH_PID_(K) (K & (HHASH_SIZE - 1))

#define Hr(x)  info->hist->x           // 'hist ref', minimize stolen impact

typedef unsigned long long TIC_t;

typedef struct HST_t {
    TIC_t tics;                        // last frame's tics count
    long maj, min;                     // last frame's maj/min_flt counts
    int pid;                           // record 'key'
    int lnk;                           // next on hash chain
} HST_t;


struct history_info {
    int    num_tasks;                  // used as index (tasks tallied)
    int    HHist_siz;                  // max number of HST_t structs
    HST_t *PHist_sav;                  // alternating 'old/new' HST_t anchors
    HST_t *PHist_new;
    int    HHash_one [HHASH_SIZE];     // the actual hash tables
    int    HHash_two [HHASH_SIZE];     // (accessed via PHash_sav/PHash_new)
    int    HHash_nul [HHASH_SIZE];     // an 'empty' hash table image
    int   *PHash_sav;                  // alternating 'old/new' hash tables
    int   *PHash_new;                  // (aka. the 'one/two' actual tables)
};


static void config_history (
        struct procps_pidsinfo *info)
{
    int i;

    for (i = 0; i < HHASH_SIZE; i++)   // make the 'empty' table image
        Hr(HHash_nul[i]) = -1;
    memcpy(Hr(HHash_one), Hr(HHash_nul), sizeof(Hr(HHash_nul)));
    memcpy(Hr(HHash_two), Hr(HHash_nul), sizeof(Hr(HHash_nul)));
    Hr(PHash_sav) = Hr(HHash_one);     // alternating 'old/new' hash tables
    Hr(PHash_new) = Hr(HHash_two);
} // end: config_history


static inline HST_t *histget (
        struct procps_pidsinfo *info,
        int pid)
{
    int V = Hr(PHash_sav[_HASH_PID_(pid)]);

    while (-1 < V) {
        if (Hr(PHist_sav[V].pid) == pid)
            return &Hr(PHist_sav[V]);
        V = Hr(PHist_sav[V].lnk); }
    return NULL;
} // end: histget


static inline void histput (
        struct procps_pidsinfo *info,
        unsigned this)
{
    int V = _HASH_PID_(Hr(PHist_new[this].pid));

    Hr(PHist_new[this].lnk) = Hr(PHash_new[V]);
    Hr(PHash_new[V] = this);
} // end: histput

#undef _HASH_PID_


static int make_hist (
        struct procps_pidsinfo *info,
        proc_t *p)
{
 #define nSLOT info->hist->num_tasks
    TIC_t tics;
    HST_t *h;

    if (nSLOT + 1 >= Hr(HHist_siz)) {
        Hr(HHist_siz) += MEMORY_INCR;
        Hr(PHist_sav) = realloc(Hr(PHist_sav), sizeof(HST_t) * Hr(HHist_siz));
        Hr(PHist_new) = realloc(Hr(PHist_new), sizeof(HST_t) * Hr(HHist_siz));
        if (!Hr(PHist_sav) || !Hr(PHist_new))
            return -ENOMEM;
    }
    Hr(PHist_new[nSLOT].pid)  = p->tid;
    Hr(PHist_new[nSLOT].tics) = tics = (p->utime + p->stime);
    Hr(PHist_new[nSLOT].maj)  = p->maj_flt;
    Hr(PHist_new[nSLOT].min)  = p->min_flt;

    histput(info, nSLOT);

    if ((h = histget(info, p->tid))) {
        tics -= h->tics;
        p->pcpu = tics;
        p->maj_delta = p->maj_flt - h->maj;
        p->min_delta = p->min_flt - h->min;
    }

    nSLOT++;
    return 0;
 #undef nSLOT
} // end: make_hist


static inline void toggle_history (
        struct procps_pidsinfo *info)
{
    void *v;

    v = Hr(PHist_sav);
    Hr(PHist_sav) = Hr(PHist_new);
    Hr(PHist_new) = v;

    v = Hr(PHash_sav);
    Hr(PHash_sav) = Hr(PHash_new);
    Hr(PHash_new) = v;
    memcpy(Hr(PHash_new), Hr(HHash_nul), sizeof(Hr(HHash_nul)));

    info->hist->num_tasks = 0;
} // end: toggle_history


#ifdef UNREF_RPTHASH
static void unref_rpthash (
        struct procps_pidsinfo *info)
{
    int i, j, pop, total_occupied, maxdepth, maxdepth_sav, numdepth
        , cross_foot, sz = HHASH_SIZE * (int)sizeof(int)
        , hsz = (int)sizeof(HST_t) * Hr(HHist_siz);
    int depths[HHASH_SIZE];

    for (i = 0, total_occupied = 0, maxdepth = 0; i < HHASH_SIZE; i++) {
        int V = Hr(PHash_new[i]);
        j = 0;
        if (-1 < V) {
            ++total_occupied;
            while (-1 < V) {
                V = Hr(PHist_new[V].lnk);
                if (-1 < V) j++;
            }
        }
        depths[i] = j;
        if (maxdepth < j) maxdepth = j;
    }
    maxdepth_sav = maxdepth;

    fprintf(stderr,
        "\n    History Memory Costs:"
        "\n\tHST_t size = %d, total allocated = %d,"
        "\n\tthus PHist_new & PHist_sav consumed %dk (%d) total bytes."
        "\n"
        "\n\tTwo hash tables provide for %d entries each + 1 extra 'empty' image,"
        "\n\tthus %dk (%d) bytes per table for %dk (%d) total bytes."
        "\n"
        "\n\tGrand total = %dk (%d) bytes."
        "\n"
        "\n    Hash Results Report:"
        "\n\tTotal hashed = %d"
        "\n\tLevel-0 hash entries = %d (%d%% occupied)"
        "\n\tMax Depth = %d"
        "\n\n"
        , (int)sizeof(HST_t),  Hr(HHist_siz)
        , hsz / 1024, hsz
        , HHASH_SIZE
        , sz / 1024, sz, (sz * 3) / 1024, sz * 3
        , (hsz + (sz * 3)) / 1024, hsz + (sz * 3)
        , info->hist->num_tasks
        , total_occupied, (total_occupied * 100) / HHASH_SIZE
        , maxdepth);

    if (total_occupied) {
        for (pop = total_occupied, cross_foot = 0; maxdepth; maxdepth--) {
            for (i = 0, numdepth = 0; i < HHASH_SIZE; i++)
                if (depths[i] == maxdepth) ++numdepth;
            fprintf(stderr,
                "\t %5d (%3d%%) hash table entries at depth %d\n"
                , numdepth, (numdepth * 100) / total_occupied, maxdepth);
            pop -= numdepth;
            cross_foot += numdepth;
            if (0 == pop && cross_foot == total_occupied) break;
        }
        if (pop) {
            fprintf(stderr, "\t %5d (%3d%%) unchained entries (at depth 0)\n"
                , pop, (pop * 100) / total_occupied);
            cross_foot += pop;
        }
        fprintf(stderr,
            "\t -----\n"
            "\t %5d total entries occupied\n", cross_foot);

        if (maxdepth_sav > 1) {
            fprintf(stderr, "\n    PIDs at max depth: ");
            for (i = 0; i < HHASH_SIZE; i++)
                if (depths[i] == maxdepth_sav) {
                    j = Hr(PHash_new[i]);
                    fprintf(stderr, "\n\tpos %4d:  %05d", i, Hr(PHist_new[j].pid));
                    while (-1 < j) {
                        j = Hr(PHist_new[j].lnk);
                        if (-1 < j) fprintf(stderr, ", %05d", Hr(PHist_new[j].pid));
                    }
                }
            fprintf(stderr, "\n");
        }
    }
} // end: unref_rpthash
#endif // UNREF_RPTHASH

#undef Hr
#undef HHASH_SIZE


// ___ Standard Private Functions |||||||||||||||||||||||||||||||||||||||||||||

static inline void assign_results (
        struct procps_pidsinfo *info,
        struct pids_stack *stack,
        proc_t *p)
{
    struct pids_result *this = stack->head;

    for (;;) {
        enum pids_item item = this->item;
        if (item >= PROCPS_PIDS_logical_end)
            break;
        Item_table[item].setsfunc(info, this, p);
        info->dirty_stacks |= Item_table[item].freefunc ? 1 : 0;
        ++this;
    }
    return;
} // end: assign_results


static inline void cleanup_stack (
        struct pids_result *this)
{
    for (;;) {
        enum pids_item item = this->item;
        if (item >= PROCPS_PIDS_logical_end)
            break;
        if (Item_table[item].freefunc)
            Item_table[item].freefunc(this);
        if (item > PROCPS_PIDS_noop)
            this->result.ull_int = 0;
        ++this;
    }
} // end: cleanup_stack


static inline void cleanup_stacks_all (
        struct procps_pidsinfo *info)
{
    struct stacks_extent *ext = info->extents;
    int i;

    while (ext) {
        for (i = 0; ext->stacks[i]; i++)
            cleanup_stack(ext->stacks[i]->head);
        ext = ext->next;
    };
    info->dirty_stacks = 0;
} // end: cleanup_stacks_all


        /*
         * This routine exists in case we ever want to offer something like
         * 'static' or 'invarient' results stacks.  By unsplicing an extent
         * from the info anchor it will be isolated from future reset/free. */
static struct stacks_extent *extent_cut (
        struct procps_pidsinfo *info,
        struct stacks_extent *ext)
{
    struct stacks_extent *p = info->extents;

    if (ext) {
        if (ext == p) {
            info->extents = p->next;
            return ext;
        }
        do {
            if (ext == p->next) {
                p->next = p->next->next;
                return ext;
            }
            p = p->next;
        } while (p);
    }
    return NULL;
} // end: extent_cut


static void extents_free_all (
        struct procps_pidsinfo *info)
{
    struct stacks_extent *ext = info->extents;

    while (ext) {
        info->extents = ext->next;
        free(ext);
        ext = info->extents;
    };
    info->dirty_stacks = 0;
} // end: extents_free_all


static inline struct pids_result *itemize_stack (
        struct pids_result *p,
        int depth,
        enum pids_item *items)
{
    struct pids_result *p_sav = p;
    int i;

    for (i = 0; i < depth; i++) {
        p->item = items[i];
        p->result.ull_int = 0;
        ++p;
    }
    return p_sav;
} // end: itemize_stack


static void itemize_stacks_all (
        struct procps_pidsinfo *info)
{
    struct stacks_extent *ext = info->extents;

    while (ext) {
        int i;
        for (i = 0; ext->stacks[i]; i++)
            itemize_stack(ext->stacks[i]->head, info->curitems, info->items);
        ext = ext->next;
    };
    info->dirty_stacks = 0;
}


static inline int items_check_failed (
        int numitems,
        enum pids_item *items)
{
    int i;

    /* if an enum is passed instead of an address of one or more enums, ol' gcc
     * will silently convert it to an address (possibly NULL).  only clang will
     * offer any sort of warning like the following:
     *
     * warning: incompatible integer to pointer conversion passing 'int' to parameter of type 'enum pids_item *'
     * if (procps_pids_new(&info, PROCPS_PIDS_noop, 3) < 0)
     *                            ^~~~~~~~~~~~~~~~
     */
    if (numitems < 1
    || (void *)items < (void *)0x8000)      // twice as big as our largest enum
        return -1;
    for (i = 0; i < numitems; i++) {
        // a pids_item is currently unsigned, but we'll protect our future
        if (items[i] < 0)
            return -1;
        if (items[i] >= PROCPS_PIDS_logical_end) {
            return -1;
        }
    }
    return 0;
} // end: items_check_failed


static inline void libflags_set (
        struct procps_pidsinfo *info)
{
    enum pids_item e;
    int i, n;

    memset (info->ref_counts, 0, sizeof(info->ref_counts));
    info->oldflags = info->history_yes = 0;
    for (i = 0; i < info->curitems; i++) {
        if (((e = info->items[i])) >= PROCPS_PIDS_logical_end)
            break;
        info->oldflags |= Item_table[e].oldflags;
        info->history_yes |= Item_table[e].needhist;
        n = Item_table[e].refcount;
        if (n > -1) ++info->ref_counts[n];
    }
    if (info->oldflags & f_either) {
        if (!(info->oldflags & f_stat))
            info->oldflags |= f_status;
    }
    return;
} // end: libflags_set


static inline void oldproc_close (
        PROCTAB **this)
{
    if (*this != NULL) {
        closeproc(*this);
        *this = NULL;
    }
} // end: oldproc_close


static inline int oldproc_open (
        PROCTAB **this,
        unsigned flags,
        ...)
{

    va_list vl;
    int *ids;
    int num = 0;

    if (*this == NULL) {
        va_start(vl, flags);
        ids = va_arg(vl, int*);
        if (flags & PROC_UID) num = va_arg(vl, int);
        va_end(vl);
        if (NULL == (*this = openproc(flags, ids, num)))
            return 0;
    }

    return 1;
} // end: oldproc_open


static inline int proc_tally (
        struct procps_pidsinfo *info,
        struct pids_counts *counts,
        proc_t *p)
{
    switch (p->state) {
        case 'R':
            ++counts->running;
            break;
        case 'S':
        case 'D':
            ++counts->sleeping;
            break;
        case 'T':
            ++counts->stopped;
            break;
        case 'Z':
            ++counts->zombied;
            break;
        default:                // keep gcc happy
            break;
    }
    ++counts->total;

    if (info->history_yes)
        return !make_hist(info, p);
    return 1;
} // end: proc_tally


/*
 * stacks_alloc():
 *
 * Allocate and initialize one or more stacks each of which is anchored in an
 * associated pids_stack structure.
 *
 * All such stacks will will have their result structures properly primed with
 * 'items', while the result itself will be zeroed.
 *
 * Returns an array of pointers representing the 'heads' of each new stack.
 */
static struct stacks_extent *stacks_alloc (
        struct procps_pidsinfo *info,
        int maxstacks)
{
    struct stacks_extent *p_blob;
    struct pids_stack **p_vect;
    struct pids_stack *p_head;
    size_t vect_size, head_size, list_size, blob_size;
    void *v_head, *v_list;
    int i;

    if (info == NULL || info->items == NULL)
        return NULL;
    if (maxstacks < 1)
        return NULL;

    vect_size  = sizeof(void *) * maxstacks;                   // size of the addr vectors |
    vect_size += sizeof(void *);                               // plus NULL addr delimiter |
    head_size  = sizeof(struct pids_stack);                    // size of that head struct |
    list_size  = sizeof(struct pids_result) * info->maxitems;  // any single results stack |
    blob_size  = sizeof(struct stacks_extent);                 // the extent anchor itself |
    blob_size += vect_size;                                    // plus room for addr vects |
    blob_size += head_size * maxstacks;                        // plus room for head thing |
    blob_size += list_size * maxstacks;                        // plus room for our stacks |

    /* note: all of our memory is allocated in a single blob, facilitating a later free(). |
             as a minimum, it is important that the result structures themselves always be |
             contiguous for every stack since they are accessed through relative position. | */
    if (NULL == (p_blob = calloc(1, blob_size)))
        return NULL;

    p_blob->next = info->extents;                              // push this extent onto... |
    info->extents = p_blob;                                    // ...some existing extents |
    p_vect = (void *)p_blob + sizeof(struct stacks_extent);    // prime our vector pointer |
    p_blob->stacks = p_vect;                                   // set actual vectors start |
    v_head = (void *)p_vect + vect_size;                       // prime head pointer start |
    v_list = v_head + (head_size * maxstacks);                 // prime our stacks pointer |

    for (i = 0; i < maxstacks; i++) {
        p_head = (struct pids_stack *)v_head;
        p_head->head = itemize_stack((struct pids_result *)v_list, info->curitems, info->items);
        p_blob->stacks[i] = p_head;
        v_list += list_size;
        v_head += head_size;
    }
    p_blob->ext_numstacks = maxstacks;
    return p_blob;
} // end: stacks_alloc


static int stacks_fetch (
        struct procps_pidsinfo *info)
{
 #define n_alloc  info->fetch.n_alloc
 #define n_inuse  info->fetch.n_inuse
 #define n_saved  info->fetch.n_alloc_save
    static proc_t task;    // static for initial zeroes + later dynamic free(s)
    struct stacks_extent *ext;

    // initialize stuff -----------------------------------
    if (!info->fetch.anchor) {
        if (!(info->fetch.anchor = calloc(sizeof(void *), MEMORY_INCR)))
            return -ENOMEM;
        n_alloc = MEMORY_INCR;
    }
    if (!info->extents) {
        if (!(ext = stacks_alloc(info, n_alloc)))
            return -ENOMEM;
        memset(info->fetch.anchor, 0, sizeof(void *) * n_alloc);
        memcpy(info->fetch.anchor, ext->stacks, sizeof(void *) * n_alloc);
        itemize_stacks_all(info);
    }
    cleanup_stacks_all(info);
    toggle_history(info);
    memset(&info->fetch.summary.counts, 0, sizeof(struct pids_counts));

    // iterate stuff --------------------------------------
    n_inuse = 0;
    while (info->read_something(info->PT, &task)) {
        if (!(n_inuse < n_alloc)) {
            n_alloc += MEMORY_INCR;
            if ((!(info->fetch.anchor = realloc(info->fetch.anchor, sizeof(void *) * n_alloc)))
            || (!(ext = stacks_alloc(info, MEMORY_INCR))))
                return -1;
            memcpy(info->fetch.anchor + n_inuse, ext->stacks, sizeof(void *) * MEMORY_INCR);
        }
        if (!proc_tally(info, &info->fetch.summary.counts, &task))
            return -1;
        assign_results(info, info->fetch.anchor[n_inuse++], &task);
    }

    // finalize stuff -------------------------------------
    if (n_saved < n_alloc + 1) {
        n_saved = n_alloc + 1;
        if (!(info->fetch.summary.stacks = realloc(info->fetch.summary.stacks, sizeof(void *) * n_saved)))
            return -1;
    }
    memcpy(info->fetch.summary.stacks, info->fetch.anchor, sizeof(void *) * n_inuse);
    info->fetch.summary.stacks[n_inuse] = NULL;
    return n_inuse;     // callers beware, this might be zero !
 #undef n_alloc
 #undef n_inuse
 #undef n_saved
} // end: stacks_fetch


// ___ Public Functions |||||||||||||||||||||||||||||||||||||||||||||||||||||||

// --- standard required functions --------------------------------------------

/*
 * procps_pids_new():
 *
 * @info: location of returned new structure
 *
 * Returns: 0 on success <0 on failure
 */
PROCPS_EXPORT int procps_pids_new (
        struct procps_pidsinfo **info,
        enum pids_item *items,
        int numitems)
{
    struct procps_pidsinfo *p;
    double uptime_secs;
    int pgsz;

    if (info == NULL || *info != NULL)
        return -EINVAL;

    if (!(p = calloc(1, sizeof(struct procps_pidsinfo))))
        return -ENOMEM;

    /* if we're without items or numitems, a later call to
       procps_pids_reset() will become mandatory */
    if (items && numitems) {
        if (items_check_failed(numitems, items)) {
            free(p);
            return -EINVAL;
        }
        // allow for our PROCPS_PIDS_logical_end
        p->maxitems = numitems + 1;
        if (!(p->items = calloc(p->maxitems, sizeof(enum pids_item)))) {
            free(p);
            return -ENOMEM;
        }
        memcpy(p->items, items, sizeof(enum pids_item) * numitems);
        p->items[numitems] = PROCPS_PIDS_logical_end;
        p->curitems = p->maxitems;
        libflags_set(p);
    }

    if (!(p->hist = calloc(MEMORY_INCR, sizeof(struct history_info)))) {
        free(p->items);
        free(p);
        return -ENOMEM;
    }
    config_history(p);

    pgsz = getpagesize();
    while (pgsz > 1024) { pgsz >>= 1; p->pgs2k_shift++; }

    p->hertz = procps_hertz_get();
    procps_uptime(&uptime_secs, NULL);
    p->boot_seconds = uptime_secs;

    p->refcount = 1;
    *info = p;
    return 0;
} // end: procps_pids_new


PROCPS_EXPORT int procps_pids_ref (
        struct procps_pidsinfo *info)
{
    if (info == NULL)
        return -EINVAL;

    info->refcount++;
    return info->refcount;
} // end: procps_pids_ref


PROCPS_EXPORT int procps_pids_unref (
        struct procps_pidsinfo **info)
{
    if (info == NULL || *info == NULL)
        return -EINVAL;

    (*info)->refcount--;
    if ((*info)->refcount == 0) {
#ifdef UNREF_RPTHASH
        unref_rpthash(*info);
#endif
        if ((*info)->extents) {
            cleanup_stacks_all(*info);
            do {
                struct stacks_extent *p = (*info)->extents;
                (*info)->extents = (*info)->extents->next;
                free(p);
            } while ((*info)->extents);
        }
        if ((*info)->otherexts) {
            struct stacks_extent *nextext, *ext = (*info)->otherexts;
            while (ext) {
                nextext = ext->next;
                cleanup_stack(ext->stacks[0]->head);
                free(ext);
                ext = nextext;
            };
        }
        if ((*info)->fetch.anchor)
            free((*info)->fetch.anchor);
        if ((*info)->fetch.summary.stacks)
            free((*info)->fetch.summary.stacks);

        if ((*info)->items)
            free((*info)->items);
        if ((*info)->hist) {
            free((*info)->hist->PHist_sav);
            free((*info)->hist->PHist_new);
            free((*info)->hist);
        }

        free(*info);
        *info = NULL;
        return 0;
    }
    return (*info)->refcount;
} // end: procps_pids_unref


// --- variable interface functions -------------------------------------------

PROCPS_EXPORT struct pids_stack *fatal_proc_unmounted (
        struct procps_pidsinfo *info,
        int return_self)
{
    static proc_t self;
    struct stacks_extent *ext;

    // this is very likely the *only* newlib function where the
    // context (procps_pidsinfo) of NULL will ever be permitted
    look_up_our_self(&self);
    if (!return_self)
        return NULL;

    if (info == NULL
    || !(ext = stacks_alloc(info, 1))
    || !extent_cut(info, ext))
        return NULL;

    ext->next = info->otherexts;
    info->otherexts = ext;
    assign_results(info, ext->stacks[0], &self);

    return ext->stacks[0];
} // end: fatal_proc_unmounted


PROCPS_EXPORT struct pids_stack *procps_pids_get (
        struct procps_pidsinfo *info,
        enum pids_fetch_type which)
{
    static proc_t task;    // static for initial zeroes + later dynamic free(s)

    if (info == NULL)
        return NULL;
    if (!info->curitems)
        return NULL;
    if (which != PROCPS_FETCH_TASKS_ONLY && which != PROCPS_FETCH_THREADS_TOO)
        return NULL;

fresh_start:
    if (!info->get_ext) {
        if (!(info->get_ext = stacks_alloc(info, 1)))
            return NULL;
        if (!oldproc_open(&info->get_PT, info->oldflags))
            return NULL;
        info->get_type = which;
        info->read_something = which ? readeither : readproc;
    }

    if (info->get_type != which) {
        oldproc_close(&info->get_PT);
        cleanup_stack(info->get_ext->stacks[0]->head);
        if (extent_cut(info, info->get_ext))
            free(info->get_ext);
        info->get_ext = NULL;
        goto fresh_start;
    }

    cleanup_stack(info->get_ext->stacks[0]->head);

    if (NULL == info->read_something(info->get_PT, &task))
        return NULL;
    assign_results(info, info->get_ext->stacks[0], &task);

    return info->get_ext->stacks[0];
} // end: procps_pids_get


/* procps_pids_reap():
 *
 * Harvest all the available tasks/threads and provide the result
 * stacks along with a summary of the information gathered.
 *
 * Returns: pointer to a pids_fetch struct on success, NULL on error.
 */
PROCPS_EXPORT struct pids_fetch *procps_pids_reap (
        struct procps_pidsinfo *info,
        enum pids_fetch_type which)
{
    int rc;

    if (info == NULL)
        return NULL;
    if (!info->curitems)
        return NULL;
    if (which != PROCPS_FETCH_TASKS_ONLY && which != PROCPS_FETCH_THREADS_TOO)
        return NULL;

    if (!oldproc_open(&info->PT, info->oldflags))
        return NULL;
    info->read_something = which ? readeither : readproc;

    rc = stacks_fetch(info);

    oldproc_close(&info->PT);
    // we better have found at least 1 pid
    return (rc > 0) ? &info->fetch.summary : NULL;
} // end: procps_pids_reap


PROCPS_EXPORT int procps_pids_reset (
        struct procps_pidsinfo *info,
        enum pids_item *newitems,
        int newnumitems)
{
    if (info == NULL || newitems == NULL)
        return -EINVAL;
    if (items_check_failed(newnumitems, newitems))
        return -EINVAL;

    /* shame on this caller, they didn't change anything. and unless they have
       altered the depth of the stacks we're not gonna change anything either! */
    if (info->curitems == newnumitems + 1
    && !memcmp(info->items, newitems, sizeof(enum pids_item) * newnumitems))
        return 0;

    if (info->maxitems < newnumitems + 1) {
        if (info->dirty_stacks)
            cleanup_stacks_all(info);
        // allow for our PROCPS_PIDS_logical_end
        info->maxitems = newnumitems + 1;
        if (!(info->items = realloc(info->items, sizeof(enum pids_item) * info->maxitems)))
            return -ENOMEM;
        extents_free_all(info);
    }

    if (info->dirty_stacks)
        cleanup_stacks_all(info);

    memcpy(info->items, newitems, sizeof(enum pids_item) * newnumitems);
    info->items[newnumitems] = PROCPS_PIDS_logical_end;
    // account for above PROCPS_PIDS_logical_end
    info->curitems = newnumitems + 1;

    itemize_stacks_all(info);
    libflags_set(info);

    return 0;
} // end: procps_pids_reset


/* procps_pids_select():
 *
 * Harvest any processes matching the specified PID or UID and provide the
 * result stacks along with a summary of the information gathered.
 *
 * Returns: pointer to a pids_fetch struct on success, NULL on error.
 */
PROCPS_EXPORT struct pids_fetch *procps_pids_select (
        struct procps_pidsinfo *info,
        unsigned *these,
        int numthese,
        enum pids_select_type which)
{
    unsigned ids[FILL_ID_MAX + 1];
    int rc;

    if (info == NULL || these == NULL)
        return NULL;
    if (numthese < 1 || numthese > FILL_ID_MAX)
        return NULL;
    if (which != PROCPS_SELECT_PID && which != PROCPS_SELECT_UID)
        return NULL;

    // this zero delimiter is really only needed with PROCPS_SELECT_PID
    memcpy(ids, these, sizeof(unsigned) * numthese);
    ids[numthese] = 0;

    if (!oldproc_open(&info->PT, (info->oldflags | which), ids, numthese))
        return NULL;
    info->read_something = readproc;

    rc = stacks_fetch(info);

    oldproc_close(&info->PT);
    // no guarantee any pids/uids were found
    return (rc > -1) ? &info->fetch.summary : NULL;
} // end: procps_pids_select


/*
 * procps_pids_sort():
 *
 * Sort stacks anchored in the passed pids_stack pointers array
 * based on the designated sort enumerator and specified order.
 *
 * Returns those same addresses sorted.
 *
 * Note: all of the stacks must be homogeneous (of equal length and content).
 */
PROCPS_EXPORT struct pids_stack **procps_pids_sort (
        struct procps_pidsinfo *info,
        struct pids_stack *stacks[],
        int numstacked,
        enum pids_item sortitem,
        enum pids_sort_order order)
{
    struct sort_parms parms;
    struct pids_result *p;
    int offset;

    if (info == NULL || stacks == NULL)
        return NULL;
    // a pids_item is currently unsigned, but we'll protect our future
    if (sortitem < 0  || sortitem >= PROCPS_PIDS_logical_end)
        return NULL;
    if (order != PROCPS_SORT_ASCEND && order != PROCPS_SORT_DESCEND)
        return NULL;
    if (numstacked < 2)
        return stacks;

    offset = 0;
    p = stacks[0]->head;
    for (;;) {
        if (p->item == sortitem)
            break;
        ++offset;
        if (offset >= info->curitems)
            return NULL;
        if (p->item >= PROCPS_PIDS_logical_end)
            return NULL;
        ++p;
    }
    parms.offset = offset;
    parms.order = order;

    qsort_r(stacks, numstacked, sizeof(void *), (QSR_t)Item_table[p->item].sortfunc, &parms);
    return stacks;
} // end: procps_pids_sort
