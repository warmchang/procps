/*
 * Copyright 1998 by Albert Cahalan; all rights resered.         
 * This file may be used subject to the terms and conditions of the
 * GNU Library General Public License Version 2, or any later version  
 * at your option, as published by the Free Software Foundation.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Library General Public License for more details.
 */                                 
#include <sys/types.h>

/* sanitize a string, without the nice BSD library function:     */
/* strvis(vis_args, k->ki_args, VIS_TAB | VIS_NL | VIS_NOSLASH)  */
int octal_escape_str(char *dst, const char *src, size_t n){
  unsigned char c;
  char d;
  size_t i;
  const char *codes =
  "Z------abtnvfr-------------e----"
  " *******************************"  /* better: do not print any space */
  "****************************\\***"
  "*******************************-"
  "--------------------------------"
  "********************************"
  "********************************"
  "********************************";
  for(i=0; i<n;){
    c = (unsigned char) *(src++);
    d = codes[c];
    switch(d){
    case 'Z':
      goto leave;
    case '*':
      i++;
      *(dst++) = c;
      break;
    case '-':
      if(i+4 > n) goto leave;
      i += 4;
      *(dst++) = '\\';
      *(dst++) = "01234567"[c>>6];
      *(dst++) = "01234567"[(c>>3)&07];
      *(dst++) = "01234567"[c&07];
      break;
    default:
      if(i+2 > n) goto leave;
      i += 2;
      *(dst++) = '\\';
      *(dst++) = d;
      break;
    }
  }
leave:
  *(dst++) = '\0';
  return i;
}

/* sanitize a string via one-way mangle */
int simple_escape_str(char *dst, const char *src, size_t n){
  unsigned char c;
  size_t i;
  const char *codes =
  "Z-------------------------------"
  "********************************"
  "********************************"
  "*******************************-"
  "--------------------------------"
  "********************************"
  "********************************"
  "********************************";
  for(i=0; i<n;){
    c = (unsigned char) *(src++);
    switch(codes[c]){
    case 'Z':
      goto leave;
    case '*':
      i++;
      *(dst++) = c;
      break;
    case '-':
      i++;
      *(dst++) = '?';
      break;
    }
  }
leave:
  *(dst++) = '\0';
  return i;
}

/* escape a string as desired */
int escape_str(char *dst, const char *src, size_t n){
  return simple_escape_str(dst, src, n);
}

/* escape an argv or environment string array */
int escape_strlist(char *dst, const char **src, size_t n){
  size_t i = 0;
  while(*src){
    i += simple_escape_str(dst+i, *src, n-i);
    if((n-i > 1) && (*(src+1))) dst[i++] = ' ';
    src++;
  }
  return i;
}
