/*
 * ps.c                - show process status
 *
 * Copyright (c) 1992 Branko Lankester
 *
 * Snarfed and HEAVILY modified for procps by Michael K. Johnson,
 * (johnsonm@sunsite.unc.edu).  What is used is what is required to have a
 *  common interface.
 *
 * Massive modifications by Charles Blake (cblake@bbn.com).  Rewrite
 * of the system process table code, multilevel sorting, device number
 * database, forest feature (contributed by ...), environment variables, GNU
 * style long options, pid list filtering (contributed by Michael Shields).
 *
 * Michael K. Johnson <johnsonm@redhat.com> again in charge, merging
 * patches that have accumulated over a long time.
 *
 * Changes Copyright (C) 1993, 1994, 1997 Michael K. Johnson,
 *   and   Copyright (C) 1995, 1996 Charles Blake
 * See file COPYING for copyright details.
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <time.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/param.h>

#include <proc/version.h>
#include <proc/readproc.h>
#include <proc/procps.h>
#include <proc/devname.h>
#include <proc/tree.h>
#include <proc/sysinfo.h>
#include <proc/status.h>
#include <proc/compare.h>

static void show_short(char*, proc_t*);
static void show_long (char*, proc_t*);
static void show_user (char*, proc_t*);
static void show_jobs (char*, proc_t*);
static void show_sig  (char*, proc_t*);
static void show_vm   (char*, proc_t*);
static void show_m    (char*, proc_t*);
static void show_regs (char*, proc_t*);

/* this struct replaces the previously parallel fmt_fnc and hdrs */
static struct {
    void      (*format)(char*,proc_t*);
    char        CL_option_char;
    const char*	default_sort;
    const char*	header;
} mode[] = {
    { show_short,  0 , "Up", "  PID TTY STAT TIME COMMAND" },
    { show_long,  'l', "Pp", " FLAGS   UID   PID  PPID PRI  NI   SIZE   RSS WCHAN       STA TTY TIME COMMAND" },
    { show_user,  'u', "up", "USER       PID %CPU %MEM  SIZE   RSS TTY STAT START   TIME COMMAND" },
    { show_jobs,  'j', "gPp"," PPID   PID  PGID   SID TTY TPGID  STAT  UID   TIME COMMAND" },
    { show_sig,   's', "p",  "  UID   PID SIGNAL   BLOCKED  IGNORED  CATCHED  STAT TTY   TIME COMMAND" },
    { show_vm,    'v', "r",  "  PID TTY STAT TIME  PAGEIN TSIZ DSIZ  RSS   LIM %MEM COMMAND" },
    { show_m,     'm', "r",  "  PID TTY MAJFLT MINFLT   TRS   DRS  SIZE  SWAP   RSS  SHRD   LIB  DT COMMAND" },
    { show_regs,  'X', "p",  "NR   PID    STACK      ESP      EIP TMOUT ALARM STAT TTY   TIME COMMAND" },
    { NULL, 0, NULL, NULL }
};

/* some convenient integer constants corresponding to the above rows. */
enum PS_MODALITY { PS_D = 0, PS_L, PS_U, PS_J, PS_S, PS_V, PS_M, PS_X };

static char * prtime         (char *s, unsigned long t, unsigned long rel);
static void   usage          (char* context);
static void   set_cmdspc     (int w_opts);
static void   show_procs     (unsigned maxcmd, int do_header, int, void*,int);
static void   show_cmd_env   (char* tskcmd, char** cmd, char** env, unsigned maxch);
static void   show_a_proc    (proc_t* the_proc, unsigned maxcmd);
static void   show_time      (char *s, proc_t * p);
static void   add_node       (char *s, proc_t *task);
static int    node_cmp       (const void *s1, const void *s2);
static void   show_tree      (int n, int depth, char *continued);
static void   show_forest    (void);

static int CL_fmt       = 0;

/* process list filtering command line options */

static int CL_all,
    CL_kern_comm,
    CL_no_ctty,
    CL_run_only;
static char  * CL_ctty;
static pid_t * CL_pids;          /* zero-terminated list, dynamically allocated */

/* process display modification command line options */

static int CL_show_env,
    CL_num_outp,          /* numeric fields for user, wchan, tty */
    CL_sort     = 1,
    CL_forest,
    CL_Sum,
    CL_pg_shift = (PAGE_SHIFT - 10);	/* default: show k instead of pages */

/* Globals */

static unsigned cmdspc = 80;     /* space left for cmd+env after table per row */
static int      GL_current_time; /* some global system parameters */
static long     GL_time_now;
static int      GL_wchan_nout;   /* this is can also be set on the command-line  */


/*****************************/
int main(int argc, char **argv) {
    char *p;
    int width = 0,
	do_header = 1,
	user_ord = 0,
	next_arg = 0,
	toppid = 0,
	pflags, N = 1;
    void* args = NULL;
    dev_t tty[2] = { 0 };
    uid_t uid[1];
    
    do {
        --argc;		/* shift to next arg. */
        ++argv;
        for (p = *argv; p && *p; ++p) {
            switch (*p) {
	      case '-':               /* "--" ==> long name options */
		if (*(p+1) == '-') {
                    if (strncmp(p+2,"sort",4)==0) {
			if (parse_long_sort(p+7))
			    usage("unrecognized long sort option\n");
			user_ord = 1;
			next_arg = 1;
			break;
                    } else if (strncmp(p+2, "help", 4) == 0) {
			usage(NULL);
			return 0;
                    } else if (strncmp(p+2, "version", 6) == 0) {
			display_version();
			return 0;
                    } else if (*(p+2) != '\0')	/* just '-- '; not an error */
			usage("ps: unknown long option\n");
		}
		if (!getenv("I_WANT_A_BROKEN_PS")) {
		    fprintf(stderr,
			    "warning: `-' deprecated; use `ps %s', not `ps %s'\n",
			    (p+1), p);
		}
		break;
	      case 'l': CL_fmt = PS_L;   /* replaceable by a */	break;
	      case 'u': CL_fmt = PS_U;   /* loop over mode[] */	break;
	      case 'j': CL_fmt = PS_J;				break;
	      case 's': CL_fmt = PS_S;				break;
	      case 'v': CL_fmt = PS_V;				break;
	      case 'm': CL_fmt = PS_M;				break;
	      case 'X': CL_fmt = PS_X;   /* regs */		break;

	      case 'r': CL_run_only = 1; /* list filters */	break;
	      case 'a': CL_all = 1;				break;
	      case 'x': CL_no_ctty = 1;				break;
	      case 't': CL_ctty = p + 1;
		next_arg = 1;				break;

	      case 'e': CL_show_env = 1; /* output modifiers */	break;
	      case 'f': CL_forest = 1;
		CL_kern_comm = 0;			break;
	      case 'c': CL_kern_comm = 1;			break;
	      case 'w': ++width;				break;
	      case 'h': do_header = 0;				break;
	      case 'n': CL_num_outp = 1;
		GL_wchan_nout = 1;			break;
	      case 'S': CL_Sum = 1;				break;
	      case 'p': CL_pg_shift = 0;			break;
	      case 'o': CL_sort = !CL_sort;			break;
	      case 'O':
		if (parse_sort_opt(p+1))
		    usage("short form sort flag parse error\n");
		user_ord = 1;
		next_arg = 1;
		break;
	      case 'V': display_version(); exit(0);
	      default:
                /* Step through, reading+alloc space for comma-delim pids */
		if (isdigit(*p)) {
		    while (isdigit(*p)) {
			CL_pids = xrealloc(CL_pids, (toppid + 2)*sizeof(pid_t));
			CL_pids[toppid++] = atoi(p);
			while (isdigit(*p))
			    p++;
			if (*p == ',')
			    p++;
		    }
		    CL_pids[toppid] = 0;
		    next_arg = 1;
		}
		if (*p)
		    usage("unrecognized option or trailing garbage\n");
            }
            if (next_arg) {
                next_arg = 0;
                break;       /* end loop over chars in this argument */
            }
        }
    } while (argc > 1);

    if (!CL_sort)	/* since the unsorted mode is intended to be speedy */
	CL_forest = 0;	/* turn off the expensive forest option as well. */

    if (CL_fmt == PS_L) {
	if (open_psdb(NULL)) GL_wchan_nout = 1;
    }

    set_cmdspc(width);

    if (!(GL_current_time = uptime(0,0)))
	return 1;
    GL_time_now = time(0L);

    if (CL_sort && !user_ord)
        parse_sort_opt(mode[CL_fmt].default_sort);

    /* NOTE:  all but option parsing has really been done to enable
     * multiple uid/tty/state filtering as well as multiple pid filtering
     */
    pflags = PROC_ANYTTY;	/* defaults */

    if (!CL_kern_comm)	pflags |= PROC_FILLCMD;  	 /* verbosity flags */
    if (CL_fmt == PS_M) pflags |= PROC_FILLMEM;
    if (CL_show_env)	pflags |= PROC_FILLENV;
    if (!CL_num_outp)	pflags |= PROC_FILLUSR;

    if (CL_no_ctty)	pflags &= ~PROC_ANYTTY;		/* filter flags */
    if (CL_run_only)  { pflags |= PROC_STAT; args = "RD"; }
    else if (!CL_all)      { pflags |= PROC_UID;  args = uid; uid[0] = getuid(); pflags &= ~PROC_STAT; }
    if (CL_pids)      { pflags |= PROC_PID;  args = CL_pids; pflags &= ~PROC_UID; pflags &= ~PROC_STAT; }
    if (CL_ctty) {
	if ((tty[0] = tty_to_dev(CL_ctty)) == (dev_t)-1) {
	    fprintf(stderr, "the name `%s' is not a tty\n", CL_ctty);
	    exit(1);
	}
	pflags = (pflags | PROC_TTY) & ~(PROC_ANYTTY|PROC_STAT|PROC_UID|PROC_PID);
	args = tty;
    }
    show_procs(cmdspc, do_header, pflags, args, N);
    return 0;
}

/***************************/
/* print a context dependent usage message and maybe exit */
static void usage(char* context) {
    fprintf(stderr,
	    "%s"
            "usage:  ps acehjlnrsSuvwx{t<tty>|#|O[-]u[-]U..} \\\n"
            "           --sort:[-]key1,[-]key2,...\n"
            "           --help gives you this message\n"
            "           --version prints version information\n",
	    context ? context : "");
    if (context)
	exit(1);	/* prevent bad exit status by calling usage(NULL) */
}

/*****************************/
/* set maximum chars displayed on a line based on screen size.
 * Always allow for the header, with n+1 lines of output per row.
 */
static void set_cmdspc(int n) {
    struct winsize win;
    int h = strlen(mode[CL_fmt].header),
	c = strlen("COMMAND");

    if (ioctl(1, TIOCGWINSZ, &win) != -1 && win.ws_col > 0)
	cmdspc = win.ws_col;
    if (n > 100) n = 100;	/* max of 100 'w' options */
    if (cmdspc > h)
	cmdspc = cmdspc*(n+1) - h + c;
    else
	cmdspc = cmdspc*n + c;
}

/*****************************/
static const char *ttyc(int tty, int pid){
  static char outbuf[8];
  if(CL_num_outp)
    snprintf(outbuf, sizeof outbuf, "%4.4x", tty);
  else
    dev_to_tty(outbuf, 4, tty, pid, ABBREV_TTY|ABBREV_DEV|ABBREV_PTS);
  return outbuf;
}

/*****************************/
/* This is the main driver routine that iterates over the process table.
 */
static void show_procs(unsigned maxcmd, int do_header, int pflags, void* args, int N) {
    static proc_t buf; /* less dynamic memory allocation when not sorting */
    PROCTAB* tab;
    proc_t **ptable = NULL, *next, *retbuf = NULL;
    int n = 0;

    /* initiate process table scan */
    tab = openproc(pflags, args, N);
    if (!tab) {
        fprintf(stderr, "Error: can not access /proc.\n");
        exit(1);
    }

    if (do_header) puts(mode[CL_fmt].header);	/* print header */

    if (!(CL_sort || CL_forest))	/* when sorting and forest are both */
	retbuf = &buf;			/* off we can use a static buffer */

    while ((next = readproc(tab,retbuf))) {	/* read next process */
	n++;					/* Now either: */
	if (CL_forest) {			/*    add process to tree */
	    /* FIXME: this static is disgusting, but to keep binary
	     * compatibility within the 1.2 series I'll just go from
	     * 256 bytes to 1024 bytes, which should be good enough
	     * for most people....  :-(  In the future, the format
	     * format function should probably dynamically allocate
	     * strings.
	     */
	    static char s[1024];
	    (mode[CL_fmt].format)(s, next);
	    if (CL_fmt != PS_V && CL_fmt != PS_M)
		show_time(s+strlen(s), next);
	    add_node(s, next);
	} else if (CL_sort) {			/*    add process to table */
	    ptable = xrealloc(ptable, n*sizeof(proc_t*));
	    ptable[n-1] = next;
	} else {				/*    or show it right away */
	    show_a_proc(&buf, maxcmd);
	    if (buf.cmdline) free((void*)(buf.cmdline[0]));
	    if (buf.environ) free((void*)(buf.environ[0]));
	}
    }
    if (!n) {
	fprintf(stderr, "No processes available.\n");
	exit(1);
    }
    if (CL_sort && !CL_forest) {	/* just print sorted table */
	int i;
	qsort(ptable, n, sizeof(proc_t*), (void*)mult_lvl_cmp);
	for (i = 0; i < n; i++) {
	    show_a_proc(ptable[i], maxcmd);
	    freeproc(ptable[i]);
	}
	free(ptable);
    } else if (CL_forest)
	show_forest();
}

/*****************************/
/* show the trailing command and environment in available space.
 * use abbreviated cmd if requested, NULL list, or singleton NULL string
 */
static void show_cmd_env(char* tskcmd, char** cmd, char** env, unsigned maxch) {
    if (CL_kern_comm)		/* no () when explicit request for tsk cmd */
	maxch = print_str(stdout, tskcmd, maxch);
    else if (!cmd || !*cmd || (!cmd[1] && !*cmd)) {
	/* no /proc//cmdline ==> bounding () */
	if (maxch) {
	    fputc('(', stdout);
	    maxch--;
	}
	maxch = print_str(stdout, tskcmd, maxch);
	if (maxch) {
	    fputc(')', stdout);
	    maxch--;
	}
    } else
	maxch = print_strlist(stdout, cmd, " ", maxch);
    if (CL_show_env && env)
	print_strlist(stdout, env, " ", maxch);
    fputc('\n', stdout);
}


/*****************************/
/* format a single process for output.
 */
static void show_a_proc(proc_t* p, unsigned maxch) {
    static char s[2048];
    (mode[CL_fmt].format)(s, p);
    if (CL_fmt != PS_V && CL_fmt != PS_M)
	show_time(s+strlen(s), p);
    printf("%s", s);
    show_cmd_env(p->cmd, p->cmdline, p->environ, maxch);
}

/****** The format functions for the various formatting modes follow *****/

/*****************************/
static void show_short(char *s, proc_t *p) {
    sprintf(s, "%5d %3s %s", p->pid, ttyc(p->tty,p->pid), status(p));
}


/*****************************/
static void show_long(char *s, proc_t *p) {
    char wchanb[32];
    
    if (GL_wchan_nout)
	sprintf(wchanb, " %-9lx ", p->wchan & 0xffffffff);
    else
	sprintf(wchanb, "%-11.11s", wchan(p->wchan));
    sprintf(s, "%6lx %5d %5d %5d %3ld %3ld %6ld %5ld %-11.11s %s%3s",
	    p->flags, p->euid, p->pid, p->ppid, p->priority, p->nice,
	    p->vsize >> 10, p->rss << (PAGE_SHIFT - 10), wchanb, status(p),
	    ttyc(p->tty,p->pid));
}


/*****************************/
static void show_jobs(char *s, proc_t *p) {
    sprintf(s, "%5d %5d %5d %5d %3s %5d  %s %5d ",
	    p->ppid, p->pid, p->pgrp, p->session, ttyc(p->tty,p->pid), p->tpgid, status(p),
	    p->euid);
}


/*****************************/
static void show_user(char *s, proc_t *p) {
    long pmem, total_time, seconds;
    time_t start;
    unsigned int pcpu;

    if (CL_num_outp)
	s += sprintf(s, "%5d    ", p->euid);
    else
	s += sprintf(s, "%-8s ", p->euser);
    /* Hertz, being UL, forces 64-bit calculation on 64-bit machines */
    seconds = (((GL_current_time * Hertz) - p->start_time) / Hertz);
    start = GL_time_now - seconds;
    total_time = (p->utime + p->stime +
		  (CL_Sum ? p->cutime + p->cstime : 0));
    pcpu = seconds ?
	(total_time * 10 * 100 / Hertz) / seconds :
	0;
    if (pcpu > 999) pcpu = 999;
    pmem = p->rss * 1000 / (kb_main_total >> (PAGE_SHIFT-10));
    sprintf(s, "%5d %2u.%u %2ld.%ld %5ld %5ld %3s %s %.6s ",
	    p->pid,  pcpu / 10, pcpu % 10,  pmem / 10, pmem % 10,
	    p->vsize >> 10, p->rss << (PAGE_SHIFT - 10), ttyc(p->tty,p->pid), status(p),
	    ctime(&start) + (GL_time_now - start > 3600*24 ? 4 : 10));
}


/*****************************/
/* TODO: this is broken... but does anyone care? */
static void show_sig(char *s, proc_t *p) {
#undef SIGNAL_STRING /* not filled in by the function oldps calls */
#ifdef SIGNAL_STRING
#define TAIL(s) ((s)+(long)strlen(s)-8)
    sprintf(s, "%5d %5d %s %s %s %s %s  %3s ",
	p->euid, p->pid,
	TAIL(p->signal), TAIL(p->blocked), TAIL(p->sigignore), TAIL(p->sigcatch),
	status(p), ttyc(p->tty,p->pid)
    );
#else
    sprintf(s, "%5d %5d %08x %08x %08x %08x %s  %3s ",
	p->euid, p->pid,
	(unsigned)p->signal, (unsigned)p->blocked, (unsigned)p->sigignore, (unsigned)p->sigcatch,
	status(p), ttyc(p->tty,p->pid)
    );
#endif
}


/*****************************/
static void show_vm(char *s, proc_t *p) {
    int pmem;

    s += sprintf(s,"%5d %3s %s", p->pid, ttyc(p->tty,p->pid), status(p));
    show_time(s, p);
    s += strlen(s);
    s += sprintf(s, " %6ld %4ld %4ld %4ld ",
		 p->maj_flt + (CL_Sum ? p->cmaj_flt : 0),
		 p->vsize ? (p->end_code - p->start_code) >> 10 : 0,
		 p->vsize ? (p->vsize - p->end_code + p->start_code) >> 10 : 0,
		 p->rss << (PAGE_SHIFT - 10));
    if(p->rss_rlim == RLIM_INFINITY)
	s += sprintf(s, "   xx ");
    else
	s += sprintf(s, "%5ld ", p->rss_rlim >> 10);
    pmem = p->rss * 1000 / (kb_main_total >> (PAGE_SHIFT-10));
    sprintf(s, "%2d.%d ", pmem / 10, pmem % 10);
}



/*****************************/
static void show_m(char *s, proc_t *p) {
    sprintf(s, "%5d %3s %6ld %6ld %5ld %5ld %5ld %5ld %5ld %5ld %5ld %3ld ", 
	    p->pid, ttyc(p->tty,p->pid),
	    p->maj_flt + (CL_Sum ? p->cmaj_flt : 0),
	    p->min_flt + (CL_Sum ? p->cmin_flt : 0),
	    p->trs << CL_pg_shift,
	    p->drs << CL_pg_shift,
	    p->size << CL_pg_shift,
	    (p->size - p->resident) << CL_pg_shift,
	    p->resident << CL_pg_shift,
	    p->share << CL_pg_shift,
	    p->lrs << CL_pg_shift,
	    p->dt);
}


/*****************************/
static void show_regs(char *s, proc_t *p) {
    char time1[16], time2[16];

    sprintf(s, "%2ld %5d %8lx %8lx %8lx %s %s %s  %3s ",
	    p->start_code >> 26, p->pid, p->start_stack,
	    p->kstk_esp, p->kstk_eip,
	    prtime(time1, p->timeout, GL_current_time*Hertz),
	    prtime(time2, p->it_real_value, 0),
	    status(p), ttyc(p->tty,p->pid));
}


/*****************************/
static char *prtime(char *s, unsigned long t, unsigned long rel) {
    if (t == 0) {
        sprintf(s, "     ");
        return s;
    }
    if ((long) t == -1) {
        sprintf(s, "   xx");
        return s;
    }
    if ((long) (t -= rel) < 0)
        t = 0;
    if (t > 9999)
        sprintf(s, "%5lu", t / 100);
    else
        sprintf(s, "%2lu.%02lu", t / 100, t % 100);
    return s;
}


/*****************************/
static void show_time(char *s, proc_t * p) {
    unsigned t;
    t = (p->utime + p->stime) / Hertz;
    if (CL_Sum) t += (p->cutime + p->cstime) / Hertz;
    sprintf(s, "%3d:%02d ", t / 60, t % 60);
}



/*****************************/
/* fancy process family tree based cmdline printing.  Building the tree
   should be relegated to libproc and only the printing logic should
   remain here.
*/
static struct tree_node * node;  /* forest mode globals */
static int      nodes = 0;
static int      maxnodes = 0;

/*****************************/

static void add_node(char *s, proc_t *task) {
    if (maxnodes == 0) {
	maxnodes = 64;
        node = (struct tree_node *)
            xmalloc(sizeof(struct tree_node) * maxnodes);
    }
    if (nodes >= maxnodes) {
	maxnodes *= 2;
        node = (struct tree_node *)
            xrealloc(node, sizeof(struct tree_node) * maxnodes);
    }
    node[nodes].proc        = task;
    node[nodes].pid         = task->pid;
    node[nodes].ppid        = task->ppid;
    node[nodes].line        = strdup(s);
    node[nodes].cmd         = task->cmd;
    node[nodes].cmdline     = task->cmdline;
    node[nodes].environ     = task->environ;
    node[nodes].children    = 0;
    node[nodes].have_parent = 0;
    nodes++;
}


/*****************************/
static int node_cmp(const void *s1, const void *s2) {
    struct tree_node *n1 = (struct tree_node *) s1;
    struct tree_node *n2 = (struct tree_node *) s2;
    return n1->pid - n2->pid;
}


/*****************************/
static void show_tree(int n, int depth, char *continued) {
    int i, cols = 0;

    fprintf(stdout, "%s", node[n].line);
    for (i = 0; i < depth; i++) {
        if (cols + 4 >= cmdspc - 1)
            break; 
        if (i == depth - 1)
            printf(" \\_ ");
        else if (continued[i])
            printf(" |  ");
        else
            printf("    ");
        cols += 4;
    }
    show_cmd_env(node[n].cmd, node[n].cmdline, node[n].environ, cmdspc - cols);
    for (i = 0; i < node[n].children; i++) {
        continued[depth] = i != node[n].children - 1;
        show_tree(node[n].child[i], depth + 1, continued);
    }
}


/*****************************/
static void show_forest() {
    register int i, j;
    int parent;
    char continued[1024];

    if (CL_sort)
	qsort((void*)node, nodes, sizeof(struct tree_node), (void*)node_mult_lvl_cmp);

    for (i = 0; i < nodes; i++) {
        if (node[i].ppid > 1 && node[i].pid != node[i].ppid) {
	    parent = -1;
	    for (j=0; j<nodes; j++)
		if (node[j].pid==node[i].ppid)
		    parent = j;
        } else
            parent = -1;
        if (parent >= 0) {
            node[i].have_parent++;
            if (node[parent].children == 0) {
                node[parent].child = (int*)xmalloc(16 * sizeof(int*));
                node[parent].maxchildren = 16;
            }
            else if (node[parent].children == node[parent].maxchildren) {
                node[parent].maxchildren *= 2;
                node[parent].child = (int*)xrealloc(node[parent].child,
						    node[parent].maxchildren
						    * sizeof(int*));
            }
            node[parent].child[node[parent].children++] = i;
        }
    }

    for (i = 0; i < nodes; i++) {
        if (!node[i].have_parent)
            show_tree(i, 0, continued);
    }
}
