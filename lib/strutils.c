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
	return EXIT_FAILURE;
}
#endif				/* TEST_PROGRAM */
