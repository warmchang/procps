/* top.h - Header file:         show Linux processes */
/*
 * Copyright (c) 2002, by:      James C. Warner
 *    All rights reserved.      8921 Hilloway Road
 *                              Eden Prairie, Minnesota 55347 USA
 *                             <warnerjc@worldnet.att.net>
 *
 * This file may be used subject to the terms and conditions of the
 * GNU Library General Public License Version 2, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 */
/* For their contributions to this program, the author wishes to thank:
 *    Craig Small, <csmall@small.dropbear.id.au>
 *    Albert D. Cahalan, <albert@users.sf.net>
 */
#ifndef _Itop
#define _Itop

        /* Determines for whom we're destined ------------------------------ */
//#define UGH_ITS_4_RH            /* use the redhat libproc conventions      */

        /* Defines intended to be experimented with ------------------------ */
//#define CASEUP_HEXES            /* show any hex values in upper case       */
//#define CASEUP_SCALE            /* show scaled time/num suffix upper case  */
//#define CASEUP_SUMMK            /* show memory summary kilobytes with 'K'  */
//#define POSIX_CMDLIN            /* use '[ ]' for kernel threads, not '( )' */
//#define SORT_SUPRESS            /* *attempt* to reduce qsort overhead      */
//#define USE_LIB_STA3            /* use lib status (3 ch) vs. proc_t (1 ch) */
//#define WARN_NOT_SMP            /* restrict '1' & 'I' commands to true smp */

        /* Development/Debugging defines ----------------------------------- */
//#define ATEOJ_REPORT            /* report a bunch of stuff, at end-of-job  */
//#define PRETEND2_5_X            /* pretend we're linux 2.5.x (for IO-wait) */
//#define PRETEND4CPUS            /* pretend we're smp with 4 ticsers (sic)  */
//#define PRETENDNOCAP            /* use a terminal without essential caps   */
//#define YIELDCPU_OFF            /* hang on tight, DON'T issue sched_yield  */

#ifdef PRETEND2_5_X
#define linux_version_code LINUX_VERSION(2,5,43)
#endif

/*######  Some Miscellaneous constants  ##################################*/

        /* The default delay twix updates */
#define DEF_DELAY  3.0

        /* The length of time a 'message' is displayed */
#define MSG_SLEEP  2

        /* The default value for the 'k' (kill) request */
#define DEF_SIGNAL  SIGTERM

        /* Specific process id monitoring support (command line only) */
#define MONPIDMAX  20

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
#define STATBUFSZ  1024
#define BIGBUFSIZ  2048
#define RCFBUFSIZ  SMLBUFSIZ
#define USRNAMSIZ  GETBUFSIZ
#define COLBUFSIZ  SMLBUFSIZ + CLRBUFSIZ
#define ROWBUFSIZ  MEDBUFSIZ + CLRBUFSIZ


/*######  Some Miscellaneous Macro definitions  ##########################*/

        /* Yield table size as 'int' */
#define MAXTBL(t)  (int)(sizeof(t) / sizeof(t[0]))

        /* Convert some proc stuff into vaules we can actually use */
#define BYTES_2K(n)  (unsigned)( (n) >> 10 )
#define PAGES_2B(n)  (unsigned)( (n) * Page_size )
#define PAGES_2K(n)  BYTES_2K(PAGES_2B(n))
#define PAGE_CNT(n)  (unsigned)( (n) / Page_size )

        /* Used as return arguments in *some* of the sort callbacks */
#define SORT_lt  ( Frame_srtflg > 0 ?  1 : -1 )
#define SORT_gt  ( Frame_srtflg > 0 ? -1 :  1 )
#define SORT_eq  0

        /* Used to reference and create sort callback functions --
           note: some of the callbacks are NOT your father's callbacks, they're
                 highly optimized to save them ol' precious cycles! */
#define _SF(f)  (QSORT_t)sort_ ## f
#define _SC_NUM1(f,n) \
   static int sort_ ## f (const proc_t **P, const proc_t **Q) { \
      if ( (*P)->n < (*Q)->n ) return SORT_lt; \
      if ( (*P)->n > (*Q)->n ) return SORT_gt; \
      return SORT_eq; }
#define _SC_NUM2(f,n1,n2) \
   static int sort_ ## f (const proc_t **P, const proc_t **Q) { \
      if ( ((*P)->n1 - (*P)->n2) < ((*Q)->n1 - (*Q)->n2) ) return SORT_lt; \
      if ( ((*P)->n1 - (*P)->n2) > ((*Q)->n1 - (*Q)->n2) ) return SORT_gt; \
      return SORT_eq; }
#define _SC_NUMx(f,n) \
   static int sort_ ## f (const proc_t **P, const proc_t **Q) { \
      return Frame_srtflg * ( (*Q)->n - (*P)->n ); }
#define _SC_STRx(f,s) \
   static int sort_ ## f (const proc_t **P, const proc_t **Q) { \
      return Frame_srtflg * strcmp((*Q)->s, (*P)->s); }

        /* Used to 'inline' those portions of the display requiring formatting
           while ensuring we won't be blindsided by some whacko terminal's
           '$<..>' (millesecond delay) lurking in a terminfo string.  */
#define PUTP(fmt,arg...) do { \
           char _str[ROWBUFSIZ]; \
           snprintf(_str, sizeof(_str), fmt, ## arg); \
           putp(_str); \
        } while (0);

/*------  Special Macros (debug and/or informative)  ---------------------*/

        /* Orderly end, with any sort of message - see fmtmk */
#define debug_END(s) { \
           static void std_err (const char *); \
           fputs(Cap_clr_scr, stdout); \
           std_err(s); \
        }

        /* A poor man's breakpoint, if he's too lazy to learn gdb */
#define its_YOUR_fault { *((char *)0) = '!'; }


/*######  Some Typedef's and Enum's  #####################################*/

        /* This typedef just ensures consistent 'process flags' handling */
typedef unsigned PFLG_t;

        /* These typedefs attempt to ensure consistent 'ticks' handling */
typedef unsigned long long TICS_t;
typedef          long long STIC_t;

        /* Sorted columns support. */
typedef int (*QSORT_t)(const void *, const void *);

        /* This structure consolidates the information that's used
           in a variety of display roles. */
typedef struct {
   const char   *head;  /* name for column headings + toggle/reorder fields */
   const char   *fmts;  /* sprintf format string for field display */
   const int     width; /* field width, if applicable */
   const int     scale; /* scale_num type, if applicable */
   const QSORT_t sort;  /* sort function */
   const char   *desc;  /* description for toggle/reorder fields */
} FTAB_t;

        /* This structure stores one piece of critical 'history'
           information from one frame to the next -- we don't calc
           and save data that goes unused like the old top! */
typedef struct {
   int    pid;
   TICS_t tics;
} HIST_t;

        /* This structure stores a frame's cpu tics used in history
           calculations.  It exists primarily for SMP support but serves
           all environments. */
typedef struct {
   TICS_t u,            /* ticks count as represented in /proc/stat */
          n,            /* (not in the order of our display) */
          s,
          i,
          w;
   TICS_t u_sav,        /* tics count in the order of our display */
          s_sav,
          n_sav,
          i_sav,
          w_sav;
} CPUS_t;

        /* The scaling 'type' used with scale_num() -- this is how
           the passed number is interpreted should scaling be necessary */
enum scale_num {
   SK_no, SK_Kb, SK_Mb, SK_Gb
};

        /* Flags for each possible field */
enum pflag {
   P_PID, P_PPD, P_PGD, P_UID, P_USR, P_GRP, P_TTY,
   P_PRI, P_NCE,
   P_CPN, P_CPU, P_TME, P_TM2,
   P_MEM, P_VRT, P_SWP, P_RES, P_COD, P_DAT, P_SHR,
   P_FLT, P_DRT,
   P_STA, P_CMD, P_WCH, P_FLG
};


        /* ////////////////////////////////////////////////////////////// */
        /* Special Section: multiple windows/field groups  ---------------*/
        /* (kind of a header within a header:  constants, macros & types) */

#define GROUPSMAX  4            /* the max number of simultaneous windows */
#define WINNAMSIZ  4            /* max size of a window's basic name      */
#define GRPNAMSIZ  6            /* window's name + number as in: '#:...'  */
#define CAPTABMAX  9            /* a window's captab used by show_special */

#define Flags_TOG  1            /* these are used to direct wins_reflag   */
#define Flags_SET  2
#define Flags_OFF  3

        /* The Persistent 'Mode' flags!
           All of these are preserved in the rc file, as a single integer.
           Thus, once personalized, this top will stay personalized across
           restarts (not like the old top, who only remembered fields)!
           Note: the letter shown is the corresponding 'command' toggle
         */
        /* 'View_' flags affect the summary information, taken from 'Curwin' */
#define View_CPUSUM  0x8000     /* '1' - show combined cpu stats (vs. each)  */
#define View_LOADAV  0x4000     /* 'l' - display load avg and uptime summary */
#define View_STATES  0x2000     /* 't' - display task/cpu(s) states summary  */
#define View_MEMORY  0x1000     /* 'm' - display memory summary              */
        /* 'Show_' & 'Qsrt_' flags are for task display in a visible window  */
#define Show_COLORS  0x0800     /* 'z' - show in color (vs. mono)            */
#define Show_HIBOLD  0x0400     /* 'b' - rows and/or cols bold (vs. reverse) */
#define Show_HICOLS  0x0200     /* 'x' - show sort column highlighted        */
#define Show_HIROWS  0x0100     /* 'y' - show running tasks highlighted      */
#define Show_CMDLIN  0x0080     /* 'c' - show cmdline vs. name               */
#define Show_CTIMES  0x0040     /* 'S' - show times as cumulative            */
#define Show_IDLEPS  0x0020     /* 'i' - show idle processes (all tasks)     */
#define Qsrt_NORMAL  0x0010     /* 'R' - reversed column sort (high to low)  */
        /* these flag(s) have no command as such - they're for internal use  */
#define VISIBLE_tsk  0x0008     /* tasks are showable when in 'Mode_altscr'  */
#define NEWFRAM_cwo  0x0004     /* new frame (if anyone cares) - in Curwin   */
#define EQUWINS_cwo  0x0002     /* rebalance tasks next frame (off 'i'/ 'n') */
                                /* ...set in Curwin, but impacts all windows */
#define flag_unused  0x0001     /* ----------------------- future use, maybe */

        /* Current-window-only flags -- always turned off at end-of-window!  */
#define FLGSOFF_cwo  EQUWINS_cwo | NEWFRAM_cwo

        /* Default flags if there's no rcfile to provide user customizations */
#define DEF_WINFLGS ( View_LOADAV | View_STATES | View_CPUSUM | View_MEMORY | \
   Show_HIBOLD | Show_HIROWS | Show_IDLEPS | Qsrt_NORMAL | VISIBLE_tsk )

        /* Used to test/manipulate the window flags */
#define CHKw(q,f)   (int)(q->winflags & (f))
#define TOGw(q,f)   q->winflags ^=  (f)
#define SETw(q,f)   q->winflags |=  (f)
#define OFFw(q,f)   q->winflags &= ~(f)
#define VIZCHKc     (!Mode_altscr || Curwin->winflags & VISIBLE_tsk) \
                        ? 1 : win_warn()
#define VIZTOGc(f)  (!Mode_altscr || Curwin->winflags & VISIBLE_tsk) \
                        ? TOGw(Curwin, f) : win_warn()

        /* This structure stores configurable information for each window.
           By expending a little effort in its creation and user requested
           maintainence, the only real additional per frame cost of having
           windows is an extra sort -- but that's just on ptrs! */
typedef struct win {
   struct win *next,                    /* next window in window stack    */
              *prev;                    /* prior window in window stack   */
   char       *captab [CAPTABMAX];      /* captab needed by show_special  */
   int         winnum,                  /* window's num (array pos + 1)   */
               winlines;                /* task window's rows (volatile)  */
   int         winflags;        /* 'view', 'show' and 'sort' mode flags   */
   PFLG_t      procflags [PFLAGSSIZ],   /* fieldscur subset, as enum      */
               sortindx;                /* sort field, as a procflag      */
   int         maxpflgs,        /* number of procflags (upcase fieldscur) */
               maxtasks,        /* user requested maximum, 0 equals all   */
               maxcmdln,        /* max length of a process' command line  */
               summclr,                 /* color num used in summ info    */
               msgsclr,                 /*        "       in msgs/pmts    */
               headclr,                 /*        "       in cols head    */
               taskclr;                 /*        "       in task display */
   int         len_rownorm,     /* lengths of the corresponding terminfo  */
               len_rowhigh;     /* strings to save mkcol() a strlen call  */
   char        capclr_sum [CLRBUFSIZ],  /* terminfo strings built from    */
               capclr_msg [CLRBUFSIZ],  /*    above clrs (& rebuilt too), */
               capclr_pmt [CLRBUFSIZ],  /*    but NO recurring costs !!!  */
               capclr_hdr [CLRBUFSIZ],     /* note: sum, msg and pmt strs */
               capclr_rowhigh [CLRBUFSIZ], /*    are only used when this  */
               capclr_rownorm [CLRBUFSIZ]; /*    window is the 'Curwin'!  */
   char        grpname   [GRPNAMSIZ],   /* window number:name, printable  */
               winname   [WINNAMSIZ],   /* window name, user changeable   */
               fieldscur [PFLAGSSIZ],   /* fields displayed and ordered   */
               columnhdr [SMLBUFSIZ],   /* column headings for procflags  */
               colusrnam [USRNAMSIZ];   /* if selected by the 'u' command */
} WIN_t;
        /* ////////////////////////////////////////////////////////////// */


/*######  Display Support *Data*  ########################################*/

        /* An rcfile 'footprint' used to invalidate existing local rcfile
           and the global rcfile path + name */
#define RCF_FILEID  'a'
#define SYS_RCFILE  "/etc/toprc"

        /* The default fields displayed and their order,
           if nothing is specified by the loser, oops user */
#define DEF_FIELDS  "AEHIOQTWKNMXbcdfgjplrsuvyz"
        /* Pre-configured field groupss */
#define JOB_FIELDS  "ABXcefgjlrstuvyzMKNHIWOPQD"
#define MEM_FIELDS  "ANOPQRSTUVXbcdefgjlmyzWHIK"
#define USR_FIELDS  "DEFGABXchijlopqrstuvyzMKNW"
        /* Used by fields_sort, placed here for peace-of-mind */
#define NUL_FIELDS  "abcdefghijklmnopqrstuvwxyz"

        /* These are the possible fscanf formats used in /proc/stat
           reads during history processing.
           ( 5th number only for Linux 2.5.41 and above ) */
#define CPU_FMTS_JUST1  "cpu %Lu %Lu %Lu %Lu %Lu"
#ifdef PRETEND4CPUS
#define CPU_FMTS_MULTI CPU_FMTS_JUST1
#else
#define CPU_FMTS_MULTI  "cpu%*d %Lu %Lu %Lu %Lu %Lu"
#endif

        /* This is the format for 'command line' display in the absence
           of a command line (kernel thread). */
#ifdef POSIX_CMDLIN
#define CMDLINE_FMTS  "[%s]"
#else
#define CMDLINE_FMTS  "( %s )"
#endif

        /* Summary Lines specially formatted string(s) --
           see 'show_special' for syntax details + other cautions. */
#define LOADAV_line  "%s -%s\n"
#define LOADAV_line_alt  "%s\06 -%s\n"
#define STATES_line1  "Tasks:\03" \
   " %3u \02total,\03 %3u \02running,\03 %3u \02sleeping,\03 %3u \02stopped,\03 %3u \02zombie\03\n"
#define STATES_line2x4  "%s\03" \
   " %#5.1f%% \02user,\03 %#5.1f%% \02system,\03 %#5.1f%% \02nice,\03 %#5.1f%% \02idle\03\n"
#define STATES_line2x5  "%s\03" \
   " %#5.1f%% \02user,\03 %#5.1f%% \02system,\03 %#5.1f%% \02nice,\03 %#5.1f%% \02idle,\03 %#5.1f%% \02IO-wait\03\n"
#ifdef CASEUP_SUMMK
#define MEMORY_line1  "Mem: \03" \
   " %8uK \02total,\03 %8uK \02used,\03 %8uK \02free,\03 %8uK \02buffers\03\n"
#define MEMORY_line2  "Swap:\03" \
   " %8uK \02total,\03 %8uK \02used,\03 %8uK \02free,\03 %8uK \02cached\03\n"
#else
#define MEMORY_line1  "Mem: \03" \
   " %8uk \02total,\03 %8uk \02used,\03 %8uk \02free,\03 %8uk \02buffers\03\n"
#define MEMORY_line2  "Swap:\03" \
   " %8uk \02total,\03 %8uk \02used,\03 %8uk \02free,\03 %8uk \02cached\03\n"
#endif

        /* Keyboard Help specially formatted string(s) --
           see 'show_special' for syntax details + other cautions. */
#define KEYS_help \
   "Help for Interactive Commands\02 - %s\n" \
   "Window \01%s\06: \01Cumulative mode \03%s\02.  \01System\06: \01Delay \03%.1f secs\02; \01Secure mode \03%s\02.\n" \
   "\n" \
   "  l,t,m     Toggle Summary: '\01l\02' load avg; '\01t\02' task/cpu stats; '\01m\02' mem info\n" \
   "  1,I       Toggle SMP view: '\0011\02' single/separate states; '\01I\02' Irix/Solaris mode\n" \
   "  Z\05         Change color mappings\n" \
   "\n" \
   "  f,o     . Fields/Columns: '\01f\02' add or remove; '\01o\02' change display order\n" \
   "  F or O  . Select sort field\n" \
   "  <,>     . Move sort field: '\01<\02' next col left; '\01>\02' next col right\n" \
   "  R       . Toggle normal/reverse sort\n" \
   "  c,i,S   . Toggle: '\01c\02' cmd name/line; '\01i\02' idle tasks; '\01S\02' cumulative time\n" \
   "  x,y\05     . Toggle highlights: '\01x\02' sort field; '\01y\02' running tasks\n" \
   "  z,b\05     . Toggle: '\01z\02' color/mono; '\01b\02' bold/reverse (only if 'x' or 'y')\n" \
   "  u       . Show specific user only\n" \
   "  n or #  . Set maximum tasks displayed\n" \
   "\n" \
   "%s" \
   "  W         Write configuration file\n" \
   "  q         Quit\n" \
   "          ( commands shown with '.' require a \01visible\02 task display \01window\02 ) \n" \
   "Press '\01h\02' or '\01?\02' for help with \01Windows\02,\n" \
   "any other key to continue " \
   ""

        /* This guy goes into the help text (maybe) */
#define KEYS_help_unsecured \
   "  k,r       Manipulate tasks: '\01k\02' kill; '\01r\02' renice\n" \
   "  d or s    Set update interval\n" \
   ""

        /* Fields Reorder/Toggle specially formatted string(s) --
           see 'show_special' for syntax details + other cautions
           note: the leading newline below serves really dumb terminals;
                 if there's no 'cursor_home', the screen will be a mess
                 but this part will still be functional. */
#define FIELDS_current \
   "\n%sCurrent Fields\02: \01 %s \04 for window \01%s\06\n%s " \
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
   "  0x00002000  PF_FREE_PAGES (2.5)\n" \
   "  0x00008000  debug flag (2.5)\n" \
   "  0x00024000  special threads (2.5)\n" \
   "  0x001D0000  special states (2.5)\n" \
   "  0x00100000  PF_USEDFPU (thru 2.4)\n" \
   ""
/* no room, sacrificed this one:  'Killed for out-of-memory' */
/* "  0x00001000  PF_MEMDIE (2.5)\n" ....................... */

        /* Sort Select specially formatted string(s) --
           see 'show_special' for syntax details + other cautions
           note: the leading newline below serves really dumb terminals;
                 if there's no 'cursor_home', the screen will be a mess
                 but this part will still be functional. */
#define SORT_fields \
   "\n%sCurrent Sort Field\02: \01 %c \04 for window \01%s\06\n%s " \
   ""

        /* Some extra explanatory text which accompanies the Sort display.
           note: the newlines cannot actually be used, they just serve as
                 substring delimiters for the 'display_fields' routine. */
#define SORT_xtra \
   "Note1:\n" \
   "  If a selected sort field can't be\n" \
   "  shown due to screen width or your\n" \
   "  field order, the '<' and '>' keys\n" \
   "  will be unavailable until a field\n" \
   "  within viewable range is chosen.\n" \
   "\n" \
   "Note2:\n" \
   "  Field sorting uses internal values,\n" \
   "  not those in column display.  Thus,\n" \
   "  the TTY & WCHAN fields will violate\n" \
   "  strict ASCII collating sequence.\n" \
   "  (shame on you if WCHAN is chosen)\n" \
   ""

        /* Colors Help specially formatted string(s) --
           see 'show_special' for syntax details + other cautions. */
#define COLOR_help \
   "Help for color mapping\02 - %s\n" \
   "current window: \01%s\06\n" \
   "\n" \
   "   color -\03 04:25:44 up 8 days, 50 min,  7 users,  load average:\n" \
   "   Tasks:\03  64 \02total,\03   2 \02running,\03  62 \02sleeping,\03   0 \02stopped,\03\n" \
   "   State cpu0 :\03   76.5%% \02user,\03  11.2%% \02system,\03   0.0%% \02nice,\03\n" \
   "   \01 Nasty Message! \04  -or-  \01Input Prompt\05\n" \
   "   \01  PID TTY     PR  NI %%CPU    TIME+   VIRT SWAP STA Command  \06\n" \
   "   17284 \10pts/2  \07  8   0  0.0   0:00.75  1380    0 S   /bin/bash \10\n" \
   "   \01 8601 pts/1    7 -10  0.4   0:00.03   916    0 R < color -b \07\n" \
   "   11005 \10?      \07  9   0  0.0   0:02.50  2852 1008 S   amor -ses \10\n" \
   "   available toggles: \01b\02 =bold/reverse (\01%s\02), \01z\02 =color/mono (\01%s\02)\n" \
   "\n" \
   "Select \01target\02 as upper case letter:\n" \
   "   S\02 = Summary Data,\01  M\02 = Messages/Prompts,\n" \
   "   H\02 = Column Heads,\01  T\02 = Task Information\n" \
   "Select \01color\02 as number:\n" \
   "   0\02 = black,\01  1\02 = red,    \01  2\02 = green,\01  3\02 = yellow,\n" \
   "   4\02 = blue, \01  5\02 = magenta,\01  6\02 = cyan, \01  7\02 = white\n" \
   "\n" \
   "Selected: \01target\02 \01 %c \04; \01color\02 \01 %d \04\n" \
   "   press 'q' to abort changes to window '\01%s\02'\n" \
   "   press 'a' or 'w' to commit & change another, <Enter> to commit and end " \
   ""

        /* Windows/Field Group Help specially formatted string(s) --
           see 'show_special' for syntax details + other cautions. */
#define WINDOWS_help \
   "Help for Windows / Field Groups\02 - \"Current Window\" = \01 %s \06\n" \
   "\n" \
   ". Use multiple \01windows\02, each with separate config opts (color,fields,sort,etc)\n" \
   ". The 'current' window controls the \01Summary Area\02 and responds to your \01Commands\02\n" \
   "  . that window's \01task display\02 can be turned \01Off\02 & \01On\02, growing/shrinking others\n" \
   "  . with \01NO\02 task display, some commands will be \01disabled\02 ('i','R','n','c', etc)\n" \
   "    until a \01different window\02 has been activated, making it the 'current' window\n" \
   ". You \01change\02 the 'current' window by: \01 1\02) cycling forward/backward;\01 2\02) choosing\n" \
   "  a specific field group; or\01 3\02) exiting the color mapping screen\n" \
   ". Commands \01available anytime   -------------\02\n" \
   "    A       . Alternate display mode toggle, show \01Single\02 / \01Multiple\02 windows\n" \
   "    G       . Choose another field group and make it 'current', or change now\n" \
   "              by selecting a number from: \01 1\02 =%s;\01 2\02 =%s;\01 3\02 =%s; or\01 4\02 =%s\n" \
   ". Commands \01requiring\02 '\01A\02' mode\01  -------------\02\n" \
   "    g       . Change the \01Name\05 of the 'current' window/field group\n" \
   " \01*\04  a , w   . Cycle through all four windows:  '\01a\05' Forward; '\01w\05' Backward\n" \
   " \01*\04  - , _   . Show/Hide:  '\01-\05' \01Current\02 window; '\01_\05' all \01Visible\02/\01Invisible\02\n" \
   "  The screen will be divided evenly between task displays.  But you can make\n" \
   "  some \01larger\02 or \01smaller\02, using '\01n\02' and '\01i\02' commands.  Then later you could:\n" \
   " \01*\04  = , +   . Rebalance tasks:  '\01=\05' \01Current\02 window; '\01+\05' \01Every\02 window\n" \
   "              (this also forces the \01current\02 or \01every\02 window to become visible)\n" \
   "\n" \
   "In '\01A\02' mode, '\01*\04' keys are your \01essential\02 commands.  Please try the '\01a\02' and '\01w\02'\n" \
   "commands plus the 'G' sub-commands NOW.  Press <Enter> to make 'Current' " \
   ""


/*######  Some Prototypes (ha!)  #########################################*/

   /* None of these are necessary when the source file is properly
    * organized -- they're here for documentation purposes !
    * Note also that functions are alphabetical within a group to aid
    * source code navigation, which often influences the identifers. */
/*------  Sort callbacks  ------------------------------------------------*/
/*        for each possible field, in the form of:                        */
/*atic int         sort_P_XXX (const proc_t **P, const proc_t **Q);       */
/*------  Tiny useful routine(s)  ----------------------------------------*/
//atic int         chin (int ech, char *buf, unsigned cnt);
//atic const char *fmtmk (const char *fmts, ...);
//atic char       *strim (int sp, char *str);
//atic const char *tg2 (int x, int y);
/*------  Exit/Interrput routines  ---------------------------------------*/
//atic void        bye_bye (int eno, const char *str);
//atic void        stop (int dont_care_sig);
//atic void        std_err (const char *str);
//atic void        suspend (int dont_care_sig);
/*------  Misc Color/Display support  ------------------------------------*/
//atic void        capsmk (WIN_t *q);
//atic void        msg_save (const char *fmts, ...);
//atic void        show_msg (const char *str);
//atic void        show_pmt (const char *str);
//atic void        show_special (const char *glob);
/*------  Small Utility routines  ----------------------------------------*/
//atic char       *ask4str (const char *prompt);
//atic float       get_float (const char *prompt);
//atic int         get_int (const char *prompt);
//atic const char *scale_num (unsigned num, const int width, const unsigned type);
//atic const char *scale_tics (TICS_t tics, const int width);
//atic void        time_elapsed (void);
/*------  Library Alternatives  ------------------------------------------*/
//atic void       *alloc_c (unsigned numb);
//atic void       *alloc_r (void *q, unsigned numb);
//atic CPUS_t     *refreshcpus (CPUS_t *cpus);
//atic proc_t    **refreshprocs (proc_t **table, int flags);
/*------  Startup routines  ----------------------------------------------*/
//atic void        before (char *me);
//atic void        configs_read (void);
//atic void        parse_args (char **args);
//atic void        whack_terminal (void);
/*------  Field Selection/Ordering routines  -----------------------------*/
/*atic FTAB_t      Fieldstab[] = { ... }                                  */
//atic void        display_fields (const char *fields, const char *xtra);
//atic void        fields_reorder (void);
//atic void        fields_sort (void);
//atic void        fields_toggle (void);
/*------  Windows/Field Groups support  ----------------------------------*/
//atic void        win_colsheads (WIN_t *q);
//atic inline int  win_fldviz (WIN_t *q, int flg);
//atic void        win_names (WIN_t *q, const char *name);
//atic void        win_select (char ch);
//atic int         win_warn (void);
//atic void        winsclr (WIN_t *q, int save);
//atic void        wins_colors (void);
//atic void        wins_reflag (int what, int flg);
//atic void        wins_resize (int dont_care_sig);
//atic void        windows_stage1 (void);
//atic void        windows_stage2 (void);
/*------  Per-Frame Display support  -------------------------------------*/
//atic void        cpudo (CPUS_t *cpu, const char *pfx);
//atic void        frame_states (proc_t **ppt, int show);
//atic void        frame_storage (void);
//atic void        mkcol (WIN_t *q, PFLG_t idx, int sta, int *pad, char *buf, ...);
//atic void        show_a_task (WIN_t *q, proc_t *task);
/*------  Main Screen routines  ------------------------------------------*/
//atic void        do_key (unsigned c);
//atic proc_t    **do_summary (void);
//atic void        do_window (proc_t **ppt, WIN_t *q, int *lscr);
//atic void        sohelpme (int wix, int max);
//atic void        so_lets_see_em (void);
/*------  Entry point  ---------------------------------------------------*/
//     int         main (int dont_care_argc, char **argv);

        /* just sanity check(s)... */
#if USRNAMSIZ < GETBUFSIZ
 #error "Jeeze, USRNAMSIZ Must NOT be less than GETBUFSIZ !"
#endif

#endif /* _Itop */

