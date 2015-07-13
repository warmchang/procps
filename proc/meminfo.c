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
    struct chains_anchor *chained;
};

struct chain_vectors {
    struct chains_anchor *owner;
    struct meminfo_chain **heads;
};

struct chains_anchor {
    int depth;
    int header_size;
    struct chain_vectors *vectors;
    struct chains_anchor *self;
    struct chains_anchor *next;
};


/*
 * procps_meminfo_new():
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
 * procps_meminfo_read():
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
        if ((*info)->chained) {
            do {
                struct chains_anchor *p = (*info)->chained;
                (*info)->chained = (*info)->chained->next;
                free(p);
            } while((*info)->chained);
        }
        free(*info);
        *info = NULL;
        return 0;
    }
    return (*info)->refcount;
}

/*
 * Accessor functions
 */
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
        case PROCPS_MEM_noop:
            return 0;
    }
    return 0;
}

PROCPS_EXPORT int procps_meminfo_getchain (
        struct procps_meminfo *info,
        struct meminfo_result *these)
{
    if (info == NULL || these == NULL)
        return -EINVAL;

    do {
        switch (these->item) {
            case PROCPS_MEM_ACTIVE:
                these->result = info->data.active;
                break;
            case PROCPS_MEM_INACTIVE:
                these->result = info->data.inactive;
                break;
            case PROCPS_MEMHI_FREE:
                these->result = info->data.high_free;
                break;
            case PROCPS_MEMHI_TOTAL:
                these->result = info->data.high_total;
                break;
            case PROCPS_MEMHI_USED:
                if (info->data.high_free > info->data.high_total)
                    these->result = 0;
                else
                    these->result = info->data.high_total - info->data.high_free;
                break;
            case PROCPS_MEMLO_FREE:
                these->result = info->data.low_free;
                break;
            case PROCPS_MEMLO_TOTAL:
                these->result = info->data.low_total;
                break;
            case PROCPS_MEMLO_USED:
                if (info->data.low_free > info->data.low_total)
                    these->result = 0;
                else
                    these->result = info->data.low_total - info->data.low_free;
                break;
            case PROCPS_MEM_AVAILABLE:
                these->result = info->data.available;
                break;
            case PROCPS_MEM_BUFFERS:
                these->result = info->data.buffers;
                break;
            case PROCPS_MEM_CACHED:
                these->result = info->data.cached;
                break;
            case PROCPS_MEM_FREE:
                these->result = info->data.free;
                break;
            case PROCPS_MEM_SHARED:
                these->result = info->data.shared;
                break;
            case PROCPS_MEM_TOTAL:
                these->result = info->data.total;
                break;
            case PROCPS_MEM_USED:
                these->result = info->data.used;
                break;
            case PROCPS_SWAP_FREE:
                these->result = info->data.swap_free;
                break;
            case PROCPS_SWAP_TOTAL:
                these->result = info->data.swap_total;
                break;
            case PROCPS_SWAP_USED:
                if (info->data.swap_free > info->data.swap_total)
                    these->result = 0;
                else
                    these->result = info->data.swap_total - info->data.swap_free;
                break;
            case PROCPS_MEM_noop:
                break;
            default:
                return -EINVAL;
        }
        these = these->next;
    } while (these);

    return 0;
}

PROCPS_EXPORT int procps_meminfo_chain_fill (
        struct procps_meminfo *info,
        struct meminfo_chain *chain)
{
    int rc;

    if (info == NULL || chain == NULL || chain->head == NULL)
        return -EINVAL;
    if ((rc == procps_meminfo_read(info)) < 0)
        return rc;

    return procps_meminfo_getchain(info, chain->head);
}

static void chains_validate (struct meminfo_chain **v, const char *who)
{
#if 0
    #include <stdio.h>
    int i, x, n = 0;
    struct chain_vectors *p = (struct chain_vectors *)v - 1;

    fprintf(stderr, "%s: called by '%s'\n", __func__, who);
    fprintf(stderr, "%s: owned by %p (whose self = %p)\n", __func__, p->owner, p->owner->self);
    for (x = 0; v[x]; x++) {
        struct meminfo_chain *h = v[x];
        struct meminfo_result *r = h->head;
        fprintf(stderr, "%s:   vector[%02d] = %p", __func__, x, h);
        i = 0;
        do {
            i++;
            r = r->next;
        } while (r);
        fprintf(stderr, ", chain %d found %d elements\n", n, i);
        ++n;
    }
    fprintf(stderr, "%s: found %d chain(s)\n", __func__, x);
    fprintf(stderr, "%s: this header size = %2d\n", __func__, (int)p->owner->header_size);
    fprintf(stderr, "%s: sizeof(struct meminfo_chain)  = %2d\n", __func__, (int)sizeof(struct meminfo_chain));
    fprintf(stderr, "%s: sizeof(struct meminfo_result) = %2d\n", __func__, (int)sizeof(struct meminfo_result));
    fputc('\n', stderr);
    return;
#endif
}

static struct meminfo_result *chain_make (
        struct meminfo_result *p,
        int maxitems,
        enum meminfo_item *items)
{
    struct meminfo_result *p_sav = p;
    int i;

    for (i = 0; i < maxitems; i++) {
        if (i > PROCPS_MEM_noop)
            p->item = PROCPS_MEM_noop;
        else
            p->item = items[i];
        p->result = 0;
        p->next = p + 1;
        ++p;
    }
    (--p)->next = NULL;

    return p_sav;
}

/*
 * procps_meminfo_chains_alloc():
 *
 * A local copy of code borrowed from slab.c to support the public version
 * representing a single chain.  Currently there is no conceivable need
 * for multiple chains in the 'memory' arena.
 */
static struct meminfo_chain **procps_meminfo_chains_alloc (
        struct procps_meminfo *info,
        int maxchains,
        int chain_extra,
        int maxitems,
        enum meminfo_item *items)
{
    struct chains_anchor *p_blob;
    struct chain_vectors *p_vect;
    struct meminfo_chain *p_head;
    size_t vect_size, head_size, list_size, blob_size;
    void *v_head, *v_list;
    int i;

    if (info == NULL || items == NULL)
        return NULL;
    if (maxchains < 1 || maxitems < 1)
        return NULL;

    vect_size  = sizeof(struct chain_vectors);                 // address vector struct
    vect_size += sizeof(void *) * maxchains;                   // plus vectors themselves
    vect_size += sizeof(void *);                               // plus NULL delimiter
    head_size  = sizeof(struct meminfo_chain) + chain_extra;   // a head struct + user stuff
    list_size  = sizeof(struct meminfo_result) * maxitems;     // a results chain
    blob_size  = sizeof(struct chains_anchor);                 // the anchor itself
    blob_size += vect_size;                                    // all vectors + delims
    blob_size += head_size * maxchains;                        // all head structs + user stuff
    blob_size += list_size * maxchains;                        // all results chains

    /* note: all memory is allocated in a single blob, facilitating a later free().
       as a minimum, it's important that the result structures themselves always be
       contiguous for any given chain (just as they are when defined statically). */
    if (NULL == (p_blob = calloc(1, blob_size)))
        return NULL;

    p_blob->next = info->chained;
    info->chained = p_blob;
    p_blob->self  = p_blob;
    p_blob->header_size = head_size;
    p_blob->vectors = (void *)p_blob + sizeof(struct chains_anchor);
    p_vect = p_blob->vectors;
    p_vect->owner = p_blob->self;
    p_vect->heads = (void *)p_vect + sizeof(struct chain_vectors);
    v_head = (void *)p_vect + vect_size;
    v_list = v_head + (head_size * maxchains);

    for (i = 0; i < maxchains; i++) {
        p_head = (struct meminfo_chain *)v_head;
        p_head->head = chain_make((struct meminfo_result *)v_list, maxitems, items);
        p_blob->vectors->heads[i] = p_head;
        v_list += list_size;
        v_head += head_size;
    }
    p_blob->depth = maxchains;
    chains_validate(p_blob->vectors->heads, __func__);
    return p_blob->vectors->heads;
}

/*
 * procps_meminfo_chain_alloc():
 *
 * Allocate and initialize a single result chain under a simplified interface.
 *
 * Such a chain will will have its result structures properly primed with
 * 'items' and 'next' pointers, while the result itself will be zeroed.
 *
 */
PROCPS_EXPORT struct meminfo_chain *procps_meminfo_chain_alloc (
        struct procps_meminfo *info,
        int maxitems,
        enum meminfo_item *items)
{
    struct meminfo_chain **v;

    if (info == NULL || items == NULL || maxitems < 1)
        return NULL;
    v = procps_meminfo_chains_alloc(info, 1, 0, maxitems, items);
    if (!v)
        return NULL;
    chains_validate(v, __func__);
    return v[0];
}
