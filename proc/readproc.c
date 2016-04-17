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
#include "alloc.h"
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

#ifdef QUICK_THREADS
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
        if(c<0x30) break;
        ull = (ull<<4) | (c - (c>0x57) ? 0x57 : 0x30) ;
    }
    return ull;
}
#endif

static int task_dir_missing;

// free any additional dynamically acquired storage associated with a proc_t
// ( and if it's to be reused, refresh it otherwise destroy it )
static inline void free_acquired (proc_t *p, int reuse) {
#ifdef QUICK_THREADS
    if (!IS_THREAD(p)) {
#endif
        if (p->environ)  free((void*)*p->environ);
        if (p->cmdline)  free((void*)*p->cmdline);
        if (p->cgroup)   free((void*)*p->cgroup);
        if (p->supgid)   free(p->supgid);
        if (p->supgrp)   free(p->supgrp);
        if (p->cmd)      free(p->cmd);
        if (p->sd_mach)  free(p->sd_mach);
        if (p->sd_ouid)  free(p->sd_ouid);
        if (p->sd_seat)  free(p->sd_seat);
        if (p->sd_sess)  free(p->sd_sess);
        if (p->sd_slice) free(p->sd_slice);
        if (p->sd_unit)  free(p->sd_unit);
        if (p->sd_uunit) free(p->sd_uunit);
#ifdef QUICK_THREADS
    }
#endif
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

static void status2proc(char *S, proc_t *restrict P, int is_proc){
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
        if (!P->cmd)
            P->cmd = strndup(buf, 15);
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
        P->vm_data = strtol(S,&S,10);
        continue;
    case_VmExe:
        P->vm_exe = strtol(S,&S,10);
        continue;
    case_VmLck:
        P->vm_lock = strtol(S,&S,10);
        continue;
    case_VmLib:
        P->vm_lib = strtol(S,&S,10);
        continue;
    case_VmRSS:
        P->vm_rss = strtol(S,&S,10);
        continue;
    case_RssAnon:       // subset of VmRSS, linux-4.5
        P->vm_rss_anon = strtol(S,&S,10);
        continue;
    case_RssFile:       // subset of VmRSS, linux-4.5
        P->vm_rss_file = strtol(S,&S,10);
        continue;
    case_RssShmem:      // subset of VmRSS, linux-4.5
        P->vm_rss_shared = strtol(S,&S,10);
        continue;
    case_VmSize:
        P->vm_size = strtol(S,&S,10);
        continue;
    case_VmStk:
        P->vm_stack = strtol(S,&S,10);
        continue;
    case_VmSwap: // Linux 2.6.34
        P->vm_swap = strtol(S,&S,10);
        continue;
    case_Groups:
    {   char *nl = strchr(S, '\n');
        int j = nl ? (nl - S) : strlen(S);

        if (j) {
            P->supgid = xmalloc(j+1);       // +1 in case space disappears
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

    if (!P->supgid)
        P->supgid = xstrdup("-");

LEAVE(0x220);
}
#undef GPERF_TABLE_SIZE

static void supgrps_from_supgids (proc_t *p) {
    char *g, *s;
    int t;

    if (!p->supgid || '-' == *p->supgid) {
        p->supgrp = xstrdup("-");
        return;
    }
    s = p->supgid;
    t = 0;
    do {
        if (',' == *s) ++s;
        g = group_from_gid((uid_t)strtol(s, &s, 10));
        p->supgrp = xrealloc(p->supgrp, P_G_SZ+t+2);
        t += snprintf(p->supgrp+t, P_G_SZ+2, "%s%s", t ? "," : "", g);
    } while (*s);
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

#ifdef WITH_SYSTEMD
static void sd2proc(proc_t *restrict p) {
    char buf[64];
    uid_t uid;

    if (0 > sd_pid_get_machine_name(p->tid, &p->sd_mach))
        p->sd_mach = strdup("-");

    if (0 > sd_pid_get_owner_uid(p->tid, &uid))
        p->sd_ouid = strdup("-");
    else {
        snprintf(buf, sizeof(buf), "%d", (int)uid);
        p->sd_ouid = strdup(buf);
    }

    if (0 > sd_pid_get_session(p->tid, &p->sd_sess)) {
        p->sd_sess = strdup("-");
        p->sd_seat = strdup("-");
    } else {
        if (0 > sd_session_get_seat(p->sd_sess, &p->sd_seat))
            p->sd_seat = strdup("-");
    }

    if (0 > sd_pid_get_slice(p->tid, &p->sd_slice))
        p->sd_slice = strdup("-");

    if (0 > sd_pid_get_unit(p->tid, &p->sd_unit))
        p->sd_unit = strdup("-");

    if (0 > sd_pid_get_user_unit(p->tid, &p->sd_uunit))
        p->sd_uunit = strdup("-");
}
#endif
///////////////////////////////////////////////////////////////////////


// Reads /proc/*/stat files, being careful not to trip over processes with
// names like ":-) 1 2 3 4 5 6".
static void stat2proc(const char* S, proc_t *restrict P) {
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
    if (!P->cmd)
       P->cmd = strndup(S, num);
    S = tmp + 2;                 // skip ") "

    num = sscanf(S,
       "%c "
       "%d %d %d %d %d "
       "%lu %lu %lu %lu %lu "
       "%llu %llu %llu %llu "  /* utime stime cutime cstime */
       "%ld %ld "
       "%d "
       "%ld "
       "%llu "  /* start_time */
       "%lu "
       "%ld "
       "%lu %lu %lu %lu %lu %lu "
       "%*s %*s %*s %*s " /* discard, no RT signals & Linux 2.1 used hex */
       "%lu %*u %*u "
       "%d %d "
       "%lu %lu",
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
       &P->rtprio, &P->sched  /* both added to 2.5.18 */
    );

    if(!P->nlwp){
      P->nlwp = 1;
    }

LEAVE(0x160);
}

/////////////////////////////////////////////////////////////////////////

static void statm2proc(const char* s, proc_t *restrict P) {
    int num;
    num = sscanf(s, "%ld %ld %ld %ld %ld %ld %ld",
	   &P->size, &P->resident, &P->share,
	   &P->trs, &P->lrs, &P->drs, &P->dt);
/*    fprintf(stderr, "statm2proc converted %d fields.\n",num); */
}

static int file2str(const char *directory, const char *what, struct utlbuf_s *ub) {
 #define buffGRW 1024
    char path[PROCPATHLEN];
    int fd, num, tot_read = 0;

    /* on first use we preallocate a buffer of minimum size to emulate
       former 'local static' behavior -- even if this read fails, that
       buffer will likely soon be used for another subdirectory anyway
       ( besides, with this xcalloc we will never need to use memcpy ) */
    if (ub->buf) ub->buf[0] = '\0';
    else ub->buf = xcalloc((ub->siz = buffGRW));
    sprintf(path, "%s/%s", directory, what);
    if (-1 == (fd = open(path, O_RDONLY, 0))) return -1;
    while (0 < (num = read(fd, ub->buf + tot_read, ub->siz - tot_read))) {
        tot_read += num;
        if (tot_read < ub->siz) break;
        ub->buf = xrealloc(ub->buf, (ub->siz += buffGRW));
    };
    ub->buf[tot_read] = '\0';
    close(fd);
    if (tot_read < 1) return -1;
    return tot_read;
 #undef buffGRW
}

static char** file2strvec(const char* directory, const char* what) {
    char buf[2048];	/* read buf bytes at a time */
    char *p, *rbuf = 0, *endbuf, **q, **ret;
    int fd, tot = 0, n, c, end_of_file = 0;
    int align;

    sprintf(buf, "%s/%s", directory, what);
    fd = open(buf, O_RDONLY, 0);
    if(fd==-1) return NULL;

    /* read whole file into a memory buffer, allocating as we go */
    while ((n = read(fd, buf, sizeof buf - 1)) >= 0) {
	if (n < (int)(sizeof buf - 1))
	    end_of_file = 1;
	if (n == 0 && rbuf == 0) {
	    close(fd);
	    return NULL;	/* process died between our open and read */
	}
	if (n < 0) {
	    if (rbuf)
		free(rbuf);
	    close(fd);
	    return NULL;	/* read error */
	}
	if (end_of_file && (n == 0 || buf[n-1]))/* last read char not null */
	    buf[n++] = '\0';			/* so append null-terminator */
	rbuf = xrealloc(rbuf, tot + n);		/* allocate more memory */
	memcpy(rbuf + tot, buf, n);		/* copy buffer into it */
	tot += n;				/* increment total byte ctr */
	if (end_of_file)
	    break;
    }
    close(fd);
    if (n <= 0 && !end_of_file) {
	if (rbuf) free(rbuf);
	return NULL;		/* read error */
    }
    endbuf = rbuf + tot;			/* count space for pointers */
    align = (sizeof(char*)-1) - ((tot + sizeof(char*)-1) & (sizeof(char*)-1));
    for (c = 0, p = rbuf; p < endbuf; p++) {
	if (!*p || *p == '\n')
	    c += sizeof(char*);
	if (*p == '\n')
	    *p = 0;
    }
    c += sizeof(char*);				/* one extra for NULL term */

    rbuf = xrealloc(rbuf, tot + c + align);	/* make room for ptrs AT END */
    endbuf = rbuf + tot;			/* addr just past data buf */
    q = ret = (char**) (endbuf+align);		/* ==> free(*ret) to dealloc */
    *q++ = p = rbuf;				/* point ptrs to the strings */
    endbuf--;					/* do not traverse final NUL */
    while (++p < endbuf)
    	if (!*p)				/* NUL char implies that */
	    *q++ = p+1;				/* next string -> next char */

    *q = 0;					/* null ptr list terminator */
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

static char** vectorize_this_str (const char* src) {
 #define pSZ  (sizeof(char*))
    char *cpy, **vec;
    int adj, tot;

    tot = strlen(src) + 1;                       // prep for our vectors
    adj = (pSZ-1) - ((tot + pSZ-1) & (pSZ-1));   // calc alignment bytes
    cpy = xcalloc(tot + adj + (2 * pSZ));        // get new larger buffer
    snprintf(cpy, tot, "%s", src);               // duplicate their string
    vec = (char**)(cpy + tot + adj);             // prep pointer to pointers
    *vec = cpy;                                  // point 1st vector to string
    *(vec+1) = NULL;                             // null ptr 'list' delimit
    return vec;                                  // ==> free(*vec) to dealloc
 #undef pSZ
}

    // This routine reads a 'cgroup' for the designated proc_t.
    // It is similar to file2strvec except we filter and concatenate
    // the data into a single string represented as a single vector.
static void fill_cgroup_cvt (const char* directory, proc_t *restrict p) {
 #define vMAX ( MAX_BUFSZ - (int)(dst - dst_buffer) )
    char *src, *dst, *grp, *eob;
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
    p->cgroup = vectorize_this_str(dst_buffer[0] ? dst_buffer : "-");
 #undef vMAX
}

    // This routine reads a 'cmdline' for the designated proc_t, "escapes"
    // the result into a single string represented as a single vector
    // and guarantees the caller a valid proc_t.cmdline pointer.
static void fill_cmdline_cvt (const char* directory, proc_t *restrict p) {
 #define uFLG ( ESC_BRACKETS | ESC_DEFUNCT )
    int whackable_int = MAX_BUFSZ;

    if (read_unvectored(src_buffer, MAX_BUFSZ, directory, "cmdline", ' '))
        escape_str(dst_buffer, src_buffer, MAX_BUFSZ, &whackable_int);
    else
        escape_command(dst_buffer, p, MAX_BUFSZ, &whackable_int, uFLG);
    p->cmdline = vectorize_this_str(dst_buffer);
 #undef uFLG
}

    // This routine reads an 'environ' for the designated proc_t and
    // guarantees the caller a valid proc_t.environ pointer.
static void fill_environ_cvt (const char* directory, proc_t *restrict p) {
    int whackable_int = MAX_BUFSZ;

    dst_buffer[0] = '\0';
    if (read_unvectored(src_buffer, MAX_BUFSZ, directory, "environ", ' '))
        escape_str(dst_buffer, src_buffer, MAX_BUFSZ, &whackable_int);
    p->environ = vectorize_this_str(dst_buffer[0] ? dst_buffer : "-");
}


    // Provide the means to value proc_t.lxcname (perhaps only with "-") while
    // tracking all names already seen thus avoiding the overhead of repeating
    // malloc() and free() calls.
static const char *lxc_containers (const char *path) {
    static struct utlbuf_s ub = { NULL, 0 };   // util buffer for whole cgroup
    static char lxc_none[] = "-";
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
                const char *name;
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
            ele = (struct lxc_ele *)xmalloc(sizeof(struct lxc_ele));
            ele->name = xstrdup(p2);
            ele->next = anchor;                // push the new container name
            anchor = ele;
            return ele->name;                  // return a new container name
        }
    }
    return lxc_none;
}
///////////////////////////////////////////////////////////////////////


/* These are some nice GNU C expression subscope "inline" functions.
 * The can be used with arbitrary types and evaluate their arguments
 * exactly once.
 */

/* Test if item X of type T is present in the 0 terminated list L */
#   define XinL(T, X, L) ( {			\
	    T  x = (X), *l = (L);		\
	    while (*l && *l != x) l++;		\
	    *l == x;				\
	} )

/* Test if item X of type T is present in the list L of length N */
#   define XinLN(T, X, L, N) ( {		\
	    T x = (X), *l = (L);		\
	    int i = 0, n = (N);			\
	    while (i < n && l[i] != x) i++;	\
	    i < n && l[i] == x;			\
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

    if (stat(path, &sb) == -1)                  /* no such dirent (anymore) */
        goto next_proc;

    if ((flags & PROC_UID) && !XinLN(uid_t, sb.st_uid, PT->uids, PT->nuid))
        goto next_proc;                 /* not one of the requested uids */

    p->euid = sb.st_uid;                        /* need a way to get real uid */
    p->egid = sb.st_gid;                        /* need a way to get real gid */

    if (flags & PROC_FILLSTAT) {                // read /proc/#/stat
        if (file2str(path, "stat", &ub) == -1)
            goto next_proc;
        stat2proc(ub.buf, p);
    }

    if (flags & PROC_FILLMEM) {                 // read /proc/#/statm
        if (file2str(path, "statm", &ub) != -1)
            statm2proc(ub.buf, p);
    }

    if (flags & PROC_FILLSTATUS) {              // read /proc/#/status
        if (file2str(path, "status", &ub) != -1){
            status2proc(ub.buf, p, 1);
            if (flags & PROC_FILLSUPGRP)
                supgrps_from_supgids(p);
        }
    }

    // if multithreaded, some values are crap
    if(p->nlwp > 1){
      p->wchan = ~0ul;
    }

    /* some number->text resolving which is time consuming */
    /* ( names are cached, so memcpy to arrays was silly ) */
    if (flags & PROC_FILLUSR){
        p->euser = user_from_uid(p->euid);
        if(flags & PROC_FILLSTATUS) {
            p->ruser = user_from_uid(p->ruid);
            p->suser = user_from_uid(p->suid);
            p->fuser = user_from_uid(p->fuid);
        }
    }

    /* some number->text resolving which is time consuming */
    /* ( names are cached, so memcpy to arrays was silly ) */
    if (flags & PROC_FILLGRP){
        p->egroup = group_from_gid(p->egid);
        if(flags & PROC_FILLSTATUS) {
            p->rgroup = group_from_gid(p->rgid);
            p->sgroup = group_from_gid(p->sgid);
            p->fgroup = group_from_gid(p->fgid);
        }
    }

    if (flags & PROC_FILLENV) {                 // read /proc/#/environ
        if (flags & PROC_EDITENVRCVT)
            fill_environ_cvt(path, p);
        else
            p->environ = file2strvec(path, "environ");
    } else
        p->environ = NULL;

    if (flags & (PROC_FILLCOM|PROC_FILLARG)) {  // read /proc/#/cmdline
        if (flags & PROC_EDITCMDLCVT)
            fill_cmdline_cvt(path, p);
        else
            p->cmdline = file2strvec(path, "cmdline");
    } else
        p->cmdline = NULL;

    if ((flags & PROC_FILLCGROUP)) {            // read /proc/#/cgroup
        if (flags & PROC_EDITCGRPCVT)
            fill_cgroup_cvt(path, p);
        else
            p->cgroup = file2strvec(path, "cgroup");
    } else
        p->cgroup = NULL;

    if (flags & PROC_FILLOOM) {
        if (file2str(path, "oom_score", &ub) != -1)
            oomscore2proc(ub.buf, p);
        if (file2str(path, "oom_score_adj", &ub) != -1)
            oomadj2proc(ub.buf, p);
    }

    if (flags & PROC_FILLNS)                    // read /proc/#/ns/*
        procps_ns_read_pid(p->tid, &(p->ns));


#ifdef WITH_SYSTEMD
    if (flags & PROC_FILLSYSTEMD)               // get sd-login.h stuff
        sd2proc(p);
#endif

    if (flags & PROC_FILL_LXC)                  // value the lxc name
        p->lxcname = lxc_containers(path);

    return p;
next_proc:
    return NULL;
}

//////////////////////////////////////////////////////////////////////////////////
// This reads /proc/*/task/* data, for one task.
#ifdef QUICK_THREADS
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

    if (stat(path, &sb) == -1)                  /* no such dirent (anymore) */
        goto next_task;

//  if ((flags & PROC_UID) && !XinLN(uid_t, sb.st_uid, PT->uids, PT->nuid))
//      goto next_task;                         /* not one of the requested uids */

    t->euid = sb.st_uid;                        /* need a way to get real uid */
    t->egid = sb.st_gid;                        /* need a way to get real gid */

    if (flags & PROC_FILLSTAT) {                        // read /proc/#/task/#/stat
        if (file2str(path, "stat", &ub) == -1)
            goto next_task;
        stat2proc(ub.buf, t);
    }

#ifndef QUICK_THREADS
    if (flags & PROC_FILLMEM)                           // read /proc/#/task/#statm
        if (file2str(path, "statm", &ub) != -1)
            statm2proc(ub.buf, t);
#endif

    if (flags & PROC_FILLSTATUS) {                      // read /proc/#/task/#/status
        if (file2str(path, "status", &ub) != -1) {
            status2proc(ub.buf, t, 0);
#ifndef QUICK_THREADS
            if (flags & PROC_FILLSUPGRP)
                supgrps_from_supgids(t);
#endif
        }
    }

    /* some number->text resolving which is time consuming */
    /* ( names are cached, so memcpy to arrays was silly ) */
    if (flags & PROC_FILLUSR){
        t->euser = user_from_uid(t->euid);
        if(flags & PROC_FILLSTATUS) {
            t->ruser = user_from_uid(t->ruid);
            t->suser = user_from_uid(t->suid);
            t->fuser = user_from_uid(t->fuid);
        }
    }

    /* some number->text resolving which is time consuming */
    /* ( names are cached, so memcpy to arrays was silly ) */
    if (flags & PROC_FILLGRP){
        t->egroup = group_from_gid(t->egid);
        if(flags & PROC_FILLSTATUS) {
            t->rgroup = group_from_gid(t->rgid);
            t->sgroup = group_from_gid(t->sgid);
            t->fgroup = group_from_gid(t->fgid);
        }
    }

#ifdef QUICK_THREADS
    if (!p) {
        if (flags & PROC_FILLMEM)
            if (file2str(path, "statm", &ub) != -1)
                statm2proc(ub.buf, t);

        if (flags & PROC_FILLSUPGRP)
            supgrps_from_supgids(t);
#endif
        if (flags & PROC_FILLENV) {                     // read /proc/#/task/#/environ
            if (flags & PROC_EDITENVRCVT)
                fill_environ_cvt(path, t);
            else
                t->environ = file2strvec(path, "environ");
        } else
            t->environ = NULL;

        if (flags & (PROC_FILLCOM|PROC_FILLARG)) {      // read /proc/#/task/#/cmdline
            if (flags & PROC_EDITCMDLCVT)
                fill_cmdline_cvt(path, t);
            else
                t->cmdline = file2strvec(path, "cmdline");
        } else
            t->cmdline = NULL;

        if ((flags & PROC_FILLCGROUP)) {                // read /proc/#/task/#/cgroup
            if (flags & PROC_EDITCGRPCVT)
                fill_cgroup_cvt(path, t);
            else
                t->cgroup = file2strvec(path, "cgroup");
        } else
            t->cgroup = NULL;

#ifdef WITH_SYSTEMD
        if (flags & PROC_FILLSYSTEMD)                   // get sd-login.h stuff
            sd2proc(t);
#endif

        if (flags & PROC_FILL_LXC)                      // value the lxc name
            t->lxcname = lxc_containers(path);

#ifdef QUICK_THREADS
    } else {
        t->size     = p->size;
        t->resident = p->resident;
        t->share    = p->share;
        t->trs      = p->trs;
        t->lrs      = p->lrs;
        t->drs      = p->drs;
        t->dt       = p->dt;
        t->cmdline  = p->cmdline;  // better not free these until done with all threads!
        t->environ  = p->environ;
        t->cgroup   = p->cgroup;
        t->supgid   = p->supgid;
        t->supgrp   = p->supgrp;
        t->sd_mach  = p->sd_mach;
        t->sd_ouid  = p->sd_ouid;
        t->sd_seat  = p->sd_seat;
        t->sd_sess  = p->sd_sess;
        t->sd_slice = p->sd_slice;
        t->sd_unit  = p->sd_unit;
        t->sd_uunit = p->sd_uunit;
        t->lxcname = p->lxcname;
        MK_THREAD(t);
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

    return t;
next_task:
    return NULL;
#ifndef QUICK_THREADS
    (void)p;
#endif
}

//////////////////////////////////////////////////////////////////////////////////
// This finds processes in /proc in the traditional way.
// Return non-zero on success.
static int simple_nextpid(PROCTAB *restrict const PT, proc_t *restrict const p) {
  static struct dirent *ent;		/* dirent handle */
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
  static struct dirent *ent;		/* dirent handle */
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
  snprintf(path, PROCPATHLEN, "/proc/%d/task/%s", p->tgid, ent->d_name);
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
  if(!p) p = xcalloc(sizeof *p);
  else free_acquired(p, 1);

  for(;;){
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
    if (!x) x = xcalloc(sizeof(*x));
    else free_acquired(x,1);
    if (new_p) goto next_task;

next_proc:
    new_p = NULL;
    for (;;) {
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
    PROCTAB* PT = xmalloc(sizeof(PROCTAB));

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

    if (!src_buffer){
        src_buffer = xmalloc(MAX_BUFSZ);
        dst_buffer = xmalloc(MAX_BUFSZ);
    }
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
void look_up_our_self(proc_t *p) {
    struct utlbuf_s ub = { NULL, 0 };

    if(file2str("/proc/self", "stat", &ub) == -1){
        fprintf(stderr, "Error, do this: mount -t proc proc /proc\n");
        _exit(47);
    }
    stat2proc(ub.buf, p);  // parse /proc/self/stat
    free(ub.buf);
}

#undef MK_THREAD
#undef IS_THREAD
#undef MAX_BUFSZ
