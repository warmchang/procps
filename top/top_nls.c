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

#include "top.h"
#include "top_nls.h"


        /*
         * These are our three string tables with the following contents:
         *    Desc : fields descriptions not to exceed 20 screen positions
         *    Norm : regular text possibly also containing c-format specifiers
         *    Uniq : show_special specially formatted strings
         *
         * The latter table presents the greates translation challenge !
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

   snprintf(buf, sizeof(buf), "%s", _(
      "-------------------------------------------------------------------------------\n"
      "Note for Translators:\n"
      "   This group of single lines contains plain text only used as the\n"
      "   field descriptions.  Each translated line MUST be kept to a length\n"
      "   of 20 characters or less.\n"
      "\n"
      "( this text is for information only and need never be translated )\n"
      ""));

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

   snprintf(buf, sizeof(buf), "%s", _(
      "-------------------------------------------------------------------------------\n"
      "Note for Translators:\n"
      "   This group of lines contains both plain text and c-format strings.\n"
      "   Some strings reflect switches used to affect the running program\n"
      "   and should not be translated without also making corresponding\n"
      "   c-code logic changes.\n"
      "\n"
      "( this text is for information only and need never be translated )\n"
      ""));

   snprintf(buf, sizeof(buf), "%s", _(
      "\n"
      "\tsignal %d (%s) was caught by %s, please\n"
      "\tsee http://www.debian.org/Bugs/Reporting\n"
      ""));
   Norm_nlstab[EXIT_signals_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _(
      "inappropriate '%s'\n"
      "usage:\t%s%s"
      ""));
   Norm_nlstab[WRONG_switch_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _(
      "\t%s\n"
      "usage:\t%s%s"
      ""));
   Norm_nlstab[HELP_cmdline_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("failed /proc/stat open: %s"));
   Norm_nlstab[FAIL_statopn_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("failed openproc: %s"));
   Norm_nlstab[FAIL_openlib_txt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("bad delay interval '%s'"));
   Norm_nlstab[BAD_delayint_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("bad iterations argument '%s'"));
   Norm_nlstab[BAD_niterate_arg] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("pid limit (%d) exceeded"));
   Norm_nlstab[LIMIT_exceed_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("bad pid '%s'"));
   Norm_nlstab[BAD_mon_pids_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("-%c requires argument"));
   Norm_nlstab[MISSING_args_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("bad width arg '%s', must > %d"));
   Norm_nlstab[BAD_widtharg_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _(
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
   Norm_nlstab[DISABLED_cmd_fmt] = strdup(buf);

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

   snprintf(buf, sizeof(buf), "%s", _(
      "-------------------------------------------------------------------------------\n"
      "Note for Translators:\n"
      "   The next 11 text groups contain unprintable characters used to\n"
      "   index a capabilities table at run-time.  You may need a special\n"
      "   editor which can accomodate such data without altering it.\n"
      "\n"
      "   Please see the comments in the source file for additional\n"
      "   information and guidance regarding these strings.\n"
      "\n"
      "( this text is for information only and need never be translated )\n"
      ""));

        /*
         * These lines contain special formatting elements and are very
         * carefully designed to fit within a 80x24 terminal window.
         *    The special formatting consists of:
         *       "some text <_delimiter_> some more text <_delimiter_>...\n"
         *    Where <_delimiter_> is a single byte in the range of:
         *       \01 through \10  (in decimalizee, 1 - 8)
         *    and is used to select an 'attribute' from a capabilities table
         *    which is then applied to the *preceding* substring.
         * Once recognized, the delimiter is replaced with a null character
         * and viola, we've got a substring ready to output!  Strings or
         * substrings without delimiters will receive the Cap_norm attribute.
         *
         * note: the following is an example of the capabilities
         *       table for which the unprintable characters are
         *       used as an index.
         * +------------------------------------------------------+
         * | char *captab[] = {                 :   Cap's/Delim's |
         * |   Cap_norm, Cap_norm,              =   \00, \01,     |
         * |   cap_bold, capclr_sum,            =   \02, \03,     |
         * |   capclr_msg, capclr_pmt,          =   \04, \05,     |
         * |   capclr_hdr,                      =   \06,          |
         * |   capclr_rowhigh,                  =   \07,          |
         * |   capclr_rownorm  };               =   \10 [octal!]  |
         * +------------------------------------------------------+ */

   snprintf(buf, sizeof(buf), "%s", _(
      "Help for Interactive Commands\02 - %s\n"
      "Window \01%s\06: \01Cumulative mode \03%s\02.  \01System\06: \01Delay \03%.1f secs\02; \01Secure mode \03%s\02.\n"
      "\n"
      "  Z\05,\01B\05       Global: '\01Z\02' change color mappings; '\01B\02' disable/enable bold\n"
      "  l,t,m     Toggle Summaries: '\01l\02' load avg; '\01t\02' task/cpu stats; '\01m\02' mem info\n"
      "  1,I       Toggle SMP view: '\0011\02' single/separate states; '\01I\02' Irix/Solaris mode\n"
      "  f,F       Manage Fields: add/remove; change order; select sort field\n"
      "\n"
      "  <,>     . Move sort field: '\01<\02' next col left; '\01>\02' next col right\n"
      "  R,H,V   . Toggle: '\01R\02' norm/rev sort; '\01H\02' show threads; '\01V\02' forest view\n"
      "  c,i,S   . Toggle: '\01c\02' cmd name/line; '\01i\02' idle tasks; '\01S\02' cumulative time\n"
      "  x\05,\01y\05     . Toggle highlights: '\01x\02' sort field; '\01y\02' running tasks\n"
      "  z\05,\01b\05     . Toggle: '\01z\02' color/mono; '\01b\02' bold/reverse (only if 'x' or 'y')\n"
      "  u,U     . Show: '\01u\02' effective user; '\01U\02' real, saved, file or effective user\n"
      "  n or #  . Set maximum tasks displayed\n"
      "  C,...   . Toggle scroll coordinates msg for: \01up\02,\01down\02,\01left\02,right\02,\01home\02,\01end\02\n"
      "\n"
      "%s"
      "  W         Write configuration file\n"
      "  q         Quit\n"
      "          ( commands shown with '.' require a \01visible\02 task display \01window\02 ) \n"
      "Press '\01h\02' or '\01?\02' for help with \01Windows\02,\n"
      "any other key to continue "
      ""));
   Uniq_nlstab[KEYS_helpbas_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _(
      "  k,r       Manipulate tasks: '\01k\02' kill; '\01r\02' renice\n"
      "  d or s    Set update interval\n"
      ""));
   Uniq_nlstab[KEYS_helpext_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _(
      "Help for Windows / Field Groups\02 - \"Current Window\" = \01 %s \06\n"
      "\n"
      ". Use multiple \01windows\02, each with separate config opts (color,fields,sort,etc)\n"
      ". The 'current' window controls the \01Summary Area\02 and responds to your \01Commands\02\n"
      "  . that window's \01task display\02 can be turned \01Off\02 & \01On\02, growing/shrinking others\n"
      "  . with \01NO\02 task display, some commands will be \01disabled\02 ('i','R','n','c', etc)\n"
      "    until a \01different window\02 has been activated, making it the 'current' window\n"
      ". You \01change\02 the 'current' window by: \01 1\02) cycling forward/backward;\01 2\02) choosing\n"
      "  a specific field group; or\01 3\02) exiting the color mapping or fields screens\n"
      ". Commands \01available anytime   -------------\02\n"
      "    A       . Alternate display mode toggle, show \01Single\02 / \01Multiple\02 windows\n"
      "    g       . Choose another field group and make it 'current', or change now\n"
      "              by selecting a number from: \01 1\02 =%s;\01 2\02 =%s;\01 3\02 =%s; or\01 4\02 =%s\n"
      ". Commands \01requiring\02 '\01A\02' mode\01  -------------\02\n"
      "    G       . Change the \01Name\05 of the 'current' window/field group\n"
      " \01*\04  a , w   . Cycle through all four windows:  '\01a\05' Forward; '\01w\05' Backward\n"
      " \01*\04  - , _   . Show/Hide:  '\01-\05' \01Current\02 window; '\01_\05' all \01Visible\02/\01Invisible\02\n"
      "  The screen will be divided evenly between task displays.  But you can make\n"
      "  some \01larger\02 or \01smaller\02, using '\01n\02' and '\01i\02' commands.  Then later you could:\n"
      " \01*\04  = , +   . Rebalance tasks:  '\01=\05' \01Current\02 window; '\01+\05' \01Every\02 window\n"
      "              (this also forces the \01current\02 or \01every\02 window to become visible)\n"
      "\n"
      "In '\01A\02' mode, '\01*\04' keys are your \01essential\02 commands.  Please try the '\01a\02' and '\01w\02'\n"
      "commands plus the 'g' sub-commands NOW.  Press <Enter> to make 'Current' "
      ""));
   Uniq_nlstab[WINDOWS_help_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _(
      "Help for color mapping\02 - %s\n"
      "current window: \01%s\06\n"
      "\n"
      "   color - 04:25:44 up 8 days, 50 min,  7 users,  load average:\n"
      "   Tasks:\03  64 \02total,\03   2 \03running,\03  62 \02sleeping,\03   0 \02stopped,\03\n"
      "   %%Cpu(s):\03  76.5 \02user,\03  11.2 \02system,\03   0.0 \02nice,\03  12.3 \02idle\03\n"
      "   \01 Nasty Message! \04  -or-  \01Input Prompt\05\n"
      "   \01  PID TTY     PR  NI %%CPU    TIME+   VIRT SWAP S COMMAND    \06\n"
      "   17284 \10pts/2  \07  8   0  0.0   0:00.75  1380    0 S /bin/bash   \10\n"
      "   \01 8601 pts/1    7 -10  0.4   0:00.03   916    0 R color -b -z\07\n"
      "   11005 \10?      \07  9   0  0.0   0:02.50  2852 1008 S amor -sessi\10\n"
      "   available toggles: \01B\02 =disable bold globally (\01%s\02),\n"
      "       \01z\02 =color/mono (\01%s\02), \01b\02 =tasks \"bold\"/reverse (\01%s\02)\n"
      "\n"
      "Select \01target\02 as upper case letter:\n"
      "   S\02 = Summary Data,\01  M\02 = Messages/Prompts,\n"
      "   H\02 = Column Heads,\01  T\02 = Task Information\n"
      "Select \01color\02 as number:\n"
      "   0\02 = black,\01  1\02 = red,    \01  2\02 = green,\01  3\02 = yellow,\n"
      "   4\02 = blue, \01  5\02 = magenta,\01  6\02 = cyan, \01  7\02 = white\n"
      "\n"
      "Selected: \01target\02 \01 %c \04; \01color\02 \01 %d \04\n"
      "   press 'q' to abort changes to window '\01%s\02'\n"
      "   press 'a' or 'w' to commit & change another, <Enter> to commit and end    "
      ""));
   Uniq_nlstab[COLOR_custom_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _(
      "Fields Management\02 for window \01%s\06, whose current sort field is \01%s\02\n"
      "   Navigate with Up/Dn, Right selects for move then <Enter> or Left commits,\n"
      "   'd' or <Space> toggles display, 's' sets sort.  Use 'q' or <Esc> to end! "
      ""));
   Uniq_nlstab[FIELD_header_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("%s:\03"
      " %3u \02total,\03 %3u \02running,\03 %3u \02sleeping,\03 %3u \02stopped,\03 %3u \02zombie\03\n"
      ""));
   Uniq_nlstab[STATE_line_1_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("%%%s\03"
      " %#5.1f  \02user,\03 %#5.1f  \02system,\03 %#5.1f  \02nice,\03 %#5.1f  \02idle\03\n"
      ""));
   Uniq_nlstab[STATE_lin2x4_fmt] = strdup(buf);

        /* These are the meanings for abbreviations used below:
         *    lnx 2.5.x, procps-3.0.5  : IO-wait = i/o wait time
         *    lnx 2.6.x, procps-3.1.12 : IO-wait now wa, hi = hard irq, si = soft irq
         *    lnx 2.7.x, procps-3.2.7  : st = steal time */
   snprintf(buf, sizeof(buf), "%s", _("%%%s\03"
      " %#5.1f  \02user,\03 %#5.1f  \02system,\03 %#5.1f  \02nice,\03 %#5.1f  \02idle,\03 %#5.1f  \02IO-wait\03\n"
      ""));
   Uniq_nlstab[STATE_lin2x5_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("%%%s\03"
      " %#5.1f \02us,\03 %#5.1f \02sy,\03 %#5.1f \02ni,\03 %#5.1f \02id,\03 %#5.1f \02wa,\03 %#5.1f \02hi,\03 %#5.1f \02si\03\n"
      ""));
   Uniq_nlstab[STATE_lin2x6_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _("%%%s\03"
      "%#5.1f \02us,\03%#5.1f \02sy,\03%#5.1f \02ni,\03%#5.1f \02id,\03%#5.1f \02wa,\03%#5.1f \02hi,\03%#5.1f \02si,\03%#5.1f \02st\03\n"
      ""));
   Uniq_nlstab[STATE_lin2x7_fmt] = strdup(buf);

   snprintf(buf, sizeof(buf), "%s", _(
      "%s Mem: \03 %8lu \02total,\03 %8lu \02used,\03 %8lu \02free,\03 %8lu \02buffers\03\n"
      "%s Swap:\03 %8lu \02total,\03 %8lu \02used,\03 %8lu \02free,\03 %8lu \02cached\03\n"
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
