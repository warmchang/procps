/*
 * help.c - ps help output
 * Copyright 1998-2004 by Albert Cahalan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"


enum {
  HELP_SMP, HELP_LST, HELP_OUT,
  HELP_THD, HELP_MSC, HELP_ALL,
  HELP_default
};

static struct {
  const char *word;
  const char *abrv;
} help_tab[HELP_default];


static int parse_help_opt (const char *opt) {
/* Translation Notes for ps Help #1 ---------------------------------
   .  This next group of lines represents 6 pairs of words + abbreviations
   .  which are the basis of the 'ps' program help text.
   .
   .  The words and abbreviations you provide will alter program behavior.
   .  They will also appear in the help usage summary associated with the
   .  "Notes for ps Help #2" below.
   .
   .  In their English form, help text would look like this:
   .      Try 'ps --help <simple|list|output|threads|misc|all>'
   .       or 'ps --help <s|l|o|t|m|a>'
   .      for additional help text.
   .
   .  When translating these 6 pairs you may choose any appropriate
   .  language equivalents and the only requirement is the abbreviated
   .  representations must be unique.
   .
   .  By default, those abbreviations are single characters.  However,
   .  they are not limited to only one character after translation.
   . */

/* Translation Hint, Pair #1 */
  help_tab[HELP_SMP].word = _("simple"); help_tab[HELP_SMP].abrv = _("s");
/* Translation Hint, Pair #2 */
  help_tab[HELP_LST].word  = _("list"); help_tab[HELP_LST].abrv = _("l");
/* Translation Hint, Pair #3 */
  help_tab[HELP_OUT].word = _("output"); help_tab[HELP_OUT].abrv = _("o");
/* Translation Hint, Pair #4 */
  help_tab[HELP_THD].word = _("threads"); help_tab[HELP_THD].abrv = _("t");
/* Translation Hint, Pair #5 */
  help_tab[HELP_MSC].word = _("misc"); help_tab[HELP_MSC].abrv = _("m");
/* Translation Hint, Pair #6 */
  help_tab[HELP_ALL].word = _("all"); help_tab[HELP_ALL].abrv = _("a");
/*
 * the above are doubled on each line so they carry the same .pot
 * line # reference and thus appear more like true "pairs" even
 * though xgettext will produce separate msgid/msgstr groups */

  if(opt) {
    int i;
    for (i = HELP_SMP; i < HELP_default; i++)
      if (!strcmp(opt, help_tab[i].word) || !strcmp(opt, help_tab[i].abrv))
        return i;
  }
  return HELP_default;
}


void do_help (const char *opt, int rc) NORETURN;
void do_help (const char *opt, int rc) {
  FILE *out = (rc == EXIT_SUCCESS) ? stdout : stderr;
  int section = parse_help_opt(opt);

  fprintf(out, _("\n"
    "Usage:\n"
    " %s [options]\n"), myname);

  if (section == HELP_SMP || section == HELP_ALL) {
    fprintf(out, _("\n"
      "Basic options:\n"
      " -A, -e               all processes\n"
      " -a                   all with tty, except session leaders\n"
      "  a                   all with tty, including other users\n"
      " -d                   all except session leaders\n"
      " -N, --deselect       negate selection\n"
      "  r                   only running processes\n"
      "  T                   all processes on this terminal\n"
      "  x                   processes without controlling ttys\n"));
  }
  if (section == HELP_LST || section == HELP_ALL) {
    fprintf(out, _("\n"
      "Selection by list:\n"
      " -C <command>         command name\n"
      " -G, --Group <gid>    real group id or name\n"
      " -g, --group <group>  session or effective group name\n"
      " -p, --pid <pid>      process id\n"
      "     --ppid <pid>     select by parent process id\n"
      " -s, --sid <session>  session id\n"
      " -t, t, --tty <tty>   terminal\n"
      " -u, U, --user <uid>  effective user id or name\n"
      " -U, --User <uid>     real user id or name\n"
      "\n"
      "  selection <arguments> take either:\n"
      "    comma-separated list e.g. '-u root,nobody' or\n"
      "    blank-separated list e.g. '-p 123 4567'\n"));
  }
  if (section == HELP_OUT || section == HELP_ALL) {
    fprintf(out, _("\n"
      "Output formats:\n"
      " -F                   extra full\n"
      " -f                   full-format, including command lines\n"
      "  f, --forest         ascii art process tree\n"
      " -H                   show process hierarchy\n"
      " -j                   jobs format\n"
      "  j                   BSD job control format\n"
      " -l                   long format\n"
      "  l                   BSD long format\n"
      " -M, Z                add security data (for SELinux)\n"
      " -O <format>          preloaded with default columns\n"
      "  O <format>          as -O, with BSD personality\n"
      " -o, o, --format <format>\n"
      "                      user defined format\n"
      "  s                   signal format\n"
      "  u                   user-oriented format\n"
      "  v                   virtual memory format\n"
      "  X                   register format\n"
      " -y                   do not show flags, show rrs vs. addr (used with -l)\n"
      "     --context        display security context (for SELinux)\n"
      "     --headers        repeat header lines, one per page\n"
      "     --no-headers     do not print header at all\n"
      "     --cols, --columns, --width <num>\n"
      "                      set screen width\n"
      "     --rows, --lines <num>\n"
      "                      set screen height\n"));
  }
  if (section == HELP_THD || section == HELP_ALL) {
    fprintf(out, _("\n"
      "Show threads:\n"
      "  H                   as if they where processes\n"
      " -L                   possibly with LWP and NLWP columns\n"
      " -m, m                after processes\n"
      " -T                   possibly with SPID column\n"));
  }
  if (section == HELP_MSC || section == HELP_ALL) {
    fprintf(out, _("\n"
      "Miscellaneous options:\n"
      " -c                   show scheduling class with -l option\n"
      "  c                   show true command name\n"
      "  e                   show the environment after command\n"
      "  k,    --sort        specify sort order as: [+|-]key[,[+|-]key[,...]]\n"
      "  L                   list format specifiers\n"
      "  n                   display numeric uid and wchan\n"
      "  S,    --cumulative  include some dead child process data\n"
      " -y                   do not show flags, show rss (only with -l)\n"
      " -V, V, --version     display version information and exit\n"
      " -w, w                unlimited output width\n"
      "\n"
      "        --%s <%s|%s|%s|%s|%s|%s>\n"
      "                      display help and exit\n")
        , the_word_help
        , help_tab[HELP_SMP].word, help_tab[HELP_LST].word
        , help_tab[HELP_OUT].word, help_tab[HELP_THD].word
        , help_tab[HELP_MSC].word, help_tab[HELP_ALL].word);
  }
  if (section == HELP_default) {
/* Translation Notes for ps Help #2 ---------------------------------
   .  Most of the following c-format string is derived from the 6
   .  pairs of words + chars mentioned above in "Notes for ps Help #1".
   .
   .  In its full English form, help text would look like this:
   .      Try 'ps --help <simple|list|output|threads|misc|all>'
   .       or 'ps --help <s|l|o|t|m|a>'
   .      for additional help text.
   .
   .  The word for "help" will be translated elsewhere.  Thus, the only
   .  translations below will be: "Try", "or" and "for additional...".
   . */
    fprintf(out, _("\n"
      " Try '%s --%s <%s|%s|%s|%s|%s|%s>'\n"
      "  or '%s --%s <%s|%s|%s|%s|%s|%s>'\n"
      " for additional help text.\n")
        , myname, the_word_help
        , help_tab[HELP_SMP].word, help_tab[HELP_LST].word
        , help_tab[HELP_OUT].word, help_tab[HELP_THD].word
        , help_tab[HELP_MSC].word, help_tab[HELP_ALL].word
        , myname, the_word_help
        , help_tab[HELP_SMP].abrv, help_tab[HELP_LST].abrv
        , help_tab[HELP_OUT].abrv, help_tab[HELP_THD].abrv
        , help_tab[HELP_MSC].abrv, help_tab[HELP_ALL].abrv);
  }
  fprintf(out, _("\nFor more details see ps(1).\n"));
  exit(rc);
}

/* Missing:
 *
 * -P e k
 *
 */
