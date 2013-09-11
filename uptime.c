/*
 * uptime.c - display system uptime
 * Copyright (C) 2012 Craig Small <csmall-procps@enc.com.au>
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
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

#include "c.h"
#include "fileutils.h"
#include "nls.h"
#include "proc/sysinfo.h"
#include "proc/whattime.h"
#include "proc/version.h"

static void print_uptime_since()
{
	double now, uptime_secs, idle_secs;
	time_t up_since_secs;
	struct tm *up_since;
	struct timeval tim;

	/* Get the current time and convert it to a double */
	gettimeofday(&tim, NULL);
	now = tim.tv_sec + (tim.tv_usec / 1000000.0);

	/* Get the uptime and calculate when that was */
	uptime(&uptime_secs, &idle_secs);
	up_since_secs = (time_t) ((now - uptime_secs) + 0.5);

	/* Show this */
	up_since = localtime(&up_since_secs);
	printf("%04d-%02d-%02d %02d:%02d:%02d\n",
		up_since->tm_year + 1900, up_since->tm_mon + 1, up_since->tm_mday,
		up_since->tm_hour, up_since->tm_min, up_since->tm_sec);
}

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options]\n"), program_invocation_short_name);
	fputs(USAGE_OPTIONS, out);
	fputs(_(" -p, --pretty   show uptime in pretty format\n"), out);
	fputs(USAGE_HELP, out);
	fputs(_(" -s, --since    system up since\n"), out);
	fputs(USAGE_VERSION, out);
	fprintf(out, USAGE_MAN_TAIL("uptime(1)"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int c, p = 0;

	static const struct option longopts[] = {
		{"pretty", no_argument, NULL, 'p'},
		{"help", no_argument, NULL, 'h'},
		{"since", no_argument, NULL, 's'},
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, 0}
	};

#ifdef HAVE_PROGRAM_INVOCATION_NAME
	program_invocation_name = program_invocation_short_name;
#endif
	setlocale (LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	atexit(close_stdout);

	while ((c = getopt_long(argc, argv, "phsV", longopts, NULL)) != -1)
		switch (c) {
		case 'p':
			p = 1;
			break;
		case 'h':
			usage(stdout);
		case 's':
			print_uptime_since();
			return EXIT_SUCCESS;
		case 'V':
			printf(PROCPS_NG_VERSION);
			return EXIT_SUCCESS;
		default:
			usage(stderr);
		}

	print_uptime(p);
	return EXIT_SUCCESS;
}
