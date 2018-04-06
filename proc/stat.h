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

#ifndef PROCPS_STAT_H
#define PROCPS_STAT_H

#ifdef __cplusplus
extern "C" {
#endif

enum stat_item {
    STAT_noop,                    //        ( never altered )
    STAT_extra,                   //        ( reset to zero )

    STAT_TIC_ID,                  //   s_int
    STAT_TIC_NUMA_NODE,           //   s_int
    STAT_TIC_NUM_CONTRIBUTORS,    //   s_int
    STAT_TIC_USER,                // ull_int
    STAT_TIC_NICE,                // ull_int
    STAT_TIC_SYSTEM,              // ull_int
    STAT_TIC_IDLE,                // ull_int
    STAT_TIC_IOWAIT,              // ull_int
    STAT_TIC_IRQ,                 // ull_int
    STAT_TIC_SOFTIRQ,             // ull_int
    STAT_TIC_STOLEN,              // ull_int
    STAT_TIC_GUEST,               // ull_int
    STAT_TIC_GUEST_NICE,          // ull_int

    STAT_TIC_SUM_TOTAL,           // ull_int
    STAT_TIC_SUM_BUSY,            // ull_int
    STAT_TIC_SUM_IDLE,            // ull_int
    STAT_TIC_SUM_USER,            // ull_int
    STAT_TIC_SUM_SYSTEM,          // ull_int

    STAT_TIC_DELTA_USER,          //  sl_int
    STAT_TIC_DELTA_NICE,          //  sl_int
    STAT_TIC_DELTA_SYSTEM,        //  sl_int
    STAT_TIC_DELTA_IDLE,          //  sl_int
    STAT_TIC_DELTA_IOWAIT,        //  sl_int
    STAT_TIC_DELTA_IRQ,           //  sl_int
    STAT_TIC_DELTA_SOFTIRQ,       //  sl_int
    STAT_TIC_DELTA_STOLEN,        //  sl_int
    STAT_TIC_DELTA_GUEST,         //  sl_int
    STAT_TIC_DELTA_GUEST_NICE,    //  sl_int

    STAT_TIC_DELTA_SUM_TOTAL,     //  sl_int
    STAT_TIC_DELTA_SUM_BUSY,      //  sl_int
    STAT_TIC_DELTA_SUM_IDLE,      //  sl_int
    STAT_TIC_DELTA_SUM_USER,      //  sl_int
    STAT_TIC_DELTA_SUM_SYSTEM,    //  sl_int

    STAT_SYS_CTX_SWITCHES,        //  ul_int
    STAT_SYS_INTERRUPTS,          //  ul_int
    STAT_SYS_PROC_BLOCKED,        //  ul_int
    STAT_SYS_PROC_CREATED,        //  ul_int
    STAT_SYS_PROC_RUNNING,        //  ul_int
    STAT_SYS_TIME_OF_BOOT,        //  ul_int

    STAT_SYS_DELTA_CTX_SWITCHES,  //   s_int
    STAT_SYS_DELTA_INTERRUPTS,    //   s_int
    STAT_SYS_DELTA_PROC_BLOCKED,  //   s_int
    STAT_SYS_DELTA_PROC_CREATED,  //   s_int
    STAT_SYS_DELTA_PROC_RUNNING   //   s_int
};

enum stat_reap_type {
    STAT_REAP_CPUS_ONLY,
    STAT_REAP_CPUS_AND_NODES
};

enum stat_sort_order {
    STAT_SORT_ASCEND   = +1,
    STAT_SORT_DESCEND  = -1
};


struct stat_result {
    enum stat_item item;
    union {
        signed int          s_int;
        signed long         sl_int;
        unsigned long       ul_int;
        unsigned long long  ull_int;
    } result;
};

struct stat_stack {
    struct stat_result *head;
};

struct stat_reap {
    int total;
    struct stat_stack **stacks;
};

struct stat_reaped {
    struct stat_stack *summary;
    struct stat_reap *cpus;
    struct stat_reap *nodes;
};


#define STAT_SUMMARY_ID    -11111
#define STAT_NODE_INVALID  -22222

#define STAT_GET( info, actual_enum, type ) ( { \
    struct stat_result *r = procps_stat_get( info, actual_enum ); \
    r ? r->result . type : 0; } )

#define STAT_VAL( relative_enum, type, stack, info ) \
    stack -> head [ relative_enum ] . result . type


struct stat_info;

int procps_stat_new   (struct stat_info **info);
int procps_stat_ref   (struct stat_info  *info);
int procps_stat_unref (struct stat_info **info);

struct stat_result *procps_stat_get (
    struct stat_info *info,
    enum stat_item item);

struct stat_reaped *procps_stat_reap (
    struct stat_info *info,
    enum stat_reap_type what,
    enum stat_item *items,
    int numitems);

struct stat_stack *procps_stat_select (
    struct stat_info *info,
    enum stat_item *items,
    int numitems);

struct stat_stack **procps_stat_sort (
    struct stat_info *info,
    struct stat_stack *stacks[],
    int numstacked,
    enum stat_item sortitem,
    enum stat_sort_order order);

#ifdef __cplusplus
}
#endif
#endif
