/* top.h - Header file:         show Linux processes */
/*
 * Copyright (c) 2002 - James C. Warner, <warnerjc@worldnet.att.net>
 *    All rights reserved.
 * This file may be used subject to the terms and conditions of the
 * GNU Library General Public License Version 2, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 */
#ifndef _Itop
#define _Itop

        /* Determines for whom we're destined, debian or !#@*#! redhat ----- */
//#define UGH_ITS_4_RH            /* use the redhat libproc conventions      */

        /* Defines intended to be played with ------------------------------ */
//#define UPCASE_HEXES            /* show any hex values in upper case       */
//#define UPCASE_SCALE            /* show scaled times & memory upper case   */
//#define UPCASE_SUMMK            /* show memory summary kilobytes with 'K'  */

//#define COLOR_SLEEPY            /* purples and blues (sissy colors)        */
//#define COLOR_CONSOL            /* for linux consoles (white on black)     */

        /* Development/Debugging defines ----------------------------------- */
//#define TRAK_MAXCAPS            /* track & report misc terminfo stuff      */
//#define TRAK_MAXBUFS            /* track & report internal buffer usage    */

        /* Special define to test extremely dumb terminals! ---------------- */
//#define PRETENDNOCAP            /* use a terminal without essential caps   */


/*######  Some Typedef's and Enum's  #####################################*/

        /* This typedef attempts to ensure consistent 'ticks' handling. */
typedef unsigned long TICS_t;

        /* This structure consolidates the information that's used
           in a variety of display roles. */
typedef struct {
   const char *head;    /* name for column headings + toggle/reorder fields */
   const char *fmts;    /* sprintf format string for field display */
   const int   width;   /* field width, if applicable */
   const int   scale;   /* scale_num type, if applicable */
   const int   sort;    /* sort type, if applicable (used soley by mkcol) */
   const char *desc;    /* description for toggle/reorder fields */
} FTAB_t;

        /* This structure stores one piece of critical 'history'
           information from one frame to the next -- we don't calc
           and save data that goes unused like the old top! */
typedef struct {
   int    pid;
   TICS_t tics;
} HIST_t;

        /* This structure serves single (non-smp), multiple (smp) and
           consolidated (smp, but Show_cpusum == 1) environments.  It is
           used to calculate a frame's cpu(s) state percentages */
typedef struct {
   TICS_t u,    /* ticks count as represented in /proc/stat */
          n,    /* (not in the order of our display) */
          s,
          i;
} CPUS_t;

        /* Sorted columns support. */
typedef int (*QSORT_t)(const void *, const void *);
enum sort {
   S_CMD = 'C', S_MEM = 'M', S_TME = 'T', S_PID = 'P', S_TTY = 'Y',
   S_CPU = 'U', S_USR = 'E'
};

        /* The scaling 'type' used with scale_num() -- this is how
           the passed number is interpreted should scaling be necessary */
enum scale_num {
   SK_no, SK_Kb, SK_Mb, SK_Gb
};

        /* Flags for each possible field.
           At the moment 32 are supported [ see PFlags/PFLAGSSIZ ] */
enum pflag {
   P_PID, P_PPID, P_UID, P_USER, P_GROUP, P_TTY,
   P_PR, P_NI,
   P_NCPU, P_CPU, P_TIME, P_TIME2,
   P_MEM, P_VIRT, P_SWAP, P_RES, P_CODE, P_DATA, P_SHR,
   P_FAULT, P_DIRTY,
   P_STA, P_CMD, P_WCHAN, P_FLAGS
};


/*######  Some Miscellaneous Macro definitions  ##########################*/

        /* Message line placement at runtime */
#define MSG_line  ( HSum_lines - 2 )

        /* Used as return argument to achieve normal/reversed sorts
           in the sort callbacks */
#define SORT_lt  ( Sort_normal ?  1 : -1 )
#define SORT_gt  ( Sort_normal ? -1 :  1 )

        /* Convert some proc stuff into vaules we can actually use */
#define BYTES_2K(n)  (unsigned)( (n) >> 10 )
#define PAGES_2B(n)  (unsigned)( (n) * Page_size )
#define PAGES_2K(n)  BYTES_2K(PAGES_2B(n))
#define PAGE_CNT(n)  (unsigned)( (n) / Page_size )

        /* Yield table size as 'int' */
#define MAXtbl(t)  ( (int)(sizeof(t)/sizeof(t[0])) )


/*######  Special Macros (debug and/or informative)  #####################*/

        /* Orderly end, with any sort of message - see fmtmk */
#define debug_END(s)  { \
           static void std_err (const char *); \
           fputs(Cap_clr_scr, stdout); \
           std_err(s); \
        }

#ifdef TRAK_MAXBUFS
        /* Provide for tracking maximum buffer sizes */
#define ARPTBUF(f,b)  "\n\t%4d  of  %d\t" #f ", " #b
#define ASIZBUF(f,b)  Siz_ ## f ## _ ## b
#define AUSEBUF(f,b)  Use_ ## f ## _ ## b
#define BUF2INT(f,b)  static int AUSEBUF(f,b) = 0, ASIZBUF(f,b) = 0;
#define MAXBUFS(f,b)  { \
           if ((int)strlen(b) > AUSEBUF(f,b)) \
              AUSEBUF(f,b) = (int)strlen(b); \
           ASIZBUF(f,b) = (int)sizeof(b); \
        }
#endif


/*######  Some Miscellaneous constants  ##################################*/

        /* The default value for the 'k' (kill) request */
#define DEF_SIGNAL  SIGTERM

        /* The default delay twix updates */
#define DEF_DELAY  2.0

        /* The length of time a 'message' is displayed */
#define MSG_SLEEP  1

        /* Minimum summary lines (thus, just msg line + col heads) */
#define SUMMINLINS  2

        /* Specific process id monitoring support (command line only) */
#define MONPIDMAX  12

        /* Miscellaneous buffer sizes with liberal values
           -- mostly just to pinpoint source code usage/dependancies */
#define PFLAGSSIZ    32
#define CAPBUFSIZ    32
#define CLRBUFSIZ    64
#define GETBUFSIZ    32
#define TNYBUFSIZ    32
#define SMLBUFSIZ   256
#define MEDBUFSIZ   512
#define OURPATHSZ  1024
#define BIGBUFSIZ  2048
#define RCFBUFSIZ  SMLBUFSIZ
#define USRNAMSIZ  GETBUFSIZ
#define COLBUFSIZ  SMLBUFSIZ + CLRBUFSIZ
#define ROWBUFSIZ  MEDBUFSIZ + CLRBUFSIZ

        /* Color stuff...
           note: we avoid the use of background color so as to maximize
                 compatibility with the user's xterm settings */
#ifdef COLOR_SLEEPY
#define BASEcolor  COLOR_MAGENTA
#define MSGScolor  COLOR_CYAN
#define HEADcolor  COLOR_BLUE
#else
#ifdef COLOR_CONSOL
#define BASEcolor  COLOR_CYAN
#define MSGScolor  COLOR_RED
#define HEADcolor  COLOR_WHITE
#else
#define BASEcolor  COLOR_RED
#define MSGScolor  COLOR_RED
#define HEADcolor  COLOR_YELLOW
#endif
#endif


/*######  Display Support *Data*  ########################################*/

        /* The default fields displayed and their order,
           if nothing is specified by the loser, oops user */
#define DEF_FIELDS  "AbcDefGHiJkLMNOPqrstuVWxy"
        /* Help screen selectable grouped fields */
#define JOB_FIELDS  "ABWdefikqrstuxyLJMGHVNOPC"
#define MEM_FIELDS  "AMNOPQRSTUWbcdefiklxyVGHJ"
#define USR_FIELDS  "CDEFABWghiknopqrstuxyLJMV"


        /* These are the possible fscanf formats used in /proc/stat
           reads during history processing. */
#define CPU_FMTS_MULTI  "cpu%*d %lu %lu %lu %lu\n"
#define CPU_FMTS_JUST1  "cpu %lu %lu %lu %lu\n"


        /* Summary Lines specially formatted string(s) --
           see 'show_special' for syntax details + other cautions. */
#define LOADAV_line   "%s -\03%s\n"
#define STATES_line1  "Tasks:\03" \
   " %3u \02total,\03 %3u \02running,\03 %3u \02sleeping,\03" \
   " %3u \02stopped,\03 %3u \02zombie\03\n"
#define STATES_line2  "%s\03" \
   " %#5.1f%% \02user,\03 %#5.1f%% \02system,\03" \
   " %#5.1f%% \02nice,\03 %#5.1f%% \02idle\03\n"
#ifdef UPCASE_SUMMK
#define MEMORY_line1  "Mem: \03" \
   " %8uK \02total,\03 %8uK \02used,\03" \
   " %8uK \02free,\03 %8uK \02buffers\03\n"
#define MEMORY_line2  "Swap:\03" \
   " %8uK \02total,\03 %8uK \02used,\03" \
   " %8uK \02free,\03 %8uK \02cached\03\n"
#else
#define MEMORY_line1  "Mem: \03" \
   " %8uk \02total,\03 %8uk \02used,\03" \
   " %8uk \02free,\03 %8uk \02buffers\03\n"
#define MEMORY_line2  "Swap:\03" \
   " %8uk \02total,\03 %8uk \02used,\03" \
   " %8uk \02free,\03 %8uk \02cached\03\n"
#endif


        /* Colors Help specially formatted string(s) --
           see 'show_special' for syntax details + other cautions. */
#define COLOR_sample \
   "%s%s's \01Help for color mapping\02 - %s\n" \
   "\n" \
   "   color -\03 08:41:24 up 1 day,  6:07,  7 users,  load average:\n" \
   "   Tasks:\03  64 \02total,\03   2 \02running,\03  62 \02sleeping,\03   0 \02stopped,\03\n" \
   "   State cpu0 :\03   76.5%% \02user,\03  11.2%% \02system,\03   0.0%% \02nice,\03\n" \
   "   \01 Nasty Message! \04  -or-  \01Input Prompt\05\n" \
   "   \01  PID    TTY  PR  NI %%CPU    TIME+   VIRT SWAP STA Command \06\n" \
   "   17284 \03 pts/2 \07  8   0  0.0   0:00.75  1380    0 S   /bin/bas\03\n" \
   "   \01 8601  pts/1   7 -10  0.4   0:00.03   916    0 R<  color -b\07\n" \
   "   11005 \03     ? \07  9   0  0.0   0:02.50  2852 1008 S   amor -se\03\n" \
   "    2924 \03     ? \07  9  -1  0.0   1:08.16 30040  20m S<  X :0\03\n" \
   "   (normal toggles available: \01b\02 = bold/reverse; \01z\02 = color/mono)\n" \
   "\n\n" \
   "Select \01target\02 as upper case letter:\n" \
   "   B\02 = Base color,\01  H\02 = Column Headings,\01  M\02 = Messages/Prompts\n" \
   "Select \01color\02 as number:\n" \
   "   0\02 = black,\01  1\02 = red,    \01  2\02 = green,\01  3\02 = yellow,\n" \
   "   4\02 = blue, \01  5\02 = magenta,\01  6\02 = cyan, \01  7\02 = white\n" \
   "\n" \
   "Selected: \01target\02 \01 %c \04; \01color\02 \01 %d \04\n" \
   "   press 'q' to abort or <Enter> to commit " \
   ""


        /* Keyboard Help specially formatted string(s) --
           see 'show_special' for syntax details + other cautions. */
#define HELP_data \
   "%s's \01Help for interactive commands\02 - %s\n" \
   "status:  \01Secure mode \03%s\02;\01 Cumulative mode \03%s\02; \01Delay time \03%.1f\02 seconds\n" \
   "   sp or ^L    Redraw screen\n" \
   "   o or O      Rearrange fields\n" \
   "   f or F      Add and remove fields, or select group now from:\n" \
   "                  \01j\02) job fields; \01m\02) memory fields; \01u\02) user fields\n" \
   "   Z           Change color mapping\05\n" \
   "\n" \
   "C,M,P,T,U,Y,E  Sort: \01C\02) cmd; \01M\02) mem; \01P\02) pid; \01T\02) time; \01U\02) cpu; \01Y\02) tty; \01E\02) user\n" \
   "   R           Toggle normal/reverse sort for any of above\n" \
   "   l,t,m       Toggle summary: \01l\02) load avg; \01t\02) task/cpu stats; \01m\02) mem info\n" \
   "   c,i,S       Toggle: \01c\02) cmd name/line; \01i\02) idle tasks; \01S\02) cumulative time\n" \
   "   x,y\05         Toggle highlights: \01x\02) sort field; \01y\02) running tasks\n" \
   "   z,b\05         Toggle: \01z\02) color/mono; \01b\02) bold/reverse if 'x' or 'y'\n" \
   "   1,I         SMP views:\01 1\02) single/separate states; \01I\02) Irix/Solaris mode\n" \
   "\n" \
   "%s" \
   "   u           Show specific user only\n" \
   "   # or n      Set maximum tasks displayed\n" \
   "   W           Write configuration file\n" \
   "   q           Quit\n" \
   "Press any key to continue " \
   ""

        /* This guy goes above the 'u' help text (maybe) */
#define HELP_unsecured \
   "   k           Kill a task\n" \
   "   r           Renice a task\n" \
   "   s or d      Set update interval\n" \
   ""


        /* Fields Reorder/Toggle specially formatted string(s) --
           see 'show_special' for syntax details + other cautions
           note: the leading newline below serves really dumb terminals;
                 if there's no 'cursor_home', the screen will be a mess
                 but this part will still be functional. */
#define FIELDS_current \
   "\n%s%s's\01 Current Fields\02: \01 %s \04\n%s " \
   ""

        /* Some extra explanatory text which accompanies the Fields display.
           note: the newlines cannot actually be used, they just serve as
                 substring delimiters for the 'display_fields' routine. */
#define FIELDS_xtra \
   "Flags field:\n" \
   "  0x00000001  PF_ALIGNWARN\n" \
   "  0x00000002  PF_STARTING\n" \
   "  0x00000004  PF_EXITING\n" \
   "  0x00000040  PF_FORKNOEXEC\n" \
   "  0x00000100  PF_SUPERPRIV\n" \
   "  0x00000200  PF_DUMPCORE\n" \
   "  0x00000400  PF_SIGNALED\n" \
   "  0x00000800  PF_MEMALLOC\n" \
   "  0x00040000  PF_KERNTHREAD (2.5)\n" \
   "  0x00100000  PF_USEDFPU (thru 2.4)\n" \
   "  0x00400000  PF_ATOMICALLOC\n" \
   "\n" \
   "Memory notes:\n" \
   "  VIRT = SWAP + RES\n" \
   "  RES  = CODE + DATA\n" \
   ""

/*######  Some Prototypes (ha!)  #########################################*/

   /* None of these are necessary when the source file is properly
    * organized -- they're here for documentation purposes !
    * Note also that functions are alphabetical within a group to aid
    * source code navigation, which often influences choice of identifers. */
/*------  Sort callbacks  ------------------------------------------------*/
//atic int         sort_cmd (proc_t **P, proc_t **Q);
//atic int         sort_cpu (proc_t **P, proc_t **Q);
//atic int         sort_mem (proc_t **P, proc_t **Q);
//atic int         sort_pid (proc_t **P, proc_t **Q);
//atic int         sort_tme (proc_t **P, proc_t **Q);
//atic int         sort_tty (proc_t **P, proc_t **Q);
//atic int         sort_usr (proc_t **P, proc_t **Q);
/*------  Tiny useful routine(s)  ----------------------------------------*/
//atic int         chin (int ech, char *buf, unsigned cnt);
//atic const char *fmtmk (const char *fmts, ...);
//atic char       *strim (int sp, char *str);
//atic char       *tg2 (int x, int y);
/*------  Misc Color/Highlighting support  -------------------------------*/
//atic void        capsmk (void);
//atic void        msg_save (const char *fmts, ...);
//atic void        show_msg (const char *str);
//atic void        show_pmt (const char *str);
//atic void        show_special (const char *glob);
//atic void        tweak_colors (void);
/*------  Small utility routines  ----------------------------------------*/
//atic char       *ask_str (const char *prompt);
//atic float       get_float (const char *prompt);
//atic int         get_int (const char *prompt);
//atic void        mkheadings (void);
//atic char       *scale_num (unsigned num, const unsigned width, const unsigned type);
//atic char       *scale_tics (TICS_t tics, const unsigned width);
//atic float       time_elapsed (void);
/*------  Exit and Signal handled routines  ------------------------------*/
//atic void        bye_bye (int eno, const char *str);
//atic void        std_err (const char *str);
//atic void        stop (int dont_care_sig);
//atic void        suspend (int dont_care_sig);
//atic void        window_resize (int dont_care_sig);
/*------  Startup routines  ----------------------------------------------*/
//atic void        parse_argvs (char **argv);
//atic void        parse_rc (char *opts);
//atic void        rcfiles_read (void);
//atic void        terminal_set (void);
/*------  Field Selection/Ordering routines  -----------------------------*/
//atic void        display_fields (void);
//atic void        fields_reorder (void);
//atic void        fields_toggle (void);
/*------  Library Alternatives  ------------------------------------------*/
//atic void       *alloc_c (unsigned numb);
//atic void       *alloc_r (void *q, unsigned numb);
//atic proc_t    **readprocs (proc_t **tbl);
/*------  Main screen routines  ------------------------------------------*/
//atic void        do_key (unsigned c);
#ifdef UGH_ITS_4_RH
//atic unsigned    frame_memory (void);
#else
//atic void        frame_memory (void);
#endif
//atic void        frame_smp (FILE *fp, const char *fmt, CPUS_t *cpu, const char *pfx);
//atic void        frame_states (proc_t **p, int show);
//atic void        mkcol (unsigned idx, int sta, int *pad, char *buf, ...);
#ifdef UGH_ITS_4_RH
//atic void        show_a_task (proc_t *task, unsigned mempgs);
#else
//atic void        show_a_task (proc_t *task);
#endif
//atic void        show_everything (void);
/*------  Entry point  ---------------------------------------------------*/
//     int         main (int dont_care_argc, char **argv);


        /* just sanity check(s)... */
#if USRNAMSIZ < GETBUFSIZ
 #error "Jeeze, USRNAMSIZ Must NOT be less than GETBUFSIZ !"
#endif

#endif /* _Itop */

