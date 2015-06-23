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

struct procps_meminfo;
int procps_meminfo_new(struct procps_meminfo **info);
int procps_meminfo_read(struct procps_meminfo *info);
struct procps_meminfo *procps_meminfo_ref(struct procps_meminfo *info);
struct procps_meminfo *procps_meminfo_unref(struct procps_meminfo *info);

enum meminfo_item {
    PROCPS_MEM_ACTIVE,
    PROCPS_MEM_INACTIVE,
    PROCPS_MEMHI_FREE,
    PROCPS_MEMHI_TOTAL,
    PROCPS_MEMHI_USED,
    PROCPS_MEMLO_FREE,
    PROCPS_MEMLO_TOTAL,
    PROCPS_MEMLO_USED,
    PROCPS_MEM_AVAILABLE,
    PROCPS_MEM_BUFFERS,
    PROCPS_MEM_CACHED,
    PROCPS_MEM_FREE,
    PROCPS_MEM_SHARED,
    PROCPS_MEM_TOTAL,
    PROCPS_MEM_USED,
    PROCPS_SWAP_FREE,
    PROCPS_SWAP_TOTAL,
    PROCPS_SWAP_USED,
};
unsigned long procps_meminfo_get(struct procps_meminfo *info, enum meminfo_item item);

__END_DECLS
#endif

