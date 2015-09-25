/*
 * kill.c - send a signal to process
 * Copyright 1998-2002 by Albert Cahalan
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

#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <ctype.h>

#include "c.h"
#include "signals.h"
#include "strutils.h"
#include "nls.h"

/* kill help */
static void __attribute__ ((__noreturn__)) print_usage(FILE * out)
{
    fputs(USAGE_HEADER, out);
    fprintf(out,
              _(" %s [options] <pid> [...]\n"), program_invocation_short_name);
    fputs(USAGE_OPTIONS, out);
    fputs(_(" <pid> [...]            send signal to every <pid> listed\n"), out);
    fputs(_(" -<signal>, -s, --signal <signal>\n"
        "                        specify the <signal> to be sent\n"), out);
    fputs(_(" -l, --list=[<signal>]  list all signal names, or convert one to a name\n"), out);
    fputs(_(" -L, --table            list all signal names in a nice table\n"), out);
    fputs(USAGE_SEPARATOR, out);
    fputs(USAGE_HELP, out);
    fputs(USAGE_VERSION, out);
    fprintf(out, USAGE_MAN_TAIL("kill(1)"));
    exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
    int signo, i, sigopt=0, loop=1;
    long pid;
    int exitvalue = EXIT_SUCCESS;

    static const struct option longopts[] = {
        {"list", optional_argument, NULL, 'l'},
        {"table", no_argument, NULL, 'L'},
        {"signal", required_argument, NULL, 's'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'V'},
        {NULL, 0, NULL, 0}
    };


    setlocale (LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);


    if (argc < 2)
        print_usage(stderr);

    signo = skill_sig_option(&argc, argv);
    if (signo < 0)
        signo = SIGTERM;
    else
        sigopt++;

    opterr=0; /* suppress errors on -123 */
    while (loop == 1 && (i = getopt_long(argc, argv, "l::Ls:hV", longopts, NULL)) != -1)
        switch (i) {
        case 'l':
            if (optarg) {
                char *s;
                s = strtosig(optarg);
                if (s)
                    printf("%s\n", s);
                else
                    xwarnx(_("unknown signal name %s"),
                          optarg);
                free(s);
            } else {
                unix_print_signals();
            }
            exit(EXIT_SUCCESS);
        case 'L':
            pretty_print_signals();
            exit(EXIT_SUCCESS);
        case 's':
            signo = signal_name_to_number(optarg);
            break;
        case 'h':
            print_usage(stdout);
        case 'V':
            fprintf(stdout, PROCPS_NG_VERSION);
            exit(EXIT_SUCCESS);
        case '?':
            if (!isdigit(optopt)) {
                xwarnx(_("invalid argument %c"), optopt);
                print_usage(stderr);
            } else {
                /* Special case for signal digit negative
                 * PIDs */
                pid = (long)('0' - optopt);
                if (kill((pid_t)pid, signo) != 0)
                exitvalue = EXIT_FAILURE;
                exit(exitvalue);
            }
            loop=0;
            break;
        default:
            print_usage(stderr);
        }

    argc -= optind + sigopt;
    argv += optind;

    for (i = 0; i < argc; i++) {
        pid = strtol_or_err(argv[i], _("failed to parse argument"));
        if (!kill((pid_t) pid, signo))
            continue;
        exitvalue = EXIT_FAILURE;
        continue;
    }

    return exitvalue;
}
