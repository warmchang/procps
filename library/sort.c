/*
 * sort.c - a 'stable' mergesort
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
 */
int mergesort_r (
        void *base,
        size_t nmemb,
        size_t size,
        int (*compar)(const void *, const void *, void *),
        void *arg)
{
    size_t mid, i, j, k;
    char *array;
    void *temp;

    if (nmemb < 2) return 1; // already sorted

    array = base;
    mid = nmemb / 2;

    // allocate temporary buffer for merging
    if (!(temp = malloc(nmemb * size)))
        return 0;

    // recursive sort left half
    if (!mergesort_r(array, mid, size, compar, arg))
        return 0;
    // recursive sort right half
    if (!mergesort_r(array + (mid * size), nmemb - mid, size, compar, arg))
        return 0;

    // merge step
    i = k = 0;
    j = mid;
    while (i < mid && j < nmemb) {
        if (compar(array + (i * size), array + (j * size), arg) <= 0) {
            memcpy((char *)temp + (k * size), array + (i * size), size);
            i++;
        } else {
            memcpy((char *)temp + (k * size), array + (j * size), size);
            j++;
        }
        k++;
    }

    // copy remaining elements
    while (i < mid) {
        memcpy((char *)temp + (k * size), array + (i * size), size);
        i++; k++;
    }
    while (j < nmemb) {
        memcpy((char *)temp + (k * size), array + (j * size), size);
        j++; k++;
    }

    // copy merged result back to original array
    memcpy(array, temp, nmemb * size);
    free(temp);
    return 1;
}
