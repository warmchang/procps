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

#include <features.h>

__BEGIN_DECLS

enum slabs_item {
    PROCPS_SLABS_OBJS,                 // u_int
    PROCPS_SLABS_AOBJS,                // u_int
    PROCPS_SLABS_PAGES,                // u_int
    PROCPS_SLABS_SLABS,                // u_int
    PROCPS_SLABS_ASLABS,               // u_int
    PROCPS_SLABS_CACHES,               // u_int
    PROCPS_SLABS_ACACHES,              // u_int
    PROCPS_SLABS_SIZE_AVG,             // u_int
    PROCPS_SLABS_SIZE_MIN,             // u_int
    PROCPS_SLABS_SIZE_MAX,             // u_int
    PROCPS_SLABS_SIZE_TOTAL,           // ul_int
    PROCPS_SLABS_SIZE_ACTIVE,          // ul_int
    PROCPS_SLABS_noop,                 // n/a
    PROCPS_SLABS_stack_end             // n/a
};

enum slabnode_item {
    PROCPS_SLABNODE_SIZE,              // ul_int
    PROCPS_SLABNODE_OBJS,              // u_int
    PROCPS_SLABNODE_AOBJS,             // u_int
    PROCPS_SLABNODE_OBJ_SIZE,          // u_int
    PROCPS_SLABNODE_OBJS_PER_SLAB,     // u_int
    PROCPS_SLABNODE_PAGES_PER_SLAB,    // u_int
    PROCPS_SLABNODE_SLABS,             // u_int
    PROCPS_SLABNODE_ASLABS,            // u_int
    PROCPS_SLABNODE_USE,               // u_int
    PROCPS_SLABNODE_NAME,              // str
    PROCPS_SLABNODE_noop,              // n/a
    PROCPS_SLABNODE_stack_end          // n/a
};

struct procps_slabinfo;

struct slabnode_stack {
    struct slab_result *head;
};

struct slab_result {
    int item;
    union {
        unsigned int u_int;
        unsigned long ul_int;
        char *str;
    } result;
};

int procps_slabinfo_new (struct procps_slabinfo **info);
int procps_slabinfo_read (struct procps_slabinfo *info);

int procps_slabinfo_ref (struct procps_slabinfo *info);
int procps_slabinfo_unref (struct procps_slabinfo **info);

unsigned long procps_slabs_get (
    struct procps_slabinfo *info,
    enum slabs_item item);

int procps_slabs_getstack (
    struct procps_slabinfo *info,
    struct slab_result *these);

int procps_slabnode_count (struct procps_slabinfo *info);

const char *procps_slabnode_getname (
    struct procps_slabinfo *info,
    int nodeid);

unsigned long procps_slabnode_get (
    struct procps_slabinfo *info,
    enum slabnode_item item,
    int nodeid);

int procps_slabnode_getstack (
    struct procps_slabinfo *info,
    struct slab_result *these,
    int nodeid);

int procps_slabnode_stack_fill (
    struct procps_slabinfo *info,
    struct slabnode_stack *stack,
    int nodeid);

int procps_slabnode_stacks_fill (
    struct procps_slabinfo *info,
    struct slabnode_stack **stacks,
    int maxstacks);

struct slabnode_stack *procps_slabnode_stack_alloc (
    struct procps_slabinfo *info,
    int maxitems,
    enum slabnode_item *items);

struct slabnode_stack **procps_slabnode_stacks_alloc (
    struct procps_slabinfo *info,
    int maxstacks,
    int maxitems,
    enum slabnode_item *items);

struct slabnode_stack **procps_slabnode_stacks_sort (
    struct procps_slabinfo *info,
    struct slabnode_stack **stacks,
    int numstacked,
    enum slabnode_item sort);

__END_DECLS

#endif /* _PROC_SLAB_H */
