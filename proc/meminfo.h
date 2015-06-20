/*
 * libprocps - Library to read proc filesystem
 * meminfo - Parse /proc/meminfo
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
    PROCPS_MEMINFO_ACTIVE,
    PROCPS_MEMINFO_INACTIVE,
    PROCPS_MEMINFO_HIGH_FREE,
    PROCPS_MEMINFO_HIGH_TOTAL,
    PROCPS_MEMINFO_LOW_FREE,
    PROCPS_MEMINFO_LOW_TOTAL,
    PROCPS_MEMINFO_MAIN_AVAILABLE,
    PROCPS_MEMINFO_MAIN_BUFFERS,
    PROCPS_MEMINFO_MAIN_CACHED,
    PROCPS_MEMINFO_MAIN_FREE,
    PROCPS_MEMINFO_MAIN_SHARED,
    PROCPS_MEMINFO_MAIN_TOTAL,
    PROCPS_MEMINFO_MAIN_USED,
    PROCPS_MEMINFO_SWAP_FREE,
    PROCPS_MEMINFO_SWAP_TOTAL,
    PROCPS_MEMINFO_SWAP_USED,
};
unsigned long procps_meminfo_get(struct procps_meminfo *info, enum meminfo_item item);

__END_DECLS
#endif

