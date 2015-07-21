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
    PROCPS_CPU_USER,           // jiff
    PROCPS_CPU_NICE,           // jiff
    PROCPS_CPU_SYSTEM,         // jiff
    PROCPS_CPU_IDLE,           // jiff
    PROCPS_CPU_IOWAIT,         // jiff
    PROCPS_CPU_IRQ,            // jiff
    PROCPS_CPU_SIRQ,           // jiff
    PROCPS_CPU_STOLEN,         // jiff
    PROCPS_CPU_GUEST,          // jiff
    PROCPS_CPU_GNICE,          // jiff
    PROCPS_CPU_noop,           // n/a
    PROCPS_CPU_stack_end       // n/a
};

enum procps_stat_item {
    PROCPS_STAT_INTR,          // u_int
    PROCPS_STAT_CTXT,          // u_int
    PROCPS_STAT_BTIME,         // u_int
    PROCPS_STAT_PROCS,         // u_int
    PROCPS_STAT_PROCS_BLK,     // u_int
    PROCPS_STAT_PROCS_RUN,     // u_int
    PROCPS_STAT_noop,          // n/a
    PROCPS_STAT_stack_end      // n/a
};

typedef unsigned long long jiff;

struct procps_jiffs {
    jiff user, nice, system, idle, iowait, irq, sirq, stolen, guest, gnice;
};

struct procps_jiffs_hist {
    struct procps_jiffs new;
    struct procps_jiffs old;
    int id;
};

struct stat_result {
    int item;
    union {
        unsigned int u_int;
        jiff jiff;
    } result;
    struct stat_result *next;
};

struct procps_stat;

int procps_stat_new (struct procps_stat **info);
int procps_stat_read (struct procps_stat *info, const int cpu_only);
int procps_stat_read_jiffs (struct procps_stat *info);

int procps_stat_ref (struct procps_stat *info);
int procps_stat_unref (struct procps_stat **info);

jiff procps_stat_cpu_get (
    struct procps_stat *info,
    enum procps_cpu_item item);

int procps_stat_cpu_getstack (
    struct procps_stat *info,
    struct stat_result *item);

int procps_stat_jiffs_get (
    struct procps_stat *info,
    struct procps_jiffs *item,
    int which);

int procps_stat_jiffs_hist_get (
    struct procps_stat *info,
    struct procps_jiffs_hist *item,
    int which);

int procps_stat_jiffs_fill (
    struct procps_stat *info,
    struct procps_jiffs *item,
    int numitems);

int procps_stat_jiffs_hist_fill (
    struct procps_stat *info,
    struct procps_jiffs_hist *item,
    int numitems);

unsigned int procps_stat_sys_get (
    struct procps_stat *info,
    enum procps_stat_item item);

int procps_stat_sys_getstack (
    struct procps_stat *info,
    struct stat_result *item);

__END_DECLS
#endif
