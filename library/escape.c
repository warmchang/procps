/*
 * escape.c - printing handling
 *
 * Copyright © 2011-2025 Jim Warner <james.warner@comcast.net>
 * Copyright © 2016-2024 Craig Small <csmall@dropbear.xyz>
 * Copyright © 1998-2005 Albert Cahalan
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
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "escape.h"
#include "readproc.h"
#include "nls.h"

#define SECURE_ESCAPE_ARGS(dst, bytes) do { \
  if ((bytes) <= 0) return 0; \
  *(dst) = '\0'; \
  if ((bytes) >= INT_MAX) return 0; \
} while (0)


/*
 * Validate a UTF-8 string, with some characters possibly escaped,
 * while remaining compliant with RFC 3629
 *
 * FIXME: not future-proof
 */
static void u8charlen (unsigned char *s, unsigned size) {
   int n;
   unsigned x;

   while (size) {
      // 0xxxxxxx, U+0000 - U+007F
      if (s[0] <= 0x7f) { n = 1; goto esc_maybe; }
      if (size >= 2 && (s[1] & 0xc0) == 0x80) {
         // 110xxxxx 10xxxxxx, U+0080 - U+07FF
         if (s[0] >= 0xc2 && s[0] <= 0xdf) { n = 2; goto next_up; };
         if (size >= 3 && (s[2] & 0xc0) == 0x80) {
#ifndef OFF_UNICODE_PUA
            x = ((unsigned)s[0] << 16) + ((unsigned)s[1] << 8) + (unsigned)s[2];
            /* 11101110 10000000 10000000, U+E000 - primary PUA begin
               11101111 10100011 10111111, U+F8FF - primary PUA end */
            if (x >= 0xee8080 && x <= 0xefa3bf) goto esc_definitely;
#endif
            x = (unsigned)s[0] << 6 | (s[1] & 0x3f);
            // 1110xxxx 10xxxxxx 10xxxxxx, U+0800 - U+FFFF minus U+D800 - U+DFFF
            if ((x >= 0x3820 && x <= 0x3b5f) || (x >= 0x3b80 && x <= 0x3bff)) { n = 3; goto next_up; };
            if (size >= 4 && (s[3] & 0xc0) == 0x80) {
#ifndef OFF_UNICODE_PUA
               unsigned y;
               y = ((unsigned)s[0] << 24) + ((unsigned)s[1] << 16) + ((unsigned)s[2] << 8) + (unsigned)s[3];
               /* 11110011 10110000 10000000 10000000, U+F0000  - supplemental PUA-A begin
                  11110011 10111111 10111111 10111101, U+FFFFD  - supplemental PUA-A end */
               if (y >= 0xf3b08080 && y <= 0Xf3bfbfbd) goto esc_definitely;
               /* 11110100 10000000 10000000 10000000, U+100000 - supplemental PUA-B begin
                  11110100 10001111 10111111 10111101, U+10FFFD - supplemental PUA-B end */
               if (y >= 0xf4808080 && y <= 0Xf48fbfbd) goto esc_definitely;
#endif
               // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx, U+010000 - U+10FFFF
               if (x >= 0x3c10 && x <= 0x3d0f) { n = 4; goto next_up; };
            }
         }
      }
      // invalid or incomplete sequence
esc_definitely:
      n = -1;
      // fall through
esc_maybe:
      /*  Escape the character to a '?' if
       *  Not valid UTF (when n is -1)
       *  Non-control chars below SPACE
       *  DEL
       *  32 unicode multibyte control characters which begin at U+0080 (0xc280)
       */
      if ((n < 0)
      || ((s[0] < 0x20)
      || ((s[0] == 0x7f)
      || ((s[0] == 0xc2 && s[1] >= 0x80 && s[1] <= 0x9f)))))
         *s = '?';
      n = 1;
      // fall through
next_up:
      s += n;
      size -= n;
   }
}

/*
 * Given a bad locale/corrupt str, replace all non-printing stuff
 */
static inline void esc_all (unsigned char *str) {
   while (*str) {
      if (!isprint(*str))
          *str = '?';
      ++str;
   }
}

int escape_str (char *dst, const char *src, int bufsize) {
   static __thread int utf_sw = 0;
   int n;

   if (utf_sw == 0) {
      char *enc = nl_langinfo(CODESET);
      utf_sw = enc && strcasecmp(enc, "UTF-8") == 0 ? 1 : -1;
   }
   SECURE_ESCAPE_ARGS(dst, bufsize);
   n = snprintf(dst, bufsize, "%s", src);
   if (n < 0) {
      *dst = '\0';
      return 0;
   }
   if (n >= bufsize) n = bufsize-1;
   if (utf_sw < 0)
      esc_all((unsigned char *)dst);
   else
      u8charlen((unsigned char *)dst, n);
   return n;
}

int escape_command (char *outbuf, const proc_t *pp, int bytes, unsigned flags) {
   int overhead = 0;
   int end = 0;

   if (flags & ESC_BRACKETS)
      overhead += 2;
   if (flags & ESC_DEFUNCT) {
      if (pp->state == 'Z') overhead += 10;    // chars in " <defunct>"
      else flags &= ~ESC_DEFUNCT;
   }
   if (overhead + 1 >= bytes) {
      // if no room for even one byte of the command name
      outbuf[0] = '\0';
      return 0;
   }
   if (flags & ESC_BRACKETS)
      outbuf[end++] = '[';
   end += escape_str(outbuf+end, pp->cmd, bytes-overhead);
   // we want "[foo] <defunct>", not "[foo <defunct>]"
   if (flags & ESC_BRACKETS)
      outbuf[end++] = ']';
   if (flags & ESC_DEFUNCT) {
      memcpy(outbuf+end, " <defunct>", 10);
      end += 10;
   }
   outbuf[end] = '\0';
   return end;  // bytes, not including the NUL
}
