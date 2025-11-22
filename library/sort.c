/*
 * sort.c - a 'stable' mergesort with:
 *    minimum malloc overhead
 *    minimum memcpy overhead
 *    no need for a size parm
 *
 * Copyright © 2025 Jim Warner <james.warner@comcast.net>
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

#include <stdlib.h>
#include <string.h>

/*
 * mergesort with callback and user parameter (like qsort_r)
 *   base:   pointer to the first element
 *   nmemb:  number of elements
 *   compar: comparator function (returns <0, 0, >0)
 *   arg:    extra user parameter passed to comparator
 *
 * but, we return 1 on success or 0 on malloc failure! |
 * plus we issue only a single malloc each invocation! |
 * plus we issue only a single memcpy each invocation! |
 *
 * Note:
 *   This guy deals EXCLUSIVELY with sorting pointers. |
 *   As a result of this, we can move them around much |
 *   more efficiently than thousands of memcpy() calls |
 *   who then moves a whopping 8 bytes with each call. |
 */

int mergesort_r (
        void *base,
        size_t nmemb,
        int (*compar)(const void *, const void *, void *),
        void *arg)
{
    void *aux;
    char **bas, **buf, **tmp;
    size_t top_half, middle, bottom_half, depth, t, b, k;

    if (nmemb < 2) return 1;

    // allocate one auxiliary buffer for the entire sort
    if (!(aux = malloc(nmemb * sizeof(void *))))
        return 0;

    bas = base;
    buf = aux;

    // bottom-up merge sort
    for (depth = 1; depth < nmemb; depth *= 2) {

        for (top_half = 0; top_half < nmemb; top_half += 2 * depth) {
            middle = (top_half + depth < nmemb) ? top_half + depth : nmemb;
            bottom_half = (top_half + (2 * depth) < nmemb) ? top_half + (2 * depth) : nmemb;

            b = middle;
            t = k = top_half;

            // merge two sorted halfs into buffer
            while (t < middle && b < bottom_half) {
                if (compar(bas + t, bas + b, arg) <= 0) {
                    *(buf + k) = *(bas + t);
                    t++;
                } else {
                    *(buf + k) = *(bas + b);
                    b++;
                }
                k++;
            }

            // copy remaining top stuff
            while (t < middle) {
                *(buf + k) = *(bas + t);
                t++; k++;
            }
            // copy remaining bottom stuff
            while (b < bottom_half) {
                *(buf + k) = *(bas + b);
                b++; k++;
            }
        }

        // swap roles of bas and buf
        tmp = bas;
        bas = buf;
        buf = tmp;
    }

    // if sorted data is in aux, copy back to base
    if (bas != (char **)base)
        memcpy(base, bas, nmemb * sizeof (void *));

    free(aux);
    return 1;
} // end: mergesort_r
