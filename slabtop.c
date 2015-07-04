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
#include <proc/slab.h>

#define DEFAULT_SORT_ITEM PROCPS_SLABNODE_OBJS

static unsigned short cols, rows;
static struct termios saved_tty;
static long delay = 3;
static int run_once = 0;

#define print_line(fmt, ...) if (run_once) printf(fmt, __VA_ARGS__); else printw(fmt, __VA_ARGS__)
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
static enum procps_slabinfo_nodeitem get_sort_item(
        const char key,
        enum procps_slabinfo_nodeitem old_sort)
{
    switch (tolower(key)) {
    case 'n':
        return PROCPS_SLABNODE_NAME;
    case 'o':
        return PROCPS_SLABNODE_OBJS;
    case 'a':
        return PROCPS_SLABNODE_AOBJS;
    case 's':
        return PROCPS_SLABNODE_SIZE;
    case 'b':
        return PROCPS_SLABNODE_OBJS_PER_SLAB;
    case 'p':
        return PROCPS_SLABNODE_PAGES_PER_SLAB;
    case 'l':
        return PROCPS_SLABNODE_SLABS;
    case 'v':
        return PROCPS_SLABNODE_ASLABS;
    case 'c':
        return PROCPS_SLABNODE_SIZE;
    case 'u':
        return PROCPS_SLABNODE_USE;
    default:
        return old_sort;
    }
}

#if 0
    case 'Q':
        delay = 0;
        break;
    }
}
#endif

static void print_stats(struct procps_slabinfo *info)
{
#define STAT_VAL(e) stats[e].result
    enum stat_enums {
        stat_AOBJS,   stat_OBJS,   stat_ASLABS, stat_SLABS,
        stat_ACACHES, stat_CACHES, stat_ACTIVE, stat_TOTAL,
        stat_MIN,     stat_AVG,    stat_MAX,
    };
    static struct procps_slabinfo_result stats[] = {
        { PROCPS_SLABINFO_AOBJS,        0, &stats[1] },
        { PROCPS_SLABINFO_OBJS,         0, &stats[2] },
        { PROCPS_SLABINFO_ASLABS,       0, &stats[3] },
        { PROCPS_SLABINFO_SLABS,        0, &stats[4] },
        { PROCPS_SLABINFO_ACACHES,      0, &stats[5] },
        { PROCPS_SLABINFO_CACHES,       0, &stats[6] },
        { PROCPS_SLABINFO_SIZE_ACTIVE,  0, &stats[7] },
        { PROCPS_SLABINFO_SIZE_TOTAL,   0, &stats[8] },
        { PROCPS_SLABINFO_SIZE_MIN,     0, &stats[9] },
        { PROCPS_SLABINFO_SIZE_AVG,     0, &stats[10] },
        { PROCPS_SLABINFO_SIZE_MAX,     0, NULL },
    };

    if (procps_slabinfo_stat_getchain(info, stats) < 0)
        xerrx(EXIT_FAILURE,
              _("Error getting slabinfo results"));

    print_line(" %-35s: %d / %d (%.1f%%)\n"
               " %-35s: %d / %d (%.1f%%)\n"
               " %-35s: %d / %d (%.1f%%)\n"
               " %-35s: %.2fK / %.2fK (%.1f%%)\n"
               " %-35s: %.2fK / %.2fK / %.2fK\n\n",
               /* Translation Hint: Next five strings must not
                * exceed 35 length in characters.  */
               /* xgettext:no-c-format */
               _("Active / Total Objects (% used)"),
               STAT_VAL(stat_AOBJS), STAT_VAL(stat_OBJS),
               100.0 * STAT_VAL(stat_AOBJS) / STAT_VAL(stat_OBJS),
                   /* xgettext:no-c-format */
               _("Active / Total Slabs (% used)"),
               STAT_VAL(stat_ASLABS), STAT_VAL(stat_SLABS),
               100.0 * STAT_VAL(stat_ASLABS) / STAT_VAL(stat_SLABS),
                   /* xgettext:no-c-format */
               _("Active / Total Caches (% used)"),
               STAT_VAL(stat_ACACHES), STAT_VAL(stat_CACHES),
               100.0 * STAT_VAL(stat_ACACHES) / STAT_VAL(stat_CACHES),
                   /* xgettext:no-c-format */
               _("Active / Total Size (% used)"),
               STAT_VAL(stat_ACTIVE) / 1024.0 , STAT_VAL(stat_TOTAL) / 1024.0,
               100.0 * STAT_VAL(stat_ACTIVE) / STAT_VAL(stat_TOTAL),
               _("Minimum / Average / Maximum Object"),
               STAT_VAL(stat_MIN) / 1024.0, STAT_VAL(stat_AVG) / 1024.0,
               STAT_VAL(stat_MAX) / 1024.0);
#undef STAT_VAL
}

static void cleanup(const int is_tty, struct procps_slabinfo **slab_info)
{
    if (is_tty)
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_tty);
    if (!run_once)
        endwin();
    procps_slabinfo_unref(slab_info);
}

int main(int argc, char *argv[])
{
    int is_tty, o;
    int nr_slabs;
    unsigned short old_rows;
    int retval = EXIT_SUCCESS;
    struct procps_slabinfo *slab_info;
    enum procps_slabinfo_nodeitem sort_item = DEFAULT_SORT_ITEM;

    static const struct option longopts[] = {
        { "delay",    required_argument, NULL, 'd' },
        { "sort",    required_argument, NULL, 's' },
        { "once",    no_argument,       NULL, 'o' },
        { "help",    no_argument,       NULL, 'h' },
        { "version",    no_argument,       NULL, 'V' },
        {  NULL, 0, NULL, 0 }
    };

    struct procps_slabnode_result result[] = {
        { PROCPS_SLABNODE_OBJS,          0, &result[1] },
        { PROCPS_SLABNODE_AOBJS,         0, &result[2] },
        { PROCPS_SLABNODE_USE,           0, &result[3] },
        { PROCPS_SLABNODE_OBJ_SIZE,      0, &result[4] },
        { PROCPS_SLABNODE_SLABS,         0, &result[5] },
        { PROCPS_SLABNODE_OBJS_PER_SLAB, 0, &result[6] },
        { PROCPS_SLABNODE_SIZE,          0, NULL } };
    enum result_enums {
        stat_OBJS, stat_AOBJS, stat_USE, stat_OSIZE, stat_SLABS,
        stat_OPS, stat_SIZE };

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
        case 's':
            sort_item = get_sort_item(optarg[0], sort_item);
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

    if (procps_slabinfo_new(&slab_info) < 0)
        xerrx(EXIT_FAILURE,
              _("Unable to create slabinfo structure"));


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

#define STAT_VAL(e) result[e].result
    do {
        char *slab_name;
        struct timeval tv;
        fd_set readfds;
        char c;
        int i, myerrno;

        if (procps_slabinfo_read(slab_info) < 0) {
            xwarn(_("Unable to read slabinfo"));
            retval = EXIT_FAILURE;
            break;
        }

        if (!run_once && old_rows != rows) {
            resizeterm(rows, cols);
            old_rows = rows;
        }

        move(0, 0);
        print_stats(slab_info);

        if (procps_slabinfo_sort(slab_info, sort_item) < 0) {
            xwarn(_("Unable to sort slabnodes"));
            retval= EXIT_FAILURE;
            break;
        }

        attron(A_REVERSE);
        /* Translation Hint: Please keep alignment of the
         * following intact. */
        print_line("%-78s\n", _("  OBJS ACTIVE  USE OBJ SIZE  SLABS OBJ/SLAB CACHE SIZE NAME"));
        attroff(A_REVERSE);

        if ((nr_slabs = procps_slabinfo_node_count(slab_info)) < 0) {
            xwarn(_("Unable to count slabinfo nodes"));
            retval = EXIT_FAILURE;
            break;
        }

        for (i=0 ; i < rows - 8 && i < nr_slabs; i++) {
            if (procps_slabinfo_node_getchain(slab_info, result, i) < 0) {
                xwarn(_("Unable to get slabinfo node data"));
                retval = EXIT_FAILURE;
                break;
            }
            slab_name= procps_slabinfo_node_getname(slab_info, i);
            print_line("%6u %6u %3u%% %7.2fK %6u %8u %9uK %-23s\n",
                       STAT_VAL(stat_OBJS), STAT_VAL(stat_AOBJS),
                       STAT_VAL(stat_USE),
                       STAT_VAL(stat_OSIZE) / 1024.0, STAT_VAL(stat_SLABS),
                       STAT_VAL(stat_OPS),
                       (unsigned)(STAT_VAL(stat_SIZE) / 1024),
                       slab_name?slab_name:"(unknown)");
        }
        if (!run_once) {
            refresh();
            FD_ZERO(&readfds);
            FD_SET(STDIN_FILENO, &readfds);
            tv.tv_sec = delay;
            tv.tv_usec = 0;
            if (select(STDOUT_FILENO, &readfds, NULL, NULL, &tv) > 0) {
                if (read(STDIN_FILENO, &c, 1) != 1)
                    break;
                if (c == 'Q' || c == 'q')
                    delay = 0;
                else
                    sort_item = get_sort_item(c, sort_item);
            }
        }
    } while (delay);
    cleanup(is_tty, &slab_info);

}
