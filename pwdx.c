/*
 * Copyright 2004 Nicholas Miell
 *
 * This file may be used subject to the terms and conditions of the
 * GNU Library General Public License Version 2 as published by the
 * Free Software Foundation.This program is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU Library General Public License for more
 * details.
 */

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "proc/version.h"
#include "c.h"
#include "nls.h"

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fputs(USAGE_HEADER, out);
	fprintf(out, " %s [options] pid...\n", program_invocation_short_name);
	fputs(USAGE_OPTIONS, out);
	fputs(USAGE_HELP, out);
	fputs(USAGE_VERSION, out);
	fprintf(out, USAGE_MAN_TAIL("pwdx(1)"));

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	char ch;
	int retval = 0, i;
	int alloclen = 128;
	char *pathbuf;

	static const struct option longopts[] = {
		{"version", no_argument, 0, 'V'},
		{"help", no_argument, 0, 'h'},
		{NULL, 0, 0, 0}
	};

	setlocale (LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((ch = getopt_long(argc, argv, "Vh", longopts, NULL)) != -1)
		switch (ch) {
		case 'V':
			printf(PROCPS_NG_VERSION);
			return EXIT_SUCCESS;
		case 'h':
			usage(stdout);
		default:
			usage(stderr);
		}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage(stderr);

	pathbuf = malloc(alloclen);

	for (i = 0; i < argc; i++) {
		char *s;
		ssize_t len;
		/* Constant 10 is the length of strings "/proc/" + "/cwd" + 1 */
		char buf[10 + strlen(argv[i]) + 1];

		/*
		 * At this point, all arguments are in the form
		 * /proc/NNNN or NNNN, so a simple check based on
		 * the first char is possible
		 */
		if (argv[i][0] != '/')
			snprintf(buf, sizeof buf, "/proc/%s/cwd", argv[i]);
		else
			snprintf(buf, sizeof buf, "%s/cwd", argv[i]);

		/*
		 * buf contains /proc/NNNN/cwd symlink name
		 * on entry, the target of that symlink on return
		 */
		while ((len = readlink(buf, pathbuf, alloclen)) == alloclen) {
			alloclen *= 2;
			pathbuf = realloc(pathbuf, alloclen);
		}

		if (len < 0) {
			s = strerror(errno == ENOENT ? ESRCH : errno);
			retval = 1;
		} else {
			pathbuf[len] = 0;
			s = pathbuf;
		}

		printf("%s: %s\n", argv[i], s);
	}

	return retval;
}
