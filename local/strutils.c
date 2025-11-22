/*
 * strutils.c - various string routines shared by commands
 * This file was originally copied from util-linux at fall 2011.
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
#include <stdbool.h>
#include <assert.h>
#include <ctype.h>
#include <assert.h>
#include <wchar.h>
#include "xalloc.h"
#include "c.h"
#include "xalloc.h"
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

/*
 * Covert a string into a double in a non-locale aware way.
 * This means the decimal point can be either . or ,
 * Also means you cannot use the other for thousands separator
 *
 * Exits on failure like its other _or_err cousins
 */
double strtod_nol_or_err(const char *str, const char *errmesg)
{
	double num;
	const char *cp, *radix;
	double mult;
	int negative = 0;

	if (str != NULL && *str != '\0') {
		num = 0.0;
		cp = str;
		/* strip leading spaces */
		while (isspace(*cp))
			cp++;

		/* get sign */
		if (*cp == '-') {
			negative = 1;
			cp++;
		} else if (*cp == '+')
			cp++;

		/* find radix */
		radix = cp;
		mult=0.1;
		while(isdigit(*radix)) {
			radix++;
			mult *= 10;
		}
		while(isdigit(*cp)) {
			num += (*cp - '0') * mult;
			mult /= 10;
			cp++;
		}
		/* got the integers */
		if (*cp == '\0')
			return (negative?-num:num);
		if (*cp != '.' && *cp != ',')
			error(EXIT_FAILURE, EINVAL, "%s: '%s'", errmesg, str);

		cp++;
		mult = 0.1;
		while(isdigit(*cp)) {
			num += (*cp - '0') * mult;
			mult /= 10;
			cp++;
		}
		if (*cp == '\0')
			return (negative?-num:num);
	}
	error(EXIT_FAILURE, errno, "%s: '%s'", errmesg, str);
	return 0;
}

// column width of a multi-byte string
// s is \0-term.
// pwcs !=NULL => address of s converted to wide string is stored in *pwcs, will
// be \0-term., no additional cost in receiving it, caller free()s.
// Error => -1 and *pwcs is unchanged.
//
// When addstr()ing in ncurses, make use of pwcs and addwstr() it, cause else
// ncurses will do the whole conversion and validation of the sequences in s
// again.
int mbswidth(const char *restrict s, wchar_t *restrict *const restrict pwcs)
{
	assert(s);

	size_t wclen = mbstowcs(NULL, s, 0);  // wclen doesn't incl. \0
	if (wclen == (size_t)-1) {
		errno = EILSEQ;
		return -1;
	}
	++wclen;  // ok, it's !=SIZE_MAX
	if ((uintmax_t)wclen * sizeof(wchar_t) / sizeof(wchar_t) != wclen) {
		errno = EOVERFLOW;
		return -1;
	}

	wchar_t *wcs = xmalloc(wclen * sizeof(*wcs));
	mbstowcs(wcs, s, wclen);

	// wcswidth() is useless. POSIX 2001 doesn't say what it does when the width
	// is > INT_MAX. May be some UB.
	int ret = 0;
	int curwid;
	for (--wclen; wclen; --wclen) {
		curwid = wcwidth(wcs[wclen-1]);
		if (curwid == -1) {
			free(wcs);
			errno = EILSEQ;
			return -1;
		}
		if (INT_MAX - ret < curwid) {
			free(wcs);
			errno = EOVERFLOW;
			return -1;
		}
		ret += curwid;
	}

	if (pwcs) {
		*pwcs = wcs;
	} else free(wcs);
	return ret;
}
