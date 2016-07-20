/*
 * diskstats - Disk statistics - part of procps
 *
 * Copyright (c) 2003 Fabian Frederick
 * Copyright (C) 2003 Albert Cahalan
 * Copyright (C) 2015 Craig Small <csmall@enc.com.au>
 * Copyright (C) 2016 Jim Warner <james.warner@comcast.net>
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
#ifndef PROC_DISKSTATS_H
#define PROC_DISKSTATS_H

#include <sys/cdefs.h>

__BEGIN_DECLS

enum diskstats_item {
    PROCPS_DISKSTATS_noop,                 //        ( never altered )
    PROCPS_DISKSTATS_extra,                //        ( reset to zero )

    PROCPS_DISKSTATS_NAME,                 //    str
    PROCPS_DISKSTATS_TYPE,                 //  s_int
    PROCPS_DISKSTATS_MAJOR,                //  s_int
    PROCPS_DISKSTATS_MINOR,                //  s_int

    PROCPS_DISKSTATS_READS,                // ul_int
    PROCPS_DISKSTATS_READS_MERGED,         // ul_int
    PROCPS_DISKSTATS_READ_SECTORS,         // ul_int
    PROCPS_DISKSTATS_READ_TIME,            // ul_int
    PROCPS_DISKSTATS_WRITES,               // ul_int
    PROCPS_DISKSTATS_WRITES_MERGED,        // ul_int
    PROCPS_DISKSTATS_WRITE_SECTORS,        // ul_int
    PROCPS_DISKSTATS_WRITE_TIME,           // ul_int
    PROCPS_DISKSTATS_IO_TIME,              // ul_int
    PROCPS_DISKSTATS_IO_WTIME,             // ul_int

    PROCPS_DISKSTATS_IO_INPROGRESS,        //  s_int

    PROCPS_DISKSTATS_DELTA_READS,          //  s_int
    PROCPS_DISKSTATS_DELTA_READS_MERGED,   //  s_int
    PROCPS_DISKSTATS_DELTA_READ_SECTORS,   //  s_int
    PROCPS_DISKSTATS_DELTA_READ_TIME,      //  s_int
    PROCPS_DISKSTATS_DELTA_WRITES,         //  s_int
    PROCPS_DISKSTATS_DELTA_WRITES_MERGED,  //  s_int
    PROCPS_DISKSTATS_DELTA_WRITE_SECTORS,  //  s_int
    PROCPS_DISKSTATS_DELTA_WRITE_TIME,     //  s_int
    PROCPS_DISKSTATS_DELTA_IO_TIME,        //  s_int
    PROCPS_DISKSTATS_DELTA_IO_WTIME        //  s_int
};

enum diskstats_sort_order {
    PROCPS_DISKSTATS_SORT_ASCEND   = +1,
    PROCPS_DISKSTATS_SORT_DESCEND  = -1
};


struct diskstats_result {
    enum diskstats_item item;
    union {
        signed int     s_int;
        unsigned long  ul_int;
        char          *str;
    } result;
};

struct diskstats_stack {
    struct diskstats_result *head;
};

struct diskstats_reap {
    int total;
    struct diskstats_stack **stacks;
};


#define PROCPS_DISKSTATS_TYPE_DISK       -11111
#define PROCPS_DISKSTATS_TYPE_PARTITION  -22222

#define PROCPS_DISKSTATS_GET( diskstats, actual_enum, type ) \
    procps_diskstats_get( diskstats, actual_enum ) -> result . type

#define PROCPS_DISKSTATS_VAL( relative_enum, type, stack) \
    stack -> head [ relative_enum ] . result . type


struct procps_diskstats;

int procps_diskstats_new   (struct procps_diskstats **info);
int procps_diskstats_ref   (struct procps_diskstats  *info);
int procps_diskstats_unref (struct procps_diskstats **info);

struct diskstats_result *procps_diskstats_get (
    struct procps_diskstats *info,
    const char *name,
    enum diskstats_item item);

struct diskstats_reap *procps_diskstats_reap (
    struct procps_diskstats *info,
    enum diskstats_item *items,
    int numitems);

struct diskstats_stack *procps_diskstats_select (
    struct procps_diskstats *info,
    const char *name,
    enum diskstats_item *items,
    int numitems);

struct diskstats_stack **procps_diskstats_sort (
    struct procps_diskstats *info,
    struct diskstats_stack *stacks[],
    int numstacked,
    enum diskstats_item sortitem,
    enum diskstats_sort_order order);

__END_DECLS
#endif
