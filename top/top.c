/* top.c - Source file:         show Linux processes */
/*
 * Copyright (c) 2002-2019, by: James C. Warner
 *
 * This file may be used subject to the terms and conditions of the
 * GNU Library General Public License Version 2, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 */
/* For contributions to this program, the author wishes to thank:
 *    Craig Small, <csmall@small.dropbear.id.au>
 *    Albert D. Cahalan, <albert@users.sf.net>
 *    Sami Kerola, <kerolasa@iki.fi>
 */

#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>            // foul sob, defines all sorts of stuff...
#undef    raw
#undef    tab
#undef    TTY
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/select.h>      // also available via <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>       // also available via <stdlib.h>

#include "../include/fileutils.h"
#include "../include/nls.h"

#include "../proc/alloc.h"
#include "../proc/devname.h"
#include "../proc/numa.h"
#include "../proc/procps.h"
#include "../proc/readproc.h"
#include "../proc/sig.h"
#include "../proc/sysinfo.h"
#include "../proc/version.h"
#include "../proc/wchan.h"
#include "../proc/whattime.h"

#include "top.h"
#include "top_nls.h"


/*######  Miscellaneous global stuff  ####################################*/

        /* The original and new terminal definitions
           (only set when not in 'Batch' mode) */
static struct termios Tty_original,    // our inherited terminal definition
#ifdef TERMIOS_ONLY
                      Tty_tweaked,     // for interactive 'line' input
#endif
                      Tty_raw;         // for unsolicited input
static int Ttychanged = 0;

        /* Last established cursor state/shape */
static const char *Cursor_state = "";

        /* Program name used in error messages and local 'rc' file name */
static char *Myname;

        /* Our constant sigset, so we need initialize it but once */
static sigset_t Sigwinch_set;

        /* The 'local' config file support */
static char  Rc_name [OURPATHSZ];
static RCF_t Rc = DEF_RCFILE;
static int   Rc_questions;

        /* The run-time acquired page stuff */
static unsigned Pg2K_shft = 0;

        /* SMP, Irix/Solaris mode, Linux 2.5.xx support */
static CPU_t      *Cpu_tics;
static int         Cpu_faux_tot;
static float       Cpu_pmax;
static const char *Cpu_States_fmts;

        /* Specific process id monitoring support */
static pid_t Monpids [MONPIDMAX+1] = { 0 };
static int   Monpidsidx = 0;

        /* Current screen dimensions.
           note: the number of processes displayed is tracked on a per window
                 basis (see the WIN_t).  Max_lines is the total number of
                 screen rows after deducting summary information overhead. */
        /* Current terminal screen size. */
static int Screen_cols, Screen_rows, Max_lines;

        /* This is really the number of lines needed to display the summary
           information (0 - nn), but is used as the relative row where we
           stick the cursor between frames. */
static int Msg_row;

        /* Global/Non-windows mode stuff that is NOT persistent */
static int Batch = 0,           // batch mode, collect no input, dumb output
           Loops = -1,          // number of iterations, -1 loops forever
           Secure_mode = 0,     // set if some functionality restricted
           Thread_mode = 0,     // set w/ 'H' - show threads via readeither()
           Width_mode = 0;      // set w/ 'w' - potential output override

        /* Unchangeable cap's stuff built just once (if at all) and
           thus NOT saved in a WIN_t's RCW_t.  To accommodate 'Batch'
           mode, they begin life as empty strings so the overlying
           logic need not change ! */
static char  Cap_clr_eol    [CAPBUFSIZ] = "",    // global and/or static vars
             Cap_nl_clreos  [CAPBUFSIZ] = "",    // are initialized to zeros!
             Cap_clr_scr    [CAPBUFSIZ] = "",    // the assignments used here
             Cap_curs_norm  [CAPBUFSIZ] = "",    // cost nothing but DO serve
             Cap_curs_huge  [CAPBUFSIZ] = "",    // to remind people of those
             Cap_curs_hide  [CAPBUFSIZ] = "",    // batch requirements!
             Cap_clr_eos    [CAPBUFSIZ] = "",
             Cap_home       [CAPBUFSIZ] = "",
             Cap_norm       [CAPBUFSIZ] = "",
             Cap_reverse    [CAPBUFSIZ] = "",
             Caps_off       [CAPBUFSIZ] = "",
             Caps_endline   [CAPBUFSIZ] = "";
#ifndef RMAN_IGNORED
static char  Cap_rmam       [CAPBUFSIZ] = "",
             Cap_smam       [CAPBUFSIZ] = "";
        /* set to 1 if writing to the last column would be troublesome
           (we don't distinguish the lowermost row from the other rows) */
static int   Cap_avoid_eol = 0;
#endif
static int   Cap_can_goto = 0;

        /* Some optimization stuff, to reduce output demands...
           The Pseudo_ guys are managed by adj_geometry and frame_make.  They
           are exploited in a macro and represent 90% of our optimization.
           The Stdout_buf is transparent to our code and regardless of whose
           buffer is used, stdout is flushed at frame end or if interactive. */
static char  *Pseudo_screen;
static int    Pseudo_row = PROC_XTRA;
static size_t Pseudo_size;
#ifndef OFF_STDIOLBF
        // less than stdout's normal buffer but with luck mostly '\n' anyway
static char  Stdout_buf[2048];
#endif

        /* Our four WIN_t's, and which of those is considered the 'current'
           window (ie. which window is associated with any summ info displayed
           and to which window commands are directed) */
static WIN_t  Winstk [GROUPSMAX];
static WIN_t *Curwin;

        /* Frame oriented stuff that can't remain local to any 1 function
           and/or that would be too cumbersome managed as parms,
           and/or that are simply more efficiently handled as globals
           [ 'Frames_...' (plural) stuff persists beyond 1 frame ]
           [ or are used in response to async signals received ! ] */
static volatile int Frames_signal;     // time to rebuild all column headers
static          int Frames_libflags;   // PROC_FILLxxx flags
static int          Frame_maxtask;     // last known number of active tasks
                                       // ie. current 'size' of proc table
static float        Frame_etscale;     // so we can '*' vs. '/' WHEN 'pcpu'
static unsigned     Frame_running,     // state categories for this frame
                    Frame_sleepin,
                    Frame_stopped,
                    Frame_zombied;
static int          Frame_srtflg,      // the subject window's sort direction
                    Frame_ctimes,      // the subject window's ctimes flag
                    Frame_cmdlin;      // the subject window's cmdlin flag

        /* Support for 'history' processing so we can calculate %cpu */
static int    HHist_siz;               // max number of HST_t structs
static HST_t *PHist_sav,               // alternating 'old/new' HST_t anchors
             *PHist_new;
#ifndef OFF_HST_HASH
#define       HHASH_SIZ  1024
static int    HHash_one [HHASH_SIZ],   // actual hash tables ( hereafter known
              HHash_two [HHASH_SIZ],   // as PHash_sav/PHash_new )
              HHash_nul [HHASH_SIZ];   // 'empty' hash table image
static int   *PHash_sav = HHash_one,   // alternating 'old/new' hash tables
             *PHash_new = HHash_two;
#endif

        /* Support for automatically sized fixed-width column expansions.
         * (hopefully, the macros help clarify/document our new 'feature') */
static int Autox_array [EU_MAXPFLGS],
           Autox_found;
#define AUTOX_NO      EU_MAXPFLGS
#define AUTOX_COL(f)  if (EU_MAXPFLGS > f && f >= 0) Autox_array[f] = Autox_found = 1
#define AUTOX_MODE   (0 > Rc.fixed_widest)

        /* Support for scale_mem and scale_num (to avoid duplication. */
#ifdef CASEUP_SUFIX                                                // nls_maybe
   static char Scaled_sfxtab[] =  { 'K', 'M', 'G', 'T', 'P', 'E', 0 };
#else                                                              // nls_maybe
   static char Scaled_sfxtab[] =  { 'k', 'm', 'g', 't', 'p', 'e', 0 };
#endif

        /* Support for NUMA Node display and node expansion/targeting */
#ifndef OFF_STDERROR
static int Stderr_save = -1;
#endif
static int Numa_node_tot;
static int Numa_node_sel = -1;

        /* Support for Graphing of the View_STATES ('t') and View_MEMORY ('m')
           commands -- which are now both 4-way toggles */
#define GRAPH_prefix  25     // beginning text + opening '['
#define GRAPH_actual  100    // the actual bars or blocks
#define GRAPH_suffix  2      // ending ']' + trailing space
static float Graph_adj;      // bars/blocks scaling factor
static int   Graph_len;      // scaled length (<= GRAPH_actual)
static const char Graph_blks[] = "                                                                                                    ";
static const char Graph_bars[] = "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||";

        /* Support for 'Other Filters' in the configuration file */
static const char Osel_delim_1_txt[] = "begin: saved other filter data -------------------\n";
static const char Osel_delim_2_txt[] = "end  : saved other filter data -------------------\n";
static const char Osel_window_fmts[] = "window #%d, osel_tot=%d\n";
#define OSEL_FILTER   "filter="
static const char Osel_filterO_fmt[] = "\ttype=%d,\t" OSEL_FILTER "%s\n";
static const char Osel_filterI_fmt[] = "\ttype=%d,\t" OSEL_FILTER "%*s\n";

/*######  Sort callbacks  ################################################*/

        /*
         * These happen to be coded in the enum identifier alphabetic order,
         * not the order of the enum 'pflgs' value.  Also note that a callback
         * routine may serve more than one column.
         */

SCB_STRS(CGN, cgname)
SCB_STRS(CGR, cgroup[0])
SCB_STRV(CMD, Frame_cmdlin, cmdline, cmd)
SCB_NUM1(COD, trs)
SCB_NUMx(CPN, processor)
SCB_NUM1(CPU, pcpu)
SCB_NUM1(DAT, drs)
SCB_NUM1(DRT, dt)
SCB_STRS(ENV, environ[0])
SCB_NUM1(FL1, maj_flt)
SCB_NUM1(FL2, min_flt)
SCB_NUM1(FLG, flags)
SCB_NUM1(FV1, maj_delta)
SCB_NUM1(FV2, min_delta)
SCB_NUMx(GID, egid)
SCB_STRS(GRP, egroup)
SCB_STRS(LXC, lxcname)
SCB_NUMx(NCE, nice)
static int SCB_NAME(NMA) (const proc_t **P, const proc_t **Q) {
   /* this is a terrible cost to pay for sorting on numa nodes, but it's
      necessary if we're to avoid ABI breakage via changes to the proc_t */
   int p = numa_node_of_cpu((*P)->processor);
   int q = numa_node_of_cpu((*Q)->processor);
   return Frame_srtflg * ( q - p );
}
SCB_NUM1(NS1, ns[IPCNS])
SCB_NUM1(NS2, ns[MNTNS])
SCB_NUM1(NS3, ns[NETNS])
SCB_NUM1(NS4, ns[PIDNS])
SCB_NUM1(NS5, ns[USERNS])
SCB_NUM1(NS6, ns[UTSNS])
SCB_NUM1(OOA, oom_adj)
SCB_NUM1(OOM, oom_score)
SCB_NUMx(PGD, pgrp)
SCB_NUMx(PID, tid)
SCB_NUMx(PPD, ppid)
SCB_NUMx(PRI, priority)
SCB_NUM1(RES, resident)                // also serves MEM !
SCB_NUM1(RZA, vm_rss_anon)
SCB_NUM1(RZF, vm_rss_file)
SCB_NUM1(RZL, vm_lock)
SCB_NUM1(RZS, vm_rss_shared)
SCB_STRX(SGD, supgid)
SCB_STRS(SGN, supgrp)
SCB_NUM1(SHR, share)
SCB_NUM1(SID, session)
SCB_NUMx(STA, state)
SCB_NUM1(SWP, vm_swap)
SCB_NUMx(TGD, tgid)
SCB_NUMx(THD, nlwp)
                                       // also serves TM2 !
static int SCB_NAME(TME) (const proc_t **P, const proc_t **Q) {
   if (Frame_ctimes) {
      if (((*P)->cutime + (*P)->cstime + (*P)->utime + (*P)->stime)
        < ((*Q)->cutime + (*Q)->cstime + (*Q)->utime + (*Q)->stime))
           return SORT_lt;
      if (((*P)->cutime + (*P)->cstime + (*P)->utime + (*P)->stime)
        > ((*Q)->cutime + (*Q)->cstime + (*Q)->utime + (*Q)->stime))
           return SORT_gt;
   } else {
      if (((*P)->utime + (*P)->stime) < ((*Q)->utime + (*Q)->stime))
         return SORT_lt;
      if (((*P)->utime + (*P)->stime) > ((*Q)->utime + (*Q)->stime))
         return SORT_gt;
   }
   return SORT_eq;
}
SCB_NUM1(TPG, tpgid)
SCB_NUMx(TTY, tty)
SCB_NUMx(UED, euid)
SCB_STRS(UEN, euser)
SCB_NUMx(URD, ruid)
SCB_STRS(URN, ruser)
SCB_NUMx(USD, suid)
SCB_NUM2(USE, vm_rss, vm_swap)
SCB_STRS(USN, suser)
SCB_NUM1(VRT, size)
SCB_NUM1(WCH, wchan)

#ifdef OFF_HST_HASH
        /* special sort for procs_hlp() ! ------------------------ */
static int sort_HST_t (const HST_t *P, const HST_t *Q) {
   return P->pid - Q->pid;
}
#endif

/*######  Tiny useful routine(s)  ########################################*/

        /*
         * This routine simply formats whatever the caller wants and
         * returns a pointer to the resulting 'const char' string... */
static const char *fmtmk (const char *fmts, ...) __attribute__((format(printf,1,2)));
static const char *fmtmk (const char *fmts, ...) {
   static char buf[BIGBUFSIZ];          // with help stuff, our buffer
   va_list va;                          // requirements now exceed 1k

   va_start(va, fmts);
   vsnprintf(buf, sizeof(buf), fmts, va);
   va_end(va);
   return (const char *)buf;
} // end: fmtmk


        /*
         * This guy is just our way of avoiding the overhead of the standard
         * strcat function (should the caller choose to participate) */
static inline char *scat (char *dst, const char *src) {
   while (*dst) dst++;
   while ((*(dst++) = *(src++)));
   return --dst;
} // end: scat


        /*
         * This guy just facilitates Batch and protects against dumb ttys
         * -- we'd 'inline' him but he's only called twice per frame,
         * yet used in many other locations. */
static const char *tg2 (int x, int y) {
   // it's entirely possible we're trying for an invalid row...
   return Cap_can_goto ? tgoto(cursor_address, x, y) : "";
} // end: tg2

/*######  Exit/Interrput routines  #######################################*/

        /*
         * Reset the tty, if necessary */
static void at_eoj (void) {
   if (Ttychanged) {
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &Tty_original);
      if (keypad_local) putp(keypad_local);
      putp(tg2(0, Screen_rows));
      putp("\n");
#ifdef OFF_SCROLLBK
      if (exit_ca_mode) {
         // this next will also replace top's most recent screen with the
         // original display contents that were visible at our invocation
         putp(exit_ca_mode);
      }
#endif
      putp(Cap_curs_norm);
      putp(Cap_clr_eol);
#ifndef RMAN_IGNORED
      putp(Cap_smam);
#endif
   }
   fflush(stdout);
#ifndef OFF_STDERROR
   /* we gotta reverse the stderr redirect which was employed during start up
      and needed because the two libnuma 'weak' functions were useless to us! */
   if (-1 < Stderr_save) {
      dup2(Stderr_save, fileno(stderr));
      close(Stderr_save);
      Stderr_save = -1;      // we'll be ending soon anyway but what the heck
   }
#endif
} // end: at_eoj


        /*
         * The real program end */
static void bye_bye (const char *str) NORETURN;
static void bye_bye (const char *str) {
   sigset_t ss;

// POSIX.1-2004 async-signal-safe: sigfillset, sigprocmask
   sigfillset(&ss);
   sigprocmask(SIG_BLOCK, &ss, NULL);
   at_eoj();                 // restore tty in preparation for exit
#ifdef ATEOJ_RPTSTD
{  proc_t *p;
   if (!str && !Frames_signal && Ttychanged) { fprintf(stderr,
      "\n%s's Summary report:"
      "\n\tProgram"
      "\n\t   %s"
      "\n\t   Hertz = %u (%u bytes, %u-bit time)"
      "\n\t   page_bytes = %d, Cpu_faux_tot = %d, smp_num_cpus = %d"
      "\n\t   sizeof(CPU_t) = %u, sizeof(HST_t) = %u (%d HST_t's/Page), HHist_siz = %u"
      "\n\t   sizeof(proc_t) = %u, sizeof(proc_t.cmd) = %u, sizeof(proc_t *) = %u"
      "\n\t   Frames_libflags = %08lX"
      "\n\t   SCREENMAX = %u, ROWMINSIZ = %u, ROWMAXSIZ = %u"
      "\n\t   PACKAGE = '%s', LOCALEDIR = '%s'"
      "\n\tTerminal: %s"
      "\n\t   device = %s, ncurses = v%s"
      "\n\t   max_colors = %d, max_pairs = %d"
      "\n\t   Cap_can_goto = %s"
      "\n\t   Screen_cols = %d, Screen_rows = %d"
      "\n\t   Max_lines = %d, most recent Pseudo_size = %u"
#ifndef OFF_STDIOLBF
      "\n\t   Stdout_buf = %u, BUFSIZ = %u"
#endif
      "\n\tWindows and Curwin->"
      "\n\t   sizeof(WIN_t) = %u, GROUPSMAX = %d"
      "\n\t   winname = %s, grpname = %s"
#ifdef CASEUP_HEXES
      "\n\t   winflags = %08X, maxpflgs = %d"
#else
      "\n\t   winflags = %08x, maxpflgs = %d"
#endif
      "\n\t   sortindx = %d, fieldscur = %s"
      "\n\t   maxtasks = %d, varcolsz = %d, winlines = %d"
      "\n\t   strlen(columnhdr) = %d"
      "\n"
      , __func__
      , PACKAGE_STRING
      , (unsigned)Hertz, (unsigned)sizeof(Hertz), (unsigned)sizeof(Hertz) * 8
      , (int)page_bytes, Cpu_faux_tot, (int)smp_num_cpus, (unsigned)sizeof(CPU_t)
      , (unsigned)sizeof(HST_t), ((int)page_bytes / (int)sizeof(HST_t)), HHist_siz
      , (unsigned)sizeof(proc_t), (unsigned)sizeof(p->cmd), (unsigned)sizeof(proc_t *)
      , (long)Frames_libflags
      , (unsigned)SCREENMAX, (unsigned)ROWMINSIZ, (unsigned)ROWMAXSIZ
      , PACKAGE, LOCALEDIR
#ifdef PRETENDNOCAP
      , "dumb"
#else
      , termname()
#endif
      , ttyname(STDOUT_FILENO), NCURSES_VERSION
      , max_colors, max_pairs
      , Cap_can_goto ? "yes" : "No!"
      , Screen_cols, Screen_rows
      , Max_lines, (unsigned)Pseudo_size
#ifndef OFF_STDIOLBF
      , (unsigned)sizeof(Stdout_buf), (unsigned)BUFSIZ
#endif
      , (unsigned)sizeof(WIN_t), GROUPSMAX
      , Curwin->rc.winname, Curwin->grpname
      , Curwin->rc.winflags, Curwin->maxpflgs
      , Curwin->rc.sortindx, Curwin->rc.fieldscur
      , Curwin->rc.maxtasks, Curwin->varcolsz, Curwin->winlines
      , (int)strlen(Curwin->columnhdr)
      );
   }
}
#endif // end: ATEOJ_RPTSTD

#ifndef OFF_HST_HASH
#ifdef ATEOJ_RPTHSH
   if (!str && !Frames_signal && Ttychanged) {
      int i, j, pop, total_occupied, maxdepth, maxdepth_sav, numdepth
         , cross_foot, sz = HHASH_SIZ * (unsigned)sizeof(int);
      int depths[HHASH_SIZ];

      for (i = 0, total_occupied = 0, maxdepth = 0; i < HHASH_SIZ; i++) {
         int V = PHash_new[i];
         j = 0;
         if (-1 < V) {
            ++total_occupied;
            while (-1 < V) {
               V = PHist_new[V].lnk;
               if (-1 < V) j++;
            }
         }
         depths[i] = j;
         if (maxdepth < j) maxdepth = j;
      }
      maxdepth_sav = maxdepth;

      fprintf(stderr,
         "\n%s's Supplementary HASH report:"
         "\n\tTwo Tables providing for %d entries each + 1 extra for 'empty' image"
         "\n\t%dk (%d bytes) per table, %d total bytes (including 'empty' image)"
         "\n\tResults from latest hash (PHash_new + PHist_new)..."
         "\n"
         "\n\tTotal hashed = %d"
         "\n\tLevel-0 hash entries = %d (%d%% occupied)"
         "\n\tMax Depth = %d"
         "\n\n"
         , __func__
         , HHASH_SIZ, sz / 1024, sz, sz * 3
         , Frame_maxtask
         , total_occupied, (total_occupied * 100) / HHASH_SIZ
         , maxdepth + 1);

      if (total_occupied) {
         for (pop = total_occupied, cross_foot = 0; maxdepth; maxdepth--) {
            for (i = 0, numdepth = 0; i < HHASH_SIZ; i++)
               if (depths[i] == maxdepth) ++numdepth;
            fprintf(stderr,
               "\t %5d (%3d%%) hash table entries at depth %d\n"
               , numdepth, (numdepth * 100) / total_occupied, maxdepth + 1);
            pop -= numdepth;
            cross_foot += numdepth;
            if (0 == pop && cross_foot == total_occupied) break;
         }
         if (pop) {
            fprintf(stderr, "\t %5d (%3d%%) unchained hash table entries\n"
               , pop, (pop * 100) / total_occupied);
            cross_foot += pop;
         }
         fprintf(stderr,
            "\t -----\n"
            "\t %5d total entries occupied\n", cross_foot);

         if (maxdepth_sav > 1) {
            fprintf(stderr, "\nPIDs at max depth: ");
            for (i = 0; i < HHASH_SIZ; i++)
               if (depths[i] == maxdepth_sav) {
                  j = PHash_new[i];
                  fprintf(stderr, "\n\tpos %4d:  %05d", i, PHist_new[j].pid);
                  while (-1 < j) {
                     j = PHist_new[j].lnk;
                     if (-1 < j) fprintf(stderr, ", %05d", PHist_new[j].pid);
                  }
               }
            fprintf(stderr, "\n");
         }
      }
   }
#endif // end: ATEOJ_RPTHSH
#endif // end: OFF_HST_HASH

   numa_uninit();
   if (str) {
      fputs(str, stderr);
      exit(EXIT_FAILURE);
   }
   if (Batch) fputs("\n", stdout);
   exit(EXIT_SUCCESS);
} // end: bye_bye


        /*
         * Standard error handler to normalize the look of all err output */
static void error_exit (const char *str) NORETURN;
static void error_exit (const char *str) {
   static char buf[MEDBUFSIZ];

   /* we'll use our own buffer so callers can still use fmtmk() and, after
      twelve long years, 2013 was the year we finally eliminated the leading
      tab character -- now our message can get lost in screen clutter too! */
   snprintf(buf, sizeof(buf), "%s: %s\n", Myname, str);
   bye_bye(buf);
} // end: error_exit


        /*
         * Catches all remaining signals not otherwise handled */
static void sig_abexit (int sig) {
   sigset_t ss;

// POSIX.1-2004 async-signal-safe: sigfillset, sigprocmask, signal, raise
   sigfillset(&ss);
   sigprocmask(SIG_BLOCK, &ss, NULL);
   at_eoj();                 // restore tty in preparation for exit
   fprintf(stderr, N_fmt(EXIT_signals_fmt)
      , sig, signal_number_to_name(sig), Myname);
   signal(sig, SIG_DFL);     // allow core dumps, if applicable
   raise(sig);               // ( plus set proper return code )
   _exit(sig | 0x80);        // if default sig action is ignore
} // end: sig_abexit


        /*
         * Catches:
         *    SIGALRM, SIGHUP, SIGINT, SIGPIPE, SIGQUIT, SIGTERM,
         *    SIGUSR1 and SIGUSR2 */
static void sig_endpgm (int dont_care_sig) NORETURN;
static void sig_endpgm (int dont_care_sig) {
   bye_bye(NULL);
   (void)dont_care_sig;
} // end: sig_endpgm


        /*
         * Catches:
         *    SIGTSTP, SIGTTIN and SIGTTOU */
static void sig_paused (int dont_care_sig) {
// POSIX.1-2004 async-signal-safe: tcsetattr, tcdrain, raise
   if (-1 == tcsetattr(STDIN_FILENO, TCSAFLUSH, &Tty_original))
      error_exit(fmtmk(N_fmt(FAIL_tty_set_fmt), strerror(errno)));
   if (keypad_local) putp(keypad_local);
   putp(tg2(0, Screen_rows));
   putp(Cap_curs_norm);
#ifndef RMAN_IGNORED
   putp(Cap_smam);
#endif
   // tcdrain(STDOUT_FILENO) was not reliable prior to ncurses-5.9.20121017,
   // so we'll risk POSIX's wrath with good ol' fflush, lest 'Stopped' gets
   // co-mingled with our most recent output...
   fflush(stdout);
   raise(SIGSTOP);
   // later, after SIGCONT...
   if (-1 == tcsetattr(STDIN_FILENO, TCSAFLUSH, &Tty_raw))
      error_exit(fmtmk(N_fmt(FAIL_tty_set_fmt), strerror(errno)));
#ifndef RMAN_IGNORED
   putp(Cap_rmam);
#endif
   if (keypad_xmit) putp(keypad_xmit);
   putp(Cursor_state);
   Frames_signal = BREAK_sig;
   (void)dont_care_sig;
} // end: sig_paused


        /*
         * Catches:
         *    SIGCONT and SIGWINCH */
static void sig_resize (int dont_care_sig) {
// POSIX.1-2004 async-signal-safe: tcdrain
   tcdrain(STDOUT_FILENO);
   Frames_signal = BREAK_sig;
   (void)dont_care_sig;
} // end: sig_resize


        /*
         * Handles libproc memory errors, so our tty can be reset */
static void xalloc_our_handler (const char *fmts, ...) {
   static char buf[MEDBUFSIZ];
   va_list va;

   va_start(va, fmts);
   vsnprintf(buf, sizeof(buf), fmts, va);
   va_end(va);
   scat(buf, "\n");
   bye_bye(buf);
} // end: xalloc_our_handler

/*######  Special UTF-8 Multi-Byte support  ##############################*/

        /* Support for NLS translated multi-byte strings */
static char UTF8_tab[] = {
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x00 - 0x0F
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x10 - 0x1F
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x20 - 0x2F
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x30 - 0x3F
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x40 - 0x4F
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x50 - 0x5F
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x60 - 0x6F
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x70 - 0x7F
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0x80 - 0x8F
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0x90 - 0x9F
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0xA0 - 0xAF
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0xB0 - 0xBF
  -1,-1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // 0xC0 - 0xCF, 0xC2 = begins 2
   2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // 0xD0 - 0xDF
   3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, // 0xE0 - 0xEF, 0xE0 = begins 3
   4, 4, 4, 4, 4,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, // 0xF0 - 0xFF, 0xF0 = begins 4
};                                                 //            ( 0xF5 & beyond invalid )


        /*
         * Accommodate any potential differences between some multibyte
         * character sequence and the screen columns needed to print it */
static inline int utf8_cols (const unsigned char *p, int n) {
#ifndef OFF_XTRAWIDE
   wchar_t wc;

   if (n > 1) {
      (void)mbtowc(&wc, (const char *)p, n);
      if ((n = wcwidth(wc)) < 1) n = 1;
   }
   return n;
#else
   (void)p; (void)n;
   return 1;
#endif
} // end: utf8_cols


        /*
         * Determine difference between total bytes versus printable
         * characters in that passed, potentially multi-byte, string */
static int utf8_delta (const char *str) {
   const unsigned char *p = (const unsigned char *)str;
   int clen, cnum = 0;

   while (*p) {
      // -1 represents a decoding error, pretend it's untranslated ...
      if (0 > (clen = UTF8_tab[*p])) return 0;
      cnum += utf8_cols(p, clen);
      p += clen;
   }
   return (int)((const char *)p - str) - cnum;
} // end: utf8_delta


        /*
         * Determine a physical end within a potential multi-byte string
         * where maximum printable chars could be accommodated in width */
static int utf8_embody (const char *str, int width) {
   const unsigned char *p = (const unsigned char *)str;
   int clen, cnum = 0;

   if (width > 0) {
      while (*p) {
         // -1 represents a decoding error, pretend it's untranslated ...
         if (0 > (clen = UTF8_tab[*p])) return width;
         if (width < (cnum += utf8_cols(p, clen))) break;
         p += clen;
      }
   }
   return (int)((const char *)p - str);
} // end: utf8_embody


        /*
         * Like the regular justify_pad routine but this guy
         * can accommodate the multi-byte translated strings */
static const char *utf8_justify (const char *str, int width, int justr) {
   static char l_fmt[]  = "%-*.*s%s", r_fmt[] = "%*.*s%s";
   static char buf[SCREENMAX];
   char tmp[SCREENMAX];

   snprintf(tmp, sizeof(tmp), "%.*s", utf8_embody(str, width), str);
   width += utf8_delta(tmp);
   snprintf(buf, sizeof(buf), justr ? r_fmt : l_fmt, width, width, tmp, COLPADSTR);
   return buf;
} // end: utf8_justify


        /*
         * Returns a physical or logical column number given a
         * multi-byte string and a target column value */
static int utf8_proper_col (const char *str, int col, int tophysical) {
   const unsigned char *p = (const unsigned char *)str;
   int clen, tlen = 0, cnum = 0;

   while (*p) {
      // -1 represents a decoding error, don't encourage repositioning ...
      if (0 > (clen = UTF8_tab[*p])) return col;
      if (cnum + 1 > col && tophysical) break;
      p += clen;
      tlen += clen;
      if (tlen > col && !tophysical) break;
      ++cnum;
   }
   return tophysical ? tlen : cnum;
} // end: utf8_proper_col

/*######  Misc Color/Display support  ####################################*/

        /*
         * Make the appropriate caps/color strings for a window/field group.
         * note: we avoid the use of background color so as to maximize
         *       compatibility with the user's xterm settings */
static void capsmk (WIN_t *q) {
   /* macro to test if a basic (non-color) capability is valid
         thanks: Floyd Davidson <floyd@ptialaska.net> */
 #define tIF(s)  s ? s : ""
   /* macro to make compatible with netbsd-curses too
         thanks: rofl0r <retnyg@gmx.net> */
 #define tPM(a,b) tparm(a, b, 0, 0, 0, 0, 0, 0, 0, 0)
   static int capsdone = 0;

   // we must NOT disturb our 'empty' terminfo strings!
   if (Batch) return;

   // these are the unchangeable puppies, so we only do 'em once
   if (!capsdone) {
      STRLCPY(Cap_clr_eol, tIF(clr_eol))
      STRLCPY(Cap_clr_eos, tIF(clr_eos))
      STRLCPY(Cap_clr_scr, tIF(clear_screen))
      // due to the leading newline, the following must be used with care
      snprintf(Cap_nl_clreos, sizeof(Cap_nl_clreos), "\n%s", tIF(clr_eos));
      STRLCPY(Cap_curs_huge, tIF(cursor_visible))
      STRLCPY(Cap_curs_norm, tIF(cursor_normal))
      STRLCPY(Cap_curs_hide, tIF(cursor_invisible))
      STRLCPY(Cap_home, tIF(cursor_home))
      STRLCPY(Cap_norm, tIF(exit_attribute_mode))
      STRLCPY(Cap_reverse, tIF(enter_reverse_mode))
#ifndef RMAN_IGNORED
      if (!eat_newline_glitch) {
         STRLCPY(Cap_rmam, tIF(exit_am_mode))
         STRLCPY(Cap_smam, tIF(enter_am_mode))
         if (!*Cap_rmam || !*Cap_smam) {
            *Cap_rmam = '\0';
            *Cap_smam = '\0';
            if (auto_right_margin)
               Cap_avoid_eol = 1;
         }
         putp(Cap_rmam);
      }
#endif
      snprintf(Caps_off, sizeof(Caps_off), "%s%s", Cap_norm, tIF(orig_pair));
      snprintf(Caps_endline, sizeof(Caps_endline), "%s%s", Caps_off, Cap_clr_eol);
      if (tgoto(cursor_address, 1, 1)) Cap_can_goto = 1;
      capsdone = 1;
   }

   /* the key to NO run-time costs for configurable colors -- we spend a
      little time with the user now setting up our terminfo strings, and
      the job's done until he/she/it has a change-of-heart */
   STRLCPY(q->cap_bold, CHKw(q, View_NOBOLD) ? Cap_norm : tIF(enter_bold_mode))
   if (CHKw(q, Show_COLORS) && max_colors > 0) {
      STRLCPY(q->capclr_sum, tPM(set_a_foreground, q->rc.summclr))
      snprintf(q->capclr_msg, sizeof(q->capclr_msg), "%s%s"
         , tPM(set_a_foreground, q->rc.msgsclr), Cap_reverse);
      snprintf(q->capclr_pmt, sizeof(q->capclr_pmt), "%s%s"
         , tPM(set_a_foreground, q->rc.msgsclr), q->cap_bold);
      snprintf(q->capclr_hdr, sizeof(q->capclr_hdr), "%s%s"
         , tPM(set_a_foreground, q->rc.headclr), Cap_reverse);
      snprintf(q->capclr_rownorm, sizeof(q->capclr_rownorm), "%s%s"
         , Caps_off, tPM(set_a_foreground, q->rc.taskclr));
   } else {
      q->capclr_sum[0] = '\0';
#ifdef USE_X_COLHDR
      snprintf(q->capclr_msg, sizeof(q->capclr_msg), "%s%s"
         , Cap_reverse, q->cap_bold);
#else
      STRLCPY(q->capclr_msg, Cap_reverse)
#endif
      STRLCPY(q->capclr_pmt, q->cap_bold)
      STRLCPY(q->capclr_hdr, Cap_reverse)
      STRLCPY(q->capclr_rownorm, Cap_norm)
   }

   // composite(s), so we do 'em outside and after the if
   snprintf(q->capclr_rowhigh, sizeof(q->capclr_rowhigh), "%s%s"
      , q->capclr_rownorm, CHKw(q, Show_HIBOLD) ? q->cap_bold : Cap_reverse);
 #undef tIF
 #undef tPM
} // end: capsmk


        /*
         * Show an error message (caller may include '\a' for sound) */
static void show_msg (const char *str) {
   PUTT("%s%s %.*s %s%s%s"
      , tg2(0, Msg_row)
      , Curwin->capclr_msg
      , utf8_embody(str, Screen_cols - 2)
      , str
      , Cap_curs_hide
      , Caps_off
      , Cap_clr_eol);
   fflush(stdout);
   usleep(MSG_USLEEP);
} // end: show_msg


        /*
         * Show an input prompt + larger cursor (if possible) */
static int show_pmt (const char *str) {
   char buf[MEDBUFSIZ];
   int len;

   snprintf(buf, sizeof(buf), "%.*s", utf8_embody(str, Screen_cols - 2), str);
   len = utf8_delta(buf);
#ifdef PRETENDNOCAP
   PUTT("\n%s%s%.*s %s%s%s"
#else
   PUTT("%s%s%.*s %s%s%s"
#endif
      , tg2(0, Msg_row)
      , Curwin->capclr_pmt
      , (Screen_cols - 2) + len
      , buf
      , Cap_curs_huge
      , Caps_off
      , Cap_clr_eol);
   fflush(stdout);
   len = strlen(buf) - len;
   // +1 for the space we added or -1 for the cursor...
   return (len + 1 < Screen_cols) ? len + 1 : Screen_cols - 1;
} // end: show_pmt


        /*
         * Create and print the optional scroll coordinates message */
static void show_scroll (void) {
   char tmp1[SMLBUFSIZ];
#ifndef SCROLLVAR_NO
   char tmp2[SMLBUFSIZ];
#endif
   int totpflgs = Curwin->totpflgs;
   int begpflgs = Curwin->begpflg + 1;

#ifndef USE_X_COLHDR
   if (CHKw(Curwin, Show_HICOLS)) {
      totpflgs -= 2;
      if (ENUpos(Curwin, Curwin->rc.sortindx) < Curwin->begpflg) begpflgs -= 2;
   }
#endif
   if (1 > totpflgs) totpflgs = 1;
   if (1 > begpflgs) begpflgs = 1;
   snprintf(tmp1, sizeof(tmp1), N_fmt(SCROLL_coord_fmt), Curwin->begtask + 1, Frame_maxtask, begpflgs, totpflgs);
#ifndef SCROLLVAR_NO
   if (Curwin->varcolbeg) {
      snprintf(tmp2, sizeof(tmp2), " + %d", Curwin->varcolbeg);
      scat(tmp1, tmp2);
   }
#endif
   PUTT("%s%s  %.*s%s", tg2(0, Msg_row), Caps_off, Screen_cols - 3, tmp1, Cap_clr_eol);
} // end: show_scroll


        /*
         * Show lines with specially formatted elements, but only output
         * what will fit within the current screen width.
         *    Our special formatting consists of:
         *       "some text <_delimiter_> some more text <_delimiter_>...\n"
         *    Where <_delimiter_> is a two byte combination consisting of a
         *    tilde followed by an ascii digit in the range of 1 - 8.
         *       examples: ~1, ~5, ~8, etc.
         *    The tilde is effectively stripped and the next digit
         *    converted to an index which is then used to select an
         *    'attribute' from a capabilities table.  That attribute
         *    is then applied to the *preceding* substring.
         * Once recognized, the delimiter is replaced with a null character
         * and viola, we've got a substring ready to output!  Strings or
         * substrings without delimiters will receive the Cap_norm attribute.
         *
         * Caution:
         *    This routine treats all non-delimiter bytes as displayable
         *    data subject to our screen width marching orders.  If callers
         *    embed non-display data like tabs or terminfo strings in our
         *    glob, a line will truncate incorrectly at best.  Worse case
         *    would be truncation of an embedded tty escape sequence.
         *
         *    Tabs must always be avoided or our efforts are wasted and
         *    lines will wrap.  To lessen but not eliminate the risk of
         *    terminfo string truncation, such non-display stuff should
         *    be placed at the beginning of a "short" line. */
static void show_special (int interact, const char *glob) {
  /* note: the following is for documentation only,
           the real captab is now found in a group's WIN_t !
     +------------------------------------------------------+
     | char *captab[] = {                 :   Cap's = Index |
     |   Cap_norm, Cap_norm,              =   \000, \001,   |
     |   cap_bold, capclr_sum,            =   \002, \003,   |
     |   capclr_msg, capclr_pmt,          =   \004, \005,   |
     |   capclr_hdr,                      =   \006,         |
     |   capclr_rowhigh,                  =   \007,         |
     |   capclr_rownorm  };               =   \010 [octal!] |
     +------------------------------------------------------+ */
  /* ( Pssst, after adding the termcap transitions, row may )
     ( exceed 300+ bytes, even in an 80x24 terminal window! )
     ( Shown here are the former buffer size specifications )
     ( char tmp[SMLBUFSIZ], lin[MEDBUFSIZ], row[LRGBUFSIZ]. )
     ( So now we use larger buffers and a little protection )
     ( against overrunning them with this 'lin_end - glob'. )

     ( That was uncovered during 'Inspect' development when )
     ( this guy was being considered for a supporting role. )
     ( However, such an approach was abandoned. As a result )
     ( this function is called only with a glob under top's )
     ( control and never containing any 'raw/binary' chars! ) */
   char tmp[LRGBUFSIZ], lin[LRGBUFSIZ], row[ROWMAXSIZ];
   char *rp, *lin_end, *sub_beg, *sub_end;
   int room;

   // handle multiple lines passed in a bunch
   while ((lin_end = strchr(glob, '\n'))) {
     #define myMIN(a,b) (((a) < (b)) ? (a) : (b))
      size_t lessor = myMIN((size_t)(lin_end - glob), sizeof(lin) -3);

      // create a local copy we can extend and otherwise abuse
      memcpy(lin, glob, lessor);
      // zero terminate this part and prepare to parse substrings
      lin[lessor] = '\0';
      room = Screen_cols;
      sub_beg = sub_end = lin;
      *(rp = row) = '\0';

      while (*sub_beg) {
         int ch = *sub_end;
         if ('~' == ch) ch = *(sub_end + 1) - '0';
         switch (ch) {
            case 0:                    // no end delim, captab makes normal
               // only possible when '\n' was NOT preceded with a '~#' sequence
               // ( '~1' thru '~8' is valid range, '~0' is never actually used )
               *(sub_end + 1) = '\0';  // extend str end, then fall through
               *(sub_end + 2) = '\0';  // ( +1 optimization for usual path )
            case 1: case 2: case 3: case 4:
            case 5: case 6: case 7: case 8:
               *sub_end = '\0';
               snprintf(tmp, sizeof(tmp), "%s%.*s%s"
                  , Curwin->captab[ch], utf8_embody(sub_beg, room), sub_beg, Caps_off);
               rp = scat(rp, tmp);
               room -= (sub_end - sub_beg);
               room += utf8_delta(sub_beg);
               sub_beg = (sub_end += 2);
               break;
            default:                   // nothin' special, just text
               ++sub_end;
         }
         if (0 >= room) break;         // skip substrings that won't fit
      }

      if (interact) PUTT("%s%s\n", row, Cap_clr_eol);
      else PUFF("%s%s\n", row, Caps_endline);
      glob = ++lin_end;                // point to next line (maybe)

     #undef myMIN
   } // end: while 'lines'

   /* If there's anything left in the glob (by virtue of no trailing '\n'),
      it probably means caller wants to retain cursor position on this final
      line.  That, in turn, means we're interactive and so we'll just do our
      'fit-to-screen' thingy while also leaving room for the cursor... */
   if (*glob) PUTT("%.*s", utf8_embody(glob, Screen_cols - 1), glob);
} // end: show_special

/*######  Low Level Memory/Keyboard/File I/O support  ####################*/

        /*
         * Handle our own memory stuff without the risk of leaving the
         * user's terminal in an ugly state should things go sour. */

static void *alloc_c (size_t num) MALLOC;
static void *alloc_c (size_t num) {
   void *pv;

   if (!num) ++num;
   if (!(pv = calloc(1, num)))
      error_exit(N_txt(FAIL_alloc_c_txt));
   return pv;
} // end: alloc_c


static void *alloc_r (void *ptr, size_t num) MALLOC;
static void *alloc_r (void *ptr, size_t num) {
   void *pv;

   if (!num) ++num;
   if (!(pv = realloc(ptr, num)))
      error_exit(N_txt(FAIL_alloc_r_txt));
   return pv;
} // end: alloc_r


static char *alloc_s (const char *str) MALLOC;
static char *alloc_s (const char *str) {
   return strcpy(alloc_c(strlen(str) +1), str);
} // end: alloc_s


        /*
         * An 'I/O available' routine which will detect raw single byte |
         * unsolicited keyboard input which was susceptible to SIGWINCH |
         * interrupts (or any other signal).  He'll also support timout |
         * in the absence of any user keystrokes or a signal interrupt. | */
static inline int ioa (struct timespec *ts) {
   fd_set fs;
   int rc;

   FD_ZERO(&fs);
   FD_SET(STDIN_FILENO, &fs);

#ifdef SIGNALS_LESS // conditional comments are silly, but help in documenting
   // hold here until we've got keyboard input, any signal except SIGWINCH
   // or (optionally) we timeout with nanosecond granularity
#else
   // hold here until we've got keyboard input, any signal (including SIGWINCH)
   // or (optionally) we timeout with nanosecond granularity
#endif
   rc = pselect(STDIN_FILENO + 1, &fs, NULL, NULL, ts, &Sigwinch_set);

   if (rc < 0) rc = 0;
   return rc;
} // end: ioa


        /*
         * This routine isolates ALL user INPUT and ensures that we
         * wont be mixing I/O from stdio and low-level read() requests */
static int ioch (int ech, char *buf, unsigned cnt) {
   int rc = -1;

#ifdef TERMIOS_ONLY
   if (ech) {
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &Tty_tweaked);
      rc = read(STDIN_FILENO, buf, cnt);
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &Tty_raw);
   } else {
      if (ioa(NULL))
         rc = read(STDIN_FILENO, buf, cnt);
   }
#else
   (void)ech;
   if (ioa(NULL))
      rc = read(STDIN_FILENO, buf, cnt);
#endif

   // zero means EOF, might happen if we erroneously get detached from terminal
   if (0 == rc) bye_bye(NULL);

   // it may have been the beginning of a lengthy escape sequence
   tcflush(STDIN_FILENO, TCIFLUSH);

   // note: we do NOT produce a valid 'string'
   return rc;
} // end: ioch


        /*
         * Support for single or multiple keystroke input AND
         * escaped cursor motion keys.
         * note: we support more keys than we currently need, in case
         *       we attract new consumers in the future */
static int iokey (int action) {
   static struct {
      const char *str;
      int key;
   } tinfo_tab[] = {
      { NULL, kbd_BKSP  }, { NULL, kbd_INS   }, { NULL, kbd_DEL   },
      { NULL, kbd_LEFT  }, { NULL, kbd_DOWN  }, { NULL, kbd_UP    }, { NULL, kbd_RIGHT },
      { NULL, kbd_HOME  }, { NULL, kbd_PGDN  }, { NULL, kbd_PGUP  }, { NULL, kbd_END   },
         // remainder are alternatives for above, just in case...
         // ( the h,j,k,l entries are the vim cursor motion keys )
      { "\033h",    kbd_LEFT  }, { "\033j",    kbd_DOWN  }, /* meta+      h,j */
      { "\033k",    kbd_UP    }, { "\033l",    kbd_RIGHT }, /* meta+      k,l */
      { "\033\010", kbd_HOME  }, { "\033\012", kbd_PGDN  }, /* ctrl+meta+ h,j */
      { "\033\013", kbd_PGUP  }, { "\033\014", kbd_END   }, /* ctrl+meta+ k,l */
      { "\xC3\xA8", kbd_LEFT  }, { "\xC3\xAA", kbd_DOWN  }, /* meta+      h,j (some xterms) */
      { "\xC3\xAB", kbd_UP    }, { "\xC3\xAC", kbd_RIGHT }, /* meta+      k,l (some xterms) */
      { "\xC2\x88", kbd_HOME  }, { "\xC2\x8A", kbd_PGDN  }, /* ctrl+meta+ h,j (some xterms) */
      { "\xC2\x8B", kbd_PGUP  }, { "\xC2\x8C", kbd_END   }  /* ctrl+meta+ k,l (some xterms) */
   };
#ifdef TERMIOS_ONLY
   char buf[SMLBUFSIZ], *pb;
#else
   static char buf[SMLBUFSIZ];
   static int pos, len;
   char *pb;
#endif
   int i;

   if (action == 0) {
    #define tOk(s)  s ? s : ""
      tinfo_tab[0].str  = tOk(key_backspace);
      tinfo_tab[1].str  = tOk(key_ic);
      tinfo_tab[2].str  = tOk(key_dc);
      tinfo_tab[3].str  = tOk(key_left);
      tinfo_tab[4].str  = tOk(key_down);
      tinfo_tab[5].str  = tOk(key_up);
      tinfo_tab[6].str  = tOk(key_right);
      tinfo_tab[7].str  = tOk(key_home);
      tinfo_tab[8].str  = tOk(key_npage);
      tinfo_tab[9].str  = tOk(key_ppage);
      tinfo_tab[10].str = tOk(key_end);
      // next is critical so returned results match bound terminfo keys
      putp(tOk(keypad_xmit));
      // ( converse keypad_local issued at pause/pgm end, just in case )
      return 0;
    #undef tOk
   }

   if (action == 1) {
      memset(buf, '\0', sizeof(buf));
      if (1 > ioch(0, buf, sizeof(buf)-1)) return 0;
   }

#ifndef TERMIOS_ONLY
   if (action == 2) {
      if (pos < len)
         return buf[pos++];            // exhaust prior keystrokes
      pos = len = 0;
      memset(buf, '\0', sizeof(buf));
      if (1 > ioch(0, buf, sizeof(buf)-1)) return 0;
      if (isprint(buf[0])) {           // no need for translation
         len = strlen(buf);
         pos = 1;
         return buf[0];
      }
   }
#endif

   /* some emulators implement 'key repeat' too well and we get duplicate
      key sequences -- so we'll focus on the last escaped sequence, while
      also allowing use of the meta key... */
   if (!(pb = strrchr(buf, '\033'))) pb = buf;
   else if (pb > buf && '\033' == *(pb - 1)) --pb;

   for (i = 0; i < MAXTBL(tinfo_tab); i++)
      if (!strcmp(tinfo_tab[i].str, pb))
         return tinfo_tab[i].key;

   // no match, so we'll return single non-escaped keystrokes only
   if (buf[0] == '\033' && buf[1]) return -1;
   return buf[0];
} // end: iokey


#ifdef TERMIOS_ONLY
        /*
         * Get line oriented interactive input from the user,
         * using native tty support */
static char *ioline (const char *prompt) {
   static const char ws[] = "\b\f\n\r\t\v\x1b\x9b";  // 0x1b + 0x9b are escape
   static char buf[MEDBUFSIZ];
   char *p;

   show_pmt(prompt);
   memset(buf, '\0', sizeof(buf));
   ioch(1, buf, sizeof(buf)-1);

   if ((p = strpbrk(buf, ws))) *p = '\0';
   // note: we DO produce a vaid 'string'
   return buf;
} // end: ioline

#else
        /*
         * Get line oriented interactive input from the user,
         * going way beyond native tty support by providing:
         * . true line editing, not just destructive backspace
         * . an input limit sensitive to current screen dimensions
         * . ability to recall prior strings for re-input/re-editing */
static char *ioline (const char *prompt) {
 #define savMAX  50
    // thank goodness memmove allows the two strings to overlap
 #define sqzSTR  { memmove(&buf[pos], &buf[pos+1], bufMAX-pos); \
       buf[sizeof(buf)-1] = '\0'; }
 #define expSTR  if (len+1 < bufMAX && len+beg+1 < Screen_cols) { \
       memmove(&buf[pos+1], &buf[pos], bufMAX-pos); buf[pos] = ' '; }
 #define logCOL  (pos+1)
 #define phyCOL  (beg+pos+1)
 #define bufMAX  ((int)sizeof(buf)-2)  // -1 for '\0' string delimeter
   static char buf[MEDBUFSIZ+1];       // +1 for '\0' string delimeter
   static int ovt;
   int beg, pos, len, key, i;
   struct lin_s {
      struct lin_s *bkw;               // ptr to older saved strs
      struct lin_s *fwd;               // ptr to newer saved strs
      char *str;                       // the saved string
   };
   static struct lin_s *anchor, *plin;

   if (!anchor) {
      anchor = alloc_c(sizeof(struct lin_s));
      anchor->str = alloc_s("");       // top-of-stack == empty str
   }
   plin = anchor;
   pos = 0;
   beg = show_pmt(prompt);
   memset(buf, '\0', sizeof(buf));
   putp(ovt ? Cap_curs_huge : Cap_curs_norm);

   do {
      fflush(stdout);
      len = strlen(buf);
      key = iokey(2);
      switch (key) {
         case 0:
            buf[0] = '\0';
            return buf;
         case kbd_ESC:
            buf[0] = kbd_ESC;
            return buf;
         case kbd_ENTER:
            continue;
         case kbd_INS:
            ovt = !ovt;
            putp(ovt ? Cap_curs_huge : Cap_curs_norm);
            break;
         case kbd_DEL:
            sqzSTR
            break;
         case kbd_BKSP :
            if (0 < pos) { --pos; sqzSTR }
            break;
         case kbd_LEFT:
            if (0 < pos) --pos;
            break;
         case kbd_RIGHT:
            if (pos < len) ++pos;
            break;
         case kbd_HOME:
            pos = 0;
            break;
         case kbd_END:
            pos = len;
            break;
         case kbd_UP:
            if (plin->bkw) {
               plin = plin->bkw;
               memset(buf, '\0', sizeof(buf));
               pos = snprintf(buf, sizeof(buf), "%.*s", Screen_cols - beg - 1, plin->str);
            }
            break;
         case kbd_DOWN:
            memset(buf, '\0', sizeof(buf));
            if (plin->fwd) plin = plin->fwd;
            pos = snprintf(buf, sizeof(buf), "%.*s", Screen_cols - beg - 1, plin->str);
            break;
         default:                      // what we REALLY wanted (maybe)
            if (isprint(key) && logCOL < bufMAX && phyCOL < Screen_cols) {
               if (!ovt) expSTR
               buf[pos++] = key;
            }
            break;
      }
      putp(fmtmk("%s%s%s", tg2(beg, Msg_row), Cap_clr_eol, buf));
      putp(tg2(beg+pos, Msg_row));
   } while (key != kbd_ENTER);

   // weed out duplicates, including empty strings (top-of-stack)...
   for (i = 0, plin = anchor; ; i++) {
#ifdef RECALL_FIXED
      if (!STRCMP(plin->str, buf))     // if matched, retain original order
         return buf;
#else
      if (!STRCMP(plin->str, buf)) {   // if matched, rearrange stack order
         if (i > 1) {                  // but not null str or if already #2
            if (plin->bkw)             // splice around this matched string
               plin->bkw->fwd = plin->fwd; // if older exists link to newer
            plin->fwd->bkw = plin->bkw;    // newer linked to older or NULL
            anchor->bkw->fwd = plin;   // stick matched on top of former #2
            plin->bkw = anchor->bkw;   // keep empty string at top-of-stack
            plin->fwd = anchor;        // then prepare to be the 2nd banana
            anchor->bkw = plin;        // by sliding us in below the anchor
         }
         return buf;
      }
#endif
      if (!plin->bkw) break;           // let i equal total stacked strings
      plin = plin->bkw;                // ( with plin representing bottom )
   }
   if (i < savMAX)
      plin = alloc_c(sizeof(struct lin_s));
   else {                              // when a new string causes overflow
      plin->fwd->bkw = NULL;           // make next-to-last string new last
      free(plin->str);                 // and toss copy but keep the struct
   }
   plin->str = alloc_s(buf);           // copy user's new unique input line
   plin->bkw = anchor->bkw;            // keep empty string as top-of-stack
   if (plin->bkw)                      // did we have some already stacked?
      plin->bkw->fwd = plin;           // yep, so point prior to new string
   plin->fwd = anchor;                 // and prepare to be a second banana
   anchor->bkw = plin;                 // by sliding it in as new number 2!

   return buf;                         // protect our copy, return original
 #undef savMAX
 #undef sqzSTR
 #undef expSTR
 #undef logCOL
 #undef phyCOL
 #undef bufMAX
} // end: ioline
#endif


        /*
         * Make locale unaware float (but maybe restrict to whole numbers). */
static int mkfloat (const char *str, float *num, int whole) {
   char tmp[SMLBUFSIZ], *ep;

   if (whole) {
      *num = (float)strtol(str, &ep, 0);
      if (ep != str && *ep == '\0' && *num < INT_MAX)
         return 1;
      return 0;
   }
   snprintf(tmp, sizeof(tmp), "%s", str);
   *num = strtof(tmp, &ep);
   if (*ep != '\0') {
      // fallback - try to swap the floating point separator
      if (*ep == '.') *ep = ',';
      else if (*ep == ',') *ep = '.';
      *num = strtof(tmp, &ep);
   }
   if (ep != tmp && *ep == '\0' && *num < INT_MAX)
      return 1;
   return 0;
} // end: mkfloat


        /*
         * This routine provides the i/o in support of files whose size
         * cannot be determined in advance.  Given a stream pointer, he'll
         * try to slurp in the whole thing and return a dynamically acquired
         * buffer supporting that single string glob.
         *
         * He always creates a buffer at least READMINSZ big, possibly
         * all zeros (an empty string), even if the file wasn't read. */
static int readfile (FILE *fp, char **baddr, size_t *bsize, size_t *bread) {
   char chunk[4096*16];
   size_t num;

   *bread = 0;
   *bsize = READMINSZ;
   *baddr = alloc_c(READMINSZ);
   if (fp) {
      while (0 < (num = fread(chunk, 1, sizeof(chunk), fp))) {
         *baddr = alloc_r(*baddr, num + *bsize);
         memcpy(*baddr + *bread, chunk, num);
         *bread += num;
         *bsize += num;
      };
      *(*baddr + *bread) = '\0';
      return ferror(fp);
   }
   return ENOENT;
} // end: readfile

/*######  Small Utility routines  ########################################*/

#define GET_NUM_BAD  INT_MIN
#define GET_NUM_ESC (INT_MIN + 1)
#define GET_NUM_NOT (INT_MIN + 2)

        /*
         * Get a float from the user */
static float get_float (const char *prompt) {
   char *line;
   float f;

   line = ioline(prompt);
   if (line[0] == kbd_ESC || Frames_signal) return GET_NUM_ESC;
   if (!line[0]) return GET_NUM_NOT;
   // note: we're not allowing negative floats
   if (!mkfloat(line, &f, 0) || f < 0) {
      show_msg(N_txt(BAD_numfloat_txt));
      return GET_NUM_BAD;
   }
   return f;
} // end: get_float


        /*
         * Get an integer from the user, returning INT_MIN for error */
static int get_int (const char *prompt) {
   char *line;
   float f;

   line = ioline(prompt);
   if (line[0] == kbd_ESC || Frames_signal) return GET_NUM_ESC;
   if (!line[0]) return GET_NUM_NOT;
   // note: we've got to allow negative ints (renice)
   if (!mkfloat(line, &f, 1)) {
      show_msg(N_txt(BAD_integers_txt));
      return GET_NUM_BAD;
   }
   return (int)f;
} // end: get_int


        /*
         * Make a hex value, and maybe suppress zeroes. */
static inline const char *hex_make (KLONG num, int noz) {
   static char buf[SMLBUFSIZ];
   int i;

#ifdef CASEUP_HEXES
   snprintf(buf, sizeof(buf), "%08" KLF "X", num);
#else
   snprintf(buf, sizeof(buf), "%08" KLF "x", num);
#endif
   if (noz)
      for (i = 0; buf[i]; i++)
         if ('0' == buf[i])
            buf[i] = '.';
   return buf;
} // end: hex_make


        /*
         * Validate the passed string as a user name or number,
         * and/or update the window's 'u/U' selection stuff. */
static const char *user_certify (WIN_t *q, const char *str, char typ) {
   struct passwd *pwd;
   char *endp;
   uid_t num;

   q->usrseltyp = 0;
   q->usrselflg = 1;
   Monpidsidx = 0;
   if (*str) {
      if ('!' == *str) { ++str; q->usrselflg = 0; }
      num = (uid_t)strtoul(str, &endp, 0);
      if ('\0' == *endp) {
         pwd = getpwuid(num);
         if (!pwd) {
         /* allow foreign users, from e.g within chroot
          ( thanks Dr. Werner Fink <werner@suse.de> ) */
            q->usrseluid = num;
            q->usrseltyp = typ;
            return NULL;
         }
      } else
         pwd = getpwnam(str);
      if (!pwd) return N_txt(BAD_username_txt);
      q->usrseluid = pwd->pw_uid;
      q->usrseltyp = typ;
   }
   return NULL;
} // end: user_certify

/*######  Basic Formatting support  ######################################*/

        /*
         * Just do some justify stuff, then add post column padding. */
static inline const char *justify_pad (const char *str, int width, int justr) {
   static char l_fmt[]  = "%-*.*s%s", r_fmt[] = "%*.*s%s";
   static char buf[SCREENMAX];

   snprintf(buf, sizeof(buf), justr ? r_fmt : l_fmt, width, width, str, COLPADSTR);
   return buf;
} // end: justify_pad


        /*
         * Make and then justify a single character. */
static inline const char *make_chr (const char ch, int width, int justr) {
   static char buf[SMLBUFSIZ];

   snprintf(buf, sizeof(buf), "%c", ch);
   return justify_pad(buf, width, justr);
} // end: make_chr


        /*
         * Make and then justify an integer NOT subject to scaling,
         * and include a visual clue should tuncation be necessary. */
static inline const char *make_num (long num, int width, int justr, int col, int noz) {
   static char buf[SMLBUFSIZ];

   buf[0] = '\0';
   if (noz && Rc.zero_suppress && 0 == num)
      goto end_justifies;

   if (width < snprintf(buf, sizeof(buf), "%ld", num)) {
      if (width <= 0 || (size_t)width >= sizeof(buf))
         width = sizeof(buf)-1;
      buf[width-1] = COLPLUSCH;
      buf[width] = '\0';
      AUTOX_COL(col);
   }
end_justifies:
   return justify_pad(buf, width, justr);
} // end: make_num


        /*
         * Make and then justify a character string,
         * and include a visual clue should tuncation be necessary. */
static inline const char *make_str (const char *str, int width, int justr, int col) {
   static char buf[SCREENMAX];

   if (width < snprintf(buf, sizeof(buf), "%s", str)) {
      if (width <= 0 || (size_t)width >= sizeof(buf))
         width = sizeof(buf)-1;
      buf[width-1] = COLPLUSCH;
      buf[width] = '\0';
      AUTOX_COL(col);
   }
   return justify_pad(buf, width, justr);
} // end: make_str


        /*
         * Make and then justify a potentially multi-byte character string,
         * and include a visual clue should tuncation be necessary. */
static inline const char *make_str_utf8 (const char *str, int width, int justr, int col) {
   static char buf[SCREENMAX];
   int delta = utf8_delta(str);

   if (width + delta < snprintf(buf, sizeof(buf), "%s", str)) {
      snprintf(buf, sizeof(buf), "%.*s%c", utf8_embody(str, width-1), str, COLPLUSCH);
      delta = utf8_delta(buf);
      AUTOX_COL(col);
   }
   return justify_pad(buf, width + delta, justr);
} // end: make_str_utf8


        /*
         * Do some scaling then justify stuff.
         * We'll interpret 'num' as a kibibytes quantity and try to
         * format it to reach 'target' while also fitting 'width'. */
static const char *scale_mem (int target, unsigned long num, int width, int justr) {
   //                               SK_Kb   SK_Mb      SK_Gb      SK_Tb      SK_Pb      SK_Eb
#ifdef BOOST_MEMORY
   static const char *fmttab[] =  { "%.0f", "%#.1f%c", "%#.3f%c", "%#.3f%c", "%#.3f%c", NULL };
#else
   static const char *fmttab[] =  { "%.0f", "%.1f%c",  "%.1f%c",  "%.1f%c",  "%.1f%c",  NULL };
#endif
   static char buf[SMLBUFSIZ];
   float scaled_num;
   char *psfx;
   int i;

   buf[0] = '\0';
   if (Rc.zero_suppress && 0 >= num)
      goto end_justifies;

   scaled_num = num;
   for (i = SK_Kb, psfx = Scaled_sfxtab; i < SK_Eb; psfx++, i++) {
      if (i >= target
      && (width >= snprintf(buf, sizeof(buf), fmttab[i], scaled_num, *psfx)))
         goto end_justifies;
      scaled_num /= 1024.0;
   }

   // well shoot, this outta' fit...
   snprintf(buf, sizeof(buf), "?");
end_justifies:
   return justify_pad(buf, width, justr);
} // end: scale_mem


        /*
         * Do some scaling then justify stuff. */
static const char *scale_num (unsigned long num, int width, int justr) {
   static char buf[SMLBUFSIZ];
   float scaled_num;
   char *psfx;

   buf[0] = '\0';
   if (Rc.zero_suppress && 0 >= num)
      goto end_justifies;
   if (width >= snprintf(buf, sizeof(buf), "%lu", num))
      goto end_justifies;

   scaled_num = num;
   for (psfx = Scaled_sfxtab; 0 < *psfx; psfx++) {
      scaled_num /= 1024.0;
      if (width >= snprintf(buf, sizeof(buf), "%.1f%c", scaled_num, *psfx))
         goto end_justifies;
      if (width >= snprintf(buf, sizeof(buf), "%.0f%c", scaled_num, *psfx))
         goto end_justifies;
   }

   // well shoot, this outta' fit...
   snprintf(buf, sizeof(buf), "?");
end_justifies:
   return justify_pad(buf, width, justr);
} // end: scale_num


        /*
         * Make and then justify a percentage, with decreasing precision. */
static const char *scale_pcnt (float num, int width, int justr) {
   static char buf[SMLBUFSIZ];

   buf[0] = '\0';
   if (Rc.zero_suppress && 0 >= num)
      goto end_justifies;
#ifdef BOOST_PERCNT
   if (width >= snprintf(buf, sizeof(buf), "%#.3f", num))
      goto end_justifies;
   if (width >= snprintf(buf, sizeof(buf), "%#.2f", num))
      goto end_justifies;
#endif
   if (width >= snprintf(buf, sizeof(buf), "%#.1f", num))
      goto end_justifies;
   if (width >= snprintf(buf, sizeof(buf), "%*.0f", width, num))
      goto end_justifies;

   // well shoot, this outta' fit...
   snprintf(buf, sizeof(buf), "?");
end_justifies:
   return justify_pad(buf, width, justr);
} // end: scale_pcnt


        /*
         * Do some scaling stuff.
         * Format 'tics' to fit 'width', then justify it. */
static const char *scale_tics (TIC_t tics, int width, int justr) {
#ifdef CASEUP_SUFIX
 #define HH "%uH"                                                  // nls_maybe
 #define DD "%uD"
 #define WW "%uW"
#else
 #define HH "%uh"                                                  // nls_maybe
 #define DD "%ud"
 #define WW "%uw"
#endif
   static char buf[SMLBUFSIZ];
   unsigned long nt;    // narrow time, for speed on 32-bit
   unsigned cc;         // centiseconds
   unsigned nn;         // multi-purpose whatever

   buf[0] = '\0';
   nt  = (tics * 100ull) / Hertz;               // up to 68 weeks of cpu time
   if (Rc.zero_suppress && 0 >= nt)
      goto end_justifies;
   cc  = nt % 100;                              // centiseconds past second
   nt /= 100;                                   // total seconds
   nn  = nt % 60;                               // seconds past the minute
   nt /= 60;                                    // total minutes
   if (width >= snprintf(buf, sizeof(buf), "%lu:%02u.%02u", nt, nn, cc))
      goto end_justifies;
   if (width >= snprintf(buf, sizeof(buf), "%lu:%02u", nt, nn))
      goto end_justifies;
   nn  = nt % 60;                               // minutes past the hour
   nt /= 60;                                    // total hours
   if (width >= snprintf(buf, sizeof(buf), "%lu,%02u", nt, nn))
      goto end_justifies;
   nn = nt;                                     // now also hours
   if (width >= snprintf(buf, sizeof(buf), HH, nn))
      goto end_justifies;
   nn /= 24;                                    // now days
   if (width >= snprintf(buf, sizeof(buf), DD, nn))
      goto end_justifies;
   nn /= 7;                                     // now weeks
   if (width >= snprintf(buf, sizeof(buf), WW, nn))
      goto end_justifies;

   // well shoot, this outta' fit...
   snprintf(buf, sizeof(buf), "?");
end_justifies:
   return justify_pad(buf, width, justr);
 #undef HH
 #undef DD
 #undef WW
} // end: scale_tics

/*######  Fields Management support  #####################################*/

   /* These are the Fieldstab.lflg values used here and in calibrate_fields.
      (own identifiers as documentation and protection against changes) */
#define L_stat     PROC_FILLSTAT
#define L_statm    PROC_FILLMEM
#define L_status   PROC_FILLSTATUS
#define L_CGROUP   PROC_EDITCGRPCVT | PROC_FILLCGROUP
#define L_CMDLINE  PROC_EDITCMDLCVT | PROC_FILLARG
#define L_ENVIRON  PROC_EDITENVRCVT | PROC_FILLENV
#define L_EUSER    PROC_FILLUSR
#define L_OUSER    PROC_FILLSTATUS | PROC_FILLUSR
#define L_EGROUP   PROC_FILLSTATUS | PROC_FILLGRP
#define L_SUPGRP   PROC_FILLSTATUS | PROC_FILLSUPGRP
#define L_NS       PROC_FILLNS
#define L_LXC      PROC_FILL_LXC
#define L_OOM      PROC_FILLOOM
   // make 'none' non-zero (used to be important to Frames_libflags)
#define L_NONE     PROC_SPARE_1
   // from 'status' or 'stat' (favor stat), via bits not otherwise used
#define L_EITHER   PROC_SPARE_2
   // for calibrate_fields and summary_show 1st pass
#define L_DEFAULT  PROC_FILLSTAT

        /* These are our gosh darn 'Fields' !
           They MUST be kept in sync with pflags !! */
static FLD_t Fieldstab[] = {
   // a temporary macro, soon to be undef'd...
 #define SF(f) (QFP_t)SCB_NAME(f)
   // these identifiers reflect the default column alignment but they really
   // contain the WIN_t flag used to check/change justification at run-time!
 #define A_left  Show_JRSTRS       /* toggled with lower case 'j' */
 #define A_right Show_JRNUMS       /* toggled with upper case 'J' */

/* .width anomalies:
        a -1 width represents variable width columns
        a  0 width represents columns set once at startup (see zap_fieldstab)
   .lflg anomalies:
        EU_UED, L_NONE - natural outgrowth of 'stat()' in readproc        (euid)
        EU_CPU, L_stat - never filled by libproc, but requires times      (pcpu)
        EU_CMD, L_stat - may yet require L_CMDLINE in calibrate_fields    (cmd/cmdline)
        L_EITHER       - favor L_stat (L_status == ++cost of gpref & hash scheme)

     .width  .scale  .align    .sort     .lflg
     ------  ------  --------  --------  --------  */
   {     0,     -1,  A_right,  SF(PID),  L_NONE    },
   {     0,     -1,  A_right,  SF(PPD),  L_EITHER  },
   {     5,     -1,  A_right,  SF(UED),  L_NONE    },
   {     8,     -1,  A_left,   SF(UEN),  L_EUSER   },
   {     5,     -1,  A_right,  SF(URD),  L_status  },
   {     8,     -1,  A_left,   SF(URN),  L_OUSER   },
   {     5,     -1,  A_right,  SF(USD),  L_status  },
   {     8,     -1,  A_left,   SF(USN),  L_OUSER   },
   {     5,     -1,  A_right,  SF(GID),  L_NONE    },
   {     8,     -1,  A_left,   SF(GRP),  L_EGROUP  },
   {     0,     -1,  A_right,  SF(PGD),  L_stat    },
   {     8,     -1,  A_left,   SF(TTY),  L_stat    },
   {     0,     -1,  A_right,  SF(TPG),  L_stat    },
   {     0,     -1,  A_right,  SF(SID),  L_stat    },
   {     3,     -1,  A_right,  SF(PRI),  L_stat    },
   {     3,     -1,  A_right,  SF(NCE),  L_stat    },
   {     3,     -1,  A_right,  SF(THD),  L_EITHER  },
   {     0,     -1,  A_right,  SF(CPN),  L_stat    },
   {     5,     -1,  A_right,  SF(CPU),  L_stat    },
   {     6,     -1,  A_right,  SF(TME),  L_stat    },
   {     9,     -1,  A_right,  SF(TME),  L_stat    }, // EU_TM2 slot
   {     5,     -1,  A_right,  SF(RES),  L_statm   }, // EU_MEM slot
   {     7,  SK_Kb,  A_right,  SF(VRT),  L_statm   },
   {     6,  SK_Kb,  A_right,  SF(SWP),  L_status  },
   {     6,  SK_Kb,  A_right,  SF(RES),  L_statm   },
   {     6,  SK_Kb,  A_right,  SF(COD),  L_statm   },
   {     7,  SK_Kb,  A_right,  SF(DAT),  L_statm   },
   {     6,  SK_Kb,  A_right,  SF(SHR),  L_statm   },
   {     4,     -1,  A_right,  SF(FL1),  L_stat    },
   {     4,     -1,  A_right,  SF(FL2),  L_stat    },
   {     4,     -1,  A_right,  SF(DRT),  L_statm   },
   {     1,     -1,  A_right,  SF(STA),  L_EITHER  },
   {    -1,     -1,  A_left,   SF(CMD),  L_EITHER  },
   {    10,     -1,  A_left,   SF(WCH),  L_stat    },
   {     8,     -1,  A_left,   SF(FLG),  L_stat    },
   {    -1,     -1,  A_left,   SF(CGR),  L_CGROUP  },
   {    -1,     -1,  A_left,   SF(SGD),  L_status  },
   {    -1,     -1,  A_left,   SF(SGN),  L_SUPGRP  },
   {     0,     -1,  A_right,  SF(TGD),  L_NONE    },
   {     5,     -1,  A_right,  SF(OOA),  L_OOM     },
   {     4,     -1,  A_right,  SF(OOM),  L_OOM     },
   {    -1,     -1,  A_left,   SF(ENV),  L_ENVIRON },
   {     3,     -1,  A_right,  SF(FV1),  L_stat    },
   {     3,     -1,  A_right,  SF(FV2),  L_stat    },
   {     6,  SK_Kb,  A_right,  SF(USE),  L_status  },
   {    10,     -1,  A_right,  SF(NS1),  L_NS      }, // IPCNS
   {    10,     -1,  A_right,  SF(NS2),  L_NS      }, // MNTNS
   {    10,     -1,  A_right,  SF(NS3),  L_NS      }, // NETNS
   {    10,     -1,  A_right,  SF(NS4),  L_NS      }, // PIDNS
   {    10,     -1,  A_right,  SF(NS5),  L_NS      }, // USERNS
   {    10,     -1,  A_right,  SF(NS6),  L_NS      }, // UTSNS
   {     8,     -1,  A_left,   SF(LXC),  L_LXC     },
   {     6,  SK_Kb,  A_right,  SF(RZA),  L_status  },
   {     6,  SK_Kb,  A_right,  SF(RZF),  L_status  },
   {     6,  SK_Kb,  A_right,  SF(RZL),  L_status  },
   {     6,  SK_Kb,  A_right,  SF(RZS),  L_status  },
   {    -1,     -1,  A_left,   SF(CGN),  L_CGROUP  },
   {     0,     -1,  A_right,  SF(NMA),  L_stat    },
 #undef SF
 #undef A_left
 #undef A_right
};


        /*
         * A calibrate_fields() *Helper* function to refresh the
         * cached screen geometry and related variables */
static void adj_geometry (void) {
   static size_t pseudo_max = 0;
   static int w_set = 0, w_cols = 0, w_rows = 0;
   struct winsize wz;

   Screen_cols = columns;    // <term.h>
   Screen_rows = lines;      // <term.h>

   if (-1 != ioctl(STDOUT_FILENO, TIOCGWINSZ, &wz)
   && 0 < wz.ws_col && 0 < wz.ws_row) {
      Screen_cols = wz.ws_col;
      Screen_rows = wz.ws_row;
   }

#ifndef RMAN_IGNORED
   // be crudely tolerant of crude tty emulators
   if (Cap_avoid_eol) Screen_cols--;
#endif

   // we might disappoint some folks (but they'll deserve it)
   if (Screen_cols > SCREENMAX) Screen_cols = SCREENMAX;
   if (Screen_cols < W_MIN_COL) Screen_cols = W_MIN_COL;

   if (!w_set) {
      if (Width_mode > 0)              // -w with arg, we'll try to honor
         w_cols = Width_mode;
      else
      if (Width_mode < 0) {            // -w without arg, try environment
         char *env_columns = getenv("COLUMNS"),
              *env_lines = getenv("LINES"),
              *ep;
         if (env_columns && *env_columns) {
            long t, tc = 0;
            t = strtol(env_columns, &ep, 0);
            if (!*ep && (t > 0) && (t <= 0x7fffffffL)) tc = t;
            if (0 < tc) w_cols = (int)tc;
         }
         if (env_lines && *env_lines) {
            long t, tr = 0;
            t = strtol(env_lines, &ep, 0);
            if (!*ep && (t > 0) && (t <= 0x7fffffffL)) tr = t;
            if (0 < tr) w_rows = (int)tr;
         }
         if (!w_cols) w_cols = SCREENMAX;
         if (w_cols && w_cols < W_MIN_COL) w_cols = W_MIN_COL;
         if (w_rows && w_rows < W_MIN_ROW) w_rows = W_MIN_ROW;
      }
      if (w_cols > SCREENMAX) w_cols = SCREENMAX;
      w_set = 1;
   }

   /* keep our support for output optimization in sync with current reality
      note: when we're in Batch mode, we don't really need a Pseudo_screen
            and when not Batch, our buffer will contain 1 extra 'line' since
            Msg_row is never represented -- but it's nice to have some space
            between us and the great-beyond... */
   if (Batch) {
      if (w_cols) Screen_cols = w_cols;
      Screen_rows = w_rows ? w_rows : INT_MAX;
      Pseudo_size = (sizeof(*Pseudo_screen) * ROWMAXSIZ);
   } else {
      const int max_rows = INT_MAX / (sizeof(*Pseudo_screen) * ROWMAXSIZ);
      if (w_cols && w_cols < Screen_cols) Screen_cols = w_cols;
      if (w_rows && w_rows < Screen_rows) Screen_rows = w_rows;
      if (Screen_rows < 0 || Screen_rows > max_rows) Screen_rows = max_rows;
      Pseudo_size = (sizeof(*Pseudo_screen) * ROWMAXSIZ) * Screen_rows;
   }
   // we'll only grow our Pseudo_screen, never shrink it
   if (pseudo_max < Pseudo_size) {
      pseudo_max = Pseudo_size;
      Pseudo_screen = alloc_r(Pseudo_screen, pseudo_max);
   }
   // ensure each row is repainted (just in case)
   PSU_CLREOS(0);

   // prepare to customize potential cpu/memory graphs
   Graph_len = Screen_cols - GRAPH_prefix - GRAPH_actual - GRAPH_suffix;
   if (Graph_len >= 0) Graph_len = GRAPH_actual;
   else if (Screen_cols > 80) Graph_len = Screen_cols - GRAPH_prefix - GRAPH_suffix;
   else Graph_len = 80 - GRAPH_prefix - GRAPH_suffix;
   Graph_adj = (float)Graph_len / 100.0;

   fflush(stdout);
   Frames_signal = BREAK_off;
} // end: adj_geometry


        /*
         * A calibrate_fields() *Helper* function to build the
         * actual column headers and required library flags */
static void build_headers (void) {
   FLG_t f;
   char *s;
   WIN_t *w = Curwin;
#ifdef EQUCOLHDRYES
   int x, hdrmax = 0;
#endif
   int i;

   Frames_libflags = 0;

   do {
      if (VIZISw(w)) {
         memset((s = w->columnhdr), 0, sizeof(w->columnhdr));
         if (Rc.mode_altscr) s = scat(s, fmtmk("%d", w->winnum));
         for (i = 0; i < w->maxpflgs; i++) {
            f = w->procflgs[i];
#ifdef USE_X_COLHDR
            if (CHKw(w, Show_HICOLS) && f == w->rc.sortindx) {
               s = scat(s, fmtmk("%s%s", Caps_off, w->capclr_msg));
               w->hdrcaplen += strlen(Caps_off) + strlen(w->capclr_msg);
            }
#else
            if (EU_MAXPFLGS <= f) continue;
#endif
            if (EU_CMD == f && CHKw(w, Show_CMDLIN)) Frames_libflags |= L_CMDLINE;
            Frames_libflags |= Fieldstab[f].lflg;
            s = scat(s, utf8_justify(N_col(f)
               , VARcol(f) ? w->varcolsz : Fieldstab[f].width
               , CHKw(w, Fieldstab[f].align)));
#ifdef USE_X_COLHDR
            if (CHKw(w, Show_HICOLS) && f == w->rc.sortindx) {
               s = scat(s, fmtmk("%s%s", Caps_off, w->capclr_hdr));
               w->hdrcaplen += strlen(Caps_off) + strlen(w->capclr_hdr);
            }
#endif
         }
#ifdef EQUCOLHDRYES
         // prepare to even out column header lengths...
         if (hdrmax + w->hdrcaplen < (x = strlen(w->columnhdr))) hdrmax = x - w->hdrcaplen;
#endif
         // with forest view mode, we'll need tgid, ppid & start_time...
         if (CHKw(w, Show_FOREST)) Frames_libflags |= L_stat;
         // for 'busy' only processes, we'll need pcpu (utime & stime)...
         if (!CHKw(w, Show_IDLEPS)) Frames_libflags |= L_stat;
         // we must also accommodate an out of view sort field...
         f = w->rc.sortindx;
         Frames_libflags |= Fieldstab[f].lflg;
         if (EU_CMD == f && CHKw(w, Show_CMDLIN)) Frames_libflags |= L_CMDLINE;
         // for 'U' filtering we need the other user ids too
         if (w->usrseltyp == 'U') Frames_libflags |= L_status;
      } // end: VIZISw(w)

      if (Rc.mode_altscr) w = w->next;
   } while (w != Curwin);

#ifdef EQUCOLHDRYES
   /* now we can finally even out column header lengths
      (we're assuming entire columnhdr was memset to '\0') */
   if (Rc.mode_altscr && SCREENMAX > Screen_cols)
      for (i = 0; i < GROUPSMAX; i++) {
         w = &Winstk[i];
         if (CHKw(w, Show_TASKON))
            if (hdrmax + w->hdrcaplen > (x = strlen(w->columnhdr)))
               memset(&w->columnhdr[x], ' ', hdrmax + w->hdrcaplen - x);
      }
#endif

   // finalize/touchup the libproc PROC_FILLxxx flags for current config...
   if (Frames_libflags & L_EITHER) {
      if (!(Frames_libflags & (L_stat | L_status)))
         Frames_libflags |= L_stat;
   }
   if (!Frames_libflags) Frames_libflags = L_DEFAULT;
   if (Monpidsidx) Frames_libflags |= PROC_PID;
} // end: build_headers


        /*
         * This guy coordinates the activities surrounding the maintenance
         * of each visible window's columns headers and the library flags
         * required for the openproc interface. */
static void calibrate_fields (void) {
   FLG_t f;
   char *s;
   const char *h;
   WIN_t *w = Curwin;
   int i, varcolcnt, len;

   adj_geometry();

   do {
      if (VIZISw(w)) {
         w->hdrcaplen = 0;   // really only used with USE_X_COLHDR
         // build window's pflgsall array, establish upper bounds for maxpflgs
         for (i = 0, w->totpflgs = 0; i < EU_MAXPFLGS; i++) {
            if (FLDviz(w, i)) {
               f = FLDget(w, i);
#ifdef USE_X_COLHDR
               w->pflgsall[w->totpflgs++] = f;
#else
               if (CHKw(w, Show_HICOLS) && f == w->rc.sortindx) {
                  w->pflgsall[w->totpflgs++] = EU_XON;
                  w->pflgsall[w->totpflgs++] = f;
                  w->pflgsall[w->totpflgs++] = EU_XOF;
               } else
                  w->pflgsall[w->totpflgs++] = f;
#endif
            }
         }
         if (!w->totpflgs) w->pflgsall[w->totpflgs++] = EU_PID;

         /* build a preliminary columns header not to exceed screen width
            while accounting for a possible leading window number */
         w->varcolsz = varcolcnt = 0;
         *(s = w->columnhdr) = '\0';
         if (Rc.mode_altscr) s = scat(s, " ");
         for (i = 0; i + w->begpflg < w->totpflgs; i++) {
            f = w->pflgsall[i + w->begpflg];
            w->procflgs[i] = f;
#ifndef USE_X_COLHDR
            if (EU_MAXPFLGS <= f) continue;
#endif
            h = N_col(f);
            len = (VARcol(f) ? (int)strlen(h) : Fieldstab[f].width) + COLPADSIZ;
            // oops, won't fit -- we're outta here...
            if (Screen_cols < ((int)(s - w->columnhdr) + len)) break;
            if (VARcol(f)) { ++varcolcnt; w->varcolsz += strlen(h); }
            s = scat(s, fmtmk("%*.*s", len, len, h));
         }
#ifndef USE_X_COLHDR
         if (i >= 1 && EU_XON == w->procflgs[i - 1]) --i;
#endif

         /* establish the final maxpflgs and prepare to grow the variable column
            heading(s) via varcolsz - it may be a fib if their pflags weren't
            encountered, but that's ok because they won't be displayed anyway */
         w->maxpflgs = i;
         w->varcolsz += Screen_cols - strlen(w->columnhdr);
         if (varcolcnt) w->varcolsz /= varcolcnt;

         /* establish the field where all remaining fields would still
            fit within screen width, including a leading window number */
         *(s = w->columnhdr) = '\0';
         if (Rc.mode_altscr) s = scat(s, " ");
         w->endpflg = 0;
         for (i = w->totpflgs - 1; -1 < i; i--) {
            f = w->pflgsall[i];
#ifndef USE_X_COLHDR
            if (EU_MAXPFLGS <= f) { w->endpflg = i; continue; }
#endif
            h = N_col(f);
            len = (VARcol(f) ? (int)strlen(h) : Fieldstab[f].width) + COLPADSIZ;
            if (Screen_cols < ((int)(s - w->columnhdr) + len)) break;
            s = scat(s, fmtmk("%*.*s", len, len, h));
            w->endpflg = i;
         }
#ifndef USE_X_COLHDR
         if (EU_XOF == w->pflgsall[w->endpflg]) ++w->endpflg;
#endif
      } // end: if (VIZISw(w))

      if (Rc.mode_altscr) w = w->next;
   } while (w != Curwin);

   build_headers();
} // end: calibrate_fields


        /*
         * Display each field represented in the current window's fieldscur
         * array along with its description.  Mark with bold and a leading
         * asterisk those fields associated with the "on" or "active" state.
         *
         * Special highlighting will be accorded the "focus" field with such
         * highlighting potentially extended to include the description.
         *
         * Below is the current Fieldstab space requirement and how
         * we apportion it.  The xSUFX is considered sacrificial,
         * something we can reduce or do without.
         *            0        1         2         3
         *            12345678901234567890123456789012
         *            * HEADING = Longest Description!
         *      xPRFX ----------______________________ xSUFX
         *    ( xPRFX has pos 2 & 10 for 'extending' when at minimums )
         *
         * The first 4 screen rows are reserved for explanatory text, and
         * the maximum number of columns is Screen_cols / xPRFX + 1 space
         * between columns.  Thus, for example, with 42 fields a tty will
         * still remain useable under these extremes:
         *       rows       columns     what's
         *       tty  top   tty  top    displayed
         *       ---  ---   ---  ---    ------------------
         *        46   42    10    1    xPRFX only
         *        46   42    32    1    full xPRFX + xSUFX
         *         6    2   231   21    xPRFX only
         *        10    6   231    7    full xPRFX + xSUFX
         */
static void display_fields (int focus, int extend) {
 #define mkERR { putp("\n"); putp(N_txt(XTRA_winsize_txt)); return; }
 #define mxCOL ( (Screen_cols / 11) > 0 ? (Screen_cols / 11) : 1 )
 #define yRSVD  4
 #define xEQUS  2                      // length of suffix beginning '= '
 #define xSUFX  22                     // total suffix length, incl xEQUS
 #define xPRFX (10 + xadd)
 #define xTOTL (xPRFX + xSUFX)
   static int col_sav, row_sav;
   WIN_t *w = Curwin;                  // avoid gcc bloat with a local copy
   int i;                              // utility int (a row, tot cols, ix)
   int smax;                           // printable width of xSUFX
   int xadd = 0;                       // spacing between data columns
   int cmax = Screen_cols;             // total data column width
   int rmax = Screen_rows - yRSVD;     // total useable rows

   i = (EU_MAXPFLGS % mxCOL) ? 1 : 0;
   if (rmax < i + (EU_MAXPFLGS / mxCOL)) mkERR;
   i = EU_MAXPFLGS / rmax;
   if (EU_MAXPFLGS % rmax) ++i;
   if (i > 1) { cmax /= i; xadd = 1; }
   if (cmax > xTOTL) cmax = xTOTL;
   smax = cmax - xPRFX;
   if (smax < 0) mkERR;

   /* we'll go the extra distance to avoid any potential screen flicker
      which occurs under some terminal emulators (but it was our fault) */
   if (col_sav != Screen_cols || row_sav != Screen_rows) {
      col_sav = Screen_cols;
      row_sav = Screen_rows;
      putp(Cap_clr_eos);
   }
   fflush(stdout);

   for (i = 0; i < EU_MAXPFLGS; ++i) {
      int b = FLDviz(w, i), x = (i / rmax) * cmax, y = (i % rmax) + yRSVD;
      const char *e = (i == focus && extend) ? w->capclr_hdr : "";
      FLG_t f = FLDget(w, i);
      char sbuf[xSUFX*4];                        // 4 = max multi-byte
      int xcol, xfld;

      /* prep sacrificial suffix (allowing for beginning '= ')
         note: width passed to 'utf8_embody' may go negative, but he'll be just fine */
      snprintf(sbuf, sizeof(sbuf), "= %.*s", utf8_embody(N_fld(f), smax - xEQUS), N_fld(f));

      // obtain translated deltas (if any) ...
      xcol = utf8_delta(fmtmk("%.*s", utf8_embody(N_col(f), 7), N_col(f)));
      xfld = utf8_delta(sbuf + xEQUS);           // ignore beginning '= '

      PUTT("%s%c%s%s %s%-*.*s%s%s%s %-*.*s%s"
         , tg2(x, y)
         , b ? '*' : ' '
         , b ? w->cap_bold : Cap_norm
         , e
         , i == focus ? w->capclr_hdr : ""
         , 7 + xcol, 7 + xcol
         , N_col(f)
         , Cap_norm
         , b ? w->cap_bold : ""
         , e
         , smax + xfld, smax + xfld
         , sbuf
         , Cap_norm);
   }

   putp(Caps_off);
 #undef mkERR
 #undef mxCOL
 #undef yRSVD
 #undef xEQUS
 #undef xSUFX
 #undef xPRFX
 #undef xTOTL
} // end: display_fields


        /*
         * Manage all fields aspects (order/toggle/sort), for all windows. */
static void fields_utility (void) {
#ifndef SCROLLVAR_NO
 #define unSCRL  { w->begpflg = w->varcolbeg = 0; OFFw(w, Show_HICOLS); }
#else
 #define unSCRL  { w->begpflg = 0; OFFw(w, Show_HICOLS); }
#endif
 #define swapEM  { char c; unSCRL; c = w->rc.fieldscur[i]; \
       w->rc.fieldscur[i] = *p; *p = c; p = &w->rc.fieldscur[i]; }
 #define spewFI  { char *t; f = w->rc.sortindx; t = strchr(w->rc.fieldscur, f + FLD_OFFSET); \
       if (!t) t = strchr(w->rc.fieldscur, (f + FLD_OFFSET) | 0x80); \
       i = (t) ? (int)(t - w->rc.fieldscur) : 0; }
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy
   const char *h = NULL;
   char *p = NULL;
   int i, key;
   FLG_t f;

   spewFI
signify_that:
   putp(Cap_clr_scr);
   adj_geometry();

   do {
      if (!h) h = N_col(f);
      putp(Cap_home);
      show_special(1, fmtmk(N_unq(FIELD_header_fmt)
         , w->grpname, CHKw(w, Show_FOREST) ? N_txt(FOREST_views_txt) : h));
      display_fields(i, (p != NULL));
      fflush(stdout);

      if (Frames_signal) goto signify_that;
      key = iokey(1);
      if (key < 1) goto signify_that;

      switch (key) {
         case kbd_UP:
            if (i > 0) { --i; if (p) swapEM }
            break;
         case kbd_DOWN:
            if (i + 1 < EU_MAXPFLGS) { ++i; if (p) swapEM }
            break;
         case kbd_LEFT:
         case kbd_ENTER:
            p = NULL;
            break;
         case kbd_RIGHT:
            p = &w->rc.fieldscur[i];
            break;
         case kbd_HOME:
         case kbd_PGUP:
            if (!p) i = 0;
            break;
         case kbd_END:
         case kbd_PGDN:
            if (!p) i = EU_MAXPFLGS - 1;
            break;
         case kbd_SPACE:
         case 'd':
            if (!p) { FLDtog(w, i); unSCRL }
            break;
         case 's':
#ifdef TREE_NORESET
            if (!p && !CHKw(w, Show_FOREST)) { w->rc.sortindx = f = FLDget(w, i); h = NULL; unSCRL }
#else
            if (!p) { w->rc.sortindx = f = FLDget(w, i); h = NULL; unSCRL; OFFw(w, Show_FOREST); }
#endif
            break;
         case 'a':
         case 'w':
            Curwin = w = ('a' == key) ? w->next : w->prev;
            spewFI
            h = p = NULL;
            break;
         default:                 // keep gcc happy
            break;
      }
   } while (key != 'q' && key != kbd_ESC);
 #undef unSCRL
 #undef swapEM
 #undef spewFI
} // end: fields_utility


        /*
         * This routine takes care of auto sizing field widths
         * if/when the user sets Rc.fixed_widest to -1.  Along the
         * way he reinitializes some things for the next frame. */
static inline void widths_resize (void) {
   int i;

   // next var may also be set by the guys that actually truncate stuff
   Autox_found = 0;
   for (i = 0; i < EU_MAXPFLGS; i++) {
      if (Autox_array[i]) {
         Fieldstab[i].width++;
         Autox_array[i] = 0;
         Autox_found = 1;
      }
   }
   if (Autox_found) calibrate_fields();
} // end: widths_resize


        /*
         * This routine exists just to consolidate most of the messin'
         * around with the Fieldstab array and some related stuff. */
static void zap_fieldstab (void) {
#ifdef WIDEN_COLUMN
 #define maX(E) ( (wtab[E].wnls > wtab[E].wmin) \
  ? wtab[E].wnls : wtab[E].wmin )
   static struct {
      int wmin;         // minimum field width (-1 == variable width)
      int wnls;         // translated header column requirements
      int watx;         // +1 == non-scalable auto sized columns
   } wtab[EU_MAXPFLGS];
#endif
   static int once;
   unsigned digits;
   char buf[8];
   int i;

   if (!once) {
      Fieldstab[EU_CPN].width = 1;
      Fieldstab[EU_NMA].width = 2;
      Fieldstab[EU_PID].width = Fieldstab[EU_PPD].width
         = Fieldstab[EU_PGD].width = Fieldstab[EU_SID].width
         = Fieldstab[EU_TGD].width = Fieldstab[EU_TPG].width = 5;
      if (5 < (digits = get_pid_digits())) {
         if (10 < digits) error_exit(N_txt(FAIL_widepid_txt));
         Fieldstab[EU_PID].width = Fieldstab[EU_PPD].width
            = Fieldstab[EU_PGD].width = Fieldstab[EU_SID].width
            = Fieldstab[EU_TGD].width = Fieldstab[EU_TPG].width = digits;
      }
#ifdef WIDEN_COLUMN
      // identify our non-scalable auto sized columns
      wtab[EU_UED].watx = wtab[EU_UEN].watx = wtab[EU_URD].watx
         = wtab[EU_URN].watx = wtab[EU_USD].watx = wtab[EU_USN].watx
         = wtab[EU_GID].watx = wtab[EU_GRP].watx = wtab[EU_TTY].watx
         = wtab[EU_WCH].watx = wtab[EU_NS1].watx = wtab[EU_NS2].watx
         = wtab[EU_NS3].watx = wtab[EU_NS4].watx = wtab[EU_NS5].watx
         = wtab[EU_NS6].watx = wtab[EU_LXC].watx = +1;
      /* establish translatable header 'column' requirements
         and ensure .width reflects the widest value */
      for (i = 0; i < EU_MAXPFLGS; i++) {
         wtab[i].wmin = Fieldstab[i].width;
         wtab[i].wnls = (int)strlen(N_col(i)) - utf8_delta(N_col(i));
         if (wtab[i].wmin != -1)
            Fieldstab[i].width = maX(i);
      }
#endif
      once = 1;
   }

   /*** hotplug_acclimated ***/

   Cpu_pmax = 99.9;
   if (Rc.mode_irixps && smp_num_cpus > 1 && !Thread_mode) {
      Cpu_pmax = 100.0 * smp_num_cpus;
      if (smp_num_cpus > 10) {
         if (Cpu_pmax > 99999.0) Cpu_pmax = 99999.0;
      } else {
         if (Cpu_pmax > 999.9) Cpu_pmax = 999.9;
      }
   }

#ifdef WIDEN_COLUMN
   digits = (unsigned)snprintf(buf, sizeof(buf), "%u", (unsigned)smp_num_cpus);
   if (wtab[EU_CPN].wmin < digits) {
      if (5 < digits) error_exit(N_txt(FAIL_widecpu_txt));
      wtab[EU_CPN].wmin = digits;
      Fieldstab[EU_CPN].width = maX(EU_CPN);
   }
   digits = (unsigned)snprintf(buf, sizeof(buf), "%u", (unsigned)Numa_node_tot);
   if (wtab[EU_NMA].wmin < digits) {
      wtab[EU_NMA].wmin = digits;
      Fieldstab[EU_NMA].width = maX(EU_NMA);
   }

   // and accommodate optional wider non-scalable columns (maybe)
   if (!AUTOX_MODE) {
      for (i = 0; i < EU_MAXPFLGS; i++) {
         if (wtab[i].watx)
            Fieldstab[i].width = Rc.fixed_widest ? Rc.fixed_widest + maX(i) : maX(i);
      }
   }
#else
   digits = (unsigned)snprintf(buf, sizeof(buf), "%u", (unsigned)smp_num_cpus);
   if (1 < digits) {
      if (5 < digits) error_exit(N_txt(FAIL_widecpu_txt));
      Fieldstab[EU_CPN].width = digits;
   }
   digits = (unsigned)snprintf(buf, sizeof(buf), "%u", (unsigned)Numa_node_tot);
   if (2 < digits) {
      Fieldstab[EU_NMA].width = digits;
   }
   // and accommodate optional wider non-scalable columns (maybe)
   if (!AUTOX_MODE) {
      Fieldstab[EU_UED].width = Fieldstab[EU_URD].width
         = Fieldstab[EU_USD].width = Fieldstab[EU_GID].width
         = Rc.fixed_widest ? 5 + Rc.fixed_widest : 5;
      Fieldstab[EU_UEN].width = Fieldstab[EU_URN].width
         = Fieldstab[EU_USN].width = Fieldstab[EU_GRP].width
         = Rc.fixed_widest ? 8 + Rc.fixed_widest : 8;
      Fieldstab[EU_TTY].width = Fieldstab[EU_LXC].width
         = Rc.fixed_widest ? 8 + Rc.fixed_widest : 8;
      Fieldstab[EU_WCH].width
         = Rc.fixed_widest ? 10 + Rc.fixed_widest : 10;
      for (i = EU_NS1; i < EU_NS1 + NUM_NS; i++)
         Fieldstab[i].width
            = Rc.fixed_widest ? 10 + Rc.fixed_widest : 10;
   }
#endif

   /* plus user selectable scaling */
   Fieldstab[EU_VRT].scale = Fieldstab[EU_SWP].scale
      = Fieldstab[EU_RES].scale = Fieldstab[EU_COD].scale
      = Fieldstab[EU_DAT].scale = Fieldstab[EU_SHR].scale
      = Fieldstab[EU_USE].scale = Fieldstab[EU_RZA].scale
      = Fieldstab[EU_RZF].scale = Fieldstab[EU_RZL].scale
      = Fieldstab[EU_RZS].scale = Rc.task_mscale;

   // lastly, ensure we've got proper column headers...
   calibrate_fields();
 #undef maX
} // end: zap_fieldstab

/*######  Library Interface  #############################################*/

        /*
         * This guy's modeled on libproc's 'eight_cpu_numbers' function except
         * we preserve all cpu data in our CPU_t array which is organized
         * as follows:
         *    Cpu_tics[0] thru Cpu_tics[n] == tics for each separate cpu
         *    Cpu_tics[sumSLOT]            == tics from /proc/stat line #1
         *  [ and beyond sumSLOT           == tics for each cpu NUMA node ] */
static void cpus_refresh (void) {
 #define sumSLOT ( smp_num_cpus )
 #define totSLOT ( 1 + smp_num_cpus + Numa_node_tot)
   static FILE *fp = NULL;
   static int siz, sav_slot = -1;
   static char *buf;
   CPU_t *sum_ptr;                               // avoid gcc subscript bloat
   int i, num, tot_read;
   int node;
   char *bp;

   /*** hotplug_acclimated ***/
   if (sav_slot != sumSLOT) {
      sav_slot = sumSLOT;
      zap_fieldstab();
      if (fp) { fclose(fp); fp = NULL; }
      if (Cpu_tics) free(Cpu_tics);
   }

   /* by opening this file once, we'll avoid the hit on minor page faults
      (sorry Linux, but you'll have to close it for us) */
   if (!fp) {
      if (!(fp = fopen("/proc/stat", "r")))
         error_exit(fmtmk(N_fmt(FAIL_statopn_fmt), strerror(errno)));
      /* note: we allocate one more CPU_t via totSLOT than 'cpus' so that a
               slot can hold tics representing the /proc/stat cpu summary */
      Cpu_tics = alloc_c(totSLOT * sizeof(CPU_t));
   }
   rewind(fp);
   fflush(fp);

 #define buffGRW 1024
   /* we slurp in the entire directory thus avoiding repeated calls to fgets,
      especially in a massively parallel environment.  additionally, each cpu
      line is then frozen in time rather than changing until we get around to
      accessing it.  this helps to minimize (not eliminate) most distortions. */
   tot_read = 0;
   if (buf) buf[0] = '\0';
   else buf = alloc_c((siz = buffGRW));
   while (0 < (num = fread(buf + tot_read, 1, (siz - tot_read), fp))) {
      tot_read += num;
      if (tot_read < siz) break;
      buf = alloc_r(buf, (siz += buffGRW));
   };
   buf[tot_read] = '\0';
   bp = buf;
 #undef buffGRW

   // remember from last time around
   sum_ptr = &Cpu_tics[sumSLOT];
   memcpy(&sum_ptr->sav, &sum_ptr->cur, sizeof(CT_t));
   // then value the last slot with the cpu summary line
   if (4 > sscanf(bp, "cpu %llu %llu %llu %llu %llu %llu %llu %llu"
      , &sum_ptr->cur.u, &sum_ptr->cur.n, &sum_ptr->cur.s
      , &sum_ptr->cur.i, &sum_ptr->cur.w, &sum_ptr->cur.x
      , &sum_ptr->cur.y, &sum_ptr->cur.z))
         error_exit(N_txt(FAIL_statget_txt));
#ifndef CPU_ZEROTICS
   sum_ptr->cur.tot = sum_ptr->cur.u + sum_ptr->cur.s
      + sum_ptr->cur.n + sum_ptr->cur.i + sum_ptr->cur.w
      + sum_ptr->cur.x + sum_ptr->cur.y + sum_ptr->cur.z;
   /* if a cpu has registered substantially fewer tics than those expected,
      we'll force it to be treated as 'idle' so as not to present misleading
      percentages. */
   sum_ptr->edge =
      ((sum_ptr->cur.tot - sum_ptr->sav.tot) / smp_num_cpus) / (100 / TICS_EDGE);
#endif

   // forget all of the prior node statistics (maybe)
   if (CHKw(Curwin, View_CPUNOD) && Numa_node_tot)
      memset(sum_ptr + 1, 0, Numa_node_tot * sizeof(CPU_t));

   // now value each separate cpu's tics...
   for (i = 0; i < sumSLOT; i++) {
      CPU_t *cpu_ptr = &Cpu_tics[i];           // avoid gcc subscript bloat
#ifdef PRETEND8CPUS
      bp = buf;
#endif
      bp = 1 + strchr(bp, '\n');
      // remember from last time around
      memcpy(&cpu_ptr->sav, &cpu_ptr->cur, sizeof(CT_t));
      if (4 > sscanf(bp, "cpu%d %llu %llu %llu %llu %llu %llu %llu %llu", &cpu_ptr->id
         , &cpu_ptr->cur.u, &cpu_ptr->cur.n, &cpu_ptr->cur.s
         , &cpu_ptr->cur.i, &cpu_ptr->cur.w, &cpu_ptr->cur.x
         , &cpu_ptr->cur.y, &cpu_ptr->cur.z)) {
            break;        // tolerate cpus taken offline
      }

#ifndef CPU_ZEROTICS
      cpu_ptr->edge = sum_ptr->edge;
#endif
#ifdef PRETEND8CPUS
      cpu_ptr->id = i;
#endif
      /* henceforth, with just a little more arithmetic we can avoid
         maintaining *any* node stats unless they're actually needed */
      if (CHKw(Curwin, View_CPUNOD)
      && Numa_node_tot
      && -1 < (node = numa_node_of_cpu(cpu_ptr->id))) {
         // use our own pointer to avoid gcc subscript bloat
         CPU_t *nod_ptr = sum_ptr + 1 + node;
         nod_ptr->cur.u += cpu_ptr->cur.u; nod_ptr->sav.u += cpu_ptr->sav.u;
         nod_ptr->cur.n += cpu_ptr->cur.n; nod_ptr->sav.n += cpu_ptr->sav.n;
         nod_ptr->cur.s += cpu_ptr->cur.s; nod_ptr->sav.s += cpu_ptr->sav.s;
         nod_ptr->cur.i += cpu_ptr->cur.i; nod_ptr->sav.i += cpu_ptr->sav.i;
         nod_ptr->cur.w += cpu_ptr->cur.w; nod_ptr->sav.w += cpu_ptr->sav.w;
         nod_ptr->cur.x += cpu_ptr->cur.x; nod_ptr->sav.x += cpu_ptr->sav.x;
         nod_ptr->cur.y += cpu_ptr->cur.y; nod_ptr->sav.y += cpu_ptr->sav.y;
         nod_ptr->cur.z += cpu_ptr->cur.z; nod_ptr->sav.z += cpu_ptr->sav.z;
#ifndef CPU_ZEROTICS
         /* yep, we re-value this repeatedly for each cpu encountered, but we
            can then avoid a prior loop to selectively initialize each node */
         nod_ptr->edge = sum_ptr->edge;
#endif
         cpu_ptr->node = node;
#ifndef OFF_NUMASKIP
         nod_ptr->id = -1;
#endif
      }
   } // end: for each cpu

   Cpu_faux_tot = i;      // tolerate cpus taken offline
 #undef sumSLOT
 #undef totSLOT
} // end: cpus_refresh


#ifdef OFF_HST_HASH
        /*
         * Binary Search for HST_t's put/get support */

static inline HST_t *hstbsrch (HST_t *hst, int max, int pid) {
   int mid, min = 0;

   while (min <= max) {
      mid = (min + max) / 2;
      if (pid < hst[mid].pid) max = mid - 1;
      else if (pid > hst[mid].pid) min = mid + 1;
      else return &hst[mid];
   }
   return NULL;
} // end: hstbsrch

#else
        /*
         * Hashing functions for HST_t's put/get support
         * (not your normal 'chaining', those damn HST_t's might move!) */

#define _HASH_(K) (K & (HHASH_SIZ - 1))

static inline HST_t *hstget (int pid) {
   int V = PHash_sav[_HASH_(pid)];

   while (-1 < V) {
      if (PHist_sav[V].pid == pid) return &PHist_sav[V];
      V = PHist_sav[V].lnk; }
   return NULL;
} // end: hstget


static inline void hstput (unsigned idx) {
   int V = _HASH_(PHist_new[idx].pid);

   PHist_new[idx].lnk = PHash_new[V];
   PHash_new[V] = idx;
} // end: hstput

#undef _HASH_
#endif

        /*
         * Refresh procs *Helper* function to eliminate yet one more need
         * to loop through our darn proc_t table.  He's responsible for:
         *    1) calculating the elapsed time since the previous frame
         *    2) counting the number of tasks in each state (run, sleep, etc)
         *    3) maintaining the HST_t's and priming the proc_t pcpu field
         *    4) establishing the total number tasks for this frame */
static void procs_hlp (proc_t *this) {
#ifdef OFF_HST_HASH
   static unsigned maxt_sav = 0;        // prior frame's max tasks
#endif
   TIC_t tics;
   HST_t *h;

   if (!this) {
      static double uptime_sav;
      double uptime_cur;
      float et;
      void *v;

      uptime(&uptime_cur, NULL);
      et = uptime_cur - uptime_sav;
      if (et < 0.01) et = 0.005;
      uptime_sav = uptime_cur;

      // if in Solaris mode, adjust our scaling for all cpus
      Frame_etscale = 100.0f / ((float)Hertz * (float)et * (Rc.mode_irixps ? 1 : smp_num_cpus));
#ifdef OFF_HST_HASH
      maxt_sav = Frame_maxtask;
#endif
      Frame_maxtask = Frame_running = Frame_sleepin = Frame_stopped = Frame_zombied = 0;

      // prep for saving this frame's HST_t's (and reuse mem each time around)
      v = PHist_sav;
      PHist_sav = PHist_new;
      PHist_new = v;
#ifdef OFF_HST_HASH
      // prep for binary search by sorting the last frame's HST_t's
      qsort(PHist_sav, maxt_sav, sizeof(HST_t), (QFP_t)sort_HST_t);
#else
      v = PHash_sav;
      PHash_sav = PHash_new;
      PHash_new = v;
      memcpy(PHash_new, HHash_nul, sizeof(HHash_nul));
#endif
      return;
   }

   switch (this->state) {
      case 'R':
         Frame_running++;
         break;
      case 't':     // 't' (tracing stop)
      case 'T':
         Frame_stopped++;
         break;
      case 'Z':
         Frame_zombied++;
         break;
      default:
         /* currently: 'D' (disk sleep),
                       'I' (idle),
                       'P' (parked),
                       'S' (sleeping),
                       'X' (dead - actually 'dying' & probably never seen)
         */
         Frame_sleepin++;
         break;
   }

   if (Frame_maxtask+1 >= HHist_siz) {
      /* we're subject to integer overflow if total linux tasks ever approach |
         400+ million (but, do you think memory might be the bigger problem?) | */
      HHist_siz = HHist_siz * 5 / 4 + 100;
      PHist_sav = alloc_r(PHist_sav, sizeof(HST_t) * HHist_siz);
      PHist_new = alloc_r(PHist_new, sizeof(HST_t) * HHist_siz);
   }

   /* calculate time in this process; the sum of user time (utime) and
      system time (stime) -- but PLEASE dont waste time and effort on
      calcs and saves that go unused, like the old top! */
   PHist_new[Frame_maxtask].pid  = this->tid;
   PHist_new[Frame_maxtask].tics = tics = (this->utime + this->stime);
   // finally, save major/minor fault counts in case the deltas are displayable
   PHist_new[Frame_maxtask].maj = this->maj_flt;
   PHist_new[Frame_maxtask].min = this->min_flt;

#ifdef OFF_HST_HASH
   // find matching entry from previous frame and make stuff elapsed
   if ((h = hstbsrch(PHist_sav, maxt_sav - 1, this->tid))) {
      tics -= h->tics;
      this->maj_delta = this->maj_flt - h->maj;
      this->min_delta = this->min_flt - h->min;
   }
#else
   // hash & save for the next frame
   hstput(Frame_maxtask);
   // find matching entry from previous frame and make stuff elapsed
   if ((h = hstget(this->tid))) {
      tics -= h->tics;
      this->maj_delta = this->maj_flt - h->maj;
      this->min_delta = this->min_flt - h->min;
   }
#endif

   /* we're just saving elapsed tics, to be converted into %cpu if
      this task wins it's displayable screen row lottery... */
   this->pcpu = tics;

   // shout this to the world with the final call (or us the next time in)
   Frame_maxtask++;
} // end: procs_hlp


        /*
         * This guy's modeled on libproc's 'readproctab' function except
         * we reuse and extend any prior proc_t's.  He's been customized
         * for our specific needs and to avoid the use of <stdarg.h> */
static void procs_refresh (void) {
 #define n_used  Frame_maxtask                   // maintained by procs_hlp()
   static proc_t **private_ppt;                  // our base proc_t ptr table
   static int n_alloc = 0;                       // size of our private_ppt
   static int n_saved = 0;                       // last window ppt size
   proc_t *ptask;
   PROCTAB* PT;
   int i;
   proc_t *(*read_something)(PROCTAB*, proc_t *);

   procs_hlp(NULL);                              // prep for a new frame
   if (NULL == (PT = openproc(Frames_libflags, Monpids)))
      error_exit(fmtmk(N_fmt(FAIL_openlib_fmt), strerror(errno)));
   read_something = Thread_mode ? readeither : readproc;

   for (;;) {
      if (n_used == n_alloc) {
         /* we're subject to integer overflow if total linux tasks ever approach |
            400+ million (but, do you think memory might be the bigger problem?) | */
         n_alloc = 10 + ((n_alloc * 5) / 4);     // grow by over 25%
         private_ppt = alloc_r(private_ppt, sizeof(proc_t *) * n_alloc);
         // ensure NULL pointers for the additional memory just acquired
         memset(private_ppt + n_used, 0, sizeof(proc_t *) * (n_alloc - n_used));
      }
      // on the way to n_alloc, the library will allocate the underlying
      // proc_t storage whenever our private_ppt[] pointer is NULL...
      if (!(ptask = read_something(PT, private_ppt[n_used]))) break;
      procs_hlp((private_ppt[n_used] = ptask));  // tally this proc_t
   }

   closeproc(PT);

   // lastly, refresh each window's proc pointers table...
   if (n_saved == n_alloc)
      for (i = 0; i < GROUPSMAX; i++)
         memcpy(Winstk[i].ppt, private_ppt, sizeof(proc_t *) * n_used);
   else {
      n_saved = n_alloc;
      for (i = 0; i < GROUPSMAX; i++) {
         Winstk[i].ppt = alloc_r(Winstk[i].ppt, sizeof(proc_t *) * n_alloc);
         memcpy(Winstk[i].ppt, private_ppt, sizeof(proc_t *) * n_used);
      }
   }
 #undef n_used
} // end: procs_refresh


        /*
         * This serves as our interface to the memory & cpu count (sysinfo)
         * portion of libproc.  In support of those hotpluggable resources,
         * the sampling frequencies are reduced so as to minimize overhead. */
static void sysinfo_refresh (int forced) {
   static time_t sav_secs;
   time_t cur_secs;

   if (forced)
      sav_secs = 0;
   cur_secs = time(NULL);

   /*** hotplug_acclimated ***/
   if (3 <= cur_secs - sav_secs) {
      meminfo();
#ifndef PRETEND8CPUS
      cpuinfo();
#endif
      Numa_node_tot = numa_max_node() + 1;
      sav_secs = cur_secs;
   }
} // end: sysinfo_refresh

/*######  Inspect Other Output  ##########################################*/

        /*
         * HOWTO Extend the top 'inspect' functionality:
         *
         * To exploit the 'Y' interactive command, one must add entries to
         * the top personal configuration file.  Such entries simply reflect
         * a file to be read or command/pipeline to be executed whose results
         * will then be displayed in a separate scrollable window.
         *
         * Entries beginning with a '#' character are ignored, regardless of
         * content.  Otherwise they consist of the following 3 elements, each
         * of which must be separated by a tab character (thus 2 '\t' total):
         *     type:  literal 'file' or 'pipe'
         *     name:  selection shown on the Inspect screen
         *     fmts:  string representing a path or command
         *
         * The two types of Inspect entries are not interchangeable.
         * Those designated 'file' will be accessed using fopen/fread and must
         * reference a single file in the 'fmts' element.  Entries specifying
         * 'pipe' will employ popen/fread, their 'fmts' element could contain
         * many pipelined commands and, none can be interactive.
         *
         * Here are some examples of both types of inspection entries.
         * The first entry will be ignored due to the initial '#' character.
         * For clarity, the pseudo tab depictions (^I) are surrounded by an
         * extra space but the actual tabs would not be.
         *
         *     # pipe ^I Sockets ^I lsof -n -P -i 2>&1
         *     pipe ^I Open Files ^I lsof -P -p %d 2>&1
         *     file ^I NUMA Info ^I /proc/%d/numa_maps
         *     pipe ^I Log ^I tail -n100 /var/log/syslog | sort -Mr
         *
         * Caution:  If the output contains unprintable characters they will
         * be displayed in either the ^I notation or hexidecimal <FF> form.
         * This applies to tab characters as well.  So if one wants a more
         * accurate display, any tabs should be expanded within the 'fmts'.
         *
         * The following example takes what could have been a 'file' entry
         * but employs a 'pipe' instead so as to expand the tabs.
         *
         *     # next would have contained '\t' ...
         *     # file ^I <your_name> ^I /proc/%d/status
         *     # but this will eliminate embedded '\t' ...
         *     pipe ^I <your_name> ^I cat /proc/%d/status | expand -
         *
         * Note: If a pipe such as the following was established, one must
         * use Ctrl-C to terminate that pipe in order to review the results.
         * This is the single occasion where a '^C' will not terminate top.
         *
         *     pipe ^I Trace ^I /usr/bin/strace -p %d 2>&1
         */

        /*
         * Our driving table support, the basis for generalized inspection,
         * built at startup (if at all) from rcfile or demo entries. */
struct I_ent {
   void (*func)(char *, int);     // a pointer to file/pipe/demo function
   char *type;                    // the type of entry ('file' or 'pipe')
   char *name;                    // the selection label for display
   char *fmts;                    // format string to build path or command
   int   farg;                    // 1 = '%d' in fmts, 0 = not (future use)
   const char *caps;              // not really caps, show_special() delim's
   char *fstr;                    // entry's current/active search string
   int   flen;                    // above's strlen, without call overhead
};
struct I_struc {
   int demo;                      // do NOT save table entries in rcfile
   int total;                     // total I_ent table entries
   char *raw;                     // all entries for 'W', incl '#' & blank
   struct I_ent *tab;
};
static struct I_struc Inspect;

static char   **Insp_p;           // pointers to each line start
static int      Insp_nl;          // total lines, total Insp_p entries
static int      Insp_utf8;        // treat Insp_buf as translatable, else raw
static char    *Insp_buf;         // the results from insp_do_file/pipe
static size_t   Insp_bufsz;       // allocated size of Insp_buf
static size_t   Insp_bufrd;       // bytes actually in Insp_buf
static struct I_ent *Insp_sel;    // currently selected Inspect entry

        // Our 'make status line' macro
#define INSP_MKSL(big,txt) { int _sz = big ? Screen_cols : 80; \
   const char *_p; \
   _sz += utf8_delta(txt); \
   _p = fmtmk("%-*.*s", _sz, _sz, txt); \
   PUTT("%s%s%.*s%s", tg2(0, (Msg_row = 3)), Curwin->capclr_hdr \
      , utf8_embody(_p, Screen_cols), _p, Cap_clr_eol); \
   putp(Caps_off); fflush(stdout); }

        // Our 'row length' macro, equivalent to a strlen() call
#define INSP_RLEN(idx) (int)(Insp_p[idx +1] - Insp_p[idx] -1)

        // Our 'busy/working' macro
#define INSP_BUSY(enu)  { INSP_MKSL(0, N_txt(enu)) }


        /*
         * Establish the number of lines present in the Insp_buf glob plus
         * build the all important row start array.  It is that array that
         * others will rely on since we dare not try to use strlen() on what
         * is potentially raw binary data.  Who knows what some user might
         * name as a file or include in a pipeline (scary, ain't it?). */
static void insp_cnt_nl (void) {
   char *beg = Insp_buf;
   char *cur = Insp_buf;
   char *end = Insp_buf + Insp_bufrd + 1;

#ifdef INSP_SAVEBUF
{
   static int n = 1;
   char fn[SMLBUFSIZ];
   FILE *fd;
   snprintf(fn, sizeof(fn), "%s.Insp_buf.%02d.txt", Myname, n++);
   fd = fopen(fn, "w");
   if (fd) {
      fwrite(Insp_buf, 1, Insp_bufrd, fd);
      fclose(fd);
   }
}
#endif
   Insp_p = alloc_c(sizeof(char *) * 2);

   for (Insp_nl = 0; beg < end; beg++) {
      if (*beg == '\n') {
         Insp_p[Insp_nl++] = cur;
         // keep our array ahead of next potential need (plus the 2 above)
         Insp_p = alloc_r(Insp_p, (sizeof(char *) * (Insp_nl +3)));
         cur = beg +1;
      }
   }
   Insp_p[0] = Insp_buf;
   Insp_p[Insp_nl++] = cur;
   Insp_p[Insp_nl] = end;
   if ((end - cur) == 1)          // if there's an eof null delimiter,
      --Insp_nl;                  // don't count it as a new line
} // end: insp_cnt_nl


#ifndef INSP_OFFDEMO
        /*
         * The pseudo output DEMO utility. */
static void insp_do_demo (char *fmts, int pid) {
   (void)fmts; (void)pid;
   /* next will put us on a par with the real file/pipe read buffers
    ( and also avoid a harmless, but evil sounding, valgrind warning ) */
   Insp_bufsz = READMINSZ + strlen(N_txt(YINSP_dstory_txt));
   Insp_buf   = alloc_c(Insp_bufsz);
   Insp_bufrd = snprintf(Insp_buf, Insp_bufsz, "%s", N_txt(YINSP_dstory_txt));
   insp_cnt_nl();
} // end: insp_do_demo
#endif


        /*
         * The generalized FILE utility. */
static void insp_do_file (char *fmts, int pid) {
   char buf[LRGBUFSIZ];
   FILE *fp;
   int rc;

   snprintf(buf, sizeof(buf), fmts, pid);
   fp = fopen(buf, "r");
   rc = readfile(fp, &Insp_buf, &Insp_bufsz, &Insp_bufrd);
   if (fp) fclose(fp);
   if (rc) Insp_bufrd = snprintf(Insp_buf, Insp_bufsz, "%s"
      , fmtmk(N_fmt(YINSP_failed_fmt), strerror(errno)));
   insp_cnt_nl();
} // end: insp_do_file


        /*
         * The generalized PIPE utility. */
static void insp_do_pipe (char *fmts, int pid) {
   char buf[LRGBUFSIZ];
   struct sigaction sa;
   FILE *fp;
   int rc;

   memset(&sa, 0, sizeof(sa));
   sigemptyset(&sa.sa_mask);
   sa.sa_handler = SIG_IGN;
   sigaction(SIGINT, &sa, NULL);

   snprintf(buf, sizeof(buf), fmts, pid);
   fp = popen(buf, "r");
   rc = readfile(fp, &Insp_buf, &Insp_bufsz, &Insp_bufrd);
   if (fp) pclose(fp);
   if (rc) Insp_bufrd = snprintf(Insp_buf, Insp_bufsz, "%s"
      , fmtmk(N_fmt(YINSP_failed_fmt), strerror(errno)));
   insp_cnt_nl();

   sa.sa_handler = sig_endpgm;
   sigaction(SIGINT, &sa, NULL);
} // end: insp_do_pipe


        /*
         * This guy is a *Helper* function serving the following two masters:
         *   insp_find_str() - find the next Insp_sel->fstr match
         *   insp_mkrow_...  - highlight any Insp_sel->fstr matches in-view
         * If Insp_sel->fstr is found in the designated row, he returns the
         * offset from the start of the row, otherwise he returns a huge
         * integer so traditional fencepost usage can be employed. */
static inline int insp_find_ofs (int col, int row) {
 #define begFS (int)(fnd - Insp_p[row])
   char *p, *fnd = NULL;

   if (Insp_sel->fstr[0]) {
      // skip this row, if there's no chance of a match
      if (memchr(Insp_p[row], Insp_sel->fstr[0], INSP_RLEN(row))) {
         for ( ; col < INSP_RLEN(row); col++) {
            if (!*(p = Insp_p[row] + col))       // skip any empty strings
               continue;
            fnd = STRSTR(p, Insp_sel->fstr);     // with binary data, each
            if (fnd)                             // row may have '\0'.  so
               break;                            // our scans must be done
            col += strlen(p);                    // as individual strings.
         }
         if (fnd && fnd < Insp_p[row + 1])       // and, we must watch out
            return begFS;                        // for potential overrun!
      }
   }
   return INT_MAX;
 #undef begFS
} // end: insp_find_ofs


        /*
         * This guy supports the inspect 'L' and '&' search provisions
         * and returns the row and *optimal* column for viewing any match
         * ( we'll always opt for left column justification since any )
         * ( preceding ctrl chars appropriate an unpredictable amount ) */
static void insp_find_str (int ch, int *col, int *row) {
 #define reDUX (found) ? N_txt(WORD_another_txt) : ""
   static int found;

   if ((ch == '&' || ch == 'n') && !Insp_sel->fstr[0]) {
      show_msg(N_txt(FIND_no_next_txt));
      return;
   }
   if (ch == 'L' || ch == '/') {
      char *str = ioline(N_txt(GET_find_str_txt));
      if (*str == kbd_ESC) return;
      snprintf(Insp_sel->fstr, FNDBUFSIZ, "%s", str);
      Insp_sel->flen = strlen(Insp_sel->fstr);
      found = 0;
   }
   if (Insp_sel->fstr[0]) {
      int xx, yy;

      INSP_BUSY(YINSP_waitin_txt);
      for (xx = *col, yy = *row; yy < Insp_nl; ) {
         xx = insp_find_ofs(xx, yy);
         if (xx < INSP_RLEN(yy)) {
            found = 1;
            if (xx == *col &&  yy == *row) {     // matched where we were!
               ++xx;                             // ( was the user maybe )
               continue;                         // ( trying to fool us? )
            }
            *col = xx;
            *row = yy;
            return;
         }
         xx = 0;
         ++yy;
      }
      show_msg(fmtmk(N_fmt(FIND_no_find_fmt), reDUX, Insp_sel->fstr));
   }
 #undef reDUX
} // end: insp_find_str


        /*
         * This guy is a *Helper* function responsible for positioning a
         * single row in the current 'X axis', then displaying the results.
         * Along the way, he makes sure control characters and/or unprintable
         * characters display in a less-like fashion:
         *    '^A'    for control chars
         *    '<BC>'  for other unprintable stuff
         * Those will be highlighted with the current windows's capclr_msg,
         * while visible search matches display with capclr_hdr for emphasis.
         * ( we hide ugly plumbing in macros to concentrate on the algorithm ) */
static void insp_mkrow_raw (int col, int row) {
 #define maxSZ ( Screen_cols - to )
 #define capNO { if (hicap) { putp(Caps_off); hicap = 0; } }
 #define mkFND { PUTT("%s%.*s%s", Curwin->capclr_hdr, maxSZ, Insp_sel->fstr, Caps_off); \
    fr += Insp_sel->flen -1; to += Insp_sel->flen; hicap = 0; }
#ifndef INSP_JUSTNOT
 #define mkCTL { const char *p = fmtmk("^%c", uch + '@'); \
    PUTT("%s%.*s", (!hicap) ? Curwin->capclr_msg : "", maxSZ, p); to += 2; hicap = 1; }
 #define mkUNP { const char *p = fmtmk("<%02X>", uch); \
    PUTT("%s%.*s", (!hicap) ? Curwin->capclr_msg : "", maxSZ, p); to += 4; hicap = 1; }
#else
 #define mkCTL { if ((to += 2) <= Screen_cols) \
    PUTT("%s^%c", (!hicap) ? Curwin->capclr_msg : "", uch + '@'); hicap = 1; }
 #define mkUNP { if ((to += 4) <= Screen_cols) \
    PUTT("%s<%02X>", (!hicap) ? Curwin->capclr_msg : "", uch); hicap = 1; }
#endif
 #define mkSTD { capNO; if (++to <= Screen_cols) { static char _str[2]; \
    _str[0] = uch; putp(_str); } }
   unsigned char tline[SCREENMAX];
   int fr, to, ofs;
   int hicap = 0;

   if (col < INSP_RLEN(row))
      memcpy(tline, Insp_p[row] + col, sizeof(tline));
   else tline[0] = '\n';

   for (fr = 0, to = 0, ofs = 0; to < Screen_cols; fr++) {
      if (!ofs)
         ofs = insp_find_ofs(col + fr, row);
      if (col + fr < ofs) {
         unsigned char uch = tline[fr];
         if (uch == '\n')   break;     // a no show  (he,he)
         if (uch > 126)     mkUNP      // show as: '<AB>'
         else if (uch < 32) mkCTL      // show as:  '^C'
         else               mkSTD      // a show off (he,he)
      } else {              mkFND      // a big show (he,he)
         ofs = 0;
      }
      if (col + fr >= INSP_RLEN(row)) break;
   }
   capNO;
   putp(Cap_clr_eol);

 #undef maxSZ
 #undef capNO
 #undef mkFND
 #undef mkCTL
 #undef mkUNP
 #undef mkSTD
} // end: insp_mkrow_raw


        /*
         * This guy is a *Helper* function responsible for positioning a
         * single row in the current 'X axis' within a multi-byte string
         * then displaying the results. Along the way he ensures control
         * characters will then be displayed in two positions like '^A'.
         * ( assuming they can even get past those 'gettext' utilities ) */
static void insp_mkrow_utf8 (int col, int row) {
 #define maxSZ ( Screen_cols - to )
 #define mkFND { PUTT("%s%.*s%s", Curwin->capclr_hdr, maxSZ, Insp_sel->fstr, Caps_off); \
    fr += Insp_sel->flen; to += Insp_sel->flen; }
#ifndef INSP_JUSTNOT
 #define mkCTL { const char *p = fmtmk("^%c", uch + '@'); \
    PUTT("%s%.*s%s", Curwin->capclr_msg, maxSZ, p, Caps_off); to += 2; }
#else
 #define mkCTL { if ((to += 2) <= Screen_cols) \
    PUTT("%s^%c%s", Curwin->capclr_msg, uch + '@', Caps_off); }
#endif
 #define mkNUL { buf1[0] = ' '; doPUT(buf1) }
 #define doPUT(buf) if ((to += cno) <= Screen_cols) putp(buf);
   static char buf1[2], buf2[3], buf3[4], buf4[5];
   unsigned char tline[BIGBUFSIZ];
   int fr, to, ofs;

   col = utf8_proper_col(Insp_p[row], col, 1);
   if (col < INSP_RLEN(row))
      memcpy(tline, Insp_p[row] + col, sizeof(tline));
   else tline[0] = '\n';

   for (fr = 0, to = 0, ofs = 0; to < Screen_cols; ) {
      if (!ofs)
         ofs = insp_find_ofs(col + fr, row);
      if (col + fr < ofs) {
         unsigned char uch = tline[fr];
         int bno = UTF8_tab[uch];
         int cno = utf8_cols(&tline[fr++], bno);
         switch (bno) {
            case 1:
               if (uch == '\n') break;
               if (uch < 32) mkCTL
               else if (uch == 127) mkNUL
               else { buf1[0] = uch; doPUT(buf1) }
               break;
            case 2:
               buf2[0] = uch; buf2[1] = tline[fr++];
               doPUT(buf2)
               break;
            case 3:
               buf3[0] = uch; buf3[1] = tline[fr++]; buf3[2] = tline[fr++];
               doPUT(buf3)
               break;
            case 4:
               buf4[0] = uch; buf4[1] = tline[fr++]; buf4[2] = tline[fr++]; buf4[3] = tline[fr++];
               doPUT(buf4)
               break;
            default:
               mkNUL
               break;
         }
      } else {
         mkFND
         ofs = 0;
      }
      if (col + fr >= INSP_RLEN(row)) break;
   }
   putp(Cap_clr_eol);

 #undef maxSZ
 #undef mkFND
 #undef mkCTL
 #undef mkNUL
 #undef doPUT
} // end: insp_mkrow_utf8


        /*
         * This guy is an insp_view_choice() *Helper* function who displays
         * a page worth of of the user's damages.  He also creates a status
         * line based on maximum digits for the current selection's lines and
         * hozizontal position (so it serves to inform, not distract, by
         * otherwise being jumpy). */
static inline void insp_show_pgs (int col, int row, int max) {
   char buf[SMLBUFSIZ];
   void (*mkrow_func)(int, int);
   int r = snprintf(buf, sizeof(buf), "%d", Insp_nl);
   int c = snprintf(buf, sizeof(buf), "%d", col +Screen_cols);
   int l = row +1, ls = Insp_nl;;

   if (!Insp_bufrd)
      l = ls = 0;
   snprintf(buf, sizeof(buf), N_fmt(YINSP_status_fmt)
      , Insp_sel->name
      , r, l, r, ls
      , c, col + 1, c, col + Screen_cols
      , (unsigned long)Insp_bufrd);
   INSP_MKSL(0, buf);

   mkrow_func = Insp_utf8 ? insp_mkrow_utf8 : insp_mkrow_raw;

   for ( ; max && row < Insp_nl; row++) {
      putp("\n");
      mkrow_func(col, row);
      --max;
   }

   if (max)
      putp(Cap_nl_clreos);
} // end: insp_show_pgs


        /*
         * This guy is responsible for displaying the Insp_buf contents and
         * managing all scrolling/locate requests until the user gives up. */
static int insp_view_choice (proc_t *p) {
#ifdef INSP_SLIDE_1
 #define hzAMT  1
#else
 #define hzAMT  8
#endif
 #define maxLN (Screen_rows - (Msg_row +1))
 #define makHD(b1,b2) { \
    snprintf(b1, sizeof(b1), "%d", p->tid); \
    snprintf(b2, sizeof(b2), "%s", p->cmd); }
 #define makFS(dst) { if (Insp_sel->flen < 22) \
       snprintf(dst, sizeof(dst), "%s", Insp_sel->fstr); \
    else snprintf(dst, sizeof(dst), "%.19s...", Insp_sel->fstr); }
   char buf[LRGBUFSIZ];
   int key, curlin = 0, curcol = 0;

signify_that:
   putp(Cap_clr_scr);
   adj_geometry();

   for (;;) {
      char pid[6], cmd[64];

      if (curcol < 0) curcol = 0;
      if (curlin >= Insp_nl) curlin = Insp_nl -1;
      if (curlin < 0) curlin = 0;

      makFS(buf)
      makHD(pid,cmd)
      putp(Cap_home);
      show_special(1, fmtmk(N_unq(YINSP_hdview_fmt)
         , pid, cmd, (Insp_sel->fstr[0]) ? buf : " N/A "));   // nls_maybe
      insp_show_pgs(curcol, curlin, maxLN);
      fflush(stdout);
      /* fflush(stdin) didn't do the trick, so we'll just dip a little deeper
         lest repeated <Enter> keys produce immediate re-selection in caller */
      tcflush(STDIN_FILENO, TCIFLUSH);

      if (Frames_signal) goto signify_that;
      key = iokey(1);
      if (key < 1) goto signify_that;

      switch (key) {
         case kbd_ENTER:          // must force new iokey()
            key = INT_MAX;        // fall through !
         case kbd_ESC:
         case 'q':
            putp(Cap_clr_scr);
            return key;
         case kbd_LEFT:
            curcol -= hzAMT;
            break;
         case kbd_RIGHT:
            curcol += hzAMT;
            break;
         case kbd_UP:
            --curlin;
            break;
         case kbd_DOWN:
            ++curlin;
            break;
         case kbd_PGUP:
         case 'b':
            curlin -= maxLN -1;   // keep 1 line for reference
            break;
         case kbd_PGDN:
         case kbd_SPACE:
            curlin += maxLN -1;   // ditto
            break;
         case kbd_HOME:
         case 'g':
            curcol = curlin = 0;
            break;
         case kbd_END:
         case 'G':
            curcol = 0;
            curlin = Insp_nl - maxLN;
            break;
         case 'L':
         case '&':
         case '/':
         case 'n':
            if (!Insp_utf8)
               insp_find_str(key, &curcol, &curlin);
            else {
               int tmpcol = utf8_proper_col(Insp_p[curlin], curcol, 1);
               insp_find_str(key, &tmpcol, &curlin);
               curcol = utf8_proper_col(Insp_p[curlin], tmpcol, 0);
            }
            // must re-hide cursor in case a prompt for a string makes it huge
            putp((Cursor_state = Cap_curs_hide));
            break;
         case '=':
            snprintf(buf, sizeof(buf), "%s: %s", Insp_sel->type, Insp_sel->fmts);
            INSP_MKSL(1, buf);    // show an extended SL
            if (iokey(1) < 1)
               goto signify_that;
            break;
         default:                 // keep gcc happy
            break;
      }
   }
 #undef hzAMT
 #undef maxLN
 #undef makHD
 #undef makFS
} // end: insp_view_choice


        /*
         * This is the main Inspect routine, responsible for:
         *   1) validating the passed pid (required, but not always used)
         *   2) presenting/establishing the target selection
         *   3) arranging to fill Insp_buf (via the Inspect.tab[?].func)
         *   4) invoking insp_view_choice for viewing/scrolling/searching
         *   5) cleaning up the dynamically acquired memory afterwards */
static void inspection_utility (int pid) {
 #define mkSEL(dst) { for (i = 0; i < Inspect.total; i++) Inspect.tab[i].caps = "~1"; \
      Inspect.tab[sel].caps = "~4"; dst[0] = '\0'; \
      for (i = 0; i < Inspect.total; i++) { char _s[SMLBUFSIZ]; \
         snprintf(_s, sizeof(_s), " %s %s", Inspect.tab[i].name, Inspect.tab[i].caps); \
         strncat(dst, _s, (sizeof(dst) - 1) - strlen(dst)); } }
   char sels[SCREENMAX];
   static int sel;
   int i, key;
   proc_t *p;

   for (i = 0, p = NULL; i < Frame_maxtask; i++)
      if (pid == Curwin->ppt[i]->tid) {
         p = Curwin->ppt[i];
         break;
      }
   if (!p) {
      show_msg(fmtmk(N_fmt(YINSP_pidbad_fmt), pid));
      return;
   }
   // must re-hide cursor since the prompt for a pid made it huge
   putp((Cursor_state = Cap_curs_hide));
signify_that:
   putp(Cap_clr_scr);
   adj_geometry();

   key = INT_MAX;
   do {
      mkSEL(sels);
      putp(Cap_home);
      show_special(1, fmtmk(N_unq(YINSP_hdsels_fmt)
         , pid, p->cmd, sels));
      INSP_MKSL(0, " ");

      if (Frames_signal) goto signify_that;
      if (key == INT_MAX) key = iokey(1);
      if (key < 1) goto signify_that;

      switch (key) {
         case 'q':
         case kbd_ESC:
            break;
         case kbd_END:
            sel = 0;              // fall through !
         case kbd_LEFT:
            if (--sel < 0) sel = Inspect.total -1;
            key = INT_MAX;
            break;
         case kbd_HOME:
            sel = Inspect.total;  // fall through !
         case kbd_RIGHT:
            if (++sel >= Inspect.total) sel = 0;
            key = INT_MAX;
            break;
         case kbd_ENTER:
            INSP_BUSY(!strcmp("file", Inspect.tab[sel].type)
               ? YINSP_waitin_txt : YINSP_workin_txt);
            Insp_sel = &Inspect.tab[sel];
            Inspect.tab[sel].func(Inspect.tab[sel].fmts, pid);
            Insp_utf8 = utf8_delta(Insp_buf);
            key = insp_view_choice(p);
            free(Insp_buf);
            free(Insp_p);
            break;
         default:
            goto signify_that;
      }
   } while (key != 'q' && key != kbd_ESC);

 #undef mkSEL
} // end: inspection_utility
#undef INSP_MKSL
#undef INSP_RLEN
#undef INSP_BUSY

/*######  Other Filtering  ###############################################*/

        /*
         * This sructure is hung from a WIN_t when other filtering is active */
struct osel_s {
   struct osel_s *nxt;                         // the next criteria or NULL.
   int (*rel)(const char *, const char *);     // relational strings compare
   char *(*sel)(const char *, const char *);   // for selection str compares
   char *raw;                                  // raw user input (dup check)
   char *val;                                  // value included or excluded
   int   ops;                                  // filter delimiter/operation
   int   inc;                                  // include == 1, exclude == 0
   int   enu;                                  // field (procflag) to filter
   int   typ;                                  // typ used to set: rel & sel
};

        /*
         * A function to parse, validate and build a single 'other filter' */
static const char *osel_add (WIN_t *q, int ch, char *glob, int push) {
   int (*rel)(const char *, const char *);
   char *(*sel)(const char *, const char *);
   char raw[MEDBUFSIZ], ops, *pval;
   struct osel_s *osel;
   int inc, enu;

   if (ch == 'o') {
      rel   = strcasecmp;
      sel   = strcasestr;
   } else {
      rel   = strcmp;
      sel   = strstr;
   }

   if (!snprintf(raw, sizeof(raw), "%s", glob))
      return NULL;
   for (osel = q->osel_1st; osel; ) {
      if (!strcmp(osel->raw, raw))             // #1: is criteria duplicate?
         return N_txt(OSEL_errdups_txt);
      osel = osel->nxt;
   }
   if (*glob != '!') inc = 1;                  // #2: is it include/exclude?
   else { ++glob; inc = 0; }

   if (!(pval = strpbrk(glob, "<=>")))         // #3: do we see a delimiter?
      return fmtmk(N_fmt(OSEL_errdelm_fmt)
         , inc ? N_txt(WORD_include_txt) : N_txt(WORD_exclude_txt));
   ops = *(pval);
   *(pval++) = '\0';

   for (enu = 0; enu < EU_MAXPFLGS; enu++)     // #4: is this a valid field?
      if (!STRCMP(N_col(enu), glob)) break;
   if (enu == EU_MAXPFLGS)
      return fmtmk(N_fmt(XTRA_badflds_fmt), glob);

   if (!(*pval))                               // #5: did we get some value?
      return fmtmk(N_fmt(OSEL_errvalu_fmt)
         , inc ? N_txt(WORD_include_txt) : N_txt(WORD_exclude_txt));

   osel = alloc_c(sizeof(struct osel_s));
   osel->typ = ch;
   osel->inc = inc;
   osel->enu = enu;
   osel->ops = ops;
   if (ops == '=') osel->val = alloc_s(pval);
   else osel->val = alloc_s(justify_pad(pval, Fieldstab[enu].width, Fieldstab[enu].align));
   osel->rel = rel;
   osel->sel = sel;
   osel->raw = alloc_s(raw);

   if (push) {
      // a LIFO queue was used when we're interactive
      osel->nxt = q->osel_1st;
      q->osel_1st = osel;
   } else {
      // a FIFO queue must be employed for the rcfile
      if (!q->osel_1st)
         q->osel_1st = osel;
      else {
         struct osel_s *prev, *walk = q->osel_1st;
         do {
            prev = walk;
            walk = walk->nxt;
         } while (walk);
         prev->nxt = osel;
      }
   }
   q->osel_tot += 1;

   return NULL;
} // end: osel_add


        /*
         * A function to turn off entire other filtering in the given window */
static void osel_clear (WIN_t *q) {
   struct osel_s *osel = q->osel_1st;

   while (osel) {
      struct osel_s *nxt = osel->nxt;
      free(osel->val);
      free(osel->raw);
      free(osel);
      osel = nxt;
   }
   q->osel_tot = 0;
   q->osel_1st = NULL;
#ifndef USE_X_COLHDR
   OFFw(q, NOHISEL_xxx);
#endif
} // end: osel_clear


        /*
         * Determine if there are matching values or relationships among the
         * other criteria in this passed window -- it's called from only one
         * place, and likely inlined even without the directive */
static inline int osel_matched (const WIN_t *q, FLG_t enu, const char *str) {
   struct osel_s *osel = q->osel_1st;

   while (osel) {
      if (osel->enu == enu) {
         int r;
         switch (osel->ops) {
            case '<':                          // '<' needs the r < 0 unless
               r = osel->rel(str, osel->val);  // '!' which needs an inverse
               if ((r >= 0 && osel->inc) || (r < 0 && !osel->inc)) return 0;
               break;
            case '>':                          // '>' needs the r > 0 unless
               r = osel->rel(str, osel->val);  // '!' which needs an inverse
               if ((r <= 0 && osel->inc) || (r > 0 && !osel->inc)) return 0;
               break;
            default:
            {  char *p = osel->sel(str, osel->val);
               if ((!p && osel->inc) || (p && !osel->inc)) return 0;
            }
               break;
         }
      }
      osel = osel->nxt;
   }
   return 1;
} // end: osel_matched

/*######  Startup routines  ##############################################*/

        /*
         * No matter what *they* say, we handle the really really BIG and
         * IMPORTANT stuff upon which all those lessor functions depend! */
static void before (char *me) {
   struct sigaction sa;
   proc_t p;
   int i;
#ifndef PRETEND2_5_X
   int linux_version_code = procps_linux_version();
#else
   int linux_version_code = LINUX_VERSION(2,5,43);
#endif

   atexit(close_stdout);

   // is /proc mounted?
   look_up_our_self(&p);

   // setup our program name
   Myname = strrchr(me, '/');
   if (Myname) ++Myname; else Myname = me;

   // accommodate nls/gettext potential translations
   initialize_nls();

   // override default library memory alloc error handler
   xalloc_err_handler = xalloc_our_handler;

   // establish cpu particulars
   cpuinfo();
#ifdef PRETEND8CPUS
   smp_num_cpus = 8;
#endif
   Cpu_States_fmts = N_unq(STATE_lin2x4_fmt);
   if (linux_version_code > LINUX_VERSION(2, 5, 41))
      Cpu_States_fmts = N_unq(STATE_lin2x5_fmt);
   if (linux_version_code >= LINUX_VERSION(2, 6, 0))
      Cpu_States_fmts = N_unq(STATE_lin2x6_fmt);
   if (linux_version_code >= LINUX_VERSION(2, 6, 11))
      Cpu_States_fmts = N_unq(STATE_lin2x7_fmt);

   // get virtual page stuff
   i = page_bytes; // from sysinfo.c, at lib init
   while(i > 1024) { i >>= 1; Pg2K_shft++; }

#ifndef OFF_HST_HASH
   // prep for HST_t's put/get hashing optimizations
   for (i = 0; i < HHASH_SIZ; i++) HHash_nul[i] = -1;
   memcpy(HHash_one, HHash_nul, sizeof(HHash_nul));
   memcpy(HHash_two, HHash_nul, sizeof(HHash_nul));
#endif

   numa_init();
   Numa_node_tot = numa_max_node() + 1;

#ifndef SIGRTMAX       // not available on hurd, maybe others too
#define SIGRTMAX 32
#endif
   // lastly, establish a robust signals environment
   memset(&sa, 0, sizeof(sa));
   sigemptyset(&sa.sa_mask);
   // with user position preserved through SIGWINCH, we must avoid SA_RESTART
   sa.sa_flags = 0;
   for (i = SIGRTMAX; i; i--) {
      switch (i) {
         case SIGALRM: case SIGHUP:  case SIGINT:
         case SIGPIPE: case SIGQUIT: case SIGTERM:
         case SIGUSR1: case SIGUSR2:
            sa.sa_handler = sig_endpgm;
            break;
         case SIGTSTP: case SIGTTIN: case SIGTTOU:
            sa.sa_handler = sig_paused;
            break;
         case SIGCONT: case SIGWINCH:
            sa.sa_handler = sig_resize;
            break;
         default:
            sa.sa_handler = sig_abexit;
            break;
         case SIGKILL: case SIGSTOP:
         // because uncatchable, fall through
         case SIGCHLD: // we can't catch this
            continue;  // when opening a pipe
      }
      sigaction(i, &sa, NULL);
   }
} // end: before


        /*
         * A configs_file *Helper* function responsible for converting
         * a single window's old rc stuff into a new style rcfile entry */
static int config_cvt (WIN_t *q) {
   static struct {
      int old, new;
   } flags_tab[] = {
    #define old_View_NOBOLD  0x000001
    #define old_VISIBLE_tsk  0x000008
    #define old_Qsrt_NORMAL  0x000010
    #define old_Show_HICOLS  0x000200
    #define old_Show_THREAD  0x010000
      { old_View_NOBOLD, View_NOBOLD },
      { old_VISIBLE_tsk, Show_TASKON },
      { old_Qsrt_NORMAL, Qsrt_NORMAL },
      { old_Show_HICOLS, Show_HICOLS },
      { old_Show_THREAD, 0           }
    #undef old_View_NOBOLD
    #undef old_VISIBLE_tsk
    #undef old_Qsrt_NORMAL
    #undef old_Show_HICOLS
    #undef old_Show_THREAD
   };
   static const char fields_src[] = CVT_FIELDS;
   char fields_dst[PFLAGSSIZ], *p1, *p2;
   int i, j, x;

   // first we'll touch up this window's winflags...
   x = q->rc.winflags;
   q->rc.winflags = 0;
   for (i = 0; i < MAXTBL(flags_tab); i++) {
      if (x & flags_tab[i].old) {
         x &= ~flags_tab[i].old;
         q->rc.winflags |= flags_tab[i].new;
      }
   }
   q->rc.winflags |= x;

   // now let's convert old top's more limited fields...
   j = strlen(q->rc.fieldscur);
   if (j > CVT_FLDMAX)
      return 1;
   strcpy(fields_dst, fields_src);
   /* all other fields represent the 'on' state with a capitalized version
      of a particular qwerty key.  for the 2 additional suse out-of-memory
      fields it makes perfect sense to do the exact opposite, doesn't it?
      in any case, we must turn them 'off' temporarily... */
   if ((p1 = strchr(q->rc.fieldscur, '[')))  *p1 = '{';
   if ((p2 = strchr(q->rc.fieldscur, '\\'))) *p2 = '|';
   for (i = 0; i < j; i++) {
      int c = q->rc.fieldscur[i];
      x = tolower(c) - 'a';
      if (x < 0 || x >= CVT_FLDMAX)
         return 1;
      fields_dst[i] = fields_src[x];
      if (isupper(c))
         FLDon(fields_dst[i]);
   }
   // if we turned any suse only fields off, turn 'em back on OUR way...
   if (p1) FLDon(fields_dst[p1 - q->rc.fieldscur]);
   if (p2) FLDon(fields_dst[p2 - q->rc.fieldscur]);
   strcpy(q->rc.fieldscur, fields_dst);

   // lastly, we must adjust the old sort field enum...
   x = q->rc.sortindx;
   q->rc.sortindx = fields_src[x] - FLD_OFFSET;
   if (q->rc.sortindx < 0 || q->rc.sortindx >= EU_MAXPFLGS)
      return 1;

   return 0;
} // end: config_cvt


        /*
         * A configs_file *Helper* function responsible for reading
         * and validating a configuration file's 'Inspection' entries */
static int config_insp (FILE *fp, char *buf, size_t size) {
   int i;

   // we'll start off with a 'potential' blank or empty line
   // ( only realized if we end up with Inspect.total > 0 )
   if (!buf[0] || buf[0] != '\n') Inspect.raw = alloc_s("\n");
   else Inspect.raw = alloc_c(1);

   for (i = 0;;) {
    #define iT(element) Inspect.tab[i].element
    #define nxtLINE { buf[0] = '\0'; continue; }
      size_t lraw = strlen(Inspect.raw) +1;
      int n, x;
      char *s1, *s2, *s3;

      if (i < 0 || (size_t)i >= INT_MAX / sizeof(struct I_ent)) break;
      if (lraw >= INT_MAX - size) break;

      if (!buf[0] && !fgets(buf, size, fp)) break;
      lraw += strlen(buf) +1;
      Inspect.raw = alloc_r(Inspect.raw, lraw);
      strcat(Inspect.raw, buf);

      if (buf[0] == '#' || buf[0] == '\n') nxtLINE;
      Inspect.tab = alloc_r(Inspect.tab, sizeof(struct I_ent) * (i + 1));

      // part of this is used in a show_special() call, so let's sanitize it
      for (n = 0, x = strlen(buf); n < x; n++) {
         if ((buf[n] != '\t' && buf[n] != '\n')
          && (buf[n] < ' ')) {
            buf[n] = '.';
            Rc_questions = 1;
         }
      }
      if (!(s1 = strtok(buf, "\t\n")))  { Rc_questions = 1; nxtLINE; }
      if (!(s2 = strtok(NULL, "\t\n"))) { Rc_questions = 1; nxtLINE; }
      if (!(s3 = strtok(NULL, "\t\n"))) { Rc_questions = 1; nxtLINE; }

      switch (toupper(buf[0])) {
         case 'F':
            iT(func) = insp_do_file;
            break;
         case 'P':
            iT(func) = insp_do_pipe;
            break;
         default:
            Rc_questions = 1;
            nxtLINE;
      }
      iT(type) = alloc_s(s1);
      iT(name) = alloc_s(s2);
      iT(fmts) = alloc_s(s3);
      iT(farg) = (strstr(iT(fmts), "%d")) ? 1 : 0;
      iT(fstr) = alloc_c(FNDBUFSIZ);
      iT(flen) = 0;

      buf[0] = '\0';
      ++i;
    #undef iT
    #undef nxtLINE
   } // end: for ('inspect' entries)

   Inspect.total = i;
#ifndef INSP_OFFDEMO
   if (!Inspect.total) {
    #define mkS(n) N_txt(YINSP_demo ## n ## _txt)
      const char *sels[] = { mkS(01), mkS(02), mkS(03) };
      Inspect.total = Inspect.demo = MAXTBL(sels);
      Inspect.tab = alloc_c(sizeof(struct I_ent) * Inspect.total);
      for (i = 0; i < Inspect.total; i++) {
         Inspect.tab[i].type = alloc_s(N_txt(YINSP_deqtyp_txt));
         Inspect.tab[i].name = alloc_s(sels[i]);
         Inspect.tab[i].func = insp_do_demo;
         Inspect.tab[i].fmts = alloc_s(N_txt(YINSP_deqfmt_txt));
         Inspect.tab[i].fstr = alloc_c(FNDBUFSIZ);
      }
    #undef mkS
   }
#endif
   return 0;
} // end: config_insp


        /*
         * A configs_file *Helper* function responsible for reading
         * and validating a configuration file's 'Other Filter' entries */
static int config_osel (FILE *fp, char *buf, size_t size) {
   int i, ch, tot, wno, begun;
   char *p;

   for (begun = 0;;) {
      if (!fgets(buf, size, fp)) return 0;
      if (buf[0] == '\n') continue;
      // whoa, must be an 'inspect' entry
      if (!begun && !strstr(buf, Osel_delim_1_txt))
         return 0;
      // ok, we're now beginning
      if (!begun && strstr(buf, Osel_delim_1_txt)) {
         begun = 1;
         continue;
      }
      // this marks the end of our stuff
      if (begun && strstr(buf, Osel_delim_2_txt))
         break;

      if (2 != sscanf(buf, Osel_window_fmts, &wno, &tot))
         goto end_oops;

      for (i = 0; i < tot; i++) {
         if (!fgets(buf, size, fp)) return 1;
         if (1 > sscanf(buf, Osel_filterI_fmt, &ch)) goto end_oops;
         if ((p = strchr(buf, '\n'))) *p = '\0';
         if (!(p = strstr(buf, OSEL_FILTER))) goto end_oops;
         p += sizeof(OSEL_FILTER) - 1;
         if (osel_add(&Winstk[wno], ch, p, 0)) goto end_oops;
      }
   }
   // let's prime that buf for the next guy...
   fgets(buf, size, fp);
   return 0;

end_oops:
   Rc_questions = 1;
   return 1;
} // end: config_osel


        /*
         * A configs_reads *Helper* function responsible for processing
         * a configuration file (personal or system-wide default) */
static const char *configs_file (FILE *fp, const char *name, float *delay) {
   char fbuf[LRGBUFSIZ];
   int i, tmp_whole, tmp_fract;
   const char *p = NULL;

   p = fmtmk(N_fmt(RC_bad_files_fmt), name);
   (void)fgets(fbuf, sizeof(fbuf), fp);     // ignore eyecatcher
   if (6 != fscanf(fp
      , "Id:%c, Mode_altscr=%d, Mode_irixps=%d, Delay_time=%d.%d, Curwin=%d\n"
      , &Rc.id, &Rc.mode_altscr, &Rc.mode_irixps, &tmp_whole, &tmp_fract, &i)) {
         return p;
   }
   if (Rc.id < 'a' || Rc.id > RCF_VERSION_ID)
      return p;
   if (Rc.mode_altscr < 0 || Rc.mode_altscr > 1)
      return p;
   if (Rc.mode_irixps < 0 || Rc.mode_irixps > 1)
      return p;
   if (tmp_whole < 0)
      return p;
   // you saw that, right?  (fscanf stickin' it to 'i')
   if (i < 0 || i >= GROUPSMAX)
      return p;
   Curwin = &Winstk[i];
   // this may be ugly, but it keeps us locale independent...
   *delay = (float)tmp_whole + (float)tmp_fract / 1000;

   for (i = 0 ; i < GROUPSMAX; i++) {
      int n, x;
      WIN_t *w = &Winstk[i];
      p = fmtmk(N_fmt(RC_bad_entry_fmt), i+1, name);

      // note: "fieldscur=%__s" on next line should equal (PFLAGSSIZ -1) !
      if (2 != fscanf(fp, "%3s\tfieldscur=%99s\n"
         , w->rc.winname, w->rc.fieldscur))
            return p;
#if PFLAGSSIZ != 100
 // too bad fscanf is not as flexible with his format string as snprintf
 #error Hey, fix the above fscanf 'PFLAGSSIZ' dependency !
#endif
      // ensure there's been no manual alteration of fieldscur
      for (n = 0 ; n < EU_MAXPFLGS; n++) {
         if (&w->rc.fieldscur[n] != strrchr(w->rc.fieldscur, w->rc.fieldscur[n]))
            return p;
      }
      // be tolerant of missing release 3.3.10 graph modes additions
      if (3 > fscanf(fp, "\twinflags=%d, sortindx=%d, maxtasks=%d, graph_cpus=%d, graph_mems=%d\n"
         , &w->rc.winflags, &w->rc.sortindx, &w->rc.maxtasks, &w->rc.graph_cpus, &w->rc.graph_mems))
            return p;
      if (w->rc.sortindx < 0 || w->rc.sortindx >= EU_MAXPFLGS)
         return p;
      if (w->rc.maxtasks < 0)
         return p;
      if (w->rc.graph_cpus < 0 || w->rc.graph_cpus > 2)
         return p;
      if (w->rc.graph_mems < 0 || w->rc.graph_mems > 2)
         return p;

      if (4 != fscanf(fp, "\tsummclr=%d, msgsclr=%d, headclr=%d, taskclr=%d\n"
         , &w->rc.summclr, &w->rc.msgsclr, &w->rc.headclr, &w->rc.taskclr))
            return p;
      // would prefer to use 'max_colors', but it isn't available yet...
      if (w->rc.summclr < 0 || w->rc.summclr > 255) return p;
      if (w->rc.msgsclr < 0 || w->rc.msgsclr > 255) return p;
      if (w->rc.headclr < 0 || w->rc.headclr > 255) return p;
      if (w->rc.taskclr < 0 || w->rc.taskclr > 255) return p;

      switch (Rc.id) {
         case 'a':                          // 3.2.8 (former procps)
            if (config_cvt(w))
               return p;
         case 'f':                          // 3.3.0 thru 3.3.3 (ng)
            SETw(w, Show_JRNUMS);
         case 'g':                          // from 3.3.4 thru 3.3.8
            scat(w->rc.fieldscur, RCF_PLUS_H);
         case 'h':                          // this is release 3.3.9
            w->rc.graph_cpus = w->rc.graph_mems = 0;
            // these next 2 are really global, but best documented here
            Rc.summ_mscale = Rc.task_mscale = SK_Kb;
         case 'i':                          // actual RCF_VERSION_ID
            scat(w->rc.fieldscur, RCF_PLUS_J);
         case 'j':                          // and the next version
         default:
            if (strlen(w->rc.fieldscur) != sizeof(DEF_FIELDS) - 1)
               return p;
            for (x = 0; x < EU_MAXPFLGS; ++x)
               if (EU_MAXPFLGS <= FLDget(w, x))
                  return p;
            break;
      }
#ifndef USE_X_COLHDR
      OFFw(w, NOHIFND_xxx | NOHISEL_xxx);
#endif
   } // end: for (GROUPSMAX)

   // any new addition(s) last, for older rcfiles compatibility...
   (void)fscanf(fp, "Fixed_widest=%d, Summ_mscale=%d, Task_mscale=%d, Zero_suppress=%d\n"
      , &Rc.fixed_widest, &Rc.summ_mscale, &Rc.task_mscale, &Rc.zero_suppress);
   if (Rc.fixed_widest < -1 || Rc.fixed_widest > SCREENMAX)
      Rc.fixed_widest = 0;
   if (Rc.summ_mscale < 0   || Rc.summ_mscale > SK_Eb)
      Rc.summ_mscale = 0;
   if (Rc.task_mscale < 0   || Rc.task_mscale > SK_Pb)
      Rc.task_mscale = 0;
   if (Rc.zero_suppress < 0 || Rc.zero_suppress > 1)
      Rc.zero_suppress = 0;

   // lastly, let's process any optional glob(s) ...
   // (darn, must do osel 1st even though alphabetically 2nd)
   fbuf[0] = '\0';
   config_osel(fp, fbuf, sizeof(fbuf));
   config_insp(fp, fbuf, sizeof(fbuf));

   return NULL;
} // end: configs_file


        /*
         * A configs_reads *Helper* function responsible for ensuring the
         * complete path was established, otherwise force the 'W' to fail */
static int configs_path (const char *const fmts, ...) __attribute__((format(printf,1,2)));
static int configs_path (const char *const fmts, ...) {
   int len;
   va_list ap;

   va_start(ap, fmts);
   len = vsnprintf(Rc_name, sizeof(Rc_name), fmts, ap);
   va_end(ap);
   if (len <= 0 || (size_t)len >= sizeof(Rc_name)) {
      Rc_name[0] = '\0';
      len = 0;
   }
   return len;
} // end: configs_path


        /*
         * Try reading up to 3 rcfiles
         * 1. 'SYS_RCRESTRICT' contains two lines consisting of the secure
         *     mode switch and an update interval.  Its presence limits what
         *     ordinary users are allowed to do.
         * 2. 'Rc_name' contains multiple lines - 3 global + 3 per window.
         *     line 1  : an eyecatcher and creating program/alias name
         *     line 2  : an id, Mode_altcsr, Mode_irixps, Delay_time, Curwin.
         *     For each of the 4 windows:
         *       line a: contains w->winname, fieldscur
         *       line b: contains w->winflags, sortindx, maxtasks, graph modes
         *       line c: contains w->summclr, msgsclr, headclr, taskclr
         *     line 15 : miscellaneous additional global settings
         *     Any remaining lines are devoted to the optional entries
         *     supporting the 'Other Filter' and 'Inspect' provisions.
         * 3. 'SYS_RCDEFAULTS' system-wide defaults if 'Rc_name' absent
         *     format is identical to #2 above */
static void configs_reads (void) {
   float tmp_delay = DEF_DELAY;
   const char *p, *p_home;
   FILE *fp;

   fp = fopen(SYS_RCRESTRICT, "r");
   if (fp) {
      char fbuf[SMLBUFSIZ];
      if (fgets(fbuf, sizeof(fbuf), fp)) {     // sys rc file, line 1
         Secure_mode = 1;
         if (fgets(fbuf, sizeof(fbuf), fp))    // sys rc file, line 2
            sscanf(fbuf, "%f", &Rc.delay_time);
      }
      fclose(fp);
   }

   Rc_name[0] = '\0'; // "fopen() shall fail if pathname is an empty string."
   // attempt to use the legacy file first, if we cannot access that file, use
   // the new XDG basedir locations (XDG_CONFIG_HOME or HOME/.config) instead.
   p_home = getenv("HOME");
   if (!p_home || p_home[0] != '/') {
      const struct passwd *const pwd = getpwuid(getuid());
      if (!pwd || !(p_home = pwd->pw_dir) || p_home[0] != '/') {
         p_home = NULL;
      }
   }
   if (p_home) {
      configs_path("%s/.%src", p_home, Myname);
   }

   if (!(fp = fopen(Rc_name, "r"))) {
      p = getenv("XDG_CONFIG_HOME");
      // ensure the path we get is absolute, fallback otherwise.
      if (!p || p[0] != '/') {
         if (!p_home) goto system_default;
         p = fmtmk("%s/.config", p_home);
         (void)mkdir(p, 0700);
      }
      if (!configs_path("%s/procps", p)) goto system_default;
      (void)mkdir(Rc_name, 0700);
      if (!configs_path("%s/procps/%src", p, Myname)) goto system_default;
      fp = fopen(Rc_name, "r");
   }

   if (fp) {
      p = configs_file(fp, Rc_name, &tmp_delay);
      fclose(fp);
      if (p) goto default_or_error;
   } else {
system_default:
      fp = fopen(SYS_RCDEFAULTS, "r");
      if (fp) {
         p = configs_file(fp, SYS_RCDEFAULTS, &tmp_delay);
         fclose(fp);
         if (p) goto default_or_error;
      }
   }

   // lastly, establish the true runtime secure mode and delay time
   if (!getuid()) Secure_mode = 0;
   if (!Secure_mode) Rc.delay_time = tmp_delay;
   return;

default_or_error:
#ifdef RCFILE_NOERR
{  RCF_t rcdef = DEF_RCFILE;
   int i;
   Rc = rcdef;
   for (i = 0 ; i < GROUPSMAX; i++)
      Winstk[i].rc  = Rc.win[i];
}
#else
   error_exit(p);
#endif
} // end: configs_reads


        /*
         * Parse command line arguments.
         * Note: it's assumed that the rc file(s) have already been read
         *       and our job is to see if any of those options are to be
         *       overridden -- we'll force some on and negate others in our
         *       best effort to honor the loser's (oops, user's) wishes... */
static void parse_args (char **args) {
   /* differences between us and the former top:
      -C (separate CPU states for SMP) is left to an rcfile
      -u (user monitoring) added to compliment interactive 'u'
      -p (pid monitoring) allows a comma delimited list
      -q (zero delay) eliminated as redundant, incomplete and inappropriate
            use: "nice -n-10 top -d0" to achieve what was only claimed
      .  most switches act as toggles (not 'on' sw) for more user flexibility
      .  no deprecated/illegal use of 'breakargv:' with goto
      .  bunched args are actually handled properly and none are ignored
      .  we tolerate NO whitespace and NO switches -- maybe too tolerant? */
   static const char numbs_str[] = "+,-.0123456789";
   float tmp_delay = FLT_MAX;
   int i;

   while (*args) {
      const char *cp = *(args++);

      while (*cp) {
         char ch;
         float tmp;

         switch ((ch = *cp)) {
            case '\0':
               break;
            case '-':
               if (cp[1]) ++cp;
               else if (*args) cp = *args++;
               if (strspn(cp, "+,-."))
                  error_exit(fmtmk(N_fmt(WRONG_switch_fmt)
                     , cp, Myname, N_txt(USAGE_abbrev_txt)));
               continue;
            case '1':   // ensure behavior identical to run-time toggle
               if (CHKw(Curwin, View_CPUNOD)) OFFw(Curwin, View_CPUSUM);
               else TOGw(Curwin, View_CPUSUM);
               OFFw(Curwin, View_CPUNOD);
               SETw(Curwin, View_STATES);
               goto bump_cp;
            case 'b':
               Batch = 1;
               goto bump_cp;
            case 'c':
               TOGw(Curwin, Show_CMDLIN);
               goto bump_cp;
            case 'd':
               if (cp[1]) ++cp;
               else if (*args) cp = *args++;
               else error_exit(fmtmk(N_fmt(MISSING_args_fmt), ch));
               if (!mkfloat(cp, &tmp_delay, 0))
                  error_exit(fmtmk(N_fmt(BAD_delayint_fmt), cp));
               if (0 > tmp_delay)
                  error_exit(N_txt(DELAY_badarg_txt));
               break;
            case 'E':
            {  const char *get = "kmgtpe", *got;
               if (cp[1]) cp++;
               else if (*args) cp = *args++;
               else error_exit(fmtmk(N_fmt(MISSING_args_fmt), ch));
               if (!(got = strchr(get, tolower(*cp))))
                  error_exit(fmtmk(N_fmt(BAD_memscale_fmt), *cp));
               Rc.summ_mscale = (int)(got - get);
            }  goto bump_cp;
            case 'H':
               Thread_mode = 1;
               goto bump_cp;
            case 'h':
            case 'v':
               puts(fmtmk(N_fmt(HELP_cmdline_fmt)
                  , PACKAGE_STRING, Myname, N_txt(USAGE_abbrev_txt)));
               bye_bye(NULL);
            case 'i':
               TOGw(Curwin, Show_IDLEPS);
               Curwin->rc.maxtasks = 0;
               goto bump_cp;
            case 'n':
               if (cp[1]) cp++;
               else if (*args) cp = *args++;
               else error_exit(fmtmk(N_fmt(MISSING_args_fmt), ch));
               if (!mkfloat(cp, &tmp, 1) || 1.0 > tmp)
                  error_exit(fmtmk(N_fmt(BAD_niterate_fmt), cp));
               Loops = (int)tmp;
               break;
            case 'o':
               if (cp[1]) cp++;
               else if (*args) cp = *args++;
               else error_exit(fmtmk(N_fmt(MISSING_args_fmt), ch));
               if (*cp == '+') { SETw(Curwin, Qsrt_NORMAL); ++cp; }
               else if (*cp == '-') { OFFw(Curwin, Qsrt_NORMAL); ++cp; }
               for (i = 0; i < EU_MAXPFLGS; i++)
                  if (!STRCMP(cp, N_col(i))) break;
               if (i == EU_MAXPFLGS)
                  error_exit(fmtmk(N_fmt(XTRA_badflds_fmt), cp));
               OFFw(Curwin, Show_FOREST);
               Curwin->rc.sortindx = i;
               cp += strlen(cp);
               break;
            case 'O':
               for (i = 0; i < EU_MAXPFLGS; i++)
                  puts(N_col(i));
               bye_bye(NULL);
            case 'p':
            {  int pid; char *p;
               if (Curwin->usrseltyp) error_exit(N_txt(SELECT_clash_txt));
               do {
                  if (cp[1]) cp++;
                  else if (*args) cp = *args++;
                  else error_exit(fmtmk(N_fmt(MISSING_args_fmt), ch));
                  if (Monpidsidx >= MONPIDMAX)
                     error_exit(fmtmk(N_fmt(LIMIT_exceed_fmt), MONPIDMAX));
                  if (1 != sscanf(cp, "%d", &pid)
                  || strpbrk(cp, "+-."))
                     error_exit(fmtmk(N_fmt(BAD_mon_pids_fmt), cp));
                  if (!pid) pid = getpid();
                  for (i = 0; i < Monpidsidx; i++)
                     if (Monpids[i] == pid) goto next_pid;
                  Monpids[Monpidsidx++] = pid;
               next_pid:
                  if (!(p = strchr(cp, ','))) break;
                  cp = p;
               } while (*cp);
            }  break;
            case 's':
               Secure_mode = 1;
               goto bump_cp;
            case 'S':
               TOGw(Curwin, Show_CTIMES);
               goto bump_cp;
            case 'u':
            case 'U':
            {  const char *errmsg;
               if (Monpidsidx || Curwin->usrseltyp) error_exit(N_txt(SELECT_clash_txt));
               if (cp[1]) cp++;
               else if (*args) cp = *args++;
               else error_exit(fmtmk(N_fmt(MISSING_args_fmt), ch));
               if ((errmsg = user_certify(Curwin, cp, ch))) error_exit(errmsg);
               cp += strlen(cp);
            }  break;
            case 'w':
            {  const char *pn = NULL;
               int ai = 0, ci = 0;
               tmp = -1;
               if (cp[1]) pn = &cp[1];
               else if (*args) { pn = *args; ai = 1; }
               if (pn && !(ci = strspn(pn, numbs_str))) { ai = 0; pn = NULL; }
               if (pn && (!mkfloat(pn, &tmp, 1) || tmp < W_MIN_COL || tmp > SCREENMAX))
                  error_exit(fmtmk(N_fmt(BAD_widtharg_fmt), pn));
               Width_mode = (int)tmp;
               cp++;
               args += ai;
               if (pn) cp = pn + ci;
            }  continue;
            default :
               error_exit(fmtmk(N_fmt(UNKNOWN_opts_fmt)
                  , *cp, Myname, N_txt(USAGE_abbrev_txt)));
         } // end: switch (*cp)

         // advance cp and jump over any numerical args used above
         if (*cp) cp += strspn(&cp[1], numbs_str);
bump_cp:
         if (*cp) ++cp;
      } // end: while (*cp)
   } // end: while (*args)

   // fixup delay time, maybe...
   if (FLT_MAX > tmp_delay) {
      if (Secure_mode)
         error_exit(N_txt(DELAY_secure_txt));
      Rc.delay_time = tmp_delay;
   }
} // end: parse_args


        /*
         * Set up the terminal attributes */
static void whack_terminal (void) {
   static char dummy[] = "dumb";
   struct termios tmptty;

   // the curses part...
   if (Batch) {
      setupterm(dummy, STDOUT_FILENO, NULL);
      return;
   }
#ifdef PRETENDNOCAP
   setupterm(dummy, STDOUT_FILENO, NULL);
#else
   setupterm(NULL, STDOUT_FILENO, NULL);
#endif
   // our part...
   if (-1 == tcgetattr(STDIN_FILENO, &Tty_original))
      error_exit(N_txt(FAIL_tty_get_txt));
   // ok, haven't really changed anything but we do have our snapshot
   Ttychanged = 1;

   // first, a consistent canonical mode for interactive line input
   tmptty = Tty_original;
   tmptty.c_lflag |= (ECHO | ECHOCTL | ECHOE | ICANON | ISIG);
   tmptty.c_lflag &= ~NOFLSH;
   tmptty.c_oflag &= ~TAB3;
   tmptty.c_iflag |= BRKINT;
   tmptty.c_iflag &= ~IGNBRK;
   if (key_backspace && 1 == strlen(key_backspace))
      tmptty.c_cc[VERASE] = *key_backspace;
#ifdef TERMIOS_ONLY
   if (-1 == tcsetattr(STDIN_FILENO, TCSAFLUSH, &tmptty))
      error_exit(fmtmk(N_fmt(FAIL_tty_set_fmt), strerror(errno)));
   tcgetattr(STDIN_FILENO, &Tty_tweaked);
#endif
   // lastly, a nearly raw mode for unsolicited single keystrokes
   tmptty.c_lflag &= ~(ECHO | ECHOCTL | ECHOE | ICANON);
   tmptty.c_cc[VMIN] = 1;
   tmptty.c_cc[VTIME] = 0;
   if (-1 == tcsetattr(STDIN_FILENO, TCSAFLUSH, &tmptty))
      error_exit(fmtmk(N_fmt(FAIL_tty_set_fmt), strerror(errno)));
   tcgetattr(STDIN_FILENO, &Tty_raw);

#ifndef OFF_STDIOLBF
   // thanks anyway stdio, but we'll manage buffering at the frame level...
   setbuffer(stdout, Stdout_buf, sizeof(Stdout_buf));
#endif
#ifdef OFF_SCROLLBK
   // this has the effect of disabling any troublesome scrollback buffer...
   if (enter_ca_mode) putp(enter_ca_mode);
#endif
   // and don't forget to ask iokey to initialize his tinfo_tab
   iokey(0);
} // end: whack_terminal

/*######  Windows/Field Groups support  #################################*/

        /*
         * Value a window's name and make the associated group name. */
static void win_names (WIN_t *q, const char *name) {
   /* note: sprintf/snprintf results are "undefined" when src==dst,
            according to C99 & POSIX.1-2001 (thanks adc) */
   if (q->rc.winname != name)
      snprintf(q->rc.winname, sizeof(q->rc.winname), "%s", name);
   snprintf(q->grpname, sizeof(q->grpname), "%d:%s", q->winnum, name);
} // end: win_names


        /*
         * This guy just resets (normalizes) a single window
         * and he ensures pid monitoring is no longer active. */
static void win_reset (WIN_t *q) {
         SETw(q, Show_IDLEPS | Show_TASKON);
#ifndef SCROLLVAR_NO
         q->rc.maxtasks = q->usrseltyp = q->begpflg = q->begtask = q->begnext = q->varcolbeg = 0;
#else
         q->rc.maxtasks = q->usrseltyp = q->begpflg = q->begtask = q->begnext = 0;
#endif
         Monpidsidx = 0;
         osel_clear(q);
         q->findstr[0] = '\0';
#ifndef USE_X_COLHDR
         // NOHISEL_xxx is redundant (already turned off by osel_clear)
         OFFw(q, NOHIFND_xxx | NOHISEL_xxx);
#endif
} // end: win_reset


        /*
         * Display a window/field group (ie. make it "current"). */
static WIN_t *win_select (int ch) {
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy

   /* if there's no ch, it means we're supporting the external interface,
      so we must try to get our own darn ch by begging the user... */
   if (!ch) {
      show_pmt(N_txt(CHOOSE_group_txt));
      if (1 > (ch = iokey(1))) return w;
   }
   switch (ch) {
      case 'a':                   // we don't carry 'a' / 'w' in our
         w = w->next;             // pmt - they're here for a good
         break;                   // friend of ours -- wins_colors.
      case 'w':                   // (however those letters work via
         w = w->prev;             // the pmt too but gee, end-loser
         break;                   // should just press the darn key)
      case '1': case '2' : case '3': case '4':
         w = &Winstk[ch - '1'];
         break;
      default:                    // keep gcc happy
         break;
   }
   Curwin = w;
   mkVIZrow1(Curwin);
   return Curwin;
} // end: win_select


        /*
         * Just warn the user when a command can't be honored. */
static int win_warn (int what) {
   switch (what) {
      case Warn_ALT:
         show_msg(N_txt(DISABLED_cmd_txt));
         break;
      case Warn_VIZ:
         show_msg(fmtmk(N_fmt(DISABLED_win_fmt), Curwin->grpname));
         break;
      default:                    // keep gcc happy
         break;
   }
   /* we gotta' return false 'cause we're somewhat well known within
      macro society, by way of that sassy little tertiary operator... */
   return 0;
} // end: win_warn


        /*
         * Change colors *Helper* function to save/restore settings;
         * ensure colors will show; and rebuild the terminfo strings. */
static void wins_clrhlp (WIN_t *q, int save) {
   static int flgssav, summsav, msgssav, headsav, tasksav;

   if (save) {
      flgssav = q->rc.winflags; summsav = q->rc.summclr;
      msgssav = q->rc.msgsclr;  headsav = q->rc.headclr; tasksav = q->rc.taskclr;
      SETw(q, Show_COLORS);
   } else {
      q->rc.winflags = flgssav; q->rc.summclr = summsav;
      q->rc.msgsclr = msgssav;  q->rc.headclr = headsav; q->rc.taskclr = tasksav;
   }
   capsmk(q);
} // end: wins_clrhlp


        /*
         * Change colors used in display */
static void wins_colors (void) {
 #define kbdABORT  'q'
 #define kbdAPPLY  kbd_ENTER
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy
   int clr = w->rc.taskclr, *pclr = &w->rc.taskclr;
   char tgt = 'T';
   int key;

   if (0 >= max_colors) {
      show_msg(N_txt(COLORS_nomap_txt));
      return;
   }
   wins_clrhlp(w, 1);
   putp((Cursor_state = Cap_curs_huge));
signify_that:
   putp(Cap_clr_scr);
   adj_geometry();

   do {
      putp(Cap_home);
      // this string is well above ISO C89's minimum requirements!
      show_special(1, fmtmk(N_unq(COLOR_custom_fmt)
         , w->grpname
         , CHKw(w, View_NOBOLD) ? N_txt(ON_word_only_txt) : N_txt(OFF_one_word_txt)
         , CHKw(w, Show_COLORS) ? N_txt(ON_word_only_txt) : N_txt(OFF_one_word_txt)
         , CHKw(w, Show_HIBOLD) ? N_txt(ON_word_only_txt) : N_txt(OFF_one_word_txt)
         , tgt, max_colors, clr, w->grpname));
      putp(Cap_clr_eos);
      fflush(stdout);

      if (Frames_signal) goto signify_that;
      key = iokey(1);
      if (key < 1) goto signify_that;
      if (key == kbd_ESC) break;

      switch (key) {
         case 'S':
            pclr = &w->rc.summclr;
            clr = *pclr;
            tgt = key;
            break;
         case 'M':
            pclr = &w->rc.msgsclr;
            clr = *pclr;
            tgt = key;
            break;
         case 'H':
            pclr = &w->rc.headclr;
            clr = *pclr;
            tgt = key;
            break;
         case 'T':
            pclr = &w->rc.taskclr;
            clr = *pclr;
            tgt = key;
            break;
         case '0': case '1': case '2': case '3':
         case '4': case '5': case '6': case '7':
            clr = key - '0';
            *pclr = clr;
            break;
         case kbd_UP:
            ++clr;
            if (clr >= max_colors) clr = 0;
            *pclr = clr;
            break;
         case kbd_DOWN:
            --clr;
            if (clr < 0) clr = max_colors - 1;
            *pclr = clr;
            break;
         case 'B':
            TOGw(w, View_NOBOLD);
            break;
         case 'b':
            TOGw(w, Show_HIBOLD);
            break;
         case 'z':
            TOGw(w, Show_COLORS);
            break;
         case 'a':
         case 'w':
            wins_clrhlp((w = win_select(key)), 1);
            clr = w->rc.taskclr, pclr = &w->rc.taskclr;
            tgt = 'T';
            break;
         default:
            break;                // keep gcc happy
      }
      capsmk(w);
   } while (key != kbdAPPLY && key != kbdABORT);

   if (key == kbdABORT || key == kbd_ESC) wins_clrhlp(w, 0);

 #undef kbdABORT
 #undef kbdAPPLY
} // end: wins_colors


        /*
         * Manipulate flag(s) for all our windows. */
static void wins_reflag (int what, int flg) {
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy

   do {
      switch (what) {
         case Flags_TOG:
            TOGw(w, flg);
            break;
         case Flags_SET:          // Ummmm, i can't find anybody
            SETw(w, flg);         // who uses Flags_set ...
            break;
         case Flags_OFF:
            OFFw(w, flg);
            break;
         default:                 // keep gcc happy
            break;
      }
         /* a flag with special significance -- user wants to rebalance
            display so we gotta' off some stuff then force on two flags... */
      if (EQUWINS_xxx == flg)
         win_reset(w);

      w = w->next;
   } while (w != Curwin);
} // end: wins_reflag


        /*
         * Set up the raw/incomplete field group windows --
         * they'll be finished off after startup completes.
         * [ and very likely that will override most/all of our efforts ]
         * [               --- life-is-NOT-fair ---                     ] */
static void wins_stage_1 (void) {
   WIN_t *w;
   int i;

   for (i = 0; i < GROUPSMAX; i++) {
      w = &Winstk[i];
      w->winnum = i + 1;
      w->rc = Rc.win[i];
      w->captab[0] = Cap_norm;
      w->captab[1] = Cap_norm;
      w->captab[2] = w->cap_bold;
      w->captab[3] = w->capclr_sum;
      w->captab[4] = w->capclr_msg;
      w->captab[5] = w->capclr_pmt;
      w->captab[6] = w->capclr_hdr;
      w->captab[7] = w->capclr_rowhigh;
      w->captab[8] = w->capclr_rownorm;
      w->next = w + 1;
      w->prev = w - 1;
   }

   // fixup the circular chains...
   Winstk[GROUPSMAX - 1].next = &Winstk[0];
   Winstk[0].prev = &Winstk[GROUPSMAX - 1];
   Curwin = Winstk;
} // end: wins_stage_1


        /*
         * This guy just completes the field group windows after the
         * rcfiles have been read and command line arguments parsed.
         * And since he's the cabose of startup, he'll also tidy up
         * a few final things... */
static void wins_stage_2 (void) {
   int i;

   for (i = 0; i < GROUPSMAX; i++) {
      win_names(&Winstk[i], Winstk[i].rc.winname);
      capsmk(&Winstk[i]);
      Winstk[i].findstr = alloc_c(FNDBUFSIZ);
      Winstk[i].findlen = 0;
   }
   if (!Batch)
      putp((Cursor_state = Cap_curs_hide));
   else {
      OFFw(Curwin, View_SCROLL);
      signal(SIGHUP, SIG_IGN);    // allow running under nohup
   }
   // fill in missing Fieldstab members and build each window's columnhdr
   zap_fieldstab();

#ifndef OFF_STDERROR
   /* there's a chance that damn libnuma may spew to stderr so we gotta
      make sure he does not corrupt poor ol' top's first output screen!
      Yes, he provides some overridable 'weak' functions to change such
      behavior but we can't exploit that since we don't follow a normal
      ld route to symbol resolution (we use that dlopen() guy instead)! */
   Stderr_save = dup(fileno(stderr));
   if (-1 < Stderr_save && freopen("/dev/null", "w", stderr))
      ;                           // avoid -Wunused-result
#endif

   // with preserved 'other filters' & command line 'user filters',
   // we must ensure that we always have a visible task on row one.
   mkVIZrow1(Curwin);

   // lastly, initialize a signal set used to throttle one troublesome signal
   sigemptyset(&Sigwinch_set);
#ifdef SIGNALS_LESS
   sigaddset(&Sigwinch_set, SIGWINCH);
#endif
} // end: wins_stage_2


        /*
         * Determine if this task matches the 'u/U' selection
         * criteria for a given window */
static inline int wins_usrselect (const WIN_t *q, const int idx) {
   proc_t *p = q->ppt[idx];
   switch(q->usrseltyp) {
      case 0:                                    // uid selection inactive
         return 1;
      case 'U':                                  // match any uid
         if (p->ruid == q->usrseluid) return q->usrselflg;
         if (p->suid == q->usrseluid) return q->usrselflg;
         if (p->fuid == q->usrseluid) return q->usrselflg;
      // fall through...
      case 'u':                                  // match effective uid
         if (p->euid == q->usrseluid) return q->usrselflg;
      // fall through...
      default:                                   // no match...
         ;
   }
   return !q->usrselflg;
} // end: wins_usrselect

/*######  Forest View support  ###########################################*/

        /*
         * We try keeping most existing code unaware of these activities
         * ( plus, maintain alphabetical order within carefully chosen )
         * ( function names of: forest_a, forest_b, forest_c, forest_d )
         * ( with each name exactly 1 letter more than its predecessor ) */
static proc_t **Seed_ppt;                   // temporary win ppt pointer
static proc_t **Tree_ppt;                   // forest_create will resize
static int      Tree_idx;                   // frame_make resets to zero
        /* the next three support collapse/expand children. the Hide_pid
           array holds parent pids whose children have been manipulated.
           positive pid values represent parents with collapsed children
           while a negative pid value means children have been expanded.
           ( the first two are managed under the 'keys_task()' routine ) */
static int *Hide_pid;                       // collapsible process array
static int  Hide_tot;                       // total used in above array
#ifndef TREE_VCPUOFF
static unsigned *Hide_cpu;                  // accum tics from collapsed
#endif

        /*
         * This little recursive guy is the real forest view workhorse.
         * He fills in the Tree_ppt array and also sets the child indent
         * level which is stored in an unused proc_t padding byte. */
static void forest_adds (const int self, int level) {
   int i;

   if (Tree_idx < Frame_maxtask) {          // immunize against insanity
      if (level > 100) level = 101;         // our arbitrary nests limit
      Tree_ppt[Tree_idx] = Seed_ppt[self];  // add this as root or child
      Tree_ppt[Tree_idx++]->pad_3 = level;  // borrow 1 byte, 127 levels
#ifdef TREE_SCANALL
      for (i = 0; i < Frame_maxtask; i++) {
         if (i == self) continue;
#else
      for (i = self + 1; i < Frame_maxtask; i++) {
#endif
         if (Seed_ppt[self]->tid == Seed_ppt[i]->tgid
         || (Seed_ppt[self]->tid == Seed_ppt[i]->ppid && Seed_ppt[i]->tid == Seed_ppt[i]->tgid))
            forest_adds(i, level + 1);      // got one child any others?
      }
   }
} // end: forest_adds


#ifndef TREE_SCANALL
        /*
         * Our qsort callback to order a ppt by the non-display start_time
         * which will make us immune from any pid, ppid or tgid anomalies
         * if/when pid values are wrapped by the kernel! */
static int forest_based (const proc_t **x, const proc_t **y) {
   if ( (*x)->start_time > (*y)->start_time ) return  1;
   if ( (*x)->start_time < (*y)->start_time ) return -1;
   return 0;
} // end: forest_based
#endif


        /*
         * This routine is responsible for preparing the proc_t's for
         * a forest display in a designated window. After completion,
         * he will replace the original window ppt with our specially
         * ordered forest version. He also marks any hidden children! */
static void forest_create (WIN_t *q) {
   static int hwmsav;
   int i, j;

   Seed_ppt = q->ppt;                       // avoid passing WIN_t ptrs
   if (!Tree_idx) {                         // do just once per frame
      if (hwmsav < Frame_maxtask) {         // grow, but never shrink
         hwmsav = Frame_maxtask;
         Tree_ppt = alloc_r(Tree_ppt, sizeof(proc_t *) * hwmsav);
#ifndef TREE_VCPUOFF
         Hide_cpu = alloc_r(Hide_cpu, sizeof(unsigned) * hwmsav);
#endif
      }

#ifndef TREE_SCANALL
      qsort(Seed_ppt, Frame_maxtask, sizeof(proc_t *), (QFP_t)forest_based);
#endif
      for (i = 0; i < Frame_maxtask; i++) { // avoid any hidepid distortions
         if (!Seed_ppt[i]->pad_3)           // real & pseudo parents == zero
            forest_adds(i, 0);              // add a parent and its children
      }
#ifndef TREE_VCPUOFF
      memset(Hide_cpu, 0, sizeof(unsigned) * Frame_maxtask);
#endif
      /* we're borrowing some pad bytes in the proc_t,
         pad_2: 'x' means a collapsed thread, 'z' means an unseen child
         pad_3: where level number is stored (0 - 100) */
      for (i = 0; i < Hide_tot; i++) {
         if (Hide_pid[i] > 0) {
            for (j = 0; j < Frame_maxtask; j++) {
               if (Tree_ppt[j]->tid == Hide_pid[i]) {
                  int parent = j;
                  int children = 0;
                  char level = Tree_ppt[j]->pad_3;

                  while (j+1 < Frame_maxtask && Tree_ppt[j+1]->pad_3 > level) {
                     ++j;
                     Tree_ppt[j]->pad_2 = 'z';
#ifndef TREE_VCPUOFF
                     Hide_cpu[parent] += Tree_ppt[j]->pcpu;
#endif
                     children = 1;
                  }
                  /* if any children found (and collapsed), mark the parent
                     ( when children aren't found we won't negate the pid )
                     ( to prevent a future scan since who's to say such a )
                     ( task won't fork one or more children in the future ) */
                  if (children) Tree_ppt[parent]->pad_2 = 'x';
                  // this will force a check of the next Hide_pid[], if any
                  j = Frame_maxtask + 1;
               }
            }
            // if target task disappeared (ended), prevent further scanning
            if (j == Frame_maxtask) Hide_pid[i] = -Hide_pid[i];
         }
      }
   }
   memcpy(Seed_ppt, Tree_ppt, sizeof(proc_t *) * Frame_maxtask);
} // end: forest_create


        /*
         * This guy adds the artwork to either p->cmd or p->cmdline
         * when in forest view mode, otherwise he just returns 'em. */
static inline const char *forest_display (const WIN_t *q, const proc_t *p) {
#ifndef SCROLLVAR_NO
   static char buf[1024*64*2]; // the same as readproc's MAX_BUFSZ
#else
   static char buf[ROWMINSIZ];
#endif
   const char *which = (CHKw(q, Show_CMDLIN)) ? *p->cmdline : p->cmd;

   if (!CHKw(q, Show_FOREST) || !p->pad_3) return which;
#ifndef TREE_VWINALL
   if (q == Curwin)            // note: the following is NOT indented
#endif
   if (p->pad_2 == 'x') {
#ifdef TREE_VALTMRK
      snprintf(buf, sizeof(buf), "%*s%s", (4 * p->pad_3), "`+ ", which);
#else
      snprintf(buf, sizeof(buf), "+%*s%s", ((4 * p->pad_3) - 1), "`- ", which);
#endif
      return buf;
   }
   if (p->pad_3 > 100) snprintf(buf, sizeof(buf), "%400s%s", " +  ", which);
   else snprintf(buf, sizeof(buf), "%*s%s", (4 * p->pad_3), " `- ", which);
   return buf;
} // end: forest_display

/*######  Interactive Input Tertiary support  ############################*/

  /*
   * This section exists so as to offer some function naming freedom
   * while also maintaining the strict alphabetical order protocol
   * within each section. */

        /*
         * This guy is a *Helper* function serving the following two masters:
         *   find_string() - find the next match in a given window
         *   task_show()   - highlight all matches currently in-view
         * If q->findstr is found in the designated buffer, he returns the
         * offset from the start of the buffer, otherwise he returns -1. */
static inline int find_ofs (const WIN_t *q, const char *buf) {
   char *fnd;

   if (q->findstr[0] && (fnd = STRSTR(buf, q->findstr)))
      return (int)(fnd - buf);
   return -1;
} // end: find_ofs



   /* This is currently the one true prototype require by top.
      It is placed here, instead of top.h, so as to avoid a compiler
      warning when top_nls.c is compiled. */
static const char *task_show (const WIN_t *q, const int idx);

static void find_string (int ch) {
 #define reDUX (found) ? N_txt(WORD_another_txt) : ""
   static int found;
   int i;

   if ('&' == ch && !Curwin->findstr[0]) {
      show_msg(N_txt(FIND_no_next_txt));
      return;
   }
   if ('L' == ch) {
      char *str = ioline(N_txt(GET_find_str_txt));
      if (*str == kbd_ESC) return;
      snprintf(Curwin->findstr, FNDBUFSIZ, "%s", str);
      Curwin->findlen = strlen(Curwin->findstr);
      found = 0;
#ifndef USE_X_COLHDR
      if (Curwin->findstr[0]) SETw(Curwin, NOHIFND_xxx);
      else OFFw(Curwin, NOHIFND_xxx);
#endif
   }
   if (Curwin->findstr[0]) {
      SETw(Curwin, NOPRINT_xxx);
      for (i = Curwin->begtask; i < Frame_maxtask; i++) {
         const char *row = task_show(Curwin, i);
         if (*row && -1 < find_ofs(Curwin, row)) {
            found = 1;
            if (i == Curwin->begtask) continue;
            Curwin->begtask = i;
            return;
         }
      }
      show_msg(fmtmk(N_fmt(FIND_no_find_fmt), reDUX, Curwin->findstr));
   }
 #undef reDUX
} // end: find_string


static void help_view (void) {
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy
   char key = 1;

   putp((Cursor_state = Cap_curs_huge));
signify_that:
   putp(Cap_clr_scr);
   adj_geometry();

   show_special(1, fmtmk(N_unq(KEYS_helpbas_fmt)
      , PACKAGE_STRING
      , w->grpname
      , CHKw(w, Show_CTIMES) ? N_txt(ON_word_only_txt) : N_txt(OFF_one_word_txt)
      , Rc.delay_time
      , Secure_mode ? N_txt(ON_word_only_txt) : N_txt(OFF_one_word_txt)
      , Secure_mode ? "" : N_unq(KEYS_helpext_fmt)));
   putp(Cap_clr_eos);
   fflush(stdout);

   if (Frames_signal) goto signify_that;
   key = iokey(1);
   if (key < 1) goto signify_that;

   switch (key) {
      case kbd_ESC: case 'q':
         break;
      case '?': case 'h': case 'H':
         do {
            putp(Cap_home);
            show_special(1, fmtmk(N_unq(WINDOWS_help_fmt)
               , w->grpname
               , Winstk[0].rc.winname, Winstk[1].rc.winname
               , Winstk[2].rc.winname, Winstk[3].rc.winname));
            putp(Cap_clr_eos);
            fflush(stdout);
            if (Frames_signal || (key = iokey(1)) < 1) {
               adj_geometry();
               putp(Cap_clr_scr);
            } else w = win_select(key);
         } while (key != kbd_ENTER && key != kbd_ESC);
         break;
      default:
         goto signify_that;
   }
} // end: help_view


static void other_filters (int ch) {
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy
   const char *txt, *p;
   char *glob;

   switch (ch) {
      case 'o':
      case 'O':
         if (ch == 'o') txt = N_txt(OSEL_casenot_txt);
         else txt = N_txt(OSEL_caseyes_txt);
         glob = ioline(fmtmk(N_fmt(OSEL_prompts_fmt), w->osel_tot + 1, txt));
         if (*glob == kbd_ESC || *glob == '\0')
            return;
         if ((p = osel_add(w, ch, glob, 1))) {
            show_msg(p);
            return;
         }
#ifndef USE_X_COLHDR
         SETw(w, NOHISEL_xxx);
#endif
         break;
      case kbd_CtrlO:
         if (VIZCHKw(w)) {
            char buf[SCREENMAX], **pp;
            struct osel_s *osel;
            int i;

            i = 0;
            osel = w->osel_1st;
            pp = alloc_c((w->osel_tot + 1) * sizeof(char **));
            while (osel && i < w->osel_tot) {
               pp[i++] = osel->raw;
               osel = osel->nxt;
            }
            buf[0] = '\0';
            for ( ; i > 0; )
               strncat(buf, fmtmk("%s'%s'", " + " , pp[--i]), sizeof(buf) - (strlen(buf) + 1));
            if (buf[0]) p = buf + strspn(buf, " + ");
            else p = N_txt(WORD_noneone_txt);
            ioline(fmtmk(N_fmt(OSEL_statlin_fmt), p));
            free(pp);
         }
         break;
      default:                    // keep gcc happy
         break;
   }
} // end: other_filters


static void write_rcfile (void) {
   FILE *fp;
   int i;

   if (Rc_questions) {
      show_pmt(N_txt(XTRA_warncfg_txt));
      if ('y' != tolower(iokey(1)))
         return;
      Rc_questions = 0;
   }
   if (!(fp = fopen(Rc_name, "w"))) {
      show_msg(fmtmk(N_fmt(FAIL_rc_open_fmt), Rc_name, strerror(errno)));
      return;
   }
   fprintf(fp, "%s's " RCF_EYECATCHER, Myname);
   fprintf(fp, "Id:%c, Mode_altscr=%d, Mode_irixps=%d, Delay_time=%d.%d, Curwin=%d\n"
      , RCF_VERSION_ID
      , Rc.mode_altscr, Rc.mode_irixps
        // this may be ugly, but it keeps us locale independent...
      , (int)Rc.delay_time, (int)((Rc.delay_time - (int)Rc.delay_time) * 1000)
      , (int)(Curwin - Winstk));

   for (i = 0 ; i < GROUPSMAX; i++) {
      fprintf(fp, "%s\tfieldscur=%s\n"
         , Winstk[i].rc.winname, Winstk[i].rc.fieldscur);
      fprintf(fp, "\twinflags=%d, sortindx=%d, maxtasks=%d, graph_cpus=%d, graph_mems=%d\n"
         , Winstk[i].rc.winflags, Winstk[i].rc.sortindx, Winstk[i].rc.maxtasks
         , Winstk[i].rc.graph_cpus,  Winstk[i].rc.graph_mems);
      fprintf(fp, "\tsummclr=%d, msgsclr=%d, headclr=%d, taskclr=%d\n"
         , Winstk[i].rc.summclr, Winstk[i].rc.msgsclr
         , Winstk[i].rc.headclr, Winstk[i].rc.taskclr);
   }

   // any new addition(s) last, for older rcfiles compatibility...
   fprintf(fp, "Fixed_widest=%d, Summ_mscale=%d, Task_mscale=%d, Zero_suppress=%d\n"
      , Rc.fixed_widest, Rc.summ_mscale, Rc.task_mscale, Rc.zero_suppress);

   if (Winstk[0].osel_tot + Winstk[1].osel_tot
     + Winstk[2].osel_tot + Winstk[3].osel_tot) {
      fprintf(fp, "\n");
      fprintf(fp, Osel_delim_1_txt);
      for (i = 0 ; i < GROUPSMAX; i++) {
         struct osel_s *osel = Winstk[i].osel_1st;
         if (osel) {
            fprintf(fp, Osel_window_fmts, i, Winstk[i].osel_tot);
            do {
               fprintf(fp, Osel_filterO_fmt, osel->typ, osel->raw);
               osel = osel->nxt;
            } while (osel);
         }
      }
      fprintf(fp, Osel_delim_2_txt);
   }

   if (Inspect.raw)
      fputs(Inspect.raw, fp);

   fclose(fp);
   show_msg(fmtmk(N_fmt(WRITE_rcfile_fmt), Rc_name));
} // end: write_rcfile

/*######  Interactive Input Secondary support (do_key helpers)  ##########*/

  /*
   *  These routines exist just to keep the do_key() function
   *  a reasonably modest size. */

static void keys_global (int ch) {
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy

   switch (ch) {
      case '?':
      case 'h':
         help_view();
         break;
      case 'B':
         TOGw(w, View_NOBOLD);
         capsmk(w);
         break;
      case 'd':
      case 's':
         if (Secure_mode)
            show_msg(N_txt(NOT_onsecure_txt));
         else {
            float tmp =
               get_float(fmtmk(N_fmt(DELAY_change_fmt), Rc.delay_time));
            if (tmp > -1) Rc.delay_time = tmp;
         }
         break;
      case 'E':
         if (++Rc.summ_mscale > SK_Eb) Rc.summ_mscale = SK_Kb;
         break;
      case 'e':
         if (++Rc.task_mscale > SK_Pb) Rc.task_mscale = SK_Kb;
         break;
      case 'F':
      case 'f':
         fields_utility();
         break;
      case 'g':
         win_select(0);
         break;
      case 'H':
         Thread_mode = !Thread_mode;
         if (!CHKw(w, View_STATES))
            show_msg(fmtmk(N_fmt(THREADS_show_fmt)
               , Thread_mode ? N_txt(ON_word_only_txt) : N_txt(OFF_one_word_txt)));
         Winstk[0].begtask = Winstk[1].begtask = Winstk[2].begtask = Winstk[3].begtask = 0;
         // force an extra procs refresh to avoid %cpu distortions...
         Pseudo_row = PROC_XTRA;
         break;
      case 'I':
         if (smp_num_cpus > 1) {
            Rc.mode_irixps = !Rc.mode_irixps;
            show_msg(fmtmk(N_fmt(IRIX_curmode_fmt)
               , Rc.mode_irixps ? N_txt(ON_word_only_txt) : N_txt(OFF_one_word_txt)));
         } else
            show_msg(N_txt(NOT_smp_cpus_txt));
         break;
      case 'k':
         if (Secure_mode) {
            show_msg(N_txt(NOT_onsecure_txt));
         } else {
            int sig = SIGTERM,
                def = w->ppt[w->begtask]->tid,
                pid = get_int(fmtmk(N_txt(GET_pid2kill_fmt), def));
            if (pid > GET_NUM_ESC) {
               char *str;
               if (pid == GET_NUM_NOT) pid = def;
               str = ioline(fmtmk(N_fmt(GET_sigs_num_fmt), pid, SIGTERM));
               if (*str != kbd_ESC) {
                  if (*str) sig = signal_name_to_number(str);
                  if (Frames_signal) break;
                  if (0 < sig && kill(pid, sig))
                     show_msg(fmtmk(N_fmt(FAIL_signals_fmt)
                        , pid, sig, strerror(errno)));
                  else if (0 > sig) show_msg(N_txt(BAD_signalid_txt));
               }
            }
         }
         break;
      case 'r':
         if (Secure_mode)
            show_msg(N_txt(NOT_onsecure_txt));
         else {
            int val,
                def = w->ppt[w->begtask]->tid,
                pid = get_int(fmtmk(N_fmt(GET_pid2nice_fmt), def));
            if (pid > GET_NUM_ESC) {
               if (pid == GET_NUM_NOT) pid = def;
               val = get_int(fmtmk(N_fmt(GET_nice_num_fmt), pid));
               if (val > GET_NUM_NOT
               && setpriority(PRIO_PROCESS, (unsigned)pid, val))
                  show_msg(fmtmk(N_fmt(FAIL_re_nice_fmt)
                     , pid, val, strerror(errno)));
            }
         }
         break;
      case 'X':
      {  int wide = get_int(fmtmk(N_fmt(XTRA_fixwide_fmt), Rc.fixed_widest));
         if (wide > GET_NUM_NOT) {
            if (wide >= 0 && wide <= SCREENMAX) Rc.fixed_widest = wide;
            else Rc.fixed_widest = -1;
         }
      }
         break;
      case 'Y':
         if (!Inspect.total)
            ioline(N_txt(YINSP_noents_txt));
         else {
            int def = w->ppt[w->begtask]->tid,
                pid = get_int(fmtmk(N_fmt(YINSP_pidsee_fmt), def));
            if (pid > GET_NUM_ESC) {
               if (pid == GET_NUM_NOT) pid = def;
               if (pid) inspection_utility(pid);
            }
         }
         break;
      case 'Z':
         wins_colors();
         break;
      case '0':
         Rc.zero_suppress = !Rc.zero_suppress;
         break;
      case kbd_ENTER:             // these two have the effect of waking us
      case kbd_SPACE:             // from 'pselect', refreshing the display
         break;                   // and updating any hot-plugged resources
      default:                    // keep gcc happy
         break;
   }
} // end: keys_global


static void keys_summary (int ch) {
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy

   switch (ch) {
      case '1':
         if (CHKw(w, View_CPUNOD)) OFFw(w, View_CPUSUM);
         else TOGw(w, View_CPUSUM);
         OFFw(w, View_CPUNOD);
         SETw(w, View_STATES);
         break;
      case '2':
         if (!Numa_node_tot)
            show_msg(N_txt(NUMA_nodenot_txt));
         else {
            if (Numa_node_sel < 0) TOGw(w, View_CPUNOD);
            if (!CHKw(w, View_CPUNOD)) SETw(w, View_CPUSUM);
            SETw(w, View_STATES);
            Numa_node_sel = -1;
         }
         break;
      case '3':
         if (!Numa_node_tot)
            show_msg(N_txt(NUMA_nodenot_txt));
         else {
            int num = get_int(fmtmk(N_fmt(NUMA_nodeget_fmt), Numa_node_tot -1));
            if (num > GET_NUM_NOT) {
               if (num >= 0 && num < Numa_node_tot) {
                  Numa_node_sel = num;
                  SETw(w, View_CPUNOD | View_STATES);
                  OFFw(w, View_CPUSUM);
               } else
                  show_msg(N_txt(NUMA_nodebad_txt));
            }
         }
         break;
      case 'C':
         VIZTOGw(w, View_SCROLL);
         break;
      case 'l':
         TOGw(w, View_LOADAV);
         break;
      case 'm':
         if (!CHKw(w, View_MEMORY))
            SETw(w, View_MEMORY);
         else if (++w->rc.graph_mems > 2) {
            w->rc.graph_mems = 0;;
            OFFw(w, View_MEMORY);
         }
         break;
      case 't':
         if (!CHKw(w, View_STATES))
            SETw(w, View_STATES);
         else if (++w->rc.graph_cpus > 2) {
            w->rc.graph_cpus = 0;;
            OFFw(w, View_STATES);
         }
         break;
      default:                    // keep gcc happy
         break;
   }
} // end: keys_summary


static void keys_task (int ch) {
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy

   switch (ch) {
      case '#':
      case 'n':
         if (VIZCHKw(w)) {
            int num = get_int(fmtmk(N_fmt(GET_max_task_fmt), w->rc.maxtasks));
            if (num > GET_NUM_NOT) {
               if (-1 < num ) w->rc.maxtasks = num;
               else show_msg(N_txt(BAD_max_task_txt));
            }
         }
         break;
      case '<':
#ifdef TREE_NORESET
         if (CHKw(w, Show_FOREST)) break;
#endif
         if (VIZCHKw(w)) {
            FLG_t *p = w->procflgs + w->maxpflgs - 1;
            while (p > w->procflgs && *p != w->rc.sortindx) --p;
            if (*p == w->rc.sortindx) {
               --p;
#ifndef USE_X_COLHDR
               if (EU_MAXPFLGS < *p) --p;
#endif
               if (p >= w->procflgs) {
                  w->rc.sortindx = *p;
#ifndef TREE_NORESET
                  OFFw(w, Show_FOREST);
#endif
               }
            }
         }
         break;
      case '>':
#ifdef TREE_NORESET
         if (CHKw(w, Show_FOREST)) break;
#endif
         if (VIZCHKw(w)) {
            FLG_t *p = w->procflgs + w->maxpflgs - 1;
            while (p > w->procflgs && *p != w->rc.sortindx) --p;
            if (*p == w->rc.sortindx) {
               ++p;
#ifndef USE_X_COLHDR
               if (EU_MAXPFLGS < *p) ++p;
#endif
               if (p < w->procflgs + w->maxpflgs) {
                  w->rc.sortindx = *p;
#ifndef TREE_NORESET
                  OFFw(w, Show_FOREST);
#endif
               }
            }
         }
         break;
      case 'b':
         TOGw(w, Show_HIBOLD);
         capsmk(w);
         break;
      case 'c':
         VIZTOGw(w, Show_CMDLIN);
         break;
      case 'i':
      {  static WIN_t *w_sav;
         static int beg_sav;
         if (w_sav != w) { beg_sav = 0; w_sav = w; }
         if (CHKw(w, Show_IDLEPS)) { beg_sav = w->begtask; w->begtask = 0; }
         else { w->begtask = beg_sav; beg_sav = 0; }
      }
         VIZTOGw(w, Show_IDLEPS);
         break;
      case 'J':
         VIZTOGw(w, Show_JRNUMS);
         break;
      case 'j':
         VIZTOGw(w, Show_JRSTRS);
         break;
      case 'R':
#ifdef TREE_NORESET
         if (!CHKw(w, Show_FOREST)) VIZTOGw(w, Qsrt_NORMAL);
#else
         if (VIZCHKw(w)) {
            TOGw(w, Qsrt_NORMAL);
            OFFw(w, Show_FOREST);
         }
#endif
         break;
      case 'S':
         if (VIZCHKw(w)) {
            TOGw(w, Show_CTIMES);
            show_msg(fmtmk(N_fmt(TIME_accumed_fmt) , CHKw(w, Show_CTIMES)
               ? N_txt(ON_word_only_txt) : N_txt(OFF_one_word_txt)));
         }
         break;
      case 'O':
      case 'o':
      case kbd_CtrlO:
         if (VIZCHKw(w)) {
            other_filters(ch);
            mkVIZrow1(w);
         }
         break;
      case 'U':
      case 'u':
         if (VIZCHKw(w)) {
            const char *errmsg, *str = ioline(N_txt(GET_user_ids_txt));
            if (*str != kbd_ESC
            && (errmsg = user_certify(w, str, ch)))
                show_msg(errmsg);
            mkVIZrow1(w);
         }
         break;
      case 'V':
         if (VIZCHKw(w)) {
            TOGw(w, Show_FOREST);
            if (!ENUviz(w, EU_CMD))
               show_msg(fmtmk(N_fmt(FOREST_modes_fmt) , CHKw(w, Show_FOREST)
                  ? N_txt(ON_word_only_txt) : N_txt(OFF_one_word_txt)));
         }
         break;
      case 'v':
         if (VIZCHKw(w)) {
            if (CHKw(w, Show_FOREST)) {
               int i, pid = w->ppt[w->begtask]->tid;
#ifdef TREE_VPROMPT
               int got = get_int(fmtmk(N_txt(XTRA_vforest_fmt), pid));
               if (got < GET_NUM_NOT) break;
               if (got > GET_NUM_NOT) pid = got;
#endif
               for (i = 0; i < Hide_tot; i++) {
                  if (Hide_pid[i] == pid || Hide_pid[i] == -pid) {
                     Hide_pid[i] = -Hide_pid[i];
                     break;
                  }
               }
               if (i == Hide_tot) {
                  static int totsav;
                  if (Hide_tot >= totsav) {
                     totsav += 128;
                     Hide_pid = alloc_r(Hide_pid, sizeof(int) * totsav);
                  }
                  Hide_pid[Hide_tot++] = pid;
               } else {
                  // if everything's expanded, let's empty the array ...
                  for (i = 0; i < Hide_tot; i++)
                     if (Hide_pid[i] > 0) break;
                  if (i == Hide_tot) Hide_tot = 0;
               }
            }
         }
         break;
      case 'x':
         if (VIZCHKw(w)) {
#ifdef USE_X_COLHDR
            TOGw(w, Show_HICOLS);
            capsmk(w);
#else
            if (ENUviz(w, w->rc.sortindx)
            && !CHKw(w, NOHIFND_xxx | NOHISEL_xxx)) {
               TOGw(w, Show_HICOLS);
               if (ENUpos(w, w->rc.sortindx) < w->begpflg) {
                  if (CHKw(w, Show_HICOLS)) w->begpflg += 2;
                  else w->begpflg -= 2;
                  if (0 > w->begpflg) w->begpflg = 0;
               }
               capsmk(w);
            }
#endif
         }
         break;
      case 'y':
         if (VIZCHKw(w)) {
            TOGw(w, Show_HIROWS);
            capsmk(w);
         }
         break;
      case 'z':
         if (VIZCHKw(w)) {
            TOGw(w, Show_COLORS);
            capsmk(w);
         }
         break;
      default:                    // keep gcc happy
         break;
   }
} // end: keys_task


static void keys_window (int ch) {
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy

   switch (ch) {
      case '+':
         if (ALTCHKw) wins_reflag(Flags_OFF, EQUWINS_xxx);
         Hide_tot = 0;
         break;
      case '-':
         if (ALTCHKw) TOGw(w, Show_TASKON);
         break;
      case '=':
         win_reset(w);
         Hide_tot = 0;
         break;
      case '_':
         if (ALTCHKw) wins_reflag(Flags_TOG, Show_TASKON);
         break;
      case '&':
      case 'L':
         if (VIZCHKw(w)) find_string(ch);
         break;
      case 'A':
         Rc.mode_altscr = !Rc.mode_altscr;
         break;
      case 'a':
      case 'w':
         if (ALTCHKw) win_select(ch);
         break;
      case 'G':
         if (ALTCHKw) {
            char tmp[SMLBUFSIZ];
            STRLCPY(tmp, ioline(fmtmk(N_fmt(NAME_windows_fmt), w->rc.winname)));
            if (tmp[0] && tmp[0] != kbd_ESC) win_names(w, tmp);
         }
         break;
      case kbd_UP:
         if (VIZCHKw(w)) if (CHKw(w, Show_IDLEPS)) w->begnext = -1;
         break;
      case kbd_DOWN:
         if (VIZCHKw(w)) if (CHKw(w, Show_IDLEPS)) w->begnext = +1;
         break;
#ifdef USE_X_COLHDR // ------------------------------------
      case kbd_LEFT:
#ifndef SCROLLVAR_NO
         if (VIZCHKw(w)) {
            if (VARleft(w))
               w->varcolbeg -= SCROLLAMT;
            else if (0 < w->begpflg)
               w->begpflg -= 1;
         }
#else
         if (VIZCHKw(w)) if (0 < w->begpflg) w->begpflg -= 1;
#endif
         break;
      case kbd_RIGHT:
#ifndef SCROLLVAR_NO
         if (VIZCHKw(w)) {
            if (VARright(w)) {
               w->varcolbeg += SCROLLAMT;
               if (0 > w->varcolbeg) w->varcolbeg = 0;
            } else if (w->begpflg + 1 < w->totpflgs)
               w->begpflg += 1;
         }
#else
         if (VIZCHKw(w)) if (w->begpflg + 1 < w->totpflgs) w->begpflg += 1;
#endif
         break;
#else  // USE_X_COLHDR ------------------------------------
      case kbd_LEFT:
#ifndef SCROLLVAR_NO
         if (VIZCHKw(w)) {
            if (VARleft(w))
               w->varcolbeg -= SCROLLAMT;
            else if (0 < w->begpflg) {
               w->begpflg -= 1;
               if (EU_MAXPFLGS < w->pflgsall[w->begpflg]) w->begpflg -= 2;
            }
         }
#else
         if (VIZCHKw(w)) if (0 < w->begpflg) {
            w->begpflg -= 1;
            if (EU_MAXPFLGS < w->pflgsall[w->begpflg]) w->begpflg -= 2;
         }
#endif
         break;
      case kbd_RIGHT:
#ifndef SCROLLVAR_NO
         if (VIZCHKw(w)) {
            if (VARright(w)) {
               w->varcolbeg += SCROLLAMT;
               if (0 > w->varcolbeg) w->varcolbeg = 0;
            } else if (w->begpflg + 1 < w->totpflgs) {
               if (EU_MAXPFLGS < w->pflgsall[w->begpflg])
                  w->begpflg += (w->begpflg + 3 < w->totpflgs) ? 3 : 0;
               else w->begpflg += 1;
            }
         }
#else
         if (VIZCHKw(w)) if (w->begpflg + 1 < w->totpflgs) {
            if (EU_MAXPFLGS < w->pflgsall[w->begpflg])
               w->begpflg += (w->begpflg + 3 < w->totpflgs) ? 3 : 0;
            else w->begpflg += 1;
         }
#endif
         break;
#endif // USE_X_COLHDR ------------------------------------
      case kbd_PGUP:
         if (VIZCHKw(w)) {
            if (CHKw(w, Show_IDLEPS) && 0 < w->begtask) {
               w->begnext = -(w->winlines - (Rc.mode_altscr ? 1 : 2));
            }
         }
         break;
      case kbd_PGDN:
         if (VIZCHKw(w)) {
            if (CHKw(w, Show_IDLEPS) && w->begtask < Frame_maxtask - 1) {
               w->begnext = +(w->winlines - (Rc.mode_altscr ? 1 : 2));
            }
         }
         break;
      case kbd_HOME:
#ifndef SCROLLVAR_NO
         if (VIZCHKw(w)) if (CHKw(w, Show_IDLEPS)) w->begtask = w->begpflg = w->varcolbeg = 0;
         mkVIZrow1(w);
#else
         if (VIZCHKw(w)) if (CHKw(w, Show_IDLEPS)) w->begtask = w->begpflg = 0;
         mkVIZrow1(w);
#endif
         break;
      case kbd_END:
         if (VIZCHKw(w)) {
            if (CHKw(w, Show_IDLEPS)) {
               w->begnext = (Frame_maxtask - w->winlines) + 1;
               w->begpflg = w->endpflg;
#ifndef SCROLLVAR_NO
               w->varcolbeg = 0;
#endif
            }
         }
         break;
      default:                    // keep gcc happy
         break;
   }
} // end: keys_window


static void keys_xtra (int ch) {
// const char *xmsg;
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy

#ifdef TREE_NORESET
   if (CHKw(w, Show_FOREST)) return;
#else
   OFFw(w, Show_FOREST);
#endif
   /* these keys represent old-top compatibility --
      they're grouped here so that if users could ever be weaned,
      we would just whack do_key's key_tab entry and this function... */
   switch (ch) {
      case 'M':
         w->rc.sortindx = EU_MEM;
//       xmsg = "Memory";
         break;
      case 'N':
         w->rc.sortindx = EU_PID;
//       xmsg = "Numerical";
         break;
      case 'P':
         w->rc.sortindx = EU_CPU;
//       xmsg = "CPU";
         break;
      case 'T':
         w->rc.sortindx = EU_TM2;
//       xmsg = "Time";
         break;
      default:                    // keep gcc happy
         break;
   }
// some have objected to this message, so we'll just keep silent...
// show_msg(fmtmk("%s sort compatibility key honored", xmsg));
} // end: keys_xtra

/*######  Main Screen routines  ##########################################*/

        /*
         * Process keyboard input during the main loop */
static void do_key (int ch) {
   static struct {
      void (*func)(int ch);
      char keys[SMLBUFSIZ];
   } key_tab[] = {
      { keys_global,
         { '?', 'B', 'd', 'E', 'e', 'F', 'f', 'g', 'H', 'h'
         , 'I', 'k', 'r', 's', 'X', 'Y', 'Z', '0'
         , kbd_ENTER, kbd_SPACE, '\0' } },
      { keys_summary,
         { '1', '2', '3', 'C', 'l', 'm', 't', '\0' } },
      { keys_task,
         { '#', '<', '>', 'b', 'c', 'i', 'J', 'j', 'n', 'O', 'o'
         , 'R', 'S', 'U', 'u', 'V', 'v', 'x', 'y', 'z'
         , kbd_CtrlO, '\0' } },
      { keys_window,
         { '+', '-', '=', '_', '&', 'A', 'a', 'G', 'L', 'w'
         , kbd_UP, kbd_DOWN, kbd_LEFT, kbd_RIGHT, kbd_PGUP, kbd_PGDN
         , kbd_HOME, kbd_END, '\0' } },
      { keys_xtra,
         { 'M', 'N', 'P', 'T', '\0'} }
   };
   int i;

   switch (ch) {
      case 0:                // ignored (always)
      case kbd_ESC:          // ignored (sometimes)
         goto all_done;
      case 'q':              // no return from this guy
         bye_bye(NULL);
      case 'W':              // no need for rebuilds
         write_rcfile();
         goto all_done;
      default:               // and now, the real work...
         for (i = 0; i < MAXTBL(key_tab); ++i)
            if (strchr(key_tab[i].keys, ch)) {
               key_tab[i].func(ch);
               Frames_signal = BREAK_kbd;
               goto all_done;
            }
   };
   /* Frames_signal above will force a rebuild of all column headers and
      the PROC_FILLxxx flags.  It's NOT simply lazy programming.  Here are
      some keys that COULD require new column headers and/or libproc flags:
         'A' - likely
         'c' - likely when !Mode_altscr, maybe when Mode_altscr
         'F' - likely
         'f' - likely
         'g' - likely
         'H' - likely
         'I' - likely
         'J' - always
         'j' - always
         'Z' - likely, if 'Curwin' changed when !Mode_altscr
         '-' - likely (restricted to Mode_altscr)
         '_' - likely (restricted to Mode_altscr)
         '=' - maybe, but only when Mode_altscr
         '+' - likely (restricted to Mode_altscr)
         PLUS, likely for FOUR of the EIGHT cursor motion keys (scrolled)
      ( At this point we have a human being involved and so have all the time )
      ( in the world.  We can afford a few extra cpu cycles every now & then! )
    */

   show_msg(N_txt(UNKNOWN_cmds_txt));
all_done:
   sysinfo_refresh(1);       // let's be more responsive to hot-pluggin'
   putp((Cursor_state = Cap_curs_hide));
} // end: do_key


        /*
         * State display *Helper* function to calc and display the state
         * percentages for a single cpu.  In this way, we can support
         * the following environments without the usual code bloat.
         *    1) single cpu machines
         *    2) modest smp boxes with room for each cpu's percentages
         *    3) massive smp guys leaving little or no room for process
         *       display and thus requiring the cpu summary toggle */
static void summary_hlp (CPU_t *cpu, const char *pfx) {
   /* we'll trim to zero if we get negative time ticks,
      which has happened with some SMP kernels (pre-2.4?)
      and when cpus are dynamically added or removed */
 #define TRIMz(x)  ((tz = (SIC_t)(x)) < 0 ? 0 : tz)
   //    user    syst    nice    idle    wait    hirg    sirq    steal
   SIC_t u_frme, s_frme, n_frme, i_frme, w_frme, x_frme, y_frme, z_frme, tot_frme, tz;
   float scale;

   u_frme = TRIMz(cpu->cur.u - cpu->sav.u);
   s_frme = TRIMz(cpu->cur.s - cpu->sav.s);
   n_frme = TRIMz(cpu->cur.n - cpu->sav.n);
   i_frme = TRIMz(cpu->cur.i - cpu->sav.i);
   w_frme = TRIMz(cpu->cur.w - cpu->sav.w);
   x_frme = TRIMz(cpu->cur.x - cpu->sav.x);
   y_frme = TRIMz(cpu->cur.y - cpu->sav.y);
   z_frme = TRIMz(cpu->cur.z - cpu->sav.z);
   tot_frme = u_frme + s_frme + n_frme + i_frme + w_frme + x_frme + y_frme + z_frme;
#ifndef CPU_ZEROTICS
   if (tot_frme < cpu->edge)
      tot_frme = u_frme = s_frme = n_frme = i_frme = w_frme = x_frme = y_frme = z_frme = 0;
#endif
   if (1 > tot_frme) i_frme = tot_frme = 1;
   scale = 100.0 / (float)tot_frme;

   /* display some kinda' cpu state percentages
      (who or what is explained by the passed prefix) */
   if (Curwin->rc.graph_cpus) {
      static struct {
         const char *user, *syst, *type;
      } gtab[] = {
         { "%-.*s~7", "%-.*s~8", Graph_bars },
         { "%-.*s~4", "%-.*s~6", Graph_blks }
      };
      char user[SMLBUFSIZ], syst[SMLBUFSIZ], dual[MEDBUFSIZ];
      int ix = Curwin->rc.graph_cpus - 1;
      float pct_user = (float)(u_frme + n_frme) * scale,
            pct_syst = (float)(s_frme + x_frme + y_frme) * scale;
#ifndef QUICK_GRAPHS
      int num_user = (int)((pct_user * Graph_adj) + .5),
          num_syst = (int)((pct_syst * Graph_adj) + .5);
      if (num_user + num_syst > Graph_len) num_syst = Graph_len - num_user;
      snprintf(user, sizeof(user), gtab[ix].user, num_user, gtab[ix].type);
      snprintf(syst, sizeof(syst), gtab[ix].syst, num_syst, gtab[ix].type);
#else
      snprintf(user, sizeof(user), gtab[ix].user, (int)((pct_user * Graph_adj) + .5), gtab[ix].type);
      snprintf(syst, sizeof(syst), gtab[ix].syst, (int)((pct_syst * Graph_adj) + .4), gtab[ix].type);
#endif
      snprintf(dual, sizeof(dual), "%s%s", user, syst);
      show_special(0, fmtmk("%%%s ~3%#5.1f~2/%-#5.1f~3 %3.0f[~1%-*s]~1\n"
         , pfx, pct_user, pct_syst, pct_user + pct_syst, Graph_len +4, dual));
   } else {
      show_special(0, fmtmk(Cpu_States_fmts, pfx
         , (float)u_frme * scale, (float)s_frme * scale
         , (float)n_frme * scale, (float)i_frme * scale
         , (float)w_frme * scale, (float)x_frme * scale
         , (float)y_frme * scale, (float)z_frme * scale));
   }
 #undef TRIMz
} // end: summary_hlp


        /*
         * In support of a new frame:
         *    1) Display uptime and load average (maybe)
         *    2) Display task/cpu states (maybe)
         *    3) Display memory & swap usage (maybe) */
static void summary_show (void) {
 #define isROOM(f,n) (CHKw(w, f) && Msg_row + (n) < Screen_rows - 1)
 #define anyFLG 0xffffff
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy
   char tmp[MEDBUFSIZ];
   int i;

   // Display Uptime and Loadavg
   if (isROOM(View_LOADAV, 1)) {
      if (!Rc.mode_altscr)
         show_special(0, fmtmk(LOADAV_line, Myname, sprint_uptime(0)));
      else
         show_special(0, fmtmk(CHKw(w, Show_TASKON)? LOADAV_line_alt : LOADAV_line
            , w->grpname, sprint_uptime(0)));
      Msg_row += 1;
   } // end: View_LOADAV

   // Display Task and Cpu(s) States
   if (isROOM(View_STATES, 2)) {
      show_special(0, fmtmk(N_unq(STATE_line_1_fmt)
         , Thread_mode ? N_txt(WORD_threads_txt) : N_txt(WORD_process_txt)
         , Frame_maxtask, Frame_running, Frame_sleepin
         , Frame_stopped, Frame_zombied));
      Msg_row += 1;

      cpus_refresh();

      if (!Numa_node_tot) goto numa_nope;

      if (CHKw(w, View_CPUNOD)) {
         if (Numa_node_sel < 0) {
            // display the 1st /proc/stat line, then the nodes (if room)
            summary_hlp(&Cpu_tics[smp_num_cpus], N_txt(WORD_allcpus_txt));
            Msg_row += 1;
            // display each cpu node's states
            for (i = 0; i < Numa_node_tot; i++) {
               CPU_t *nod_ptr = &Cpu_tics[1 + smp_num_cpus + i];
               if (!isROOM(anyFLG, 1)) break;
#ifndef OFF_NUMASKIP
               if (nod_ptr->id) {
#endif
               snprintf(tmp, sizeof(tmp), N_fmt(NUMA_nodenam_fmt), i);
               summary_hlp(nod_ptr, tmp);
               Msg_row += 1;
#ifndef OFF_NUMASKIP
               }
#endif
            }
         } else {
            // display the node summary, then the associated cpus (if room)
            snprintf(tmp, sizeof(tmp), N_fmt(NUMA_nodenam_fmt), Numa_node_sel);
            summary_hlp(&Cpu_tics[1 + smp_num_cpus + Numa_node_sel], tmp);
            Msg_row += 1;
            for (i = 0; i < Cpu_faux_tot; i++) {
               if (Numa_node_sel == Cpu_tics[i].node) {
                  if (!isROOM(anyFLG, 1)) break;
                  snprintf(tmp, sizeof(tmp), N_fmt(WORD_eachcpu_fmt), Cpu_tics[i].id);
                  summary_hlp(&Cpu_tics[i], tmp);
                  Msg_row += 1;
               }
            }
         }
      } else
numa_nope:
      if (CHKw(w, View_CPUSUM)) {
         // display just the 1st /proc/stat line
         summary_hlp(&Cpu_tics[smp_num_cpus], N_txt(WORD_allcpus_txt));
         Msg_row += 1;

      } else {
         // display each cpu's states separately, screen height permitting...
         for (i = 0; i < Cpu_faux_tot; i++) {
            snprintf(tmp, sizeof(tmp), N_fmt(WORD_eachcpu_fmt), Cpu_tics[i].id);
            summary_hlp(&Cpu_tics[i], tmp);
            Msg_row += 1;
            if (!isROOM(anyFLG, 1)) break;
         }
      }
   } // end: View_STATES

   // Display Memory and Swap stats
   if (isROOM(View_MEMORY, 2)) {
    #define bfT(n)  buftab[n].buf
    #define scT(e)  scaletab[Rc.summ_mscale]. e
    #define mkM(x) (float)kb_main_ ## x / scT(div)
    #define mkS(x) (float)kb_swap_ ## x / scT(div)
    #define prT(b,z) { if (9 < snprintf(b, 10, scT(fmts), z)) b[8] = '+'; }
      static struct {
         float div;
         const char *fmts;
         const char *label;
      } scaletab[] = {
         { 1, "%.0f ", NULL },                             // kibibytes
#ifdef BOOST_MEMORY
         { 1024.0, "%#.3f ", NULL },                       // mebibytes
         { 1024.0*1024, "%#.3f ", NULL },                  // gibibytes
         { 1024.0*1024*1024, "%#.3f ", NULL },             // tebibytes
         { 1024.0*1024*1024*1024, "%#.3f ", NULL },        // pebibytes
         { 1024.0*1024*1024*1024*1024, "%#.3f ", NULL }    // exbibytes
#else
         { 1024.0, "%#.1f ", NULL },                       // mebibytes
         { 1024.0*1024, "%#.1f ", NULL },                  // gibibytes
         { 1024.0*1024*1024, "%#.1f ", NULL },             // tebibytes
         { 1024.0*1024*1024*1024, "%#.1f ", NULL },        // pebibytes
         { 1024.0*1024*1024*1024*1024, "%#.1f ", NULL }    // exbibytes
#endif
      };
      struct { //                                            0123456789
      // snprintf contents of each buf (after SK_Kb):       'nnnn.nnn 0'
      // and prT macro might replace space at buf[8] with:   ------> +
         char buf[10]; // MEMORY_lines_fmt provides for 8+1 bytes
      } buftab[8];

      if (!scaletab[0].label) {
         scaletab[0].label = N_txt(AMT_kilobyte_txt);
         scaletab[1].label = N_txt(AMT_megabyte_txt);
         scaletab[2].label = N_txt(AMT_gigabyte_txt);
         scaletab[3].label = N_txt(AMT_terabyte_txt);
         scaletab[4].label = N_txt(AMT_petabyte_txt);
         scaletab[5].label = N_txt(AMT_exxabyte_txt);
      }

      if (w->rc.graph_mems) {
         static const struct {
            const char *used, *misc, *swap, *type;
         } gtab[] = {
            { "%-.*s~7", "%-.*s~8", "%-.*s~8", Graph_bars },
            { "%-.*s~4", "%-.*s~6", "%-.*s~6", Graph_blks }
         };
         char used[SMLBUFSIZ], util[SMLBUFSIZ], dual[MEDBUFSIZ];
         float pct_used, pct_misc, pct_swap;
         int ix, num_used, num_misc;

         pct_used = (float)kb_main_used * (100.0 / (float)kb_main_total);
#ifdef MEMGRAPH_OLD
         pct_misc = (float)(kb_main_buffers + kb_main_cached) * (100.0 / (float)kb_main_total);
#else
         pct_misc = (float)(kb_main_total - kb_main_available - kb_main_used) * (100.0 / (float)kb_main_total);
#endif
         if (pct_used + pct_misc > 100.0 || pct_misc < 0) pct_misc = 0;
         pct_swap = kb_swap_total ? (float)kb_swap_used * (100.0 / (float)kb_swap_total) : 0;
         ix = w->rc.graph_mems - 1;
#ifndef QUICK_GRAPHS
         num_used = (int)((pct_used * Graph_adj) + .5),
         num_misc = (int)((pct_misc * Graph_adj) + .5);
         if (num_used + num_misc > Graph_len) num_misc = Graph_len - num_used;
         snprintf(used, sizeof(used), gtab[ix].used, num_used, gtab[ix].type);
         snprintf(util, sizeof(util), gtab[ix].misc, num_misc, gtab[ix].type);
#else
         (void)num_used; (void)num_misc;
         snprintf(used, sizeof(used), gtab[ix].used, (int)((pct_used * Graph_adj) + .5), gtab[ix].type);
         snprintf(util, sizeof(util), gtab[ix].misc, (int)((pct_misc * Graph_adj) + .4), gtab[ix].type);
#endif
         snprintf(dual, sizeof(dual), "%s%s", used, util);
         snprintf(util, sizeof(util), gtab[ix].swap, (int)((pct_swap * Graph_adj) + .5), gtab[ix].type);
         prT(bfT(0), mkM(total)); prT(bfT(1), mkS(total));
         show_special(0, fmtmk( "%s %s:~3%#5.1f~2/%-9.9s~3[~1%-*s]~1\n%s %s:~3%#5.1f~2/%-9.9s~3[~1%-*s]~1\n"
            , scT(label), N_txt(WORD_abv_mem_txt), pct_used + pct_misc, bfT(0), Graph_len +4, dual
            , scT(label), N_txt(WORD_abv_swp_txt), pct_swap, bfT(1), Graph_len +2, util));
      } else {
         unsigned long kb_main_my_misc = kb_main_buffers + kb_main_cached;
         prT(bfT(0), mkM(total)); prT(bfT(1), mkM(free));
         prT(bfT(2), mkM(used));  prT(bfT(3), mkM(my_misc));
         prT(bfT(4), mkS(total)); prT(bfT(5), mkS(free));
         prT(bfT(6), mkS(used));  prT(bfT(7), mkM(available));
         show_special(0, fmtmk(N_unq(MEMORY_lines_fmt)
            , scT(label), N_txt(WORD_abv_mem_txt), bfT(0), bfT(1), bfT(2), bfT(3)
            , scT(label), N_txt(WORD_abv_swp_txt), bfT(4), bfT(5), bfT(6), bfT(7)
            , N_txt(WORD_abv_mem_txt)));
      }
      Msg_row += 2;
    #undef bfT
    #undef scT
    #undef mkM
    #undef mkS
    #undef prT
   } // end: View_MEMORY

 #undef isROOM
 #undef anyFLG
} // end: summary_show


        /*
         * Build the information for a single task row and
         * display the results or return them to the caller. */
static const char *task_show (const WIN_t *q, const int idx) {
#ifndef SCROLLVAR_NO
 #define makeVAR(v)  { const char *pv = v; \
    if (!q->varcolbeg) cp = make_str(pv, q->varcolsz, Js, AUTOX_NO); \
    else cp = make_str(q->varcolbeg < (int)strlen(pv) ? pv + q->varcolbeg : "", q->varcolsz, Js, AUTOX_NO); }
 #define varUTF8(v)  { const char *pv = v; \
    if (!q->varcolbeg) cp = make_str_utf8(pv, q->varcolsz, Js, AUTOX_NO); \
    else cp = make_str_utf8((q->varcolbeg < ((int)strlen(pv) - utf8_delta(pv))) \
    ? pv + utf8_embody(pv, q->varcolbeg) : "", q->varcolsz, Js, AUTOX_NO); }
#else
 #define makeVAR(v) cp = make_str(v, q->varcolsz, Js, AUTOX_NO)
 #define varUTF8(v) cp = make_str_utf8(v, q->varcolsz, Js, AUTOX_NO)
#endif
 #define pages2K(n)  (unsigned long)( (n) << Pg2K_shft )
   static char rbuf[ROWMINSIZ];
   char *rp;
   int x;
   proc_t *p = q->ppt[idx];

   /* we're borrowing some pad bytes in the proc_t,
      pad_2: 'x' means a collapsed thread, 'z' means an unseen child
      pad_3: where level number is stored (0 - 100) */
#ifndef TREE_VWINALL
   if (q == Curwin)            // note: the following is NOT indented
#endif
   if (CHKw(q, Show_FOREST) && p->pad_2 == 'z')
      return "";

   // we must begin a row with a possible window number in mind...
   *(rp = rbuf) = '\0';
   if (Rc.mode_altscr) rp = scat(rp, " ");

   for (x = 0; x < q->maxpflgs; x++) {
      const char *cp = NULL;
      FLG_t       i = q->procflgs[x];
      #define S   Fieldstab[i].scale        // these used to be variables
      #define W   Fieldstab[i].width        // but it's much better if we
      #define Js  CHKw(q, Show_JRSTRS)      // represent them as #defines
      #define Jn  CHKw(q, Show_JRNUMS)      // and only exec code if used

      switch (i) {
#ifndef USE_X_COLHDR
         // these 2 aren't real procflgs, they're used in column highlighting!
         case EU_XON:
         case EU_XOF:
            cp = NULL;
            if (!CHKw(q, NOPRINT_xxx | NOHIFND_xxx | NOHISEL_xxx)) {
               /* treat running tasks specially - entire row may get highlighted
                  so we needn't turn it on and we MUST NOT turn it off */
               if (!('R' == p->state && CHKw(q, Show_HIROWS)))
                  cp = (EU_XON == i ? q->capclr_rowhigh : q->capclr_rownorm);
            }
            break;
#endif
         case EU_CGN:
            makeVAR(p->cgname);
            break;
         case EU_CGR:
            makeVAR(p->cgroup[0]);
            break;
         case EU_CMD:
            makeVAR(forest_display(q, p));
            break;
         case EU_COD:
            cp = scale_mem(S, pages2K(p->trs), W, Jn);
            break;
         case EU_CPN:
            cp = make_num(p->processor, W, Jn, AUTOX_NO, 0);
            break;
         case EU_CPU:
         {  float u = (float)p->pcpu;
#ifndef TREE_VCPUOFF
 #ifndef TREE_VWINALL
            if (q == Curwin)   // note: the following is NOT indented
 #endif
            if (CHKw(q, Show_FOREST)) u += Hide_cpu[idx];
            u *= Frame_etscale;
            if (p->pad_2 != 'x' && u > 100.0 * p->nlwp) u = 100.0 * p->nlwp;
#else
            u *= Frame_etscale;
            /* process can't use more %cpu than number of threads it has
             ( thanks Jaromir Capik <jcapik@redhat.com> ) */
            if (u > 100.0 * p->nlwp) u = 100.0 * p->nlwp;
#endif
            if (u > Cpu_pmax) u = Cpu_pmax;
            cp = scale_pcnt(u, W, Jn);
         }
            break;
         case EU_DAT:
            cp = scale_mem(S, pages2K(p->drs), W, Jn);
            break;
         case EU_DRT:
            cp = scale_num(p->dt, W, Jn);
            break;
         case EU_ENV:
            makeVAR(p->environ[0]);
            break;
         case EU_FL1:
            cp = scale_num(p->maj_flt, W, Jn);
            break;
         case EU_FL2:
            cp = scale_num(p->min_flt, W, Jn);
            break;
         case EU_FLG:
            cp = make_str(hex_make(p->flags, 1), W, Js, AUTOX_NO);
            break;
         case EU_FV1:
            cp = scale_num(p->maj_delta, W, Jn);
            break;
         case EU_FV2:
            cp = scale_num(p->min_delta, W, Jn);
            break;
         case EU_GID:
            cp = make_num(p->egid, W, Jn, EU_GID, 0);
            break;
         case EU_GRP:
            cp = make_str_utf8(p->egroup, W, Js, EU_GRP);
            break;
         case EU_LXC:
            cp = make_str(p->lxcname, W, Js, EU_LXC);
            break;
         case EU_MEM:
            cp = scale_pcnt((float)pages2K(p->resident) * 100 / kb_main_total, W, Jn);
            break;
         case EU_NCE:
            cp = make_num(p->nice, W, Jn, AUTOX_NO, 1);
            break;
         case EU_NMA:
            cp = make_num(numa_node_of_cpu(p->processor), W, Jn, AUTOX_NO, 0);
            break;
         case EU_NS1:  // IPCNS
         case EU_NS2:  // MNTNS
         case EU_NS3:  // NETNS
         case EU_NS4:  // PIDNS
         case EU_NS5:  // USERNS
         case EU_NS6:  // UTSNS
         {  long ino = p->ns[i - EU_NS1];
            cp = make_num(ino, W, Jn, i, 1);
         }
            break;
         case EU_OOA:
            cp = make_num(p->oom_adj, W, Jn, AUTOX_NO, 1);
            break;
         case EU_OOM:
            cp = make_num(p->oom_score, W, Jn, AUTOX_NO, 1);
            break;
         case EU_PGD:
            cp = make_num(p->pgrp, W, Jn, AUTOX_NO, 0);
            break;
         case EU_PID:
            cp = make_num(p->tid, W, Jn, AUTOX_NO, 0);
            break;
         case EU_PPD:
            cp = make_num(p->ppid, W, Jn, AUTOX_NO, 0);
            break;
         case EU_PRI:
            if (-99 > p->priority || 999 < p->priority) {
               cp = make_str("rt", W, Jn, AUTOX_NO);
            } else
               cp = make_num(p->priority, W, Jn, AUTOX_NO, 0);
            break;
         case EU_RES:
            cp = scale_mem(S, pages2K(p->resident), W, Jn);
            break;
         case EU_RZA:
            cp = scale_mem(S, p->vm_rss_anon, W, Jn);
            break;
         case EU_RZF:
            cp = scale_mem(S, p->vm_rss_file, W, Jn);
            break;
         case EU_RZL:
            cp = scale_mem(S, p->vm_lock, W, Jn);
            break;
         case EU_RZS:
            cp = scale_mem(S, p->vm_rss_shared, W, Jn);
            break;
         case EU_SGD:
            makeVAR(p->supgid);
            break;
         case EU_SGN:
            varUTF8(p->supgrp);
            break;
         case EU_SHR:
            cp = scale_mem(S, pages2K(p->share), W, Jn);
            break;
         case EU_SID:
            cp = make_num(p->session, W, Jn, AUTOX_NO, 0);
            break;
         case EU_STA:
            cp = make_chr(p->state, W, Js);
            break;
         case EU_SWP:
            cp = scale_mem(S, p->vm_swap, W, Jn);
            break;
         case EU_TGD:
            cp = make_num(p->tgid, W, Jn, AUTOX_NO, 0);
            break;
         case EU_THD:
            cp = make_num(p->nlwp, W, Jn, AUTOX_NO, 0);
            break;
         case EU_TM2:
         case EU_TME:
         {  TIC_t t = p->utime + p->stime;
            if (CHKw(q, Show_CTIMES)) t += (p->cutime + p->cstime);
            cp = scale_tics(t, W, Jn);
         }
            break;
         case EU_TPG:
            cp = make_num(p->tpgid, W, Jn, AUTOX_NO, 0);
            break;
         case EU_TTY:
         {  char tmp[SMLBUFSIZ];
            dev_to_tty(tmp, W, p->tty, p->tid, ABBREV_DEV);
            cp = make_str(tmp, W, Js, EU_TTY);
         }
            break;
         case EU_UED:
            cp = make_num(p->euid, W, Jn, EU_UED, 0);
            break;
         case EU_UEN:
            cp = make_str_utf8(p->euser, W, Js, EU_UEN);
            break;
         case EU_URD:
            cp = make_num(p->ruid, W, Jn, EU_URD, 0);
            break;
         case EU_URN:
            cp = make_str_utf8(p->ruser, W, Js, EU_URN);
            break;
         case EU_USD:
            cp = make_num(p->suid, W, Jn, EU_USD, 0);
            break;
         case EU_USE:
            cp = scale_mem(S, (p->vm_swap + p->vm_rss), W, Jn);
            break;
         case EU_USN:
            cp = make_str_utf8(p->suser, W, Js, EU_USN);
            break;
         case EU_VRT:
            cp = scale_mem(S, pages2K(p->size), W, Jn);
            break;
         case EU_WCH:
            cp = make_str(lookup_wchan(p->tid), W, Js, EU_WCH);
            break;
         default:                 // keep gcc happy
            continue;

      } // end: switch 'procflag'

      if (cp) {
         if (q->osel_tot && !osel_matched(q, i, cp)) return "";
         rp = scat(rp, cp);
      }
      #undef S
      #undef W
      #undef Js
      #undef Jn
   } // end: for 'maxpflgs'

   if (!CHKw(q, NOPRINT_xxx)) {
      const char *cap = ((CHKw(q, Show_HIROWS) && 'R' == p->state))
         ? q->capclr_rowhigh : q->capclr_rownorm;
      char *row = rbuf;
      int ofs;
      /* since we can't predict what the search string will be and,
         considering what a single space search request would do to
         potential buffer needs, when any matches are found we skip
         normal output routing and send all of the results directly
         to the terminal (and we sound asthmatic: poof, putt, puff) */
      if (-1 < (ofs = find_ofs(q, row))) {
         POOF("\n", cap);
         do {
            row[ofs] = '\0';
            PUTT("%s%s%s%s", row, q->capclr_hdr, q->findstr, cap);
            row += (ofs + q->findlen);
            ofs = find_ofs(q, row);
         } while (-1 < ofs);
         PUTT("%s%s", row, Caps_endline);
         // with a corrupted rbuf, ensure row is 'counted' by window_show
         rbuf[0] = '!';
      } else
         PUFF("\n%s%s%s", cap, row, Caps_endline);
   }
   return rbuf;
 #undef makeVAR
 #undef varUTF8
 #undef pages2K
} // end: task_show


        /*
         * A window_show *Helper* function ensuring that Curwin's 'begtask'
         * represents a visible process (not any hidden/filtered-out task).
         * In reality, this function is called:
         *   1) exclusively for the 'current' window
         *   2) immediately after interacting with the user
         *   3) who struck: up, down, pgup, pgdn, home, end, 'o/O' or 'u/U'
         *   4) or upon the user switching from one window to another window */
static void window_hlp (void) {
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy
   int i, reversed;

   SETw(w, NOPRINT_xxx);
   w->begtask += w->begnext;
   // next 'if' will force a forward scan ...
   if (w->begtask <= 0) { w->begtask = 0; w->begnext = +1; }
   else if (w->begtask >= Frame_maxtask) w->begtask = Frame_maxtask - 1;

   reversed = 0;
   // potentially scroll forward ...
   if (w->begnext > 0) {
fwd_redux:
      for (i = w->begtask; i < Frame_maxtask; i++) {
         if (wins_usrselect(w, i)
         && (*task_show(w, i)))
            break;
      }
      if (i < Frame_maxtask) {
         w->begtask = i;
         goto wrap_up;
      }
      // no luck forward, so let's try backward
      w->begtask = Frame_maxtask - 1;
   }

   // potentially scroll backward ...
   for (i = w->begtask; i > 0; i--) {
      if (wins_usrselect(w, i)
      && (*task_show(w, i)))
         break;
   }
   // reached the top, but maybe this guy ain't visible
   if (!(w->begtask = i) && !reversed) {
      if (!(wins_usrselect(w, 0))
      || (!(*task_show(w, 0)))) {
         reversed = 1;
         goto fwd_redux;
      }
   }

wrap_up:
   w->begnext = 0;
   OFFw(w, NOPRINT_xxx);
} // end: window_hlp


        /*
         * Squeeze as many tasks as we can into a single window,
         * after sorting the passed proc table. */
static int window_show (WIN_t *q, int wmax) {
 /* the isBUSY macro determines if a task is 'active' --
    it returns true if some cpu was used since the last sample.
    ( actual 'running' tasks will be a subset of those selected ) */
 #define isBUSY(x)   (0 < (x)->pcpu)
 #define winMIN(a,b) (((a) < (b)) ? (a) : (b))
   int i, lwin;

   // Display Column Headings -- and distract 'em while we sort (maybe)
   PUFF("\n%s%s%s", q->capclr_hdr, q->columnhdr, Caps_endline);

   if (CHKw(q, Show_FOREST))
      forest_create(q);
   else {
      if (CHKw(q, Qsrt_NORMAL)) Frame_srtflg = 1;   // this is always needed!
      else Frame_srtflg = -1;
      Frame_ctimes = CHKw(q, Show_CTIMES);          // this & next, only maybe
      Frame_cmdlin = CHKw(q, Show_CMDLIN);
      qsort(q->ppt, Frame_maxtask, sizeof(proc_t *), Fieldstab[q->rc.sortindx].sort);
   }

   if (q->begnext) window_hlp();
   else OFFw(q, NOPRINT_xxx);

   i = q->begtask;
   lwin = 1;                                        // 1 for the column header
   wmax = winMIN(wmax, q->winlines + 1);            // ditto for winlines, too

   /* the least likely scenario is also the most costly, so we'll try to avoid
      checking some stuff with each iteration and check it just once... */
   if (CHKw(q, Show_IDLEPS) && !q->usrseltyp)
      while (i < Frame_maxtask && lwin < wmax) {
         if (*task_show(q, i++))
            ++lwin;
      }
   else
      while (i < Frame_maxtask && lwin < wmax) {
         if ((CHKw(q, Show_IDLEPS) || isBUSY(q->ppt[i]))
         && wins_usrselect(q, i)
         && *task_show(q, i))
            ++lwin;
         ++i;
      }

   return lwin;
 #undef isBUSY
 #undef winMIN
} // end: window_show

/*######  Entry point plus two  ##########################################*/

        /*
         * This guy's just a *Helper* function who apportions the
         * remaining amount of screen real estate under multiple windows */
static void frame_hlp (int wix, int max) {
   int i, size, wins;

   // calc remaining number of visible windows
   for (i = wix, wins = 0; i < GROUPSMAX; i++)
      if (CHKw(&Winstk[i], Show_TASKON))
         ++wins;

   if (!wins) wins = 1;
   // deduct 1 line/window for the columns heading
   size = (max - wins) / wins;

   /* for subject window, set WIN_t winlines to either the user's
      maxtask (1st choice) or our 'foxized' size calculation
      (foxized  adj. -  'fair and balanced') */
   Winstk[wix].winlines =
      Winstk[wix].rc.maxtasks ? Winstk[wix].rc.maxtasks : size;
} // end: frame_hlp


        /*
         * Initiate the Frame Display Update cycle at someone's whim!
         * This routine doesn't do much, mostly he just calls others.
         *
         * (Whoa, wait a minute, we DO caretake those row guys, plus)
         * (we CALCULATE that IMPORTANT Max_lines thingy so that the)
         * (*subordinate* functions invoked know WHEN the user's had)
         * (ENOUGH already.  And at Frame End, it SHOULD be apparent)
         * (WE am d'MAN -- clearing UNUSED screen LINES and ensuring)
         * (that those auto-sized columns are addressed, know what I)
         * (mean?  Huh, "doesn't DO MUCH"!  Never, EVER think or say)
         * (THAT about THIS function again, Ok?  Good that's better.)
         *
         * (ps. we ARE the UNEQUALED justification KING of COMMENTS!)
         * (No, I don't mean significance/relevance, only alignment.)
         */
static void frame_make (void) {
   WIN_t *w = Curwin;             // avoid gcc bloat with a local copy
   int i, scrlins;

   // deal with potential signal(s) since the last time around...
   if (Frames_signal)
      zap_fieldstab();

   // whoa either first time or thread/task mode change, (re)prime the pump...
   if (Pseudo_row == PROC_XTRA) {
      cpus_refresh();
      procs_refresh();
      usleep(LIB_USLEEP);
      putp(Cap_clr_scr);
   } else
      putp(Batch ? "\n\n" : Cap_home);

   sysinfo_refresh(0);
   procs_refresh();

   Tree_idx = Pseudo_row = Msg_row = scrlins = 0;
   summary_show();
   Max_lines = (Screen_rows - Msg_row) - 1;

   // we're now on Msg_row so clear out any residual messages ...
   putp(Cap_clr_eol);

   if (!Rc.mode_altscr) {
      // only 1 window to show so, piece o' cake
      w->winlines = w->rc.maxtasks ? w->rc.maxtasks : Max_lines;
      scrlins = window_show(w, Max_lines);
   } else {
      // maybe NO window is visible but assume, pieces o' cakes
      for (i = 0 ; i < GROUPSMAX; i++) {
         if (CHKw(&Winstk[i], Show_TASKON)) {
            frame_hlp(i, Max_lines - scrlins);
            scrlins += window_show(&Winstk[i], Max_lines - scrlins);
         }
         if (Max_lines <= scrlins) break;
      }
   }

   /* clear to end-of-screen - critical if last window is 'idleps off'
      (main loop must iterate such that we're always called before sleep) */
   if (scrlins < Max_lines) {
      putp(Cap_nl_clreos);
      PSU_CLREOS(Pseudo_row);
   }

   if (CHKw(w, View_SCROLL) && VIZISw(Curwin))
      show_scroll();
   fflush(stdout);

   /* we'll deem any terminal not supporting tgoto as dumb and disable
      the normal non-interactive output optimization... */
   if (!Cap_can_goto) PSU_CLREOS(0);

   /* lastly, check auto-sized width needs for the next iteration */
   if (AUTOX_MODE && Autox_found)
      widths_resize();
} // end: frame_make


        /*
         * duh... */
int main (int dont_care_argc, char **argv) {
   (void)dont_care_argc;
   before(*argv);
                                        //                 +-------------+
   wins_stage_1();                      //                 top (sic) slice
   configs_reads();                     //                 > spread etc, <
   parse_args(&argv[1]);                //                 > lean stuff, <
   whack_terminal();                    //                 > onions etc. <
   wins_stage_2();                      //                 as bottom slice
                                        //                 +-------------+

   for (;;) {
      struct timespec ts;

      frame_make();

      if (0 < Loops) --Loops;
      if (!Loops) bye_bye(NULL);

      ts.tv_sec = Rc.delay_time;
      ts.tv_nsec = (Rc.delay_time - (int)Rc.delay_time) * 1000000000;

      if (Batch)
         pselect(0, NULL, NULL, NULL, &ts, NULL);
      else {
         if (ioa(&ts))
            do_key(iokey(1));
      }
           /* note: that above ioa routine exists to consolidate all logic
                    which is susceptible to signal interrupt and must then
                    produce a screen refresh. in this main loop frame_make
                    assumes responsibility for such refreshes. other logic
                    in contact with users must deal more obliquely with an
                    interrupt/refresh (hint: Frames_signal + return code)!

                    (everything is perfectly justified plus right margins)
                    (are completely filled, but of course it must be luck)
            */
   }
   return 0;
} // end: main
