/*
 * meminfo - Memory statistics part of procps
 *
 * Copyright (C) 1992-1998 by Michael K. Johnson <johnsonm@redhat.com>
 * Copyright (C) 1998-2003 Albert Cahalan
 * Copyright (C) 2015 Craig Small <csmall@enc.com.au>
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

#ifndef PROC_MEMINFO_H
#define PROC_MEMINFO_H

#include <proc/procps.h>

__BEGIN_DECLS

enum meminfo_item {
    PROCPS_MEMHI_FREE,         // ul_int
    PROCPS_MEMHI_TOTAL,        // ul_int
    PROCPS_MEMHI_USED,         // ul_int
    PROCPS_MEMLO_FREE,         // ul_int
    PROCPS_MEMLO_TOTAL,        // ul_int
    PROCPS_MEMLO_USED,         // ul_int
    PROCPS_MEM_ACTIVE,         // ul_int
    PROCPS_MEM_AVAILABLE,      // ul_int
    PROCPS_MEM_BUFFERS,        // ul_int
    PROCPS_MEM_CACHED,         // ul_int
    PROCPS_MEM_FREE,           // ul_int
    PROCPS_MEM_INACTIVE,       // ul_int
    PROCPS_MEM_SHARED,         // ul_int
    PROCPS_MEM_TOTAL,          // ul_int
    PROCPS_MEM_USED,           // ul_int
    PROCPS_SWAP_FREE,          // ul_int
    PROCPS_SWAP_TOTAL,         // ul_int
    PROCPS_SWAP_USED,          // ul_int
    PROCPS_MEM_noop,           // n/a
    PROCPS_MEM_stack_end       // n/a
};

struct procps_meminfo;

struct meminfo_result {
    enum meminfo_item item;
    union {
        unsigned long ul_int;
    } result;
};

struct meminfo_stack {
    struct meminfo_result *head;
};


int procps_meminfo_new (struct procps_meminfo **info);
int procps_meminfo_read (struct procps_meminfo *info);

int procps_meminfo_ref (struct procps_meminfo *info);
int procps_meminfo_unref (struct procps_meminfo **info);

unsigned long procps_meminfo_get (
    struct procps_meminfo *info,
    enum meminfo_item item);

int procps_meminfo_getstack (
    struct procps_meminfo *info,
    struct meminfo_result *these);

int procps_meminfo_stack_fill (
    struct procps_meminfo *info,
    struct meminfo_stack *stack);

struct meminfo_stack *procps_meminfo_stack_alloc (
    struct procps_meminfo *info,
    int maxitems,
    enum meminfo_item *items);

__END_DECLS
#endif
