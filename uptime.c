#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include "proc/whattime.h"
#include "proc/version.h"

static void __attribute__ ((__noreturn__))
    usage(FILE * out)
{
	fprintf(out,
		"\nUsage: %s [options]\n"
		"\nOptions:\n", program_invocation_short_name);
	fprintf(out,
		"  -h, --help          display this help text\n"
		"  -V, --version       display version information and exit\n");
	fprintf(out, "\nFor more information see uptime(1).\n");

	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
	int c;

	static const struct option longopts[] = {
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'V'},
		{NULL, 0, NULL, 0}
	};

	while ((c = getopt_long(argc, argv, "hV", longopts, NULL)) != -1)
		switch (c) {
		case 'h':
			usage(stdout);
		case 'V':
			display_version();
			return EXIT_SUCCESS;
		default:
			usage(stderr);
		}

	print_uptime();
	return EXIT_SUCCESS;
}
