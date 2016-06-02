/* top.h - Header file:         show Linux processes */
/*
 * Copyright (c) 2002-2015, by: James C. Warner
 *    All rights reserved.      8921 Hilloway Road
 *                              Eden Prairie, Minnesota 55347 USA
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
#ifndef _Itop
#define _Itop

#include "../proc/readproc.h"

        /* Defines represented in configure.ac ----------------------------- */
//#define BOOST_PERCNT            /* enable extra precision for two % fields */
//#define NOBOOST_MEMS            /* disable extra precision for mem fields  */
//#define NUMA_DISABLE            /* disable summary area NUMA/Nodes display */
//#define ORIG_TOPDEFS            /* with no rcfile retain original defaults */
//#define SIGNALS_LESS            /* favor reduced signal load over response */

        /* Development/Debugging defines ----------------------------------- */
//#define ATEOJ_RPTHSH            /* report on hash specifics, at end-of-job */
//#define ATEOJ_RPTSTD            /* report on misc stuff, at end-of-job     */
//#define CASEUP_HEXES            /* show any hex values in upper case       */
//#define CASEUP_SUFIX            /* show time/mem/cnts suffix in upper case */
//#define CPU_ZEROTICS            /* tolerate few tics when cpu off vs. idle */
//#define EQUCOLHDRYES            /* yes, do equalize column header lengths  */
//#define INSP_JUSTNOT            /* don't smooth unprintable right margins  */
//#define INSP_OFFDEMO            /* disable demo screens, issue msg instead */
//#define INSP_SAVEBUF            /* preserve 'Insp_buf' contents in a file  */
//#define INSP_SLIDE_1            /* when scrolling left/right don't move 8  */
//#define MEMGRAPH_OLD            /* don't use 'available' when graphing Mem */
//#define OFF_HST_HASH            /* use BOTH qsort+bsrch vs. hashing scheme */
//#define OFF_NUMASKIP            /* do NOT skip numa nodes if discontinuous */
//#define OFF_SCROLLBK            /* disable tty emulators scrollback buffer */
//#define OFF_STDERROR            /* disable our stderr buffering (redirect) */
//#define OFF_STDIOLBF            /* disable our own stdout _IOFBF override  */
//#define PRETEND2_5_X            /* pretend we're linux 2.5.x (for IO-wait) */
//#define PRETEND8CPUS            /* pretend we're smp with 8 ticsers (sic)  */
//#define PRETENDNOCAP            /* use a terminal without essential caps   */
//#define PRETEND_NUMA            /* pretend 4 (or 3 w/o OFF_NUMASKIP) Nodes */
//#define QUICK_GRAPHS            /* use fast algorithm, accept +2% distort  */
//#define RCFILE_NOERR            /* rcfile errs silently default, vs. fatal */
//#define RECALL_FIXED            /* don't reorder saved strings if recalled */
//#define RMAN_IGNORED            /* don't consider auto right margin glitch */
//#define SCROLLVAR_NO            /* disable intra-column horizontal scroll  */
//#define STRINGCASENO            /* case insenstive compare/locate versions */
//#define TERMIOS_ONLY            /* just limp along with native input only  */
//#define TREE_NORESET            /* sort keys do NOT force forest view OFF  */
//#define TREE_SCANALL            /* rescan array w/ forest view, avoid sort */
//#define USE_X_COLHDR            /* emphasize header vs. whole col, for 'x' */
//#define VALIDATE_NLS            /* validate the integrity of all nls tbls  */


/*######  Notes, etc.  ###################################################*/

        /* The following convention is used to identify those areas where
           adaptations for hotplugging are to be found ...
              *** hotplug_acclimated ***
           ( hopefully libproc will also be supportive of our efforts ) */

        /* For introducing inaugural cgroup support, thanks to:
              Jan Gorig <jgorig@redhat.com> - April, 2011 */

        /* For the motivation and path to nls support, thanks to:
              Sami Kerola, <kerolasa@iki.fi> - December, 2011 */

        /* There are still some short strings that may yet be candidates
           for nls support inclusion.  They're identified with:
              // nls_maybe */

        /* For initiating the topic of potential % CPU distortions due to
           to kernel and/or cpu anomalies (see CPU_ZEROTICS), thanks to:
              Jaromir Capik, <jcapik@redhat.com> - February, 2012 */

        /* For the impetus and NUMA/Node prototype design, thanks to:
              Lance Shelton <LShelton@fusionio.com> - April, 2013 */

#ifdef PRETEND2_5_X
#define linux_version_code LINUX_VERSION(2,5,43)
#endif

   // pretend as if #define _GNU_SOURCE
char *strcasestr(const char *haystack, const char *needle);

#ifdef STRINGCASENO
#define STRSTR  strcasestr
#define STRCMP  strcasecmp
#else
#define STRSTR  strstr
#define STRCMP  strcmp
#endif


/*######  Some Miscellaneous constants  ##################################*/

        /* The default delay twix updates */
#ifdef ORIG_TOPDEFS
#define DEF_DELAY  3.0
#else
#define DEF_DELAY  1.5
#endif

        /* Length of time a message is displayed and the duration
           of a 'priming' wait during library startup (in microseconds) */
#define MSG_USLEEP  1250000
#define LIB_USLEEP  150000

        /* Specific process id monitoring support (command line only) */
#define MONPIDMAX  20

        /* Output override minimums (the -w switch and/or env vars) */
#define W_MIN_COL  3
#define W_MIN_ROW  3

        /* Miscellaneous buffers with liberal values and some other defines
           -- mostly just to pinpoint source code usage/dependancies */
#define SCREENMAX   512
   /* the above might seem pretty stingy, until you consider that with every
      field displayed the column header would be approximately 250 bytes
      -- so SCREENMAX provides for all fields plus a 250+ byte command line */
#define CAPBUFSIZ    32
#define CLRBUFSIZ    64
#define PFLAGSSIZ    80
#define SMLBUFSIZ   128
#define MEDBUFSIZ   256
#define LRGBUFSIZ   512
#define OURPATHSZ  1024
#define BIGBUFSIZ  2048
   /* in addition to the actual display data, our row might have to accommodate
      many termcap/color transitions - these definitions ensure we have room */
#define ROWMINSIZ  ( SCREENMAX +  4 * (CAPBUFSIZ + CLRBUFSIZ) )
#define ROWMAXSIZ  ( SCREENMAX + 16 * (CAPBUFSIZ + CLRBUFSIZ) )
   // minimum size guarantee for dynamically acquired 'readfile' buffer
#define READMINSZ  2048
   // size of preallocated search string buffers, same as ioline()
#define FNDBUFSIZ  MEDBUFSIZ


   // space between task fields/columns
#define COLPADSTR   " "
#define COLPADSIZ   ( sizeof(COLPADSTR) - 1 )
   // continuation ch when field/column truncated
#define COLPLUSCH   '+'

   // support for keyboard stuff (cursor motion keystrokes, mostly)
#define kbd_ESC    '\033'
#define kbd_SPACE  ' '
#define kbd_ENTER  '\n'
#define kbd_UP     129
#define kbd_DOWN   130
#define kbd_LEFT   131
#define kbd_RIGHT  132
#define kbd_PGUP   133
#define kbd_PGDN   134
#define kbd_HOME   135
#define kbd_END    136
#define kbd_BKSP   137
#define kbd_INS    138
#define kbd_DEL    139
#define kbd_CtrlO  '\017'

        /* Special value in Pseudo_row to force an additional procs refresh
           -- used at startup and for task/thread mode transitions */
#define PROC_XTRA  -1

#ifndef CPU_ZEROTICS
        /* This is the % used in establishing the tics threshold below
           which a cpu is treated as 'idle' rather than displaying
           misleading state percentages */
#define TICS_EDGE  20
#endif


/* #####  Enum's and Typedef's  ############################################ */

        /* Flags for each possible field (and then some) --
           these MUST be kept in sync with the FLD_t Fieldstab[] array !! */
enum pflag {
   EU_PID = 0, EU_PPD,
   EU_UED, EU_UEN, EU_URD, EU_URN, EU_USD, EU_USN,
   EU_GID, EU_GRP, EU_PGD, EU_TTY, EU_TPG, EU_SID,
   EU_PRI, EU_NCE, EU_THD,
   EU_CPN, EU_CPU, EU_TME, EU_TM2,
   EU_MEM, EU_VRT, EU_SWP, EU_RES, EU_COD, EU_DAT, EU_SHR,
   EU_FL1, EU_FL2, EU_DRT,
   EU_STA, EU_CMD, EU_WCH, EU_FLG, EU_CGR,
   EU_SGD, EU_SGN, EU_TGD,
   EU_OOA, EU_OOM,
   EU_ENV,
   EU_FV1, EU_FV2,
   EU_USE,
   EU_NS1, EU_NS2, EU_NS3, EU_NS4, EU_NS5, EU_NS6,
   EU_LXC,
   EU_RZA, EU_RZF, EU_RZL, EU_RZS,
#ifdef USE_X_COLHDR
   // not really pflags, used with tbl indexing
   EU_MAXPFLGS
#else
   // not really pflags, used with tbl indexing & col highlighting
   EU_MAXPFLGS, EU_XON, EU_XOF
#endif
};

        /* The scaling 'target' used with memory fields */
enum scale_enum {
   SK_Kb, SK_Mb, SK_Gb, SK_Tb, SK_Pb, SK_Eb
};

        /* Used to manipulate (and document) the Frames_signal states */
enum resize_states {
   BREAK_off = 0, BREAK_kbd, BREAK_sig
};

        /* This typedef just ensures consistent 'process flags' handling */
typedef unsigned char FLG_t;

        /* These typedefs attempt to ensure consistent 'ticks' handling */
typedef unsigned long long TIC_t;
typedef          long long SIC_t;

        /* Sort support, callback function signature */
typedef int (*QFP_t)(const void *, const void *);

        /* This structure consolidates the information that's used
           in a variety of display roles. */
typedef struct FLD_t {
   int           width;         // field width, if applicable
   int           scale;         // scaled target, if applicable
   const int     align;         // the default column alignment flag
   const QFP_t   sort;          // sort function
   const int     lflg;          // PROC_FILLxxx flag(s) needed by this field
} FLD_t;

#ifdef OFF_HST_HASH
        /* This structure supports 'history' processing and records the
           bare minimum of needed information from one frame to the next --
           we don't calc and save data that goes unused like the old top. */
typedef struct HST_t {
   TIC_t tics;                  // last frame's tics count
   unsigned long maj, min;      // last frame's maj/min_flt counts
   int pid;                     // record 'key'
} HST_t;
#else
        /* This structure supports 'history' processing and records the bare
           minimum of needed information from one frame to the next -- we do
           not calc and save data that goes unused like the old top nor will
           we incur the additional overhead of sort to support binary search
           (or worse, a friggin' for loop) when retrievals become necessary! */
typedef struct HST_t {
   TIC_t tics;                  // last frame's tics count
   unsigned long maj, min;      // last frame's maj/min_flt counts
   int pid;                     // record 'key'
   int lnk;                     // next on hash chain
} HST_t;
#endif

        /* These 2 structures store a frame's cpu tics used in history
           calculations.  They exist primarily for SMP support but serve
           all environments. */
typedef struct CT_t {
   /* other kernels: u == user/us, n == nice/ni, s == system/sy, i == idle/id
      2.5.41 kernel: w == IO-wait/wa (io wait time)
      2.6.0  kernel: x == hi (hardware irq time), y == si (software irq time)
      2.6.11 kernel: z == st (virtual steal time) */
   TIC_t u, n, s, i, w, x, y, z;  // as represented in /proc/stat
#ifndef CPU_ZEROTICS
   SIC_t tot;                     // total from /proc/stat line 1
#endif
} CT_t;

typedef struct CPU_t {
   CT_t cur;                      // current frame's cpu tics
   CT_t sav;                      // prior frame's cpu tics
#ifndef CPU_ZEROTICS
   SIC_t edge;                    // tics adjustment threshold boundary
#endif
   int id;                        // cpu number (0 - nn), or numa active flag
#ifndef NUMA_DISABLE
   int node;                      // the numa node it belongs to
#endif
} CPU_t;

        /* /////////////////////////////////////////////////////////////// */
        /* Special Section: multiple windows/field groups  --------------- */
        /* ( kind of a header within a header: constants, types & macros ) */

#define CAPTABMAX  9             /* max entries in each win's caps table   */
#define GROUPSMAX  4             /* the max number of simultaneous windows */
#define WINNAMSIZ  4             /* size of RCW_t winname buf (incl '\0')  */
#define GRPNAMSIZ  WINNAMSIZ+2   /* window's name + number as in: '#:...'  */

        /* The Persistent 'Mode' flags!
           These are preserved in the rc file, as a single integer and the
           letter shown is the corresponding 'command' toggle */
        // 'View_' flags affect the summary (minimum), taken from 'Curwin'
#define View_CPUSUM  0x008000     // '1' - show combined cpu stats (vs. each)
#define View_CPUNOD  0x400000     // '2' - show numa node cpu stats ('3' also)
#define View_LOADAV  0x004000     // 'l' - display load avg and uptime summary
#define View_STATES  0x002000     // 't' - display task/cpu(s) states summary
#define View_MEMORY  0x001000     // 'm' - display memory summary
#define View_NOBOLD  0x000008     // 'B' - disable 'bold' attribute globally
#define View_SCROLL  0x080000     // 'C' - enable coordinates msg w/ scrolling
        // 'Show_' & 'Qsrt_' flags are for task display in a visible window
#define Show_COLORS  0x000800     // 'z' - show in color (vs. mono)
#define Show_HIBOLD  0x000400     // 'b' - rows and/or cols bold (vs. reverse)
#define Show_HICOLS  0x000200     // 'x' - show sort column emphasized
#define Show_HIROWS  0x000100     // 'y' - show running tasks highlighted
#define Show_CMDLIN  0x000080     // 'c' - show cmdline vs. name
#define Show_CTIMES  0x000040     // 'S' - show times as cumulative
#define Show_IDLEPS  0x000020     // 'i' - show idle processes (all tasks)
#define Show_TASKON  0x000010     // '-' - tasks showable when Mode_altscr
#define Show_FOREST  0x000002     // 'V' - show cmd/cmdlines with ascii art
#define Qsrt_NORMAL  0x000004     // 'R' - reversed column sort (high to low)
#define Show_JRSTRS  0x040000     // 'j' - right justify "string" data cols
#define Show_JRNUMS  0x020000     // 'J' - right justify "numeric" data cols
        // these flag(s) have no command as such - they're for internal use
#define INFINDS_xxx  0x010000     // build rows for find_string, not display
#define EQUWINS_xxx  0x000001     // rebalance all wins & tasks (off i,n,u/U)
#ifndef USE_X_COLHDR
#define NOHISEL_xxx  0x200000     // restrict Show_HICOLS for osel temporarily
#define NOHIFND_xxx  0x100000     // restrict Show_HICOLS for find temporarily
#endif

        // Default flags if there's no rcfile to provide user customizations
#ifdef ORIG_TOPDEFS
#define DEF_WINFLGS ( View_LOADAV | View_STATES | View_CPUSUM | View_MEMORY \
   | Show_HIBOLD | Show_HIROWS | Show_IDLEPS | Show_TASKON | Show_JRNUMS \
   | Qsrt_NORMAL )
#define DEF_GRAPHS2  0, 0
#define DEF_SCALES2  SK_Kb, SK_Kb
#define ALT_WINFLGS  DEF_WINFLGS
#define ALT_GRAPHS2  0, 0
#else
#define DEF_WINFLGS ( View_LOADAV | View_STATES | View_MEMORY \
   | Show_COLORS | Show_FOREST | Show_HIROWS | Show_IDLEPS | Show_JRNUMS | Show_TASKON \
   | Qsrt_NORMAL )
#define DEF_GRAPHS2  1, 2
#define DEF_SCALES2  SK_Gb, SK_Mb
#define ALT_WINFLGS (DEF_WINFLGS | Show_HIBOLD) & ~Show_FOREST
#define ALT_GRAPHS2  2, 0
#endif

        /* These are used to direct wins_reflag */
enum reflag_enum {
   Flags_TOG, Flags_SET, Flags_OFF
};

        /* These are used to direct win_warn */
enum warn_enum {
   Warn_ALT, Warn_VIZ
};

        /* This type helps support both a window AND the rcfile */
typedef struct RCW_t {  // the 'window' portion of an rcfile
   int    sortindx,               // sort field (represented as procflag)
          winflags,               // 'view', 'show' and 'sort' mode flags
          maxtasks,               // user requested maximum, 0 equals all
          graph_cpus,             // 't' - View_STATES supplementary vals
          graph_mems,             // 'm' - View_MEMORY supplememtary vals
          summclr,                // a colors 'number' used for summ info
          msgsclr,                //             "           in msgs/pmts
          headclr,                //             "           in cols head
          taskclr;                //             "           in task rows
   char   winname [WINNAMSIZ],    // name for the window, user changeable
          fieldscur [PFLAGSSIZ];  // the fields for display & their order
} RCW_t;

        /* This represents the complete rcfile */
typedef struct RCF_t {
   char   id;                   // rcfile version id
   int    mode_altscr;          // 'A' - Alt display mode (multi task windows)
   int    mode_irixps;          // 'I' - Irix vs. Solaris mode (SMP-only)
   float  delay_time;           // 'd'/'s' - How long to sleep twixt updates
   int    win_index;            // Curwin, as index
   RCW_t  win [GROUPSMAX];      // a 'WIN_t.rc' for each window
   int    fixed_widest;         // 'X' - wider non-scalable col addition
   int    summ_mscale;          // 'E' - scaling of summary memory values
   int    task_mscale;          // 'e' - scaling of process memory values
   int    zero_suppress;        // '0' - suppress scaled zeros toggle
} RCF_t;

        /* This structure stores configurable information for each window.
           By expending a little effort in its creation and user requested
           maintenance, the only real additional per frame cost of having
           windows is an extra sort -- but that's just on pointers! */
typedef struct WIN_t {
   FLG_t  pflgsall [PFLAGSSIZ],        // all 'active/on' fieldscur, as enum
          procflgs [PFLAGSSIZ];        // fieldscur subset, as enum
   RCW_t  rc;                          // stuff that gets saved in the rcfile
   int    winnum,          // a window's number (array pos + 1)
          winlines,        // current task window's rows (volatile)
          maxpflgs,        // number of displayed procflgs ("on" in fieldscur)
          totpflgs,        // total of displayable procflgs in pflgsall array
          begpflg,         // scrolled beginning pos into pflgsall array
          endpflg,         // scrolled ending pos into pflgsall array
          begtask,         // scrolled beginning pos into Frame_maxtask
#ifndef SCROLLVAR_NO
          varcolbeg,       // scrolled position within variable width col
#endif
          varcolsz,        // max length of variable width column(s)
          usrseluid,       // validated uid for 'u/U' user selection
          usrseltyp,       // the basis for matching above uid
          usrselflg,       // flag denoting include/exclude matches
          hdrcaplen;       // column header xtra caps len, if any
   char   capclr_sum [CLRBUFSIZ],      // terminfo strings built from
          capclr_msg [CLRBUFSIZ],      //   RCW_t colors (& rebuilt too),
          capclr_pmt [CLRBUFSIZ],      //   but NO recurring costs !
          capclr_hdr [CLRBUFSIZ],      //   note: sum, msg and pmt strs
          capclr_rowhigh [CLRBUFSIZ],  //         are only used when this
          capclr_rownorm [CLRBUFSIZ],  //         window is the 'Curwin'!
          cap_bold [CAPBUFSIZ],        // support for View_NOBOLD toggle
          grpname [GRPNAMSIZ],         // window number:name, printable
#ifdef USE_X_COLHDR
          columnhdr [ROWMINSIZ],       // column headings for procflgs
#else
          columnhdr [SCREENMAX],       // column headings for procflgs
#endif
         *captab [CAPTABMAX];          // captab needed by show_special()
   struct osel_s *osel_1st;            // other selection criteria anchor
   int    osel_tot;                    // total of other selection criteria
   char  *osel_prt;                    // other stuff printable as status line
   char  *findstr;                     // window's current/active search string
   int    findlen;                     // above's strlen, without call overhead
   proc_t **ppt;                       // this window's proc_t ptr array
   struct WIN_t *next,                 // next window in window stack
                *prev;                 // prior window in window stack
} WIN_t;

        // Used to test/manipulate the window flags
#define CHKw(q,f)    (int)((q)->rc.winflags & (f))
#define TOGw(q,f)    (q)->rc.winflags ^=  (f)
#define SETw(q,f)    (q)->rc.winflags |=  (f)
#define OFFw(q,f)    (q)->rc.winflags &= ~(f)
#define ALTCHKw      (Rc.mode_altscr ? 1 : win_warn(Warn_ALT))
#define VIZISw(q)    (!Rc.mode_altscr || CHKw(q,Show_TASKON))
#define VIZCHKw(q)   (VIZISw(q)) ? 1 : win_warn(Warn_VIZ)
#define VIZTOGw(q,f) (VIZISw(q)) ? TOGw(q,(f)) : win_warn(Warn_VIZ)

        // Used to test/manipulte fieldscur values
#define FLDon(c)     ((c) |= 0x80)
#define FLDget(q,i)  ((FLG_t)((q)->rc.fieldscur[i] & 0x7f) - FLD_OFFSET)
#define FLDtog(q,i)  ((q)->rc.fieldscur[i] ^= 0x80)
#define FLDviz(q,i)  ((q)->rc.fieldscur[i] &  0x80)
#define ENUviz(w,E)  (NULL != memchr((w)->procflgs, E, (w)->maxpflgs))
#define ENUpos(w,E)  ((int)((FLG_t*)memchr((w)->pflgsall, E, (w)->totpflgs) - (w)->pflgsall))

        // Support for variable width columns (and potentially scrolling too)
#define VARcol(E)    (-1 == Fieldstab[E].width)
#ifndef SCROLLVAR_NO
#ifdef USE_X_COLHDR
#define VARright(w)  (1 == w->maxpflgs && VARcol(w->procflgs[0]))
#else
#define VARright(w) ((1 == w->maxpflgs && VARcol(w->procflgs[0])) || \
                     (3 == w->maxpflgs && EU_XON == w->procflgs[0] && VARcol(w->procflgs[1])))
#endif
#define VARleft(w)   (w->varcolbeg && VARright(w))
#define SCROLLAMT    8
#endif

        /* Special Section: end ------------------------------------------ */
        /* /////////////////////////////////////////////////////////////// */


/*######  Some Miscellaneous Macro definitions  ##########################*/

        /* Yield table size as 'int' */
#define MAXTBL(t)  (int)(sizeof(t) / sizeof(t[0]))

        /* A null-terminating strncpy, assuming strlcpy is not available.
           ( and assuming callers don't need the string length returned ) */
#define STRLCPY(dst,src) { strncpy(dst, src, sizeof(dst)); dst[sizeof(dst) - 1] = '\0'; }

        /* Used to clear all or part of our Pseudo_screen */
#define PSU_CLREOS(y) memset(&Pseudo_screen[ROWMAXSIZ*y], '\0', Pseudo_size-(ROWMAXSIZ*y))

        /* Used as return arguments in *some* of the sort callbacks */
#define SORT_lt  ( Frame_srtflg > 0 ?  1 : -1 )
#define SORT_gt  ( Frame_srtflg > 0 ? -1 :  1 )
#define SORT_eq  0

        /* Used to create *most* of the sort callback functions
           note: some of the callbacks are NOT your father's callbacks, they're
                 highly optimized to save them ol' precious cycles! */
#define SCB_NAME(f) sort_EU_ ## f
#define SCB_NUM1(f,n) \
   static int SCB_NAME(f) (const proc_t **P, const proc_t **Q) { \
      if ( (*P)->n < (*Q)->n ) return SORT_lt; \
      if ( (*P)->n > (*Q)->n ) return SORT_gt; \
      return SORT_eq; }
#define SCB_NUM2(f,n1,n2) \
   static int SCB_NAME(f) (const proc_t **P, const proc_t **Q) { \
      if ( ((*P)->n1+(*P)->n2) < ((*Q)->n1+(*Q)->n2) ) return SORT_lt; \
      if ( ((*P)->n1+(*P)->n2) > ((*Q)->n1+(*Q)->n2) ) return SORT_gt; \
      return SORT_eq; }
#define SCB_NUMx(f,n) \
   static int SCB_NAME(f) (const proc_t **P, const proc_t **Q) { \
      return Frame_srtflg * ( (*Q)->n - (*P)->n ); }
#define SCB_STRS(f,s) \
   static int SCB_NAME(f) (const proc_t **P, const proc_t **Q) { \
      if (!(*P)->s || !(*Q)->s) return SORT_eq; \
      return Frame_srtflg * STRCMP((*Q)->s, (*P)->s); }
#define SCB_STRV(f,b,v,s) \
   static int SCB_NAME(f) (const proc_t **P, const proc_t **Q) { \
      if (b) { \
         if (!(*P)->v || !(*Q)->v) return SORT_eq; \
         return Frame_srtflg * STRCMP((*Q)->v[0], (*P)->v[0]); } \
      return Frame_srtflg * STRCMP((*Q)->s, (*P)->s); }
#define SCB_STRX(f,s) \
   int strverscmp(const char *s1, const char *s2); \
   static int SCB_NAME(f) (const proc_t **P, const proc_t **Q) { \
      if (!(*P)->s || !(*Q)->s) return SORT_eq; \
      return Frame_srtflg * strverscmp((*Q)->s, (*P)->s); }

/*
 * The following three macros are used to 'inline' those portions of the
 * display process involved in formatting, while protecting against any
 * potential embedded 'millesecond delay' escape sequences.
 */
        /**  PUTT - Put to Tty (used in many places)
               . for temporary, possibly interactive, 'replacement' output
               . may contain ANY valid terminfo escape sequences
               . need NOT represent an entire screen row */
#define PUTT(fmt,arg...) do { \
      char _str[ROWMAXSIZ]; \
      snprintf(_str, sizeof(_str), fmt, ## arg); \
      putp(_str); \
   } while (0)

        /**  PUFF - Put for Frame (used in only 3 places)
               . for more permanent frame-oriented 'update' output
               . may NOT contain cursor motion terminfo escapes
               . assumed to represent a complete screen ROW
               . subject to optimization, thus MAY be discarded */
#define PUFF(fmt,arg...) do { \
      char _str[ROWMAXSIZ], *_eol; \
      _eol = _str + snprintf(_str, sizeof(_str), fmt, ## arg); \
      if (Batch) { \
         while (*(--_eol) == ' '); *(++_eol) = '\0'; putp(_str); } \
      else { \
         char *_ptr = &Pseudo_screen[Pseudo_row * ROWMAXSIZ]; \
         if (Pseudo_row + 1 < Screen_rows) ++Pseudo_row; \
         if (!strcmp(_ptr, _str)) putp("\n"); \
         else { \
            strcpy(_ptr, _str); \
            putp(_ptr); } } \
   } while (0)

        /**  POOF - Pulled Out of Frame (used in only 1 place)
               . for output that is/was sent directly to the terminal
                 but would otherwise have been counted as a Pseudo_row */
#define POOF(str,cap) do { \
      putp(str); putp(cap); \
      Pseudo_screen[Pseudo_row * ROWMAXSIZ] = '\0'; \
      if (Pseudo_row + 1 < Screen_rows) ++Pseudo_row; \
   } while (0)

        /* Orderly end, with any sort of message - see fmtmk */
#define debug_END(s) { \
           void error_exit (const char *); \
           fputs(Cap_clr_scr, stdout); \
           error_exit(s); \
        }

        /* A poor man's breakpoint, if he's too lazy to learn gdb */
#define its_YOUR_fault { *((char *)0) = '!'; }


/*######  Display Support *Data*  ########################################*/
/*######  Some Display Support *Data*  ###################################*/
/*      ( see module top_nls.c for the nls translatable data ) */

        /* Configuration files support */
#define SYS_RCFILESPEC  "/etc/toprc"
#define RCF_EYECATCHER  "Config File (Linux processes with windows)\n"
#define RCF_VERSION_ID  'i'
#define RCF_PLUS_H      "\\]^_`abcdefghij"

        /* The default fields displayed and their order, if nothing is
           specified by the loser, oops user.
           note: any *contiguous* ascii sequence can serve as fieldscur
                 characters as long as the initial value is coordinated
                 with that specified for FLD_OFFSET
           ( we're providing for up to 70 fields currently, )
           ( with just one escaped value, the '\' character ) */
#define FLD_OFFSET  '%'
   //   seq_fields  "%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghij"
#ifdef ORIG_TOPDEFS
#define DEF_FIELDS  "¥¨³´»½ÀÄ·º¹Å&')*+,-./012568<>?ABCFGHIJKLMNOPQRSTUVWXYZ[" RCF_PLUS_H
#else
#define DEF_FIELDS  "¥&K¨³´»½@·º¹56ÄFÅ')*+,-./0128<>?ABCGHIJLMNOPQRSTUVWXYZ[" RCF_PLUS_H
#endif
        /* Pre-configured windows/field groups */
#define JOB_FIELDS  "¥¦¹·º(³´Ä»½@<§Å)*+,-./012568>?ABCFGHIJKLMNOPQRSTUVWXYZ[" RCF_PLUS_H
#define MEM_FIELDS  "¥º»<½¾¿ÀÁMBNÃD34·Å&'()*+,-./0125689FGHIJKLOPQRSTUVWXYZ[" RCF_PLUS_H
#define USR_FIELDS  "¥¦§¨ª°¹·ºÄÅ)+,-./1234568;<=>?@ABCFGHIJKLMNOPQRSTUVWXYZ[" RCF_PLUS_H
        // old top fields ( 'a'-'z' ) in positions 0-25
        // other suse old top fields ( '{|' ) in positions 26-27
#define CVT_FIELDS  "%&*'(-0346789:;<=>?@ACDEFGML)+,./125BHIJKNOPQRSTUVWXYZ["
#define CVT_FLDMAX  28


        /* The default values for the local config file */
#define DEF_RCFILE { \
   RCF_VERSION_ID, 0, 1, DEF_DELAY, 0, { \
   { EU_CPU, DEF_WINFLGS, 0, DEF_GRAPHS2, \
      COLOR_RED, COLOR_RED, COLOR_YELLOW, COLOR_RED, \
      "Def", DEF_FIELDS }, \
   { EU_PID, ALT_WINFLGS, 0, ALT_GRAPHS2, \
      COLOR_CYAN, COLOR_CYAN, COLOR_WHITE, COLOR_CYAN, \
      "Job", JOB_FIELDS }, \
   { EU_MEM, ALT_WINFLGS, 0, ALT_GRAPHS2, \
      COLOR_MAGENTA, COLOR_MAGENTA, COLOR_BLUE, COLOR_MAGENTA, \
      "Mem", MEM_FIELDS }, \
   { EU_UEN, ALT_WINFLGS, 0, ALT_GRAPHS2, \
      COLOR_YELLOW, COLOR_YELLOW, COLOR_GREEN, COLOR_YELLOW, \
      "Usr", USR_FIELDS } \
   }, 0, DEF_SCALES2, 0 }

        /* Summary Lines specially formatted string(s) --
           see 'show_special' for syntax details + other cautions. */
#define LOADAV_line  "%s -%s\n"
#define LOADAV_line_alt  "%s~6 -%s\n"


/*######  For Piece of mind  #############################################*/

        /* just sanity check(s)... */
#if defined(ATEOJ_RPTHSH) && defined(OFF_HST_HASH)
# error 'ATEOJ_RPTHSH' conflicts with 'OFF_HST_HASH'
#endif
#if defined(RECALL_FIXED) && defined(TERMIOS_ONLY)
# error 'RECALL_FIXED' conflicts with 'TERMIOS_ONLY'
#endif
#if defined(PRETEND_NUMA) && defined(NUMA_DISABLE)
# error 'PRETEND_NUMA' confilcts with 'NUMA_DISABLE'
#endif
#if defined(OFF_NUMASKIP) && defined(NUMA_DISABLE)
# error 'OFF_NUMASKIP' confilcts with 'NUMA_DISABLE'
#endif
#if (LRGBUFSIZ < SCREENMAX)
# error 'LRGBUFSIZ' must NOT be less than 'SCREENMAX'
#endif
#if defined(TERMIOS_ONLY)
# warning 'TERMIOS_ONLY' disables input recall and makes man doc incorrect
#endif
#if defined(MEMGRAPH_OLD)
# warning 'MEMGRAPH_OLD' will make the man document Section 2c. misleading
#endif


/*######  Some Prototypes (ha!)  #########################################*/

   /* These 'prototypes' are here exclusively for documentation purposes. */
   /* ( see the find_string function for the one true required protoype ) */
/*------  Sort callbacks  ------------------------------------------------*/
/*        for each possible field, in the form of:                        */
/*atic int           sort_EU_XXX (const proc_t **P, const proc_t **Q);    */
/*------  Tiny useful routine(s)  ----------------------------------------*/
//atic const char   *fmtmk (const char *fmts, ...);
//atic inline char  *scat (char *dst, const char *src);
//atic const char   *tg2 (int x, int y);
/*------  Exit/Interrput routines  ---------------------------------------*/
//atic void          at_eoj (void);
//atic void          bye_bye (const char *str);
//atic void          error_exit (const char *str);
//atic void          sig_abexit (int sig);
//atic void          sig_endpgm (int dont_care_sig);
//atic void          sig_paused (int dont_care_sig);
//atic void          sig_resize (int dont_care_sig);
/*------  Misc Color/Display support  ------------------------------------*/
//atic void          capsmk (WIN_t *q);
//atic void          show_msg (const char *str);
//atic int           show_pmt (const char *str);
//atic void          show_special (int interact, const char *glob);
//atic void          updt_scroll_msg (void);
/*------  Low Level Memory/Keyboard/File I/O support  --------------------*/
//atic void         *alloc_c (size_t num);
//atic void         *alloc_r (void *ptr, size_t num);
//atic char         *alloc_s (const char *str);
//atic inline int    ioa (struct timespec *ts);
//atic int           ioch (int ech, char *buf, unsigned cnt);
//atic int           iokey (int action);
//atic char         *ioline (const char *prompt);
//atic int           mkfloat (const char *str, float *num, int whole);
//atic int           readfile (FILE *fp, char **baddr, size_t *bsize, size_t *bread);
/*------  Small Utility routines  ----------------------------------------*/
//atic float         get_float (const char *prompt);
//atic int           get_int (const char *prompt);
//atic inline const char *hex_make (KLONG num, int noz);
//atic void          osel_clear (WIN_t *q);
//atic inline int    osel_matched (const WIN_t *q, FLG_t enu, const char *str);
//atic const char   *user_certify (WIN_t *q, const char *str, char typ);
//atic inline int    user_matched (const WIN_t *q, const proc_t *p);
/*------  Basic Formatting support  --------------------------------------*/
//atic inline const char *justify_pad (const char *str, int width, int justr);
//atic inline const char *make_chr (const char ch, int width, int justr);
//atic inline const char *make_num (long num, int width, int justr, int col, int noz);
//atic inline const char *make_str (const char *str, int width, int justr, int col);
//atic const char   *scale_mem (int target, unsigned long num, int width, int justr);
//atic const char   *scale_num (unsigned long num, int width, int justr);
//atic const char   *scale_pcnt (float num, int width, int justr);
//atic const char   *scale_tics (TIC_t tics, int width, int justr);
/*------  Fields Management support  -------------------------------------*/
/*atic FLD_t         Fieldstab[] = { ... }                                */
//atic void          adj_geometry (void);
//atic void          build_headers (void);
//atic void          calibrate_fields (void);
//atic void          display_fields (int focus, int extend);
//atic void          fields_utility (void);
//atic inline void   widths_resize (void);
//atic void          zap_fieldstab (void);
/*------  Library Interface  ---------------------------------------------*/
//atic CPU_t        *cpus_refresh (CPU_t *cpus);
#ifdef OFF_HST_HASH
//atic inline HST_t *hstbsrch (HST_t *hst, int max, int pid);
#else
//atic inline HST_t *hstget (int pid);
//atic inline void   hstput (unsigned idx);
#endif
//atic void          procs_hlp (proc_t *p);
//atic void          procs_refresh (void);
//atic void          sysinfo_refresh (int forced);
/*------  Inspect Other Output  ------------------------------------------*/
//atic void          insp_cnt_nl (void);
#ifndef INSP_OFFDEMO
//atic void          insp_do_demo (char *fmts, int pid);
#endif
//atic void          insp_do_file (char *fmts, int pid);
//atic void          insp_do_pipe (char *fmts, int pid);
//atic inline int    insp_find_ofs (int col, int row);
//atic void          insp_find_str (int ch, int *col, int *row);
//atic inline void   insp_make_row (int col, int row);
//atic void          insp_show_pgs (int col, int row, int max);
//atic int           insp_view_choice (proc_t *obj);
//atic void          inspection_utility (int pid);
/*------  Startup routines  ----------------------------------------------*/
//atic void          before (char *me);
//atic int           config_cvt (WIN_t *q);
//atic void          configs_read (void);
//atic void          parse_args (char **args);
//atic void          whack_terminal (void);
/*------  Windows/Field Groups support  ----------------------------------*/
//atic void          win_names (WIN_t *q, const char *name);
//atic void          win_reset (WIN_t *q);
//atic WIN_t        *win_select (int ch);
//atic int           win_warn (int what);
//atic void          wins_clrhlp (WIN_t *q, int save);
//atic void          wins_colors (void);
//atic void          wins_reflag (int what, int flg);
//atic void          wins_stage_1 (void);
//atic void          wins_stage_2 (void);
/*------  Interactive Input Tertiary support  ----------------------------*/
//atic inline int    find_ofs (const WIN_t *q, const char *buf);
//atic void          find_string (int ch);
//atic void          help_view (void);
//atic void          other_selection (int ch);
//atic void          write_rcfile (void);
/*------  Interactive Input Secondary support (do_key helpers)  ----------*/
//atic void          keys_global (int ch);
//atic void          keys_summary (int ch);
//atic void          keys_task (int ch);
//atic void          keys_window (int ch);
//atic void          keys_xtra (int ch);
/*------  Forest View support  -------------------------------------------*/
//atic void          forest_adds (const int self, int level);
#ifndef TREE_SCANALL
//atic int           forest_based (const proc_t **x, const proc_t **y);
#endif
//atic void          forest_create (WIN_t *q);
//atic inline const char *forest_display (const WIN_t *q, const proc_t *p);
/*------  Main Screen routines  ------------------------------------------*/
//atic void          do_key (int ch);
//atic void          summary_hlp (CPU_t *cpu, const char *pfx);
//atic void          summary_show (void);
//atic const char   *task_show (const WIN_t *q, const proc_t *p);
//atic int           window_show (WIN_t *q, int wmax);
/*------  Entry point plus two  ------------------------------------------*/
//atic void          frame_hlp (int wix, int max);
//atic void          frame_make (void);
//     int           main (int dont_care_argc, char **argv);

#endif /* _Itop */

