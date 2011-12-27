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
	      _(" %s [options]\n"), program_invocation_short_name);
	if (section == USAGE_SELECTION || section == USAGE_ALL) {
	fputs(_("\nSimple options:\n"
		" -A               all processes\n"
		" -N, --deselect   negate selection\n"
		" -a               all without tty and session leader\n"
		" -d               all except session leader\n"
		" -e               all processes\n"
		" T                all processes on this terminal\n"
		" a                all without tty, including other users\n"
		" g                obsolete, do not use\n"
		" r                only running processes\n"
		" x                processes without controlling ttys\n"), out);
	}
	if (section == USAGE_LIST || section == USAGE_ALL) {
	fputs(_("\nSelection by list:\n"
		" -C <command>         command name\n"
		" U, -u, --user <uid>  effective user id or name\n"
		" -U, --User <uid>     real user id or name\n"
		" -G, --Group <gid>    real group id or name\n"
		" -g, --group <group>  session or effective group name\n"
		" -p, --pid <pid>      process id\n"
		" --ppid <pid>         select by parent process id\n"
		" -s, --sid <session>  session id\n"
		" t, -t, --tty <tty>   terminal\n"
		"\n selection <arguments> take csv list e.g. `-u root,nobody'\n"), out);
	}
	if (section == USAGE_OUTPUT || section == USAGE_ALL) {
	fputs(_("\nOutput formats:\n"
		" o, -o, --format <format>"
		"                  user defined format\n"
		" O  <format>      preloaded -o allowing sorting\n"
		" -O <format>      preloaded, with default columns, allowing sorting\n"
		" -j               jobs format\n"
		" j                BSD job control format\n"
		" -l               long format\n"
		" l                BSD long format\n"
		" y                do not show flags, show rrs in place addr (used with -l)\n"
		" -f               full-format\n"
		" -F               extra full\n"
		" s                signal format\n"
		" v                virtual memory\n"
		" u                user-oriented format\n"
		" X                register format\n"
		" Z, -M            security data (for SE Linux)\n"
		" f, --forest      ascii art process tree\n"
		" -H               show process hierarchy\n"
		" --context        display security context (for SE Linux)\n"
		" --heading        repeat header lines\n"
		" --no-headers     do not print header at all\n"
		" --cols <num>     set screen width\n"
		" --rows <num>     set screen height\n"), out);
	}
	if (section == USAGE_THREADS || section == USAGE_ALL) {
	fputs(_("\nShow threads:\n"
		" H                as if they where processes\n"
		" -L               possibly with LWP and NLWP columns\n"
		" -T               possibly with SPID column\n"
		" m, -m            after processes\n"), out);
	}
	if (section == USAGE_MISC || section == USAGE_ALL) {
	fputs(_("\nMisc options:\n"
		" w, -w            unlimited output width\n"
		" L                list format codes\n"
		" c                true command name\n"
		" n                display numeric uid and wchan\n"
		" -y               do not show flags, show rss (only with -l)\n"
		" -c               show scheduling class\n"
		" --sort <spec>    specify sort order, can be a csv list\n"
		" S, --cumulative  include some dead child process data\n"
		" --info           print debuggin information\n"
		" V,-V, --version  display version information and exit\n"
		" --help <selection|list|output|threads|misc|all>\n"
		"                  display help\n"), out);
	}
	if (section == USAGE_DEFAULT)

	/* Translation Hint: do not translate arguments, that breaks
	 * string comparison. Outputting something like following
	 * might work.
	 *
	 * Zry `%s --help <selection|list|output|threads|misc|all>
	 *                (zlekzio|lizt|czreen vrites|threadz|mizc|trezt)
	 */
	fprintf(out, _("\n Try `%s --help <selection|list|output|threads|misc|all>'\n"
		       " for more information.\n"), program_invocation_short_name);
	fprintf(out, USAGE_MAN_TAIL("ps(1)"));
	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

/* Missing:
 *
 * -P e k
 *
 */
