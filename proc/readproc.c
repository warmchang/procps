/*
 * New Interface to Process Table -- PROCTAB Stream (a la Directory streams)
 * Copyright (C) 1996 Charles L. Blake.
 * Copyright (C) 1998 Michael K. Johnson
 * May be distributed under the conditions of the
 * GNU Library General Public License; a copy is in COPYING
 */
#include "proc/version.h"
#include "proc/readproc.h"
#include "proc/devname.h"
#include "proc/procps.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/dir.h>
#include <sys/types.h>
#include <sys/stat.h>

#define Do(x) (flags & PROC_ ## x)	/* convenient shorthand */

/* initiate a process table scan
 */
PROCTAB* openproc(int flags, ...) {
    va_list ap;
    PROCTAB* PT = xmalloc(sizeof(PROCTAB));
    
    if (Do(PID))
      PT->procfs = NULL;
    else if (!(PT->procfs = opendir("/proc")))
      return NULL;
    PT->flags = flags;
    va_start(ap, flags);		/*  Init args list */
    if (Do(PID))
    	PT->pids = va_arg(ap, pid_t*);
    else if (Do(TTY))
    	PT->ttys = va_arg(ap, dev_t*);
    else if (Do(UID)) {
    	PT->uids = va_arg(ap, uid_t*);
	PT->nuid = va_arg(ap, int);
    } else if (Do(STAT))
    	PT->stats = va_arg(ap, char*);
    va_end(ap);				/*  Clean up args list */
    if (Do(ANYTTY) && Do(TTY))
	PT->flags = PT->flags & ~PROC_TTY; /* turn off TTY flag */
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



static void status2proc (char* S, proc_t* P, int fill) {
    char* tmp;
    if (fill == 1) {
        memset(P->cmd, 0, sizeof P->cmd);
        sscanf (S, "Name:\t%15c", P->cmd);
        tmp = strchr(P->cmd,'\n');
        *tmp='\0';
        tmp = strstr (S,"State");
        sscanf (tmp, "State:\t%c", &P->state);
    }

    tmp = strstr (S,"Pid:");
    if(tmp) sscanf (tmp,
        "Pid:\t%d\n"
        "PPid:\t%d\n",
        &P->pid,
        &P->ppid
    );
    else fprintf(stderr, "Internal error!\n");

    tmp = strstr (S,"Uid:");
    if(tmp) sscanf (tmp,
        "Uid:\t%d\t%d\t%d\t%d",
        &P->ruid, &P->euid, &P->suid, &P->fuid
    );
    else fprintf(stderr, "Internal error!\n");

    tmp = strstr (S,"Gid:");
    if(tmp) sscanf (tmp,
        "Gid:\t%d\t%d\t%d\t%d",
        &P->rgid, &P->egid, &P->sgid, &P->fgid
    );
    else fprintf(stderr, "Internal error!\n");

    tmp = strstr (S,"VmSize:");
    if(tmp) sscanf (tmp,
        "VmSize: %lu kB\n"
        "VmLck: %lu kB\n"
        "VmRSS: %lu kB\n"
        "VmData: %lu kB\n"
        "VmStk: %lu kB\n"
        "VmExe: %lu kB\n"
        "VmLib: %lu kB\n",
        &P->vm_size, &P->vm_lock, &P->vm_rss, &P->vm_data,
        &P->vm_stack, &P->vm_exe, &P->vm_lib
    );
    else /* looks like an annoying kernel thread */
    {
        P->vm_size  = 0;
        P->vm_lock  = 0;
        P->vm_rss   = 0;
        P->vm_data  = 0;
        P->vm_stack = 0;
        P->vm_exe   = 0;
        P->vm_lib   = 0;
    }

    tmp = strstr (S,"SigPnd:");
    if(tmp) sscanf (tmp,
#ifdef SIGNAL_STRING
        "SigPnd: %s SigBlk: %s SigIgn: %s %*s %s",
        P->signal, P->blocked, P->sigignore, P->sigcatch
#else
        "SigPnd: %Lx SigBlk: %Lx SigIgn: %Lx %*s %Lx",
        &P->signal, &P->blocked, &P->sigignore, &P->sigcatch
#endif
    );
    else fprintf(stderr, "Internal error!\n");
}



/* stat2proc() makes sure it can handle arbitrary executable file basenames
 * for `cmd', i.e. those with embedded whitespace or embedded ')'s.
 * Such names confuse %s (see scanf(3)), so the string is split and %39c
 * is used instead. (except for embedded ')' "(%[^)]c)" would work.
 */
static void stat2proc(char* S, proc_t* P) {
    int num;
    char* tmp = strrchr(S, ')');	/* split into "PID (cmd" and "<rest>" */
    *tmp = '\0';			/* replace trailing ')' with NUL */
    /* fill in default values for older kernels */
    P->exit_signal = SIGCHLD;
    P->processor = 0;
    /* parse these two strings separately, skipping the leading "(". */
    memset(P->cmd, 0, sizeof P->cmd);	/* clear even though *P xcalloc'd ?! */
    sscanf(S, "%d (%15c", &P->pid, P->cmd);   /* comm[16] in kernel */
    num = sscanf(tmp + 2,			/* skip space after ')' too */
       "%c "
       "%d %d %d %d %d "
       "%lu %lu %lu %lu %lu %lu %lu "
       "%ld %ld %ld %ld %ld %ld "
       "%lu %lu "
       "%ld "
       "%lu %lu %lu %lu %lu %lu "
       "%*s %*s %*s %*s " /* discard, no RT signals & Linux 2.1 used hex */
       "%lu %lu %lu "
       "%d %d",
       &P->state,
       &P->ppid, &P->pgrp, &P->session, &P->tty, &P->tpgid,
       &P->flags, &P->min_flt, &P->cmin_flt, &P->maj_flt, &P->cmaj_flt, &P->utime, &P->stime,
       &P->cutime, &P->cstime, &P->priority, &P->nice, &P->timeout, &P->it_real_value,
       &P->start_time, &P->vsize,
       &P->rss,
       &P->rss_rlim, &P->start_code, &P->end_code, &P->start_stack, &P->kstk_esp, &P->kstk_eip,
/*     P->signal, P->blocked, P->sigignore, P->sigcatch,   */ /* can't use */
       &P->wchan, &P->nswap, &P->cnswap,
/* -- Linux 2.0.35 ends here -- */
       &P->exit_signal, &P->processor  /* 2.2.1 ends with "exit_signal" */
/* -- Linux 2.2.8 and 2.3.47 end here -- */
    );
    
    /* fprintf(stderr, "stat2proc converted %d fields.\n",num); */
    if (P->tty == 0)
	P->tty = -1;  /* the old notty val, update elsewhere bef. moving to 0 */
}

static void statm2proc(char* s, proc_t* P) {
    int num;
    num = sscanf(s, "%ld %ld %ld %ld %ld %ld %ld",
	   &P->size, &P->resident, &P->share,
	   &P->trs, &P->lrs, &P->drs, &P->dt);
/*    fprintf(stderr, "statm2proc converted %d fields.\n",num); */
}

static int file2str(char *directory, char *what, char *ret, int cap) {
    static char filename[80];
    int fd, num_read;

    sprintf(filename, "%s/%s", directory, what);
    if ( (fd       = open(filename, O_RDONLY, 0)) == -1 ) return -1;
    if ( (num_read = read(fd, ret, cap - 1))      <= 0 ) num_read = -1;
    else ret[num_read] = 0;
    close(fd);
    return num_read;
}

static char** file2strvec(char* directory, char* what) {
    char buf[2048];	/* read buf bytes at a time */
    char *p, *rbuf = 0, *endbuf, **q, **ret;
    int fd, tot = 0, n, c, end_of_file = 0;
    int align;

    sprintf(buf, "%s/%s", directory, what);
    if ( (fd = open(buf, O_RDONLY, 0) ) == -1 ) return NULL;

    /* read whole file into a memory buffer, allocating as we go */
    while ((n = read(fd, buf, sizeof buf - 1)) > 0) {
	if (n < sizeof buf - 1)
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
 * Unfortunately, the reverse filtering option in which any PID *except* the
 * ones listed is pursued.
 */
proc_t* readproc(PROCTAB* PT, proc_t* rbuf) {
    static struct direct *ent;		/* dirent handle */
    static struct stat sb;		/* stat buffer */
    static char path[32], sbuf[512];	/* bufs for stat,statm */
    int allocated = 0, matched = 0;	/* flags */
    proc_t *p = NULL;

    /* loop until a proc matching restrictions is found or no more processes */
    /* I know this could be a while loop -- this way is easier to indent ;-) */
next_proc:				/* get next PID for consideration */

/*printf("PT->flags is 0x%08x\n", PT->flags);*/
#define flags (PT->flags)

    if (Do(PID)) {
	if (!*PT->pids)			/* set to next item in pids */
	    return NULL;
	sprintf(path, "/proc/%d", *(PT->pids)++);
	matched = 1;
    } else {					/* get next numeric /proc ent */
	while ((ent = readdir(PT->procfs)) &&
	       (*ent->d_name < '0' || *ent->d_name > '9'))
	    ;
	if (!ent || !ent->d_name)
	    return NULL;
	sprintf(path, "/proc/%s", ent->d_name);
    }
    if (stat(path, &sb) == -1)		/* no such dirent (anymore) */
	goto next_proc;
    if (Do(UID) && !XinLN(uid_t, sb.st_uid, PT->uids, PT->nuid))
	goto next_proc;			/* not one of the requested uids */

    if (!allocated) {				 /* assign mem for return buf */
	p = rbuf ? rbuf : xcalloc(p, sizeof *p); /* passed buf or alloced mem */
	allocated = 1;				 /* remember space is set up */
    }
    p->euid = sb.st_uid;			/* need a way to get real uid */

    if ((file2str(path, "stat", sbuf, sizeof sbuf)) == -1)
	goto next_proc;			/* error reading /proc/#/stat */
    stat2proc(sbuf, p);				/* parse /proc/#/stat */

    if (!matched && Do(TTY) && !XinL(dev_t, p->tty, PT->ttys))
	goto next_proc;			/* not one of the requested ttys */

    if (!matched && Do(ANYTTY) && p->tty == -1)
	goto next_proc;			/* no controlling terminal */

    if (!matched && Do(STAT) && !strchr(PT->stats,p->state))
	goto next_proc;			/* not one of the requested states */

    if (Do(FILLMEM)) {				/* read, parse /proc/#/statm */
	if ((file2str(path, "statm", sbuf, sizeof sbuf)) != -1 )
	    statm2proc(sbuf, p);		/* ignore statm errors here */
    }						/* statm fields just zero */

    if (Do(FILLSTATUS)) {         /* read, parse /proc/#/status */
       if ((file2str(path, "status", sbuf, sizeof sbuf)) != -1 ){
           status2proc(sbuf, p, 0 /*FIXME*/);
       }
    }

    /* some number->text resolving which is time consuming */
    if (Do(FILLUSR)){
	strncpy(p->euser,   user_from_uid(p->euid), sizeof p->euser);
        strncpy(p->egroup, group_from_gid(p->egid), sizeof p->egroup);
        if(Do(FILLSTATUS)) {
            strncpy(p->ruser,   user_from_uid(p->ruid), sizeof p->ruser);
            strncpy(p->rgroup, group_from_gid(p->rgid), sizeof p->rgroup);
            strncpy(p->suser,   user_from_uid(p->suid), sizeof p->suser);
            strncpy(p->sgroup, group_from_gid(p->sgid), sizeof p->sgroup);
            strncpy(p->fuser,   user_from_uid(p->fuid), sizeof p->fuser);
            strncpy(p->fgroup, group_from_gid(p->fgid), sizeof p->fgroup);
        }
    }

    if (Do(FILLCMD))				/* read+parse /proc/#/cmdline */
	p->cmdline = file2strvec(path, "cmdline");
    if (Do(FILLENV))				/* read+parse /proc/#/environ */
	p->environ = file2strvec(path, "environ");
    
    if (p->state == 'Z')			/* fixup cmd for zombies */
	strncat(p->cmd," <defunct>", sizeof p->cmd);

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
 * Unfortunately, the reverse filtering option in which any PID *except* the
 * ones listed is pursued.
 */
proc_t* ps_readproc(PROCTAB* PT, proc_t* rbuf) {
    static struct direct *ent;		/* dirent handle */
    static struct stat sb;		/* stat buffer */
    static char path[32], sbuf[512];	/* bufs for stat,statm */
    int allocated = 0 /* , matched = 0 */ ;	/* flags */
    proc_t *p = NULL;

    /* loop until a proc matching restrictions is found or no more processes */
    /* I know this could be a while loop -- this way is easier to indent ;-) */
next_proc:				/* get next PID for consideration */

/*printf("PT->flags is 0x%08x\n", PT->flags);*/
#define flags (PT->flags)

	while ((ent = readdir(PT->procfs)) &&
	       (*ent->d_name < '0' || *ent->d_name > '9'))
	    ;
	if (!ent || !ent->d_name)
	    return NULL;
	sprintf(path, "/proc/%s", ent->d_name);

    if (stat(path, &sb) == -1)		/* no such dirent (anymore) */
	goto next_proc;

    if (!allocated) {				 /* assign mem for return buf */
	p = rbuf ? rbuf : xcalloc(p, sizeof *p); /* passed buf or alloced mem */
	allocated = 1;				 /* remember space is set up */
    }
    p->euid = sb.st_uid;			/* need a way to get real uid */

    if ((file2str(path, "stat", sbuf, sizeof sbuf)) == -1)
	goto next_proc;			/* error reading /proc/#/stat */
    stat2proc(sbuf, p);				/* parse /proc/#/stat */

/*    if (Do(FILLMEM)) {*/				/* read, parse /proc/#/statm */
	if ((file2str(path, "statm", sbuf, sizeof sbuf)) != -1 )
	    statm2proc(sbuf, p);		/* ignore statm errors here */
/*    }			*/			/* statm fields just zero */

  /*  if (Do(FILLSTATUS)) { */        /* read, parse /proc/#/status */
       if ((file2str(path, "status", sbuf, sizeof sbuf)) != -1 ){
           status2proc(sbuf, p, 0 /*FIXME*/);
       }
/*    }*/

    /* some number->text resolving which is time consuming */
 /*   if (Do(FILLUSR)){ */
	strncpy(p->euser,   user_from_uid(p->euid), sizeof p->euser);
        strncpy(p->egroup, group_from_gid(p->egid), sizeof p->egroup);
/*        if(Do(FILLSTATUS)) { */
            strncpy(p->ruser,   user_from_uid(p->ruid), sizeof p->ruser);
            strncpy(p->rgroup, group_from_gid(p->rgid), sizeof p->rgroup);
            strncpy(p->suser,   user_from_uid(p->suid), sizeof p->suser);
            strncpy(p->sgroup, group_from_gid(p->sgid), sizeof p->sgroup);
            strncpy(p->fuser,   user_from_uid(p->fuid), sizeof p->fuser);
            strncpy(p->fgroup, group_from_gid(p->fgid), sizeof p->fgroup);
/*        }*/
/*    }*/

/*    if (Do(FILLCMD))	*/			/* read+parse /proc/#/cmdline */
	p->cmdline = file2strvec(path, "cmdline");
/*    if (Do(FILLENV))	*/			/* read+parse /proc/#/environ */
	p->environ = file2strvec(path, "environ");
    
    if (p->state == 'Z')			/* fixup cmd for zombies */
	strncat(p->cmd," <defunct>", sizeof p->cmd);

    return p;
}
#undef flags


void look_up_our_self(proc_t *p) {
    static char path[32], sbuf[512];	/* bufs for stat,statm */
    sprintf(path, "/proc/%d", getpid());
    file2str(path, "stat", sbuf, sizeof sbuf);
    stat2proc(sbuf, p);				/* parse /proc/#/stat */
    file2str(path, "statm", sbuf, sizeof sbuf);
    statm2proc(sbuf, p);		/* ignore statm errors here */
    file2str(path, "status", sbuf, sizeof sbuf);
    status2proc(sbuf, p, 0 /*FIXME*/);
}


/* Convenient wrapper around openproc and readproc to slurp in the whole process
 * tree subset satisfying the constraints of flags and the optional PID list.
 * Free allocated memory with freeproctree().  The tree structure is a classic
 * left-list children + right-list siblings.  The algorithm is a two-pass of the
 * process table.  Since most process trees will have children with strictly
 * increasing PIDs, most of the structure will be picked up in the first pass.
 * The second loop then cleans up any nodes which turn out to have preceeded
 * their parent in /proc order.
 */

/* Traverse tree 't' breadth-first looking for a process with pid p */
static proc_t* LookupPID(proc_t* t, pid_t p) {
    proc_t* tmp = NULL;
    if (!t)
	return NULL;
    if (t->pid == p)				/* look here/terminate recursion */
	return t;
    if ((tmp = LookupPID(t->l, p)))		/* recurse over children */
	return tmp;
    for (; t; t=t->r)				/* recurse over siblings */
	if ((tmp = LookupPID(tmp, p)))
	    return tmp;
    return NULL;
}

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
    if (Do(UID)) {
	/* temporary variables to ensure that va_arg() instances
	 * are called in the right order
	 */
	uid_t* u;
	int i;

	u = va_arg(ap, uid_t*);
	i = va_arg(ap, int);
	PT = openproc(flags, u, i);
    }
    else if (Do(PID) || Do(TTY) || Do(STAT))
	PT = openproc(flags, va_arg(ap, void*)); /* assume ptr sizes same */
    else
	PT = openproc(flags);
    va_end(ap);
    do {					/* read table: */
	tab = xrealloc(tab, (n+1)*sizeof(proc_t*));/* realloc as we go, using */
	tab[n] = readproc(PT, NULL);		  /* final null to terminate */
    } while (tab[n++]);				  /* stop when NULL reached */
    closeproc(PT);
    return tab;
}
