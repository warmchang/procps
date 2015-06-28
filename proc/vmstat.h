/*
 * libprocps - Library to read proc filesystem
 *
 * Copyright (C) 1995 Martin Schulze <joey@infodrom.north.de>
 * Copyright (C) 1996 Charles Blake <cblake@bbn.com>
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
#ifndef PROC_VMSTAT_H
#define PROC_VMSTAT_H

#include <proc/procps.h>

__BEGIN_DECLS

enum vmstat_item {
    PROCPS_VMSTAT_PGPGIN,
    PROCPS_VMSTAT_PGPGOUT,
    PROCPS_VMSTAT_PSWPIN,
    PROCPS_VMSTAT_PSWPOUT
};

struct vmstat_result {
    enum vmstat_item item;
    unsigned long result;
    struct vmstat_result *next;
};

struct procps_vmstat;

int procps_vmstat_new (struct procps_vmstat **info);
int procps_vmstat_read (struct procps_vmstat *info);

struct procps_vmstat *procps_vmstat_ref (struct procps_vmstat *info);
struct procps_vmstat *procps_vmstat_unref (struct procps_vmstat *info);

unsigned long procps_vmstat_get (struct procps_vmstat *info, enum vmstat_item item);
int procps_vmstat_get_chain (struct procps_vmstat *info, struct vmstat_result *item);

__END_DECLS
#endif
