/* top.c - Source file:         show Linux processes */
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
#include <asm/param.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <term.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <values.h>
   /*
      I am listing precisely why each header is needed because of the
      construction of libproc -- separate header files may not always be
      available and function names are not normalized.  We have avoided
      some library routine(s) as overkill and have subsumed some others.
   */
        /* need: 1 define + dev_to_tty */
#include "proc/devname.h"
        /* need: (ksym.c) open_psdb_message, wchan, close_psdb (redhat only) */
#include "proc/procps.h"
        /* need: 2 types + openproc, readproc, closeproc */
#include "proc/readproc.h"
#ifdef UGH_ITS_4_RH
        /* need: get_signal2 */
#include "proc/signals.h"
#else
        /* need: signal_name_to_number */
#include "proc/sig.h"
#endif
        /* need: status */
#include "proc/status.h"
        /* need: meminfo stuff */
#include "proc/sysinfo.h"
        /* need: procps_version */
#include "proc/version.h"
        /* need: sprint_uptime */
#include "proc/whattime.h"

#include "top.h"


/*######  Miscellaneous global stuff  ####################################*/

        /* The original and new terminal attributes */
static struct termios Savedtty,
                      Rawtty;
static int  Ttychanged = 0;

        /* Program names used in error messages and 'rc' file names */
static char *Myname,
            *Myrealname;

        /* The Name of the config file(s), dynamically constructed */
static char  RCfile     [OURPATHSZ],
             RCfile_Sys [SMLBUFSIZ];

        /* The run-time acquired page size */
static int  Page_size;

        /* SMP and Irix/Solaris mode support */
static int  Cpu_tot,
           *Cpu_map;

        /* Specific process id monitoring support */
static pid_t  Monpids [MONPIDMAX] = { 0 };
static int    Monpidsidx = 0;

        /* A postponed error message */
static char  Msg_delayed [SMLBUFSIZ];
static int   Msg_awaiting = 0;

        /* Configurable Display support ##################################*/

static unsigned  PFlags      [PFLAGSSIZ];
static char      CurFields   [PFLAGSSIZ],
                 ColHeadings [SMLBUFSIZ],
                 ColUsername [USRNAMSIZ];
static int       NumFields;

        /* Current window size.
           note: Max_tasks is the specified number of processes to display
                 (zero for infinite), and Max_lines is the actual number
                 which, in turn, depends on screen size and summary info. */
static int  Screen_cols, Screen_rows,
            Max_lines, Max_tasks;

        /* Number of lines needed to display the summary information
           which is never less than 2 since it also must accomodate the
           message line + column headings -- set soley by mkheadings! */
static int  HSum_lines;

        /* Maximum length to display of a process' command name/line
           (excluding padding due to terminfo strings) */
static int  Max_cmd;

        /* Controls how long we sleep between screen updates.
           Accurate to microseconds (when not in 'Batch' mode). */
static float  Delay_time = DEF_DELAY;

        /* Special flags dealing with the kernel symbol table */
static int  No_ksyms = -1,      /* set to '0' if ksym avail, '1' otherwise   */
            PSDBopen = 0;       /* set to '1' if psdb opened (now postponed) */

        /* The Mode flags.
           Except for Batch and Loops, all of them are preserved in the
           rc file.  Thus, once personalized it stays personalized!
           note: the 'letter' shown is the On condition, some default to Off */
static int  Batch = 0,          /* batch mode, collect no input, dumb output */
            Loops = -1,         /* number of iterations, -1 loops forever    */
            Secure_mode = 0,    /* set if some functionality restricted      */
            Irix_mode   = 1,    /* 'I' - Irix vs. Solaris view (SMP-only)    */
            Sort_normal = 1,    /* 'R' - reversed column sort order          */
            HSum_loadav = 1,    /* 'l' - display load avg and uptime summary */
            HSum_states = 1,    /* 't' - display task/cpu(s) states summary  */
            HSum_memory = 1,    /* 'm' - display memory summary              */
            Show_ctimes = 1,    /* 'S' - show times as cumulative            */
            Show_cmdlin = 0,    /* 'c' - show cmdline vs. name               */
            Show_idleps = 1,    /* 'i' - show idle processes (all processes) */
            Show_hicols = 0,    /* 'x' - show sort column highlighted        */
            Show_hirows = 1,    /* 'y' - show running task(s) highlighted    */
            Show_colors = 0,    /* 'z' - show in color (vs. mono)            */
            Show_hibold = 1,    /* 'b' - rows and/or col bold (vs. reverse)  */
            Show_cpusum = 1;    /* '1' - show combined cpu stats (vs. each)  */

        /* Current current sort type (this too is preserved in the rc file)
         * and handler -- goodbye libproc mult_lvl_cmp, etc. overkill. */
static int     Sort_type = -1;
static QSORT_t Sort_func = NULL;

        /* These are our gosh darn 'Fields' !
           They MUST be kept in sync with pflags !! */
static FTAB_t  Fieldstab[] = {
/*   head           fmts     width   scale   sort    desc
     -----------    -------  ------  ------  ------  ---------------------- */
   { "  PID ",      "%5d ",     -1,     -1,  S_PID,  "Process Id"           },
   { " PPID ",      "%5d ",     -1,     -1,     -1,  "Parent Process Pid"   },
   { " UID ",       "%4d ",     -1,     -1,     -1,  "User Id"              },
   { "USER     ",   "%-8.8s ",  -1,     -1,  S_USR,  "User Name"            },
   { "GROUP    ",   "%-8.8s ",  -1,     -1,     -1,  "Group Name"           },
   { "TTY      ",   "%-8.8s ",   8,     -1,  S_TTY,  "Controlling Tty"      },
   { " PR ",        "%3ld ",    -1,     -1,     -1,  "Priority"             },
   { " NI ",        "%3ld ",    -1,     -1,     -1,  "Nice value"           },
   { "#C ",         "%2d ",     -1,     -1,     -1,  "Last used cpu (SMP)"  },
   { "%CPU ",       "%#4.1f ",  -1,     -1,  S_CPU,  "CPU usage"            },
   { "  TIME ",     "%6.6s ",    6,     -1,  S_TME,  "CPU Time"             },
   { "   TIME+  ",  "%9.9s ",    9,     -1,  S_TME,  "CPU Time, hundredths" },
   { "%MEM ",       "%#4.1f ",  -1,     -1,  S_MEM,  "Memory usage (RES)"   },
   { " VIRT ",      "%5.5s ",    5,  SK_Kb,     -1,  "Virtual Image (kb)"   },
   { "SWAP ",       "%4.4s ",    4,  SK_Kb,     -1,  "Swapped size (kb)"    },
   { " RES ",       "%4.4s ",    4,  SK_Kb,  S_MEM,  "Resident size (kb)"   },
   { "CODE ",       "%4.4s ",    4,  SK_Kb,     -1,  "Code size (kb)"       },
   { "DATA ",       "%4.4s ",    4,  SK_Kb,     -1,  "Data+Stack size (kb)" },
   { " SHR ",       "%4.4s ",    4,  SK_Kb,     -1,  "Shared Mem size (kb)" },
   { "nFLT ",       "%4.4s ",    4,  SK_no,     -1,  "Page Fault count"     },
   { "nDRT ",       "%4.4s ",    4,  SK_no,     -1,  "Dirty Pages count"    },
   { "STA ",        "%3.3s ",   -1,     -1,     -1,  "Process Status"       },
   /** next entry's special: '.head' will be formatted using table entry's own
                             '.fmts' plus runtime supplied conversion args! */
   { "Command ",    "%-*.*s ",  -1,     -1,  S_CMD,  "Command line/name"    },
   { "WCHAN     ",  "%-9.9s ",  -1,     -1,     -1,  "Sleeping in Function" },
   /** next entry's special: the 0's will be replaced with '.'! */
#ifdef UPCASE_HEXES
   { "Flags    ",   "%08lX ",   -1,     -1,     -1,  "Task Flags <sched.h>" }
#else
   { "Flags    ",   "%08lx ",   -1,     -1,     -1,  "Task Flags <sched.h>" }
#endif
};

        /* Miscellaneous Color stuff #####################################*/

        /* Our three basic colors --
           they can be manipulated by the 'tweak_colors' routine */
static int  Base_color = BASEcolor,
            Msgs_color = MSGScolor,
            Head_color = HEADcolor;

        /* Some cap's stuff to reduce runtime calls --
           to accomodate 'Batch' mode, they begin life as empty strings */
static char  Cap_bold       [CAPBUFSIZ] = "",
             Cap_clr_eol    [CAPBUFSIZ] = "",
             Cap_clr_eos    [CAPBUFSIZ] = "",
             Cap_clr_scr    [CAPBUFSIZ] = "",
             Cap_curs_norm  [CAPBUFSIZ] = "",
             Cap_curs_huge  [CAPBUFSIZ] = "",
             Cap_home       [CAPBUFSIZ] = "",
             Cap_norm       [CAPBUFSIZ] = "",
             Cap_reverse    [CAPBUFSIZ] = "",
             Caps_off       [CAPBUFSIZ] = "";
static char  Sum_color      [CLRBUFSIZ] = "",
             Msg_color      [CLRBUFSIZ] = "",
             Pmt_color      [CLRBUFSIZ] = "",
             Hdr_color      [CLRBUFSIZ] = "",
             Row_color_norm [CLRBUFSIZ] = "",
             Row_color_high [CLRBUFSIZ] = "";
static int   Cap_can_goto = 0,
             Len_row_high = 0,
             Len_row_norm = 0;

        /* Development/debug stuff #######################################*/

#ifdef TRAK_MAXCAPS
static int  Max_pads = 0, Min_pads = MAXINT,
            Max_rbuf = 0, Min_rbuf = MAXINT;
#endif

#ifdef TRAK_MAXBUFS
   BUF2INT(fmtmk, buf)
   BUF2INT(show_special, tmp)
   BUF2INT(ask_str, buf)
   BUF2INT(scale_num, buf)
   BUF2INT(scale_tics, buf)
   BUF2INT(std_err, buf)
   BUF2INT(frame_states, tmp)
   BUF2INT(mkcol, tmp)
   BUF2INT(show_a_task, cbuf)
   BUF2INT(show_a_task, rbuf)
   BUF2INT(rcfiles_read, fbuf)
   BUF2INT(rcfiles_read, RCfile)
   BUF2INT(rcfiles_read, RCfile_Sys)
   BUF2INT(do_key, ColUsername)
   BUF2INT(mkheadings, CurFields)
   BUF2INT(mkheadings, ColHeadings)
   BUF2INT(main, not_really_tmp)
#endif


/*######  Sort callbacks  ################################################*/

static int sort_cmd (proc_t **P, proc_t **Q)
{
   /* if a process doesn't have a cmdline, we'll consider it a kernel thread
      -- since show_a_task gives such tasks special treatment, we must too */
   if (Show_cmdlin && ((*P)->cmdline || (*Q)->cmdline)) {
      if (!(*P)->cmdline) return SORT_lt;
      if (!(*Q)->cmdline) return SORT_gt;
      if ( 0 > strncmp((*P)->cmdline[0], (*Q)->cmdline[0], (unsigned)Max_cmd) )
         return SORT_lt;
      if ( 0 < strncmp((*P)->cmdline[0], (*Q)->cmdline[0], (unsigned)Max_cmd) )
         return SORT_gt;
   } else {
      /* this part also handles the compare if both are kernel threads */
      if ( 0 > strcmp((*P)->cmd, (*Q)->cmd) ) return SORT_lt;
      if ( 0 < strcmp((*P)->cmd, (*Q)->cmd) ) return SORT_gt;
   }
   return 0;
}


static int sort_cpu (proc_t **P, proc_t **Q)
{
   if ( (*P)->pcpu < (*Q)->pcpu ) return SORT_lt;
   if ( (*P)->pcpu > (*Q)->pcpu ) return SORT_gt;
   return 0;
}


static int sort_mem (proc_t **P, proc_t **Q)
{
   if ( (*P)->resident < (*Q)->resident ) return SORT_lt;
   if ( (*P)->resident > (*Q)->resident ) return SORT_gt;
      /* still equal?  we'll try to fix that... */
   if ( (*P)->size < (*Q)->size ) return SORT_lt;
   if ( (*P)->size > (*Q)->size ) return SORT_gt;
   return 0;
}


static int sort_pid (proc_t **P, proc_t **Q)
{
   if ( (*P)->pid < (*Q)->pid ) return SORT_lt;
   if ( (*P)->pid > (*Q)->pid ) return SORT_gt;
   return 0;
}


static int sort_tme (proc_t **P, proc_t **Q)
{
   if (Show_ctimes) {
      if ( ((*P)->cutime + (*P)->cstime + (*P)->utime + (*P)->stime)
        < ((*Q)->cutime + (*Q)->cstime + (*Q)->utime + (*Q)->stime) )
           return SORT_lt;
      if ( ((*P)->cutime + (*P)->cstime + (*P)->utime + (*P)->stime)
        > ((*Q)->cutime + (*Q)->cstime + (*Q)->utime + (*Q)->stime) )
           return SORT_gt;
   } else {
      if ( ((*P)->utime + (*P)->stime) < ((*Q)->utime + (*Q)->stime))
         return SORT_lt;
      if ( ((*P)->utime + (*P)->stime) > ((*Q)->utime + (*Q)->stime))
         return SORT_gt;
   }
   return 0;
}


static int sort_tty (proc_t **P, proc_t **Q)
{
   if ( (*P)->tty < (*Q)->tty ) return SORT_lt;
   if ( (*P)->tty > (*Q)->tty ) return SORT_gt;
      /* still equal?  we'll try to fix that... */
   return sort_pid(P, Q);
}


static int sort_usr (proc_t **P, proc_t **Q)
{
   if ( 0 > strcmp((*P)->euser, (*Q)->euser) ) return SORT_lt;
   if ( 0 < strcmp((*P)->euser, (*Q)->euser) ) return SORT_gt;
      /* still equal?  we'll try to fix that... */
   return sort_pid(P, Q);
}


/*######  Tiny useful routine(s)  ########################################*/

        /*
         * This routine isolates ALL user input and ensures that we
         * wont be mixing I/O from stdio and low-level read() requests */
static int chin (int ech, char *buf, unsigned cnt)
{
   int rc;

   fflush(stdout);
   if (!ech)
      rc = read(STDIN_FILENO, buf, cnt);
   else {
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &Savedtty);
      rc = read(STDIN_FILENO, buf, cnt);
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &Rawtty);
   }
      /* may be the beginning of a lengthy escape sequence  */
   tcflush(STDIN_FILENO, TCIFLUSH);
   return rc;                   /* note: we do NOT produce a vaid 'string' */
}


        /*
         * This routine simply formats whatever the caller wants and
         * returns a pointer to the resulting 'const char' string... */
static const char *fmtmk (const char *fmts, ...)
{
   static char buf[BIGBUFSIZ];          /* with help stuff, our buffer */
   va_list va;                          /* requirements exceed 1k */

   va_start(va, fmts);
   vsprintf(buf, fmts, va);
   va_end(va);
#ifdef TRAK_MAXBUFS
   MAXBUFS(fmtmk, buf);
#endif
   return (const char *)buf;
}


        /*
         * What need be said... */
static char *strim (int sp, char *str)
{
   const char *ws = "\b\f\n\r\t\v";
   char *p;

   /* this guy was originally designed just to trim the rc file lines and
      any 'open_psdb_message' result which arrived with an inappropriate
      newline (thanks to 'sysmap_mmap') -- but when tabs (^I) were found
      in some proc cmdlines, a choice was offered twix space or null... */
   if (sp)
      while ((p = strpbrk(str, ws))) *p = ' ';
   else
      if ((p = strpbrk(str, ws))) *p = 0;
   return str;
}


        /*
         * This guy just facilitates Batch and protects against dumb ttys. */
static char *tg2 (int x, int y)
{
   return Cap_can_goto ? tgoto(cursor_address, x, y) : (char *)"\n";
}


/*######  Misc Color/Highlighting support  ###############################*/

        /*
         * Make the appropriate caps/color strings and set some
         * lengths which are used to distinguish twix the displayed
         * columns and an actual printf row!
         * note: we avoid the use of background color so as to maximize
         *       compatibility with the user's xterm settings */
static void capsmk (void)
{
   /* macro to test if a basic (non-color) capability is valid
         thanks: Floyd Davidson <floyd@ptialaska.net> */
#define tIF(s)  s ? s : ""
   static int capsdone = 0;

      /* just a precaution for the future, we aren't called now... */
   if (Batch) return;

      /* these are the unchangeable puppies, so we only do 'em once */
   if (!capsdone) {
      strcpy(Cap_bold, tIF(enter_bold_mode));
      strcpy(Cap_clr_eol, tIF(clr_eol));
      strcpy(Cap_clr_eos, tIF(clr_eos));
      strcpy(Cap_clr_scr, tIF(clear_screen));
      strcpy(Cap_curs_huge, tIF(cursor_visible));
      strcpy(Cap_curs_norm, tIF(cursor_normal));
      strcpy(Cap_home, tIF(cursor_home));
      strcpy(Cap_norm, tIF(exit_attribute_mode));
      strcpy(Cap_reverse, tIF(enter_reverse_mode));
      sprintf(Caps_off, "%s%s", Cap_norm, tIF(orig_pair));
      if (tgoto(cursor_address, 1, 1)) Cap_can_goto = 1;
      capsdone = 1;
   }

      /* now do the changeable guys... */
   if (Show_colors && max_colors > 0) {
      strcpy(Sum_color, tparm(set_a_foreground, Base_color));
      sprintf(Msg_color, "%s%s"
         , tparm(set_a_foreground, Msgs_color), Cap_reverse);
      sprintf(Pmt_color, "%s%s"
         , tparm(set_a_foreground, Msgs_color), Cap_bold);
      sprintf(Hdr_color, "%s%s"
         , tparm(set_a_foreground, Head_color), Cap_reverse);
      sprintf(Row_color_norm, "%s%s"
         , Caps_off, tparm(set_a_foreground, Base_color));
   } else {
      Sum_color[0] = '\0';
      strcpy(Msg_color, Cap_reverse);
      strcpy(Pmt_color, Cap_bold);
      strcpy(Hdr_color, Cap_reverse);
      strcpy(Row_color_norm, Cap_norm);
   }

   sprintf(Row_color_high, "%s%s"
      , Row_color_norm, Show_hibold ? Cap_bold : Cap_reverse);
   Len_row_norm = strlen(Row_color_norm);
   Len_row_high = strlen(Row_color_high);

#undef tIF
}


        /*
         * Show an error, but not right now.
         * Due to the postponed opening of ksym, using open_psdb_message,
         * if P_WCHAN had been selected and the program is restarted, the
         * message would otherwise be displayed prematurely */
static void msg_save (const char *fmts, ...)
{
   char tmp[SMLBUFSIZ];
   va_list va;

   va_start(va, fmts);
   vsprintf(tmp, fmts, va);
   va_end(va);
      /* we'll add some extra attention grabbers to whatever this is */
   sprintf(Msg_delayed, "\a***  %s  ***", strim(0, tmp));
   Msg_awaiting = 1;
}


        /*
         * Show an error message (caller may include a '\a' for sound) */
static void show_msg (const char *str)
{
   printf("%s%s %s %s%s"
      , tg2(0, MSG_line)
      , Msg_color
      , str
      , Caps_off
      , Cap_clr_eol);
   fflush(stdout);
   sleep(MSG_SLEEP);
   Msg_awaiting = 0;
}


        /*
         * Show an input prompt + larger cursor */
static void show_pmt (const char *str)
{
   printf("%s%s%s: %s%s"
      , tg2(0, MSG_line)
      , Pmt_color
      , str
      , Cap_curs_huge
      , Caps_off);
   fflush(stdout);
}


        /*
         * Show lines with specially formatted elements, but only output
         * what will fit within the current screen width.
         *    Our special formatting consists of:
         *       "some text <_delimiter_> some more text <_delimiter_>...\n"
         *    Where <_delimiter_> is a single byte in the range of:
         *       \01 through \07 (more properly \0001 - \0007)
         *    and is used to select an 'attribute' from a capabilities table
         *    which is then applied to the *preceding* substring.
         *
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
static void show_special (const char *glob)
{
   static char *captab[] = {            /* Cap's/Delim's */
      Cap_norm, Cap_norm, Cap_bold,     /* \00, \01, \02 */
      Sum_color,                        /* \03           */
      Msg_color, Pmt_color,             /* \04, \05      */
      Hdr_color,                        /* \06           */
      Row_color_high  };                /* \07           */
   char tmp[BIGBUFSIZ], *cap, *lin_end, *sub_beg, *sub_end;
   int room;

      /* handle multiple lines passed in a bunch */
   while ((lin_end = strchr(glob, '\n'))) {

         /* create a local copy we can extend and otherwise abuse */
      memcpy(tmp, glob, (unsigned)(lin_end - glob));
#ifdef TRAK_MAXBUFS
      MAXBUFS(show_special, tmp);
#endif
         /* zero terminate this part and prepare to parse substrings */
      tmp[lin_end - glob] = '\0';
      room = Screen_cols;
      sub_beg = sub_end = tmp;

      while (*sub_beg) {
         switch (*sub_end) {
            case '\00' :                /* no end delim, captab makes normal */
               *(sub_end + 1) = '\0';   /* extend str end, then fall through */
            case '\01' ... '\07' :
               cap = captab[(int)*sub_end];
               *sub_end = '\0';
               printf("%s%.*s%s", cap, room, sub_beg, Caps_off);
               room -= (sub_end - sub_beg);
               sub_beg = ++sub_end;
               break;
            default :                   /* nothin' special, just text */
               ++sub_end;
         }

         if (0 >= room) break;          /* skip substrings that won't fit */
      } /* end: while 'subtrings' */

      printf("%s\n", Cap_clr_eol);      /* emulate truncated newline */
      glob = ++lin_end;                 /* point to next line (maybe) */

   } /* end: while 'lines' */

   /* if there's anything left in the glob (by virtue of no trailing '\n'),
      it probably means caller wants to retain cursor position on this final
      line -- ok then, we'll just do our 'fit-to-screen' thingy... */
   if (strlen(glob)) printf("%.*s", Screen_cols, glob);
   fflush(stdout);
}


        /*
         * Change colors used in display */
static void tweak_colors (void)
{
#define kbdABORT  'q'
#define kbdAPPLY  '\n'
   int clrssav = Show_colors, boldsav = Show_hibold,
       basesav = Base_color, msgssav = Msgs_color, headsav = Head_color;
   int clr = Base_color, *pclr = &Base_color;
   char ch, tgt = 'B';

   if (0 >= max_colors) {
      show_msg("\aNo colors to map!");
      return;
   }
   if (!Show_colors) {
      Show_colors = 1;
      capsmk();
   }
   printf("%s%s", Cap_clr_scr, Cap_curs_huge);

   do {
         /* this string is well above ISO C89's minimum requirements! */
      show_special(fmtmk(COLOR_sample
         , Cap_home, Myname, procps_version, tgt, clr));
      chin(0, &ch, 1);
      switch (ch) {
         case 'B' :
            pclr = &Base_color;
            clr = *pclr;
            tgt = ch;
            break;
         case 'M' :
            pclr = &Msgs_color;
            clr = *pclr;
            tgt = ch;
            break;
         case 'H' :
            pclr = &Head_color;
            clr = *pclr;
            tgt = ch;
            break;
         case '0' ... '7' :
            clr = ch - '0';
            break;
         case 'b' :
            Show_hibold = !Show_hibold;
            break;
         case 'z' :
            Show_colors = !Show_colors;
            break;
      }
      *pclr = clr;
      capsmk();
   } while (kbdAPPLY != ch && kbdABORT != ch);

   if (kbdABORT == ch) {
      Show_colors = clrssav; Show_hibold = boldsav;
      Base_color = basesav; Msgs_color = msgssav; Head_color = headsav;
      capsmk();
   }
   putp(Cap_curs_norm);

#undef kbdABORT
#undef kbdAPPLY
}


/*######  Small utility routines  ########################################*/

        /*
         * Get a string from the user */
static char *ask_str (const char *prompt)
{
   static char buf[GETBUFSIZ];

   show_pmt(prompt);
   memset(buf, '\0', sizeof(buf));
   chin(1, buf, sizeof(buf) - 1);
   putp(Cap_curs_norm);

#ifdef TRAK_MAXBUFS
   MAXBUFS(ask_str, buf);
#endif
   return strim(0, buf);
}


        /*
         * Get a float from the user */
static float get_float (const char *prompt)
{
   char *line;
   float f;

   if (!(*(line = ask_str(prompt)))) return -1;
      /* note: we're not allowing negative floats */
   if (strcspn(line, ".1234567890")) {
      show_msg("\aNot valid");
      return -1;
   }
   sscanf(line, "%f", &f);
   return f;
}


        /*
         * Get an integer from the user */
static int get_int (const char *prompt)
{
   char *line;
   int n;

   if (!(*(line = ask_str(prompt)))) return -1;
      /* note: we've got to allow negative ints (renice)  */
   if (strcspn(line, "-1234567890")) {
      show_msg("\aNot valid");
      return -1;
   }
   sscanf(line, "%d", &n);
   return n;
}


        /*
         * Set the number of fields/columns to display.
         * Create the field/column headings and set maximum cmdline length.
         * Establish the heading/summary lines currently in use.
         * Adjust the number of tasks to display. */
static void mkheadings (void)
{
   const char *h;
   int i, needpsdb = 0;

      /* build our PFlags array and establish a tentative NumFields */
   for (i = 0, NumFields = 0; i < (int)strlen(CurFields); i++) {
      if (isupper(CurFields[i]))
         PFlags[NumFields++] = CurFields[i] - 'A';
   }

      /* build a preliminary ColHeadings not to exceed screen width */
   ColHeadings[0] = '\0';
   for (i = 0; i < NumFields; i++) {
      h = Fieldstab[PFlags[i]].head;
         /* oops, won't fit -- we're outta here... */
      if (Screen_cols < (int)(strlen(ColHeadings) + strlen(h))) break;
      strcat(ColHeadings, h);
   }

      /* establish the final NumFields and prepare to grow the command
         column heading via Max_cmd -- it may be a fib if P_CMD wasn't
         encountered, but that's ok because it won't be displayed anyway */
   NumFields = i;
   Max_cmd = Screen_cols
      - (strlen(ColHeadings) - strlen(Fieldstab[P_CMD].head)) - 1;

      /* now we can build the true run-time ColHeadings and format the
         command column heading if P_CMD is really being displayed */
   ColHeadings[0] = '\0';
   for (i = 0; i < NumFields; i++) {
         /* are we gonna' need the kernel symbol table? */
      if (P_WCHAN == PFlags[i]) needpsdb = 1;
      h = Fieldstab[PFlags[i]].head;
      if (P_CMD == PFlags[i])
         strcat(ColHeadings, fmtmk(Fieldstab[P_CMD].fmts, Max_cmd, Max_cmd, h));
      else
         strcat(ColHeadings, h);
   }

      /* set the number of heading lines and calc display height */
   HSum_lines = SUMMINLINS;
   if (HSum_loadav) HSum_lines += 1;
   if (HSum_states) {
      if (Show_cpusum) HSum_lines += 2;
         /* no tellin' how darn many cpus they might have -- if they exceed
            screen height, they'll have to suffer scroll...
            (Max_lines may go negative, which is as good as 0) */
      else HSum_lines += Cpu_tot + 1;
   }
   if (HSum_memory) HSum_lines += 2;

   Max_lines = Screen_rows - HSum_lines;
   if (Max_tasks && Max_tasks < Max_lines)
      Max_lines = Max_tasks;

      /* do we (still) need the kernel symbol table? */
   if (needpsdb) {
      if (-1 == No_ksyms) {
         No_ksyms = 0;
         if (open_psdb_message(NULL, msg_save))
            /* why so counter-intuitive, couldn't open_psdb_message
               mirror sysmap_mmap -- that func does all the work anyway? */
            No_ksyms = 1;
         else
            PSDBopen = 1;
      }
   }
#ifdef UGH_ITS_4_RH
   else if (PSDBopen) {
      close_psdb();
      PSDBopen = 0;
      No_ksyms = -1;
   }
#endif

#ifdef TRAK_MAXBUFS
   MAXBUFS(mkheadings, CurFields);
   MAXBUFS(mkheadings, ColHeadings);
#endif
}


        /*
         * Do some scaling stuff.
         * We'll interpret 'num' as one of the following types and
         * try to format it to fit 'width'.
         *    SK_no (0) it's a byte count
         *    SK_Kb (1) it's kilobytes
         *    SK_Mb (2) it's megabytes
         *    SK_Gb (3) it's gigabytes  */
static char *scale_num (unsigned num, const unsigned width, const unsigned type)
{
      /* kilobytes, megabytes, gigabytes, too-big-for-int-bytes */
   static double scale[] = { 1024, 1024*1024, 1024*1024*1024, 0 };
      /* kilo, mega, giga, none */
#ifdef UPCASE_SCALE
   static char nextup[] =  { 'K', 'M', 'G', 0 };
#else
   static char nextup[] =  { 'k', 'm', 'g', 0 };
#endif
   static char buf[TNYBUFSIZ];
   double *dp;
   char *up;

      /* try an unscaled version first... */
   sprintf(buf, "%d", num);
#ifdef TRAK_MAXBUFS
   MAXBUFS(scale_num, buf);
#endif
   if (strlen(buf) <= width)
      return buf;

      /* now try successively higher types until it fits */
   for (up = nextup + type, dp = scale; *dp; ++dp, ++up) {
         /* the most accurate version */
      sprintf(buf, "%.1f%c", num / *dp, *up);
      if (strlen(buf) <= width)
         return buf;
         /* the integer version */
      sprintf(buf, "%d%c", (int)(num / *dp), *up);
      if (strlen(buf) <= width)
         return buf;
   }
      /* well shoot, this outta' fit... */
   return (char *)"?";
}


        /*
         * Do some scaling stuff.
         * Format 'tics' to fit 'width' */
static char *scale_tics (TICS_t tics, const unsigned width)
{
   static struct {
      unsigned    div;
      const char *fmt;
   } ttab[] = {
     /* minutes        hours          days            weeks */
#ifdef UPCASE_SCALE
      { 60, "%uM" }, { 60, "%uH" }, { 24, "%uD" }, {  7, "%uW" }
#else
      { 60, "%um" }, { 60, "%uh" }, { 24, "%ud" }, {  7, "%uw" }
#endif
   };
   static char buf[TNYBUFSIZ];
   unsigned i, t;

      /* try successively higher units until it fits */
   t = tics / Hertz;
   sprintf(buf, "%d:%02d.%02d"                 /* minutes:seconds.tenths */
      , t/60, t%60, (int)((tics*100)/Hertz)%100);
#ifdef TRAK_MAXBUFS
   MAXBUFS(scale_tics, buf);
#endif

   if (strlen(buf) <= width)
      return buf;
   sprintf(buf, "%d:%02d", t/60, t%60);         /* minutes:seconds */
   if (strlen(buf) <= width)
      return buf;

      /* try successively: minutes; hours; days; weeks */
   for (i = 0; i < MAXtbl(ttab); i++) {
      t /= ttab[i].div;
      sprintf(buf, ttab[i].fmt, t);
      if (strlen(buf) <= width)
         return buf;
   };
      /* well shoot, this outta' fit... */
   return (char *)"?";
}


        /*
         * Calculate and return the elapsed time since the last update
         * which is then used in % CPU calc's. */
static float time_elapsed (void)
{
    static struct timeval oldtimev;
    struct timeval timev;
    struct timezone timez;
    float et;

    gettimeofday(&timev, &timez);
    et = (timev.tv_sec - oldtimev.tv_sec)
       + (float)(timev.tv_usec - oldtimev.tv_usec) / 1000000.0;
    oldtimev.tv_sec = timev.tv_sec;
    oldtimev.tv_usec = timev.tv_usec;
    return et;
}


/*######  Exit and Signal handled routines  ##############################*/

        /*
         * The usual program end --
         * called only by functions in this section. */
static void bye_bye (int eno, const char *str)
{
#ifdef UGH_ITS_4_RH
   if (PSDBopen)
      close_psdb();
#endif
   if (!Batch)
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &Savedtty);
   printf("%s%s\n", tg2(0, Screen_rows), Cap_curs_norm);

   if (str) {
     if (eno) perror(str);
     else fputs(str, stderr);
   }
#ifdef TRAK_MAXCAPS
   fprintf(stderr,
      "\nTRAK_MAXCAPS report:"
      "\n\tBasic lengths:"
      "\n\t   Cap_home = %d, Cap_bold = %d, Cap_norm = %d, Cap_reverse = %d"
      "\n\t   Cap_clr_eol = %d, Cap_clr_eos = %d, Cap_clr_scr = %d"
      "\n\t   Cap_curs_norm = %d, Cap_curs_huge = %d"
      "\n\t   Caps_off = %d"
      "\n\tColor lengths:"
      "\n\t   Sum_color = %d, Msg_color = %d, Pmt_color = %d, Hdr_color = %d"
      "\n\t   Row_color_norm = %d, Len_row_norm = '%d'"
      "\n\t   Row_color_high = %d, Len_row_high = '%d'"
      "\n\tMax_pads = %d, Min_pads = %d"
      "\n\tMax_rbuf = %d, Min_rbuf = %d"
      "\n"
      , strlen(Cap_home), strlen(Cap_bold), strlen(Cap_norm), strlen(Cap_reverse)
      , strlen(Cap_clr_eol), strlen(Cap_clr_eos), strlen(Cap_clr_scr)
      , strlen(Cap_curs_norm), strlen(Cap_curs_huge)
      , strlen(Caps_off)
      , strlen(Sum_color), strlen(Msg_color), strlen(Pmt_color), strlen(Hdr_color)
      , strlen(Row_color_norm), Len_row_norm
      , strlen(Row_color_high), Len_row_high
      , Max_pads, Min_pads
      , Max_rbuf, Min_rbuf);
#endif
#ifdef TRAK_MAXBUFS
   fprintf(stderr,
      "\nTRAK_MAXBUFS report:"
      "\n\tused      size\tfunction, buffer"
      "\n\t----      ----\t-----------------------"
      ARPTBUF(fmtmk, buf)
      ARPTBUF(show_special, tmp)
      ARPTBUF(ask_str, buf)
      ARPTBUF(scale_num, buf)
      ARPTBUF(scale_tics, buf)
      ARPTBUF(std_err, buf)
      ARPTBUF(frame_states, tmp)
      ARPTBUF(mkcol, tmp)
      ARPTBUF(show_a_task, cbuf)
      ARPTBUF(show_a_task, rbuf)
      ARPTBUF(rcfiles_read, fbuf)
      ARPTBUF(rcfiles_read, RCfile)
      ARPTBUF(rcfiles_read, RCfile_Sys)
      ARPTBUF(do_key, ColUsername)
      ARPTBUF(mkheadings, CurFields)
      ARPTBUF(mkheadings, ColHeadings)
      ARPTBUF(main, not_really_tmp)
      "\n"
      , AUSEBUF(fmtmk, buf), ASIZBUF(fmtmk, buf)
      , AUSEBUF(show_special, tmp), ASIZBUF(show_special, tmp)
      , AUSEBUF(ask_str, buf), ASIZBUF(ask_str, buf)
      , AUSEBUF(scale_num, buf), ASIZBUF(scale_num, buf)
      , AUSEBUF(scale_tics, buf), ASIZBUF(scale_tics, buf)
      , AUSEBUF(std_err, buf), ASIZBUF(std_err, buf)
      , AUSEBUF(frame_states, tmp), ASIZBUF(frame_states, tmp)
      , AUSEBUF(mkcol, tmp), ASIZBUF(mkcol, tmp)
      , AUSEBUF(show_a_task, cbuf), ASIZBUF(show_a_task, cbuf)
      , AUSEBUF(show_a_task, rbuf), ASIZBUF(show_a_task, rbuf)
      , AUSEBUF(rcfiles_read, fbuf), ASIZBUF(rcfiles_read, fbuf)
      , AUSEBUF(rcfiles_read, RCfile) , ASIZBUF(rcfiles_read, RCfile)
      , AUSEBUF(rcfiles_read, RCfile_Sys) , ASIZBUF(rcfiles_read, RCfile_Sys)
      , AUSEBUF(do_key, ColUsername), ASIZBUF(do_key, ColUsername)
      , AUSEBUF(mkheadings, CurFields), ASIZBUF(mkheadings, CurFields)
      , AUSEBUF(mkheadings, ColHeadings), ASIZBUF(mkheadings, ColHeadings)
      , AUSEBUF(main, not_really_tmp), ASIZBUF(main, not_really_tmp));
#endif

#if defined(TRAK_MAXCAPS) || defined(TRAK_MAXBUFS)
   fprintf(stderr,
      "\nbye_bye's Summary report:"
      "\n\tprogram names:"
      "\n\t   Myrealname = %s, Myname = %s"
      "\n\tterminal = '%s'"
      "\n\t   device = %s, ncurses = v%s"
      "\n\t   max_colors = %d, max_pairs = %d"
      "\n\t   Cap_can_goto = %s"
      "\n\tScreen_cols = %d, Screen_rows = %d"
      "\n\tNumFields = %d, HSum_lines = %d"
      "\n\tMax_lines = %d, Max_cmd = %d, Max_tasks = %d"
      "\n\tPage_size = %d"
      "\n"
      , Myrealname, Myname
#ifdef PRETENDNOCAP
      , "dumb"
#else
      , termname()
#endif
      , ttyname(STDOUT_FILENO), NCURSES_VERSION
      , max_colors, max_pairs
      , Cap_can_goto ? "yes" : "No!"
      , Screen_cols, Screen_rows
      , NumFields, HSum_lines
      , Max_lines, Max_cmd, Max_tasks
      , Page_size);
#endif

   if (str && !eno) eno = 1;
   exit(eno);
}


        /*
         * Standard error handler to normalize the look of all err o/p */
static void std_err (const char *str)
{
   static char buf[SMLBUFSIZ];

   fflush(stdout);
      /* we'll use our own buffer so callers can still use fmtmk() */
   sprintf(buf, "\t%s: %s\n", Myname, str);
#ifdef TRAK_MAXBUFS
   MAXBUFS(std_err, buf);
#endif
   if (!Ttychanged) {
      fprintf(stderr, buf);
      exit(1);
   }
      /* not to worry, he'll change our exit code to 1 due to 'buf' */
   bye_bye(0, buf);
}


        /*
         * Normal end of execution.
         * catches:
         *    SIGALRM, SIGHUP, SIGINT, SIGPIPE, SIGQUIT and SIGTERM */
static void stop (int dont_care_sig)
{
   bye_bye(0, NULL);
}


        /*
         * Suspend ourself.
         * catches:
         *    SIGTSTP, SIGTTIN and SIGTTOU */
static void suspend (int dont_care_sig)
{
        /* reset terminal */
   tcsetattr(STDIN_FILENO, TCSAFLUSH, &Savedtty);
   printf("%s%s", tg2(0, Screen_rows), Cap_curs_norm);
   fflush(stdout);
   raise(SIGSTOP);
        /* later, after SIGCONT... */
   if (!Batch)
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &Rawtty);
}


        /*
         * Set the screen dimensions and call the real workhorse.
         * catches:
         *    SIGWINCH and SIGCONT */
static void window_resize (int dont_care_sig)
{
   struct winsize w;

   Screen_cols = columns;
   Screen_rows = lines;
   if (-1 != (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w))) {
      Screen_cols = w.ws_col;
      Screen_rows = w.ws_row;
   }
      /* he'll calculate header size, length of command field, etc ... */
   mkheadings();
}


/*######  Startup routines  ##############################################*/

        /*
         * Parse command line arguments
         * (and force ol' main into a much needed diet)
         * Note: it's assumed that the rc file(s) have already been read
         *       and our job is to see if any of those options are to be
         *       overridden -- we'll force some on and negate others in our
         *       best effort to honor the user's wishes... */
static void parse_argvs (char **argv)
{
   /* difference(s) from traditional top:
      -q (zero delay time) eliminated as redundant, use -d0
      -p (pid monitoring) allows comma delimited list
      no deprecated/illegal use of 'goto' with 'breakargv:'
      bunched args are actually handled properly and none are ignored
      (we tolerate No whitespace and No switches -- maybe too tolerant) */
   const char *usage = " -hv | -bcisS -d delay -n iterations -p pid [,pid ...]\n";
   float tmp_delay = MAXFLOAT;
   char *p;

   ++argv;
   while (*argv) {
      char *cp = *(argv++);

      while (*cp) {
         switch (*cp) {
            case '\0':
            case '-':
               break;
            case 'b':
               Batch = 1;
               break;
            case 'c':
               Show_cmdlin = !Show_cmdlin;
               break;
            case 'd':
               if (cp[1]) ++cp;
               else if (*argv) cp = *argv++;
               else std_err("-d requires argument");
                  /* a negative delay will be dealt with shortly... */
               if (1 != sscanf(cp, "%f", &tmp_delay))
                  std_err(fmtmk("bad delay '%s'", cp));
               break;
            case 'h': case 'H':
            case 'v': case 'V':
               std_err(fmtmk("\t%s\nusage:\t%s%s"
                  , procps_version, Myname, usage));
            case 'i':
               Show_idleps = !Show_idleps;
               break;
            case 'n':
               if (cp[1]) cp++;
               else if (*argv) cp = *argv++;
               else std_err("-n requires argument");
               if (1 != sscanf(cp, "%d", &Loops) || 0 > Loops)
                  std_err(fmtmk("bad iterations arg '%s'", cp));
               break;
            case 'p':
               if (cp[1]) cp++;
               else if (*argv) cp = *argv++;
               else std_err("-p requires argument");
               do {
                  if (Monpidsidx >= MONPIDMAX)
                     std_err(fmtmk("pid limit (%d) exceeded", MONPIDMAX));
                  if (1 != sscanf(cp, "%d", &Monpids[Monpidsidx])
                  || 0 > Monpids[Monpidsidx])
                     std_err(fmtmk("bad pid '%s'", cp));
                  if (!Monpids[Monpidsidx])
                     Monpids[Monpidsidx] = getpid();
                  Monpidsidx++;
                  if (!(p = strchr(cp, ',')))
                     break;
                  cp = ++p;
               } while (*cp);
               break;
            case 's':
               Secure_mode = 1;
               break;
            case 'S':
               Show_ctimes = !Show_ctimes;
               break;
            default :
               std_err(fmtmk("unknown argument '%c'\nusage:\t%s%s"
                  , *cp, Myname, usage));

         } /* end: switch */
            /* advance cp and jump over any numerical args used above */
         cp += strspn(++cp, "-.1234567890 ");

      } /* end: while *cp */
   } /* end: while *argv */

      /* fixup delay time, maybe... */
   if (MAXFLOAT != tmp_delay) {
      if (Secure_mode || 0 > tmp_delay)
         msg_save("Delay time Not changed");
      else
         Delay_time = tmp_delay;
   }

      /* set the default sort type, maybe... */
   if (-1 == Sort_type || Monpidsidx) {
      Sort_type = S_PID;
      Sort_func = (QSORT_t)sort_pid;
   }
}


        /*
         * Parse the options string as read from line two of the "local"
         * configuration file. */
static void parse_rc (char *opts)
{
   unsigned i;

   for (i = 0; i < strlen(opts); i++) {
      switch (opts[i]) {
         case 'b':
            Show_hibold = 1;
            break;
         case 'c':
            Show_cmdlin = 1;
            break;
         case 'C':
            Sort_type = S_CMD;
            Sort_func = (QSORT_t)sort_cmd;
            break;
         case 'E':
            Sort_type = S_USR;
            Sort_func = (QSORT_t)sort_usr;
            break;
         case 'i':
            Show_idleps = 1;
            break;
         case 'I':
            Irix_mode = 1;
            break;
         case 'l':
            HSum_loadav = 1;
            break;
         case 'm':
            HSum_memory = 1;
            break;
         case 'M':
            Sort_type = S_MEM;
            Sort_func = (QSORT_t)sort_mem;
            break;
         case 'P':
            Sort_type = S_PID;
            Sort_func = (QSORT_t)sort_pid;
            break;
         case 'R':
            Sort_normal = 1;
            break;
         case 'S':
            Show_ctimes = 1;
            break;
         case 't':
            HSum_states = 1;
            break;
         case 'T':
            Sort_type = S_TME;
            Sort_func = (QSORT_t)sort_tme;
            break;
         case 'U':
            Sort_type = S_CPU;
            Sort_func = (QSORT_t)sort_cpu;
            break;
         case 'x':
            Show_hicols = 1;
            break;
         case 'y':
            Show_hirows = 1;
            break;
         case 'Y':
            Sort_type = S_TTY;
            Sort_func = (QSORT_t)sort_tty;
            break;
         case 'z':
            Show_colors = 1;
            break;
         case '1':
            Show_cpusum = 1;
            break;
         case ' ' :             /* these serve as rc file comments */
         case '\t':             /* and will be treated as 'eol' */
         case '\n':
            return;
         default :
            std_err(fmtmk("bad config option - '%c'", opts[i]));
      }
   }
}


        /*
         * Build the two RC file names then try to read 'em.
         *
         * '/etc/RCfile_Sys' contains two lines consisting of the secure mode
         *   switch and an update interval.  It's presence limits what ordinary
         *   users are allowed to do.
         *
         * '$HOME/RCfile' contains five lines.
         *   line 1: specifies the fields that are to be displayed
         *           and their order.  Uppercase letters == displayed,
         *           lowercase letters == not shown.
         *   line 2: specifies miscellaneous display options and
         *           toggles along with the prefered sort order.
         *   line 3: specifies maximum processes, 0 == unlimited.
         *   line 4: specifies the update interval.  If run in secure
         *           mode, this value will be ignored except for root.
         *   line 5: specifies the 3 basic color values. */
static void rcfiles_read (void)
{
   char fbuf[RCFBUFSIZ];
   FILE *fp;

   strcpy(RCfile_Sys, fmtmk("/etc/%src", Myrealname));
#ifdef TRAK_MAXBUFS
   MAXBUFS(rcfiles_read, RCfile_Sys);
#endif
   if (getenv("HOME"))
      strcpy(RCfile, fmtmk("%s%c", getenv("HOME"), '/'));
   strcat(RCfile, fmtmk(".%src", Myname));
#ifdef TRAK_MAXBUFS
   MAXBUFS(rcfiles_read, RCfile);
#endif

   fp = fopen(RCfile_Sys, "r");
   if (fp) {
      fbuf[0] = '\0';
      fgets(fbuf, sizeof(fbuf), fp);            /* sys rc file, line #1 */
#ifdef TRAK_MAXBUFS
      MAXBUFS(rcfiles_read, fbuf);
#endif
      if (strchr(fbuf, 's')) Secure_mode = 1;

      fbuf[0] = '\0';
      fgets(fbuf, sizeof(fbuf), fp);            /* sys rc file, line #2 */
#ifdef TRAK_MAXBUFS
      MAXBUFS(rcfiles_read, fbuf);
#endif
      fclose(fp);
      sscanf(fbuf, "%f", &Delay_time);
   }

   fp = fopen(RCfile, "r");
   if (fp) {
      fbuf[0] = '\0';
      if (fgets(fbuf, sizeof(fbuf), fp)) {      /* rc file, line #1 */
#ifdef TRAK_MAXBUFS
         MAXBUFS(rcfiles_read, fbuf);
#endif
         strcpy(CurFields, strim(0, fbuf));
            /* Now that we've found an rc file, we'll honor those
               preferences by first turning off everything... */
         Irix_mode = 0;
         HSum_states = HSum_memory = HSum_loadav = Show_cpusum = 0;
         Show_cmdlin = Show_ctimes = Show_idleps = Sort_normal = 0;
         Show_colors = Show_hicols = Show_hirows = Show_hibold = 0;

         fbuf[0] = '\0';
         fgets(fbuf, sizeof(fbuf), fp);         /* rc file, line #2 */
#ifdef TRAK_MAXBUFS
         MAXBUFS(rcfiles_read, fbuf);
#endif
            /* we could subsume this next guy since we're the only caller
               -- but we're both too fat already... */
         parse_rc(strim(0, fbuf));

         fbuf[0] = '\0';
         fgets(fbuf, sizeof(fbuf), fp);         /* rc file, line #3 */
#ifdef TRAK_MAXBUFS
         MAXBUFS(rcfiles_read, fbuf);
#endif
         sscanf(fbuf, "%d", &Max_tasks);

         fbuf[0] = '\0';
         fgets(fbuf, sizeof(fbuf), fp);         /* rc file, line #4 */
#ifdef TRAK_MAXBUFS
         MAXBUFS(rcfiles_read, fbuf);
#endif
         if (!Secure_mode || !getuid())
            sscanf(fbuf, "%f", &Delay_time);

         fbuf[0] = '\0';
         fgets(fbuf, sizeof(fbuf), fp);         /* rc file, line #5 */
#ifdef TRAK_MAXBUFS
         MAXBUFS(rcfiles_read, fbuf);
#endif
         sscanf(fbuf, "%d,%d,%d", &Base_color, &Msgs_color, &Head_color);
      }
      fclose(fp);
   }
      /* protect against meddling leading to a possible fault --
         shorter would kinda' work, longer ain't healthy for us! */
   if (strlen(CurFields) != strlen(DEF_FIELDS))
      strcpy(CurFields, DEF_FIELDS);

      /* lastly, establish the true runtime secure mode */
   Secure_mode = getuid() ? Secure_mode : 0;
}


        /*
         * Set up the terminal attributes */
static void terminal_set (void)
{
   struct termios newtty;

      /* first the ncurses part... */
#ifdef PRETENDNOCAP
   setupterm((char *)"dumb", STDOUT_FILENO, NULL);
#else
   setupterm(NULL, STDOUT_FILENO, NULL);
#endif
      /* now our part... */
   if (!Batch) {
      if (-1 == tcgetattr(STDIN_FILENO, &Savedtty))
         std_err("tcgetattr() failed");
      capsmk();
      newtty = Savedtty;
      newtty.c_lflag &= ~ICANON;
      newtty.c_lflag &= ~ECHO;
      newtty.c_cc[VMIN] = 1;
      newtty.c_cc[VTIME] = 0;

      Ttychanged = 1;
      if (-1 == tcsetattr(STDIN_FILENO, TCSAFLUSH, &newtty)) {
         putp(Cap_clr_scr);
         std_err(fmtmk("Failed tty set: %s", strerror(errno)));
      }
      tcgetattr(STDIN_FILENO, &Rawtty);
      putp(Cap_clr_scr);
      fflush(stdout);
   }
}


/*######  Field Selection/Ordering routines  #############################*/

        /*
         * Display the current fields and their order.
         * Upper case indicates a displayed field, display order is
         * according to the order of the letters.
         *
         * A short description of each field is shown as well and is
         * marked by a leading asterisk (*) if currently displayed.
         *
         * After all fields have been displayed, some extra explanatory
         * text is then output */
static void display_fields (void)
{
   const char *p, *x;
   int i, cmax = Screen_cols / 2, rmax = Screen_rows - 3;

   /* we're relying on our callers to first clear the screen --
      thus 'fields_toggle' can avoid screen flicker since he's
      too lazy to handle his own asterisk (*) logic */
   putp(Cap_bold);
   for (i = 0; i < MAXtbl(Fieldstab); ++i) {
         /* advance past any leading spaces */
      for (p = Fieldstab[i].head; ' ' == *p; ++p)
         ;
      printf("%s%c %c: %-10s = %s"
         , tg2((i / rmax) * cmax, (i % rmax) + 3)
         , strchr(CurFields, i + 'A') ? '*' : ' '
         , i + 'A'
         , p
         , Fieldstab[i].desc);
   }
   putp(Row_color_norm);
   x = FIELDS_xtra;
   while ((p = strchr(x, '\n'))) {
      ++i;
      printf("%s%.*s"
         , tg2((i / rmax) * cmax, (i % rmax) + 3)
         , p - x, x);
      x = ++p;
   }
   putp(Caps_off);
}


        /*
         * Change order of displayed fields. */
static void fields_reorder (void)
{
   static char prompt[] =
      "Upper case characters move field left, lower case right";
   char c, *p;
   int i;

   printf("%s%s", Cap_clr_scr, Cap_curs_huge);
   display_fields();
   do {
      show_special(fmtmk(FIELDS_current
         , Cap_home, Myname, CurFields, prompt));
      chin(0, &c, 1);
      i = toupper(c) - 'A';
      if (i < 0 || i >= MAXtbl(Fieldstab))
         break;
      if (((p = strchr(CurFields, i + 'A')))
      || ((p = strchr(CurFields, i + 'a')))) {
         if (isupper(c)) p--;
         if (('\0' != p[1]) && (p >= CurFields)) {
            c    = p[0];
            p[0] = p[1];
            p[1] = c;
         }
      }
   } while (1);
   putp(Cap_curs_norm);
   mkheadings();
}


        /*
         * Toggle displayed fields. */
static void fields_toggle (void)
{
   static char prompt[] =
      "Toggle fields with a-x, type any other key to return";
   char c, *p;
   int i;

   printf("%s%s", Cap_clr_scr, Cap_curs_huge);
   do {
      display_fields();
      show_special(fmtmk(FIELDS_current
         , Cap_home, Myname, CurFields, prompt));
      chin(0, &c, 1);
      i = toupper(c) - 'A';
      if (i < 0 || i >= MAXtbl(Fieldstab))
         break;
      if ((p = strchr(CurFields, i + 'A')))
         *p = i + 'a';
      else if ((p = strchr(CurFields, i + 'a')))
         *p = i + 'A';
   } while (1);
   putp(Cap_curs_norm);
   mkheadings();
}


/*######  Library Alternatives  ##########################################*/

        /*
         * Handle our own memory stuff without the risk of leaving the
         * user's terminal in an ugly state should things go sour. */
static const char *alloc_msg = "Failed memory allocate (%d bytes)";

static void *alloc_c (unsigned numb)
{
   void * p;

   if (!numb) ++numb;
   if (!(p = calloc(1, numb)))
      std_err(fmtmk(alloc_msg, numb));
   return p;
}


static void *alloc_r (void *q, unsigned numb)
{
   void *p;

   if (!numb) ++numb;
   if (!(p = realloc(q, numb)))
      std_err(fmtmk(alloc_msg, numb));
   return p;
}


        /*
         * This guy is modeled on libproc's readproctab function except
         * we reuse and extend any prior proc_t's.  He's been customized
         * for our specific needs and to avoid the use of <stdarg.h> */
static proc_t **readprocs (proc_t **tbl)
{
#define PTRsz  sizeof(proc_t *)         /* eyeball candy */
#define ENTsz  sizeof(proc_t)
   static int flags = PROC_FILLMEM | PROC_FILLCMD | PROC_FILLUSR
                    | PROC_FILLSTATUS | PROC_FILLSTAT;
   static unsigned savmax = 0;          /* first time, bypass: (i)  */
   proc_t *ptsk = (proc_t *)-1;         /* first time, force: (ii)  */
   unsigned curmax = 0;                 /* every time               */
   PROCTAB* PT;

   if (Monpidsidx) {
      PT = openproc(flags | PROC_PID, Monpids);
         /* work around a previous bug in openproc (now corrected) */
      PT->procfs = NULL;
   } else
      PT = openproc(flags);

      /* i) Allocated Chunks:  *Existing* table;  refresh + reuse */
   while (curmax < savmax) {
      if (tbl[curmax]->cmdline) {
         free(*tbl[curmax]->cmdline);
         tbl[curmax]->cmdline = NULL;
      }
      if (!(ptsk = readproc(PT, tbl[curmax]))) break;
      ++curmax;
   }

      /* ii) Unallocated Chunks:  *New* or *Existing* table;  extend + fill */
   while (ptsk) {
         /* realloc as we go, keeping 'tbl' ahead of 'currmax++' */
      tbl = alloc_r(tbl, (curmax + 1) * PTRsz);
         /* here, readproc will allocate the underlying proc_t stg */
      if ((ptsk = readproc(PT, NULL)))
         tbl[curmax++] = ptsk;
   }
   closeproc(PT);

      /* iii) Chunkless:  make 'eot' entry, after possible extension */
   if (curmax >= savmax) {
      tbl = alloc_r(tbl, (curmax + 1) * PTRsz);
         /* here, we must allocate the underlying proc_t stg ourselves */
      tbl[curmax] = alloc_c(ENTsz);
      savmax = curmax + 1;
   }
      /* this frame's end, but not necessarily end of allocated space */
   tbl[curmax]->pid = -1;
   return tbl;

#undef PTRsz
#undef ENTsz
}


/*######  Main screen routines  ##########################################*/

        /*
         * Process keyboard input during the main loop plus the three
         * special immediate keys used with help processing.
         * (thus making us only slightly recursive) */
static void do_key (unsigned c)
{
#define kbdCTRL_L  12
      /* standardized 'secure mode' errors */
   const char *smerror = "\aCan't %s in secure mode";

   switch (c) {
      case 'b':
         if (!Show_hicols && !Show_hirows)
            show_msg("\aNothing to highlight!");
         else {
            Show_hibold = !Show_hibold;
         }
         capsmk();
         break;

      case 'c':
         Show_cmdlin = !Show_cmdlin;
         break;

      case 'C':
         Sort_type = S_CMD;
         Sort_func = (QSORT_t)sort_cmd;
         break;

      case 'E':
         Sort_type = S_USR;
         Sort_func = (QSORT_t)sort_usr;
         break;

      case 'f':
      case 'F':
         fields_toggle();
         break;

      case 'i':
         Show_idleps = !Show_idleps;
         break;

      case 'I':
         if (Cpu_tot > 1) {
            Irix_mode = !Irix_mode;
            show_msg(fmtmk("Irix mode %s", Irix_mode ? "On" : "Off"));
         } else
            show_msg("\aIrix mode requires SMP!");
         break;

      case 'k':
         if (Secure_mode) {
            show_msg(fmtmk(smerror, "kill"));
         } else {
            int sig, pid = get_int("PID to kill");

            if (-1 != pid) {
#ifdef UGH_ITS_4_RH
               sig = get_signal2(
#else
               sig = signal_name_to_number(
#endif
                  ask_str(fmtmk("Kill PID %d with signal [%i]"
                     , pid, DEF_SIGNAL)));
               if (-1 == sig) sig = DEF_SIGNAL;
               if (sig && kill(pid, sig))
                  show_msg(fmtmk("\aKill of PID '%d' with '%d' failed: %s"
                     , pid, sig, strerror(errno)));
            }
         }
         break;

      case 'l':
         HSum_loadav = !HSum_loadav;
         mkheadings();
         break;

      case 'm':
         HSum_memory = !HSum_memory;
         mkheadings();
         break;

      case 'M':
         Sort_type = S_MEM;
         Sort_func = (QSORT_t)sort_mem;
         break;

      case 'n':
      case '#':
      {  int num;

         if (-1 != (num = get_int("Processes to display (0 = unlimited)"))) {
            Max_tasks = num;
            window_resize(0);
         }
      }
         break;

      case 'o':
      case 'O':
         fields_reorder();
         break;

      case 'P':
         Sort_type = S_PID;
         Sort_func = (QSORT_t)sort_pid;
         break;

      case 'q':
         stop(0);

      case 'r':
         if (Secure_mode)
            show_msg(fmtmk(smerror, "renice"));
         else {
            int pid, val;

            pid = get_int("PID to renice");
            if (-1 == pid) break;
            val = get_int(fmtmk("Renice PID %d to value", pid));
            if (setpriority(PRIO_PROCESS, (unsigned)pid, val))
               show_msg(fmtmk("\aRenice of PID %d to %d failed: %s"
                  , pid, val, strerror(errno)));
         }
         break;

      case 'R':
         Sort_normal = !Sort_normal;
         break;

      case 's':
      case 'd':
         if (Secure_mode)
            show_msg(fmtmk(smerror, "change delay"));
         else {
            float tmp =
               get_float(fmtmk("Change delay from %.1f to", Delay_time));
            if (tmp > -1) Delay_time = tmp;
         }
         break;

      case 'S':
         Show_ctimes = !Show_ctimes;
         show_msg(fmtmk("Cumulative time %s", Show_ctimes ? "On" : "Off"));
         break;

      case 't':
         HSum_states = !HSum_states;
         mkheadings();
         break;

      case 'T':
         Sort_type = S_TME;
         Sort_func = (QSORT_t)sort_tme;
         break;

      case 'u':
         strcpy(ColUsername, ask_str("Which User (Blank for All)"));
#ifdef TRAK_MAXBUFS
         MAXBUFS(do_key, ColUsername);
#endif
         break;

      case 'U':
         Sort_type = S_CPU;
         Sort_func = (QSORT_t)sort_cpu;
         break;

      case 'W':
      {  FILE *fp = fopen(RCfile, "w");

         if (fp) {
            fprintf(fp, "%s\t\t# Fields displayed and ordering\n"
               , CurFields);
            fprintf(fp, "%s%s%s%s%s%s%s%s%s%s%s%s%s%c"
                        "\t\t\t\t# Misc options (spelled poorly)\n"
               , Show_cmdlin ? "c" : "", Sort_normal ? "R" : ""
               , HSum_loadav ? "l" : "", Show_hirows ? "y" : ""
               , HSum_memory ? "m" : "", Show_hicols ? "x" : ""
               , Show_idleps ? "i" : "", Show_ctimes ? "S" : ""
               , Irix_mode   ? "I" : "", HSum_states ? "t" : ""
               , Show_hibold ? "b" : "", Show_colors ? "z" : ""
               , Show_cpusum ? "1" : "", Sort_type);
            fprintf(fp, "%d\t\t\t\t\t# Number of tasks shown\n"
               , Max_tasks);
            fprintf(fp, "%.1f\t\t\t\t\t# Delay between updates\n"
               , Delay_time);
            fprintf(fp, "%u,%u,%u\t\t\t\t\t# Base, Msgs & Head colors\n"
               , Base_color, Msgs_color, Head_color);
            fclose(fp);
            show_msg(fmtmk("Wrote configuration to '%s'", RCfile));
         } else
            show_msg(fmtmk("\aFailed '%s' open: %s", RCfile, strerror(errno)));
      }
         break;

      case 'x':
         Show_hicols = !Show_hicols;
         capsmk();
         break;

      case 'y':
         Show_hirows = !Show_hirows;
         capsmk();
         break;

      case 'Y':
         Sort_type = S_TTY;
         Sort_func = (QSORT_t)sort_tty;
         break;

      case 'z':
         Show_colors = !Show_colors;
         capsmk();
         break;

      case 'Z':
         tweak_colors();
         break;

      case '?':
      case 'h':
      {  char ch;

         printf("%s%s", Cap_clr_scr, Cap_curs_huge);
            /* this string is well above ISO C89's minimum requirements! */
         show_special(fmtmk(HELP_data
            , Myname, procps_version
            , Secure_mode ? "On" : "Off", Show_ctimes ? "On" : "Off", Delay_time
            , Secure_mode ? "" : HELP_unsecured));
         chin(0, &ch, 1);
         putp(Cap_curs_norm);
            /* our help screen currently provides for three 'immediate' keys,
               two of which conflict with the main process display keys */
         switch (ch) {
            case 'j' :
               strcpy(CurFields, JOB_FIELDS);
               ch = 'T';        /* force sort on time */
               break;
            case 'm' :
               strcpy(CurFields, MEM_FIELDS);
               ch = 'M';        /* force sort on %mem/res */
               break;
            case 'u' :
               strcpy(CurFields, USR_FIELDS);
               ch = 'E';        /* force sort on user */
               break;
            default :
               ch = 0;
         }
         if (ch) {
            /* besides resetting the sort environment with the call to us
               below, the 'Show_' manipulations will provide our most subtle
               hint as to what the user has just wrought */
            Show_hibold = Show_hicols = Sort_normal = 1;
            capsmk();
            mkheadings();
            do_key((unsigned)ch);
         }
      }
         break;

      case ' ':
      case kbdCTRL_L:
         putp(Cap_clr_scr);
         break;

      case '1':
         Show_cpusum = !Show_cpusum;
         mkheadings();
         break;

      case '\n':          /* just ignore it */
         break;

      default:
         show_msg("\aUnknown command -- try 'h' for help");
   }

#undef kbdCTRL_L
#undef smERROR
}


#ifdef UGH_ITS_4_RH
        /*
         * Obtain memory information and display it.
         * Return the total memory available as a page count which is
         * then used in % memory calc's. */
static unsigned frame_memory (void)
{
   /* don't be mislead by the proc/sysinfo subscripts, they're just poorly
      chosen names for enumerations apparently designed to make source
      lines as imbalanced and as long as possible */
   unsigned long long **memarray;

   if (!(memarray = meminfo()))
      std_err("Failed /proc/meminfo read");

   if (HSum_memory) {
      show_special(fmtmk(MEMORY_line1
         , BYTES_2K(memarray[meminfo_main][meminfo_total])
         , BYTES_2K(memarray[meminfo_main][meminfo_used])
         , BYTES_2K(memarray[meminfo_main][meminfo_free])
         , BYTES_2K(memarray[meminfo_main][meminfo_buffers])));

      show_special(fmtmk(MEMORY_line2
         , BYTES_2K(memarray[meminfo_swap][meminfo_total])
         , BYTES_2K(memarray[meminfo_swap][meminfo_used])
         , BYTES_2K(memarray[meminfo_swap][meminfo_free])
         , BYTES_2K(memarray[meminfo_total][meminfo_cached])));
   }

   return PAGE_CNT(memarray[meminfo_main][meminfo_total]);
}

#else

        /*
         * Obtain memory information and display it. */
static void frame_memory (void)
{
   meminfo();
   if (HSum_memory) {
      show_special(fmtmk(MEMORY_line1
         , kb_main_total
         , kb_main_used
         , kb_main_free
         , kb_main_buffers));

      show_special(fmtmk(MEMORY_line2
         , kb_swap_total
         , kb_swap_used
         , kb_swap_free
         , kb_main_cached));
   }
}
#endif /* end: UGH_ITS_4_RH */


        /*
         * State display *Helper* function to calc and display the state
         * percentages for a single cpu.  In this way, we can support
         * the following environments without the usual code bloat.
         *    1 - single cpu machines
         *    2 - modest smp boxes with room for each cpu's percentages
         *    3 - massive smp guys leaving little or no room for process
         *        display and thus requiring the Show_cpusum toggle */
static void frame_smp (FILE *fp, const char *fmt, CPUS_t *cpu, const char *pfx)
{
        /* we'll trim to zero if we get negative time ticks,
           which has happened with some SMP kernels (pre-2.4?) */
#define TRIMz(x)  ((tz = (long)x) < 0 ? 0 : tz)
   TICS_t u_tics, s_tics, n_tics, i_tics;
   long   u_frme, s_frme, n_frme, i_frme, tot_frme, tz;

   if (4 != fscanf(fp, fmt, &u_tics, &n_tics, &s_tics, &i_tics))
      std_err("Failed /proc/stat read");

   u_frme = TRIMz(u_tics - cpu->u);
   s_frme = TRIMz(s_tics - cpu->s);
   n_frme = TRIMz(n_tics - cpu->n);
   i_frme = TRIMz(i_tics - cpu->i);
   tot_frme = u_frme + s_frme + n_frme + i_frme;
   if (1 > tot_frme) tot_frme = 1;

      /* display some kinda' cpu state percentages
         (who or what is explained by the passed prefix) */
   show_special(fmtmk(STATES_line2
      , pfx
      , (float)u_frme * 100 / tot_frme
      , (float)s_frme * 100 / tot_frme
      , (float)n_frme * 100 / tot_frme
      , (float)i_frme * 100 / tot_frme));

      /* remember for next time around */
   cpu->u = u_tics;
   cpu->s = s_tics;
   cpu->n = n_tics;
   cpu->i = i_tics;

#undef TRIMz
}


        /*
         * Calc the number of tasks in each state (run, sleep, etc)
         * Calc percent cpu usage for each task (pcpu)
         * Calc the cpu(s) percent in each state (user, system, nice, idle) */
static void frame_states (proc_t **p, int show)
{
   static HIST_t   *hist_sav = NULL;
   static unsigned  hist_siz;
   static int       hist_tot, showsav;
   static CPUS_t   *smpcpu;
   HIST_t          *hist_new;
   unsigned         total, running, sleeping, stopped, zombie;
   float            etime;
   int              i;

   if (!hist_sav) {
      hist_tot = 0;
         /* room for 512 HIST_t's (if Page_size == 4k) */
      hist_siz = (Page_size / sizeof(HIST_t));
      hist_sav = alloc_c(hist_siz);
         /* note: we allocate one more CPUS_t than Cpu_tot so that the last
                  slot can hold tics representing the /proc/stat cpu summary
                  (first line read)  -- that slot supports Show_cpusum */
      smpcpu = alloc_c((1 + Cpu_tot) * sizeof(CPUS_t));
      showsav = Show_cpusum;
   }

   hist_new = alloc_c(hist_siz);
   total = running = sleeping = stopped = zombie = 0;
   etime = time_elapsed();

      /* make a pass through the data to get stats */
   while (-1 != p[total]->pid) {
      TICS_t tics;
      proc_t *this = p[total];

      switch (this->state) {
         case 'S':
         case 'D':
            sleeping++;
            break;
         case 'T':
            stopped++;
            break;
         case 'Z':
            zombie++;
            break;
         case 'R':
            running++;
            break;
      }

      if (total * sizeof(HIST_t) >= hist_siz) {
         hist_siz += (Page_size / sizeof(HIST_t));
         hist_sav = alloc_r(hist_sav, hist_siz);
         hist_new = alloc_r(hist_new, hist_siz);
      }

         /* calculate time in this process; the sum of user time (utime)
            + system time (stime) -- but PLEASE dont waste time and effort
            calculating and saving data that goes unused, like the old top! */
      hist_new[total].pid  = this->pid;
      hist_new[total].tics = tics = (this->utime + this->stime);

         /* find matching entry from previous pass and make ticks elapsed */
      for (i = 0; i < hist_tot; i++) {
         if (this->pid == hist_sav[i].pid) {
            tics -= hist_sav[i].tics;
            break;
         }
      }

         /* finally calculate an integer version of %cpu for this task
            and plug it into the unfilled slot in proc_t */
      this->pcpu = (tics * 1000 / Hertz) / etime;
      if (this->pcpu > 999) this->pcpu = 999;

         /* if in Solaris mode, adjust cpu percentage not only for the cpu
            the process is running on, but for all cpus together */
      if (!Irix_mode) this->pcpu /= Cpu_tot;

      total++;
   } /* end: while 'p[total]->pid' */

   if (show) {
      FILE  *fp;

         /* whoa, we've changed modes -- gotta' clean old histories */
      if (Show_cpusum != showsav) {
            /* fresh start for the last slot in the history area */
         if (Show_cpusum) memset(&smpcpu[Cpu_tot], '\0', sizeof(CPUS_t));
            /* fresh start for the true smpcpu history area  */
         else memset(smpcpu, '\0', Cpu_tot * sizeof(CPUS_t));
         showsav = Show_cpusum;
      }

         /* display Task states */
      show_special(fmtmk(STATES_line1
         , total, running, sleeping, stopped, zombie));

         /* now arrange to calculate and display states for all Cpu's */
      if (!(fp = fopen("/proc/stat", "r")))
         std_err(fmtmk("Failed /proc/stat open: %s", strerror(errno)));

      if (Show_cpusum)
            /* retrieve and display just the 1st /proc/stat line */
         frame_smp(fp, CPU_FMTS_JUST1, &smpcpu[Cpu_tot], "Cpu(s) state:");
      else {
         char tmp[SMLBUFSIZ];

            /* skip the 1st line, which reflects total cpu states */
         if (!fgets(tmp, sizeof(tmp), fp))
            std_err("Failed /proc/stat read");
#ifdef TRAK_MAXBUFS
         MAXBUFS(frame_states, tmp);
#endif
            /* now do each cpu's states separately */
         for (i = 0; i < Cpu_tot; i++) {
            sprintf(tmp, "%-6scpu%-2d:"         /* [ cpu states as ]      */
               , i ? " " : "State"              /*    'State cpu0 : ... ' */
               , Irix_mode ? i : Cpu_map[i]);   /*    '      cpu1 : ... ' */
            frame_smp(fp, CPU_FMTS_MULTI, &smpcpu[i], tmp);
         }
      }

      fclose(fp);
   } /* end: if 'show' */

      /* save this frame's information */
   hist_tot = total;
   memcpy(hist_sav, hist_new, hist_siz);
   free(hist_new);
      /* finally, sort the processes on whatever... */
   qsort(p, total, sizeof(proc_t *), (QSORT_t)Sort_func);
}


        /*
         * Task display *Helper* function to handle highlighted
         * column transitions.  */
static void mkcol (unsigned idx, int sta, int *pad, char *buf, ...)
{
   char tmp[COLBUFSIZ];
   va_list va;

   va_start(va, buf);
   if (!Show_hicols || Sort_type != Fieldstab[idx].sort) {
      vsprintf(buf, Fieldstab[idx].fmts, va);
   } else {
      vsprintf(tmp, Fieldstab[idx].fmts, va);
      sprintf(buf, "%s%s", Row_color_high, tmp);
      *pad += Len_row_high;
      if (!Show_hirows || 'R' != sta) {
         strcat(buf, Row_color_norm);
         *pad += Len_row_norm;
      }
   }
   va_end(va);
#ifdef TRAK_MAXBUFS
   MAXBUFS(mkcol, tmp);
#endif
}


        /*
         * Displays information for a single task. */
#ifdef UGH_ITS_4_RH
static void show_a_task (proc_t *task, unsigned mempgs)
#else
static void show_a_task (proc_t *task)
#endif
{
   /* the following macro is used for those columns that are NOT sortable
      so as to avoid the function call overhead since mkcol cannot be made
      inline -- if additional sort columns are added, change the appropriate
      switch label's usage to lower case and thus invoke the real function */
#define MKCOL(idx,sta,pad,buf,arg) \
           sprintf(buf, Fieldstab[idx].fmts, arg)
   char rbuf[ROWBUFSIZ];
   int i, x, pad;

   pad = 0;
   rbuf[0] = '\0';

   for (i = 0; i < NumFields; i++) {
      char cbuf[COLBUFSIZ];
      unsigned f, s, w;

      cbuf[0] = '\0';
      f = PFlags[i];
      s = Fieldstab[f].scale;
      w = Fieldstab[f].width;

      switch (f) {
         case P_CMD:
         {  char *cmdptr, cmdnam[ROWBUFSIZ];

            if (Show_cmdlin) {
               cmdnam[0] = '\0';
               if (task->cmdline) {
                  x = 0;
                  do {
                     /* whoa, during a kernel build, parts of the make
                        process will create cmdlines in excess of 3000 bytes
                        but *without* the typical intervening nulls */
                     strcat(cmdnam
                        , fmtmk("%.*s ", Max_cmd, task->cmdline[x++]));
                     /* whoa, gnome's xscreensaver had a ^I in his cmdline
                        creating a line wrap when the window was maximized &
                        the tab came into view -- that in turn, whacked
                        our heading lines so we'll strim those !#@*#!... */
                     strim(1, cmdnam);
                        /* enough, don't ya think? */
                     if (Max_cmd < (int)strlen(cmdnam))
                        break;
                  } while (task->cmdline[x]);
               } else {
                  /* if this process really doesn't have a cmdline, we'll
                     consider it a kernel thread and display it uniquely
                     [ we need sort_cmd's complicity in this plot ] */
                  strcpy(cmdnam, fmtmk("( %s )", task->cmd));
               }
               cmdptr = cmdnam;
            } else
               cmdptr = task->cmd;
               /* hurry up, before cmdptr goes out of scope... */
            mkcol(f, task->state, &pad, cbuf, Max_cmd, Max_cmd, cmdptr);
         }
            break;
         case P_CODE:
            MKCOL(f, task->state, &pad, cbuf
               , scale_num(PAGES_2K(task->trs), w, s));
            break;
         case P_CPU:
            mkcol(f, task->state, &pad, cbuf, (float)task->pcpu / 10);
            break;
         case P_DATA:
            MKCOL(f, task->state, &pad, cbuf
               , scale_num(PAGES_2K(task->drs), w, s));
            break;
         case P_DIRTY:
            MKCOL(f, task->state, &pad, cbuf
               , scale_num((unsigned)task->dt, w, s));
            break;
         case P_FAULT:
            MKCOL(f, task->state, &pad, cbuf
               , scale_num(task->maj_flt, w, s));
            break;
         case P_FLAGS:
            MKCOL(f, task->state, &pad, cbuf, task->flags);
            for (x = 0; x < (int)strlen(cbuf); x++)
               if ('0' == cbuf[x]) cbuf[x] = '.';
            break;
         case P_GROUP:
            MKCOL(f, task->state, &pad, cbuf, task->egroup);
            break;
         case P_MEM:
            mkcol(f, task->state, &pad, cbuf
#ifdef UGH_ITS_4_RH
               , (float)task->resident * 100 / mempgs);
#else
               , (float)PAGES_2K(task->resident) * 100 / kb_main_total);
#endif
            break;
         case P_NCPU:
#ifdef UGH_ITS_4_RH
            MKCOL(f, task->state, &pad, cbuf, task->lproc);
#else
            MKCOL(f, task->state, &pad, cbuf, task->processor);
#endif
            break;
         case P_NI:
            MKCOL(f, task->state, &pad, cbuf, task->nice);
            break;
         case P_PID:
            mkcol(f, task->state, &pad, cbuf, task->pid);
            break;
         case P_PPID:
            MKCOL(f, task->state, &pad, cbuf, task->ppid);
            break;
         case P_PR:
            MKCOL(f, task->state, &pad, cbuf, task->priority);
            break;
         case P_RES:
               /* 'rss' vs 'resident' (which includes IO memory) ?
                  -- we'll ensure that VIRT = SWAP + RES */
            mkcol(f, task->state, &pad, cbuf
               , scale_num(PAGES_2K(task->resident), w, s));
            break;
         case P_SHR:
            MKCOL(f, task->state, &pad, cbuf
               , scale_num(PAGES_2K(task->share), w, s));
            break;
         case P_STA:
            MKCOL(f, task->state, &pad, cbuf, status(task));
            break;
         case P_SWAP:
            MKCOL(f, task->state, &pad, cbuf
               , scale_num(PAGES_2K(task->size - task->resident), w, s));
            break;
         case P_TIME:
         case P_TIME2:
         {  TICS_t t;

            t = task->utime + task->stime;
            if (Show_ctimes)
               t += (task->cutime + task->cstime);
            mkcol(f, task->state, &pad, cbuf, scale_tics(t, w));
         }
            break;
         case P_TTY:
         {  char tmp[TNYBUFSIZ];

            dev_to_tty(tmp, Fieldstab[f].width
               , task->tty, task->pid, ABBREV_DEV);
            mkcol(f, task->state, &pad, cbuf, tmp);
         }
            break;
         case P_UID:
            MKCOL(f, task->state, &pad, cbuf, task->euid);
            break;
         case P_USER:
            mkcol(f, task->state, &pad, cbuf, task->euser);
            break;
         case P_VIRT:
            MKCOL(f, task->state, &pad, cbuf
               , scale_num(PAGES_2K(task->size), w, s));
            break;
         case P_WCHAN:
            if (No_ksyms)
#ifdef UPCASE_HEXES
               MKCOL(f, task->state, &pad, cbuf
                  , fmtmk("x%08lX", (long)task->wchan));
#else
               MKCOL(f, task->state, &pad, cbuf
                  , fmtmk("x%08lx", (long)task->wchan));
#endif
            else
               MKCOL(f, task->state, &pad, cbuf, wchan(task->wchan));
            break;

        } /* end: switch 'PFlags[i]' */
#ifdef TRAK_MAXBUFS
        MAXBUFS(show_a_task, cbuf);
#endif
        strcat(rbuf, cbuf);

   } /* end: for 'NumFields' */
#ifdef TRAK_MAXBUFS
   MAXBUFS(show_a_task, rbuf);
#endif

   /* This row buffer could be stuffed with parameterized strings.
      We are thus advised to always use tputs/putp, but it works just
      fine with good ol' printf... */
   printf("\n%s%.*s%s%s"
      , (Show_hirows && 'R' == task->state) ? Row_color_high : Row_color_norm
      , Screen_cols + pad
      , rbuf
      , Caps_off
      , Cap_clr_eol);

#ifdef TRAK_MAXCAPS
   if (pad > Max_pads) Max_pads = pad;
   if (pad < Min_pads) Min_pads = pad;
      /* now that we have TRAK_MAXBUFS, the next two duplicate some effort */
   if ((int)strlen(rbuf) > Max_rbuf) Max_rbuf = (int)strlen(rbuf);
   if ((int)strlen(rbuf) < Min_rbuf) Min_rbuf = (int)strlen(rbuf);
#endif

#undef MKCOL
}


        /*
         * Read all process info and display it. */
static void show_everything (void)
{
   static proc_t **p_table = NULL;
   int ntask, nline;
#ifdef UGH_ITS_4_RH
   unsigned mempgs;
#endif

   if (!p_table) {
      p_table = readprocs(p_table);
      frame_states(p_table, 0);
      sleep(1);
   } else
      putp(Batch ? "\n" : Cap_home);

  /*
   ** Display Load averages */
   if (HSum_loadav)
      show_special(fmtmk(LOADAV_line, Myname, sprint_uptime()));

  /*
   ** Display System stats (also calc 'pcpu' and sort p_table) */
   p_table = readprocs(p_table);
   frame_states(p_table, HSum_states);

  /*
   ** Display Memory and Swap space usage */
#ifdef UGH_ITS_4_RH
   mempgs = frame_memory();
#else
   frame_memory();
#endif

  /*
   ** Display Headings for columns */
   printf("%s%s%s%s%s"
      , tg2(0, MSG_line + 1)
      , Hdr_color
      , ColHeadings
      , Caps_off
      , Cap_clr_eol);

   /* Finally!  Loop through to find each task, and display it ...
      ... lather, rinse, repeat */
   ntask = nline = 0;
   while (-1 != p_table[ntask]->pid && nline < Max_lines) {
      if ((Show_idleps
      || ('S' != p_table[ntask]->state && 'Z' != p_table[ntask]->state))
      && ((!ColUsername[0])
      || (!strcmp(ColUsername, p_table[ntask]->euser)) ) ) {
        /*
         ** Display a process Row */
#ifdef UGH_ITS_4_RH
         show_a_task(p_table[ntask], mempgs);
#else
         show_a_task(p_table[ntask]);
#endif
         if (!Batch) ++nline;
      }
      ++ntask;
   }

   printf("%s%s%s", Cap_clr_eos, tg2(0, MSG_line), Cap_clr_eol);
   fflush(stdout);
}


/*######  Entry point  ###################################################*/

int main (int dont_care_argc, char **argv)
{
   char not_really_tmp[OURPATHSZ];
   int i;

      /* setup our program name(s)... */
   Myname = strrchr(argv[0], '/');
   if (Myname) ++Myname; else Myname = argv[0];
   Myrealname = Myname;
   memset(not_really_tmp, '\0', sizeof(not_really_tmp));
      /* proper permissions should deny symlinks to /usr/bin for ordinary
         users, but root may have employed them -- Myrealname will be used
         in constructing the global rc filename ... */
   if (-1 != readlink(argv[0], not_really_tmp, sizeof(not_really_tmp) - 1)) {
      Myrealname = strrchr(not_really_tmp, '/');
      if (Myrealname) ++Myrealname; else Myrealname = not_really_tmp;
#ifdef TRAK_MAXBUFS
      MAXBUFS(main, not_really_tmp);
#endif
   }

      /* setup some important system stuff... */
   Page_size = getpagesize();
   Cpu_tot = sysconf(_SC_NPROCESSORS_ONLN);
   if (1 > Cpu_tot) Cpu_tot = 1;
   Cpu_map = alloc_r(NULL, sizeof(int) * Cpu_tot);
   for (i = 0; i < Cpu_tot; i++)
      Cpu_map[i] = i;

   rcfiles_read();
   parse_argvs(argv);
   terminal_set();
   window_resize(0);

      /* set up signal handlers */
   signal(SIGALRM,  stop);
   signal(SIGHUP,   stop);
   signal(SIGINT,   stop);
   signal(SIGPIPE,  stop);
   signal(SIGQUIT,  stop);
   signal(SIGTERM,  stop);
   signal(SIGTSTP,  suspend);
   signal(SIGTTIN,  suspend);
   signal(SIGTTOU,  suspend);
   signal(SIGCONT,  window_resize);
   signal(SIGWINCH, window_resize);

      /* loop, collecting process info and sleeping */
   do {
      struct timeval tv;
      fd_set fs;
      char c;

      show_everything();
      if (Msg_awaiting) show_msg(Msg_delayed);
      if (0 < Loops) --Loops;
      if (!Loops) stop(0);

      if (Batch)
         sleep((unsigned)Delay_time);
      else {
            /* Linux reports time not slept, so we must reinit every time */
         tv.tv_sec = Delay_time;
         tv.tv_usec = (Delay_time - (int)Delay_time) * 1000000;
         FD_ZERO(&fs);
         FD_SET(STDIN_FILENO, &fs);
         if (0 < select(STDIN_FILENO+1, &fs, NULL, NULL, &tv)
         &&  0 < chin(0, &c, 1))
            do_key((unsigned)c);
      }
   } while (1);

   return 0;
}
