/*
 * slab.c - slab related functions for libproc
 *
 * Chris Rivera <cmrivera@ufl.edu>
 * Robert Love <rml@tech9.net>
 *
 * Copyright (C) 2003 Chris Rivera
 * Copyright 2004, Albert Cahalan
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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <proc/slab.h>
#include "procps-private.h"

#define SLABINFO_FILE        "/proc/slabinfo"
#define SLABINFO_LINE_LEN    2048
#define SLAB_INFO_NAME_LEN   128

struct slabinfo_stats {
    unsigned long total_size;       /* size of all objects */
    unsigned long active_size;      /* size of all active objects */
    unsigned int  nr_objs;          /* number of objects, among all caches */
    unsigned int  nr_active_objs;   /* number of active objects, among all caches */
    unsigned int  nr_pages;         /* number of pages consumed by all objects */
    unsigned int  nr_slabs;         /* number of slabs, among all caches */
    unsigned int  nr_active_slabs;  /* number of active slabs, among all caches */
    unsigned int  nr_caches;        /* number of caches */
    unsigned int  nr_active_caches; /* number of active caches */
    unsigned int  avg_obj_size;     /* average object size */
    unsigned int  min_obj_size;     /* size of smallest object */
    unsigned int  max_obj_size;     /* size of largest object */
};

struct slabinfo_node {
    char name[SLAB_INFO_NAME_LEN];  /* name of this cache */
    unsigned long cache_size;       /* size of entire cache */
    unsigned int  nr_objs;          /* number of objects in this cache */
    unsigned int  nr_active_objs;   /* number of active objects */
    unsigned int  obj_size;         /* size of each object */
    unsigned int  objs_per_slab;    /* number of objects per slab */
    unsigned int  pages_per_slab;   /* number of pages per slab */
    unsigned int  nr_slabs;         /* number of slabs in this cache */
    unsigned int  nr_active_slabs;  /* number of active slabs */
    unsigned int  use;              /* percent full: total / active */
};

struct procps_slabinfo {
    int refcount;
    FILE *slabinfo_fp;
    struct slabinfo_stats stats;
    struct slabinfo_node *nodes;    /* first slabnode of this list */
    int nodes_alloc;                /* nodes alloc()ed */
    int nodes_used;                 /* nodes using alloced memory */
    struct stacks_anchor *stacked;
};

struct stack_vectors {
    struct stacks_anchor *owner;
    struct slabnode_stack **heads;
};

struct stacks_anchor {
    int depth;
    int inuse;
    struct stack_vectors *vectors;
    struct stacks_anchor *self;
    struct stacks_anchor *next;
};


/*
 * Zero out the slabnode data, keeping the memory allocated.
 */
static void slabnodes_clear (
        struct procps_slabinfo *info)
{
    if (info == NULL || info->nodes == NULL || info->nodes_alloc < 1)
        return;
    memset(info->nodes, 0, sizeof(struct slabinfo_node)*info->nodes_alloc);
    info->nodes_used = 0;
}

/* Alloc up more slabnode memory, if required
 */
static int slabnodes_alloc (
        struct procps_slabinfo *info)
{
    struct slabinfo_node *new_nodes;
    int new_count;

    if (info == NULL)
        return -EINVAL;

    if (info->nodes_used < info->nodes_alloc)
        return 0;
    /* Increment the allocated number of slabs */
    new_count = info->nodes_alloc * 5/4+30;

    new_nodes = realloc(info->nodes, sizeof(struct slabinfo_node) * new_count);
    if (!new_nodes)
        return -ENOMEM;
    info->nodes = new_nodes;
    info->nodes_alloc = new_count;
    return 0;
}

/*
 * get_slabnode - allocate slab_info structures using a free list
 *
 * In the fast path, we simply return a node off the free list.  In the slow
 * list, we malloc() a new node.  The free list is never automatically reaped,
 * both for simplicity and because the number of slab caches is fairly
 * constant.
 */
static int get_slabnode (
        struct procps_slabinfo *info,
        struct slabinfo_node **node)
{
    int retval;

    if (!info)
        return -EINVAL;

    if (info->nodes_used == info->nodes_alloc) {
        if ((retval = slabnodes_alloc(info)) < 0)
            return retval;
    }
    *node = &(info->nodes[info->nodes_used++]);
    return 0;
}

/* parse_slabinfo20:
 *
 * sactual parse routine for slabinfo 2.x (2.6 kernels)
 * Note: difference between 2.0 and 2.1 is in the ": globalstat" part where version 2.1
 * has extra column <nodeallocs>. We don't use ": globalstat" part in both versions.
 *
 * Formats (we don't use "statistics" extensions)
 *
 *  slabinfo - version: 2.1
 *  # name            <active_objs> <num_objs> <objsize> <objperslab> <pagesperslab> \
 *  : tunables <batchcount> <limit> <sharedfactor> \
 *  : slabdata <active_slabs> <num_slabs> <sharedavail>
 *
 *  slabinfo - version: 2.1 (statistics)
 *  # name            <active_objs> <num_objs> <objsize> <objperslab> <pagesperslab> \
 *  : tunables <batchcount> <limit> <sharedfactor> \
 *  : slabdata <active_slabs> <num_slabs> <sharedavail> \
 *  : globalstat <listallocs> <maxobjs> <grown> <reaped> <error> <maxfreeable> <freelimit> <nodeallocs> \
 *  : cpustat <allochit> <allocmiss> <freehit> <freemiss>
 *
 *  slabinfo - version: 2.0
 *  # name            <active_objs> <num_objs> <objsize> <objperslab> <pagesperslab> \
 *  : tunables <batchcount> <limit> <sharedfactor> \
 *  : slabdata <active_slabs> <num_slabs> <sharedavail>
 *
 *  slabinfo - version: 2.0 (statistics)
 *  # name            <active_objs> <num_objs> <objsize> <objperslab> <pagesperslab> \
 *  : tunables <batchcount> <limit> <sharedfactor> \
 *  : slabdata <active_slabs> <num_slabs> <sharedavail> \
 *  : globalstat <listallocs> <maxobjs> <grown> <reaped> <error> <maxfreeable> <freelimit> \
 *  : cpustat <allochit> <allocmiss> <freehit> <freemiss>
 */
static int parse_slabinfo20 (
        struct procps_slabinfo *info)
{
    struct slabinfo_node *node;
    char buffer[SLABINFO_LINE_LEN];
    int retval;
    int page_size = getpagesize();
    struct slabinfo_stats *stats = &(info->stats);

    stats->min_obj_size = INT_MAX;
    stats->max_obj_size = 0;

    while (fgets(buffer, SLABINFO_LINE_LEN, info->slabinfo_fp )) {
        if (buffer[0] == '#')
            continue;

        if ((retval = get_slabnode(info, &node)) < 0)
            return retval;

        if (sscanf(buffer,
                   "%" STRINGIFY(SLAB_INFO_NAME_LEN) "s" \
                   "%u %u %u %u %u : tunables %*u %*u %*u : slabdata %u %u %*u",
                   node->name,
                   &node->nr_active_objs, &node->nr_objs,
                   &node->obj_size, &node->objs_per_slab,
                   &node->pages_per_slab, &node->nr_active_slabs,
                   &node->nr_slabs) < 8) {
            if (errno != 0)
                return -errno;
            return -EINVAL;
        }

        if (!node->name[0])
            snprintf(node->name, sizeof(node->name), "%s", "unknown");

        if (node->obj_size < stats->min_obj_size)
            stats->min_obj_size = node->obj_size;
        if (node->obj_size > stats->max_obj_size)
            stats->max_obj_size = node->obj_size;

        node->cache_size = (unsigned long)node->nr_slabs * node->pages_per_slab
            * page_size;

        if (node->nr_objs) {
            node->use = (unsigned int)100 * node->nr_active_objs / node->nr_objs;
            stats->nr_active_caches++;
        } else
            node->use = 0;

        stats->nr_objs += node->nr_objs;
        stats->nr_active_objs += node->nr_active_objs;
        stats->total_size += (unsigned long)node->nr_objs * node->obj_size;
        stats->active_size += (unsigned long)node->nr_active_objs * node->obj_size;
        stats->nr_pages += node->nr_slabs * node->pages_per_slab;
        stats->nr_slabs += node->nr_slabs;
        stats->nr_active_slabs += node->nr_active_slabs;
        stats->nr_caches++;
    }

    if (stats->nr_objs)
        stats->avg_obj_size = stats->total_size / stats->nr_objs;

    return 0;
}

/*
 * procps_slabinfo_new():
 *
 * @info: location of returned new structure
 *
 * Returns: 0 on success <0 on failure
 */
PROCPS_EXPORT int procps_slabinfo_new (
        struct procps_slabinfo **info)
{
    struct procps_slabinfo *si;

    if (info == NULL)
        return -EINVAL;

    si = calloc(1, sizeof(struct procps_slabinfo));
    if (!si)
        return -ENOMEM;

    si->refcount = 1;
    si->slabinfo_fp = NULL;
    si->nodes_alloc = 0;
    si->nodes_used = 0;
    si->nodes = NULL;
    *info = si;
    return 0;
}

/* procps_slabinfo_read():
 *
 * Read the data out of /proc/slabinfo putting the information
 * into the supplied info container
 *
 * Returns: 0 on success, negative on error
 */
PROCPS_EXPORT int procps_slabinfo_read (
        struct procps_slabinfo *info)
{
    char line[SLABINFO_LINE_LEN];
    int retval, major, minor;

    if (info == NULL)
        return -1;

    memset(&(info->stats), 0, sizeof(struct slabinfo_stats));
    if ((retval = slabnodes_alloc(info)) < 0)
        return retval;

    slabnodes_clear(info);

    if (NULL == info->slabinfo_fp &&
        (info->slabinfo_fp = fopen(SLABINFO_FILE, "r")) == NULL)
        return -errno;

    if (fseek(info->slabinfo_fp, 0L, SEEK_SET) < 0)
        return -errno;

    /* Parse the version string */
    if (!fgets(line, SLABINFO_LINE_LEN, info->slabinfo_fp))
        return -errno;

    if (sscanf(line, "slabinfo - version: %d.%d", &major, &minor) != 2)
        return -EINVAL;

    if (major == 2)
        retval = parse_slabinfo20(info);
    else
        return -ERANGE;

    return retval;
}

PROCPS_EXPORT int procps_slabinfo_ref (
        struct procps_slabinfo *info)
{
    if (info == NULL)
        return -EINVAL;

    info->refcount++;
    return info->refcount;
}

PROCPS_EXPORT int procps_slabinfo_unref (
        struct procps_slabinfo **info)
{
    if (info == NULL || *info == NULL)
        return -EINVAL;

    (*info)->refcount--;
    if ((*info)->refcount == 0) {
        if ((*info)->slabinfo_fp) {
            fclose((*info)->slabinfo_fp);
            (*info)->slabinfo_fp = NULL;
        }
        if ((*info)->stacked) {
            do {
                struct stacks_anchor *p = (*info)->stacked;
                (*info)->stacked = (*info)->stacked->next;
                free(p);
            } while((*info)->stacked);
        }
        free((*info)->nodes);
        free(*info);
        *info = NULL;
        return 0;
    }
    return (*info)->refcount;
}

PROCPS_EXPORT unsigned long procps_slabs_get (
        struct procps_slabinfo *info,
        enum slabs_item item)
{
    /* note: most of the results we might return are actually just
       unsigned int, but we must accommodate the largest potential
       result and so return an unsigned long */
    if (info == NULL)
        return -EINVAL;

    switch (item) {
        case PROCPS_SLABS_OBJS:
            return info->stats.nr_objs;
        case PROCPS_SLABS_AOBJS:
            return info->stats.nr_active_objs;
        case PROCPS_SLABS_PAGES:
            return info->stats.nr_pages;
        case PROCPS_SLABS_SLABS:
            return info->stats.nr_slabs;
        case PROCPS_SLABS_ASLABS:
            return info->stats.nr_active_slabs;
        case PROCPS_SLABS_CACHES:
            return info->stats.nr_caches;
        case PROCPS_SLABS_ACACHES:
            return info->stats.nr_active_caches;
        case PROCPS_SLABS_SIZE_AVG:
            return info->stats.avg_obj_size;
        case PROCPS_SLABS_SIZE_MIN:
            return info->stats.min_obj_size;
        case PROCPS_SLABS_SIZE_MAX:
            return info->stats.max_obj_size;
        case PROCPS_SLABS_SIZE_TOTAL:
            return info->stats.total_size;
        case PROCPS_SLABS_SIZE_ACTIVE:
            return info->stats.active_size;
        default:
            return 0;
    }
}

PROCPS_EXPORT int procps_slabs_getstack (
        struct procps_slabinfo *info,
        struct slab_result *these)
{
    if (info == NULL || these == NULL)
        return -EINVAL;

    for (;;) {
        switch (these->item) {
            case PROCPS_SLABS_OBJS:
                these->result.u_int = info->stats.nr_objs;
                break;
            case PROCPS_SLABS_AOBJS:
                these->result.u_int = info->stats.nr_active_objs;
                break;
            case PROCPS_SLABS_PAGES:
                these->result.u_int = info->stats.nr_pages;
                break;
            case PROCPS_SLABS_SLABS:
                these->result.u_int = info->stats.nr_slabs;
                break;
            case PROCPS_SLABS_ASLABS:
                these->result.u_int = info->stats.nr_active_slabs;
                break;
            case PROCPS_SLABS_CACHES:
                these->result.u_int = info->stats.nr_caches;
                break;
            case PROCPS_SLABS_ACACHES:
                these->result.u_int = info->stats.nr_active_caches;
                break;
            case PROCPS_SLABS_SIZE_AVG:
                these->result.u_int = info->stats.avg_obj_size;
                break;
            case PROCPS_SLABS_SIZE_MIN:
                these->result.u_int = info->stats.min_obj_size;
                break;
            case PROCPS_SLABS_SIZE_MAX:
                these->result.u_int = info->stats.max_obj_size;
                break;
            case PROCPS_SLABS_SIZE_TOTAL:
                these->result.ul_int = info->stats.total_size;
                break;
            case PROCPS_SLABS_SIZE_ACTIVE:
                these->result.ul_int = info->stats.active_size;
                break;
            case PROCPS_SLABS_noop:
                // don't disturb potential user data in the result struct
                break;
            case PROCPS_SLABS_stack_end:
                return 0;
            default:
                return -EINVAL;
        }
        ++these;
    }
}

/*
 * procps_slabnode_getname():
 *
 * @info: slabinfo structure with data read in
 * @nodeid: number of node we want the name for
 *
 * Find the name of the given node
 *
 * Returns: name or NULL on error
 */
PROCPS_EXPORT const char *procps_slabnode_getname (
        struct procps_slabinfo *info,
        int nodeid)
{
    if (info == NULL)
        return NULL;
    if (nodeid > info->nodes_used)
        return NULL;
    return info->nodes[nodeid].name;
}

PROCPS_EXPORT unsigned long procps_slabnode_get (
        struct procps_slabinfo *info,
        enum slabnode_item item,
        int nodeid)
{
    /* note: most of the results we might return are actually just
       unsigned int, but we must accommodate the largest potential
       result and so return an unsigned long */
    if (info == NULL)
        return -EINVAL;

    switch (item) {
        case PROCPS_SLABNODE_SIZE:
            return info->nodes[nodeid].cache_size;
        case PROCPS_SLABNODE_OBJS:
            return info->nodes[nodeid].nr_objs;
        case PROCPS_SLABNODE_AOBJS:
            return info->nodes[nodeid].nr_active_objs;
        case PROCPS_SLABNODE_OBJ_SIZE:
            return info->nodes[nodeid].obj_size;
        case PROCPS_SLABNODE_OBJS_PER_SLAB:
            return info->nodes[nodeid].objs_per_slab;
        case PROCPS_SLABNODE_PAGES_PER_SLAB:
            return info->nodes[nodeid].pages_per_slab;
        case PROCPS_SLABNODE_SLABS:
            return info->nodes[nodeid].nr_slabs;
        case PROCPS_SLABNODE_ASLABS:
            return info->nodes[nodeid].nr_active_slabs;
        case PROCPS_SLABNODE_USE:
            return info->nodes[nodeid].use;
        default:
            return 0;
    }
}

PROCPS_EXPORT int procps_slabnode_getstack (
        struct procps_slabinfo *info,
        struct slab_result *these,
        int nodeid)
{
    if (info == NULL || these == NULL)
        return -EINVAL;
    if (nodeid > info->nodes_used)
        return -EINVAL;

    for (;;) {
        switch (these->item) {
            case PROCPS_SLABNODE_SIZE:
                these->result.ul_int = info->nodes[nodeid].cache_size;
                break;
            case PROCPS_SLABNODE_OBJS:
                these->result.u_int = info->nodes[nodeid].nr_objs;
                break;
            case PROCPS_SLABNODE_AOBJS:
                these->result.u_int = info->nodes[nodeid].nr_active_objs;
                break;
            case PROCPS_SLABNODE_OBJ_SIZE:
                these->result.u_int = info->nodes[nodeid].obj_size;
                break;
            case PROCPS_SLABNODE_OBJS_PER_SLAB:
                these->result.u_int = info->nodes[nodeid].objs_per_slab;
                break;
            case PROCPS_SLABNODE_PAGES_PER_SLAB:
                these->result.u_int = info->nodes[nodeid].pages_per_slab;
                break;
            case PROCPS_SLABNODE_SLABS:
                these->result.u_int = info->nodes[nodeid].nr_slabs;
                break;
            case PROCPS_SLABNODE_ASLABS:
                these->result.u_int = info->nodes[nodeid].nr_active_slabs;
                break;
            case PROCPS_SLABNODE_USE:
                these->result.u_int = info->nodes[nodeid].use;
                break;
            case PROCPS_SLABNODE_NAME:
                these->result.str = info->nodes[nodeid].name;
                break;
            case PROCPS_SLABNODE_noop:
                // don't disturb potential user data in the result struct
                break;
            case PROCPS_SLABNODE_stack_end:
                return 0;
            default:
                return -EINVAL;
        }
        ++these;
    }
}


PROCPS_EXPORT int procps_slabnode_stack_fill (
    struct procps_slabinfo *info,
    struct slabnode_stack *stack,
    int nodeid)
{
    int rc;

    if (info == NULL || stack == NULL || stack->head == NULL)
        return -EINVAL;

    if ((rc = procps_slabinfo_read(info)) < 0)
        return rc;

    return procps_slabnode_getstack(info, stack->head, nodeid);
}

/*
 * procps_slabnode_count():
 *
 * @info: read in slabinfo structure
 *
 * Returns: number of nodes in @info or <0 on error
 */
PROCPS_EXPORT int procps_slabnode_count (
        struct procps_slabinfo *info)
{
    int rc = 0;

    if (!info)
        return -EINVAL;
    if (!info->nodes_used)
        rc = procps_slabinfo_read(info);
    if (rc < 0)
        return rc;
    return info->nodes_used;
}

PROCPS_EXPORT int procps_slabnode_stacks_fill (
    struct procps_slabinfo *info,
    struct slabnode_stack **stacks,
    int maxstacks)
{
    int i, rc;

    if (info == NULL || *stacks == NULL)
        return -EINVAL;
    if (maxstacks < 1)
        return -EINVAL;

    if ((rc = procps_slabinfo_read(info)) < 0)
        return rc;

    if (maxstacks > info->stacked->depth)
        maxstacks = info->stacked->depth;
    if (maxstacks > info->nodes_used)
        maxstacks = info->nodes_used;

    for (i = 0; i < maxstacks; i++) {
        if (stacks[i] == NULL)
            break;
        if ((rc = procps_slabnode_getstack(info, stacks[i]->head, i) < 0))
            return rc;
    }

    info->stacked->inuse = i;
    return info->stacked->inuse;
}

static void stacks_validate (struct slabnode_stack **v, const char *who)
{
#if 0
    #include <stdio.h>
    int i, t, x, n = 0;
    struct stack_vectors *p = (struct stack_vectors *)v - 1;

    fprintf(stderr, "%s: called by '%s'\n", __func__, who);
    fprintf(stderr, "%s: owned by %p (whose self = %p)\n", __func__, p->owner, p->owner->self);
    for (x = 0; v[x]; x++) {
        struct slabnode_stack *h = v[x];
        struct slab_result *r = h->head;
        fprintf(stderr, "%s:   vector[%02d] = %p", __func__, x, h);
        for (i = 0; r->item < PROCPS_SLABNODE_stack_end; i++, r++)
            ;
        t = i + 1;
        fprintf(stderr, ", stack %d found %d elements\n", n, i);
        ++n;
    }
    fprintf(stderr, "%s: found %d stack(s), each %d bytes (including eos)\n", __func__, x, (int)sizeof(struct slab_result) * t);
    fprintf(stderr, "%s: found %d stack(s)\n", __func__, x);
    fprintf(stderr, "%s: sizeof(struct slabnode_stack)  = %2d\n", __func__, (int)sizeof(struct slabnode_stack));
    fprintf(stderr, "%s: sizeof(struct slab_result)     = %2d\n", __func__, (int)sizeof(struct slab_result));
    fputc('\n', stderr);
    return;
#endif
}

static struct slab_result *stack_make (
        struct slab_result *p,
        int maxitems,
        enum slabnode_item *items)
{
    struct slab_result *p_sav = p;
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
        enum slabnode_item *items)
{
    int i;

    for (i = 0; i < maxitems; i++) {
        if (items[i] < 0)
            return 0;
        if (items[i] > PROCPS_SLABNODE_stack_end)
            return 0;
    }
    if (items[maxitems -1] != PROCPS_SLABNODE_stack_end)
        return 0;
    return 1;
}

/*
 * procps_slabnode_stacks_alloc():
 *
 * Allocate and initialize one or more stacks each of which is anchored in an
 * associated slabnode_stack structure (which may include extra user space).
 *
 * All such stacks will will have their result structures properly primed with
 * 'items', while the result itself will be zeroed.
 *
 * Returns an array of pointers representing the 'heads' of each new stack.
 */
PROCPS_EXPORT struct slabnode_stack **procps_slabnode_stacks_alloc (
        struct procps_slabinfo *info,
        int maxstacks,
        int maxitems,
        enum slabnode_item *items)
{
    struct stacks_anchor *p_blob;
    struct stack_vectors *p_vect;
    struct slabnode_stack *p_head;
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
    head_size  = sizeof(struct slabnode_stack);                // a head struct
    list_size  = sizeof(struct slab_result) * maxitems;        // a results stack
    blob_size  = sizeof(struct stacks_anchor);                 // the anchor itself
    blob_size += vect_size;                                    // all vectors + delims
    blob_size += head_size * maxstacks;                        // all head structs
    blob_size += list_size * maxstacks;                        // all results stacks

    /* note: all memory is allocated in a single blob, facilitating a later free().
       as a minimum, it's important that the result structures themselves always be
       contiguous for any given stack (just as they are when defined statically). */
    if (NULL == (p_blob = calloc(1, blob_size)))
        return NULL;

    p_blob->next = info->stacked;
    info->stacked = p_blob;
    p_blob->self  = p_blob;
    p_blob->vectors = (void *)p_blob + sizeof(struct stacks_anchor);
    p_vect = p_blob->vectors;
    p_vect->owner = p_blob->self;
    p_vect->heads = (void *)p_vect + sizeof(struct stack_vectors);
    v_head = (void *)p_vect + vect_size;
    v_list = v_head + (head_size * maxstacks);

    for (i = 0; i < maxstacks; i++) {
        p_head = (struct slabnode_stack *)v_head;
        p_head->head = stack_make((struct slab_result *)v_list, maxitems, items);
        p_blob->vectors->heads[i] = p_head;
        v_list += list_size;
        v_head += head_size;
    }
    p_blob->depth = maxstacks;
    stacks_validate(p_blob->vectors->heads, __func__);
    return p_blob->vectors->heads;
}

/*
 * procps_slabnode_stack_alloc():
 *
 * Allocate and initialize a single result stack under a simplified interface.
 *
 * Such a stack will will have its result structures properly primed with
 * 'items', while the result itself will be zeroed.
 *
 */
PROCPS_EXPORT struct slabnode_stack *procps_slabnode_stack_alloc (
        struct procps_slabinfo *info,
        int maxitems,
        enum slabnode_item *items)
{
    struct slabnode_stack **v;

    if (info == NULL || items == NULL || maxitems < 1)
        return NULL;
    v = procps_slabnode_stacks_alloc(info, 1, maxitems, items);
    if (!v)
        return NULL;
    stacks_validate(v, __func__);
    return v[0];
}

static int stacks_sort (
        const struct slabnode_stack **A,
        const struct slabnode_stack **B,
        enum slabnode_item *offset)
{
    const struct slab_result *a = (*A)->head + *offset;
    const struct slab_result *b = (*B)->head + *offset;
    // note: everything will be sorted high-to-low
    switch (a->item) {
        case PROCPS_SLABNODE_noop:
        case PROCPS_SLABNODE_stack_end:
            break;
        case PROCPS_SLABNODE_NAME:
            return strcoll(a->result.str, b->result.str);
        case PROCPS_SLABNODE_SIZE:
            if ( a->result.ul_int > b->result.ul_int ) return -1;
            if ( a->result.ul_int < b->result.ul_int ) return +1;
            break;
        default:
            if ( a->result.u_int > b->result.u_int ) return -1;
            if ( a->result.u_int < b->result.u_int ) return +1;
            break;
    }
    return 0;
}

/*
 * procps_slabnode_stacks_sort():
 *
 * Sort stacks anchored as 'heads' in the passed slabnode_stack pointers
 * array based on the designated sort enumerator.
 *
 * Returns those same addresses sorted.
 *
 * Note: all of the stacks must be homogeneous (of equal length and content).
 */
PROCPS_EXPORT struct slabnode_stack **procps_slabnode_stacks_sort (
        struct procps_slabinfo *info,
        struct slabnode_stack **stacks,
        int numstacked,
        enum slabnode_item sort)
{
 #define QSORT_r  int (*)(const void *, const void *, void *)
    struct slab_result *p = stacks[0]->head;
    int offset = 0;;

    if (info == NULL || stacks == NULL)
        return NULL;
    if (sort < 0  || sort > PROCPS_SLABNODE_noop)
        return NULL;
    if (numstacked > info->stacked->depth)
        return NULL;
    if (numstacked < 2)
        return stacks;

    if (numstacked > info->stacked->inuse)
        numstacked = info->stacked->inuse;

    for (;;) {
        if (p->item == sort)
            break;
        ++offset;
        if (p->item == PROCPS_SLABNODE_stack_end)
            return NULL;
        ++p;
    }
    qsort_r(stacks, numstacked, sizeof(void *), (QSORT_r)stacks_sort, &offset);
    return stacks;
 #undef QSORT_r
}
