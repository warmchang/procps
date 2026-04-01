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
 * plus we issue, at most, one memcpy each invocation! |
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
    char **src, **next_src, **dst;
    size_t top_start, bottom_start, bottom_end, half_depth, t, b;

    if (nmemb < 2) return 1;

    // allocate one auxiliary buffer for the entire sort
    if (!(aux = malloc(nmemb * sizeof(void *))))
        return 0;

    src = base;
    dst = next_src = aux;

    // bottom-up merge sort
    for (half_depth = 1; half_depth < nmemb; half_depth *= 2) {

        for (top_start = 0; top_start < nmemb; top_start += 2 * half_depth) {
            bottom_start = (top_start + half_depth < nmemb) ? top_start + half_depth : nmemb;
            bottom_end = (top_start + (2 * half_depth) < nmemb) ? top_start + (2 * half_depth) : nmemb;

            t = top_start;
            b = bottom_start;

            // merge two sorted halves into buffer
            while (t < bottom_start && b < bottom_end) {
                if (compar(src + t, src + b, arg) <= 0)
                    *dst = *(src + t++);
                else
                    *dst = *(src + b++);
                dst++;
            }

            // copy remaining top stuff
            while (t < bottom_start)
                *(dst++) = *(src + t++);

            // copy remaining bottom stuff
            while (b < bottom_end)
                *(dst++) = *(src + b++);
        }

        // swap roles of src and dst
        dst = src;
        src = next_src;
        next_src = dst;
    }

    // if sorted data is in aux, copy back to base
    // (and remember, pointers have been switched)
    if (src != (char **)base)
        memcpy(base, src, nmemb * sizeof(void *));

    free(aux);
    return 1;
} // end: mergesort_r
