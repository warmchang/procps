#ifndef PROCPS_PROC_ESCAPE_H
#define PROCPS_PROC_ESCAPE_H

#include "readproc.h"


#define ESC_ARGS     0x1  // try to use cmdline instead of cmd
#define ESC_BRACKETS 0x2  // if using cmd, put '[' and ']' around it
#define ESC_DEFUNCT  0x4  // mark zombies with " <defunct>"

int escape_command(char *__restrict const outbuf, const proc_t *__restrict const pp, int bytes, int *cells, unsigned flags);

#define ESC_STRETCH 1  // since we mangle to '?' this is 1 (would be 4 for octal escapes)
int escape_str(char *__restrict dst, const char *__restrict src, int bufsize, int *maxcells);

#endif
