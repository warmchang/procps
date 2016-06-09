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

#ifndef PROC_STAT_H
#define PROC_STAT_H

__BEGIN_DECLS

enum stat_item {
    PROCPS_STAT_noop,                    //        ( never altered )
    PROCPS_STAT_extra,                   //        ( reset to zero )

    PROCPS_STAT_TIC_ID,                  //   s_int
    PROCPS_STAT_TIC_NUMA_NODE,           //   s_int
    PROCPS_STAT_TIC_USER,                // ull_int
    PROCPS_STAT_TIC_NICE,                // ull_int
    PROCPS_STAT_TIC_SYSTEM,              // ull_int
    PROCPS_STAT_TIC_IDLE,                // ull_int
    PROCPS_STAT_TIC_IOWAIT,              // ull_int
    PROCPS_STAT_TIC_IRQ,                 // ull_int
    PROCPS_STAT_TIC_SOFTIRQ,             // ull_int
    PROCPS_STAT_TIC_STOLEN,              // ull_int
    PROCPS_STAT_TIC_GUEST,               // ull_int
    PROCPS_STAT_TIC_GUEST_NICE,          // ull_int

    PROCPS_STAT_TIC_DELTA_USER,          //  sl_int
    PROCPS_STAT_TIC_DELTA_NICE,          //  sl_int
    PROCPS_STAT_TIC_DELTA_SYSTEM,        //  sl_int
    PROCPS_STAT_TIC_DELTA_IDLE,          //  sl_int
    PROCPS_STAT_TIC_DELTA_IOWAIT,        //  sl_int
    PROCPS_STAT_TIC_DELTA_IRQ,           //  sl_int
    PROCPS_STAT_TIC_DELTA_SOFTIRQ,       //  sl_int
    PROCPS_STAT_TIC_DELTA_STOLEN,        //  sl_int
    PROCPS_STAT_TIC_DELTA_GUEST,         //  sl_int
    PROCPS_STAT_TIC_DELTA_GUEST_NICE,    //  sl_int

    PROCPS_STAT_SYS_CTX_SWITCHES,        //  ul_int
    PROCPS_STAT_SYS_INTERRUPTS,          //  ul_int
    PROCPS_STAT_SYS_PROC_BLOCKED,        //  ul_int
    PROCPS_STAT_SYS_PROC_CREATED,        //  ul_int
    PROCPS_STAT_SYS_PROC_RUNNING,        //  ul_int
    PROCPS_STAT_SYS_TIME_OF_BOOT,        //  ul_int

    PROCPS_STAT_SYS_DELTA_CTX_SWITCHES,  //   s_int
    PROCPS_STAT_SYS_DELTA_INTERRUPTS,    //   s_int
    PROCPS_STAT_SYS_DELTA_PROC_BLOCKED,  //   s_int
    PROCPS_STAT_SYS_DELTA_PROC_CREATED,  //   s_int
    PROCPS_STAT_SYS_DELTA_PROC_RUNNING,  //   s_int
};

enum stat_reap_type {
    STAT_REAP_CPUS_ONLY,
    STAT_REAP_CPUS_AND_NODES
};

struct stat_result {
    enum stat_item item;
    union {
        signed int            s_int;
        signed long          sl_int;
        unsigned long        ul_int;
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


#define PROCPS_STAT_SUMMARY_ID    -11111
#define PROCPS_STAT_NODE_INVALID  -22222

#define PROCPS_STAT_VAL(rel_enum,type,stack) \
    stack -> head [ rel_enum ] . result . type


struct procps_statinfo;

int procps_stat_new   (struct procps_statinfo **info);
int procps_stat_ref   (struct procps_statinfo  *info);
int procps_stat_unref (struct procps_statinfo **info);

signed long long procps_stat_get (
    struct procps_statinfo *info,
    enum stat_item item);

struct stat_reaped *procps_stat_reap (
    struct procps_statinfo *info,
    enum stat_reap_type what,
    enum stat_item *items,
    int numitems);

struct stat_stack *procps_stat_select (
    struct procps_statinfo *info,
    enum stat_item *items,
    int numitems);

__END_DECLS

#endif
