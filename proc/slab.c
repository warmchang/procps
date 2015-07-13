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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <proc/slab.h>
#include "procps-private.h"

#define SLABINFO_FILE        "/proc/slabinfo"
#define SLABINFO_LINE_LEN    2048
#define SLAB_INFO_NAME_LEN   128

struct slabinfo_node {
    char name[SLAB_INFO_NAME_LEN];  /* name of this cache */
    unsigned long cache_size;       /* size of entire cache */
    unsigned nr_objs;               /* number of objects in this cache */
    unsigned nr_active_objs;        /* number of active objects */
    unsigned obj_size;              /* size of each object */
    unsigned objs_per_slab;         /* number of objects per slab */
    unsigned pages_per_slab;        /* number of pages per slab */
    unsigned nr_slabs;              /* number of slabs in this cache */
    unsigned nr_active_slabs;       /* number of active slabs */
    unsigned use;                   /* percent full: total / active */
};

struct slabinfo_stats {
    unsigned long total_size;       /* size of all objects */
    unsigned long active_size;      /* size of all active objects */
    unsigned nr_objs;               /* number of objects, among all caches */
    unsigned nr_active_objs;        /* number of active objects, among all caches */
    unsigned nr_pages;              /* number of pages consumed by all objects */
    unsigned nr_slabs;              /* number of slabs, among all caches */
    unsigned nr_active_slabs;       /* number of active slabs, among all caches */
    unsigned nr_caches;             /* number of caches */
    unsigned nr_active_caches;      /* number of active caches */
    unsigned avg_obj_size;          /* average object size */
    unsigned min_obj_size;          /* size of smallest object */
    unsigned max_obj_size;          /* size of largest object */
};

struct procps_slabinfo {
    int refcount;
    FILE *slabinfo_fp;
    struct slabinfo_stats stats;
    struct slabinfo_node *nodes;    /* first slabnode of this list */
    int nodes_alloc;                /* nodes alloc()ed */
    int nodes_used;                 /* nodes using alloced memory */
    struct chains_anchor *chained;
};

struct chain_vectors {
    struct chains_anchor *owner;
    struct slabnode_chain **heads;
};

struct chains_anchor {
    int depth;
    int inuse;
    int header_size;
    struct chain_vectors *vectors;
    struct chains_anchor *self;
    struct chains_anchor *next;
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
                   "%" STRINGIFY(SLAB_INFO_NAME_LEN)
                   "s %d %d %d %d %d : tunables %*d %*d %*d : \
                   slabdata %d %d %*d", node->name,
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
            node->use = 100 * node->nr_active_objs / node->nr_objs;
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
 * into the supplie info container
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
        if ((*info)->chained) {
            do {
                struct chains_anchor *p = (*info)->chained;
                (*info)->chained = (*info)->chained->next;
                free(p);
            } while((*info)->chained);
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
        case PROCPS_SLABS_noop:
            break;
    }
    return 0;
}

PROCPS_EXPORT int procps_slabs_getchain (
        struct procps_slabinfo *info,
        struct slabs_result *these)
{
    if (info == NULL || these == NULL)
        return -EINVAL;

    do {
        switch (these->item) {
            case PROCPS_SLABS_OBJS:
                these->result = info->stats.nr_objs;
                break;
            case PROCPS_SLABS_AOBJS:
                these->result = info->stats.nr_active_objs;
                break;
            case PROCPS_SLABS_PAGES:
                these->result = info->stats.nr_pages;
                break;
            case PROCPS_SLABS_SLABS:
                these->result = info->stats.nr_slabs;
                break;
            case PROCPS_SLABS_ASLABS:
                these->result = info->stats.nr_active_slabs;
                break;
            case PROCPS_SLABS_CACHES:
                these->result = info->stats.nr_caches;
                break;
            case PROCPS_SLABS_ACACHES:
                these->result = info->stats.nr_active_caches;
                break;
            case PROCPS_SLABS_SIZE_AVG:
                these->result = info->stats.avg_obj_size;
                break;
            case PROCPS_SLABS_SIZE_MIN:
                these->result = info->stats.min_obj_size;
                break;
            case PROCPS_SLABS_SIZE_MAX:
                these->result = info->stats.max_obj_size;
                break;
            case PROCPS_SLABS_SIZE_TOTAL:
                these->result = info->stats.total_size;
                break;
            case PROCPS_SLABS_SIZE_ACTIVE:
                these->result = info->stats.active_size;
                break;
            default:
                return -EINVAL;
        }
        these = these->next;
    } while(these);
    return 0;
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
        case PROCPS_SLABNODE_NAME:
        case PROCPS_SLABNODE_noop:
            return 0;
        default:
            return -EINVAL;
    }
    return 0;
}

PROCPS_EXPORT int procps_slabnode_getchain (
        struct procps_slabinfo *info,
        struct slabnode_result *these,
        int nodeid)
{
    if (info == NULL || these == NULL)
        return -EINVAL;
    if (nodeid > info->nodes_used)
        return -EINVAL;

    do {
        switch (these->item) {
            case PROCPS_SLABNODE_SIZE:
                these->result.num = info->nodes[nodeid].cache_size;
                break;
            case PROCPS_SLABNODE_OBJS:
                these->result.num = info->nodes[nodeid].nr_objs;
                break;
            case PROCPS_SLABNODE_AOBJS:
                these->result.num = info->nodes[nodeid].nr_active_objs;
                break;
            case PROCPS_SLABNODE_OBJ_SIZE:
                these->result.num = info->nodes[nodeid].obj_size;
                break;
            case PROCPS_SLABNODE_OBJS_PER_SLAB:
                these->result.num = info->nodes[nodeid].objs_per_slab;
                break;
            case PROCPS_SLABNODE_PAGES_PER_SLAB:
                these->result.num = info->nodes[nodeid].pages_per_slab;
                break;
            case PROCPS_SLABNODE_SLABS:
                these->result.num = info->nodes[nodeid].nr_slabs;
                break;
            case PROCPS_SLABNODE_ASLABS:
                these->result.num = info->nodes[nodeid].nr_active_slabs;
                break;
            case PROCPS_SLABNODE_USE:
                these->result.num = info->nodes[nodeid].use;
                break;
            case PROCPS_SLABNODE_NAME:
                these->result.str = info->nodes[nodeid].name;
                break;
            case PROCPS_SLABNODE_noop:
                break;
            default:
                return -EINVAL;
        }
        these = these->next;
    } while(these);
    return 0;
}


PROCPS_EXPORT int procps_slabnode_chain_fill (
    struct procps_slabinfo *info,
    struct slabnode_chain *chain,
    int nodeid)
{
    int rc;

    if (info == NULL || chain == NULL || chain->head == NULL)
        return -EINVAL;

    if ((rc = procps_slabinfo_read(info)) < 0)
        return rc;

    return procps_slabnode_getchain(info, chain->head, nodeid);
}

/*
 * procps_slabnode_count():
 *
 * @info: read in slabinfo structure
 *
 * Returns: number of nodes in @info or <0 on error
 */
PROCPS_EXPORT int procps_slabnode_count (
        const struct procps_slabinfo *info)
{
    if (!info)
        return -EINVAL;
    return info->nodes_used;
}

PROCPS_EXPORT int procps_slabnode_chains_fill (
    struct procps_slabinfo *info,
    struct slabnode_chain **chains,
    int maxchains)
{
    int i, rc;

    if (info == NULL || *chains == NULL)
        return -EINVAL;
    if (maxchains < 1)
        return -EINVAL;

    if ((rc = procps_slabinfo_read(info)) < 0)
        return rc;

    if (maxchains > info->chained->depth)
        maxchains = info->chained->depth;
    if (maxchains > info->nodes_used)
        maxchains = info->nodes_used;

    for (i = 0; i < maxchains; i++) {
        if (chains[i] == NULL)
            break;
        if ((rc = procps_slabnode_getchain(info, chains[i]->head, i) < 0))
            return rc;
    }

    info->chained->inuse = i;
    return info->chained->inuse;
}

static void chains_validate (struct slabnode_chain **v, const char *who)
{
#if 0
    #include <stdio.h>
    int i, x, n = 0;
    struct chain_vectors *p = (struct chain_vectors *)v - 1;

    fprintf(stderr, "%s: called by '%s'\n", __func__, who);
    fprintf(stderr, "%s: owned by %p (whose self = %p)\n", __func__, p->owner, p->owner->self);
    for (x = 0; v[x]; x++) {
        struct slabnode_chain *h = v[x];
        struct slabnode_result *r = h->head;
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
    fprintf(stderr, "%s: sizeof(struct slabnode_chain)  = %2d\n", __func__, (int)sizeof(struct slabnode_chain));
    fprintf(stderr, "%s: sizeof(struct slabnode_result) = %2d\n", __func__, (int)sizeof(struct slabnode_result));
    fputc('\n', stderr);
    return;
#endif
}

static struct slabnode_result *chain_make (
        struct slabnode_result *p,
        int maxitems,
        enum slabnode_item *items)
{
    struct slabnode_result *p_sav = p;
    int i;

    for (i = 0; i < maxitems; i++) {
        if (i > PROCPS_SLABNODE_noop)
            p->item = PROCPS_SLABNODE_noop;
        else
            p->item = items[i];
        p->result.num = 0;
        p->next = p + 1;
        ++p;
    }
    (--p)->next = NULL;

    return p_sav;
}

/*
 * procps_slabnode_chains_alloc():
 *
 * Allocate and initialize one or more chains each of which is anchored in an
 * associated meminfo_chain structure (which may include extra user space).
 *
 * All such chains will will have their result structures properly primed with
 * 'items' and 'next' pointers, while the result itself will be zeroed.
 *
 * Returns an array of pointers representing the 'heads' of each new chain.
 */
PROCPS_EXPORT struct slabnode_chain **procps_slabnode_chains_alloc (
        struct procps_slabinfo *info,
        int maxchains,
        int chain_extra,
        int maxitems,
        enum slabnode_item *items)
{
    struct chains_anchor *p_blob;
    struct chain_vectors *p_vect;
    struct slabnode_chain *p_head;
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
    head_size  = sizeof(struct slabnode_chain) + chain_extra;  // a head struct + user stuff
    list_size  = sizeof(struct slabnode_result) * maxitems;    // a results chain
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
        p_head = (struct slabnode_chain *)v_head;
        p_head->head = chain_make((struct slabnode_result *)v_list, maxitems, items);
        p_blob->vectors->heads[i] = p_head;
        v_list += list_size;
        v_head += head_size;
    }
    p_blob->depth = maxchains;
    chains_validate(p_blob->vectors->heads, __func__);
    return p_blob->vectors->heads;
}

/*
 * procps_slabnode_chain_alloc():
 *
 * Allocate and initialize a single result chain under a simplified interface.
 *
 * Such a chain will will have its result structures properly primed with
 * 'items' and 'next' pointers, while the result itself will be zeroed.
 *
 */
PROCPS_EXPORT struct slabnode_chain *procps_slabnode_chain_alloc (
        struct procps_slabinfo *info,
        int maxitems,
        enum slabnode_item *items)
{
    struct slabnode_chain **v;

    if (info == NULL || items == NULL || maxitems < 1)
        return NULL;
    v = procps_slabnode_chains_alloc(info, 1, 0, maxitems, items);
    if (!v)
        return NULL;
    chains_validate(v, __func__);
    return v[0];
}

static int chains_sort (
        const struct slabnode_chain **A,
        const struct slabnode_chain **B,
        enum slabnode_item *offset)
{
    const struct slabnode_result *a = (*A)->head + *offset;
    const struct slabnode_result *b = (*B)->head + *offset;
    // note: everything will be sorted high-to-low
    if (a->item == PROCPS_SLABNODE_NAME)
        return strcoll(a->result.str, b->result.str);
    if ( a->result.num > b->result.num ) return -1;
    if ( a->result.num < b->result.num ) return +1;
    return 0;
}

/*
 * procps_slabnode_chains_sort():
 *
 * Sort chains anchored as 'heads' in the passed slabnode_chain pointers
 * array based on the designated sort enumerator.
 *
 * Returns those same addresses sorted.
 *
 * Note: all of the chains must be homogeneous (of equal length and content).
 */
PROCPS_EXPORT struct slabnode_chain **procps_slabnode_chains_sort (
        struct procps_slabinfo *info,
        struct slabnode_chain **chains,
        int numchained,
        enum slabnode_item sort)
{
 #define QSORT_r  int (*)(const void *, const void *, void *)
    struct slabnode_result *p = chains[0]->head;
    int offset = 0;;

    if (info == NULL || chains == NULL)
        return NULL;
    if (sort < 0  || sort >= PROCPS_SLABNODE_noop)
        return NULL;
    if (numchained > info->chained->depth)
        return NULL;
    if (numchained < 2)
        return chains;

    if (numchained > info->chained->inuse)
        numchained = info->chained->inuse;

    for (;;) {
        if (p->item == sort)
            break;
        ++offset;
        if (!(p = p->next))
            return NULL;
    }
    qsort_r(chains, numchained, sizeof(void *), (QSORT_r)chains_sort, &offset);
    return chains;
 #undef QSORT_r
}
