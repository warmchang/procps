/*
 * slab.h - slab related functions for libproc
 *
 * Copyright (C) 1998-2005 Albert Cahalan
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

#ifndef _PROC_SLAB_H
#define _PROC_SLAB_H

__BEGIN_DECLS

enum slabs_item {
    PROCPS_SLABS_OBJS,
    PROCPS_SLABS_AOBJS,
    PROCPS_SLABS_PAGES,
    PROCPS_SLABS_SLABS,
    PROCPS_SLABS_ASLABS,
    PROCPS_SLABS_CACHES,
    PROCPS_SLABS_ACACHES,
    PROCPS_SLABS_SIZE_AVG,
    PROCPS_SLABS_SIZE_MIN,
    PROCPS_SLABS_SIZE_MAX,
    PROCPS_SLABS_SIZE_TOTAL,
    PROCPS_SLABS_SIZE_ACTIVE,
    PROCPS_SLABS_noop
};

enum slabnode_item {
    PROCPS_SLABNODE_SIZE,
    PROCPS_SLABNODE_OBJS,
    PROCPS_SLABNODE_AOBJS,
    PROCPS_SLABNODE_OBJ_SIZE,
    PROCPS_SLABNODE_OBJS_PER_SLAB,
    PROCPS_SLABNODE_PAGES_PER_SLAB,
    PROCPS_SLABNODE_SLABS,
    PROCPS_SLABNODE_ASLABS,
    PROCPS_SLABNODE_USE,
    PROCPS_SLABNODE_NAME,
    PROCPS_SLABNODE_noop
};

struct procps_slabinfo;

struct slabs_result {
    enum slabs_item item;
    unsigned long result;
    struct slabs_result *next;
};

struct slabnode_chain {
    struct slabnode_result *head;
};

struct slabnode_result {
    enum slabnode_item item;
    union {
        unsigned long num;
        char *str;
    } result;
    struct slabnode_result *next;
};

int procps_slabinfo_new (struct procps_slabinfo **info);
int procps_slabinfo_read (struct procps_slabinfo *info);

int procps_slabinfo_ref (struct procps_slabinfo *info);
int procps_slabinfo_unref (struct procps_slabinfo **info);

unsigned long procps_slabs_get (
    struct procps_slabinfo *info,
    enum slabs_item item);

int procps_slabs_getchain (
    struct procps_slabinfo *info,
    struct slabs_result *these);

int procps_slabnode_count (const struct procps_slabinfo *info);

const char *procps_slabnode_getname (
    struct procps_slabinfo *info,
    int nodeid);

unsigned long procps_slabnode_get (
    struct procps_slabinfo *info,
    enum slabnode_item item,
    int nodeid);

int procps_slabnode_getchain (
    struct procps_slabinfo *info,
    struct slabnode_result *these,
    int nodeid);

int procps_slabnode_chain_fill (
    struct procps_slabinfo *info,
    struct slabnode_chain *chain,
    int nodeid);

int procps_slabnode_chains_fill (
    struct procps_slabinfo *info,
    struct slabnode_chain **chains,
    int maxchains);

struct slabnode_chain *procps_slabnode_chain_alloc (
    struct procps_slabinfo *info,
    int maxitems,
    enum slabnode_item *items);

struct slabnode_chain **procps_slabnode_chains_alloc (
    struct procps_slabinfo *info,
    int maxchains,
    int chain_extra,
    int maxitems,
    enum slabnode_item *items);

struct slabnode_chain **procps_slabnode_chains_sort (
    struct procps_slabinfo *info,
    struct slabnode_chain **chains,
    int numchained,
    enum slabnode_item sort);

__END_DECLS

#endif /* _PROC_SLAB_H */
