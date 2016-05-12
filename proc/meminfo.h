/*
 * libprocps - Library to read proc filesystem
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

#ifndef PROC_MEMINFO_H
#define PROC_MEMINFO_H

__BEGIN_DECLS

enum meminfo_item {
    PROCPS_MEMINFO_noop,                  // n/a     ( never altered )
    PROCPS_MEMINFO_extra,                 // n/a     ( reset to zero )
    /*
        note: all of the following values are exressed as KiB
    */
    PROCPS_MEMINFO_MEM_ACTIVE,            //  ul_int
    PROCPS_MEMINFO_MEM_ACTIVE_ANON,       //  ul_int
    PROCPS_MEMINFO_MEM_ACTIVE_FILE,       //  ul_int
    PROCPS_MEMINFO_MEM_ANON,              //  ul_int
    PROCPS_MEMINFO_MEM_AVAILABLE,         //  ul_int
    PROCPS_MEMINFO_MEM_BOUNCE,            //  ul_int
    PROCPS_MEMINFO_MEM_BUFFERS,           //  ul_int
    PROCPS_MEMINFO_MEM_CACHED,            //  ul_int
    PROCPS_MEMINFO_MEM_COMMIT_LIMIT,      //  ul_int
    PROCPS_MEMINFO_MEM_COMMITTED_AS,      //  ul_int
    PROCPS_MEMINFO_MEM_HARD_CORRUPTED,    //  ul_int
    PROCPS_MEMINFO_MEM_DIRTY,             //  ul_int
    PROCPS_MEMINFO_MEM_FREE,              //  ul_int
    PROCPS_MEMINFO_MEM_HUGE_ANON,         //  ul_int
    PROCPS_MEMINFO_MEM_HUGE_FREE,         //  ul_int
    PROCPS_MEMINFO_MEM_HUGE_RSVD,         //  ul_int
    PROCPS_MEMINFO_MEM_HUGE_SIZE,         //  ul_int
    PROCPS_MEMINFO_MEM_HUGE_SURPLUS,      //  ul_int
    PROCPS_MEMINFO_MEM_HUGE_TOTAL,        //  ul_int
    PROCPS_MEMINFO_MEM_INACTIVE,          //  ul_int
    PROCPS_MEMINFO_MEM_INACTIVE_ANON,     //  ul_int
    PROCPS_MEMINFO_MEM_INACTIVE_FILE,     //  ul_int
    PROCPS_MEMINFO_MEM_KERNEL_STACK,      //  ul_int
    PROCPS_MEMINFO_MEM_LOCKED,            //  ul_int
    PROCPS_MEMINFO_MEM_MAPPED,            //  ul_int
    PROCPS_MEMINFO_MEM_NFS_UNSTABLE,      //  ul_int
    PROCPS_MEMINFO_MEM_PAGE_TABLES,       //  ul_int
    PROCPS_MEMINFO_MEM_SHARED,            //  ul_int
    PROCPS_MEMINFO_MEM_SLAB,              //  ul_int
    PROCPS_MEMINFO_MEM_SLAB_RECLAIM,      //  ul_int
    PROCPS_MEMINFO_MEM_SLAB_UNRECLAIM,    //  ul_int
    PROCPS_MEMINFO_MEM_TOTAL,             //  ul_int
    PROCPS_MEMINFO_MEM_UNEVICTABLE,       //  ul_int
    PROCPS_MEMINFO_MEM_USED,              //  ul_int
    PROCPS_MEMINFO_MEM_VM_ALLOC_CHUNK,    //  ul_int
    PROCPS_MEMINFO_MEM_VM_ALLOC_TOTAL,    //  ul_int
    PROCPS_MEMINFO_MEM_VM_ALLOC_USED,     //  ul_int
    PROCPS_MEMINFO_MEM_WRITEBACK,         //  ul_int
    PROCPS_MEMINFO_MEM_WRITEBACK_TMP,     //  ul_int

    PROCPS_MEMINFO_DELTA_ACTIVE,          //   s_int
    PROCPS_MEMINFO_DELTA_ACTIVE_ANON,     //   s_int
    PROCPS_MEMINFO_DELTA_ACTIVE_FILE,     //   s_int
    PROCPS_MEMINFO_DELTA_ANON,            //   s_int
    PROCPS_MEMINFO_DELTA_AVAILABLE,       //   s_int
    PROCPS_MEMINFO_DELTA_BOUNCE,          //   s_int
    PROCPS_MEMINFO_DELTA_BUFFERS,         //   s_int
    PROCPS_MEMINFO_DELTA_CACHED,          //   s_int
    PROCPS_MEMINFO_DELTA_COMMIT_LIMIT,    //   s_int
    PROCPS_MEMINFO_DELTA_COMMITTED_AS,    //   s_int
    PROCPS_MEMINFO_DELTA_HARD_CORRUPTED,  //   s_int
    PROCPS_MEMINFO_DELTA_DIRTY,           //   s_int
    PROCPS_MEMINFO_DELTA_FREE,            //   s_int
    PROCPS_MEMINFO_DELTA_HUGE_ANON,       //   s_int
    PROCPS_MEMINFO_DELTA_HUGE_FREE,       //   s_int
    PROCPS_MEMINFO_DELTA_HUGE_RSVD,       //   s_int
    PROCPS_MEMINFO_DELTA_HUGE_SIZE,       //   s_int
    PROCPS_MEMINFO_DELTA_HUGE_SURPLUS,    //   s_int
    PROCPS_MEMINFO_DELTA_HUGE_TOTAL,      //   s_int
    PROCPS_MEMINFO_DELTA_INACTIVE,        //   s_int
    PROCPS_MEMINFO_DELTA_INACTIVE_ANON,   //   s_int
    PROCPS_MEMINFO_DELTA_INACTIVE_FILE,   //   s_int
    PROCPS_MEMINFO_DELTA_KERNEL_STACK,    //   s_int
    PROCPS_MEMINFO_DELTA_LOCKED,          //   s_int
    PROCPS_MEMINFO_DELTA_MAPPED,          //   s_int
    PROCPS_MEMINFO_DELTA_NFS_UNSTABLE,    //   s_int
    PROCPS_MEMINFO_DELTA_PAGE_TABLES,     //   s_int
    PROCPS_MEMINFO_DELTA_SHARED,          //   s_int
    PROCPS_MEMINFO_DELTA_SLAB,            //   s_int
    PROCPS_MEMINFO_DELTA_SLAB_RECLAIM,    //   s_int
    PROCPS_MEMINFO_DELTA_SLAB_UNRECLAIM,  //   s_int
    PROCPS_MEMINFO_DELTA_TOTAL,           //   s_int
    PROCPS_MEMINFO_DELTA_UNEVICTABLE,     //   s_int
    PROCPS_MEMINFO_DELTA_USED,            //   s_int
    PROCPS_MEMINFO_DELTA_VM_ALLOC_CHUNK,  //   s_int
    PROCPS_MEMINFO_DELTA_VM_ALLOC_TOTAL,  //   s_int
    PROCPS_MEMINFO_DELTA_VM_ALLOC_USED,   //   s_int
    PROCPS_MEMINFO_DELTA_WRITEBACK,       //   s_int
    PROCPS_MEMINFO_DELTA_WRITEBACK_TMP,   //   s_int

    PROCPS_MEMINFO_MEMHI_FREE,            //  ul_int
    PROCPS_MEMINFO_MEMHI_TOTAL,           //  ul_int
    PROCPS_MEMINFO_MEMHI_USED,            //  ul_int

    PROCPS_MEMINFO_MEMLO_FREE,            //  ul_int
    PROCPS_MEMINFO_MEMLO_TOTAL,           //  ul_int
    PROCPS_MEMINFO_MEMLO_USED,            //  ul_int

    PROCPS_MEMINFO_SWAP_CACHED,           //  ul_int
    PROCPS_MEMINFO_SWAP_FREE,             //  ul_int
    PROCPS_MEMINFO_SWAP_TOTAL,            //  ul_int
    PROCPS_MEMINFO_SWAP_USED              //  ul_int
};

struct procps_meminfo;

struct meminfo_result {
    enum meminfo_item item;
    union {
        signed int      s_int;
        unsigned long  ul_int;
    } result;
};

struct meminfo_stack {
    struct meminfo_result *head;
};


#define PROCPS_MEMINFO_VAL(rel_enum,type,stack) \
    stack -> head [ rel_enum ] . result . type


struct procps_meminfo;

int procps_meminfo_new   (struct procps_meminfo **info);
int procps_meminfo_ref   (struct procps_meminfo  *info);
int procps_meminfo_unref (struct procps_meminfo **info);

signed long procps_meminfo_get (
    struct procps_meminfo *info,
    enum meminfo_item item);

struct meminfo_stack *procps_meminfo_select (
    struct procps_meminfo *info,
    enum meminfo_item *items,
    int numitems);

__END_DECLS

#endif
