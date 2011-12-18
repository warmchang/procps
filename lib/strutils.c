/* 
 * This file was copied from util-linux at fall 2011.
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

	if (str == NULL || *str == '\0')
		goto err;
	errno = 0;
	num = strtol(str, &end, 10);
	if (errno || str == end || (end && *end))
		goto err;

	return num;
 err:
	if (errno)
		err(EXIT_FAILURE, "%s: '%s'", errmesg, str);
	else
		errx(EXIT_FAILURE, "%s: '%s'", errmesg, str);
}
/*
 * same as strtod(3) but exit on failure instead of returning crap
 */
double strtod_or_err(const char *str, const char *errmesg)
{
	double num;
	char *end = NULL;

	if (str == NULL || *str == '\0')
		goto err;
	errno = 0;
	num = strtod(str, &end);

	if (errno || str == end || (end && *end))
		goto err;

	return num;
 err:
	if (errno)
		err(EXIT_FAILURE, "%s: '%s'", errmesg, str);
	else
		errx(EXIT_FAILURE, "%s: '%s'", errmesg, str);
	return 0;
}

#ifdef TEST_PROGRAM
int main(int argc, char *argv[])
{
	return EXIT_FAILURE;
}
#endif				/* TEST_PROGRAM */
