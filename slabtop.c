/*
 * slabtop.c - utility to display kernel slab information.
 *
 * Chris Rivera <cmrivera@ufl.edu>
 * Robert Love <rml@tech9.net>
 *
 * Copyright (C) 2003 Chris Rivera
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

#include <locale.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <ncurses.h>
#include <termios.h>
#include <getopt.h>
#include <ctype.h>
#include <sys/ioctl.h>

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "c.h"
#include "fileutils.h"
#include "nls.h"
#include "strutils.h"
#include "proc/slab.h"
#include "proc/version.h"

#define DEF_SORT_FUNC		sort_nr_objs

static unsigned short cols, rows;
static struct termios saved_tty;
static long delay = 3;
static int (*sort_func)(const struct slab_info *, const struct slab_info *);

static struct slab_info *merge_objs(struct slab_info *a, struct slab_info *b)
{
	struct slab_info sorted_list;
	struct slab_info *curr = &sorted_list;

	while ((a != NULL) && (b != NULL)) {
		if (sort_func(a, b)) {
			curr->next = a;
			curr = a;
			a = a->next;
		} else {
			curr->next = b;
			curr = b;
			b = b->next;
		}
	}

	curr->next = (a == NULL) ? b : a;
	return sorted_list.next;
}

/*
 * slabsort - merge sort the slab_info linked list based on sort_func
 */
static struct slab_info *slabsort(struct slab_info *list)
{
	struct slab_info *a, *b;

	if ((list == NULL) || (list->next == NULL))
		return list;

	a = list;
	b = list->next;

	while ((b != NULL) && (b->next != NULL)) {
		list = list->next;
		b = b->next->next;
	}

	b = list->next;
	list->next = NULL;

	return merge_objs(slabsort(a), slabsort(b));
}

/*
 * Sort Routines.  Each of these should be associated with a command-line
 * search option.  The functions should fit the prototype:
 *
 *	int sort_foo(const struct slab_info *a, const struct slab_info *b)
 *
 * They return one if the first parameter is larger than the second
 * Otherwise, they return zero.
 */

static int sort_name(const struct slab_info *a, const struct slab_info *b)
{
	return (strcmp(a->name, b->name) < 0) ? 1 : 0;
}

static int sort_nr_objs(const struct slab_info *a, const struct slab_info *b)
{
	return (a->nr_objs > b->nr_objs);
}

static int sort_nr_active_objs(const struct slab_info *a,
				const struct slab_info *b)
{
	return (a->nr_active_objs > b->nr_active_objs);
}

static int sort_obj_size(const struct slab_info *a, const struct slab_info *b)
{
	return (a->obj_size > b->obj_size);
}

static int sort_objs_per_slab(const struct slab_info *a,
				const struct slab_info *b)
{
	return (a->objs_per_slab > b->objs_per_slab);
}

static int sort_pages_per_slab(const struct slab_info *a,
		const struct slab_info *b)
{
	return (a->pages_per_slab > b->pages_per_slab);
}

static int sort_nr_slabs(const struct slab_info *a, const struct slab_info *b)
{
	return (a->nr_slabs > b->nr_slabs);
}

static int sort_nr_active_slabs(const struct slab_info *a,
			const struct slab_info *b)
{
	return (a->nr_active_slabs > b->nr_active_slabs);
}


static int sort_use(const struct slab_info *a, const struct slab_info *b)
{
	return (a->use > b->use);
}

static int sort_cache_size(const struct slab_info *a, const struct slab_info *b)
{
	return (a->cache_size > b->cache_size);
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
}

static void sigint_handler(int unused __attribute__ ((__unused__)))
{
	delay = 0;
}

static void __attribute__((__noreturn__)) usage(FILE *out)
{
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options]\n"), program_invocation_short_name);
	fputs(USAGE_OPTIONS, out);
	fputs(_(" -d, --delay <secs>  delay updates\n"), out);
	fputs(_(" -o, --once          only display once, then exit\n"), out);
	fputs(_(" -s, --sort <char>   specify sort criteria by character (see below)\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fputs(USAGE_HELP, out);
	fputs(USAGE_VERSION, out);

	fputs(_("\nThe following are valid sort criteria:\n"), out);
	fputs(_(" a: sort by number of active objects\n"), out);
	fputs(_(" b: sort by objects per slab\n"), out);
	fputs(_(" c: sort by cache size\n"), out);
	fputs(_(" l: sort by number of slabs\n"), out);
	fputs(_(" v: sort by number of active slabs\n"), out);
	fputs(_(" n: sort by name\n"), out);
	fputs(_(" o: sort by number of objects (the default)\n"), out);
	fputs(_(" p: sort by pages per slab\n"), out);
	fputs(_(" s: sort by object size\n"), out);
	fputs(_(" u: sort by cache utilization\n"), out);
	fprintf(out, USAGE_MAN_TAIL("slabtop(1)"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

/*
 * set_sort_func - return the slab_sort_func that matches the given key.
 * On unrecognizable key, DEF_SORT_FUNC is returned.
 */
static void * set_sort_func(char key)
{
	switch (key) {
	case 'n':
		return (void *) sort_name;
	case 'o':
		return (void *) sort_nr_objs;
	case 'a':
		return (void *) sort_nr_active_objs;
	case 's':
		return (void *) sort_obj_size;
	case 'b':
		return (void *) sort_objs_per_slab;
	case 'p':
		return (void *) sort_pages_per_slab;
	case 'l':
		return (void *) sort_nr_slabs;
	case 'v':
		return (void *) sort_nr_active_slabs;
	case 'c':
		return (void *) sort_cache_size;
	case 'u':
		return (void *) sort_use;
	default:
		return (void *) DEF_SORT_FUNC;
	}
}

static void parse_input(char c)
{
	c = toupper(c);
	switch(c) {
	case 'A':
		sort_func = sort_nr_active_objs;
		break;
	case 'B':
		sort_func = sort_objs_per_slab;
		break;
	case 'C':
		sort_func = sort_cache_size;
		break;
	case 'L':
		sort_func = sort_nr_slabs;
		break;
	case 'V':
		sort_func = sort_nr_active_slabs;
		break;
	case 'N':
		sort_func = sort_name;
		break;
	case 'O':
		sort_func = sort_nr_objs;
		break;
	case 'P':
		sort_func = sort_pages_per_slab;
		break;
	case 'S':
		sort_func = sort_obj_size;
		break;
	case 'U':
		sort_func = sort_use;
		break;
	case 'Q':
		delay = 0;
		break;
	}
}

#define print_line(fmt, ...) if (run_once) printf(fmt, __VA_ARGS__); else printw(fmt, __VA_ARGS__)
int main(int argc, char *argv[])
{
	int is_tty, o;
	unsigned short old_rows;
	struct slab_info *slab_list = NULL;
	int run_once = 0, retval = EXIT_SUCCESS;

	static const struct option longopts[] = {
		{ "delay",	required_argument, NULL, 'd' },
		{ "sort",	required_argument, NULL, 's' },
		{ "once",	no_argument,	   NULL, 'o' },
		{ "help",	no_argument,	   NULL, 'h' },
		{ "version",	no_argument,	   NULL, 'V' },
		{  NULL, 0, NULL, 0 }
	};

#ifdef HAVE_PROGRAM_INVOCATION_NAME
	program_invocation_name = program_invocation_short_name;
#endif
	setlocale (LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	atexit(close_stdout);

	sort_func = DEF_SORT_FUNC;

	while ((o = getopt_long(argc, argv, "d:s:ohV", longopts, NULL)) != -1) {
		switch (o) {
		case 'd':
			errno = 0;
			delay = strtol_or_err(optarg, _("illegal delay"));
			if (delay < 1)
				xerrx(EXIT_FAILURE,
					_("delay must be positive integer"));
			break;
		case 's':
			sort_func = (int (*)(const struct slab_info*,
				const struct slab_info *)) set_sort_func(optarg[0]);
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

	is_tty = isatty(STDIN_FILENO);
	if (is_tty && tcgetattr(STDIN_FILENO, &saved_tty) == -1)
		xwarn(_("terminal setting retrieval"));

	old_rows = rows;
	term_size(0);
	if (!run_once) {
		initscr();
		resizeterm(rows, cols);
		signal(SIGWINCH, term_size);
	}
	signal(SIGINT, sigint_handler);

	do {
		struct slab_info *curr;
		struct slab_stat stats;
		struct timeval tv;
		fd_set readfds;
		char c;
		int i;
		memset(&stats, 0, sizeof(struct slab_stat));

		if (get_slabinfo(&slab_list, &stats)) {
			retval = EXIT_FAILURE;
			break;
		}

		if (!run_once && old_rows != rows) {
			resizeterm(rows, cols);
			old_rows = rows;
		}

		move(0, 0);
		print_line(" %-35s: %d / %d (%.1f%%)\n"
		       " %-35s: %d / %d (%.1f%%)\n"
		       " %-35s: %d / %d (%.1f%%)\n"
		       " %-35s: %.2fK / %.2fK (%.1f%%)\n"
		       " %-35s: %.2fK / %.2fK / %.2fK\n\n",
		       /* Translation Hint: Next five strings must not
			* exceed 35 length in characters.  */
		       /* xgettext:no-c-format */
		       _("Active / Total Objects (% used)"),
		       stats.nr_active_objs, stats.nr_objs,
		       100.0 * stats.nr_active_objs / stats.nr_objs,
	               /* xgettext:no-c-format */
		       _("Active / Total Slabs (% used)"),
		       stats.nr_active_slabs, stats.nr_slabs,
		       100.0 * stats.nr_active_slabs / stats.nr_slabs,
	               /* xgettext:no-c-format */
		       _("Active / Total Caches (% used)"),
		       stats.nr_active_caches, stats.nr_caches,
		       100.0 * stats.nr_active_caches / stats.nr_caches,
	               /* xgettext:no-c-format */
		       _("Active / Total Size (% used)"),
		       stats.active_size / 1024.0, stats.total_size / 1024.0,
		       100.0 * stats.active_size / stats.total_size,
		       _("Minimum / Average / Maximum Object"),
		       stats.min_obj_size / 1024.0, stats.avg_obj_size / 1024.0,
		       stats.max_obj_size / 1024.0);

		slab_list = slabsort(slab_list);

		attron(A_REVERSE);
		/* Translation Hint: Please keep alignment of the
		 * following intact. */
		print_line("%-78s\n", _("  OBJS ACTIVE  USE OBJ SIZE  SLABS OBJ/SLAB CACHE SIZE NAME"));
		attroff(A_REVERSE);

		curr = slab_list;
		for (i = 0; i < rows - 8 && curr->next; i++) {
			print_line("%6u %6u %3u%% %7.2fK %6u %8u %9uK %-23s\n",
				curr->nr_objs, curr->nr_active_objs, curr->use,
				curr->obj_size / 1024.0, curr->nr_slabs,
				curr->objs_per_slab, (unsigned)(curr->cache_size / 1024),
				curr->name);
			curr = curr->next;
		}

		put_slabinfo(slab_list);
		if (!run_once) {
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
		}
	} while (delay);

	if (is_tty)
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_tty);
	free_slabinfo(slab_list);
	if (!run_once)
		endwin();
	return retval;
}
