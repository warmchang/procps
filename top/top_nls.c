/* top_nls.c - provide the basis for future nls translations */
/*
 * Copyright (c) 2011,          by: James C. Warner
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
 *    Sami Kerola, <kerolasa@iki.fi>
 */

#include <stdio.h>
#include <string.h>

#include "../include/nls.h"

#include "top.h"
#include "top_nls.h"

        /* Programmer Note: Unless you have *something* following the gettext
         .                  macro, gettext will refuse to see any TRANSLATORS
         .                  comments.  Thus empty strings have been added for
         .                  potential future comment additions.
         .
         .    /* TRANSLATORS: ...
         .    snprintf(buf, sizeof(buf), "%s", _(           // unseen comment
         .
         .    /* TRANSLATORS: ...
         .    snprintf(buf, sizeof(buf), "%s", _(""         // now it's seen!
         */


        /*
         * These are our three string tables with the following contents:
         *    Desc : fields descriptions not to exceed 20 screen positions
         *    Norm : regular text possibly also containing c-format specifiers
         *    Uniq : show_special specially formatted strings
         *
         * The latter table presents the greatest translation challenge !
         */
const char *Desc_nlstab[P_MAXPFLGS];
const char *Norm_nlstab[norm_MAX];
const char *Uniq_nlstab[uniq_MAX];


        /*
         * This routine builds the nls table containing plain text only
         * used as the field descriptions.  Each translated line MUST be
         * kept to a maximum of 20 characters or less! */
static void build_desc_nlstab (void) {
   char buf[SMLBUFSIZ];

/* -----------------------------------------------------------------------
.  Note for Translators:
.     The following single lines contain only plain text used as
.     the descriptions under Field Management when the 'f' key is typed.
.
.     To avoid truncation, each translated line MUST be kept to a length
.     of 20 characters or less.\n"
. */

   snprintf(buf, sizeof(buf), "%s", _("Process Id"));
   Desc_nlstab[P_PID] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Parent Process pid"));
   Desc_nlstab[P_PPD] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Effective User Id"));
   Desc_nlstab[P_UED] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Effective User Name"));
   Desc_nlstab[P_UEN] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Real User Id"));
   Desc_nlstab[P_URD] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Real User Name"));
   Desc_nlstab[P_URN] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Saved User Id"));
   Desc_nlstab[P_USD] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Saved User Name"));
   Desc_nlstab[P_USN] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Group Id"));
   Desc_nlstab[P_GID] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Group Name"));
   Desc_nlstab[P_GRP] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Process Group Id"));
   Desc_nlstab[P_PGD] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Controlling Tty"));
   Desc_nlstab[P_TTY] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Tty Process Grp Id"));
   Desc_nlstab[P_TPG] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Session Id"));
   Desc_nlstab[P_SID] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Priority"));
   Desc_nlstab[P_PRI] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Nice Value"));
   Desc_nlstab[P_NCE] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Number of Threads"));
   Desc_nlstab[P_THD] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Last Used Cpu (SMP)"));
   Desc_nlstab[P_CPN] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("CPU Usage"));
   Desc_nlstab[P_CPU] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("CPU Time"));
   Desc_nlstab[P_TME] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("CPU Time, hundredths"));
   Desc_nlstab[P_TM2] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Memory Usage (RES)"));
   Desc_nlstab[P_MEM] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Virtual Image (kb)"));
   Desc_nlstab[P_VRT] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Swapped Size (kb)"));
   Desc_nlstab[P_SWP] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Resident Size (kb)"));
   Desc_nlstab[P_RES] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Code Size (kb)"));
   Desc_nlstab[P_COD] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Data+Stack Size (kb)"));
   Desc_nlstab[P_DAT] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Shared Mem Size (kb)"));
   Desc_nlstab[P_SHR] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Major Page Faults"));
   Desc_nlstab[P_FL1] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Minor Page Faults"));
   Desc_nlstab[P_FL2] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Dirty Pages Count"));
   Desc_nlstab[P_DRT] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Process Status"));
   Desc_nlstab[P_STA] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Command Name/Line"));
   Desc_nlstab[P_CMD] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Sleeping in Function"));
   Desc_nlstab[P_WCH] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Task Flags <sched.h>"));
   Desc_nlstab[P_FLG] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Control Groups"));
   Desc_nlstab[P_CGR] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Supp Groups IDs"));
   Desc_nlstab[P_SGD] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Supp Groups Names"));
   Desc_nlstab[P_SGN] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("Thread Group Id"));
#ifdef OOMEM_ENABLE
   Desc_nlstab[P_TGD] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("oom_adjustment (2^X)"));
   Desc_nlstab[P_OOA] = strdup(buf);
   snprintf(buf, sizeof(buf), "%s", _("oom_score (badness)"));
   Desc_nlstab[P_OOM] = strdup(buf);
#endif
}


        /*
         * This routine builds the nls table containing both plain text
         * and regular c-format strings. */
static void build_norm_nlstab (void) {
   char buf[MEDBUFSIZ];

/* -----------------------------------------------------------------------
.  Note for Translators:
.     This group of lines contains both plain text and c-format strings.
.
.     Some strings reflect switches used to affect the running program
.     and should not be translated without also making corresponding
.     c-code logic changes.
. */

   snprintf(buf, sizeof(buf), "%s", _(""
      "\tsignal %d (%s) was caught by %s, please\n"
      "\tsee http://www.debian.org/Bugs/Reporting\n"
      ""));
   Norm_nlstab[EXIT_signals_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _(""
      "inappropriate '%s'\n"
      "usage:\t%s%s"
      ""));
   Norm_nlstab[WRONG_switch_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _(""
      "\t%s\n"
      "usage:\t%s%s"
      ""));
   Norm_nlstab[HELP_cmdline_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("failed /proc/stat open: %s"));
   Norm_nlstab[FAIL_statopn_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("failed openproc: %s"));
   Norm_nlstab[FAIL_openlib_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("bad delay interval '%s'"));
   Norm_nlstab[BAD_delayint_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("bad iterations argument '%s'"));
   Norm_nlstab[BAD_niterate_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("pid limit (%d) exceeded"));
   Norm_nlstab[LIMIT_exceed_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("bad pid '%s'"));
   Norm_nlstab[BAD_mon_pids_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("-%c requires argument"));
   Norm_nlstab[MISSING_args_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("bad width arg '%s', must > %d"));
   Norm_nlstab[BAD_widtharg_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _(""
      "unknown option '%c'\n"
      "usage:\t%s%s"
      ""));
   Norm_nlstab[UNKNOWN_opts_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("-d disallowed in \"secure\" mode"));
   Norm_nlstab[DELAY_secure_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("-d requires positive argument"));
   Norm_nlstab[DELAY_badarg_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("On"));
   Norm_nlstab[ON_word_only_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Off"));
   Norm_nlstab[OFF_one_word_txt] = strdup(buf);

/* Translation Hint: Only the following words should be translated
   .                 delay, limit, user, cols */
   snprintf(buf, sizeof(buf), "%s", _(" -hv | -bcHiSs -d delay -n limit -u|U user | -p pid[,pid] -w [cols]"));
   Norm_nlstab[USAGE_abbrev_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("failed /proc/stat read"));
   Norm_nlstab[FAIL_statget_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Forest mode %s"));
   Norm_nlstab[FOREST_modes_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("failed tty get"));
   Norm_nlstab[FAIL_tty_get_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("failed Tty_tweaked set: %s"));
   Norm_nlstab[FAIL_tty_mod_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("failed Tty_raw set: %s"));
   Norm_nlstab[FAIL_tty_raw_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Choose field group (1 - 4)"));
   Norm_nlstab[CHOOSE_group_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Command disabled, 'A' mode required"));
   Norm_nlstab[DISABLED_cmd_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Command disabled, activate %s with '-' or '_'"));
   Norm_nlstab[DISABLED_win_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("No colors to map!"));
   Norm_nlstab[COLORS_nomap_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Failed '%s' open: %s"));
   Norm_nlstab[FAIL_rc_open_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Wrote configuration to '%s'"));
   Norm_nlstab[WRITE_rcfile_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Change delay from %.1f to"));
   Norm_nlstab[DELAY_change_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Show threads %s"));
   Norm_nlstab[THREADS_show_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Irix mode %s"));
   Norm_nlstab[IRIX_curmode_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("pid to signal/kill"));
   Norm_nlstab[GET_pid2kill_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Send pid %d signal [%d/sigterm]"));
   Norm_nlstab[GET_sigs_num_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Failed signal pid '%d' with '%d': %s"));
   Norm_nlstab[FAIL_signals_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Invalid signal"));
   Norm_nlstab[BAD_signalid_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("PID to renice"));
   Norm_nlstab[GET_pid2nice_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Renice PID %d to value"));
   Norm_nlstab[GET_nice_num_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Failed renice of PID %d to %d: %s"));
   Norm_nlstab[FAIL_re_nice_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Rename window '%s' to (1-3 chars)"));
   Norm_nlstab[NAME_windows_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Cumulative time %s"));
   Norm_nlstab[TIME_accumed_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Maximum tasks = %d, change to (0 is unlimited)"));
   Norm_nlstab[GET_max_task_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Invalid maximum"));
   Norm_nlstab[BAD_max_task_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Nothing to highlight!"));
   Norm_nlstab[HILIGHT_cant_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Which user (blank for all)"));
   Norm_nlstab[GET_user_ids_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Unknown command - try 'h' for help"));
   Norm_nlstab[UNKNOWN_cmds_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("scroll coordinates: y = %d/%d (tasks), x = %d/%d (fields)"));
   Norm_nlstab[SCROLL_coord_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("failed memory allocate"));
   Norm_nlstab[FAIL_alloc_c_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("failed memory re-allocate"));
   Norm_nlstab[FAIL_alloc_r_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Unacceptable floating point"));
   Norm_nlstab[BAD_numfloat_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Invalid user"));
   Norm_nlstab[BAD_username_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("failed sigprocmask, SIG_BLOCK: %s"));
   Norm_nlstab[FAIL_sigstop_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("failed sigprocmask, SIG_SETMASK: %s"));
   Norm_nlstab[FAIL_sigmask_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("forest view"));
   Norm_nlstab[FOREST_views_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("failed pid maximum size test"));
   Norm_nlstab[FAIL_widepid_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("failed number of cpus test"));
   Norm_nlstab[FAIL_widecpu_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("incompatible rcfile, you should delete '%s'"));
   Norm_nlstab[RC_bad_files_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("window entry #%d corrupt, please delete '%s'"));
   Norm_nlstab[RC_bad_entry_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Unavailable in secure mode"));
   Norm_nlstab[NOT_onsecure_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Only 1 cpu detected"));
   Norm_nlstab[NOT_smp_cpus_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Unacceptable integer"));
   Norm_nlstab[BAD_integers_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("conflicting process selections (U/p/u)"));
   Norm_nlstab[SELECT_clash_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Kb"));
   Norm_nlstab[AMT_kilobyte_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Mb"));
   Norm_nlstab[AMT_megabyte_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Gb"));
   Norm_nlstab[AMT_gigabyte_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Threads"));
   Norm_nlstab[WORD_threads_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("Tasks"));
   Norm_nlstab[WORD_process_txt] = strdup(buf);
}


        /*
         * This routine builds the nls table containing specially
         * formatted strings designed to fit within an 80x24 terminal. */
static void build_uniq_nsltab (void) {
   char buf[BIGBUFSIZ];

/* -----------------------------------------------------------------------
.  Note for Translators:
.     The next several text groups contain special escape sequences
.     representing values used to index a table at run-time.
.
.     Each such sequence consists of a slash and exactly 3 numbers.
.     Examples would be '\002', '\020', etc.  In at least one case,
.     another number follows those 3 numbers, making it appear as
.     though the escape sequence is \0011.
.
.     If you remove those escape sequences, it would make translation
.     easier.  However, the ability to display colors and bold text at
.     run-time will have been lost.
.
.     Additionally, each of these text groups was designed to display
.     in a 80x24 terminal window.  Hopefully, any translations will
.     adhere to that goal lest the translated text be truncated.
.
.     If you would like additional information regarding these strings,
.     please see the prolog to the show_special function in the top.c
.     source file.
. */

   snprintf(buf, sizeof(buf), "%s", _(""
      "Help for Interactive Commands\002 - %s\n"
      "Window \001%s\006: \001Cumulative mode \003%s\002.  \001System\006: \001Delay \003%.1f secs\002; \001Secure mode \003%s\002.\n"
      "\n"
      "  Z\005,\001B\005       Global: '\001Z\002' change color mappings; '\001B\002' disable/enable bold\n"
      "  l,t,m     Toggle Summaries: '\001l\002' load avg; '\001t\002' task/cpu stats; '\001m\002' mem info\n"
      "  1,I       Toggle SMP view: '\0011\002' single/separate states; '\001I\002' Irix/Solaris mode\n"
      "  f,F       Manage Fields: add/remove; change order; select sort field\n"
      "\n"
      "  <,>     . Move sort field: '\001<\002' next col left; '\001>\002' next col right\n"
      "  R,H,V   . Toggle: '\001R\002' norm/rev sort; '\001H\002' show threads; '\001V\002' forest view\n"
      "  c,i,S   . Toggle: '\001c\002' cmd name/line; '\001i\002' idle tasks; '\001S\002' cumulative time\n"
      "  x\005,\001y\005     . Toggle highlights: '\001x\002' sort field; '\001y\002' running tasks\n"
      "  z\005,\001b\005     . Toggle: '\001z\002' color/mono; '\001b\002' bold/reverse (only if 'x' or 'y')\n"
      "  u,U     . Show: '\001u\002' effective user; '\001U\002' real, saved, file or effective user\n"
      "  n or #  . Set maximum tasks displayed\n"
      "  C,...   . Toggle scroll coordinates msg for: \001up\002,\001down\002,\001left\002,right\002,\001home\002,\001end\002\n"
      "\n"
      "%s"
      "  W         Write configuration file\n"
      "  q         Quit\n"
      "          ( commands shown with '.' require a \001visible\002 task display \001window\002 ) \n"
      "Press '\001h\002' or '\001?\002' for help with \001Windows\002,\n"
      "any other key to continue "
      ""));
   Uniq_nlstab[KEYS_helpbas_fmt] = strdup(buf);

/* Translation Hint: As is true for the text above, the "keys" shown to the left and
   .                 also imbedded in the translatable text (along with escape seqs)
   .                 should never themselves be translated. */
   snprintf(buf, sizeof(buf), "%s", _(""
      "  k,r       Manipulate tasks: '\001k\002' kill; '\001r\002' renice\n"
      "  d or s    Set update interval\n"
      ""));
   Uniq_nlstab[KEYS_helpext_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _(""
      "Help for Windows / Field Groups\002 - \"Current Window\" = \001 %s \006\n"
      "\n"
      ". Use multiple \001windows\002, each with separate config opts (color,fields,sort,etc)\n"
      ". The 'current' window controls the \001Summary Area\002 and responds to your \001Commands\002\n"
      "  . that window's \001task display\002 can be turned \001Off\002 & \001On\002, growing/shrinking others\n"
      "  . with \001NO\002 task display, some commands will be \001disabled\002 ('i','R','n','c', etc)\n"
      "    until a \001different window\002 has been activated, making it the 'current' window\n"
      ". You \001change\002 the 'current' window by: \001 1\002) cycling forward/backward;\001 2\002) choosing\n"
      "  a specific field group; or\001 3\002) exiting the color mapping or fields screens\n"
      ". Commands \001available anytime   -------------\002\n"
      "    A       . Alternate display mode toggle, show \001Single\002 / \001Multiple\002 windows\n"
      "    g       . Choose another field group and make it 'current', or change now\n"
      "              by selecting a number from: \001 1\002 =%s;\001 2\002 =%s;\001 3\002 =%s; or\001 4\002 =%s\n"
      ". Commands \001requiring\002 '\001A\002' mode\001  -------------\002\n"
      "    G       . Change the \001Name\005 of the 'current' window/field group\n"
      " \001*\004  a , w   . Cycle through all four windows:  '\001a\005' Forward; '\001w\005' Backward\n"
      " \001*\004  - , _   . Show/Hide:  '\001-\005' \001Current\002 window; '\001_\005' all \001Visible\002/\001Invisible\002\n"
      "  The screen will be divided evenly between task displays.  But you can make\n"
      "  some \001larger\002 or \001smaller\002, using '\001n\002' and '\001i\002' commands.  Then later you could:\n"
      " \001*\004  = , +   . Rebalance tasks:  '\001=\005' \001Current\002 window; '\001+\005' \001Every\002 window\n"
      "              (this also forces the \001current\002 or \001every\002 window to become visible)\n"
      "\n"
      "In '\001A\002' mode, '\001*\004' keys are your \001essential\002 commands.  Please try the '\001a\002' and '\001w\002'\n"
      "commands plus the 'g' sub-commands NOW.  Press <Enter> to make 'Current' "
      ""));
   Uniq_nlstab[WINDOWS_help_fmt] = strdup(buf);

/* -----------------------------------------------------------------------
.  Note for Translators:
.     The following 'Help for color mapping' simulated screen should
.     probably NOT be translated due to complications caused by the
.     xgettext program.
.
.     Some escape sequences will be converted as follows and there is
.     unfortunately NO way to prevent it.
.        \007  -->  \a
.        \010  -->  \b
.
.     This means they will be lost in the clutter of other text.  Besides,
.     the simulated screen is terribly hard to follow in this form and any
.     translation will likely produce extremly unpleasing results that are
.     unlikely to parallel the running top program.
. */
   snprintf(buf, sizeof(buf), "%s", _(""
      "Help for color mapping\002 - %s\n"
      "current window: \001%s\006\n"
      "\n"
      "   color - 04:25:44 up 8 days, 50 min,  7 users,  load average:\n"
      "   Tasks:\003  64 \002total,\003   2 \003running,\003  62 \002sleeping,\003   0 \002stopped,\003\n"
      "   %%Cpu(s):\003  76.5 \002user,\003  11.2 \002system,\003   0.0 \002nice,\003  12.3 \002idle\003\n"
      "   \001 Nasty Message! \004  -or-  \001Input Prompt\005\n"
      "   \001  PID TTY     PR  NI %%CPU    TIME+   VIRT SWAP S COMMAND    \006\n"
      "   17284 \010pts/2  \007  8   0  0.0   0:00.75  1380    0 S /bin/bash   \010\n"
      "   \001 8601 pts/1    7 -10  0.4   0:00.03   916    0 R color -b -z\007\n"
      "   11005 \010?      \007  9   0  0.0   0:02.50  2852 1008 S amor -sessi\010\n"
      "   available toggles: \001B\002 =disable bold globally (\001%s\002),\n"
      "       \001z\002 =color/mono (\001%s\002), \001b\002 =tasks \"bold\"/reverse (\001%s\002)\n"
      "\n"
      "Select \001target\002 as upper case letter:\n"
      "   S\002 = Summary Data,\001  M\002 = Messages/Prompts,\n"
      "   H\002 = Column Heads,\001  T\002 = Task Information\n"
      "Select \001color\002 as number:\n"
      "   0\002 = black,\001  1\002 = red,    \001  2\002 = green,\001  3\002 = yellow,\n"
      "   4\002 = blue, \001  5\002 = magenta,\001  6\002 = cyan, \001  7\002 = white\n"
      "\n"
      "Selected: \001target\002 \001 %c \004; \001color\002 \001 %d \004\n"
      "   press 'q' to abort changes to window '\001%s\002'\n"
      "   press 'a' or 'w' to commit & change another, <Enter> to commit and end    "
      ""));
   Uniq_nlstab[COLOR_custom_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _(""
      "Fields Management\002 for window \001%s\006, whose current sort field is \001%s\002\n"
      "   Navigate with Up/Dn, Right selects for move then <Enter> or Left commits,\n"
      "   'd' or <Space> toggles display, 's' sets sort.  Use 'q' or <Esc> to end! "
      ""));
   Uniq_nlstab[FIELD_header_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("%s:\003"
      " %3u \002total,\003 %3u \002running,\003 %3u \002sleeping,\003 %3u \002stopped,\003 %3u \002zombie\003\n"
      ""));
   Uniq_nlstab[STATE_line_1_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("%%%s\003"
      " %#5.1f  \002user,\003 %#5.1f  \002system,\003 %#5.1f  \002nice,\003 %#5.1f  \002idle\003\n"
      ""));
   Uniq_nlstab[STATE_lin2x4_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("%%%s\003"
      " %#5.1f  \002user,\003 %#5.1f  \002system,\003 %#5.1f  \002nice,\003 %#5.1f  \002idle,\003 %#5.1f  \002IO-wait\003\n"
      ""));
   Uniq_nlstab[STATE_lin2x5_fmt] = strdup(buf);

/* Translation Hint: Only the following abbreviations need be translated
   .                 us = user, sy = system, ni = nice, id = idle, wa = wait,
   .                 hi hardware interrupt, si = software interrupt */
   snprintf(buf, sizeof(buf), "%s", _("%%%s\003"
      " %#5.1f \002us,\003 %#5.1f \002sy,\003 %#5.1f \002ni,\003 %#5.1f \002id,\003 %#5.1f \002wa,\003 %#5.1f \002hi,\003 %#5.1f \002si\003\n"
      ""));
   Uniq_nlstab[STATE_lin2x6_fmt] = strdup(buf);

/* Translation Hint: Only the following abbreviations need be translated
   .                 us = user, sy = system, ni = nice, id = idle, wa = wait,
   .                 hi hardware interrupt, si = software interrupt, st = steal time */
   snprintf(buf, sizeof(buf), "%s", _("%%%s\003"
      "%#5.1f \002us,\003%#5.1f \002sy,\003%#5.1f \002ni,\003%#5.1f \002id,\003%#5.1f \002wa,\003%#5.1f \002hi,\003%#5.1f \002si,\003%#5.1f \002st\003\n"
      ""));
   Uniq_nlstab[STATE_lin2x7_fmt] = strdup(buf);

/* Translation Hint: Only the following need be translated
   .                 abbreviations: Mem = physical memory/ram, Swap = the linux swap file
   .                 words:         total, used, free, buffers, cached */
   snprintf(buf, sizeof(buf), "%s", _(""
      "%s Mem: \003 %8lu \002total,\003 %8lu \002used,\003 %8lu \002free,\003 %8lu \002buffers\003\n"
      "%s Swap:\003 %8lu \002total,\003 %8lu \002used,\003 %8lu \002free,\003 %8lu \002cached\003\n"
      ""));
   Uniq_nlstab[MEMORY_lines_fmt] = strdup(buf);
}


        /*
         * Duh... */
void initialize_nsl (void) {
   build_desc_nlstab();
   build_norm_nlstab();
   build_uniq_nsltab();
}
