/*
  Some output conversion routines for libproc
  Copyright (C) 1996, Charles Blake.  See COPYING for details.
*/
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "procps.h"

#if 0
/* output a string, converting unprintables to octal as we go, and stopping after
   processing max chars of output (accounting for expansion due to octal rep).
*/
unsigned print_str(FILE *restrict file, const char *restrict const s, unsigned max) {
    unsigned i;
    for (i=0; s[i] && i < max; i++)
	if (isprint(s[i]) || s[i] == ' ')
	    fputc(s[i], file);
	else {
	    if (max > i+3) {
		fprintf(file, "\\%03o", (unsigned char)s[i]);
		i += 3; /* 4 printed, but i counts one */
	    } else
		return max - i;
	}
    return max - i;
}
#endif

/* output an argv style NULL-terminated string list, converting unprintables
   to octal as we go, separating items of the list by 'sep' and stopping after
   processing max chars of output (accounting for expansion due to octal rep).
*/
unsigned print_strlist(FILE *restrict file, const char *restrict const *restrict strs, unsigned max) {
    unsigned i, n;
    for (n=0; *strs && n < max; strs++) {
	for (i=0; strs[0][i] && n+i < max; i++)
	    if (isprint(strs[0][i]) || strs[0][i] == ' ')
		fputc(strs[0][i], file);
	    else {
		if (max > n+i+3) {
		    fprintf(file, "\\%03o", (unsigned char)strs[0][i]);
		    n += 3; /* 4 printed, but i counts one */
		} else
		    return max - n;
	    }
	n += i;
	if (n + 1 < max) {
	    fputc(' ', file);
	    n++;
	} else
	    return max - n;
    }
    return max - n;
}
