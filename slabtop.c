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

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <locale.h>
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "c.h"
#include "fileutils.h"
#include "nls.h"
#include "strutils.h"
#include <proc/procps.h>

#define DEFAULT_SORT  PROCPS_SLABNODE_OBJS
#define CHAINS_ALLOC  150

static unsigned short Cols, Rows;
static struct termios Saved_tty;
static long Delay = 3;
static int Run_once = 0;

static struct procps_slabinfo *Slab_info;

enum slabnode_item Sort_item = DEFAULT_SORT;

enum slabnode_item Node_items[] = {
    PROCPS_SLABNODE_OBJS,     PROCPS_SLABNODE_AOBJS, PROCPS_SLABNODE_USE,
    PROCPS_SLABNODE_OBJ_SIZE, PROCPS_SLABNODE_SLABS, PROCPS_SLABNODE_OBJS_PER_SLAB,
    PROCPS_SLABNODE_SIZE,     PROCPS_SLABNODE_NAME,
    /* next 2 are sortable but are not displayable,
       thus they need not be represented in the Relative_enums */
    PROCPS_SLABNODE_PAGES_PER_SLAB, PROCPS_SLABNODE_ASLABS,
    PROCPS_SLABNODE_stack_end };

enum Relative_enums {
    my_OBJS,  my_AOBJS, my_USE,  my_OSIZE,
    my_SLABS, my_OPS,   my_SIZE, my_NAME };

#define MAX_ITEMS (int)(sizeof(Node_items) / sizeof(Node_items[0]))

#define PRINT_line(fmt, ...) if (Run_once) printf(fmt, __VA_ARGS__); else printw(fmt, __VA_ARGS__)


/*
 * term_resize - set the globals 'Cols' and 'Rows' to the current terminal size
 */
static void term_resize (int unusused __attribute__ ((__unused__)))
{
    struct winsize ws;

    if ((ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1) && ws.ws_row > 10) {
        Cols = ws.ws_col;
        Rows = ws.ws_row;
    } else {
        Cols = 80;
        Rows = 24;
    }
}

static void sigint_handler (int unused __attribute__ ((__unused__)))
{
    Delay = 0;
}

static void __attribute__((__noreturn__)) usage (FILE *out)
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
    fputs(_(" v: sort by (non display) number of active slabs\n"), out);
    fputs(_(" n: sort by name\n"), out);
    fputs(_(" o: sort by number of objects (the default)\n"), out);
    fputs(_(" p: sort by (non display) pages per slab\n"), out);
    fputs(_(" s: sort by object size\n"), out);
    fputs(_(" u: sort by cache utilization\n"), out);
    fprintf(out, USAGE_MAN_TAIL("slabtop(1)"));

    exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

/*
 * set_sort_func - return the slab_sort_func that matches the given key.
 * On unrecognizable key, DEFAULT_SORT is returned.
 */
static enum slabnode_item set_sort_item (
        const char key)
{
    switch (tolower(key)) {
    case 'n':
        return PROCPS_SLABNODE_NAME;
    case 'o':
        return PROCPS_SLABNODE_OBJS;
    case 'a':
        return PROCPS_SLABNODE_AOBJS;
    case 's':
        return PROCPS_SLABNODE_OBJ_SIZE;
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
        return DEFAULT_SORT;
    }
}

static void parse_opts (int argc, char **argv)
{
    static const struct option longopts[] = {
        { "delay",   required_argument, NULL, 'd' },
        { "sort",    required_argument, NULL, 's' },
        { "once",    no_argument,       NULL, 'o' },
        { "help",    no_argument,       NULL, 'h' },
        { "version", no_argument,       NULL, 'V' },
        {  NULL,     0,                 NULL,  0  }};
    int o;

    while ((o = getopt_long(argc, argv, "d:s:ohV", longopts, NULL)) != -1) {
        switch (o) {
        case 'd':
            errno = 0;
            Delay = strtol_or_err(optarg, _("illegal delay"));
            if (Delay < 1)
                xerrx(EXIT_FAILURE, _("delay must be positive integer"));
            break;
        case 's':
            Sort_item = set_sort_item(optarg[0]);
            break;
        case 'o':
            Run_once=1;
            Delay = 0;
            break;
        case 'V':
            printf(PROCPS_NG_VERSION);
            exit(EXIT_SUCCESS);
        case 'h':
            usage(stdout);
        default:
            usage(stderr);
        }
    }
}

static void print_summary (void)
{
    enum slabs_enums {
        stat_AOBJS,   stat_OBJS,   stat_ASLABS, stat_SLABS,
        stat_ACACHES, stat_CACHES, stat_ACTIVE, stat_TOTAL,
        stat_MIN,     stat_AVG,    stat_MAX
    };
    static struct slab_result stats[] = {
        { PROCPS_SLABS_AOBJS,       0 },
        { PROCPS_SLABS_OBJS,        0 },
        { PROCPS_SLABS_ASLABS,      0 },
        { PROCPS_SLABS_SLABS,       0 },
        { PROCPS_SLABS_ACACHES,     0 },
        { PROCPS_SLABS_CACHES,      0 },
        { PROCPS_SLABS_SIZE_ACTIVE, 0 },
        { PROCPS_SLABS_SIZE_TOTAL,  0 },
        { PROCPS_SLABS_SIZE_MIN,    0 },
        { PROCPS_SLABS_SIZE_AVG,    0 },
        { PROCPS_SLABS_SIZE_MAX,    0 },
        { PROCPS_SLABS_stack_end,   0 }
    };

    if (procps_slabs_getstack(Slab_info, stats) < 0) \
        xerrx(EXIT_FAILURE, _("Error getting slab summary results"));

    PRINT_line(" %-35s: %u / %u (%.1f%%)\n"
               , /* Translation Hint: Next five strings must not
                  * exceed a length of 35 characters.  */
                 /* xgettext:no-c-format */
                 _("Active / Total Objects (% used)")
               , stats[stat_AOBJS].result.u_int
               , stats[stat_OBJS ].result.u_int
               , 100.0 * stats[stat_AOBJS].result.u_int / stats[stat_OBJS].result.u_int);
    PRINT_line(" %-35s: %u / %u (%.1f%%)\n"
               , /* xgettext:no-c-format */
                 _("Active / Total Slabs (% used)")
               , stats[stat_ASLABS].result.u_int
               , stats[stat_SLABS  ].result.u_int
               , 100.0 * stats[stat_ASLABS].result.u_int / stats[stat_SLABS].result.u_int);
    PRINT_line(" %-35s: %u / %u (%.1f%%)\n"
               , /* xgettext:no-c-format */
                 _("Active / Total Caches (% used)")
               , stats[stat_ACACHES].result.u_int
               , stats[stat_CACHES ].result.u_int
               , 100.0 * stats[stat_ACACHES].result.u_int / stats[stat_CACHES].result.u_int);
    PRINT_line(" %-35s: %.2fK / %.2fK (%.1f%%)\n"
               , /* xgettext:no-c-format */
                 _("Active / Total Size (% used)")
               , stats[stat_ACTIVE].result.ul_int / 1024.0
               , stats[stat_TOTAL ].result.ul_int / 1024.0
               , 100.0 * stats[stat_ACTIVE].result.ul_int / stats[stat_TOTAL].result.ul_int);
    PRINT_line(" %-35s: %.2fK / %.2fK / %.2fK\n\n"
               , _("Minimum / Average / Maximum Object")
               , stats[stat_MIN].result.u_int / 1024.0
               , stats[stat_AVG].result.u_int / 1024.0
               , stats[stat_MAX].result.u_int / 1024.0);
}

static void print_headings (void)
{
    /* Translation Hint: Please keep alignment of the
     * following intact. */
    PRINT_line("%-78s\n", _("  OBJS ACTIVE  USE OBJ SIZE  SLABS OBJ/SLAB CACHE SIZE NAME"));
}

static void print_details (struct slabnode_stack *stack)
{
    PRINT_line("%6u %6u %3u%% %7.2fK %6u %8u %9luK %-23s\n"
        , stack->head[my_OBJS ].result.u_int
        , stack->head[my_AOBJS].result.u_int
        , stack->head[my_USE  ].result.u_int
        , stack->head[my_OSIZE].result.u_int / 1024.0
        , stack->head[my_SLABS].result.u_int
        , stack->head[my_OPS  ].result.u_int
        , stack->head[my_SIZE ].result.ul_int / 1024
        , stack->head[my_NAME ].result.str);

    return;
}


int main(int argc, char *argv[])
{
    int is_tty, nr_slabs, rc = EXIT_SUCCESS;
    unsigned short old_rows;
    struct slabnode_stack **v;

#ifdef HAVE_PROGRAM_INVOCATION_NAME
    program_invocation_name = program_invocation_short_name;
#endif
    setlocale (LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
    atexit(close_stdout);

    parse_opts(argc, argv);

    if (procps_slabinfo_new(&Slab_info) < 0)
        xerrx(EXIT_FAILURE, _("Unable to create slabinfo structure"));

    if (!(v = procps_slabnode_stacks_alloc(Slab_info, CHAINS_ALLOC, MAX_ITEMS, Node_items)))
        xerrx(EXIT_FAILURE, _("Unable to allocate slabinfo nodes"));

    if (!Run_once) {
        is_tty = isatty(STDIN_FILENO);
        if (is_tty && tcgetattr(STDIN_FILENO, &Saved_tty) == -1)
            xwarn(_("terminal setting retrieval"));
        old_rows = Rows;
        term_resize(0);
        initscr();
        resizeterm(Rows, Cols);
        signal(SIGWINCH, term_resize);
        signal(SIGINT, sigint_handler);
    }

    do {
        struct timeval tv;
        fd_set readfds;
        int i;

        // this next guy also performs the procps_slabnode_read() call
        if ((nr_slabs = procps_slabnode_stacks_fill(Slab_info, v, CHAINS_ALLOC)) < 0) {
            xwarn(_("Unable to get slabinfo node data"));
            rc = EXIT_FAILURE;
            break;
        }
        if (!(v = procps_slabnode_stacks_sort(Slab_info, v, nr_slabs, Sort_item))) {
            xwarn(_("Unable to sort slab nodes"));
            rc = EXIT_FAILURE;
            break;
        }

        if (Run_once) {
            print_summary();
            print_headings();
            for (i = 0; i < nr_slabs; i++)
                print_details(v[i]);
            break;
        }

        if (old_rows != Rows) {
            resizeterm(Rows, Cols);
            old_rows = Rows;
        }
        move(0, 0);
        print_summary();
        attron(A_REVERSE);
        print_headings();
        attroff(A_REVERSE);

        for (i = 0; i < Rows - 8 && i < nr_slabs; i++)
            print_details(v[i]);

        refresh();
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        tv.tv_sec = Delay;
        tv.tv_usec = 0;
        if (select(STDOUT_FILENO, &readfds, NULL, NULL, &tv) > 0) {
            char c;
            if (read(STDIN_FILENO, &c, 1) != 1
            || (c == 'Q' || c == 'q'))
                break;
            Sort_item = set_sort_item(c);
        }
    // made zero by sigint_handler()
    } while (Delay);

    if (!Run_once) {
        if (is_tty)
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &Saved_tty);
        endwin();
    }
    procps_slabinfo_unref(&Slab_info);
    return rc;
}
