/*
 * New Interface to Process Table -- PROCTAB Stream (a la Directory streams)
 * Copyright (C) 1996 Charles L. Blake.
 * Copyright (C) 1998 Michael K. Johnson
 * Copyright 1998-2003 Albert Cahalan
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

#include "version.h"
#include "readproc.h"
#include "escape.h"
#include "pwcache.h"
#include "devname.h"
#include "procps.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef WITH_SYSTEMD
#include <systemd/sd-login.h>
#endif
#include <proc/namespace.h>

// sometimes it's easier to do this manually, w/o gcc helping
#ifdef PROF
extern void __cyg_profile_func_enter(void*,void*);
#define ENTER(x) __cyg_profile_func_enter((void*)x,(void*)x)
#define LEAVE(x) __cyg_profile_func_exit((void*)x,(void*)x)
#else
#define ENTER(x)
#define LEAVE(x)
#endif

#ifdef FALSE_THREADS
// used when multi-threaded and some memory must not be freed
#define MK_THREAD(q)   q->pad_1 =  '\xee'
#define IS_THREAD(q) ( q->pad_1 == '\xee' )
#endif

// utility buffers of MAX_BUFSZ bytes each, available to
// any function following an openproc() call
static char *src_buffer,
            *dst_buffer;
#define MAX_BUFSZ 1024*64*2

// dynamic 'utility' buffer support for file2str() calls
struct utlbuf_s {
    char *buf;     // dynamically grown buffer
    int   siz;     // current len of the above
} utlbuf_s;

#ifndef SIGNAL_STRING
// convert hex string to unsigned long long
static unsigned long long unhex(const char *restrict cp){
    unsigned long long ull = 0;
    for(;;){
        char c = *cp++;
        if(!( (c >= '0' && c <= '9') ||
              (c >= 'A' && c <= 'F') ||
              (c >= 'a' && c <= 'f') )) break;
        ull = (ull<<4) | (c - (c >= 'a' ? 'a'-10 : c >= 'A' ? 'A'-10 : '0'));
    }
    return ull;
}
#endif

static int task_dir_missing;

// free any additional dynamically acquired storage associated with a proc_t
// ( and if it's to be reused, refresh it otherwise destroy it )
static inline void free_acquired (proc_t *p, int reuse) {
    if (p->environ)   free((void*)p->environ);
    if (p->cmdline)   free((void*)p->cmdline);
    if (p->cgname)    free((void*)p->cgname);
    if (p->cgroup)    free((void*)p->cgroup);
    if (p->environ_v) free((void*)*p->environ_v);
    if (p->cmdline_v) free((void*)*p->cmdline_v);
    if (p->cgroup_v)  free((void*)*p->cgroup_v);
    if (p->supgid)    free(p->supgid);
    if (p->supgrp)    free(p->supgrp);
    if (p->cmd)       free(p->cmd);
    if (p->sd_mach)   free(p->sd_mach);
    if (p->sd_ouid)   free(p->sd_ouid);
    if (p->sd_seat)   free(p->sd_seat);
    if (p->sd_sess)   free(p->sd_sess);
    if (p->sd_slice)  free(p->sd_slice);
    if (p->sd_unit)   free(p->sd_unit);
    if (p->sd_uunit)  free(p->sd_uunit);

    memset(p, reuse ? '\0' : '\xff', sizeof(*p));
}

///////////////////////////////////////////////////////////////////////////

typedef struct status_table_struct {
    unsigned char name[8];        // /proc/*/status field name
    unsigned char len;            // name length
#ifdef LABEL_OFFSET
    long offset;                  // jump address offset
#else
    void *addr;
#endif
} status_table_struct;

#ifdef LABEL_OFFSET
#define F(x) {#x, sizeof(#x)-1, (long)(&&case_##x-&&base)},
#else
#define F(x) {#x, sizeof(#x)-1, &&case_##x},
#endif
#define NUL  {"", 0, 0},

#define GPERF_TABLE_SIZE 128

// Derived from:
// gperf -7 --language=ANSI-C --key-positions=1,3,4 -C -n -c <if-not-piped>
// ( --key-positions verified by omission & reported "Computed positions" )
//
// Suggested method:
// Grep this file for "case_", then strip those down to the name.
// Eliminate duplicates (due to #ifs), the '    case_' prefix and
// any c comments.  Leave the colon and newline so that "Pid:\n",
// "Threads:\n", etc. would be lines, but no quote, no escape, etc.
//
// After a pipe through gperf, insert the resulting 'asso_values'
// into our 'asso' array.  Then convert the gperf 'wordlist' array
// into our 'table' array by wrapping the string literals within
// the F macro and replacing empty strings with the NUL define.
//
// In the status_table_struct watch out for name size (grrr, expanding)
// and the number of entries. Currently, the table is padded to 128
// entries and we therefore mask with 127.

static int status2proc (char *S, proc_t *restrict P, int is_proc) {
    long Threads = 0;
    long Tgid = 0;
    long Pid = 0;

  // 128 entries because we trust the kernel to use ASCII names
  static const unsigned char asso[] =
    {
      101, 101, 101, 101, 101, 101, 101, 101, 101, 101,
      101, 101, 101, 101, 101, 101, 101, 101, 101, 101,
      101, 101, 101, 101, 101, 101, 101, 101, 101, 101,
      101, 101, 101, 101, 101, 101, 101, 101, 101, 101,
      101, 101, 101, 101, 101, 101, 101, 101, 101, 101,
      101, 101, 101, 101, 101, 101, 101, 101,   6, 101,
      101, 101, 101, 101, 101,  45,  55,  25,  31,  50,
       50,  10,   0,  35, 101, 101,  21, 101,  30, 101,
       20,  36,   0,   5,   0,  40,   0,   0, 101, 101,
      101, 101, 101, 101, 101, 101, 101,  30, 101,  15,
        0,   1, 101,  10, 101,  10, 101, 101, 101,  25,
      101,  40,   0, 101,   0,  50,   6,  40, 101,   1,
       35, 101, 101, 101, 101, 101, 101, 101
    };

    static const status_table_struct table[GPERF_TABLE_SIZE] = {
      F(VmHWM)
      F(Threads)
      NUL NUL NUL
      F(VmRSS)
      F(VmSwap)
      NUL NUL NUL
      F(Tgid)
      F(VmStk)
      NUL NUL NUL
      F(VmSize)
      F(Gid)
      NUL NUL NUL
      F(VmPTE)
      F(VmPeak)
      NUL NUL NUL
      F(ShdPnd)
      F(Pid)
      NUL NUL NUL
      F(PPid)
      F(VmLib)
      NUL NUL NUL
      F(SigPnd)
      F(VmLck)
      NUL NUL NUL
      F(SigCgt)
      F(State)
      NUL NUL NUL
      F(CapPrm)
      F(Uid)
      NUL NUL NUL
      F(SigIgn)
      F(SigQ)
      NUL NUL NUL
      F(RssShmem)
      F(Name)
      NUL NUL NUL
      F(CapInh)
      F(VmData)
      NUL NUL NUL
      F(FDSize)
      NUL NUL NUL NUL
      F(SigBlk)
      NUL NUL NUL NUL
      F(CapEff)
      NUL NUL NUL NUL
      F(CapBnd)
      NUL NUL NUL NUL
      F(VmExe)
      NUL NUL NUL NUL
      F(Groups)
      NUL NUL NUL NUL
      F(RssAnon)
      NUL NUL NUL NUL
      F(RssFile)
    };

#undef F
#undef NUL

ENTER(0x220);

    goto base;

    for(;;){
        char *colon;
        status_table_struct entry;

        // advance to next line
        S = strchr(S, '\n');
        if(!S) break;            // if no newline
        S++;

        // examine a field name (hash and compare)
    base:
        if(!*S) break;
        entry = table[(GPERF_TABLE_SIZE -1) & (asso[(int)S[3]] + asso[(int)S[2]] + asso[(int)S[0]])];
        colon = strchr(S, ':');
        if(!colon) break;
        if(colon[1]!='\t') break;
        if(colon-S != entry.len) continue;
        if(memcmp(entry.name,S,colon-S)) continue;

        S = colon+2; // past the '\t'

#ifdef LABEL_OFFSET
        goto *(&&base + entry.offset);
#else
        goto *entry.addr;
#endif

    case_Name:
    {   char buf[16];
        unsigned u = 0;
        while(u < sizeof(buf) - 1u){
            int c = *S++;
            if(c=='\n') break;
            if(c=='\0') break;     // should never happen
            if(c=='\\'){
                c = *S++;
                if(c=='\n') break; // should never happen
                if(!c)      break; // should never happen
                if(c=='n') c='\n'; // else we assume it is '\\'
            }
            buf[u++] = c;
        }
        buf[u] = '\0';
        if (!P->cmd && !(P->cmd = strndup(buf, 15)))
            return 1;
        S--;   // put back the '\n' or '\0'
        continue;
    }
#ifdef SIGNAL_STRING
    case_ShdPnd:
        memcpy(P->signal, S, 16);
        P->signal[16] = '\0';
        continue;
    case_SigBlk:
        memcpy(P->blocked, S, 16);
        P->blocked[16] = '\0';
        continue;
    case_SigCgt:
        memcpy(P->sigcatch, S, 16);
        P->sigcatch[16] = '\0';
        continue;
    case_SigIgn:
        memcpy(P->sigignore, S, 16);
        P->sigignore[16] = '\0';
        continue;
    case_SigPnd:
        memcpy(P->_sigpnd, S, 16);
        P->_sigpnd[16] = '\0';
        continue;
#else
    case_ShdPnd:
        P->signal = unhex(S);
        continue;
    case_SigBlk:
        P->blocked = unhex(S);
        continue;
    case_SigCgt:
        P->sigcatch = unhex(S);
        continue;
    case_SigIgn:
        P->sigignore = unhex(S);
        continue;
    case_SigPnd:
        P->_sigpnd = unhex(S);
        continue;
#endif
    case_State:
        P->state = *S;
        continue;
    case_Tgid:
        Tgid = strtol(S,&S,10);
        continue;
    case_Pid:
        Pid = strtol(S,&S,10);
        continue;
    case_PPid:
        P->ppid = strtol(S,&S,10);
        continue;
    case_Threads:
        Threads = strtol(S,&S,10);
        continue;
    case_Uid:
        P->ruid = strtol(S,&S,10);
        P->euid = strtol(S,&S,10);
        P->suid = strtol(S,&S,10);
        P->fuid = strtol(S,&S,10);
        continue;
    case_Gid:
        P->rgid = strtol(S,&S,10);
        P->egid = strtol(S,&S,10);
        P->sgid = strtol(S,&S,10);
        P->fgid = strtol(S,&S,10);
        continue;
    case_VmData:
        P->vm_data = (unsigned long)strtol(S,&S,10);
        continue;
    case_VmExe:
        P->vm_exe = (unsigned long)strtol(S,&S,10);
        continue;
    case_VmLck:
        P->vm_lock = (unsigned long)strtol(S,&S,10);
        continue;
    case_VmLib:
        P->vm_lib = (unsigned long)strtol(S,&S,10);
        continue;
    case_VmRSS:
        P->vm_rss = (unsigned long)strtol(S,&S,10);
        continue;
    case_RssAnon:       // subset of VmRSS, linux-4.5
        P->vm_rss_anon = (unsigned long)strtol(S,&S,10);
        continue;
    case_RssFile:       // subset of VmRSS, linux-4.5
        P->vm_rss_file = (unsigned long)strtol(S,&S,10);
        continue;
    case_RssShmem:      // subset of VmRSS, linux-4.5
        P->vm_rss_shared = (unsigned long)strtol(S,&S,10);
        continue;
    case_VmSize:
        P->vm_size = (unsigned long)strtol(S,&S,10);
        continue;
    case_VmStk:
        P->vm_stack = (unsigned long)strtol(S,&S,10);
        continue;
    case_VmSwap: // Linux 2.6.34
        P->vm_swap = (unsigned long)strtol(S,&S,10);
        continue;
    case_Groups:
    {   char *nl = strchr(S, '\n');
        int j = nl ? (nl - S) : strlen(S);

#ifdef FALSE_THREADS
        if (j && !IS_THREAD(P)) {
#else
        if (j) {
#endif
            P->supgid = malloc(j+1);        // +1 in case space disappears
            if (!P->supgid)
                return 1;
            memcpy(P->supgid, S, j);
            if (' ' != P->supgid[--j]) ++j;
            P->supgid[j] = '\0';            // whack the space or the newline
            for ( ; j; j--)
                if (' '  == P->supgid[j])
                    P->supgid[j] = ',';
        }
        continue;
    }
    case_CapBnd:
    case_CapEff:
    case_CapInh:
    case_CapPrm:
    case_FDSize:
    case_SigQ:
    case_VmHWM: // 2005, peak VmRSS unless VmRSS is bigger
    case_VmPTE:
    case_VmPeak: // 2005, peak VmSize unless VmSize is bigger
        continue;
    }

#if 0
    // recent kernels supply per-tgid pending signals
    if(is_proc && *ShdPnd){
        memcpy(P->signal, ShdPnd, 16);
        P->signal[16] = '\0';
    }
#endif

    // recent kernels supply per-tgid pending signals
#ifdef SIGNAL_STRING
    if(!is_proc || !P->signal[0]){
        memcpy(P->signal, P->_sigpnd, 16);
        P->signal[16] = '\0';
    }
#else
    if(!is_proc){
        P->signal = P->_sigpnd;
    }
#endif

    // Linux 2.4.13-pre1 to max 2.4.xx have a useless "Tgid"
    // that is not initialized for built-in kernel tasks.
    // Only 2.6.0 and above have "Threads" (nlwp) info.

    if(Threads){
        P->nlwp = Threads;
        P->tgid = Tgid;     // the POSIX PID value
        P->tid  = Pid;      // the thread ID
    }else{
        P->nlwp = 1;
        P->tgid = Pid;
        P->tid  = Pid;
    }

#ifdef FALSE_THREADS
    if (!P->supgid && !IS_THREAD(P)) {
#else
    if (!P->supgid) {
#endif
        P->supgid = strdup("-");
        if (!P->supgid)
            return 1;
    }
LEAVE(0x220);
    return 0;
}
#undef GPERF_TABLE_SIZE

static int supgrps_from_supgids (proc_t *p) {
    char *g, *s;
    int t;

    if (!p->supgid || '-' == *p->supgid) {
        if (!(p->supgrp = strdup("-")))
            return 1;
        return 0;
    }
    s = p->supgid;
    t = 0;
    do {
        if (',' == *s) ++s;
        g = pwcache_get_group((uid_t)strtol(s, &s, 10));
        if (!(p->supgrp = realloc(p->supgrp, P_G_SZ+t+2)))
            return 1;
        t += snprintf(p->supgrp+t, P_G_SZ+2, "%s%s", t ? "," : "", g);
    } while (*s);

    return 0;
}

///////////////////////////////////////////////////////////////////////
static inline void oomscore2proc(const char* S, proc_t *restrict P)
{
    sscanf(S, "%d", &P->oom_score);
}

static inline void oomadj2proc(const char* S, proc_t *restrict P)
{
    sscanf(S, "%d", &P->oom_adj);
}
///////////////////////////////////////////////////////////////////////

static int sd2proc (proc_t *restrict p) {
#ifdef WITH_SYSTEMD
    char buf[64];
    uid_t uid;

    if (0 > sd_pid_get_machine_name(p->tid, &p->sd_mach)) {
        if (!(p->sd_mach = strdup("-")))
            return 1;
    }
    if (0 > sd_pid_get_owner_uid(p->tid, &uid)) {
        if (!(p->sd_ouid = strdup("-")))
            return 1;
    } else {
        snprintf(buf, sizeof(buf), "%d", (int)uid);
        if (!(p->sd_ouid = strdup(buf)))
            return 1;
    }
    if (0 > sd_pid_get_session(p->tid, &p->sd_sess)) {
        if (!(p->sd_sess = strdup("-")))
            return 1;
        if (!(p->sd_seat = strdup("-")))
            return 1;
    } else {
        if (0 > sd_session_get_seat(p->sd_sess, &p->sd_seat))
            if (!(p->sd_seat = strdup("-")))
                return 1;
    }
    if (0 > sd_pid_get_slice(p->tid, &p->sd_slice))
        if (!(p->sd_slice = strdup("-")))
            return 1;
    if (0 > sd_pid_get_unit(p->tid, &p->sd_unit))
        if (!(p->sd_unit = strdup("-")))
            return 1;
    if (0 > sd_pid_get_user_unit(p->tid, &p->sd_uunit))
        if (!(p->sd_uunit = strdup("-")))
            return 1;
#else
    if (!(p->sd_mach  = strdup("?")))
        return 1;
    if (!(p->sd_ouid  = strdup("?")))
        return 1;
    if (!(p->sd_seat  = strdup("?")))
        return 1;
    if (!(p->sd_sess  = strdup("?")))
        return 1;
    if (!(p->sd_slice = strdup("?")))
        return 1;
    if (!(p->sd_unit  = strdup("?")))
        return 1;
    if (!(p->sd_uunit = strdup("?")))
        return 1;
#endif
    return 0;
}
///////////////////////////////////////////////////////////////////////


// Reads /proc/*/stat files, being careful not to trip over processes with
// names like ":-) 1 2 3 4 5 6".
static int stat2proc (const char* S, proc_t *restrict P) {
    unsigned num;
    char* tmp;

ENTER(0x160);

    /* fill in default values for older kernels */
    P->processor = 0;
    P->rtprio = -1;
    P->sched = -1;
    P->nlwp = 0;

    S = strchr(S, '(') + 1;
    tmp = strrchr(S, ')');
    num = tmp - S;
    if(num >= 16) num = 15;
    if (!P->cmd && !(P->cmd = strndup(S, num)))
       return 1;
    S = tmp + 2;                 // skip ") "

    num = sscanf(S,
       "%c "                      // state
       "%d %d %d %d %d "          // ppid, pgrp, sid, tty_nr, tty_pgrp
       "%lu %lu %lu %lu %lu "     // flags, min_flt, cmin_flt, maj_flt, cmaj_flt
       "%llu %llu %llu %llu "     // utime, stime, cutime, cstime
       "%d %d "                   // priority, nice
       "%d "                      // num_threads
       "%lu "                     // 'alarm' == it_real_value (obsolete, always 0)
       "%llu "                    // start_time
       "%lu "                     // vsize
       "%lu "                     // rss
       "%lu %lu %lu %lu %lu %lu " // rsslim, start_code, end_code, start_stack, esp, eip
       "%*s %*s %*s %*s "         // pending, blocked, sigign, sigcatch                      <=== DISCARDED
       "%lu %*u %*u "             // 0 (former wchan), 0, 0                                  <=== Placeholders only
       "%d %d "                   // exit_signal, task_cpu
       "%d %d "                   // rt_priority, policy (sched)
       "%llu %llu %llu",          // blkio_ticks, gtime, cgtime
       &P->state,
       &P->ppid, &P->pgrp, &P->session, &P->tty, &P->tpgid,
       &P->flags, &P->min_flt, &P->cmin_flt, &P->maj_flt, &P->cmaj_flt,
       &P->utime, &P->stime, &P->cutime, &P->cstime,
       &P->priority, &P->nice,
       &P->nlwp,
       &P->alarm,
       &P->start_time,
       &P->vsize,
       &P->rss,
       &P->rss_rlim, &P->start_code, &P->end_code, &P->start_stack, &P->kstk_esp, &P->kstk_eip,
/*     P->signal, P->blocked, P->sigignore, P->sigcatch,   */ /* can't use */
       &P->wchan, /* &P->nswap, &P->cnswap, */  /* nswap and cnswap dead for 2.4.xx and up */
/* -- Linux 2.0.35 ends here -- */
       &P->exit_signal, &P->processor,  /* 2.2.1 ends with "exit_signal" */
/* -- Linux 2.2.8 to 2.5.17 end here -- */
       &P->rtprio, &P->sched,  /* both added to 2.5.18 */
       &P->blkio_tics, &P->gtime, &P->cgtime
    );

    if(!P->nlwp){
      P->nlwp = 1;
    }

    return 0;
LEAVE(0x160);
}

/////////////////////////////////////////////////////////////////////////

static void statm2proc(const char* s, proc_t *restrict P) {
    sscanf(s, "%lu %lu %lu %lu %lu %lu %lu",
           &P->size, &P->resident, &P->share,
           &P->trs, &P->lrs, &P->drs, &P->dt);
}

static int file2str(const char *directory, const char *what, struct utlbuf_s *ub) {
 #define buffGRW 1024
    char path[PROCPATHLEN];
    int fd, num, tot_read = 0;

    /* on first use we preallocate a buffer of minimum size to emulate
       former 'local static' behavior -- even if this read fails, that
       buffer will likely soon be used for another subdirectory anyway
       ( besides, with the calloc call we will never need use memcpy ) */
    if (ub->buf) ub->buf[0] = '\0';
    else {
        ub->buf = calloc(1, (ub->siz = buffGRW));
        if (!ub->buf) return -1;
    }
    sprintf(path, "%s/%s", directory, what);
    if (-1 == (fd = open(path, O_RDONLY, 0))) return -1;
    while (0 < (num = read(fd, ub->buf + tot_read, ub->siz - tot_read))) {
        tot_read += num;
        if (tot_read < ub->siz) break;
        if (!(ub->buf = realloc(ub->buf, (ub->siz += buffGRW)))) {
            close(fd);
            return -1;
        }
    };
    ub->buf[tot_read] = '\0';
    close(fd);
    if (tot_read < 1) return -1;
    return tot_read;
 #undef buffGRW
}

static char** file2strvec(const char* directory, const char* what) {
    char buf[2048];     /* read buf bytes at a time */
    char *p, *rbuf = 0, *endbuf, **q, **ret;
    int fd, tot = 0, n, c, end_of_file = 0;
    int align;

    sprintf(buf, "%s/%s", directory, what);
    fd = open(buf, O_RDONLY, 0);
    if(fd==-1)
        return NULL;

    /* read whole file into a memory buffer, allocating as we go */
    while ((n = read(fd, buf, sizeof buf - 1)) >= 0) {
        if (n < (int)(sizeof buf - 1))
            end_of_file = 1;
        if (n == 0 && rbuf == 0) {
            close(fd);
            return NULL;        /* process died between our open and read */
        }
        if (end_of_file && (n == 0 || buf[n-1]))/* last read char not null */
            buf[n++] = '\0';                    /* so append null-terminator */
        rbuf = realloc(rbuf, tot + n);          /* allocate more memory */
        if (!rbuf) return NULL;
        memcpy(rbuf + tot, buf, n);             /* copy buffer into it */
        tot += n;                               /* increment total byte ctr */
        if (end_of_file)
            break;
    }
    close(fd);
    if (n <= 0 && !end_of_file) {
        if (rbuf)
            free(rbuf);
        return NULL;            /* read error */
    }
    endbuf = rbuf + tot;                        /* count space for pointers */
    align = (sizeof(char*)-1) - ((tot + sizeof(char*)-1) & (sizeof(char*)-1));
    for (c = 0, p = rbuf; p < endbuf; p++) {
        if (!*p || *p == '\n')
            c += sizeof(char*);
        if (*p == '\n')
            *p = 0;
    }
    c += sizeof(char*);                         /* one extra for NULL term */

    rbuf = realloc(rbuf, tot + c + align);      /* make room for ptrs AT END */
    if (!rbuf) return NULL;
    endbuf = rbuf + tot;                        /* addr just past data buf */
    q = ret = (char**) (endbuf+align);          /* ==> free(*ret) to dealloc */
    *q++ = p = rbuf;                            /* point ptrs to the strings */
    endbuf--;                                   /* do not traverse final NUL */
    while (++p < endbuf)
        if (!*p)                                /* NUL char implies that */
            *q++ = p+1;                         /* next string -> next char */

    *q = 0;                                     /* null ptr list terminator */
    return ret;
}

    // this is the former under utilized 'read_cmdline', which has been
    // generalized in support of these new libproc flags:
    //     PROC_EDITCGRPCVT, PROC_EDITCMDLCVT and PROC_EDITENVRCVT
static int read_unvectored(char *restrict const dst, unsigned sz, const char* whom, const char *what, char sep) {
    char path[PROCPATHLEN];
    int fd;
    unsigned n = 0;

    snprintf(path, sizeof(path), "%s/%s", whom, what);
    fd = open(path, O_RDONLY);
    if(fd==-1) return 0;

    for(;;){
        ssize_t r = read(fd,dst+n,sz-n);
        if(r==-1){
            if(errno==EINTR) continue;
            break;
        }
        n += r;
        if(n==sz) {      // filled the buffer
            --n;         // make room for '\0'
            break;
        }
        if(r==0) break;  // EOF
    }
    close(fd);
    if(n){
        int i=n;
        while(i && dst[i-1]=='\0') --i; // skip trailing zeroes
        while(i--)
            if(dst[i]=='\n' || dst[i]=='\0') dst[i]=sep;
        if(dst[n-1]==' ') dst[n-1]='\0';
    }
    dst[n] = '\0';
    return n;
}

    // This routine reads a 'cgroup' for the designated proc_t and
    // guarantees the caller a valid proc_t.cgroup pointer.
static int fill_cgroup_cvt (const char* directory, proc_t *restrict p) {
 #define vMAX ( MAX_BUFSZ - (int)(dst - dst_buffer) )
    char *src, *dst, *grp, *eob, *name;
    int tot, x, whackable_int = MAX_BUFSZ;

    *(dst = dst_buffer) = '\0';                  // empty destination
    tot = read_unvectored(src_buffer, MAX_BUFSZ, directory, "cgroup", '\0');
    for (src = src_buffer, eob = src_buffer + tot; src < eob; src += x) {
        x = 1;                                   // loop assist
        if (!*src) continue;
        x = strlen((grp = src));
        if ('/' == grp[x - 1]) continue;         // skip empty root cgroups
#if 0
        grp += strspn(grp, "0123456789:");       // jump past group number
#endif
        dst += snprintf(dst, vMAX, "%s", (dst > dst_buffer) ? "," : "");
        dst += escape_str(dst, grp, vMAX, &whackable_int);
    }
    if (!(p->cgroup = strdup(dst_buffer[0] ? dst_buffer : "-")))
        return 1;
    name = strstr(p->cgroup, ":name=");
    if (name && *(name+6)) name += 6; else name = p->cgroup;
    if (!(p->cgname = strdup(name)))
        return 1;
    return 0;
 #undef vMAX
}

    // This routine reads a 'cmdline' for the designated proc_t, "escapes"
    // the result into a single string while guaranteeing the caller a
    // valid proc_t.cmdline pointer.
static int fill_cmdline_cvt (const char* directory, proc_t *restrict p) {
 #define uFLG ( ESC_BRACKETS | ESC_DEFUNCT )
    int whackable_int = MAX_BUFSZ;

    if (read_unvectored(src_buffer, MAX_BUFSZ, directory, "cmdline", ' '))
        escape_str(dst_buffer, src_buffer, MAX_BUFSZ, &whackable_int);
    else
        escape_command(dst_buffer, p, MAX_BUFSZ, &whackable_int, uFLG);
    p->cmdline = strdup(dst_buffer[0] ? dst_buffer : "?");
    if (!p->cmdline)
        return 1;
    return 0;
 #undef uFLG
}

    // This routine reads an 'environ' for the designated proc_t and
    // guarantees the caller a valid proc_t.environ pointer.
static int fill_environ_cvt (const char* directory, proc_t *restrict p) {
    int whackable_int = MAX_BUFSZ;

    dst_buffer[0] = '\0';
    if (read_unvectored(src_buffer, MAX_BUFSZ, directory, "environ", ' '))
        escape_str(dst_buffer, src_buffer, MAX_BUFSZ, &whackable_int);
    p->environ = strdup(dst_buffer[0] ? dst_buffer : "-");
    if (!p->environ)
        return 1;
    return 0;
}


    // Provide the means to value proc_t.lxcname (perhaps only with "-") while
    // tracking all names already seen thus avoiding the overhead of repeating
    // malloc() and free() calls.
static char *lxc_containers (const char *path) {
    static struct utlbuf_s ub = { NULL, 0 };   // util buffer for whole cgroup
    static char lxc_none[] = "-";
    static char lxc_oops[] = "?";              // used when memory alloc fails
    /*
       try to locate the lxc delimiter eyecatcher somewhere in a task's cgroup
       directory -- the following are from nested privileged plus unprivileged
       containers, where the '/lxc/' delimiter precedes the container name ...
           10:cpuset:/lxc/lxc-P/lxc/lxc-P-nested
           10:cpuset:/user.slice/user-1000.slice/session-c2.scope/lxc/lxc-U/lxc/lxc-U-nested

       ... some minor complications are the potential addition of more cgroups
       for a controller displacing the lxc name (normally last on a line), and
       environments with unexpected /proc/##/cgroup ordering/contents as with:
           10:cpuset:/lxc/lxc-P/lxc/lxc-P-nested/MY-NEW-CGROUP
       or
           2:name=systemd:/
           1:cpuset,cpu,cpuacct,devices,freezer,net_cls,blkio,perf_event,net_prio:/lxc/lxc-P
    */
    if (file2str(path, "cgroup", &ub) > 0) {
        static const char lxc_delm[] = "/lxc/";
        char *p1;

        if ((p1 = strstr(ub.buf, lxc_delm))) {
            static struct lxc_ele {
                struct lxc_ele *next;
                char *name;
            } *anchor = NULL;
            struct lxc_ele *ele = anchor;
            char *p2;

            if ((p2 = strchr(p1, '\n')))       // isolate a controller's line
                *p2 = '\0';
            do {                               // deal with nested containers
                p2 = p1 + (sizeof(lxc_delm)-1);
                p1 = strstr(p2, lxc_delm);
            } while (p1);
            if ((p1 = strchr(p2, '/')))        // isolate name only substring
                *p1 = '\0';
            while (ele) {                      // have we already seen a name
                if (!strcmp(ele->name, p2))
                    return ele->name;          // return just a recycled name
                ele = ele->next;
            }
            if (!(ele = (struct lxc_ele *)malloc(sizeof(struct lxc_ele))))
                return lxc_oops;
            if (!(ele->name = strdup(p2))) {
                free(ele);
                return lxc_oops;
            }
            ele->next = anchor;                // push the new container name
            anchor = ele;
            return ele->name;                  // return a new container name
        }
    }
    return lxc_none;
}


    // Provide the user id at login (or -1 if not available)
static int login_uid (const char *path) {
    char buf[PROCPATHLEN];
    int fd, id, in;

    id = -1;
    snprintf(buf, sizeof(buf), "%s/loginuid", path);
    if ((fd = open(buf, O_RDONLY, 0)) != -1) {
        in = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (in > 0) {
            buf[in] = '\0';
            id = atoi(buf);
        }
    }
    return id;
}
///////////////////////////////////////////////////////////////////////


/* These are some nice GNU C expression subscope "inline" functions.
 * The can be used with arbitrary types and evaluate their arguments
 * exactly once.
 */

/* Test if item X of type T is present in the 0 terminated list L */
#   define XinL(T, X, L) ( {                    \
            T  x = (X), *l = (L);               \
            while (*l && *l != x) l++;          \
            *l == x;                            \
        } )

/* Test if item X of type T is present in the list L of length N */
#   define XinLN(T, X, L, N) ( {                \
            T x = (X), *l = (L);                \
            int i = 0, n = (N);                 \
            while (i < n && l[i] != x) i++;     \
            i < n && l[i] == x;                 \
        } )

//////////////////////////////////////////////////////////////////////////////////
// This reads process info from /proc in the traditional way, for one process.
// The pid (tgid? tid?) is already in p, and a path to it in path, with some
// room to spare.
static proc_t* simple_readproc(PROCTAB *restrict const PT, proc_t *restrict const p) {
    static struct utlbuf_s ub = { NULL, 0 };    // buf for stat,statm,status
    static struct stat sb;     // stat() buffer
    char *restrict const path = PT->path;
    unsigned flags = PT->flags;
    int rc = 0;

    if (stat(path, &sb) == -1)                  /* no such dirent (anymore) */
        goto next_proc;

    if ((flags & PROC_UID) && !XinLN(uid_t, sb.st_uid, PT->uids, PT->nuid))
        goto next_proc;                 /* not one of the requested uids */

    p->euid = sb.st_uid;                        /* need a way to get real uid */
    p->egid = sb.st_gid;                        /* need a way to get real gid */

    if (flags & PROC_FILLSTAT) {                // read /proc/#/stat
        if (file2str(path, "stat", &ub) == -1)
            goto next_proc;
        rc += stat2proc(ub.buf, p);
    }

    if (flags & PROC_FILLMEM) {                 // read /proc/#/statm
        if (file2str(path, "statm", &ub) != -1)
            statm2proc(ub.buf, p);
    }

    if (flags & PROC_FILLSTATUS) {              // read /proc/#/status
        if (file2str(path, "status", &ub) != -1){
            rc += status2proc(ub.buf, p, 1);
            if (flags & PROC_FILLSUPGRP)
                rc += supgrps_from_supgids(p);
        }
    }

    // if multithreaded, some values are crap
    if(p->nlwp > 1){
      p->wchan = ~0ul;
    }

    /* some number->text resolving which is time consuming */
    /* ( names are cached, so memcpy to arrays was silly ) */
    if (flags & PROC_FILLUSR){
        p->euser = pwcache_get_user(p->euid);
        if(flags & PROC_FILLSTATUS) {
            p->ruser = pwcache_get_user(p->ruid);
            p->suser = pwcache_get_user(p->suid);
            p->fuser = pwcache_get_user(p->fuid);
        }
    }

    /* some number->text resolving which is time consuming */
    /* ( names are cached, so memcpy to arrays was silly ) */
    if (flags & PROC_FILLGRP){
        p->egroup = pwcache_get_group(p->egid);
        if(flags & PROC_FILLSTATUS) {
            p->rgroup = pwcache_get_group(p->rgid);
            p->sgroup = pwcache_get_group(p->sgid);
            p->fgroup = pwcache_get_group(p->fgid);
        }
    }

    if (flags & PROC_FILLENV)                   // read /proc/#/environ
        p->environ_v = file2strvec(path, "environ");
    if (flags & PROC_EDITENVRCVT)
        rc += fill_environ_cvt(path, p);

    if (flags & PROC_FILLARG)                   // read /proc/#/cmdline
        p->cmdline_v = file2strvec(path, "cmdline");
    if (flags & PROC_EDITCMDLCVT)
        rc += fill_cmdline_cvt(path, p);

    if ((flags & PROC_FILLCGROUP))              // read /proc/#/cgroup
        p->cgroup_v = file2strvec(path, "cgroup");
    if (flags & PROC_EDITCGRPCVT)
        rc += fill_cgroup_cvt(path, p);

    if (flags & PROC_FILLOOM) {
        if (file2str(path, "oom_score", &ub) != -1)
            oomscore2proc(ub.buf, p);
        if (file2str(path, "oom_score_adj", &ub) != -1)
            oomadj2proc(ub.buf, p);
    }

    if (flags & PROC_FILLNS)                    // read /proc/#/ns/*
        procps_ns_read_pid(p->tid, &(p->ns));


    if (flags & PROC_FILLSYSTEMD)               // get sd-login.h stuff
        rc += sd2proc(p);

    if (flags & PROC_FILL_LXC)                  // value the lxc name
        p->lxcname = lxc_containers(path);

    if (flags & PROC_FILL_LUID)                 // value the login user id
        p->luid = login_uid(path);

    if (rc == 0) return p;
    errno = ENOMEM;
next_proc:
    return NULL;
}

//////////////////////////////////////////////////////////////////////////////////
// This reads /proc/*/task/* data, for one task.
#ifdef FALSE_THREADS
// p is the POSIX process (task group summary) & source for some copies if !NULL
#else
// p is the POSIX process (task group summary) (not needed by THIS implementation)
#endif
// t is the POSIX thread (task group member, generally not the leader)
// path is a path to the task, with some room to spare.
static proc_t* simple_readtask(PROCTAB *restrict const PT, const proc_t *restrict const p, proc_t *restrict const t, char *restrict const path) {
    static struct utlbuf_s ub = { NULL, 0 };    // buf for stat,statm,status
    static struct stat sb;     // stat() buffer
    unsigned flags = PT->flags;
    int rc = 0;

    if (stat(path, &sb) == -1)                  /* no such dirent (anymore) */
        goto next_task;

//  if ((flags & PROC_UID) && !XinLN(uid_t, sb.st_uid, PT->uids, PT->nuid))
//      goto next_task;                         /* not one of the requested uids */

    t->euid = sb.st_uid;                        /* need a way to get real uid */
    t->egid = sb.st_gid;                        /* need a way to get real gid */

    if (flags & PROC_FILLSTAT) {                        // read /proc/#/task/#/stat
        if (file2str(path, "stat", &ub) == -1)
            goto next_task;
        rc += stat2proc(ub.buf, t);
    }

#ifdef FALSE_THREADS
    if (p)
        MK_THREAD(t);
#endif

    if (flags & PROC_FILLMEM) {                         // read /proc/#/task/#statm
        if (file2str(path, "statm", &ub) != -1)
            statm2proc(ub.buf, t);
    }
    if (flags & PROC_FILLSTATUS) {                      // read /proc/#/task/#/status
        if (file2str(path, "status", &ub) != -1) {
            rc += status2proc(ub.buf, t, 0);
#ifndef FALSE_THREADS
            if (flags & PROC_FILLSUPGRP)
                rc += supgrps_from_supgids(t);
#endif
        }
    }

    /* some number->text resolving which is time consuming */
    /* ( names are cached, so memcpy to arrays was silly ) */
    if (flags & PROC_FILLUSR){
        t->euser = pwcache_get_user(t->euid);
        if(flags & PROC_FILLSTATUS) {
            t->ruser = pwcache_get_user(t->ruid);
            t->suser = pwcache_get_user(t->suid);
            t->fuser = pwcache_get_user(t->fuid);
        }
    }

    /* some number->text resolving which is time consuming */
    /* ( names are cached, so memcpy to arrays was silly ) */
    if (flags & PROC_FILLGRP){
        t->egroup = pwcache_get_group(t->egid);
        if(flags & PROC_FILLSTATUS) {
            t->rgroup = pwcache_get_group(t->rgid);
            t->sgroup = pwcache_get_group(t->sgid);
            t->fgroup = pwcache_get_group(t->fgid);
        }
    }

#ifdef FALSE_THREADS
    if (!p) {
        if (flags & PROC_FILLSUPGRP)
            rc += supgrps_from_supgids(t);
#endif
        if (flags & PROC_FILLENV)                       // read /proc/#/task/#/environ
            t->environ_v = file2strvec(path, "environ");
        if (flags & PROC_EDITENVRCVT)
            rc += fill_environ_cvt(path, t);

        if (flags & PROC_FILLARG)                       // read /proc/#/task/#/cmdline
            t->cmdline_v = file2strvec(path, "cmdline");
        if (flags & PROC_EDITCMDLCVT)
            rc += fill_cmdline_cvt(path, t);

        if ((flags & PROC_FILLCGROUP))                  // read /proc/#/task/#/cgroup
            t->cgroup_v = file2strvec(path, "cgroup");
        if (flags & PROC_EDITCGRPCVT)
            rc += fill_cgroup_cvt(path, t);

        if (flags & PROC_FILLSYSTEMD)                   // get sd-login.h stuff
            rc += sd2proc(t);

#ifdef FALSE_THREADS
    } else {
        t->cmdline   = NULL;
        t->cmdline_v = NULL;
        t->environ   = NULL;
        t->environ_v = NULL;
        t->cgname    = NULL;
        t->cgroup    = NULL;
        t->cgroup_v  = NULL;
        t->supgid    = NULL;
        t->supgrp    = NULL;
        t->sd_mach   = NULL;
        t->sd_ouid   = NULL;
        t->sd_seat   = NULL;
        t->sd_sess   = NULL;
        t->sd_slice  = NULL;
        t->sd_unit   = NULL;
        t->sd_uunit  = NULL;
    }
#endif

    if (flags & PROC_FILLOOM) {
        if (file2str(path, "oom_score", &ub) != -1)
            oomscore2proc(ub.buf, t);
        if (file2str(path, "oom_score_adj", &ub) != -1)
            oomadj2proc(ub.buf, t);
    }

    if (flags & PROC_FILLNS)                            // read /proc/#/task/#/ns/*
        procps_ns_read_pid(t->tid, &(t->ns));

    if (flags & PROC_FILL_LXC)
        t->lxcname = lxc_containers(path);

    if (flags & PROC_FILL_LUID)
        t->luid = login_uid(path);

    if (rc == 0) return t;
    errno = ENOMEM;
next_task:
#ifndef FALSE_THREADS
    (void)p;
#endif
    return NULL;
}

//////////////////////////////////////////////////////////////////////////////////
// This finds processes in /proc in the traditional way.
// Return non-zero on success.
static int simple_nextpid(PROCTAB *restrict const PT, proc_t *restrict const p) {
  static struct dirent *ent;            /* dirent handle */
  char *restrict const path = PT->path;
  for (;;) {
    ent = readdir(PT->procfs);
    if(!ent || !ent->d_name[0]) return 0;
    if(*ent->d_name > '0' && *ent->d_name <= '9') break;
  }
  p->tgid = strtoul(ent->d_name, NULL, 10);
  p->tid = p->tgid;
  memcpy(path, "/proc/", 6);
  strcpy(path+6, ent->d_name);  // trust /proc to not contain evil top-level entries
  return 1;
}

//////////////////////////////////////////////////////////////////////////////////
// This finds tasks in /proc/*/task/ in the traditional way.
// Return non-zero on success.
static int simple_nexttid(PROCTAB *restrict const PT, const proc_t *restrict const p, proc_t *restrict const t, char *restrict const path) {
  static struct dirent *ent;            /* dirent handle */
  if(PT->taskdir_user != p->tgid){
    if(PT->taskdir){
      closedir(PT->taskdir);
    }
    // use "path" as some tmp space
    snprintf(path, PROCPATHLEN, "/proc/%d/task", p->tgid);
    PT->taskdir = opendir(path);
    if(!PT->taskdir) return 0;
    PT->taskdir_user = p->tgid;
  }
  for (;;) {
    ent = readdir(PT->taskdir);
    if(!ent || !ent->d_name[0]) return 0;
    if(*ent->d_name > '0' && *ent->d_name <= '9') break;
  }
  t->tid = strtoul(ent->d_name, NULL, 10);
  t->tgid = p->tgid;
//t->ppid = p->ppid;  // cover for kernel behavior? we want both actually...?
  snprintf(path, PROCPATHLEN, "/proc/%d/task/%.10s", p->tgid, ent->d_name);
  return 1;
}

//////////////////////////////////////////////////////////////////////////////////
// This "finds" processes in a list that was given to openproc().
// Return non-zero on success. (tgid was handy)
static int listed_nextpid(PROCTAB *restrict const PT, proc_t *restrict const p) {
  char *restrict const path = PT->path;
  pid_t tgid = *(PT->pids)++;
  if(tgid){
    snprintf(path, PROCPATHLEN, "/proc/%d", tgid);
    p->tgid = tgid;
    p->tid = tgid;  // they match for leaders
  }
  return tgid;
}

//////////////////////////////////////////////////////////////////////////////////
/* readproc: return a pointer to a proc_t filled with requested info about the
 * next process available matching the restriction set.  If no more such
 * processes are available, return a null pointer (boolean false).  Use the
 * passed buffer instead of allocating space if it is non-NULL.  */

/* This is optimized so that if a PID list is given, only those files are
 * searched for in /proc.  If other lists are given in addition to the PID list,
 * the same logic can follow through as for the no-PID list case.  This is
 * fairly complex, but it does try to not to do any unnecessary work.
 */
proc_t* readproc(PROCTAB *restrict const PT, proc_t *restrict p) {
  proc_t *ret;
  proc_t *saved_p;

  PT->did_fake=0;
//  if (PT->taskdir) {
//    closedir(PT->taskdir);
//    PT->taskdir = NULL;
//    PT->taskdir_user = -1;
//  }

  saved_p = p;
  if (p) free_acquired(p, 1);
  else {
    p = calloc(1, sizeof *p);
    if (!p) goto out;
  }
  for(;;){
    if (errno == ENOMEM) goto out;
    // fills in the path, plus p->tid and p->tgid
    if (!PT->finder(PT,p)) goto out;

    // go read the process data
    ret = PT->reader(PT,p);
    if(ret) return ret;
  }

out:
  if(!saved_p) free(p);
  // FIXME: maybe set tid to -1 here, for "-" in display?
  return NULL;
}


//////////////////////////////////////////////////////////////////////////////////
// readeither: return a pointer to a proc_t filled with requested info about
// the next unique process or task available.  If no more are available,
// return a null pointer (boolean false).  Use the passed buffer instead
// of allocating space if it is non-NULL.
proc_t* readeither (PROCTAB *restrict const PT, proc_t *restrict x) {
    static proc_t skel_p;    // skeleton proc_t, only uses tid + tgid
    static proc_t *new_p;    // for process/task transitions
    char path[PROCPATHLEN];
    proc_t *saved_x, *ret;

    saved_x = x;
    if (x) free_acquired(x,1);
    else {
        x = calloc(1, sizeof(*x));
        if (!x) goto end_procs;
    }
    if (new_p) goto next_task;

next_proc:
    new_p = NULL;
    for (;;) {
        if (errno == ENOMEM) goto end_procs;
        // fills in the PT->path, plus skel_p.tid and skel_p.tgid
        if (!PT->finder(PT,&skel_p)) goto end_procs;       // simple_nextpid
        if (!task_dir_missing) break;
        if ((ret = PT->reader(PT,x))) return ret;          // simple_readproc
    }

next_task:
    // fills in our path, plus x->tid and x->tgid
    if ((!(PT->taskfinder(PT,&skel_p,x,path)))             // simple_nexttid
    || (!(ret = PT->taskreader(PT,new_p,x,path)))) {       // simple_readtask
        goto next_proc;
    }
    if (!new_p) new_p = ret;
    return ret;

end_procs:
    if (!saved_x) free(x);
    return NULL;
}


//////////////////////////////////////////////////////////////////////////////////

// initiate a process table scan
PROCTAB* openproc(unsigned flags, ...) {
    va_list ap;
    struct stat sbuf;
    static int did_stat;
    PROCTAB* PT = malloc(sizeof(PROCTAB));

    if (!PT)
        return NULL;
    if (!did_stat){
        task_dir_missing = stat("/proc/self/task", &sbuf);
        did_stat = 1;
    }
    PT->taskdir = NULL;
    PT->taskdir_user = -1;
    PT->taskfinder = simple_nexttid;
    PT->taskreader = simple_readtask;

    PT->reader = simple_readproc;
    if (flags & PROC_PID){
        PT->procfs = NULL;
        PT->finder = listed_nextpid;
    }else{
        PT->procfs = opendir("/proc");
        if (!PT->procfs) { free(PT); return NULL; }
        PT->finder = simple_nextpid;
    }
    PT->flags = flags;

    va_start(ap, flags);
    if (flags & PROC_PID)
        PT->pids = va_arg(ap, pid_t*);
    else if (flags & PROC_UID){
        PT->uids = va_arg(ap, uid_t*);
        PT->nuid = va_arg(ap, int);
    }
    va_end(ap);

    if (!src_buffer
    && !(src_buffer = malloc(MAX_BUFSZ)))
        return NULL;
    if (!dst_buffer
    && !(dst_buffer = malloc(MAX_BUFSZ)))
        return NULL;

    return PT;
}

// terminate a process table scan
void closeproc(PROCTAB* PT) {
    if (PT){
        if (PT->procfs) closedir(PT->procfs);
        if (PT->taskdir) closedir(PT->taskdir);
        memset(PT,'#',sizeof(PROCTAB));
        free(PT);
    }
}


//////////////////////////////////////////////////////////////////////////////////
int look_up_our_self(proc_t *p) {
    struct utlbuf_s ub = { NULL, 0 };
    int rc = 0;

    if(file2str("/proc/self", "stat", &ub) == -1){
        fprintf(stderr, "Error, do this: mount -t proc proc /proc\n");
        _exit(47);
    }
    rc = stat2proc(ub.buf, p);  // parse /proc/self/stat
    free(ub.buf);
    return !rc;
}

#undef MK_THREAD
#undef IS_THREAD
#undef MAX_BUFSZ
