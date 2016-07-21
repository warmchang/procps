/*
 * slabinfo.h - slab related functions for libproc
 *
 * Copyright (C) 1998-2005 Albert Cahalan
 * Copyright (C) 2015 Craig Small <csmall@enc.com.au>
 * Copyright (C) 2016 Jim Warnerl <james.warner@comcast.net>
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

enum slabinfo_item {
    PROCPS_SLABINFO_noop,            //       ( never altered )
    PROCPS_SLABINFO_extra,           //       ( reset to zero )

    PROCPS_SLABS_OBJS,               //  u_int
    PROCPS_SLABS_AOBJS,              //  u_int
    PROCPS_SLABS_PAGES,              //  u_int
    PROCPS_SLABS_SLABS,              //  u_int
    PROCPS_SLABS_ASLABS,             //  u_int
    PROCPS_SLABS_CACHES,             //  u_int
    PROCPS_SLABS_ACACHES,            //  u_int
    PROCPS_SLABS_SIZE_AVG,           //  u_int
    PROCPS_SLABS_SIZE_MIN,           //  u_int
    PROCPS_SLABS_SIZE_MAX,           //  u_int
    PROCPS_SLABS_SIZE_ACTIVE,        // ul_int
    PROCPS_SLABS_SIZE_TOTAL,         // ul_int

    PROCPS_SLABS_DELTA_OBJS,         //  s_int
    PROCPS_SLABS_DELTA_AOBJS,        //  s_int
    PROCPS_SLABS_DELTA_PAGES,        //  s_int
    PROCPS_SLABS_DELTA_SLABS,        //  s_int
    PROCPS_SLABS_DELTA_ASLABS,       //  s_int
    PROCPS_SLABS_DELTA_CACHES,       //  s_int
    PROCPS_SLABS_DELTA_ACACHES,      //  s_int
    PROCPS_SLABS_DELTA_SIZE_AVG,     //  s_int
    PROCPS_SLABS_DELTA_SIZE_MIN,     //  s_int
    PROCPS_SLABS_DELTA_SIZE_MAX,     //  s_int
    PROCPS_SLABS_DELTA_SIZE_ACTIVE,  //  s_int
    PROCPS_SLABS_DELTA_SIZE_TOTAL,   //  s_int

    PROCPS_SLABNODE_NAME,            //    str
    PROCPS_SLABNODE_OBJS,            //  u_int
    PROCPS_SLABNODE_AOBJS,           //  u_int
    PROCPS_SLABNODE_OBJ_SIZE,        //  u_int
    PROCPS_SLABNODE_OBJS_PER_SLAB,   //  u_int
    PROCPS_SLABNODE_PAGES_PER_SLAB,  //  u_int
    PROCPS_SLABNODE_SLABS,           //  u_int
    PROCPS_SLABNODE_ASLABS,          //  u_int
    PROCPS_SLABNODE_USE,             //  u_int
    PROCPS_SLABNODE_SIZE             // ul_int
};

enum slabinfo_sort_order {
    PROCPS_SLABINFO_ASCEND   = +1,
    PROCPS_SLABINFO_DESCEND  = -1
};


struct slabinfo_result {
    enum slabinfo_item item;
    union {
        signed int     s_int;
        unsigned int   u_int;
        unsigned long  ul_int;
        char          *str;
    } result;
};

struct slabinfo_stack {
    struct slabinfo_result *head;
};

struct slabinfo_reap {
    int total;
    struct slabinfo_stack **stacks;
};


#define PROCPS_SLABINFO_GET( info, actual_enum, type ) \
    procps_slabinfo_get( info, actual_enum ) -> result . type

#define PROCPS_SLABINFO_VAL( relative_enum, type, stack ) \
    stack -> head [ relative_enum ] . result . type


struct slabinfo_info;

int procps_slabinfo_new   (struct slabinfo_info **info);
int procps_slabinfo_ref   (struct slabinfo_info  *info);
int procps_slabinfo_unref (struct slabinfo_info **info);

struct slabinfo_result *procps_slabinfo_get (
    struct slabinfo_info *info,
    enum slabinfo_item item);

struct slabinfo_reap *procps_slabinfo_reap (
    struct slabinfo_info *info,
    enum slabinfo_item *items,
    int numitems);

struct slabinfo_stack *procps_slabinfo_select (
    struct slabinfo_info *info,
    enum slabinfo_item *items,
    int numitems);

struct slabinfo_stack **procps_slabinfo_sort (
    struct slabinfo_info *info,
    struct slabinfo_stack *stacks[],
    int numstacked,
    enum slabinfo_item sortitem,
    enum slabinfo_sort_order order);

__END_DECLS
#endif /* _PROC_SLAB_H */
