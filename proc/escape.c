/*
 * escape.c - printing handling
 * Copyright 1998-2002 by Albert Cahalan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "escape.h"
#include "readproc.h"


#define SECURE_ESCAPE_ARGS(dst, bytes) do { \
  if ((bytes) <= 0) return 0; \
  *(dst) = '\0'; \
  if ((bytes) >= INT_MAX) return 0; \
} while (0)


int escape_str (unsigned char *dst, const unsigned char *src, int bufsize) {
  int i, n;

  SECURE_ESCAPE_ARGS(dst, bufsize);

  n = snprintf(dst, bufsize, "%s", src);
  if (n < 0) {
    *dst = '\0';
    return 0;
  }
  if (n >= bufsize) n = bufsize-1;

  // control chars, especially tabs, create alignment problems for ps & top ...
  for (i = 0; i < n; i++)
    if (dst[i] < 0x20 || dst[i] == 0x7f)
      dst[i] = '?';

  return n;
}


int escape_command (unsigned char *outbuf, const proc_t *pp, int bytes, unsigned flags) {
  int overhead = 0;
  int end = 0;

  if(flags & ESC_BRACKETS){
    overhead += 2;
  }
  if(flags & ESC_DEFUNCT){
    if(pp->state=='Z') overhead += 10;    // chars in " <defunct>"
    else flags &= ~ESC_DEFUNCT;
  }
  if(overhead + 1 >= bytes){   // if no room for even one byte of the command name
    outbuf[0] = '\0';
    return 0;
  }
  if(flags & ESC_BRACKETS){
    outbuf[end++] = '[';
  }
  end += escape_str(outbuf+end, pp->cmd, bytes-overhead);

  // Hmmm, do we want "[foo] <defunct>" or "[foo <defunct>]"?
  if(flags & ESC_BRACKETS){
    outbuf[end++] = ']';
  }
  if(flags & ESC_DEFUNCT){
    memcpy(outbuf+end, " <defunct>", 10);
    end += 10;
  }
  outbuf[end] = '\0';
  return end;  // bytes, not including the NUL
}
