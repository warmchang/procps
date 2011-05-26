/* top.h - Header file:         show Linux processes */
/*
 * Copyright (c) 2002-2011, by: James C. Warner
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
 */
#ifndef _Itop
#define _Itop

        /* Development/Debugging defines ----------------------------------- */
//#define ATEOJ_RPTHSH            /* report on hash specifics, at end-of-job */
//#define ATEOJ_RPTSTD            /* report on misc stuff, at end-of-job     */
//#define CASEUP_HEXES            /* show any hex values in upper case       */
//#define CASEUP_SUFIX            /* show time/mem/cnts suffix in upper case */
//#define EQUCOLHDRYES            /* yes, do equalize column header lengths  */
//#define OFF_HST_HASH            /* use BOTH qsort+bsrch vs. hashing scheme */
//#define OFF_STDIOLBF            /* disable our own stdout _IOFBF override  */
//#define OOMEM_ENABLE            /* enable the SuSE out-of-memory additions *
//#define PRETEND2_5_X            /* pretend we're linux 2.5.x (for IO-wait) */
//#define PRETEND4CPUS            /* pretend we're smp with 4 ticsers (sic)  */
//#define PRETENDNOCAP            /* use a terminal without essential caps   */
//#define RCFILE_NOERR            /* rcfile errs silently default, vs. fatal */
//#define RMAN_IGNORED            /* don't consider auto right margin glitch */
//#define STRCMPNOCASE            /* use strcasecmp vs. strcmp when sorting  */
//#define TERMIOS_ONLY            /* just limp along with native input only  */
//#define USE_X_COLHDR            /* emphasize header vs. whole col, for 'x' */


/*######  Notes, etc.  ###################################################*/

        /* The following convention is used to identify those areas where
           adaptations for hotplugging are to be found ...
              *** hotplug_acclimated ***
           ( hopefully libproc will also be supportive of our efforts ) */

        /* And there are still some of these lurking here and there...
              FIXME - blah, blah... */

        /* For introducing inaugural cgroup support, thanks to:
              Jan Gorig <jgorig@redhat.com> - April, 2011 */


#ifdef PRETEND2_5_X
#define linux_version_code LINUX_VERSION(2,5,43)
#endif

#ifdef STRCMPNOCASE
#define STRSORTCMP  strcasecmp
#else
#define STRSORTCMP  strcmp
#endif

#ifdef OOMEM_ENABLE
        /* FIXME: perhaps making this a function in the suse version of
           sysinfo.c was a prelude to hotpluggable updates -- unfortunately,
           the return value is invariant as currently implemented! */
#define SMP_NUM_CPUS  smp_num_cpus()
#else
#define SMP_NUM_CPUS  smp_num_cpus
#endif


/*######  Some Miscellaneous constants  ##################################*/

        /* The default delay twix updates */
#define DEF_DELAY  3.0

        /* Length of time a 'message' is displayed (in microseconds) */
#define MSG_USLEEP  (useconds_t)1250000

        /* Specific process id monitoring support (command line only) */
#define MONPIDMAX  20

        /* Output override minimums (the -w switch and/or env vars) */
#define W_MIN_COL  3
#define W_MIN_ROW  3

        /* Miscellaneous buffers with liberal values and some other defines
           -- mostly just to pinpoint source code usage/dependancies */
#define SCREENMAX   512
   /* the above might seem pretty stingy, until you consider that with every
      one of top's fields displayed it's less than 200 bytes of column header
      -- so SCREENMAX provides for all fields plus a 300+ byte command line */
#define CAPBUFSIZ    32
#define CLRBUFSIZ    64
#define PFLAGSSIZ    64
#define SMLBUFSIZ   128
#define MEDBUFSIZ   256
#define LRGBUFSIZ   512
#define OURPATHSZ  1024
#define BIGBUFSIZ  2048
   /* in addition to the actual display data, our row might have to accomodate
      many termcap/color transitions - these definitions ensure we have room */
#define ROWMINSIZ  ( SCREENMAX +  4 * (CAPBUFSIZ + CLRBUFSIZ) )
#define ROWMAXSIZ  ( SCREENMAX + 16 * (CAPBUFSIZ + CLRBUFSIZ) )

   // support for keyboard stuff (cursor motion keystrokes, mostly)
#define kbd_ENTER  '\n'
#define kbd_ESC    '\033'
#define kbd_SPACE  ' '
#define kbd_UP     '\x81'
#define kbd_DOWN   '\x82'
#define kbd_RIGHT  '\x83'
#define kbd_LEFT   '\x84'
#define kbd_PGUP   '\x85'
#define kbd_PGDN   '\x86'
#define kbd_END    '\x87'
#define kbd_HOME   '\x88'
#define kbd_BKSP   '\x89'
#define kbd_INS    '\x8a'
#define kbd_DEL    '\x8b'


/* #####  Enum's and Typedef's  ############################################ */

        /* Flags for each possible field (and then some) --
           these MUST be kept in sync with the FLD_t Fieldstab[] array !! */
enum pflag {
   P_PID = 0, P_PPD,
   P_UED, P_UEN, P_URD, P_URN, P_USD, P_USN,
   P_GID, P_GRP, P_PGD, P_TTY, P_TPG, P_SID,
   P_PRI, P_NCE, P_THD,
   P_CPN, P_CPU, P_TME, P_TM2,
   P_MEM, P_VRT, P_SWP, P_RES, P_COD, P_DAT, P_SHR,
   P_FL1, P_FL2, P_DRT,
   P_STA, P_CMD, P_WCH, P_FLG, P_CGR,
#ifdef OOMEM_ENABLE
   P_OOA, P_OOM,
#endif
#ifdef USE_X_COLHDR
   // not really pflags, used with tbl indexing
   P_MAXPFLGS
#else
   // not really pflags, used with tbl indexing & col highlighting
   P_MAXPFLGS, X_XON, X_XOF
#endif
};

        /* The scaling 'type' used with scale_num() -- this is how
           the passed number is interpreted should scaling be necessary */
enum scale_num {
   SK_no, SK_Kb, SK_Mb, SK_Gb, SK_Tb
};

        /* This typedef just ensures consistent 'process flags' handling */
typedef unsigned FLG_t;

        /* These typedefs attempt to ensure consistent 'ticks' handling */
typedef unsigned long long TIC_t;
typedef          long long SIC_t;

        /* Sort support, callback function signature */
typedef int (*QFP_t)(const void *, const void *);

        /* This structure consolidates the information that's used
           in a variety of display roles. */
typedef struct FLD_t {
   const char   *head;          // name for col heads + toggle/reorder fields
   const char   *fmts;          // snprintf format string for field display
   const int     width;         // field width, if applicable
   const int     scale;         // scale_num type, if applicable
   const QFP_t   sort;          // sort function
   const int     lflg;          // PROC_FILLxxx flag(s) needed by this field
   const char   *desc;          // description for fields management
} FLD_t;

#ifdef OFF_HST_HASH
        /* This structure supports 'history' processing and ultimately records
           one piece of critical information from one frame to the next --
           we don't calc and save data that goes unused like the old top. */
typedef struct HST_t {
   TIC_t tics;                  // last frame's tics count
   int   pid;                   // record 'key'
} HST_t;
#else
        /* This structure supports 'history' processing and ultimately records
           one piece of critical information from one frame to the next --
           we don't calc and save data that goes unused like the old top nor
           do we incure the overhead of sorting to support a binary search
           (or worse, a friggin' for loop) when retrieval is necessary! */
typedef struct HST_t {
   TIC_t tics;                  // last frame's tics count
   int   pid;                   // record 'key'
   int   lnk;                   // next on hash chain
} HST_t;
#endif

        /* This structure stores a frame's cpu tics used in history
           calculations.  It exists primarily for SMP support but serves
           all environments. */
typedef struct CPU_t {
   /* other kernels: u == user/us, n == nice/ni, s == system/sy, i == idle/id
      2.5.41 kernel: w == IO-wait/wa (io wait time)
      2.6.0  kernel: x == hi (hardware irq time), y == si (software irq time)
      2.6.11 kernel: z == st (virtual steal time) */
   TIC_t u, n, s, i, w, x, y, z; // as represented in /proc/stat
   TIC_t u_sav, s_sav, n_sav, i_sav, w_sav, x_sav, y_sav, z_sav; // in the order of our display
   unsigned id;  // the CPU ID number
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
#define Qsrt_NORMAL  0x000004     // 'R' - reversed column sort (high to low)
        // these flag(s) have no command as such - they're for internal use
#define EQUWINS_xxx  0x000001     // rebalance all wins & tasks (off 'i'/ 'n')

        // Default flags if there's no rcfile to provide user customizations
#define DEF_WINFLGS ( View_LOADAV | View_STATES | View_CPUSUM | View_MEMORY \
   | Show_HIBOLD | Show_HIROWS | Show_IDLEPS | Show_TASKON | Qsrt_NORMAL )

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
   FLG_t  sortindx;             // sort field, represented as a procflag
   int    winflags,             // 'view', 'show' and 'sort' mode flags
          maxtasks,             // user requested maximum, 0 equals all
          summclr,                      // color num used in summ info
          msgsclr,                      //        "       in msgs/pmts
          headclr,                      //        "       in cols head
          taskclr;                      //        "       in task rows
   char   winname [WINNAMSIZ],          // window name, user changeable
          fieldscur [PFLAGSSIZ];        // fields displayed and ordered
} RCW_t;

        /* This represents the complete rcfile */
typedef struct RCF_t {
   int    mode_altscr;          // 'A' - Alt display mode (multi task windows)
   int    mode_irixps;          // 'I' - Irix vs. Solaris mode (SMP-only)
   float  delay_time;           // 'd'/'s' - How long to sleep twixt updates
   int    win_index;            // Curwin, as index
   RCW_t  win [GROUPSMAX];      // a 'WIN_t.rc' for each window
} RCF_t;

        /* This structure stores configurable information for each window.
           By expending a little effort in its creation and user requested
           maintainence, the only real additional per frame cost of having
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
          varcolsz,        // max length of variable width column(s)
          usrseluid,       // validated uid for 'u/U' user selection
          usrseltyp,       // the basis for matching above uid
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
         *eolcap,                      // window specific eol termcap
         *captab [CAPTABMAX];          // captab needed by show_special()
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
#define FLDget(q,i)  ((FLG_t)((q)->rc.fieldscur[i] & 0x7f) - FLD_OFFSET)
#define FLDtog(q,i)  ((q)->rc.fieldscur[i] ^= 0x80)
#define FLDviz(q,i)  ((q)->rc.fieldscur[i] &  0x80)

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
#define SCB_NAME(f) sort_P_ ## f
#define SCB_NUM1(f,n) \
   static int SCB_NAME(f) (const proc_t **P, const proc_t **Q) { \
      if ( (*P)->n < (*Q)->n ) return SORT_lt; \
      if ( (*P)->n > (*Q)->n ) return SORT_gt; \
      return SORT_eq; }
#define SCB_NUM2(f,n1,n2) \
   static int SCB_NAME(f) (const proc_t **P, const proc_t **Q) { \
      if ( ((*P)->n1 - (*P)->n2) < ((*Q)->n1 - (*Q)->n2) ) return SORT_lt; \
      if ( ((*P)->n1 - (*P)->n2) > ((*Q)->n1 - (*Q)->n2) ) return SORT_gt; \
      return SORT_eq; }
#define SCB_NUMx(f,n) \
   static int SCB_NAME(f) (const proc_t **P, const proc_t **Q) { \
      return Frame_srtflg * ( (*Q)->n - (*P)->n ); }
#define SCB_STRS(f,s) \
   static int SCB_NAME(f) (const proc_t **P, const proc_t **Q) { \
      return Frame_srtflg * STRSORTCMP((*Q)->s, (*P)->s); }
#define SCB_STRV(f,b,v,s) \
   static int SCB_NAME(f) (const proc_t **P, const proc_t **Q) { \
      if (b) { \
         if (!(*P)->v || !(*Q)->v) return SORT_eq; \
         return Frame_srtflg * STRSORTCMP((*Q)->v[0], (*P)->v[0]); } \
      return Frame_srtflg * STRSORTCMP((*Q)->s, (*P)->s); }

/*
 * The following two macros are used to 'inline' those portions of the
 * display process requiring formatting, while protecting against any
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
      char _str[ROWMAXSIZ]; \
      snprintf(_str, sizeof(_str), fmt, ## arg); \
      if (Batch) putp(_str); \
      else { \
         char *_ptr = &Pseudo_screen[Pseudo_row * ROWMAXSIZ]; \
         if (Pseudo_row + 1 < Screen_rows) ++Pseudo_row; \
         if (!strcmp(_ptr, _str)) putp("\n"); \
         else { \
            strcpy(_ptr, _str); \
            putp(_ptr); } } \
   } while (0)

        /* Orderly end, with any sort of message - see fmtmk */
#define debug_END(s) { \
           static void error_exit (const char *); \
           fputs(Cap_clr_scr, stdout); \
           error_exit(s); \
        }

        /* A poor man's breakpoint, if he's too lazy to learn gdb */
#define its_YOUR_fault { *((char *)0) = '!'; }


/*######  Display Support *Data*  ########################################*/

        /* Configuration files support */
#define SYS_RCFILESPEC  "/etc/toprc"
#define RCF_EYECATCHER  "Config File (Linux processes with windows)\n"
#define RCF_VERSION_ID  'f'

        /* The default fields displayed and their order, if nothing is
           specified by the loser, oops user.
           note: any *contiguous* ascii sequence can serve as fieldscur
                 characters as long as the initial value is coordinated
                 with that specified for FLD_OFFSET
           ( we're providing for up to 55 fields initially, )
           ( with values chosen to avoid the need to escape ) */
#define FLD_OFFSET  '%'
   //   seq_fields  "%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ["
#define DEF_FIELDS  "¥¨³´»½ÀÄ·º¹Å&')*+,-./012568<>?ABCFGHIJKLMNOPQRSTUVWXYZ["
        /* Pre-configured windows/field groups */
#define JOB_FIELDS  "¥¦¹·º³´Ä»¼½§Å()*+,-./012568>?@ABCFGHIJKLMNOPQRSTUVWXYZ["
#define MEM_FIELDS  "¥º»¼½¾¿ÀÁÃÄ³´·Å&'()*+,-./0125689BFGHIJKLMNOPQRSTUVWXYZ["
#define USR_FIELDS  "¥¦§¨ª°¹·ºÄÅ)+,-./1234568;<=>?@ABCFGHIJKLMNOPQRSTUVWXYZ["

        /* The default values for the local config file */
#define DEF_RCFILE { \
   0, 1, DEF_DELAY, 0, { \
   { P_CPU, DEF_WINFLGS, 0, \
      COLOR_RED, COLOR_RED, COLOR_YELLOW, COLOR_RED, \
      "Def", DEF_FIELDS }, \
   { P_PID, DEF_WINFLGS, 0, \
      COLOR_CYAN, COLOR_CYAN, COLOR_WHITE, COLOR_CYAN, \
      "Job", JOB_FIELDS }, \
   { P_MEM, DEF_WINFLGS, 0, \
      COLOR_MAGENTA, COLOR_MAGENTA, COLOR_BLUE, COLOR_MAGENTA, \
      "Mem", MEM_FIELDS }, \
   { P_UEN, DEF_WINFLGS, 0, \
      COLOR_YELLOW, COLOR_YELLOW, COLOR_GREEN, COLOR_YELLOW, \
      "Usr", USR_FIELDS } \
   } }

        /* The format string used with variable width columns --
           see 'calibrate_fields' for supporting logic. */
#define VARCOL_fmts  "%-*.*s "

        /* Summary Lines specially formatted string(s) --
           see 'show_special' for syntax details + other cautions. */
#define LOADAV_line  "%s -%s\n"
#define LOADAV_line_alt  "%s\06 -%s\n"
#define STATES_line1  "%s:\03" \
   " %3u \02total,\03 %3u \02running,\03 %3u \02sleeping,\03 %3u \02stopped,\03 %3u \02zombie\03\n"
#define STATES_line2x4  "%%%s\03" \
   " %#5.1f  \02user,\03 %#5.1f  \02system,\03 %#5.1f  \02nice,\03 %#5.1f  \02idle\03\n"
        /* These are the STATES_line evolutions
              lnx 2.5.x, procps-3.0.5  : IO-wait = i/o wait time
              lnx 2.6.x, procps-3.1.12 : IO-wait now wa, hi = hard irq, si = soft irq
              lnx 2.7.x, procps-3.2.7  : st = steal time */
#define STATES_line2x5  "%%%s\03" \
   " %#5.1f  \02user,\03 %#5.1f  \02system,\03 %#5.1f  \02nice,\03 %#5.1f  \02idle,\03 %#5.1f  \02IO-wait\03\n"
#define STATES_line2x6  "%%%s\03" \
   " %#5.1f \02us,\03 %#5.1f \02sy,\03 %#5.1f \02ni,\03 %#5.1f \02id,\03 %#5.1f \02wa,\03 %#5.1f \02hi,\03 %#5.1f \02si\03\n"
#define STATES_line2x7  "%%%s\03" \
   "%#5.1f \02us,\03%#5.1f \02sy,\03%#5.1f \02ni,\03%#5.1f \02id,\03%#5.1f \02wa,\03%#5.1f \02hi,\03%#5.1f \02si,\03%#5.1f \02st\03\n"
#define MEMORY_twolines  \
   "%s Mem: \03 %8lu \02total,\03 %8lu \02used,\03 %8lu \02free,\03 %8lu \02buffers\03\n" \
   "%s Swap:\03 %8lu \02total,\03 %8lu \02used,\03 %8lu \02free,\03 %8lu \02cached\03\n"

        /* Keyboard Help specially formatted string(s) --
           see 'show_special' for syntax details + other cautions. */
#define KEYS_help \
   "Help for Interactive Commands\02 - %s\n" \
   "Window \01%s\06: \01Cumulative mode \03%s\02.  \01System\06: \01Delay \03%.1f secs\02; \01Secure mode \03%s\02.\n" \
   "\n" \
   "  Z\05,\01B\05       Global: '\01Z\02' change color mappings; '\01B\02' disable/enable bold\n" \
   "  l,t,m     Toggle Summaries: '\01l\02' load avg; '\01t\02' task/cpu stats; '\01m\02' mem info\n" \
   "  1,I       Toggle SMP view: '\0011\02' single/separate states; '\01I\02' Irix/Solaris mode\n" \
   "  f,F       Manage Fields: add/remove; change order; select sort field\n" \
   "\n" \
   "  <,>     . Move sort field: '\01<\02' next col left; '\01>\02' next col right\n" \
   "  R,H     . Toggle: '\01R\02' normal/reverse sort; '\01H\02' show threads\n" \
   "  c,i,S   . Toggle: '\01c\02' cmd name/line; '\01i\02' idle tasks; '\01S\02' cumulative time\n" \
   "  x\05,\01y\05     . Toggle highlights: '\01x\02' sort field; '\01y\02' running tasks\n" \
   "  z\05,\01b\05     . Toggle: '\01z\02' color/mono; '\01b\02' bold/reverse (only if 'x' or 'y')\n" \
   "  u,U     . Show: '\01u\02' effective user; '\01U\02' real, saved, file or effective user\n" \
   "  n or #  . Set maximum tasks displayed\n" \
   "  C,...   . Toggle scroll coordinates msg for: \01up\02,\01down\02,\01left\02,right\02,\01home\02,\01end\02\n" \
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

        /* Fields Management specially formatted string(s) --
           see 'show_special' for syntax details + other cautions */
#define FIELDS_heading \
   "Fields Management\02 for window \01%s\06, whose current sort field is \01%s\02\n" \
   "   Navigate with Up/Dn, Right selects for move then <Enter> or Left commits,\n" \
   "   'd' or <Space> toggles display, 's' sets sort.  Use 'q' or <Esc> to end! " \
   ""

        /* Colors Help specially formatted string(s) --
           see 'show_special' for syntax details + other cautions. */
#define COLOR_help \
   "Help for color mapping\02 - %s\n" \
   "current window: \01%s\06\n" \
   "\n" \
   "   color - 04:25:44 up 8 days, 50 min,  7 users,  load average:\n" \
   "   Tasks:\03  64 \02total,\03   2 \03running,\03  62 \02sleeping,\03   0 \02stopped,\03\n" \
   "   %%Cpu(s):\03  76.5 \02user,\03  11.2 \02system,\03   0.0 \02nice,\03  12.3 \02idle\03\n" \
   "   \01 Nasty Message! \04  -or-  \01Input Prompt\05\n" \
   "   \01  PID TTY     PR  NI %%CPU    TIME+   VIRT SWAP S COMMAND    \06\n" \
   "   17284 \10pts/2  \07  8   0  0.0   0:00.75  1380    0 S /bin/bash   \10\n" \
   "   \01 8601 pts/1    7 -10  0.4   0:00.03   916    0 R color -b -z\07\n" \
   "   11005 \10?      \07  9   0  0.0   0:02.50  2852 1008 S amor -sessi\10\n" \
   "   available toggles: \01B\02 =disable bold globally (\01%s\02),\n" \
   "       \01z\02 =color/mono (\01%s\02), \01b\02 =tasks \"bold\"/reverse (\01%s\02)\n" \
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
   "  a specific field group; or\01 3\02) exiting the color mapping or fields screens\n" \
   ". Commands \01available anytime   -------------\02\n" \
   "    A       . Alternate display mode toggle, show \01Single\02 / \01Multiple\02 windows\n" \
   "    g       . Choose another field group and make it 'current', or change now\n" \
   "              by selecting a number from: \01 1\02 =%s;\01 2\02 =%s;\01 3\02 =%s; or\01 4\02 =%s\n" \
   ". Commands \01requiring\02 '\01A\02' mode\01  -------------\02\n" \
   "    G       . Change the \01Name\05 of the 'current' window/field group\n" \
   " \01*\04  a , w   . Cycle through all four windows:  '\01a\05' Forward; '\01w\05' Backward\n" \
   " \01*\04  - , _   . Show/Hide:  '\01-\05' \01Current\02 window; '\01_\05' all \01Visible\02/\01Invisible\02\n" \
   "  The screen will be divided evenly between task displays.  But you can make\n" \
   "  some \01larger\02 or \01smaller\02, using '\01n\02' and '\01i\02' commands.  Then later you could:\n" \
   " \01*\04  = , +   . Rebalance tasks:  '\01=\05' \01Current\02 window; '\01+\05' \01Every\02 window\n" \
   "              (this also forces the \01current\02 or \01every\02 window to become visible)\n" \
   "\n" \
   "In '\01A\02' mode, '\01*\04' keys are your \01essential\02 commands.  Please try the '\01a\02' and '\01w\02'\n" \
   "commands plus the 'g' sub-commands NOW.  Press <Enter> to make 'Current' " \
   ""


/*######  For Piece of mind  #############################################*/

        /* just sanity check(s)... */
#if defined(ATEOJ_RPTHSH) && defined(OFF_HST_HASH)
# error 'ATEOJ_RPTHSH' conflicts with 'OFF_HST_HASH'
#endif
#if defined(PRETEND4CPUS) && defined (OOMEM_ENABLE)
# error 'PRETEND4CPUS' conflicts with 'OOMEM_ENABLE'
#endif
#if (LRGBUFSIZ < SCREENMAX)
# error 'LRGBUFSIZ' must NOT be less than 'SCREENMAX'
#endif


/*######  Some Prototypes (ha!)  #########################################*/

   /* These 'prototypes' are here solely for documentation purposes */
/*------  Sort callbacks  ------------------------------------------------*/
/*        for each possible field, in the form of:                        */
/*atic int           sort_P_XXX (const proc_t **P, const proc_t **Q);     */
/*------  Tiny useful routine(s)  ----------------------------------------*/
//atic const char   *fmtmk (const char *fmts, ...);
//atic inline char  *scat (char *dst, const char *src);
//atic char         *strim (char *str);
//atic const char   *tg2 (int x, int y);
/*------  Exit/Interrput routines  ---------------------------------------*/
//atic void          bye_bye (const char *str);
//atic void          error_exit (const char *str);
//atic void          pause_pgm (void);
//atic void          sig_endpgm (int dont_care_sig);
//atic void          sig_paused (int dont_care_sig);
//atic void          sig_resize (int dont_care_sig);
/*------  Misc Color/Display support  ------------------------------------*/
//atic void          capsmk (WIN_t *q);
//atic void          msg_save (const char *fmts, ...);
//atic void          show_msg (const char *str);
//atic int           show_pmt (const char *str);
//atic inline void   show_scroll (void);
//atic void          show_special (int interact, const char *glob);
/*------  Low Level Memory/Keyboard support  -----------------------------*/
//atic void         *alloc_c (size_t num);
//atic void         *alloc_r (void *ptr, size_t num);
//atic int           chin (int ech, char *buf, unsigned cnt);
//atic int           keyin (int init);
//atic char         *linein (const char *prompt);
/*------  Small Utility routines  ----------------------------------------*/
//atic float         get_float (const char *prompt);
//atic int           get_int (const char *prompt);
//atic const char   *scale_num (unsigned long num, const int width, const int type);
//atic const char   *scale_tics (TIC_t tics, const int width);
//atic const char   *user_certify (WIN_t *q, const char *str, char typ);
//atic inline int    user_matched (WIN_t *q, const proc_t *p);
/*------  Fields Management support  -------------------------------------*/
/*atic FLD_t         Fieldstab[] = { ... }                                */
//atic void          adj_geometry (void);
//atic void          calibrate_fields (void);
//atic void          display_fields (int focus, int extend);
//atic void          fields_utility (void);
//atic void          zap_fieldstab (void);
/*------  Library Interface  ---------------------------------------------*/
//atic CPU_t        *cpus_refresh (CPU_t *cpus);
#ifdef OFF_HST_HASH
//atic inline HST_t *hstbsrch (HST_t *hst, int max, int pid);
#else
//atic inline HST_t *hstget (int pid);
//atic inline void   hstput (unsigned idx);
#endif
//atic void          prochlp (proc_t *p);
//atic proc_t      **procs_refresh (proc_t **ppt);
/*------  Startup routines  ----------------------------------------------*/
//atic void          before (char *me);
//atic void          configs_read (void);
//atic void          parse_args (char **args);
//atic void          whack_terminal (void);
/*------  Windows/Field Groups support  ----------------------------------*/
//atic void          win_names (WIN_t *q, const char *name);
//atic WIN_t        *win_select (char ch);
//atic int           win_warn (int what);
//atic void          winsclrhlp (WIN_t *q, int save);
//atic void          wins_colors (void);
//atic void          wins_reflag (int what, int flg);
//atic void          wins_stage_1 (void);
//atic void          wins_stage_2 (void);
/*------  Interactive Input support (do_key helpers)  --------------------*/
//atic void          file_writerc (void);
//atic void          help_view (void);
//atic void          keys_global (int ch);
//atic void          keys_summary (int ch);
//atic void          keys_task (int ch);
//atic void          keys_window (int ch);
//atic void          keys_xtra (int ch);
/*------  Main Screen routines  ------------------------------------------*/
//atic void          do_key (int ch);
//atic void          summaryhlp (CPU_t *cpu, const char *pfx);
//atic proc_t      **summary_show (void);
//atic void          task_show (const WIN_t *q, const proc_t *p);
//atic int           window_show (proc_t **ppt, WIN_t *q, int wmax);
/*------  Entry point plus two  ------------------------------------------*/
//atic void          framehlp (int wix, int max);
//atic void          frame_make (void);
//     int           main (int dont_care_argc, char **argv);

#endif /* _Itop */

