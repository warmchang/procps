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
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

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
    struct stacks_anchor *stacked;
};

struct stack_vectors {
    struct stacks_anchor *owner;
    struct meminfo_stack **heads;
};

struct stacks_anchor {
    int depth;
    int header_size;
    struct stack_vectors *vectors;
    struct stacks_anchor *self;
    struct stacks_anchor *next;
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
        if ((*info)->stacked) {
            do {
                struct stacks_anchor *p = (*info)->stacked;
                (*info)->stacked = (*info)->stacked->next;
                free(p);
            } while((*info)->stacked);
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
        default:
            return 0;
    }
}

PROCPS_EXPORT int procps_meminfo_getstack (
        struct procps_meminfo *info,
        struct meminfo_result *these)
{
    if (info == NULL || these == NULL)
        return -EINVAL;

    for (;;) {
        switch (these->item) {
            case PROCPS_MEM_ACTIVE:
                these->result.ul_int = info->data.active;
                break;
            case PROCPS_MEM_INACTIVE:
                these->result.ul_int = info->data.inactive;
                break;
            case PROCPS_MEMHI_FREE:
                these->result.ul_int = info->data.high_free;
                break;
            case PROCPS_MEMHI_TOTAL:
                these->result.ul_int = info->data.high_total;
                break;
            case PROCPS_MEMHI_USED:
                if (info->data.high_free > info->data.high_total)
                    these->result.ul_int = 0;
                else
                    these->result.ul_int = info->data.high_total - info->data.high_free;
                break;
            case PROCPS_MEMLO_FREE:
                these->result.ul_int = info->data.low_free;
                break;
            case PROCPS_MEMLO_TOTAL:
                these->result.ul_int = info->data.low_total;
                break;
            case PROCPS_MEMLO_USED:
                if (info->data.low_free > info->data.low_total)
                    these->result.ul_int = 0;
                else
                    these->result.ul_int = info->data.low_total - info->data.low_free;
                break;
            case PROCPS_MEM_AVAILABLE:
                these->result.ul_int = info->data.available;
                break;
            case PROCPS_MEM_BUFFERS:
                these->result.ul_int = info->data.buffers;
                break;
            case PROCPS_MEM_CACHED:
                these->result.ul_int = info->data.cached;
                break;
            case PROCPS_MEM_FREE:
                these->result.ul_int = info->data.free;
                break;
            case PROCPS_MEM_SHARED:
                these->result.ul_int = info->data.shared;
                break;
            case PROCPS_MEM_TOTAL:
                these->result.ul_int = info->data.total;
                break;
            case PROCPS_MEM_USED:
                these->result.ul_int = info->data.used;
                break;
            case PROCPS_SWAP_FREE:
                these->result.ul_int = info->data.swap_free;
                break;
            case PROCPS_SWAP_TOTAL:
                these->result.ul_int = info->data.swap_total;
                break;
            case PROCPS_SWAP_USED:
                if (info->data.swap_free > info->data.swap_total)
                    these->result.ul_int = 0;
                else
                    these->result.ul_int = info->data.swap_total - info->data.swap_free;
                break;
            case PROCPS_MEM_noop:
                // don't disturb potential user data in the result struct
                break;
            case PROCPS_MEM_stack_end:
                return 0;
            default:
                return -EINVAL;
        }
        ++these;
    }
}

PROCPS_EXPORT int procps_meminfo_stack_fill (
        struct procps_meminfo *info,
        struct meminfo_stack *stack)
{
    int rc;

    if (info == NULL || stack == NULL || stack->head == NULL)
        return -EINVAL;
    if ((rc == procps_meminfo_read(info)) < 0)
        return rc;

    return procps_meminfo_getstack(info, stack->head);
}

static void stacks_validate (struct meminfo_stack **v, const char *who)
{
#if 0
    #include <stdio.h>
    int i, t, x, n = 0;
    struct stack_vectors *p = (struct stack_vectors *)v - 1;

    fprintf(stderr, "%s: called by '%s'\n", __func__, who);
    fprintf(stderr, "%s: owned by %p (whose self = %p)\n", __func__, p->owner, p->owner->self);
    for (x = 0; v[x]; x++) {
        struct meminfo_stack *h = v[x];
        struct meminfo_result *r = h->head;
        fprintf(stderr, "%s:   vector[%02d] = %p", __func__, x, h);
        i = 0;
        for (i = 0; r->item < PROCPS_MEM_stack_end; i++, r++)
            ;
        t = i + 1;
        fprintf(stderr, ", stack %d found %d elements\n", n, i);
        ++n;
    }
    fprintf(stderr, "%s: found %d stack(s), each %d bytes (including eos)\n", __func__, x, (int)sizeof(struct meminfo_result) * t);
    fprintf(stderr, "%s: this header size = %2d\n", __func__, (int)p->owner->header_size);
    fprintf(stderr, "%s: sizeof(struct meminfo_stack)  = %2d\n", __func__, (int)sizeof(struct meminfo_stack));
    fprintf(stderr, "%s: sizeof(struct meminfo_result) = %2d\n", __func__, (int)sizeof(struct meminfo_result));
    fputc('\n', stderr);
    return;
#endif
}

static struct meminfo_result *stack_make (
        struct meminfo_result *p,
        int maxitems,
        enum meminfo_item *items)
{
    struct meminfo_result *p_sav = p;
    int i;

    for (i = 0; i < maxitems; i++) {
        p->item = items[i];
        // note: we rely on calloc to initialize actual result
        ++p;
    }

    return p_sav;
}

static int stack_items_valid (
        int maxitems,
        enum meminfo_item *items)
{
    int i;

    for (i = 0; i < maxitems; i++) {
        if (items[i] < PROCPS_MEMHI_FREE)
            return 0;
        if (items[i] > PROCPS_MEM_stack_end)
            return 0;
    }
    if (items[maxitems -1] != PROCPS_MEM_stack_end)
        return 0;
    return 1;
}


/*
 * procps_meminfo_stacks_alloc():
 *
 * A local copy of code borrowed from slab.c to support the public version
 * representing a single stack.  Currently there is no conceivable need
 * for multiple stacks in the 'memory' arena.
 */
static struct meminfo_stack **procps_meminfo_stacks_alloc (
        struct procps_meminfo *info,
        int maxstacks,
        int stack_extra,
        int maxitems,
        enum meminfo_item *items)
{
    struct stacks_anchor *p_blob;
    struct stack_vectors *p_vect;
    struct meminfo_stack *p_head;
    size_t vect_size, head_size, list_size, blob_size;
    void *v_head, *v_list;
    int i;

    if (info == NULL || items == NULL)
        return NULL;
    if (maxstacks < 1 || maxitems < 1)
        return NULL;
    if (!stack_items_valid(maxitems, items))
        return NULL;

    vect_size  = sizeof(struct stack_vectors);                 // address vector struct
    vect_size += sizeof(void *) * maxstacks;                   // plus vectors themselves
    vect_size += sizeof(void *);                               // plus NULL delimiter
    head_size  = sizeof(struct meminfo_stack) + stack_extra;   // a head struct + user stuff
    list_size  = sizeof(struct meminfo_result) * maxitems;     // a results stack
    blob_size  = sizeof(struct stacks_anchor);                 // the anchor itself
    blob_size += vect_size;                                    // all vectors + delims
    blob_size += head_size * maxstacks;                        // all head structs + user stuff
    blob_size += list_size * maxstacks;                        // all results stacks

    /* note: all memory is allocated in a single blob, facilitating a later free().
       as a minimum, it's important that the result structures themselves always be
       contiguous for any given stack (just as they are when defined statically). */
    if (NULL == (p_blob = calloc(1, blob_size)))
        return NULL;

    p_blob->next = info->stacked;
    info->stacked = p_blob;
    p_blob->self  = p_blob;
    p_blob->header_size = head_size;
    p_blob->vectors = (void *)p_blob + sizeof(struct stacks_anchor);
    p_vect = p_blob->vectors;
    p_vect->owner = p_blob->self;
    p_vect->heads = (void *)p_vect + sizeof(struct stack_vectors);
    v_head = (void *)p_vect + vect_size;
    v_list = v_head + (head_size * maxstacks);

    for (i = 0; i < maxstacks; i++) {
        p_head = (struct meminfo_stack *)v_head;
        p_head->head = stack_make((struct meminfo_result *)v_list, maxitems, items);
        p_blob->vectors->heads[i] = p_head;
        v_list += list_size;
        v_head += head_size;
    }
    p_blob->depth = maxstacks;
    stacks_validate(p_blob->vectors->heads, __func__);
    return p_blob->vectors->heads;
}

/*
 * procps_meminfo_stack_alloc():
 *
 * Allocate and initialize a single result stack under a simplified interface.
 *
 * Such a stack will will have its result structures properly primed with
 * 'items', while the result itself will be zeroed.
 *
 */
PROCPS_EXPORT struct meminfo_stack *procps_meminfo_stack_alloc (
        struct procps_meminfo *info,
        int maxitems,
        enum meminfo_item *items)
{
    struct meminfo_stack **v;

    v = procps_meminfo_stacks_alloc(info, 1, 0, maxitems, items);
    if (!v)
        return NULL;
    stacks_validate(v, __func__);
    return v[0];
}
