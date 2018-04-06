/*
 * libprocps - Library to read proc filesystem
 *
 * Copyright (C) 1996 Charles Blake <cblake@bbn.com>
 * Copyright (C) 1998 Michael K. Johnson
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
#ifndef PROC_NAMESPACE_H
#define PROC_NAMESPACE_H

#ifdef __cplusplus
extern "C" {
#endif

enum namespace_type {
    PROCPS_NS_IPC,
    PROCPS_NS_MNT,
    PROCPS_NS_NET,
    PROCPS_NS_PID,
    PROCPS_NS_USER,
    PROCPS_NS_UTS,
    PROCPS_NS_COUNT  // total namespaces (fencepost)
};

struct procps_namespaces {
    unsigned long ns[PROCPS_NS_COUNT];
};

const char *procps_ns_get_name(const int id);

int procps_ns_get_id(const char *name);

int procps_ns_read_pid(const int pid, struct procps_namespaces *nsp);

#ifdef __cplusplus
}
#endif
#endif
