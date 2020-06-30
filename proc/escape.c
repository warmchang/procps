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

#include <ctype.h>
#include <langinfo.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>  /* MB_CUR_MAX */
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <sys/types.h>

#include "escape.h"
#include "readproc.h"


#define SECURE_ESCAPE_ARGS(dst, bytes, cells) do { \
  if ((bytes) <= 0) return 0; \
  *(dst) = '\0'; \
  if ((bytes) >= INT_MAX) return 0; \
  if ((cells) >= INT_MAX) return 0; \
  if ((cells) <= 0) return 0; \
} while (0)


static int escape_str_utf8 (char *dst, const char *src, int bufsize, int *maxcells) {
  int my_cells = 0;
  int my_bytes = 0;
  mbstate_t s;

  SECURE_ESCAPE_ARGS(dst, bufsize, *maxcells);

  memset(&s, 0, sizeof (s));

  for(;;) {
    wchar_t wc;
    int len = 0;

    if(my_cells >= *maxcells || my_bytes+1 >= bufsize)
      break;

    if (!(len = mbrtowc (&wc, src, MB_CUR_MAX, &s)))
      /* 'str' contains \0 */
      break;

    if (len < 0) {
      /* invalid multibyte sequence -- zeroize state */
      memset (&s, 0, sizeof (s));
      *(dst++) = '?';
      src++;
      my_cells++;
      my_bytes++;

    } else if (len==1) {
      /* non-multibyte */
      *(dst++) = isprint(*src) ? *src : '?';
      src++;
      my_cells++;
      my_bytes++;

    } else if (!iswprint(wc)) {
      /* multibyte - no printable */
      *(dst++) = '?';
      src+=len;
      my_cells++;
      my_bytes++;

    } else {
      /* multibyte - maybe, kinda "printable" */
      int wlen = wcwidth(wc);
      // Got space?
      if (wlen > *maxcells-my_cells || len >= bufsize-(my_bytes+1)) break;
      // safe multibyte
      memcpy(dst, src, len);
      dst += len;
      src += len;
      my_bytes += len;
      if (wlen > 0) my_cells += wlen;
    }
    //fprintf(stdout, "cells: %d\n", my_cells);
  }
  *dst = '\0';

  // fprintf(stderr, "maxcells: %d, my_cells; %d\n", *maxcells, my_cells);

  *maxcells -= my_cells;
  return my_bytes;        // bytes of text, excluding the NUL
}


/* sanitize a string via one-way mangle */
int escape_str (char *dst, const char *src, int bufsize, int *maxcells) {
  unsigned char c;
  int my_cells = 0;
  int my_bytes = 0;
  const char codes[] =
  "Z..............................."
  "||||||||||||||||||||||||||||||||"
  "||||||||||||||||||||||||||||||||"
  "|||||||||||||||||||||||||||||||."
  "????????????????????????????????"
  "????????????????????????????????"
  "????????????????????????????????"
  "????????????????????????????????";

  static int utf_init=0;

  if(utf_init==0){
     /* first call -- check if UTF stuff is usable */
     char *enc = nl_langinfo(CODESET);
     utf_init = enc && strcasecmp(enc, "UTF-8")==0 ? 1 : -1;
  }
  if (utf_init==1 && MB_CUR_MAX>1) {
     /* UTF8 locales */
     return escape_str_utf8(dst, src, bufsize, maxcells);
  }

  SECURE_ESCAPE_ARGS(dst, bufsize, *maxcells);

  if(bufsize > *maxcells+1) bufsize=*maxcells+1; // FIXME: assumes 8-bit locale

  for(;;){
    if(my_cells >= *maxcells || my_bytes+1 >= bufsize)
      break;
    c = (unsigned char) *(src++);
    if(!c) break;
    if(codes[c]!='|') c=codes[c];
    my_cells++;
    my_bytes++;
    *(dst++) = c;
  }
  *dst = '\0';

  *maxcells -= my_cells;
  return my_bytes;        // bytes of text, excluding the NUL
}

/////////////////////////////////////////////////

// escape an argv or environment string array
//
// bytes arg means sizeof(buf)
static int escape_strlist (char *dst, const char **src, size_t bytes, int *cells) {
  size_t i = 0;

  for(;;){
    i += escape_str(dst+i, *src, bytes-i, cells);
    if(bytes-i < 3) break;  // need room for space, a character, and the NUL
    src++;
    if(!*src) break;  // need something to print
    if (*cells<=1) break;  // need room for printed size of text
    dst[i++] = ' ';
    --*cells;
  }
  return i;    // bytes, excluding the NUL
}

///////////////////////////////////////////////////

int escape_command (char *const outbuf, const proc_t *pp, int bytes, int *cells, unsigned flags) {
  int overhead = 0;
  int end = 0;

  if(flags & ESC_ARGS){
    const char **lc = (const char**)pp->cmdline;
    if(lc && *lc) return escape_strlist(outbuf, lc, bytes, cells);
  }
  if(flags & ESC_BRACKETS){
    overhead += 2;
  }
  if(flags & ESC_DEFUNCT){
    if(pp->state=='Z') overhead += 10;    // chars in " <defunct>"
    else flags &= ~ESC_DEFUNCT;
  }
  if(overhead + 1 >= *cells || // if no room for even one byte of the command name
     overhead + 1 >= bytes){
    outbuf[0] = '\0';
    return 0;
  }
  if(flags & ESC_BRACKETS){
    outbuf[end++] = '[';
  }
  *cells -= overhead;
  end += escape_str(outbuf+end, pp->cmd, bytes-overhead, cells);

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
