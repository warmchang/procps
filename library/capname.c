/*
 * capnames.c - Translate capability mask names
 *
 * Copyright © 2024      Craig Small <csmall@dropbear.xyz>
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

#include <stdio.h>
//#include <unistd.h>
//#//include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
//#include <limits.h>
//#include <stdlib.h>
#include <string.h>
//#include <fcntl.h>
//#include <dirent.h>
//#include <ctype.h>
//#include <pwd.h>
#include <inttypes.h>
//#include <sys/types.h>
//#include <sys/wait.h>
//#include <sys/types.h>
//#include <sys/stat.h>
//#include <sys/utsname.h>


#include "procps-private.h"
#include "escape.h"
#include "capnames.h"
#include "misc.h"

#define FULL_CAP "full"
#define FULL_CAP_MASK 0x1ffffffffff

static bool capability_isset(const uint64_t mask, const int cnum)
{
    return (mask & ((uint64_t) 1 << (cnum)));
}

PROCPS_EXPORT int procps_capability_names(char *restrict const buf, const char *restrict const capmask, const size_t buflen)
{
    unsigned int i;
    char *c = buf;
    size_t len = buflen;
    uint64_t mask_in;

    // buffer must be at least 2 for "-\0"
    if (buf == NULL || capmask == NULL || buflen < 2)
        return -EINVAL;

    if (1 != sscanf(capmask, "%" PRIx64, &mask_in))
        return -EINVAL;

    if (mask_in == 0) {
        strcpy(buf, "-");
        return 1;
    }
    
    if (mask_in == FULL_CAP_MASK) {
        size_t namelen;
        namelen = strlen(FULL_CAP);
        if (namelen+1 >= len) {
            strcpy(c, "+");
            return 1;
        }
        strcpy(c,FULL_CAP);
        return namelen;
    }

    for (i=0; i <= (CAPABILITY_COUNT) ; i++)
    {
        if (capability_isset(mask_in, i)) {
            if (cap_names[i] != NULL) { // We have a name for this capability
                int namelen;
                namelen = strlen(cap_names[i]);
                if (namelen+1 >= len) {
                    strcpy(c, "+");
                    len -= 1;
                    c += 1;
                    break;
                } else {
                    namelen = snprintf(c, len, (c==buf)?"%s":",%s",
                            cap_names[i]);
                    len -= namelen;
                    c+= namelen;
                }
            }
        }
    }
    return (int) (c-buf);
}
