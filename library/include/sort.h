/*
 * sort.h - a 'stable' mergesort
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

#ifndef PROCPS_SORT_H
#define PROCPS_SORT_H

/*
 * mergesort with callback and user parameter (like qsort_r)
 *   base:   pointer to the first element
 *   nmemb:  number of elements
 *   compar: comparator function (returns <0, 0, >0)
 *   arg:    extra user parameter passed to comparator
 *
 * but, we return 1 on success, 0 on malloc failure!
 *
 * Attention:
 *   This guy deals EXCLUSIVELY with sorting pointers,
 *   implied by the absence of that qsort 'size' parm!
 */

int mergesort_r (
        void *base,
        size_t nmemb,
        int (*compar)(const void *, const void *, void *),
        void *arg);

#endif
