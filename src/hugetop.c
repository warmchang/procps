/*
 * hugetop.c - utility to display system/process huge page information.
 *
 * zhenwei pi <pizhenwei@bytedance.com>
 *
 * Copyright © 2024      zhenwei pi
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

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <locale.h>
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <unistd.h>

#include "c.h"
#include "fileutils.h"
#include "meminfo.h"
#include "nls.h"
#include "pids.h"
#include "strutils.h"

static int run_once;
static unsigned short cols, rows;
static struct termios saved_tty;
static long delay = 3;
static unsigned long kb_hugepagesize;

enum pids_item Items[] = {
	PIDS_ID_PID,
	PIDS_CMD,
	PIDS_SMAP_HUGE_TLBPRV,
	PIDS_SMAP_HUGE_TLBSHR
};
#define ITEMS_COUNT (sizeof Items / sizeof *Items)

enum rel_items {
	EU_PID, EU_CMD, EU_SMAP_HUGE_TLBPRV, EU_SMAP_HUGE_TLBSHR
};

#define PIDS_GETINT(e) PIDS_VAL(EU_ ## e, s_int, stack, info)
#define PIDS_GETUNT(e) PIDS_VAL(EU_ ## e, u_int, stack, info)
#define PIDS_GETULL(e) PIDS_VAL(EU_ ## e, ull_int, stack, info)
#define PIDS_GETSTR(e) PIDS_VAL(EU_ ## e, str, stack, info)
#define PIDS_GETSCH(e) PIDS_VAL(EU_ ## e, s_ch, stack, info)
#define PIDS_GETSTV(e) PIDS_VAL(EU_ ## e, strv, stack, info)
#define PIDS_GETFLT(e) PIDS_VAL(EU_ ## e, real, stack, info)

static void setup_hugepage()
{
	struct meminfo_info *mem_info = NULL;
	int ret;

	ret = procps_meminfo_new(&mem_info);
	if (ret) {
		fputs("Huge page not found or not supported", stdout);
		exit(-ret);
	}

	if (!MEMINFO_GET(mem_info, MEMINFO_MEM_HUGE_SIZE, ul_int)) {
		fputs("Huge page not found or not supported", stdout);
		exit(ENOTSUP);
	}

	kb_hugepagesize = MEMINFO_GET(mem_info, MEMINFO_MEM_HUGE_SIZE, ul_int);
	procps_meminfo_unref(&mem_info);
}

/*
 * term_size - set the globals 'cols' and 'rows' to the current terminal size
 */
static void term_size(int unusused __attribute__ ((__unused__)))
{
	struct winsize ws;

	if ((ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1) && ws.ws_row > 10) {
		cols = ws.ws_col;
		rows = ws.ws_row;
	} else {
		cols = 80;
		rows = 24;
	}
	if (run_once)
		rows = USHRT_MAX;
}

static void sigint_handler(int unused __attribute__ ((__unused__)))
{
	delay = 0;
}

static void parse_input(char c)
{
	c = toupper(c);
	if (c == 'Q') {
		delay = 0;
	}
}

#define PRINT_line(fmt, ...) if (run_once) printf(fmt, __VA_ARGS__); else printw(fmt, __VA_ARGS__)

static void print_summary(void)
{
	struct meminfo_info *mem_info = NULL;
	time_t now;

	now = time(NULL);
	PRINT_line("%s - %s", program_invocation_short_name, ctime(&now));

	procps_meminfo_new(&mem_info);
	PRINT_line("Size %ldM, Total %ld, Free %ld\n",
			MEMINFO_GET(mem_info, MEMINFO_MEM_HUGE_SIZE, ul_int) / 1024,
			MEMINFO_GET(mem_info, MEMINFO_MEM_HUGE_TOTAL, ul_int),
			MEMINFO_GET(mem_info, MEMINFO_MEM_HUGE_FREE, ul_int));
	procps_meminfo_unref(&mem_info);
}

static void print_headings(void)
{
	PRINT_line("%-78s\n", _("     PID     SHARED    PRIVATE COMMAND"));
}

static void print_procs(void)
{
	struct pids_info *info = NULL;
	struct pids_stack *stack;
	unsigned long long shared_hugepages;
	unsigned long long private_hugepages;

	procps_pids_new(&info, Items, ITEMS_COUNT);
	while ((stack = procps_pids_get(info, PIDS_FETCH_TASKS_ONLY))) {
		shared_hugepages = PIDS_GETULL(SMAP_HUGE_TLBSHR) / kb_hugepagesize;
		private_hugepages = PIDS_GETULL(SMAP_HUGE_TLBPRV) / kb_hugepagesize;

		/* no huge pages in use, skip it */
		if (shared_hugepages + private_hugepages == 0)
			continue;

		PRINT_line("%*d %*llu %*llu %s\n",
			    8, PIDS_GETINT(PID),
			    10, shared_hugepages,
			    10, private_hugepages,
			    PIDS_GETSTR(CMD));
	}
	procps_pids_unref(&info);
}

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options]\n"), program_invocation_short_name);
	fputs(USAGE_OPTIONS, out);
	fputs(_(" -d, --delay <secs>  delay updates\n"), out);
	fputs(_(" -o, --once          only display once, then exit\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fputs(USAGE_HELP, out);
	fputs(USAGE_VERSION, out);

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int is_tty, o;
	unsigned short old_rows;
	int flags = 0;

	static const struct option longopts[] = {
		{ "delay",      required_argument, NULL, 'd' },
		{ "once",       no_argument,       NULL, 'o' },
		{ "help",       no_argument,       NULL, 'h' },
		{ "version",    no_argument,       NULL, 'V' },
		{  NULL,        0,                 NULL, 0   }
	};

#ifdef HAVE_PROGRAM_INVOCATION_NAME
	program_invocation_name = program_invocation_short_name;
#endif
	setlocale (LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	atexit(close_stdout);

	while ((o = getopt_long(argc, argv, "d:s:ohV", longopts, NULL)) != -1) {
		switch (o) {
			case 'd':
				errno = 0;
				delay = strtol_or_err(optarg, _("illegal delay"));
				if (delay < 1)
					xerrx(EXIT_FAILURE,
							_("delay must be positive integer"));
				break;
			case 'o':
				run_once=1;
				delay = 0;
				break;
			case 'V':
				printf(PROCPS_NG_VERSION);
				return EXIT_SUCCESS;
			case 'h':
				usage(stdout);
			default:
				usage(stderr);

		}
	}

	setup_hugepage();

	if (!run_once) {
		is_tty = isatty(STDIN_FILENO);
		if (is_tty && tcgetattr(STDIN_FILENO, &saved_tty) == -1)
			xwarn(_("terminal setting retrieval"));

		old_rows = rows;
		term_size(0);
		initscr();
		resizeterm(rows, cols);
		signal(SIGWINCH, term_size);
		signal(SIGINT, sigint_handler);
	}

	do {
		struct timeval tv;
		fd_set readfds;
		int i;
		char c;

		if (run_once) {
			print_summary();
			print_headings();
			print_procs();

			break;
		}

		if (old_rows != rows) {
			resizeterm(rows, cols);
			old_rows = rows;
		}

		move(0, 0);
		print_summary();
		attron(A_REVERSE);
		print_headings();
		attroff(A_REVERSE);
		print_procs();

		refresh();
		FD_ZERO(&readfds);
		FD_SET(STDIN_FILENO, &readfds);
		tv.tv_sec = delay;
		tv.tv_usec = 0;
		if (select(STDOUT_FILENO, &readfds, NULL, NULL, &tv) > 0) {
			if (read(STDIN_FILENO, &c, 1) != 1)
				break;
			parse_input(c);
		}

		erase();
	} while (delay);

	if (!run_once) {
		if (is_tty)
			tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_tty);
		endwin();
	}

	return 0;
}
