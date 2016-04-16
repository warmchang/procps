/*
 * diskstat - Disk statistics - part of procps
 *
 * Copyright (c) 2003 Fabian Frederick
 * Copyright (C) 2003 Albert Cahalan
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
#ifndef PROC_DISKSTAT_H
#define PROC_DISKSTAT_H

#include <features.h>

__BEGIN_DECLS

enum procps_diskstat_devitem {
    PROCPS_DISKSTAT_READS,
    PROCPS_DISKSTAT_READS_MERGED,
    PROCPS_DISKSTAT_READ_SECTORS,
    PROCPS_DISKSTAT_READ_TIME,
    PROCPS_DISKSTAT_WRITES,
    PROCPS_DISKSTAT_WRITES_MERGED,
    PROCPS_DISKSTAT_WRITE_SECTORS,
    PROCPS_DISKSTAT_WRITE_TIME,
    PROCPS_DISKSTAT_IO_INPROGRESS,
    PROCPS_DISKSTAT_IO_TIME,
    PROCPS_DISKSTAT_IO_WTIME
};

struct procps_diskstat;
struct procps_diskstat_dev;

int procps_diskstat_new (struct procps_diskstat **info);
int procps_diskstat_read (struct procps_diskstat *info);

int procps_diskstat_ref (struct procps_diskstat *info);
int procps_diskstat_unref (struct procps_diskstat **info);

int procps_diskstat_dev_count (const struct procps_diskstat *info);
int procps_diskstat_dev_getbyname(
        const struct procps_diskstat *info,
        const char *name);

unsigned long procps_diskstat_dev_get(
        const struct procps_diskstat *info,
        const enum procps_diskstat_devitem item,
        const unsigned int devid);

char* procps_diskstat_dev_getname(
        const struct procps_diskstat *info,
        const unsigned int devid);

int procps_diskstat_dev_isdisk(
        const struct procps_diskstat *info,
        const unsigned int devid);
__END_DECLS
#endif
