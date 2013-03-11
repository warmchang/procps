/*
 * strutils.c - various string routines shared by commands
 * This file was copied from util-linux at fall 2011.
 *
 * Copyright (C) 2010 Karel Zak <kzak@redhat.com>
 * Copyright (C) 2010 Davidlohr Bueso <dave@gnu.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdlib.h>

#include "c.h"
#include "strutils.h"

/*
 * same as strtol(3) but exit on failure instead of returning crap
 */
long strtol_or_err(const char *str, const char *errmesg)
{
	long num;
	char *end = NULL;

	if (str != NULL && *str != '\0') {
		errno = 0;
		num = strtol(str, &end, 10);
		if (errno == 0 && str != end && end != NULL && *end == '\0')
			return num;
	}
	error(EXIT_FAILURE, errno, "%s: '%s'", errmesg, str);
	return 0;
}

/*
 * same as strtod(3) but exit on failure instead of returning crap
 */
double strtod_or_err(const char *str, const char *errmesg)
{
	double num;
	char *end = NULL;

	if (str != NULL && *str != '\0') {
		errno = 0;
		num = strtod(str, &end);
		if (errno == 0 && str != end && end != NULL && *end == '\0')
			return num;
	}
	error(EXIT_FAILURE, errno, "%s: '%s'", errmesg, str);
	return 0;
}

#ifdef TEST_PROGRAM
int main(int argc, char *argv[])
{
	if (argc < 2) {
		error(EXIT_FAILURE, 0, "no arguments");
	} else if (argc < 3) {
		printf("%ld\n", strtol_or_err(argv[1], "strtol_or_err"));
	} else {
		printf("%lf\n", strtod_or_err(argv[2], "strtod_or_err"));
	}
	return EXIT_SUCCESS;
}
#endif				/* TEST_PROGRAM */
