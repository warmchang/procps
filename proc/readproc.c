/*
 * New Interface to Process Table -- PROCTAB Stream (a la Directory streams)
 * Copyright (C) 1996 Charles L. Blake.
 * Copyright (C) 1998 Michael K. Johnson
 * Copyright 1998-2002 Albert Cahalan
 * May be distributed under the conditions of the
 * GNU Library General Public License; a copy is in COPYING
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "version.h"
#include "readproc.h"
#include "alloc.h"
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
#include <sys/dir.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef FLASK_LINUX
#include <fs_secure.h>
#endif

#ifdef PROF
extern void __cyg_profile_func_enter(void*,void*);
#define ENTER(x) __cyg_profile_func_enter((void*)x,(void*)x)
#define LEAVE(x) __cyg_profile_func_exit((void*)x,(void*)x)
#else
#define ENTER(x)
#define LEAVE(x)
#endif

/* initiate a process table scan
 */
PROCTAB* openproc(int flags, ...) {
    va_list ap;
    PROCTAB* PT = xmalloc(sizeof(PROCTAB));
    
    if (flags & PROC_PID)
      PT->procfs = NULL;
    else if (!(PT->procfs = opendir("/proc")))
      return NULL;
    PT->flags = flags;
    va_start(ap, flags);		/*  Init args list */
    if (flags & PROC_PID)
    	PT->pids = va_arg(ap, pid_t*);
    else if (flags & PROC_UID) {
    	PT->uids = va_arg(ap, uid_t*);
	PT->nuid = va_arg(ap, int);
    }
    va_end(ap);				/*  Clean up args list */
    return PT;
}

/* terminate a process table scan
 */
void closeproc(PROCTAB* PT) {
    if (PT){
        if (PT->procfs) closedir(PT->procfs);
        free(PT);
    }
}

/* deallocate the space allocated by readproc if the passed rbuf was NULL
 */
void freeproc(proc_t* p) {
    if (!p)	/* in case p is NULL */
	return;
    /* ptrs are after strings to avoid copying memory when building them. */
    /* so free is called on the address of the address of strvec[0]. */
    if (p->cmdline)
	free((void*)*p->cmdline);
    if (p->environ)
	free((void*)*p->environ);
    free(p);
}


///////////////////////////////////////////////////////////////////////////

typedef struct status_table_struct {
    unsigned char name[6];        // /proc/*/status field name
    short len;                    // name length
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

// Derived from:
// gperf -7 --language=ANSI-C --key-positions=1,3,4 -C -n -c sml.gperf

static void status2proc(char *S, proc_t *restrict P){
    static const unsigned char asso[] = {
        56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56,
        56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 15, 56, 56, 56, 56, 56,
        56, 56, 25, 30, 15,  3, 56,  5, 56,  3, 56, 56,  3, 56, 10, 56,
        18, 56, 13,  0, 30, 25,  0, 56, 56, 56, 56, 56, 56, 56, 56, 56,
        56, 30, 56,  8,  0,  0, 56, 25, 56,  5, 56, 56, 56,  0, 56, 56,
        56, 56, 56, 56,  0, 56, 56, 56,  0, 56, 56, 56, 56, 56, 56, 56
    };
    static const status_table_struct table[] = {
        F(VmStk)
        NUL
        NUL
        F(VmExe)
        NUL
        F(VmSize)
        NUL
        NUL
        F(VmLib)
        NUL
        F(Name)
        F(VmLck)
        NUL
        F(VmRSS)
        NUL
        NUL
        NUL
        NUL
        F(ShdPnd)
        NUL
        F(Gid)
        NUL
        NUL
        F(PPid)
        NUL
        NUL
        NUL
        NUL
        F(SigIgn)
        NUL
        F(State)
        NUL
        NUL
        F(Pid)
        NUL
        F(Tgid)
        NUL
        NUL
        NUL
        NUL
        F(Uid)
        NUL
        NUL
        F(SigPnd)
        NUL
        F(VmData)
        NUL
        NUL
        NUL
        NUL
        F(SigBlk)
        NUL
        NUL
        NUL
        NUL
        F(SigCgt)
        NUL
        NUL
        NUL
        NUL
        NUL
        NUL
        NUL
        NUL
    };

#undef F
#undef NUL

ENTER(0x220);

    P->vm_size = 0;
    P->vm_lock = 0;
    P->vm_rss  = 0;
    P->vm_data = 0;
    P->vm_stack= 0;
    P->vm_exe  = 0;
    P->vm_lib  = 0;

    goto base;

    for(;;){
        char *colon;
        status_table_struct entry;

        // advance to next line
        S = strchr(S, '\n');
        if(unlikely(!S)) break;  // if no newline
        S++;

        // examine a field name (hash and compare)
    base:
        if(unlikely(!*S)) break;
        entry = table[63 & (asso[S[3]] + asso[S[2]] + asso[S[0]])];
        colon = strchr(S, ':');
        if(unlikely(!colon)) break;
        if(unlikely(colon[1]!='\t')) break;
        if(unlikely(colon-S != entry.len)) continue;
        if(unlikely(memcmp(entry.name,S,colon-S))) continue;

        S = colon+2; // past the '\t'

#ifdef LABEL_OFFSET
        goto *(&&base + entry.offset);
#else
        goto *entry.addr;
#endif

    case_Gid:
        P->rgid = strtol(S,&S,10);
        P->egid = strtol(S,&S,10);
        P->sgid = strtol(S,&S,10);
        P->fgid = strtol(S,&S,10);
        continue;
    case_Name:{
        unsigned u = 0;
        while(u < sizeof P->cmd - 1u){
            int c = *S++;
            if(unlikely(c=='\n')) break;
            if(unlikely(c=='\0')) break; // should never happen
            if(unlikely(c=='\\')){
                c = *S++;
                if(c=='\n') break; // should never happen
                if(!c)      break; // should never happen
                if(c=='n') c='\n'; // else we assume it is '\\'
            }
            P->cmd[u++] = c;
        }
        P->cmd[u] = '\0';
        S--;   // put back the '\n' or '\0'
        continue;
    }
    case_PPid:
        P->ppid = strtol(S,&S,10);
        continue;
    case_Pid:
        P->pid = strtol(S,&S,10);
        continue;

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
        memcpy(P->signal, S, 16);
        P->signal[16] = '\0';
        continue;

    case_State:
        P->state = *S;
        continue;
    case_Tgid:
        P->tgid = strtol(S,&S,10);
        continue;
    case_Uid:
        P->ruid = strtol(S,&S,10);
        P->euid = strtol(S,&S,10);
        P->suid = strtol(S,&S,10);
        P->fuid = strtol(S,&S,10);
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
    case_VmSize:
        P->vm_size = strtol(S,&S,10);
        continue;
    case_VmStk:
        P->vm_stack = strtol(S,&S,10);
        continue;
    }
LEAVE(0x220);
}

///////////////////////////////////////////////////////////////////////

// Reads /proc/*/stat files, being careful not to trip over processes with
// names like ":-) 1 2 3 4 5 6".
static void stat2proc(const char* S, proc_t *restrict P) {
    unsigned num;
    char* tmp;

ENTER(0x160);

    /* fill in default values for older kernels */
    P->exit_signal = SIGCHLD;
    P->processor = 0;
    P->rtprio = -1;
    P->sched = -1;

    S = strchr(S, '(') + 1;
    tmp = strrchr(S, ')');
    num = tmp - S;
    if(unlikely(num >= sizeof P->cmd)) num = sizeof P->cmd - 1;
    memcpy(P->cmd, S, num);
    P->cmd[num] = '\0';
    S = tmp + 2;                 // skip ") "

    num = sscanf(S,
       "%c "
       "%d %d %d %d %d "
       "%lu %lu %lu %lu %lu "
       "%Lu %Lu %Lu %Lu "  /* utime stime cutime cstime */
       "%ld %ld %ld %ld "
       "%Lu "  /* start_time */
       "%lu "
       "%ld "
       "%lu %"KLF"u %"KLF"u %"KLF"u %"KLF"u %"KLF"u "
       "%*s %*s %*s %*s " /* discard, no RT signals & Linux 2.1 used hex */
       "%"KLF"u %lu %lu "
       "%d %d "
       "%lu %lu",
       &P->state,
       &P->ppid, &P->pgrp, &P->session, &P->tty, &P->tpgid,
       &P->flags, &P->min_flt, &P->cmin_flt, &P->maj_flt, &P->cmaj_flt,
       &P->utime, &P->stime, &P->cutime, &P->cstime,
       &P->priority, &P->nice, &P->timeout, &P->it_real_value,
       &P->start_time,
       &P->vsize,
       &P->rss,
       &P->rss_rlim, &P->start_code, &P->end_code, &P->start_stack, &P->kstk_esp, &P->kstk_eip,
/*     P->signal, P->blocked, P->sigignore, P->sigcatch,   */ /* can't use */
       &P->wchan, &P->nswap, &P->cnswap,
/* -- Linux 2.0.35 ends here -- */
       &P->exit_signal, &P->processor,  /* 2.2.1 ends with "exit_signal" */
/* -- Linux 2.2.8 to 2.5.17 end here -- */
       &P->rtprio, &P->sched  /* both added to 2.5.18 */
    );
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

static int file2str(const char *directory, const char *what, char *ret, int cap) {
    static char filename[80];
    int fd, num_read;

    sprintf(filename, "%s/%s", directory, what);
    fd = open(filename, O_RDONLY, 0);
    if(unlikely(fd==-1)) return -1;
    num_read = read(fd, ret, cap - 1);
    close(fd);
    if(unlikely(num_read<=0)) return -1;
    ret[num_read] = '\0';
    return num_read;
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
    while ((n = read(fd, buf, sizeof buf - 1)) > 0) {
	if (n < (int)(sizeof buf - 1))
	    end_of_file = 1;
	if (n == 0 && rbuf == 0)
	    return NULL;	/* process died between our open and read */
	if (n < 0) {
	    if (rbuf)
		free(rbuf);
	    return NULL;	/* read error */
	}
	if (end_of_file && buf[n-1])		/* last read char not null */
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
    for (c = 0, p = rbuf; p < endbuf; p++)
    	if (!*p)
	    c += sizeof(char*);
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

// warning: interface may change
int read_cmdline(char *restrict const dst, unsigned sz, unsigned pid){
    char name[32];
    int fd;
    unsigned n = 0;
    dst[0] = '\0';
    snprintf(name, sizeof name, "/proc/%u/cmdline", pid);
    fd = open(name, O_RDONLY);
    if(fd==-1) return 0;
    for(;;){
        ssize_t r = read(fd,dst+n,sz-n);
        if(r==-1){
            if(errno==EINTR) continue;
            break;
        }
        n += r;
        if(n==sz) break; // filled the buffer
        if(r==0) break;  // EOF
    }
    close(fd);
    if(n){
        int i;
        if(n==sz) n--;
        dst[n] = '\0';
        i=n;
        while(i--){
            int c = dst[i];
            if(c<' ' || c>'~') dst[i]=' ';
        }
    }
    return n;
}

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

/* readproc: return a pointer to a proc_t filled with requested info about the
 * next process available matching the restriction set.  If no more such
 * processes are available, return a null pointer (boolean false).  Use the
 * passed buffer instead of allocating space if it is non-NULL.  */

/* This is optimized so that if a PID list is given, only those files are
 * searched for in /proc.  If other lists are given in addition to the PID list,
 * the same logic can follow through as for the no-PID list case.  This is
 * fairly complex, but it does try to not to do any unnecessary work.
 */
proc_t* readproc(PROCTAB* PT, proc_t* p) {
    static struct direct *ent;		/* dirent handle */
    static struct stat sb;		/* stat buffer */
    static char path[32], sbuf[1024];	/* bufs for stat,statm */
#ifdef FLASK_LINUX
    security_id_t secsid;
#endif
    pid_t pid;  // saved until we have a proc_t allocated for sure

    /* loop until a proc matching restrictions is found or no more processes */
    /* I know this could be a while loop -- this way is easier to indent ;-) */
next_proc:				/* get next PID for consideration */

/*printf("PT->flags is 0x%08x\n", PT->flags);*/
#define flags (PT->flags)

    if (flags & PROC_PID) {
        pid = *(PT->pids)++;
	if (unlikely(!pid)) return NULL;
	snprintf(path, sizeof path, "/proc/%d", pid);
    } else {					/* get next numeric /proc ent */
	for (;;) {
	    ent = readdir(PT->procfs);
	    if(unlikely(unlikely(!ent) || unlikely(!ent->d_name))) return NULL;
	    if(likely( likely(*ent->d_name > '0') && likely(*ent->d_name <= '9') )) break;
	}
	pid = strtoul(ent->d_name, NULL, 10);
	memcpy(path, "/proc/", 6);
	strcpy(path+6, ent->d_name);  // trust /proc to not contain evil top-level entries
//	snprintf(path, sizeof path, "/proc/%s", ent->d_name);
    }
#ifdef FLASK_LINUX
    if ( stat_secure(path, &sb, &secsid) == -1 ) /* no such dirent (anymore) */
#else
    if (unlikely(stat(path, &sb) == -1))	/* no such dirent (anymore) */
#endif
	goto next_proc;

    if ((flags & PROC_UID) && !XinLN(uid_t, sb.st_uid, PT->uids, PT->nuid))
	goto next_proc;			/* not one of the requested uids */

    if (!p)
	p = xcalloc(p, sizeof *p); /* passed buf or alloced mem */

    p->euid = sb.st_uid;			/* need a way to get real uid */
#ifdef FLASK_LINUX
    p->secsid = secsid;
#endif
    p->pid  = pid;

    if (flags & PROC_FILLSTAT) {         /* read, parse /proc/#/stat */
	if (unlikely( file2str(path, "stat", sbuf, sizeof sbuf) == -1 ))
	    goto next_proc;			/* error reading /proc/#/stat */
	stat2proc(sbuf, p);				/* parse /proc/#/stat */
    }

    if (unlikely(flags & PROC_FILLMEM)) {				/* read, parse /proc/#/statm */
	if (likely( file2str(path, "statm", sbuf, sizeof sbuf) != -1 ))
	    statm2proc(sbuf, p);		/* ignore statm errors here */
    }						/* statm fields just zero */

    if (flags & PROC_FILLSTATUS) {         /* read, parse /proc/#/status */
       if (likely( file2str(path, "status", sbuf, sizeof sbuf) != -1 )){
           status2proc(sbuf, p);
       }
    }

    /* some number->text resolving which is time consuming */
    if (flags & PROC_FILLUSR){
	strncpy(p->euser,   user_from_uid(p->euid), sizeof p->euser);
        if(flags & PROC_FILLSTATUS) {
            strncpy(p->ruser,   user_from_uid(p->ruid), sizeof p->ruser);
            strncpy(p->suser,   user_from_uid(p->suid), sizeof p->suser);
            strncpy(p->fuser,   user_from_uid(p->fuid), sizeof p->fuser);
        }
    }

    /* some number->text resolving which is time consuming */
    if (flags & PROC_FILLGRP){
        strncpy(p->egroup, group_from_gid(p->egid), sizeof p->egroup);
        if(flags & PROC_FILLSTATUS) {
            strncpy(p->rgroup, group_from_gid(p->rgid), sizeof p->rgroup);
            strncpy(p->sgroup, group_from_gid(p->sgid), sizeof p->sgroup);
            strncpy(p->fgroup, group_from_gid(p->fgid), sizeof p->fgroup);
        }
    }

    if ((flags & PROC_FILLCOM) || (flags & PROC_FILLARG))	/* read+parse /proc/#/cmdline */
	p->cmdline = file2strvec(path, "cmdline");
    else
        p->cmdline = NULL;

    if (unlikely(flags & PROC_FILLENV))			/* read+parse /proc/#/environ */
	p->environ = file2strvec(path, "environ");
    else
        p->environ = NULL;
    
    return p;
}
#undef flags

/* ps_readproc: return a pointer to a proc_t filled with requested info about the
 * next process available matching the restriction set.  If no more such
 * processes are available, return a null pointer (boolean false).  Use the
 * passed buffer instead of allocating space if it is non-NULL.  */

/* This is optimized so that if a PID list is given, only those files are
 * searched for in /proc.  If other lists are given in addition to the PID list,
 * the same logic can follow through as for the no-PID list case.  This is
 * fairly complex, but it does try to not to do any unnecessary work.
 */
proc_t* ps_readproc(PROCTAB* PT, proc_t* p) {
    static struct direct *ent;		/* dirent handle */
    static struct stat sb;		/* stat buffer */
    static char path[32], sbuf[1024];	/* bufs for stat,statm */
#ifdef FLASK_LINUX
    security_id_t secsid;
#endif
    pid_t pid;  // saved until we have a proc_t allocated for sure

    /* loop until a proc matching restrictions is found or no more processes */
    /* I know this could be a while loop -- this way is easier to indent ;-) */
next_proc:				/* get next PID for consideration */

/*printf("PT->flags is 0x%08x\n", PT->flags);*/
#define flags (PT->flags)

    for (;;) {
	ent = readdir(PT->procfs);
	if(unlikely(unlikely(!ent) || unlikely(!ent->d_name))) return NULL;
	if(likely( likely(*ent->d_name > '0') && likely(*ent->d_name <= '9') )) break;
    }
    pid = strtoul(ent->d_name, NULL, 10);
    memcpy(path, "/proc/", 6);
    strcpy(path+6, ent->d_name);  // trust /proc to not contain evil top-level entries
//  snprintf(path, sizeof path, "/proc/%s", ent->d_name);

#ifdef FLASK_LINUX
    if (stat_secure(path, &sb, &secsid) == -1) /* no such dirent (anymore) */
#else
    if (stat(path, &sb) == -1)		/* no such dirent (anymore) */
#endif
	goto next_proc;

    if (!p)
	p = xcalloc(p, sizeof *p); /* passed buf or alloced mem */

    p->euid = sb.st_uid;			/* need a way to get real uid */
#ifdef FLASK_LINUX
    p->secsid = secsid;
#endif
    p->pid  = pid;

    if ((file2str(path, "stat", sbuf, sizeof sbuf)) == -1)
	goto next_proc;			/* error reading /proc/#/stat */
    stat2proc(sbuf, p);				/* parse /proc/#/stat */

    if (flags & PROC_FILLMEM) {				/* read, parse /proc/#/statm */
	if ((file2str(path, "statm", sbuf, sizeof sbuf)) != -1 )
	    statm2proc(sbuf, p);		/* ignore statm errors here */
    }						/* statm fields just zero */

  /*  if (flags & PROC_FILLSTATUS) { */        /* read, parse /proc/#/status */
       if ((file2str(path, "status", sbuf, sizeof sbuf)) != -1 ){
           status2proc(sbuf, p);
       }
/*    }*/

    /* some number->text resolving which is time consuming */
    if (flags & PROC_FILLUSR){
	strncpy(p->euser,   user_from_uid(p->euid), sizeof p->euser);
/*        if(flags & PROC_FILLSTATUS) { */
            strncpy(p->ruser,   user_from_uid(p->ruid), sizeof p->ruser);
            strncpy(p->suser,   user_from_uid(p->suid), sizeof p->suser);
            strncpy(p->fuser,   user_from_uid(p->fuid), sizeof p->fuser);
/*        }*/
    }

    /* some number->text resolving which is time consuming */
    if (flags & PROC_FILLGRP){
        strncpy(p->egroup, group_from_gid(p->egid), sizeof p->egroup);
/*        if(flags & PROC_FILLSTATUS) { */
            strncpy(p->rgroup, group_from_gid(p->rgid), sizeof p->rgroup);
            strncpy(p->sgroup, group_from_gid(p->sgid), sizeof p->sgroup);
            strncpy(p->fgroup, group_from_gid(p->fgid), sizeof p->fgroup);
/*        }*/
    }

    if ((flags & PROC_FILLCOM) || (flags & PROC_FILLARG))	/* read+parse /proc/#/cmdline */
	p->cmdline = file2strvec(path, "cmdline");
    else
        p->cmdline = NULL;

    if (flags & PROC_FILLENV)			/* read+parse /proc/#/environ */
	p->environ = file2strvec(path, "environ");
    else
        p->environ = NULL;
    
    return p;
}
#undef flags


void look_up_our_self(proc_t *p) {
    char sbuf[1024];

    if(file2str("/proc/self", "stat", sbuf, sizeof sbuf) == -1){
        fprintf(stderr, "Error, do this: mount -t proc none /proc\n");
        _exit(47);
    }
    stat2proc(sbuf, p);    // parse /proc/self/stat
}

HIDDEN_ALIAS(readproc);

/* Convenient wrapper around openproc and readproc to slurp in the whole process
 * table subset satisfying the constraints of flags and the optional PID list.
 * Free allocated memory with freeproctab().  Access via tab[N]->member.  The
 * pointer list is NULL terminated.
 */
proc_t** readproctab(int flags, ...) {
    PROCTAB* PT = NULL;
    proc_t** tab = NULL;
    int n = 0;
    va_list ap;

    va_start(ap, flags);		/* pass through args to openproc */
    if (flags & PROC_UID) {
	/* temporary variables to ensure that va_arg() instances
	 * are called in the right order
	 */
	uid_t* u;
	int i;

	u = va_arg(ap, uid_t*);
	i = va_arg(ap, int);
	PT = openproc(flags, u, i);
    }
    else if (flags & PROC_PID)
	PT = openproc(flags, va_arg(ap, void*)); /* assume ptr sizes same */
    else
	PT = openproc(flags);
    va_end(ap);
    do {					/* read table: */
	tab = xrealloc(tab, (n+1)*sizeof(proc_t*));/* realloc as we go, using */
	tab[n] = readproc_direct(PT, NULL);     /* final null to terminate */
    } while (tab[n++]);				  /* stop when NULL reached */
    closeproc(PT);
    return tab;
}
