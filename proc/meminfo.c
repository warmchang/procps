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
    unsigned long main_available;
    unsigned long main_buffers;
    unsigned long main_cached;
    unsigned long main_free;
    unsigned long main_shared;
    unsigned long main_total;
    unsigned long main_used;
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
PROCPS_EXPORT int procps_meminfo_new(struct procps_meminfo **info)
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
PROCPS_EXPORT int procps_meminfo_read(struct procps_meminfo *info)
{
    char buf[8192];
    char *head, *tail;
    int size;
    unsigned long *valptr;

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
    if ((size = read(info->meminfo_fd, buf, sizeof(buf)-1)) < 0) {
	return -1;
    }
    buf[size] = '\0';

    /* Scan the file */
    head = buf;
    do {
	tail = strchr(head, ' ');
	if (!tail)
	    break;
	*tail = '\0';
	valptr = NULL;
	if (0 == strcmp(head, "Active:")) {
	    valptr = &(info->data.active);
	} else if (0 == strcmp(head, "Inactive:")) {
	    valptr = &(info->data.inactive);
	} else if (0 == strcmp(head, "HighFree:")) {
	    valptr = &(info->data.high_free);
	} else if (0 == strcmp(head, "HighTotal:")) {
	    valptr = &(info->data.high_total);
	} else if (0 == strcmp(head, "LowFree:")) {
	    valptr = &(info->data.low_free);
	} else if (0 == strcmp(head, "LowTotal:")) {
	    valptr = &(info->data.low_total);
	} else if (0 == strcmp(head, "MemAvailable:")) {
	    valptr = &(info->data.main_available);
	} else if (0 == strcmp(head, "Buffers:")) {
	    valptr = &(info->data.main_buffers);
	} else if (0 == strcmp(head, "Cached:")) {
	    valptr = &(info->data.main_cached);
	} else if (0 == strcmp(head, "MemFree:")) {
	    valptr = &(info->data.main_free);
	} else if (0 == strcmp(head, "Shmem:")) {
	    valptr = &(info->data.main_shared);
	} else if (0 == strcmp(head, "MemTotal:")) {
	    valptr = &(info->data.main_total);
	} else if (0 == strcmp(head, "SwapFree:")) {
	    valptr = &(info->data.swap_free);
	} else if (0 == strcmp(head, "SwapTotal:")) {
	    valptr = &(info->data.swap_total);
	}
	head - tail+1;
	if (valptr) {
	    *valptr = strtoul(head, &tail, 10);
	}

	tail = strchr(head, '\n');
	if (!tail)
	    break;
	head = tail + 1;
    } while(tail);
    return 0;
}


PROCPS_EXPORT struct procps_meminfo *procps_meminfo_ref(struct procps_meminfo *info)
{
    if (info == NULL)
	return NULL;
    info->refcount++;
    return info;
}

PROCPS_EXPORT struct procps_meminfo *procps_meminfo_unref(struct procps_meminfo *info)
{
    if (info == NULL)
	return NULL;
    info->refcount--;
    if (info->refcount > 0)
	return NULL;
    free(info);
    return NULL;
}

/* Accessor functions */
PROCPS_EXPORT unsigned long procps_meminfo_get(
	struct procps_meminfo *info,
	enum meminfo_item item)
{
    switch(item) {
	case PROCPS_MEMINFO_ACTIVE:
	    return info->data.active;
	case PROCPS_MEMINFO_SWAP_USED:
	    if (info->data.swap_free > info->data.swap_total)
		return 0;
	    return info->data.swap_total - info->data.swap_free;
    }
    return 0;
}
