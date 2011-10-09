/*
 * Copyright 1998-2004 by Albert Cahalan; all rights reserved.
 * This file may be used subject to the terms and conditions of the
 * GNU Library General Public License Version 2, or any later version
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Library General Public License for more details.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

void __attribute__ ((__noreturn__)) usage(FILE * out, int section)
{
	fputs(USAGE_HEADER, out);
	fprintf(out,
		" %s [options]\n", program_invocation_short_name);
	if (section == USAGE_SELECTION || section == USAGE_ALL) {
	fputs(_("\nSimple options:\n"), out);
	fputs(_(" -A               all processes\n"), out);
	fputs(_(" -N, --deselect   negate selection\n"), out);
	fputs(_(" -a               all without tty and session leader\n"), out);
	fputs(_(" -d               all except session leader\n"), out);
	fputs(_(" -e               all processes\n"), out);
	fputs(_(" T                all processes on this terminal\n"), out);
	fputs(_(" a                all without tty, including other users\n"), out);
	fputs(_(" g                obsolete, do not use\n"), out);
	fputs(_(" r                only running processes\n"), out);
	fputs(_(" x                processes without controlling ttys\n"), out);
	}
	if (section == USAGE_LIST || section == USAGE_ALL) {
	fputs(_("\nSelection by list:\n"), out);
	fputs(_(" -C <command>         command name\n"), out);
	fputs(_(" U, -u, --user <uid>  effective user id or name\n"), out);
	fputs(_(" -U, --User <uid>     real user id or name\n"), out);
	fputs(_(" -G, --Group <gid>    real group id or name\n"), out);
	fputs(_(" -g, --group <group>  session or effective group name\n"), out);
	fputs(_(" -p, --pid <pid>      process id\n"), out);
	fputs(_(" --ppid <pid>         select by parent process id\n"), out);
	fputs(_(" -s, --sid <session>  session id\n"), out);
	fputs(_(" t, -t, --tty <tty>   terminal\n"), out);
	fputs(_("\n selection <arguments> take csv list e.g. `-u root,nobody'\n"), out);
	}
	if (section == USAGE_OUTPUT || section == USAGE_ALL) {
	fputs(_("\nOutput formats:\n"), out);
	fputs(_(" o, -o, --format <format>"), out);
	fputs(_("                  user defined format\n"), out);
	fputs(_(" O  <format>      preloaded -o allowing sorting\n"), out);
	fputs(_(" -O <format>      preloaded, with default columns, allowing sorting\n"), out);
	fputs(_(" -j               jobs format\n"), out);
	fputs(_(" j                BSD job control format\n"), out);
	fputs(_(" -l               long format\n"), out);
	fputs(_(" l                BSD long format\n"), out);
	fputs(_(" y                do not show flags, show rrs in place addr (used with -l)\n"), out);
	fputs(_(" -f               full-format\n"), out);
	fputs(_(" -F               extra full\n"), out);
	fputs(_(" s                signal format\n"), out);
	fputs(_(" v                virtual memory\n"), out);
	fputs(_(" u                user-oriented format\n"), out);
	fputs(_(" X                register format\n"), out);
	fputs(_(" Z, -M            security data (for SE Linux)\n"), out);
	fputs(_(" f, --forest      ascii art process tree\n"), out);
	fputs(_(" -H               show process hierarchy\n"), out);
	fputs(_(" --context        display security context (for SE Linux)\n"), out);
	fputs(_(" --heading        repeat header lines\n"), out);
	fputs(_(" --no-headers     do not print header at all\n"), out);
	fputs(_(" --cols <num>     set screen width\n"), out);
	fputs(_(" --rows <num>     set screen height\n"), out);
	}
	if (section == USAGE_THREADS || section == USAGE_ALL) {
	fputs(_("\nShow threads:\n"), out);
	fputs(_(" H                as if they where processes\n"), out);
	fputs(_(" -L               possibly with LWP and NLWP columns\n"), out);
	fputs(_(" -T               possibly with SPID column\n"), out);
	fputs(_(" m, -m            after processes\n"), out);
	}
	if (section == USAGE_MISC || section == USAGE_ALL) {
	fputs(_("\nMisc options:\n"), out);
	fputs(_(" w, -w            unlimited output width\n"), out);
	fputs(_(" L                list format codes\n"), out);
	fputs(_(" c                true command name\n"), out);
	fputs(_(" n                display numeric uid and wchan\n"), out);
	fputs(_(" -y               do not show flags, show rss (only with -l)\n"), out);
	fputs(_(" -c               show scheduling class\n"), out);
	fputs(_(" --sort <spec>    specify sort order, can be a csv list\n"), out);
	fputs(_(" S, --cumulative  include some dead child process data\n"), out);
	fputs(_(" --info           print debuggin information\n"), out);
	fputs(_(" V,-V, --version  display version information and exit\n"), out);
	fputs(_(" --help <selection|list|output|threads|misc|all>\n"), out);
	fputs(_("                  display help\n"), out);
	}
	if (section == USAGE_DEFAULT)
	fprintf(out,
	      _("\n Try `%s --help <selection|list|output|threads|misc|all>'\n"
		" for more information.\n"), program_invocation_short_name);
	fprintf(out, USAGE_MAN_TAIL("ps(1)"));
	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

/* Missing:
 *
 * -P e k
 *
 */
