#ifndef PROCPS_PROC_ESCAPE_H
#define PROCPS_PROC_ESCAPE_H

#include "readproc.h"

#define ESC_BRACKETS 0x2  // if using cmd, put '[' and ']' around it
#define ESC_DEFUNCT  0x4  // mark zombies with " <defunct>"

int escape_command (unsigned char *outbuf, const proc_t *pp, int bytes, unsigned flags);

int escape_str (unsigned char *dst, const unsigned char *src, int bufsize);

#endif
