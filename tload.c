/*
 * tload.c	- terminal version of xload
 *
 * Options:
 *	-s initial scaling exponent (default = 6)
 *	-d delay
 *
 * Copyright (c) 1992 Branko Lankester
 * /proc changes by David Engel (david@ods.com)
 * Made a little more efficient by Michael K. Johnson (johnsonm@sunsite.unc.edu)
 */
#include "proc/version.h"
#include "proc/sysinfo.h"
#include "c.h"
#include "nls.h"
#include "xalloc.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

static char *screen;

static int nrows = 25;
static int ncols = 80;
static int scr_size;
static int fd = 1;
static int dly = 5;
static jmp_buf jb;

static void alrm(int signo __attribute__ ((__unused__)))
{
	signal(SIGALRM, alrm);
	alarm(dly);
}

static void setsize(int i)
{
	struct winsize win;

	signal(SIGWINCH, setsize);
	if (ioctl(fd, TIOCGWINSZ, &win) != -1) {
		if (win.ws_col > 0)
			ncols = win.ws_col;
		if (win.ws_row > 0)
			nrows = win.ws_row;
	}
	scr_size = nrows * ncols;
	if (screen == NULL)
		screen = (char *)xmalloc(scr_size);
	else
		screen = (char *)xrealloc(screen, scr_size);

	memset(screen, ' ', scr_size - 1);
	*(screen + scr_size - 2) = '\0';
	if (i)
		longjmp(jb, 0);
}

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fputs(USAGE_HEADER, out);
	fprintf(out, " %s [options] [tty]\n", program_invocation_short_name);
	fputs(USAGE_OPTIONS, out);
	fputs(_(" -d, --delay <secs>  update delay in seconds\n"), out);
	fputs(_(" -s, --scale <num>   vertical scale\n"), out);
	fputs(USAGE_SEPARATOR, out);
	fputs(USAGE_HELP, out);
	fputs(USAGE_VERSION, out);
	fprintf(out, USAGE_MAN_TAIL("tload(1)"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int lines, row, col = 0;
	int i, opt;
	double av[3];
	static double max_scale, scale_fact;
	char *scale_arg = NULL;

	static const struct option longopts[] = {
		{"scale", required_argument, NULL, 's'},
		{"delay", required_argument, NULL, 'd'},
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, 0}
	};

	setlocale (LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((opt =
		getopt_long(argc, argv, "s:d:Vh", longopts, NULL)) != -1)
		switch (opt) {
		case 's':
			scale_arg = optarg;
			break;
		case 'd':
			dly = atoi(optarg);
			break;
		case 'V':
			printf(PROCPS_NG_VERSION);
			return EXIT_SUCCESS;
			break;
		case 'h':
			usage(stdout);
		default:
			usage(stderr);
		}

	if (argc > optind)
		if ((fd = open(argv[optind], 1)) == -1)
			err(EXIT_FAILURE, _("can not open tty"));

	setsize(0);

	if (scale_arg)
		max_scale = atof(scale_arg);
	else
		max_scale = nrows;

	scale_fact = max_scale;

	setjmp(jb);
	col = 0;
	alrm(0);

	while (1) {

		if (scale_fact < max_scale)
			scale_fact *= 2.0;	/* help it drift back up. */

		loadavg(&av[0], &av[1], &av[2]);

 repeat:
		lines = av[0] * scale_fact;
		row = nrows - 1;

		while (--lines >= 0) {
			*(screen + row * ncols + col) = '*';
			if (--row < 0) {
				scale_fact /= 2.0;
				goto repeat;
			}
		}
		while (row >= 0)
			*(screen + row-- * ncols + col) = ' ';

		for (i = 1;; ++i) {
			char *p;
			row = nrows - (i * scale_fact);
			if (row < 0)
				break;
			if (*(p = screen + row * ncols + col) == ' ')
				*p = '-';
			else
				*p = '=';
		}

		if (++col == ncols) {
			--col;
			memmove(screen, screen + 1, scr_size - 1);

			for (row = nrows - 2; row >= 0; --row)
				*(screen + row * ncols + col) = ' ';
		}
		i = sprintf(screen, " %.2f, %.2f, %.2f", av[0], av[1], av[2]);
		if (i > 0)
			screen[i] = ' ';

		write(fd, "\033[H", 3);
		write(fd, screen, scr_size - 1);
		pause();
	}
}
