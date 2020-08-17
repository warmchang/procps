/*
 * libprocps - Library to read proc filesystem
 * Tests for Item_table/enumerator synchronization
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

#include <proc/diskstats.h>
#include <proc/meminfo.h>
#include <proc/pids.h>
#include <proc/slabinfo.h>
#include <proc/stat.h>
#include <proc/vmstat.h>

#include "tests.h"

static int check_diskstats (void *data) {
    struct diskstats_info *ctx = NULL;
    testname = "Itemtable check, diskstats";
    if (procps_diskstats_new(&ctx) < 0) return 0;
    return (procps_diskstats_unref(&ctx) == 0);
}

static int check_meminfo (void *data) {
    struct meminfo_info *ctx = NULL;
    testname = "Itemtable check, meminfo";
    if (procps_meminfo_new(&ctx) < 0) return 0;
    return (procps_meminfo_unref(&ctx) == 0);
}

static int check_pids (void *data) {
    struct pids_info *ctx = NULL;;
    testname = "Itemtable check, pids";
    if (procps_pids_new(&ctx, NULL, 0) < 0) return 0;
    return (procps_pids_unref(&ctx) == 0);
}

static int check_slabinfo (void *data) {
    struct slabinfo_info *ctx = NULL;
    testname = "Itemtable check, slabinfo";
    if (procps_slabinfo_new(&ctx) < 0) return 0;
    return (procps_slabinfo_unref(&ctx) == 0);
}

static int check_stat (void *data) {
    struct stat_info *ctx = NULL;
    testname = "Itemtable check, stat";
    if (procps_stat_new(&ctx) < 0) return 0;
    return (procps_stat_unref(&ctx) == 0);
}

static int check_vmstat (void *data) {
    struct vmstat_info *ctx = NULL;
    testname = "Itemtable check, vmstat";
    if (procps_vmstat_new(&ctx) < 0) return 0;
    return (procps_vmstat_unref(&ctx) == 0);
}

static TestFunction test_funcs[] = {
    check_diskstats,
    check_meminfo,
    check_pids,
    // check_slabinfo, EPERM errors
    check_stat,
    check_vmstat,
    NULL
};

int main (void) {
    return run_tests(test_funcs, NULL);
}
