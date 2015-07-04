/*
 * slab.c - slab related functions for libproc
 *
 * Chris Rivera <cmrivera@ufl.edu>
 * Robert Love <rml@tech9.net>
 *
 * Copyright (C) 2003 Chris Rivera
 * Copyright 2004, Albert Cahalan
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

#include "slab.h"
#include "procps-private.h"

#define SLABINFO_LINE_LEN    2048
#define SLABINFO_VER_LEN    100
#define SLAB_INFO_NAME_LEN      128
#define SLABINFO_FILE        "/proc/slabinfo"
#define INITIAL_NODES       30

struct procps_slabnode {
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
    struct procps_slabnode *nodes;  /* first slabnode of this list */
    int nodes_alloc;                /* nodes alloc()ed */
    int nodes_used;                 /* nodes using alloced memory */
};

/*
 * Zero out the slabnode data, keeping the memory allocated.
 */
static void slabnodes_clear(
        struct procps_slabinfo *info)
{
    if (info == NULL || info->nodes == NULL || info->nodes_alloc < 1)
        return;
    memset(info->nodes, 0, sizeof(struct procps_slabnode)*info->nodes_alloc);
    info->nodes_used = 0;
}

/* Alloc up more slabnode memory, if required
 */
static int slabnodes_alloc(
        struct procps_slabinfo *info)
{
    struct procps_slabnode *new_nodes;
    int new_count;

    if (info == NULL)
        return -EINVAL;

    if (info->nodes_used < info->nodes_alloc)
        return 0;
    /* Increment the allocated number of slabs */
    new_count = info->nodes_alloc * 5/4+30;

    if ((new_nodes = realloc(info->nodes,
                             sizeof(struct procps_slabnode)*new_count)) == NULL)
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
static int get_slabnode(
        struct procps_slabinfo *info,
        struct procps_slabnode **node)
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
static int parse_slabinfo20(
        struct procps_slabinfo *info)
{
    struct procps_slabnode *node;
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
 * parse_slabinfo11 - actual parsing routine for slabinfo 1.1 (2.4 kernels)
 */
static int parse_slabinfo11(
        struct procps_slabinfo *info)
{
    struct procps_slabnode *node;
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
                   "s %d %d %d %d %d %d",
                   node->name, &node->nr_active_objs,
                   &node->nr_objs, &node->obj_size,
                   &node->nr_active_slabs, &node->nr_slabs,
                   &node->pages_per_slab) < 6) {
            if (errno != 0)
                return -errno;
            return -EINVAL;
        }

        if (node->obj_size < stats->min_obj_size)
            stats->min_obj_size = node->obj_size;
        if (node->obj_size > stats->max_obj_size)
            stats->max_obj_size = node->obj_size;

        node->cache_size = (unsigned long)node->nr_slabs *
            node->pages_per_slab * page_size;

        if (node->nr_objs) {
            node->use = 100 * node->nr_active_objs / node->nr_objs;
            stats->nr_active_caches++;
        } else
            node->use = 0;

        if (node->obj_size)
            node->objs_per_slab = node->pages_per_slab *
                    page_size / node->obj_size;

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
 * procps_slabinfo_new:
 *
 * @info: location of returned new structure
 *
 * Returns: 0 on success <0 on failure
 */
PROCPS_EXPORT int procps_slabinfo_new(
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

/* procps_slabinfo_read:
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
    int retval, size, major, minor;

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
    else if (major == 1 && minor == 1)
        retval = parse_slabinfo11(info);
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
        free(*info);
        *info = NULL;
        return 0;
    }
    return (*info)->refcount;
}

PROCPS_EXPORT unsigned long procps_slabinfo_stat_get(
        struct procps_slabinfo *info,
        enum procps_slabinfo_stat item)
{
    if (!item)
        return 0;
    switch (item) {
    case PROCPS_SLABINFO_OBJS:
        return info->stats.nr_objs;
    case PROCPS_SLABINFO_AOBJS:
        return info->stats.nr_active_objs;
    case PROCPS_SLABINFO_PAGES:
        return info->stats.nr_pages;
    case PROCPS_SLABINFO_SLABS:
        return info->stats.nr_slabs;
    case PROCPS_SLABINFO_ASLABS:
        return info->stats.nr_active_slabs;
    case PROCPS_SLABINFO_CACHES:
        return info->stats.nr_caches;
    case PROCPS_SLABINFO_ACACHES:
        return info->stats.nr_active_caches;
    case PROCPS_SLABINFO_SIZE_AVG:
        return info->stats.avg_obj_size;
    case PROCPS_SLABINFO_SIZE_MIN:
        return info->stats.min_obj_size;
    case PROCPS_SLABINFO_SIZE_MAX:
        return info->stats.max_obj_size;
    case PROCPS_SLABINFO_SIZE_TOTAL:
        return info->stats.total_size;
    case PROCPS_SLABINFO_SIZE_ACTIVE:
        return info->stats.active_size;
    }
    return 0;
}

PROCPS_EXPORT int procps_slabinfo_stat_getchain(
        struct procps_slabinfo *info,
        struct procps_slabinfo_result *result)
{
    if (!info || !result)
        return -EINVAL;

    do {
        switch (result->item) {
        case PROCPS_SLABINFO_OBJS:
            result->result = info->stats.nr_objs;
            break;
        case PROCPS_SLABINFO_AOBJS:
            result->result = info->stats.nr_active_objs;
            break;
        case PROCPS_SLABINFO_PAGES:
            result->result = info->stats.nr_pages;
            break;
        case PROCPS_SLABINFO_SLABS:
            result->result = info->stats.nr_slabs;
            break;
        case PROCPS_SLABINFO_ASLABS:
            result->result = info->stats.nr_active_slabs;
            break;
        case PROCPS_SLABINFO_CACHES:
            result->result = info->stats.nr_caches;
            break;
        case PROCPS_SLABINFO_ACACHES:
            result->result = info->stats.nr_active_caches;
            break;
        case PROCPS_SLABINFO_SIZE_AVG:
            result->result = info->stats.avg_obj_size;
            break;
        case PROCPS_SLABINFO_SIZE_MIN:
            result->result = info->stats.min_obj_size;
            break;
        case PROCPS_SLABINFO_SIZE_MAX:
            result->result = info->stats.max_obj_size;
            break;
        case PROCPS_SLABINFO_SIZE_TOTAL:
            result->result = info->stats.total_size;
            break;
        case PROCPS_SLABINFO_SIZE_ACTIVE:
            result->result = info->stats.active_size;
            break;
        default:
            return -EINVAL;
        }
        result = result->next;
    } while(result);
    return 0;
}

/*
 * procps_slabinfo_node_getname():
 *
 * @info: slabinfo structure with data read in
 * @nodeid: number of node we want the name for
 *
 * Find the name of the given node
 *
 * Returns: name or NULL on error
 */
PROCPS_EXPORT char *procps_slabinfo_node_getname(
        struct procps_slabinfo *info,
        int nodeid)
{
    if (info == NULL)
        return NULL;
    if (nodeid > info->nodes_used)
        return NULL;
    return info->nodes[nodeid].name;
}

PROCPS_EXPORT int procps_slabinfo_node_getchain (
        struct procps_slabinfo *info,
        struct procps_slabnode_result *result,
        int nodeid)
{
    if (info == NULL || result == NULL)
        return -EINVAL;
    if (nodeid > info->nodes_used)
        return -EINVAL;

    do {
        switch (result->item) {
        case PROCPS_SLABNODE_SIZE:
            result->result = info->nodes[nodeid].cache_size;
            break;
        case PROCPS_SLABNODE_OBJS:
            result->result = info->nodes[nodeid].nr_objs;
            break;
        case PROCPS_SLABNODE_AOBJS:
            result->result = info->nodes[nodeid].nr_active_objs;
            break;
        case PROCPS_SLABNODE_OBJ_SIZE:
            result->result = info->nodes[nodeid].obj_size;
            break;
        case PROCPS_SLABNODE_OBJS_PER_SLAB:
            result->result = info->nodes[nodeid].objs_per_slab;
            break;
        case PROCPS_SLABNODE_PAGES_PER_SLAB:
            result->result = info->nodes[nodeid].pages_per_slab;
            break;
        case PROCPS_SLABNODE_SLABS:
            result->result = info->nodes[nodeid].nr_slabs;
            break;
        case PROCPS_SLABNODE_ASLABS:
            result->result = info->nodes[nodeid].nr_active_slabs;
            break;
        case PROCPS_SLABNODE_USE:
            result->result = info->nodes[nodeid].use;
            break;
        default:
            return -EINVAL;
        }
        result = result->next;
    } while(result);
    return 0;
}

/*
 * Sorting functions
 *
 * These functions sort the slabnodes. The sort type is
 * the same as the get enum
 */
static int sort_name(const void *a, const void *b)
{
    return strcmp(
                   ((struct procps_slabnode*)a)->name,
                   ((struct procps_slabnode*)b)->name);
}

static int sort_objs(const void *a, const void *b)
{
    return ((struct procps_slabnode*)b)->nr_objs -
        ((struct procps_slabnode*)a)->nr_objs;
}

static int sort_aobjs(const void *a, const void *b)
{
    return ((struct procps_slabnode*)b)->nr_active_objs -
        ((struct procps_slabnode*)a)->nr_active_objs;
}

static int sort_size(const void *a, const void *b)
{
    return ((struct procps_slabnode*)b)->obj_size -
        ((struct procps_slabnode*)a)->obj_size;
}

static int sort_objsperslab(const void *a, const void *b)
{
    return ((struct procps_slabnode*)b)->objs_per_slab -
        ((struct procps_slabnode*)a)->objs_per_slab;
}

static int sort_pagesperslab(const void *a, const void *b)
{
    return ((struct procps_slabnode*)b)->pages_per_slab -
        ((struct procps_slabnode*)a)->pages_per_slab;
}

static int sort_slabs(const void *a, const void *b)
{
    return ((struct procps_slabnode*)b)->nr_slabs -
        ((struct procps_slabnode*)a)->nr_slabs;
}

static int sort_use(const void *a, const void *b)
{
    return ((struct procps_slabnode*)b)->use -
        ((struct procps_slabnode*)a)->use;
}

static int sort_aslabs(const void *a, const void *b)
{
    return ((struct procps_slabnode*)b)->nr_active_slabs -
        ((struct procps_slabnode*)a)->nr_active_slabs;
}

/*
 * procps_slabinfo_sort:
 *
 * @info: the slabinfo that has the read data
 * @item: slabnode item to sort by
 *
 * Sort the slabnodes contained in @info based
 * upon the criteria @item
 *
 * Returns: 0 on success < on error
 */
PROCPS_EXPORT int procps_slabinfo_sort(
        struct procps_slabinfo *info,
        const enum procps_slabinfo_nodeitem item)
{
    void * sort_func = NULL;

    if (info == NULL)
        return -EINVAL;

    switch (item) {
    case PROCPS_SLABNODE_NAME:
        sort_func = sort_name;
        break;
    case PROCPS_SLABNODE_OBJS:
        sort_func = sort_objs;
        break;
    case PROCPS_SLABNODE_AOBJS:
        sort_func = sort_aobjs;
        break;
    case PROCPS_SLABNODE_SIZE:
        sort_func = sort_size;
        break;
    case PROCPS_SLABNODE_OBJS_PER_SLAB:
        sort_func = sort_objsperslab;
        break;
    case PROCPS_SLABNODE_PAGES_PER_SLAB:
        sort_func = sort_pagesperslab;
        break;
    case PROCPS_SLABNODE_SLABS:
        sort_func = sort_slabs;
        break;
    case PROCPS_SLABNODE_ASLABS:
        sort_func = sort_aslabs;
        break;
    case PROCPS_SLABNODE_USE:
        sort_func = sort_use;
        break;
    default:
        return -EINVAL;
    }
    qsort(info->nodes, info->nodes_used,
          sizeof(struct procps_slabnode), sort_func);
    return 0;
}

/*
 * procps_slabinfo_node_count():
 *
 * @info: read in slabinfo structure
 *
 * Returns: number of nodes in @info or <0 on error
 */
PROCPS_EXPORT int procps_slabinfo_node_count(
        const struct procps_slabinfo *info)
{
    if (!info)
        return -EINVAL;
    return info->nodes_used;
}
