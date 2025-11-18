/*
 * sort.c - a 'stable' mergesort (with minimum malloc overhead)
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
 *   size:   size of each element in bytes
 *   compar: comparator function (returns <0, 0, >0)
 *   arg:    extra user parameter passed to comparator
 *
 * but, we return 1 on success or 0 on malloc failure!
 * plus we issue only a single malloc each invocation!
 */
int mergesort_r (
        void *base,
        size_t nmemb,
        size_t size,
        int (*compar)(const void *, const void *, void *),
        void *arg)
{
    void *aux;
    char *bas, *buf, *tmp;
    size_t left, mid, right, width, i, l, r, k;

    if (nmemb < 2 || size == 0) return 1;

    // allocate one auxiliary buffer for the entire sort
    if (!(aux = malloc(nmemb * size)))
        return 0;

    bas = base;
    buf = aux;

    // bottom-up merge sort
    for (width = 1; width < nmemb; width *= 2) {
        for (i = 0; i < nmemb; i += 2 * width) {
            left  = i;
            mid   = (i + width < nmemb) ? i + width : nmemb;
            right = (i + (2 * width) < nmemb) ? i + (2 * width) : nmemb;

            l = left, r = mid, k = left;

            // merge two sorted halfs into buffer
            while (l < mid && r < right) {
                if (compar(bas + (l * size), bas + (r * size), arg) <= 0) {
                    memcpy(buf + (k * size), bas + (l * size), size);
                    l++;
                } else {
                    memcpy(buf + (k * size), bas + (r * size), size);
                    r++;
                }
                k++;
            }

            // copy remaining left stuff
            while (l < mid) {
                memcpy(buf + (k * size), bas + (l * size), size);
                l++; k++;
            }
            // copy remaining right stuff
            while (r < right) {
                memcpy(buf + (k * size), bas + (r * size), size);
                r++; k++;
            }
        }

        // swap roles of bas and buf
        tmp = bas;
        bas = buf;
        buf = tmp;
    }

    // if sorted data is in aux, copy back to base
    if (bas != (char *)base)
        memcpy(base, bas, nmemb * size);

    free(aux);
    return 1;
} // end: mergesort_r
