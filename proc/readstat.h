/*
 * libprocps - Library to read proc filesystem
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
#ifndef PROC_READ_STAT_H
#define PROC_READ_STAT_H

#include <proc/procps.h>

__BEGIN_DECLS

enum procps_cpu_item {
    PROCPS_CPU_USER,
    PROCPS_CPU_NICE,
    PROCPS_CPU_SYSTEM,
    PROCPS_CPU_IDLE,
    PROCPS_CPU_IOWAIT,
    PROCPS_CPU_IRQ,
    PROCPS_CPU_SIRQ,
    PROCPS_CPU_STOLEN,
    PROCPS_CPU_GUEST,
    PROCPS_CPU_GNICE
};

enum procps_stat_item {
    PROCPS_STAT_INTR,
    PROCPS_STAT_CTXT,
    PROCPS_STAT_BTIME,
    PROCPS_STAT_PROCS,
    PROCPS_STAT_PROCS_BLK,
    PROCPS_STAT_PROCS_RUN
};

typedef unsigned long long jiff;
struct procps_stat_info;
int procps_stat_new(struct procps_stat_info **info);
int procps_stat_read(struct procps_stat_info *info, const int cpu_only);
struct procps_stat_info *procps_stat_ref(struct procps_stat_info *info);
struct procps_stat_info *procps_stat_unref(struct procps_stat_info *info);
jiff procps_stat_get_cpu(struct procps_stat_info *info, enum procps_cpu_item item);
unsigned int procps_stat_get(struct procps_stat_info *info, enum procps_stat_item item);
__END_DECLS
#endif
