#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>

#include "c.h"
#include "nls.h"
#include "proc/whattime.h"
#include "proc/version.h"

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fputs(USAGE_HEADER, out);
	fprintf(out, _(" %s [options]\n"), program_invocation_short_name);
	fputs(USAGE_OPTIONS, out);
	fputs(_(" -p, --pretty   show uptime in pretty format\n"), out);
	fputs(USAGE_HELP, out);
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
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, 0}
	};

	program_invocation_name = program_invocation_short_name;
	setlocale (LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	while ((c = getopt_long(argc, argv, "phV", longopts, NULL)) != -1)
		switch (c) {
		case 'p':
			p = 1;
			break;
		case 'h':
			usage(stdout);
		case 'V':
			printf(PROCPS_NG_VERSION);
			return EXIT_SUCCESS;
		default:
			usage(stderr);
		}

	print_uptime(p);
	return EXIT_SUCCESS;
}
