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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <proc/meminfo.h>
#include "procps-private.h"

#define MEMINFO_FILE "/proc/meminfo"

struct meminfo_data {
    unsigned long active;
    unsigned long inactive;
    unsigned long high_free;
    unsigned long high_total;
    unsigned long low_free;
    unsigned long low_total;
    unsigned long available;
    unsigned long buffers;
    unsigned long cached;
    unsigned long free;
    unsigned long shared;
    unsigned long total;
    unsigned long used;
    unsigned long slab;
    unsigned long swap_free;
    unsigned long swap_total;
    unsigned long swap_used;
};

struct procps_meminfo {
    int refcount;
    int meminfo_fd;
    struct meminfo_data data;
};


/*
 * procps_meminfo_new:
 *
 * Create a new container to hold the meminfo information
 *
 * The initial refcount is 1, and needs to be decremented
 * to release the resources of the structure.
 *
 * Returns: a new meminfo info container
 */
PROCPS_EXPORT int procps_meminfo_new (
        struct procps_meminfo **info)
{
    struct procps_meminfo *m;
    m = calloc(1, sizeof(struct procps_meminfo));
    if (!m)
        return -ENOMEM;

    m->refcount = 1;
    m->meminfo_fd = -1;
    *info = m;
    return 0;
}

/*
 * procps_meminfo_read:
 *
 * Read the data out of /proc/meminfo putting the information
 * into the supplied info structure
 */
PROCPS_EXPORT int procps_meminfo_read (
        struct procps_meminfo *info)
{
    char buf[8192];
    char *head, *tail;
    int size;
    unsigned long *valptr;
    signed long mem_used;

    if (info == NULL)
        return -1;

    memset(&(info->data), 0, sizeof(struct meminfo_data));
    /* read in the data */

    if (-1 == info->meminfo_fd && (info->meminfo_fd = open(MEMINFO_FILE, O_RDONLY)) == -1) {
        return -errno;
    }
    if (lseek(info->meminfo_fd, 0L, SEEK_SET) == -1) {
        return -errno;
    }
    for (;;) {
        if ((size = read(info->meminfo_fd, buf, sizeof(buf)-1)) < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            return -errno;
        }
        break;
    }
    if (size == 0)
        return 0;
    buf[size] = '\0';

    /* Scan the file */
    head = buf;
    do {
        tail = strchr(head, ' ');
        if (!tail)
            break;
        *tail = '\0';
        valptr = NULL;
        switch (*head) {
            case 'A':
                if (0 == strcmp(head, "Active:"))
                    valptr = &(info->data.active);
                break;
            case 'B':
                if (0 == strcmp(head, "Buffers:"))
                    valptr = &(info->data.buffers);
                break;
            case 'C':
                if (0 == strcmp(head, "Cached:"))
                    valptr = &(info->data.cached);
                break;
            case 'H':
                if (0 == strcmp(head, "HighFree:"))
                    valptr = &(info->data.high_free);
                else if (0 == strcmp(head, "HighTotal:"))
                    valptr = &(info->data.high_total);
                break;
            case 'I':
                if (0 == strcmp(head, "Inactive:"))
                    valptr = &(info->data.inactive);
                break;
            case 'L':
                if (0 == strcmp(head, "LowFree:"))
                    valptr = &(info->data.low_free);
                else if (0 == strcmp(head, "LowTotal:"))
                    valptr = &(info->data.low_total);
                break;
            case 'M':
                if (0 == strcmp(head, "MemAvailable:"))
                    valptr = &(info->data.available);
                else if (0 == strcmp(head, "MemFree:"))
                    valptr = &(info->data.free);
                else if (0 == strcmp(head, "MemTotal:"))
                    valptr = &(info->data.total);
                break;
            case 'S':
                if (0 == strcmp(head, "Slab:"))
                    valptr = &(info->data.slab);
                else if (0 == strcmp(head, "SwapFree:"))
                    valptr = &(info->data.swap_free);
                else if (0 == strcmp(head, "SwapTotal:"))
                    valptr = &(info->data.swap_total);
                else if (0 == strcmp(head, "Shmem:"))
                    valptr = &(info->data.shared);
                break;
            default:
                break;
        }
        head = tail+1;
        if (valptr) {
            *valptr = strtoul(head, &tail, 10);
        }
        tail = strchr(head, '\n');
        if (!tail)
            break;
        head = tail + 1;
    } while(tail);

    if (0 == info->data.low_total) {
        info->data.low_total = info->data.total;
        info->data.low_free  = info->data.free;
    }
    if (0 == info->data.available) {
        info->data.available = info->data.free;
    }
    info->data.cached += info->data.slab;
    info->data.swap_used = info->data.swap_total - info->data.swap_free;

    /* if 'available' is greater than 'total' or our calculation of mem_used
       overflows, that's symptomatic of running within a lxc container where
       such values will be dramatically distorted over those of the host. */
    if (info->data.available > info->data.total)
        info->data.available = info->data.free;
    mem_used = info->data.total - info->data.free - info->data.cached - info->data.buffers;
    if (mem_used < 0)
        mem_used = info->data.total - info->data.free;
    info->data.used = (unsigned long)mem_used;

    return 0;
}


PROCPS_EXPORT int procps_meminfo_ref (
        struct procps_meminfo *info)
{
    if (info == NULL)
        return -EINVAL;
    info->refcount++;
    return info->refcount;
}

PROCPS_EXPORT int procps_meminfo_unref (
        struct procps_meminfo **info)
{
    if (info == NULL || *info == NULL)
        return -EINVAL;
    (*info)->refcount--;
    if ((*info)->refcount == 0) {
        free(*info);
        *info = NULL;
        return 0;
    }
    return (*info)->refcount;
}

/* Accessor functions */
PROCPS_EXPORT unsigned long procps_meminfo_get (
        struct procps_meminfo *info,
        enum meminfo_item item)
{
    switch (item) {
        case PROCPS_MEM_ACTIVE:
            return info->data.active;
        case PROCPS_MEM_INACTIVE:
            return info->data.inactive;
        case PROCPS_MEMHI_FREE:
            return info->data.high_free;
        case PROCPS_MEMHI_TOTAL:
            return info->data.high_total;
        case PROCPS_MEMHI_USED:
            if (info->data.high_free > info->data.high_total)
                return 0;
            return info->data.high_total - info->data.high_free;
        case PROCPS_MEMLO_FREE:
            return info->data.low_free;
        case PROCPS_MEMLO_TOTAL:
            return info->data.low_total;
        case PROCPS_MEMLO_USED:
            if (info->data.low_free > info->data.low_total)
                return 0;
            return info->data.low_total - info->data.low_free;
        case PROCPS_MEM_AVAILABLE:
            return info->data.available;
        case PROCPS_MEM_BUFFERS:
            return info->data.buffers;
        case PROCPS_MEM_CACHED:
            return info->data.cached;
        case PROCPS_MEM_FREE:
            return info->data.free;
        case PROCPS_MEM_SHARED:
            return info->data.shared;
        case PROCPS_MEM_TOTAL:
            return info->data.total;
        case PROCPS_MEM_USED:
            return info->data.used;
        case PROCPS_SWAP_FREE:
            return info->data.swap_free;
        case PROCPS_SWAP_TOTAL:
            return info->data.swap_total;
        case PROCPS_SWAP_USED:
            if (info->data.swap_free > info->data.swap_total)
                return 0;
            return info->data.swap_total - info->data.swap_free;
    }
    return 0;
}

PROCPS_EXPORT int procps_meminfo_get_chain (
        struct procps_meminfo *info,
        struct meminfo_result *item)
{

    if (item == NULL)
        return -EINVAL;

    do {
        switch (item->item) {
            case PROCPS_MEM_ACTIVE:
                item->result = info->data.active;
                break;
            case PROCPS_MEM_INACTIVE:
                item->result = info->data.inactive;
                break;
            case PROCPS_MEMHI_FREE:
                item->result = info->data.high_free;
                break;
            case PROCPS_MEMHI_TOTAL:
                item->result = info->data.high_total;
                break;
            case PROCPS_MEMHI_USED:
                if (info->data.high_free > info->data.high_total)
                    item->result = 0;
                else
                    item->result = info->data.high_total - info->data.high_free;
                break;
            case PROCPS_MEMLO_FREE:
                item->result = info->data.low_free;
                break;
            case PROCPS_MEMLO_TOTAL:
                item->result = info->data.low_total;
                break;
            case PROCPS_MEMLO_USED:
                if (info->data.low_free > info->data.low_total)
                    item->result = 0;
                else
                    item->result = info->data.low_total - info->data.low_free;
                break;
            case PROCPS_MEM_AVAILABLE:
                item->result = info->data.available;
                break;
            case PROCPS_MEM_BUFFERS:
                item->result = info->data.buffers;
                break;
            case PROCPS_MEM_CACHED:
                item->result = info->data.cached;
                break;
            case PROCPS_MEM_FREE:
                item->result = info->data.free;
                break;
            case PROCPS_MEM_SHARED:
                item->result = info->data.shared;
                break;
            case PROCPS_MEM_TOTAL:
                item->result = info->data.total;
                break;
            case PROCPS_MEM_USED:
                item->result = info->data.used;
                break;
            case PROCPS_SWAP_FREE:
                item->result = info->data.swap_free;
                break;
            case PROCPS_SWAP_TOTAL:
                item->result = info->data.swap_total;
                break;
            case PROCPS_SWAP_USED:
                if (info->data.swap_free > info->data.swap_total)
                    item->result = 0;
                else
                    item->result = info->data.swap_total - info->data.swap_free;
                break;
            default:
                return -EINVAL;
        }
        item = item->next;
    } while (item);

    return 0;
}
