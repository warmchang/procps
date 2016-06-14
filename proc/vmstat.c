/*
 * libprocps - Library to read proc filesystem
 *
 * Copyright (C) 1995 Martin Schulze <joey@infodrom.north.de>
 * Copyright (C) 1996 Charles Blake <cblake@bbn.com>
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

#include <errno.h>
#include <fcntl.h>
#include <search.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <proc/procps-private.h>
#include <proc/vmstat.h>


#define VMSTAT_FILE "/proc/vmstat"

/*
 *  Perhaps someday we'll all learn what is in these fields. But |
 *  that might require available linux documentation progressing |
 *  beyond a state that was acknowledged in the following thread |
 *
 *  http://www.spinics.net/lists/linux-man/msg09096.html
 */
struct vmstat_data {
    unsigned long allocstall;
    unsigned long balloon_deflate;
    unsigned long balloon_inflate;
    unsigned long balloon_migrate;
    unsigned long compact_fail;
    unsigned long compact_free_scanned;
    unsigned long compact_isolated;
    unsigned long compact_migrate_scanned;
    unsigned long compact_stall;
    unsigned long compact_success;
    unsigned long drop_pagecache;
    unsigned long drop_slab;
    unsigned long htlb_buddy_alloc_fail;
    unsigned long htlb_buddy_alloc_success;
    unsigned long kswapd_high_wmark_hit_quickly;
    unsigned long kswapd_inodesteal;
    unsigned long kswapd_low_wmark_hit_quickly;
    unsigned long nr_active_anon;
    unsigned long nr_active_file;
    unsigned long nr_alloc_batch;
    unsigned long nr_anon_pages;
    unsigned long nr_anon_transparent_hugepages;
    unsigned long nr_bounce;
    unsigned long nr_dirtied;
    unsigned long nr_dirty;
    unsigned long nr_dirty_background_threshold;
    unsigned long nr_dirty_threshold;
    unsigned long nr_file_pages;
    unsigned long nr_free_cma;
    unsigned long nr_free_pages;
    unsigned long nr_inactive_anon;
    unsigned long nr_inactive_file;
    unsigned long nr_isolated_anon;
    unsigned long nr_isolated_file;
    unsigned long nr_kernel_stack;
    unsigned long nr_mapped;
    unsigned long nr_mlock;
    unsigned long nr_pages_scanned;              // note: alphabetic WHEN capitalized (as w/ enum)
    unsigned long nr_page_table_pages;           // note: alphabetic WHEN capitalized (as w/ enum)
    unsigned long nr_shmem;
    unsigned long nr_slab_reclaimable;
    unsigned long nr_slab_unreclaimable;
    unsigned long nr_unevictable;
    unsigned long nr_unstable;
    unsigned long nr_vmscan_immediate_reclaim;
    unsigned long nr_vmscan_write;
    unsigned long nr_writeback;
    unsigned long nr_writeback_temp;
    unsigned long nr_written;
    unsigned long numa_foreign;
    unsigned long numa_hint_faults;
    unsigned long numa_hint_faults_local;
    unsigned long numa_hit;
    unsigned long numa_huge_pte_updates;
    unsigned long numa_interleave;
    unsigned long numa_local;
    unsigned long numa_miss;
    unsigned long numa_other;
    unsigned long numa_pages_migrated;
    unsigned long numa_pte_updates;
    unsigned long pageoutrun;
    unsigned long pgactivate;
    unsigned long pgalloc_dma;
    unsigned long pgalloc_dma32;
    unsigned long pgalloc_movable;
    unsigned long pgalloc_normal;
    unsigned long pgdeactivate;
    unsigned long pgfault;
    unsigned long pgfree;
    unsigned long pginodesteal;
    unsigned long pgmajfault;
    unsigned long pgmigrate_fail;
    unsigned long pgmigrate_success;
    unsigned long pgpgin;
    unsigned long pgpgout;
    unsigned long pgrefill_dma;
    unsigned long pgrefill_dma32;
    unsigned long pgrefill_movable;
    unsigned long pgrefill_normal;
    unsigned long pgrotated;
    unsigned long pgscan_direct_dma;
    unsigned long pgscan_direct_dma32;
    unsigned long pgscan_direct_movable;
    unsigned long pgscan_direct_normal;
    unsigned long pgscan_direct_throttle;
    unsigned long pgscan_kswapd_dma;
    unsigned long pgscan_kswapd_dma32;
    unsigned long pgscan_kswapd_movable;
    unsigned long pgscan_kswapd_normal;
    unsigned long pgsteal_direct_dma;
    unsigned long pgsteal_direct_dma32;
    unsigned long pgsteal_direct_movable;
    unsigned long pgsteal_direct_normal;
    unsigned long pgsteal_kswapd_dma;
    unsigned long pgsteal_kswapd_dma32;
    unsigned long pgsteal_kswapd_movable;
    unsigned long pgsteal_kswapd_normal;
    unsigned long pswpin;
    unsigned long pswpout;
    unsigned long slabs_scanned;
    unsigned long thp_collapse_alloc;
    unsigned long thp_collapse_alloc_failed;
    unsigned long thp_fault_alloc;
    unsigned long thp_fault_fallback;
    unsigned long thp_split;
    unsigned long thp_zero_page_alloc;
    unsigned long thp_zero_page_alloc_failed;
    unsigned long unevictable_pgs_cleared;
    unsigned long unevictable_pgs_culled;
    unsigned long unevictable_pgs_mlocked;
    unsigned long unevictable_pgs_munlocked;
    unsigned long unevictable_pgs_rescued;
    unsigned long unevictable_pgs_scanned;
    unsigned long unevictable_pgs_stranded;
    unsigned long workingset_activate;
    unsigned long workingset_nodereclaim;
    unsigned long workingset_refault;
    unsigned long zone_reclaim_failed;
};

struct vmstat_hist {
    struct vmstat_data new;
    struct vmstat_data old;
};

struct stacks_extent {
    int ext_numstacks;
    struct stacks_extent *next;
    struct vmstat_stack **stacks;
};

struct procps_vmstat {
    int refcount;
    int vmstat_fd;
    int vmstat_was_read;
    int dirty_stacks;
    struct vmstat_hist hist;
    int numitems;
    enum vmstat_item *items;
    struct stacks_extent *extents;
    struct hsearch_data hashtab;
};


// ___ Results 'Set' Support ||||||||||||||||||||||||||||||||||||||||||||||||||

#define setNAME(e) set_results_ ## e
#define setDECL(e) static void setNAME(e) \
    (struct vmstat_result *R, struct vmstat_hist *H)

// regular assignment
#define REG_set(e,x) setDECL(e) { R->result.ul_int = H->new . x; }
// delta assignment
#define HST_set(e,x) setDECL(e) { R->result.sl_int = ( H->new . x - H->old. x ); }

setDECL(noop)   { (void)R; (void)H; }
setDECL(extra)  { (void)R; (void)H; }

REG_set(ALLOCSTALL,                           allocstall)
REG_set(BALLOON_DEFLATE,                      balloon_deflate)
REG_set(BALLOON_INFLATE,                      balloon_inflate)
REG_set(BALLOON_MIGRATE,                      balloon_migrate)
REG_set(COMPACT_FAIL,                         compact_fail)
REG_set(COMPACT_FREE_SCANNED,                 compact_free_scanned)
REG_set(COMPACT_ISOLATED,                     compact_isolated)
REG_set(COMPACT_MIGRATE_SCANNED,              compact_migrate_scanned)
REG_set(COMPACT_STALL,                        compact_stall)
REG_set(COMPACT_SUCCESS,                      compact_success)
REG_set(DROP_PAGECACHE,                       drop_pagecache)
REG_set(DROP_SLAB,                            drop_slab)
REG_set(HTLB_BUDDY_ALLOC_FAIL,                htlb_buddy_alloc_fail)
REG_set(HTLB_BUDDY_ALLOC_SUCCESS,             htlb_buddy_alloc_success)
REG_set(KSWAPD_HIGH_WMARK_HIT_QUICKLY,        kswapd_high_wmark_hit_quickly)
REG_set(KSWAPD_INODESTEAL,                    kswapd_inodesteal)
REG_set(KSWAPD_LOW_WMARK_HIT_QUICKLY,         kswapd_low_wmark_hit_quickly)
REG_set(NR_ACTIVE_ANON,                       nr_active_anon)
REG_set(NR_ACTIVE_FILE,                       nr_active_file)
REG_set(NR_ALLOC_BATCH,                       nr_alloc_batch)
REG_set(NR_ANON_PAGES,                        nr_anon_pages)
REG_set(NR_ANON_TRANSPARENT_HUGEPAGES,        nr_anon_transparent_hugepages)
REG_set(NR_BOUNCE,                            nr_bounce)
REG_set(NR_DIRTIED,                           nr_dirtied)
REG_set(NR_DIRTY,                             nr_dirty)
REG_set(NR_DIRTY_BACKGROUND_THRESHOLD,        nr_dirty_background_threshold)
REG_set(NR_DIRTY_THRESHOLD,                   nr_dirty_threshold)
REG_set(NR_FILE_PAGES,                        nr_file_pages)
REG_set(NR_FREE_CMA,                          nr_free_cma)
REG_set(NR_FREE_PAGES,                        nr_free_pages)
REG_set(NR_INACTIVE_ANON,                     nr_inactive_anon)
REG_set(NR_INACTIVE_FILE,                     nr_inactive_file)
REG_set(NR_ISOLATED_ANON,                     nr_isolated_anon)
REG_set(NR_ISOLATED_FILE,                     nr_isolated_file)
REG_set(NR_KERNEL_STACK,                      nr_kernel_stack)
REG_set(NR_MAPPED,                            nr_mapped)
REG_set(NR_MLOCK,                             nr_mlock)
REG_set(NR_PAGES_SCANNED,                     nr_pages_scanned)
REG_set(NR_PAGE_TABLE_PAGES,                  nr_page_table_pages)
REG_set(NR_SHMEM,                             nr_shmem)
REG_set(NR_SLAB_RECLAIMABLE,                  nr_slab_reclaimable)
REG_set(NR_SLAB_UNRECLAIMABLE,                nr_slab_unreclaimable)
REG_set(NR_UNEVICTABLE,                       nr_unevictable)
REG_set(NR_UNSTABLE,                          nr_unstable)
REG_set(NR_VMSCAN_IMMEDIATE_RECLAIM,          nr_vmscan_immediate_reclaim)
REG_set(NR_VMSCAN_WRITE,                      nr_vmscan_write)
REG_set(NR_WRITEBACK,                         nr_writeback)
REG_set(NR_WRITEBACK_TEMP,                    nr_writeback_temp)
REG_set(NR_WRITTEN,                           nr_written)
REG_set(NUMA_FOREIGN,                         numa_foreign)
REG_set(NUMA_HINT_FAULTS,                     numa_hint_faults)
REG_set(NUMA_HINT_FAULTS_LOCAL,               numa_hint_faults_local)
REG_set(NUMA_HIT,                             numa_hit)
REG_set(NUMA_HUGE_PTE_UPDATES,                numa_huge_pte_updates)
REG_set(NUMA_INTERLEAVE,                      numa_interleave)
REG_set(NUMA_LOCAL,                           numa_local)
REG_set(NUMA_MISS,                            numa_miss)
REG_set(NUMA_OTHER,                           numa_other)
REG_set(NUMA_PAGES_MIGRATED,                  numa_pages_migrated)
REG_set(NUMA_PTE_UPDATES,                     numa_pte_updates)
REG_set(PAGEOUTRUN,                           pageoutrun)
REG_set(PGACTIVATE,                           pgactivate)
REG_set(PGALLOC_DMA,                          pgalloc_dma)
REG_set(PGALLOC_DMA32,                        pgalloc_dma32)
REG_set(PGALLOC_MOVABLE,                      pgalloc_movable)
REG_set(PGALLOC_NORMAL,                       pgalloc_normal)
REG_set(PGDEACTIVATE,                         pgdeactivate)
REG_set(PGFAULT,                              pgfault)
REG_set(PGFREE,                               pgfree)
REG_set(PGINODESTEAL,                         pginodesteal)
REG_set(PGMAJFAULT,                           pgmajfault)
REG_set(PGMIGRATE_FAIL,                       pgmigrate_fail)
REG_set(PGMIGRATE_SUCCESS,                    pgmigrate_success)
REG_set(PGPGIN,                               pgpgin)
REG_set(PGPGOUT,                              pgpgout)
REG_set(PGREFILL_DMA,                         pgrefill_dma)
REG_set(PGREFILL_DMA32,                       pgrefill_dma32)
REG_set(PGREFILL_MOVABLE,                     pgrefill_movable)
REG_set(PGREFILL_NORMAL,                      pgrefill_normal)
REG_set(PGROTATED,                            pgrotated)
REG_set(PGSCAN_DIRECT_DMA,                    pgscan_direct_dma)
REG_set(PGSCAN_DIRECT_DMA32,                  pgscan_direct_dma32)
REG_set(PGSCAN_DIRECT_MOVABLE,                pgscan_direct_movable)
REG_set(PGSCAN_DIRECT_NORMAL,                 pgscan_direct_normal)
REG_set(PGSCAN_DIRECT_THROTTLE,               pgscan_direct_throttle)
REG_set(PGSCAN_KSWAPD_DMA,                    pgscan_kswapd_dma)
REG_set(PGSCAN_KSWAPD_DMA32,                  pgscan_kswapd_dma32)
REG_set(PGSCAN_KSWAPD_MOVEABLE,               pgscan_kswapd_movable)
REG_set(PGSCAN_KSWAPD_NORMAL,                 pgscan_kswapd_normal)
REG_set(PGSTEAL_DIRECT_DMA,                   pgsteal_direct_dma)
REG_set(PGSTEAL_DIRECT_DMA32,                 pgsteal_direct_dma32)
REG_set(PGSTEAL_DIRECT_MOVABLE,               pgsteal_direct_movable)
REG_set(PGSTEAL_DIRECT_NORMAL,                pgsteal_direct_normal)
REG_set(PGSTEAL_KSWAPD_DMA,                   pgsteal_kswapd_dma)
REG_set(PGSTEAL_KSWAPD_DMA32,                 pgsteal_kswapd_dma32)
REG_set(PGSTEAL_KSWAPD_MOVABLE,               pgsteal_kswapd_movable)
REG_set(PGSTEAL_KSWAPD_NORMAL,                pgsteal_kswapd_normal)
REG_set(PSWPIN,                               pswpin)
REG_set(PSWPOUT,                              pswpout)
REG_set(SLABS_SCANNED,                        slabs_scanned)
REG_set(THP_COLLAPSE_ALLOC,                   thp_collapse_alloc)
REG_set(THP_COLLAPSE_ALLOC_FAILED,            thp_collapse_alloc_failed)
REG_set(THP_FAULT_ALLOC,                      thp_fault_alloc)
REG_set(THP_FAULT_FALLBACK,                   thp_fault_fallback)
REG_set(THP_SPLIT,                            thp_split)
REG_set(THP_ZERO_PAGE_ALLOC,                  thp_zero_page_alloc)
REG_set(THP_ZERO_PAGE_ALLOC_FAILED,           thp_zero_page_alloc_failed)
REG_set(UNEVICTABLE_PGS_CLEARED,              unevictable_pgs_cleared)
REG_set(UNEVICTABLE_PGS_CULLED,               unevictable_pgs_culled)
REG_set(UNEVICTABLE_PGS_MLOCKED,              unevictable_pgs_mlocked)
REG_set(UNEVICTABLE_PGS_MUNLOCKED,            unevictable_pgs_munlocked)
REG_set(UNEVICTABLE_PGS_RESCUED,              unevictable_pgs_rescued)
REG_set(UNEVICTABLE_PGS_SCANNED,              unevictable_pgs_scanned)
REG_set(UNEVICTABLE_PGS_STRANDED,             unevictable_pgs_stranded)
REG_set(WORKINGSET_ACTIVATE,                  workingset_activate)
REG_set(WORKINGSET_NODERECLAIM,               workingset_nodereclaim)
REG_set(WORKINGSET_REFAULT,                   workingset_refault)
REG_set(ZONE_RECLAIM_FAILED,                  zone_reclaim_failed)

HST_set(DELTA_ALLOCSTALL,                     allocstall)
HST_set(DELTA_BALLOON_DEFLATE,                balloon_deflate)
HST_set(DELTA_BALLOON_INFLATE,                balloon_inflate)
HST_set(DELTA_BALLOON_MIGRATE,                balloon_migrate)
HST_set(DELTA_COMPACT_FAIL,                   compact_fail)
HST_set(DELTA_COMPACT_FREE_SCANNED,           compact_free_scanned)
HST_set(DELTA_COMPACT_ISOLATED,               compact_isolated)
HST_set(DELTA_COMPACT_MIGRATE_SCANNED,        compact_migrate_scanned)
HST_set(DELTA_COMPACT_STALL,                  compact_stall)
HST_set(DELTA_COMPACT_SUCCESS,                compact_success)
HST_set(DELTA_DROP_PAGECACHE,                 drop_pagecache)
HST_set(DELTA_DROP_SLAB,                      drop_slab)
HST_set(DELTA_HTLB_BUDDY_ALLOC_FAIL,          htlb_buddy_alloc_fail)
HST_set(DELTA_HTLB_BUDDY_ALLOC_SUCCESS,       htlb_buddy_alloc_success)
HST_set(DELTA_KSWAPD_HIGH_WMARK_HIT_QUICKLY,  kswapd_high_wmark_hit_quickly)
HST_set(DELTA_KSWAPD_INODESTEAL,              kswapd_inodesteal)
HST_set(DELTA_KSWAPD_LOW_WMARK_HIT_QUICKLY,   kswapd_low_wmark_hit_quickly)
HST_set(DELTA_NR_ACTIVE_ANON,                 nr_active_anon)
HST_set(DELTA_NR_ACTIVE_FILE,                 nr_active_file)
HST_set(DELTA_NR_ALLOC_BATCH,                 nr_alloc_batch)
HST_set(DELTA_NR_ANON_PAGES,                  nr_anon_pages)
HST_set(DELTA_NR_ANON_TRANSPARENT_HUGEPAGES,  nr_anon_transparent_hugepages)
HST_set(DELTA_NR_BOUNCE,                      nr_bounce)
HST_set(DELTA_NR_DIRTIED,                     nr_dirtied)
HST_set(DELTA_NR_DIRTY,                       nr_dirty)
HST_set(DELTA_NR_DIRTY_BACKGROUND_THRESHOLD,  nr_dirty_background_threshold)
HST_set(DELTA_NR_DIRTY_THRESHOLD,             nr_dirty_threshold)
HST_set(DELTA_NR_FILE_PAGES,                  nr_file_pages)
HST_set(DELTA_NR_FREE_CMA,                    nr_free_cma)
HST_set(DELTA_NR_FREE_PAGES,                  nr_free_pages)
HST_set(DELTA_NR_INACTIVE_ANON,               nr_inactive_anon)
HST_set(DELTA_NR_INACTIVE_FILE,               nr_inactive_file)
HST_set(DELTA_NR_ISOLATED_ANON,               nr_isolated_anon)
HST_set(DELTA_NR_ISOLATED_FILE,               nr_isolated_file)
HST_set(DELTA_NR_KERNEL_STACK,                nr_kernel_stack)
HST_set(DELTA_NR_MAPPED,                      nr_mapped)
HST_set(DELTA_NR_MLOCK,                       nr_mlock)
HST_set(DELTA_NR_PAGES_SCANNED,               nr_pages_scanned)
HST_set(DELTA_NR_PAGE_TABLE_PAGES,            nr_page_table_pages)
HST_set(DELTA_NR_SHMEM,                       nr_shmem)
HST_set(DELTA_NR_SLAB_RECLAIMABLE,            nr_slab_reclaimable)
HST_set(DELTA_NR_SLAB_UNRECLAIMABLE,          nr_slab_unreclaimable)
HST_set(DELTA_NR_UNEVICTABLE,                 nr_unevictable)
HST_set(DELTA_NR_UNSTABLE,                    nr_unstable)
HST_set(DELTA_NR_VMSCAN_IMMEDIATE_RECLAIM,    nr_vmscan_immediate_reclaim)
HST_set(DELTA_NR_VMSCAN_WRITE,                nr_vmscan_write)
HST_set(DELTA_NR_WRITEBACK,                   nr_writeback)
HST_set(DELTA_NR_WRITEBACK_TEMP,              nr_writeback_temp)
HST_set(DELTA_NR_WRITTEN,                     nr_written)
HST_set(DELTA_NUMA_FOREIGN,                   numa_foreign)
HST_set(DELTA_NUMA_HINT_FAULTS,               numa_hint_faults)
HST_set(DELTA_NUMA_HINT_FAULTS_LOCAL,         numa_hint_faults_local)
HST_set(DELTA_NUMA_HIT,                       numa_hit)
HST_set(DELTA_NUMA_HUGE_PTE_UPDATES,          numa_huge_pte_updates)
HST_set(DELTA_NUMA_INTERLEAVE,                numa_interleave)
HST_set(DELTA_NUMA_LOCAL,                     numa_local)
HST_set(DELTA_NUMA_MISS,                      numa_miss)
HST_set(DELTA_NUMA_OTHER,                     numa_other)
HST_set(DELTA_NUMA_PAGES_MIGRATED,            numa_pages_migrated)
HST_set(DELTA_NUMA_PTE_UPDATES,               numa_pte_updates)
HST_set(DELTA_PAGEOUTRUN,                     pageoutrun)
HST_set(DELTA_PGACTIVATE,                     pgactivate)
HST_set(DELTA_PGALLOC_DMA,                    pgalloc_dma)
HST_set(DELTA_PGALLOC_DMA32,                  pgalloc_dma32)
HST_set(DELTA_PGALLOC_MOVABLE,                pgalloc_movable)
HST_set(DELTA_PGALLOC_NORMAL,                 pgalloc_normal)
HST_set(DELTA_PGDEACTIVATE,                   pgdeactivate)
HST_set(DELTA_PGFAULT,                        pgfault)
HST_set(DELTA_PGFREE,                         pgfree)
HST_set(DELTA_PGINODESTEAL,                   pginodesteal)
HST_set(DELTA_PGMAJFAULT,                     pgmajfault)
HST_set(DELTA_PGMIGRATE_FAIL,                 pgmigrate_fail)
HST_set(DELTA_PGMIGRATE_SUCCESS,              pgmigrate_success)
HST_set(DELTA_PGPGIN,                         pgpgin)
HST_set(DELTA_PGPGOUT,                        pgpgout)
HST_set(DELTA_PGREFILL_DMA,                   pgrefill_dma)
HST_set(DELTA_PGREFILL_DMA32,                 pgrefill_dma32)
HST_set(DELTA_PGREFILL_MOVABLE,               pgrefill_movable)
HST_set(DELTA_PGREFILL_NORMAL,                pgrefill_normal)
HST_set(DELTA_PGROTATED,                      pgrotated)
HST_set(DELTA_PGSCAN_DIRECT_DMA,              pgscan_direct_dma)
HST_set(DELTA_PGSCAN_DIRECT_DMA32,            pgscan_direct_dma32)
HST_set(DELTA_PGSCAN_DIRECT_MOVABLE,          pgscan_direct_movable)
HST_set(DELTA_PGSCAN_DIRECT_NORMAL,           pgscan_direct_normal)
HST_set(DELTA_PGSCAN_DIRECT_THROTTLE,         pgscan_direct_throttle)
HST_set(DELTA_PGSCAN_KSWAPD_DMA,              pgscan_kswapd_dma)
HST_set(DELTA_PGSCAN_KSWAPD_DMA32,            pgscan_kswapd_dma32)
HST_set(DELTA_PGSCAN_KSWAPD_MOVEABLE,         pgscan_kswapd_movable)
HST_set(DELTA_PGSCAN_KSWAPD_NORMAL,           pgscan_kswapd_normal)
HST_set(DELTA_PGSTEAL_DIRECT_DMA,             pgsteal_direct_dma)
HST_set(DELTA_PGSTEAL_DIRECT_DMA32,           pgsteal_direct_dma32)
HST_set(DELTA_PGSTEAL_DIRECT_MOVABLE,         pgsteal_direct_movable)
HST_set(DELTA_PGSTEAL_DIRECT_NORMAL,          pgsteal_direct_normal)
HST_set(DELTA_PGSTEAL_KSWAPD_DMA,             pgsteal_kswapd_dma)
HST_set(DELTA_PGSTEAL_KSWAPD_DMA32,           pgsteal_kswapd_dma32)
HST_set(DELTA_PGSTEAL_KSWAPD_MOVABLE,         pgsteal_kswapd_movable)
HST_set(DELTA_PGSTEAL_KSWAPD_NORMAL,          pgsteal_kswapd_normal)
HST_set(DELTA_PSWPIN,                         pswpin)
HST_set(DELTA_PSWPOUT,                        pswpout)
HST_set(DELTA_SLABS_SCANNED,                  slabs_scanned)
HST_set(DELTA_THP_COLLAPSE_ALLOC,             thp_collapse_alloc)
HST_set(DELTA_THP_COLLAPSE_ALLOC_FAILED,      thp_collapse_alloc_failed)
HST_set(DELTA_THP_FAULT_ALLOC,                thp_fault_alloc)
HST_set(DELTA_THP_FAULT_FALLBACK,             thp_fault_fallback)
HST_set(DELTA_THP_SPLIT,                      thp_split)
HST_set(DELTA_THP_ZERO_PAGE_ALLOC,            thp_zero_page_alloc)
HST_set(DELTA_THP_ZERO_PAGE_ALLOC_FAILED,     thp_zero_page_alloc_failed)
HST_set(DELTA_UNEVICTABLE_PGS_CLEARED,        unevictable_pgs_cleared)
HST_set(DELTA_UNEVICTABLE_PGS_CULLED,         unevictable_pgs_culled)
HST_set(DELTA_UNEVICTABLE_PGS_MLOCKED,        unevictable_pgs_mlocked)
HST_set(DELTA_UNEVICTABLE_PGS_MUNLOCKED,      unevictable_pgs_munlocked)
HST_set(DELTA_UNEVICTABLE_PGS_RESCUED,        unevictable_pgs_rescued)
HST_set(DELTA_UNEVICTABLE_PGS_SCANNED,        unevictable_pgs_scanned)
HST_set(DELTA_UNEVICTABLE_PGS_STRANDED,       unevictable_pgs_stranded)
HST_set(DELTA_WORKINGSET_ACTIVATE,            workingset_activate)
HST_set(DELTA_WORKINGSET_NODERECLAIM,         workingset_nodereclaim)
HST_set(DELTA_WORKINGSET_REFAULT,             workingset_refault)
HST_set(DELTA_ZONE_RECLAIM_FAILED,            zone_reclaim_failed)

#undef setDECL
#undef REG_set
#undef HST_set


// ___ Results 'Get' Support ||||||||||||||||||||||||||||||||||||||||||||||||||

#define getNAME(e) get_results_ ## e
#define getDECL(e) static signed long getNAME(e) \
    (struct procps_vmstat *I)

// regular get
#define REG_get(e,x) getDECL(e) { return I->hist.new. x; }
// delta get
#define HST_get(e,x) getDECL(e) { return ( I->hist.new. x - I->hist.old. x ); }

getDECL(noop)   { (void)I; return 0; }
getDECL(extra)  { (void)I; return 0; }

REG_get(ALLOCSTALL,                           allocstall)
REG_get(BALLOON_DEFLATE,                      balloon_deflate)
REG_get(BALLOON_INFLATE,                      balloon_inflate)
REG_get(BALLOON_MIGRATE,                      balloon_migrate)
REG_get(COMPACT_FAIL,                         compact_fail)
REG_get(COMPACT_FREE_SCANNED,                 compact_free_scanned)
REG_get(COMPACT_ISOLATED,                     compact_isolated)
REG_get(COMPACT_MIGRATE_SCANNED,              compact_migrate_scanned)
REG_get(COMPACT_STALL,                        compact_stall)
REG_get(COMPACT_SUCCESS,                      compact_success)
REG_get(DROP_PAGECACHE,                       drop_pagecache)
REG_get(DROP_SLAB,                            drop_slab)
REG_get(HTLB_BUDDY_ALLOC_FAIL,                htlb_buddy_alloc_fail)
REG_get(HTLB_BUDDY_ALLOC_SUCCESS,             htlb_buddy_alloc_success)
REG_get(KSWAPD_HIGH_WMARK_HIT_QUICKLY,        kswapd_high_wmark_hit_quickly)
REG_get(KSWAPD_INODESTEAL,                    kswapd_inodesteal)
REG_get(KSWAPD_LOW_WMARK_HIT_QUICKLY,         kswapd_low_wmark_hit_quickly)
REG_get(NR_ACTIVE_ANON,                       nr_active_anon)
REG_get(NR_ACTIVE_FILE,                       nr_active_file)
REG_get(NR_ALLOC_BATCH,                       nr_alloc_batch)
REG_get(NR_ANON_PAGES,                        nr_anon_pages)
REG_get(NR_ANON_TRANSPARENT_HUGEPAGES,        nr_anon_transparent_hugepages)
REG_get(NR_BOUNCE,                            nr_bounce)
REG_get(NR_DIRTIED,                           nr_dirtied)
REG_get(NR_DIRTY,                             nr_dirty)
REG_get(NR_DIRTY_BACKGROUND_THRESHOLD,        nr_dirty_background_threshold)
REG_get(NR_DIRTY_THRESHOLD,                   nr_dirty_threshold)
REG_get(NR_FILE_PAGES,                        nr_file_pages)
REG_get(NR_FREE_CMA,                          nr_free_cma)
REG_get(NR_FREE_PAGES,                        nr_free_pages)
REG_get(NR_INACTIVE_ANON,                     nr_inactive_anon)
REG_get(NR_INACTIVE_FILE,                     nr_inactive_file)
REG_get(NR_ISOLATED_ANON,                     nr_isolated_anon)
REG_get(NR_ISOLATED_FILE,                     nr_isolated_file)
REG_get(NR_KERNEL_STACK,                      nr_kernel_stack)
REG_get(NR_MAPPED,                            nr_mapped)
REG_get(NR_MLOCK,                             nr_mlock)
REG_get(NR_PAGES_SCANNED,                     nr_pages_scanned)
REG_get(NR_PAGE_TABLE_PAGES,                  nr_page_table_pages)
REG_get(NR_SHMEM,                             nr_shmem)
REG_get(NR_SLAB_RECLAIMABLE,                  nr_slab_reclaimable)
REG_get(NR_SLAB_UNRECLAIMABLE,                nr_slab_unreclaimable)
REG_get(NR_UNEVICTABLE,                       nr_unevictable)
REG_get(NR_UNSTABLE,                          nr_unstable)
REG_get(NR_VMSCAN_IMMEDIATE_RECLAIM,          nr_vmscan_immediate_reclaim)
REG_get(NR_VMSCAN_WRITE,                      nr_vmscan_write)
REG_get(NR_WRITEBACK,                         nr_writeback)
REG_get(NR_WRITEBACK_TEMP,                    nr_writeback_temp)
REG_get(NR_WRITTEN,                           nr_written)
REG_get(NUMA_FOREIGN,                         numa_foreign)
REG_get(NUMA_HINT_FAULTS,                     numa_hint_faults)
REG_get(NUMA_HINT_FAULTS_LOCAL,               numa_hint_faults_local)
REG_get(NUMA_HIT,                             numa_hit)
REG_get(NUMA_HUGE_PTE_UPDATES,                numa_huge_pte_updates)
REG_get(NUMA_INTERLEAVE,                      numa_interleave)
REG_get(NUMA_LOCAL,                           numa_local)
REG_get(NUMA_MISS,                            numa_miss)
REG_get(NUMA_OTHER,                           numa_other)
REG_get(NUMA_PAGES_MIGRATED,                  numa_pages_migrated)
REG_get(NUMA_PTE_UPDATES,                     numa_pte_updates)
REG_get(PAGEOUTRUN,                           pageoutrun)
REG_get(PGACTIVATE,                           pgactivate)
REG_get(PGALLOC_DMA,                          pgalloc_dma)
REG_get(PGALLOC_DMA32,                        pgalloc_dma32)
REG_get(PGALLOC_MOVABLE,                      pgalloc_movable)
REG_get(PGALLOC_NORMAL,                       pgalloc_normal)
REG_get(PGDEACTIVATE,                         pgdeactivate)
REG_get(PGFAULT,                              pgfault)
REG_get(PGFREE,                               pgfree)
REG_get(PGINODESTEAL,                         pginodesteal)
REG_get(PGMAJFAULT,                           pgmajfault)
REG_get(PGMIGRATE_FAIL,                       pgmigrate_fail)
REG_get(PGMIGRATE_SUCCESS,                    pgmigrate_success)
REG_get(PGPGIN,                               pgpgin)
REG_get(PGPGOUT,                              pgpgout)
REG_get(PGREFILL_DMA,                         pgrefill_dma)
REG_get(PGREFILL_DMA32,                       pgrefill_dma32)
REG_get(PGREFILL_MOVABLE,                     pgrefill_movable)
REG_get(PGREFILL_NORMAL,                      pgrefill_normal)
REG_get(PGROTATED,                            pgrotated)
REG_get(PGSCAN_DIRECT_DMA,                    pgscan_direct_dma)
REG_get(PGSCAN_DIRECT_DMA32,                  pgscan_direct_dma32)
REG_get(PGSCAN_DIRECT_MOVABLE,                pgscan_direct_movable)
REG_get(PGSCAN_DIRECT_NORMAL,                 pgscan_direct_normal)
REG_get(PGSCAN_DIRECT_THROTTLE,               pgscan_direct_throttle)
REG_get(PGSCAN_KSWAPD_DMA,                    pgscan_kswapd_dma)
REG_get(PGSCAN_KSWAPD_DMA32,                  pgscan_kswapd_dma32)
REG_get(PGSCAN_KSWAPD_MOVEABLE,               pgscan_kswapd_movable)
REG_get(PGSCAN_KSWAPD_NORMAL,                 pgscan_kswapd_normal)
REG_get(PGSTEAL_DIRECT_DMA,                   pgsteal_direct_dma)
REG_get(PGSTEAL_DIRECT_DMA32,                 pgsteal_direct_dma32)
REG_get(PGSTEAL_DIRECT_MOVABLE,               pgsteal_direct_movable)
REG_get(PGSTEAL_DIRECT_NORMAL,                pgsteal_direct_normal)
REG_get(PGSTEAL_KSWAPD_DMA,                   pgsteal_kswapd_dma)
REG_get(PGSTEAL_KSWAPD_DMA32,                 pgsteal_kswapd_dma32)
REG_get(PGSTEAL_KSWAPD_MOVABLE,               pgsteal_kswapd_movable)
REG_get(PGSTEAL_KSWAPD_NORMAL,                pgsteal_kswapd_normal)
REG_get(PSWPIN,                               pswpin)
REG_get(PSWPOUT,                              pswpout)
REG_get(SLABS_SCANNED,                        slabs_scanned)
REG_get(THP_COLLAPSE_ALLOC,                   thp_collapse_alloc)
REG_get(THP_COLLAPSE_ALLOC_FAILED,            thp_collapse_alloc_failed)
REG_get(THP_FAULT_ALLOC,                      thp_fault_alloc)
REG_get(THP_FAULT_FALLBACK,                   thp_fault_fallback)
REG_get(THP_SPLIT,                            thp_split)
REG_get(THP_ZERO_PAGE_ALLOC,                  thp_zero_page_alloc)
REG_get(THP_ZERO_PAGE_ALLOC_FAILED,           thp_zero_page_alloc_failed)
REG_get(UNEVICTABLE_PGS_CLEARED,              unevictable_pgs_cleared)
REG_get(UNEVICTABLE_PGS_CULLED,               unevictable_pgs_culled)
REG_get(UNEVICTABLE_PGS_MLOCKED,              unevictable_pgs_mlocked)
REG_get(UNEVICTABLE_PGS_MUNLOCKED,            unevictable_pgs_munlocked)
REG_get(UNEVICTABLE_PGS_RESCUED,              unevictable_pgs_rescued)
REG_get(UNEVICTABLE_PGS_SCANNED,              unevictable_pgs_scanned)
REG_get(UNEVICTABLE_PGS_STRANDED,             unevictable_pgs_stranded)
REG_get(WORKINGSET_ACTIVATE,                  workingset_activate)
REG_get(WORKINGSET_NODERECLAIM,               workingset_nodereclaim)
REG_get(WORKINGSET_REFAULT,                   workingset_refault)
REG_get(ZONE_RECLAIM_FAILED,                  zone_reclaim_failed)

HST_get(DELTA_ALLOCSTALL,                     allocstall)
HST_get(DELTA_BALLOON_DEFLATE,                balloon_deflate)
HST_get(DELTA_BALLOON_INFLATE,                balloon_inflate)
HST_get(DELTA_BALLOON_MIGRATE,                balloon_migrate)
HST_get(DELTA_COMPACT_FAIL,                   compact_fail)
HST_get(DELTA_COMPACT_FREE_SCANNED,           compact_free_scanned)
HST_get(DELTA_COMPACT_ISOLATED,               compact_isolated)
HST_get(DELTA_COMPACT_MIGRATE_SCANNED,        compact_migrate_scanned)
HST_get(DELTA_COMPACT_STALL,                  compact_stall)
HST_get(DELTA_COMPACT_SUCCESS,                compact_success)
HST_get(DELTA_DROP_PAGECACHE,                 drop_pagecache)
HST_get(DELTA_DROP_SLAB,                      drop_slab)
HST_get(DELTA_HTLB_BUDDY_ALLOC_FAIL,          htlb_buddy_alloc_fail)
HST_get(DELTA_HTLB_BUDDY_ALLOC_SUCCESS,       htlb_buddy_alloc_success)
HST_get(DELTA_KSWAPD_HIGH_WMARK_HIT_QUICKLY,  kswapd_high_wmark_hit_quickly)
HST_get(DELTA_KSWAPD_INODESTEAL,              kswapd_inodesteal)
HST_get(DELTA_KSWAPD_LOW_WMARK_HIT_QUICKLY,   kswapd_low_wmark_hit_quickly)
HST_get(DELTA_NR_ACTIVE_ANON,                 nr_active_anon)
HST_get(DELTA_NR_ACTIVE_FILE,                 nr_active_file)
HST_get(DELTA_NR_ALLOC_BATCH,                 nr_alloc_batch)
HST_get(DELTA_NR_ANON_PAGES,                  nr_anon_pages)
HST_get(DELTA_NR_ANON_TRANSPARENT_HUGEPAGES,  nr_anon_transparent_hugepages)
HST_get(DELTA_NR_BOUNCE,                      nr_bounce)
HST_get(DELTA_NR_DIRTIED,                     nr_dirtied)
HST_get(DELTA_NR_DIRTY,                       nr_dirty)
HST_get(DELTA_NR_DIRTY_BACKGROUND_THRESHOLD,  nr_dirty_background_threshold)
HST_get(DELTA_NR_DIRTY_THRESHOLD,             nr_dirty_threshold)
HST_get(DELTA_NR_FILE_PAGES,                  nr_file_pages)
HST_get(DELTA_NR_FREE_CMA,                    nr_free_cma)
HST_get(DELTA_NR_FREE_PAGES,                  nr_free_pages)
HST_get(DELTA_NR_INACTIVE_ANON,               nr_inactive_anon)
HST_get(DELTA_NR_INACTIVE_FILE,               nr_inactive_file)
HST_get(DELTA_NR_ISOLATED_ANON,               nr_isolated_anon)
HST_get(DELTA_NR_ISOLATED_FILE,               nr_isolated_file)
HST_get(DELTA_NR_KERNEL_STACK,                nr_kernel_stack)
HST_get(DELTA_NR_MAPPED,                      nr_mapped)
HST_get(DELTA_NR_MLOCK,                       nr_mlock)
HST_get(DELTA_NR_PAGES_SCANNED,               nr_pages_scanned)
HST_get(DELTA_NR_PAGE_TABLE_PAGES,            nr_page_table_pages)
HST_get(DELTA_NR_SHMEM,                       nr_shmem)
HST_get(DELTA_NR_SLAB_RECLAIMABLE,            nr_slab_reclaimable)
HST_get(DELTA_NR_SLAB_UNRECLAIMABLE,          nr_slab_unreclaimable)
HST_get(DELTA_NR_UNEVICTABLE,                 nr_unevictable)
HST_get(DELTA_NR_UNSTABLE,                    nr_unstable)
HST_get(DELTA_NR_VMSCAN_IMMEDIATE_RECLAIM,    nr_vmscan_immediate_reclaim)
HST_get(DELTA_NR_VMSCAN_WRITE,                nr_vmscan_write)
HST_get(DELTA_NR_WRITEBACK,                   nr_writeback)
HST_get(DELTA_NR_WRITEBACK_TEMP,              nr_writeback_temp)
HST_get(DELTA_NR_WRITTEN,                     nr_written)
HST_get(DELTA_NUMA_FOREIGN,                   numa_foreign)
HST_get(DELTA_NUMA_HINT_FAULTS,               numa_hint_faults)
HST_get(DELTA_NUMA_HINT_FAULTS_LOCAL,         numa_hint_faults_local)
HST_get(DELTA_NUMA_HIT,                       numa_hit)
HST_get(DELTA_NUMA_HUGE_PTE_UPDATES,          numa_huge_pte_updates)
HST_get(DELTA_NUMA_INTERLEAVE,                numa_interleave)
HST_get(DELTA_NUMA_LOCAL,                     numa_local)
HST_get(DELTA_NUMA_MISS,                      numa_miss)
HST_get(DELTA_NUMA_OTHER,                     numa_other)
HST_get(DELTA_NUMA_PAGES_MIGRATED,            numa_pages_migrated)
HST_get(DELTA_NUMA_PTE_UPDATES,               numa_pte_updates)
HST_get(DELTA_PAGEOUTRUN,                     pageoutrun)
HST_get(DELTA_PGACTIVATE,                     pgactivate)
HST_get(DELTA_PGALLOC_DMA,                    pgalloc_dma)
HST_get(DELTA_PGALLOC_DMA32,                  pgalloc_dma32)
HST_get(DELTA_PGALLOC_MOVABLE,                pgalloc_movable)
HST_get(DELTA_PGALLOC_NORMAL,                 pgalloc_normal)
HST_get(DELTA_PGDEACTIVATE,                   pgdeactivate)
HST_get(DELTA_PGFAULT,                        pgfault)
HST_get(DELTA_PGFREE,                         pgfree)
HST_get(DELTA_PGINODESTEAL,                   pginodesteal)
HST_get(DELTA_PGMAJFAULT,                     pgmajfault)
HST_get(DELTA_PGMIGRATE_FAIL,                 pgmigrate_fail)
HST_get(DELTA_PGMIGRATE_SUCCESS,              pgmigrate_success)
HST_get(DELTA_PGPGIN,                         pgpgin)
HST_get(DELTA_PGPGOUT,                        pgpgout)
HST_get(DELTA_PGREFILL_DMA,                   pgrefill_dma)
HST_get(DELTA_PGREFILL_DMA32,                 pgrefill_dma32)
HST_get(DELTA_PGREFILL_MOVABLE,               pgrefill_movable)
HST_get(DELTA_PGREFILL_NORMAL,                pgrefill_normal)
HST_get(DELTA_PGROTATED,                      pgrotated)
HST_get(DELTA_PGSCAN_DIRECT_DMA,              pgscan_direct_dma)
HST_get(DELTA_PGSCAN_DIRECT_DMA32,            pgscan_direct_dma32)
HST_get(DELTA_PGSCAN_DIRECT_MOVABLE,          pgscan_direct_movable)
HST_get(DELTA_PGSCAN_DIRECT_NORMAL,           pgscan_direct_normal)
HST_get(DELTA_PGSCAN_DIRECT_THROTTLE,         pgscan_direct_throttle)
HST_get(DELTA_PGSCAN_KSWAPD_DMA,              pgscan_kswapd_dma)
HST_get(DELTA_PGSCAN_KSWAPD_DMA32,            pgscan_kswapd_dma32)
HST_get(DELTA_PGSCAN_KSWAPD_MOVEABLE,         pgscan_kswapd_movable)
HST_get(DELTA_PGSCAN_KSWAPD_NORMAL,           pgscan_kswapd_normal)
HST_get(DELTA_PGSTEAL_DIRECT_DMA,             pgsteal_direct_dma)
HST_get(DELTA_PGSTEAL_DIRECT_DMA32,           pgsteal_direct_dma32)
HST_get(DELTA_PGSTEAL_DIRECT_MOVABLE,         pgsteal_direct_movable)
HST_get(DELTA_PGSTEAL_DIRECT_NORMAL,          pgsteal_direct_normal)
HST_get(DELTA_PGSTEAL_KSWAPD_DMA,             pgsteal_kswapd_dma)
HST_get(DELTA_PGSTEAL_KSWAPD_DMA32,           pgsteal_kswapd_dma32)
HST_get(DELTA_PGSTEAL_KSWAPD_MOVABLE,         pgsteal_kswapd_movable)
HST_get(DELTA_PGSTEAL_KSWAPD_NORMAL,          pgsteal_kswapd_normal)
HST_get(DELTA_PSWPIN,                         pswpin)
HST_get(DELTA_PSWPOUT,                        pswpout)
HST_get(DELTA_SLABS_SCANNED,                  slabs_scanned)
HST_get(DELTA_THP_COLLAPSE_ALLOC,             thp_collapse_alloc)
HST_get(DELTA_THP_COLLAPSE_ALLOC_FAILED,      thp_collapse_alloc_failed)
HST_get(DELTA_THP_FAULT_ALLOC,                thp_fault_alloc)
HST_get(DELTA_THP_FAULT_FALLBACK,             thp_fault_fallback)
HST_get(DELTA_THP_SPLIT,                      thp_split)
HST_get(DELTA_THP_ZERO_PAGE_ALLOC,            thp_zero_page_alloc)
HST_get(DELTA_THP_ZERO_PAGE_ALLOC_FAILED,     thp_zero_page_alloc_failed)
HST_get(DELTA_UNEVICTABLE_PGS_CLEARED,        unevictable_pgs_cleared)
HST_get(DELTA_UNEVICTABLE_PGS_CULLED,         unevictable_pgs_culled)
HST_get(DELTA_UNEVICTABLE_PGS_MLOCKED,        unevictable_pgs_mlocked)
HST_get(DELTA_UNEVICTABLE_PGS_MUNLOCKED,      unevictable_pgs_munlocked)
HST_get(DELTA_UNEVICTABLE_PGS_RESCUED,        unevictable_pgs_rescued)
HST_get(DELTA_UNEVICTABLE_PGS_SCANNED,        unevictable_pgs_scanned)
HST_get(DELTA_UNEVICTABLE_PGS_STRANDED,       unevictable_pgs_stranded)
HST_get(DELTA_WORKINGSET_ACTIVATE,            workingset_activate)
HST_get(DELTA_WORKINGSET_NODERECLAIM,         workingset_nodereclaim)
HST_get(DELTA_WORKINGSET_REFAULT,             workingset_refault)
HST_get(DELTA_ZONE_RECLAIM_FAILED,            zone_reclaim_failed)

#undef getDECL
#undef REG_get
#undef HST_get


// ___ Controlling Table ||||||||||||||||||||||||||||||||||||||||||||||||||||||

typedef void (*SET_t)(struct vmstat_result *, struct vmstat_hist *);
#define RS(e) (SET_t)setNAME(e)

typedef long (*GET_t)(struct procps_vmstat *);
#define RG(e) (GET_t)getNAME(e)

        /*
         * Need it be said?
         * This table must be kept in the exact same order as
         * those 'enum vmstat_item' guys ! */
static struct {
    SET_t setsfunc;              // the actual result setting routine
    GET_t getsfunc;              // a routine to return single result
} Item_table[] = {
/*  setsfunc                                 getsfunc
    ---------------------------------------  --------------------------------------  */
  { RS(noop),                                RG(noop)                                },
  { RS(extra),                               RG(extra)                               },

  { RS(ALLOCSTALL),                          RG(ALLOCSTALL)                          },
  { RS(BALLOON_DEFLATE),                     RG(BALLOON_DEFLATE)                     },
  { RS(BALLOON_INFLATE),                     RG(BALLOON_INFLATE)                     },
  { RS(BALLOON_MIGRATE),                     RG(BALLOON_MIGRATE)                     },
  { RS(COMPACT_FAIL),                        RG(COMPACT_FAIL)                        },
  { RS(COMPACT_FREE_SCANNED),                RG(COMPACT_FREE_SCANNED)                },
  { RS(COMPACT_ISOLATED),                    RG(COMPACT_ISOLATED)                    },
  { RS(COMPACT_MIGRATE_SCANNED),             RG(COMPACT_MIGRATE_SCANNED)             },
  { RS(COMPACT_STALL),                       RG(COMPACT_STALL)                       },
  { RS(COMPACT_SUCCESS),                     RG(COMPACT_SUCCESS)                     },
  { RS(DROP_PAGECACHE),                      RG(DROP_PAGECACHE)                      },
  { RS(DROP_SLAB),                           RG(DROP_SLAB)                           },
  { RS(HTLB_BUDDY_ALLOC_FAIL),               RG(HTLB_BUDDY_ALLOC_FAIL)               },
  { RS(HTLB_BUDDY_ALLOC_SUCCESS),            RG(HTLB_BUDDY_ALLOC_SUCCESS)            },
  { RS(KSWAPD_HIGH_WMARK_HIT_QUICKLY),       RG(KSWAPD_HIGH_WMARK_HIT_QUICKLY)       },
  { RS(KSWAPD_INODESTEAL),                   RG(KSWAPD_INODESTEAL)                   },
  { RS(KSWAPD_LOW_WMARK_HIT_QUICKLY),        RG(KSWAPD_LOW_WMARK_HIT_QUICKLY)        },
  { RS(NR_ACTIVE_ANON),                      RG(NR_ACTIVE_ANON)                      },
  { RS(NR_ACTIVE_FILE),                      RG(NR_ACTIVE_FILE)                      },
  { RS(NR_ALLOC_BATCH),                      RG(NR_ALLOC_BATCH)                      },
  { RS(NR_ANON_PAGES),                       RG(NR_ANON_PAGES)                       },
  { RS(NR_ANON_TRANSPARENT_HUGEPAGES),       RG(NR_ANON_TRANSPARENT_HUGEPAGES)       },
  { RS(NR_BOUNCE),                           RG(NR_BOUNCE)                           },
  { RS(NR_DIRTIED),                          RG(NR_DIRTIED)                          },
  { RS(NR_DIRTY),                            RG(NR_DIRTY)                            },
  { RS(NR_DIRTY_BACKGROUND_THRESHOLD),       RG(NR_DIRTY_BACKGROUND_THRESHOLD)       },
  { RS(NR_DIRTY_THRESHOLD),                  RG(NR_DIRTY_THRESHOLD)                  },
  { RS(NR_FILE_PAGES),                       RG(NR_FILE_PAGES)                       },
  { RS(NR_FREE_CMA),                         RG(NR_FREE_CMA)                         },
  { RS(NR_FREE_PAGES),                       RG(NR_FREE_PAGES)                       },
  { RS(NR_INACTIVE_ANON),                    RG(NR_INACTIVE_ANON)                    },
  { RS(NR_INACTIVE_FILE),                    RG(NR_INACTIVE_FILE)                    },
  { RS(NR_ISOLATED_ANON),                    RG(NR_ISOLATED_ANON)                    },
  { RS(NR_ISOLATED_FILE),                    RG(NR_ISOLATED_FILE)                    },
  { RS(NR_KERNEL_STACK),                     RG(NR_KERNEL_STACK)                     },
  { RS(NR_MAPPED),                           RG(NR_MAPPED)                           },
  { RS(NR_MLOCK),                            RG(NR_MLOCK)                            },
  { RS(NR_PAGES_SCANNED),                    RG(NR_PAGES_SCANNED)                    },
  { RS(NR_PAGE_TABLE_PAGES),                 RG(NR_PAGE_TABLE_PAGES)                 },
  { RS(NR_SHMEM),                            RG(NR_SHMEM)                            },
  { RS(NR_SLAB_RECLAIMABLE),                 RG(NR_SLAB_RECLAIMABLE)                 },
  { RS(NR_SLAB_UNRECLAIMABLE),               RG(NR_SLAB_UNRECLAIMABLE)               },
  { RS(NR_UNEVICTABLE),                      RG(NR_UNEVICTABLE)                      },
  { RS(NR_UNSTABLE),                         RG(NR_UNSTABLE)                         },
  { RS(NR_VMSCAN_IMMEDIATE_RECLAIM),         RG(NR_VMSCAN_IMMEDIATE_RECLAIM)         },
  { RS(NR_VMSCAN_WRITE),                     RG(NR_VMSCAN_WRITE)                     },
  { RS(NR_WRITEBACK),                        RG(NR_WRITEBACK)                        },
  { RS(NR_WRITEBACK_TEMP),                   RG(NR_WRITEBACK_TEMP)                   },
  { RS(NR_WRITTEN),                          RG(NR_WRITTEN)                          },
  { RS(NUMA_FOREIGN),                        RG(NUMA_FOREIGN)                        },
  { RS(NUMA_HINT_FAULTS),                    RG(NUMA_HINT_FAULTS)                    },
  { RS(NUMA_HINT_FAULTS_LOCAL),              RG(NUMA_HINT_FAULTS_LOCAL)              },
  { RS(NUMA_HIT),                            RG(NUMA_HIT)                            },
  { RS(NUMA_HUGE_PTE_UPDATES),               RG(NUMA_HUGE_PTE_UPDATES)               },
  { RS(NUMA_INTERLEAVE),                     RG(NUMA_INTERLEAVE)                     },
  { RS(NUMA_LOCAL),                          RG(NUMA_LOCAL)                          },
  { RS(NUMA_MISS),                           RG(NUMA_MISS)                           },
  { RS(NUMA_OTHER),                          RG(NUMA_OTHER)                          },
  { RS(NUMA_PAGES_MIGRATED),                 RG(NUMA_PAGES_MIGRATED)                 },
  { RS(NUMA_PTE_UPDATES),                    RG(NUMA_PTE_UPDATES)                    },
  { RS(PAGEOUTRUN),                          RG(PAGEOUTRUN)                          },
  { RS(PGACTIVATE),                          RG(PGACTIVATE)                          },
  { RS(PGALLOC_DMA),                         RG(PGALLOC_DMA)                         },
  { RS(PGALLOC_DMA32),                       RG(PGALLOC_DMA32)                       },
  { RS(PGALLOC_MOVABLE),                     RG(PGALLOC_MOVABLE)                     },
  { RS(PGALLOC_NORMAL),                      RG(PGALLOC_NORMAL)                      },
  { RS(PGDEACTIVATE),                        RG(PGDEACTIVATE)                        },
  { RS(PGFAULT),                             RG(PGFAULT)                             },
  { RS(PGFREE),                              RG(PGFREE)                              },
  { RS(PGINODESTEAL),                        RG(PGINODESTEAL)                        },
  { RS(PGMAJFAULT),                          RG(PGMAJFAULT)                          },
  { RS(PGMIGRATE_FAIL),                      RG(PGMIGRATE_FAIL)                      },
  { RS(PGMIGRATE_SUCCESS),                   RG(PGMIGRATE_SUCCESS)                   },
  { RS(PGPGIN),                              RG(PGPGIN)                              },
  { RS(PGPGOUT),                             RG(PGPGOUT)                             },
  { RS(PGREFILL_DMA),                        RG(PGREFILL_DMA)                        },
  { RS(PGREFILL_DMA32),                      RG(PGREFILL_DMA32)                      },
  { RS(PGREFILL_MOVABLE),                    RG(PGREFILL_MOVABLE)                    },
  { RS(PGREFILL_NORMAL),                     RG(PGREFILL_NORMAL)                     },
  { RS(PGROTATED),                           RG(PGROTATED)                           },
  { RS(PGSCAN_DIRECT_DMA),                   RG(PGSCAN_DIRECT_DMA)                   },
  { RS(PGSCAN_DIRECT_DMA32),                 RG(PGSCAN_DIRECT_DMA32)                 },
  { RS(PGSCAN_DIRECT_MOVABLE),               RG(PGSCAN_DIRECT_MOVABLE)               },
  { RS(PGSCAN_DIRECT_NORMAL),                RG(PGSCAN_DIRECT_NORMAL)                },
  { RS(PGSCAN_DIRECT_THROTTLE),              RG(PGSCAN_DIRECT_THROTTLE)              },
  { RS(PGSCAN_KSWAPD_DMA),                   RG(PGSCAN_KSWAPD_DMA)                   },
  { RS(PGSCAN_KSWAPD_DMA32),                 RG(PGSCAN_KSWAPD_DMA32)                 },
  { RS(PGSCAN_KSWAPD_MOVEABLE),              RG(PGSCAN_KSWAPD_MOVEABLE)              },
  { RS(PGSCAN_KSWAPD_NORMAL),                RG(PGSCAN_KSWAPD_NORMAL)                },
  { RS(PGSTEAL_DIRECT_DMA),                  RG(PGSTEAL_DIRECT_DMA)                  },
  { RS(PGSTEAL_DIRECT_DMA32),                RG(PGSTEAL_DIRECT_DMA32)                },
  { RS(PGSTEAL_DIRECT_MOVABLE),              RG(PGSTEAL_DIRECT_MOVABLE)              },
  { RS(PGSTEAL_DIRECT_NORMAL),               RG(PGSTEAL_DIRECT_NORMAL)               },
  { RS(PGSTEAL_KSWAPD_DMA),                  RG(PGSTEAL_KSWAPD_DMA)                  },
  { RS(PGSTEAL_KSWAPD_DMA32),                RG(PGSTEAL_KSWAPD_DMA32)                },
  { RS(PGSTEAL_KSWAPD_MOVABLE),              RG(PGSTEAL_KSWAPD_MOVABLE)              },
  { RS(PGSTEAL_KSWAPD_NORMAL),               RG(PGSTEAL_KSWAPD_NORMAL)               },
  { RS(PSWPIN),                              RG(PSWPIN)                              },
  { RS(PSWPOUT),                             RG(PSWPOUT)                             },
  { RS(SLABS_SCANNED),                       RG(SLABS_SCANNED)                       },
  { RS(THP_COLLAPSE_ALLOC),                  RG(THP_COLLAPSE_ALLOC)                  },
  { RS(THP_COLLAPSE_ALLOC_FAILED),           RG(THP_COLLAPSE_ALLOC_FAILED)           },
  { RS(THP_FAULT_ALLOC),                     RG(THP_FAULT_ALLOC)                     },
  { RS(THP_FAULT_FALLBACK),                  RG(THP_FAULT_FALLBACK)                  },
  { RS(THP_SPLIT),                           RG(THP_SPLIT)                           },
  { RS(THP_ZERO_PAGE_ALLOC),                 RG(THP_ZERO_PAGE_ALLOC)                 },
  { RS(THP_ZERO_PAGE_ALLOC_FAILED),          RG(THP_ZERO_PAGE_ALLOC_FAILED)          },
  { RS(UNEVICTABLE_PGS_CLEARED),             RG(UNEVICTABLE_PGS_CLEARED)             },
  { RS(UNEVICTABLE_PGS_CULLED),              RG(UNEVICTABLE_PGS_CULLED)              },
  { RS(UNEVICTABLE_PGS_MLOCKED),             RG(UNEVICTABLE_PGS_MLOCKED)             },
  { RS(UNEVICTABLE_PGS_MUNLOCKED),           RG(UNEVICTABLE_PGS_MUNLOCKED)           },
  { RS(UNEVICTABLE_PGS_RESCUED),             RG(UNEVICTABLE_PGS_RESCUED)             },
  { RS(UNEVICTABLE_PGS_SCANNED),             RG(UNEVICTABLE_PGS_SCANNED)             },
  { RS(UNEVICTABLE_PGS_STRANDED),            RG(UNEVICTABLE_PGS_STRANDED)            },
  { RS(WORKINGSET_ACTIVATE),                 RG(WORKINGSET_ACTIVATE)                 },
  { RS(WORKINGSET_NODERECLAIM),              RG(WORKINGSET_NODERECLAIM)              },
  { RS(WORKINGSET_REFAULT),                  RG(WORKINGSET_REFAULT)                  },
  { RS(ZONE_RECLAIM_FAILED),                 RG(ZONE_RECLAIM_FAILED)                 },

  { RS(DELTA_ALLOCSTALL),                    RG(DELTA_ALLOCSTALL)                    },
  { RS(DELTA_BALLOON_DEFLATE),               RG(DELTA_BALLOON_DEFLATE)               },
  { RS(DELTA_BALLOON_INFLATE),               RG(DELTA_BALLOON_INFLATE)               },
  { RS(DELTA_BALLOON_MIGRATE),               RG(DELTA_BALLOON_MIGRATE)               },
  { RS(DELTA_COMPACT_FAIL),                  RG(DELTA_COMPACT_FAIL)                  },
  { RS(DELTA_COMPACT_FREE_SCANNED),          RG(DELTA_COMPACT_FREE_SCANNED)          },
  { RS(DELTA_COMPACT_ISOLATED),              RG(DELTA_COMPACT_ISOLATED)              },
  { RS(DELTA_COMPACT_MIGRATE_SCANNED),       RG(DELTA_COMPACT_MIGRATE_SCANNED)       },
  { RS(DELTA_COMPACT_STALL),                 RG(DELTA_COMPACT_STALL)                 },
  { RS(DELTA_COMPACT_SUCCESS),               RG(DELTA_COMPACT_SUCCESS)               },
  { RS(DELTA_DROP_PAGECACHE),                RG(DELTA_DROP_PAGECACHE)                },
  { RS(DELTA_DROP_SLAB),                     RG(DELTA_DROP_SLAB)                     },
  { RS(DELTA_HTLB_BUDDY_ALLOC_FAIL),         RG(DELTA_HTLB_BUDDY_ALLOC_FAIL)         },
  { RS(DELTA_HTLB_BUDDY_ALLOC_SUCCESS),      RG(DELTA_HTLB_BUDDY_ALLOC_SUCCESS)      },
  { RS(DELTA_KSWAPD_HIGH_WMARK_HIT_QUICKLY), RG(DELTA_KSWAPD_HIGH_WMARK_HIT_QUICKLY) },
  { RS(DELTA_KSWAPD_INODESTEAL),             RG(DELTA_KSWAPD_INODESTEAL)             },
  { RS(DELTA_KSWAPD_LOW_WMARK_HIT_QUICKLY),  RG(DELTA_KSWAPD_LOW_WMARK_HIT_QUICKLY)  },
  { RS(DELTA_NR_ACTIVE_ANON),                RG(DELTA_NR_ACTIVE_ANON)                },
  { RS(DELTA_NR_ACTIVE_FILE),                RG(DELTA_NR_ACTIVE_FILE)                },
  { RS(DELTA_NR_ALLOC_BATCH),                RG(DELTA_NR_ALLOC_BATCH)                },
  { RS(DELTA_NR_ANON_PAGES),                 RG(DELTA_NR_ANON_PAGES)                 },
  { RS(DELTA_NR_ANON_TRANSPARENT_HUGEPAGES), RG(DELTA_NR_ANON_TRANSPARENT_HUGEPAGES) },
  { RS(DELTA_NR_BOUNCE),                     RG(DELTA_NR_BOUNCE)                     },
  { RS(DELTA_NR_DIRTIED),                    RG(DELTA_NR_DIRTIED)                    },
  { RS(DELTA_NR_DIRTY),                      RG(DELTA_NR_DIRTY)                      },
  { RS(DELTA_NR_DIRTY_BACKGROUND_THRESHOLD), RG(DELTA_NR_DIRTY_BACKGROUND_THRESHOLD) },
  { RS(DELTA_NR_DIRTY_THRESHOLD),            RG(DELTA_NR_DIRTY_THRESHOLD)            },
  { RS(DELTA_NR_FILE_PAGES),                 RG(DELTA_NR_FILE_PAGES)                 },
  { RS(DELTA_NR_FREE_CMA),                   RG(DELTA_NR_FREE_CMA)                   },
  { RS(DELTA_NR_FREE_PAGES),                 RG(DELTA_NR_FREE_PAGES)                 },
  { RS(DELTA_NR_INACTIVE_ANON),              RG(DELTA_NR_INACTIVE_ANON)              },
  { RS(DELTA_NR_INACTIVE_FILE),              RG(DELTA_NR_INACTIVE_FILE)              },
  { RS(DELTA_NR_ISOLATED_ANON),              RG(DELTA_NR_ISOLATED_ANON)              },
  { RS(DELTA_NR_ISOLATED_FILE),              RG(DELTA_NR_ISOLATED_FILE)              },
  { RS(DELTA_NR_KERNEL_STACK),               RG(DELTA_NR_KERNEL_STACK)               },
  { RS(DELTA_NR_MAPPED),                     RG(DELTA_NR_MAPPED)                     },
  { RS(DELTA_NR_MLOCK),                      RG(DELTA_NR_MLOCK)                      },
  { RS(DELTA_NR_PAGES_SCANNED),              RG(DELTA_NR_PAGES_SCANNED)              },
  { RS(DELTA_NR_PAGE_TABLE_PAGES),           RG(DELTA_NR_PAGE_TABLE_PAGES)           },
  { RS(DELTA_NR_SHMEM),                      RG(DELTA_NR_SHMEM)                      },
  { RS(DELTA_NR_SLAB_RECLAIMABLE),           RG(DELTA_NR_SLAB_RECLAIMABLE)           },
  { RS(DELTA_NR_SLAB_UNRECLAIMABLE),         RG(DELTA_NR_SLAB_UNRECLAIMABLE)         },
  { RS(DELTA_NR_UNEVICTABLE),                RG(DELTA_NR_UNEVICTABLE)                },
  { RS(DELTA_NR_UNSTABLE),                   RG(DELTA_NR_UNSTABLE)                   },
  { RS(DELTA_NR_VMSCAN_IMMEDIATE_RECLAIM),   RG(DELTA_NR_VMSCAN_IMMEDIATE_RECLAIM)   },
  { RS(DELTA_NR_VMSCAN_WRITE),               RG(DELTA_NR_VMSCAN_WRITE)               },
  { RS(DELTA_NR_WRITEBACK),                  RG(DELTA_NR_WRITEBACK)                  },
  { RS(DELTA_NR_WRITEBACK_TEMP),             RG(DELTA_NR_WRITEBACK_TEMP)             },
  { RS(DELTA_NR_WRITTEN),                    RG(DELTA_NR_WRITTEN)                    },
  { RS(DELTA_NUMA_FOREIGN),                  RG(DELTA_NUMA_FOREIGN)                  },
  { RS(DELTA_NUMA_HINT_FAULTS),              RG(DELTA_NUMA_HINT_FAULTS)              },
  { RS(DELTA_NUMA_HINT_FAULTS_LOCAL),        RG(DELTA_NUMA_HINT_FAULTS_LOCAL)        },
  { RS(DELTA_NUMA_HIT),                      RG(DELTA_NUMA_HIT)                      },
  { RS(DELTA_NUMA_HUGE_PTE_UPDATES),         RG(DELTA_NUMA_HUGE_PTE_UPDATES)         },
  { RS(DELTA_NUMA_INTERLEAVE),               RG(DELTA_NUMA_INTERLEAVE)               },
  { RS(DELTA_NUMA_LOCAL),                    RG(DELTA_NUMA_LOCAL)                    },
  { RS(DELTA_NUMA_MISS),                     RG(DELTA_NUMA_MISS)                     },
  { RS(DELTA_NUMA_OTHER),                    RG(DELTA_NUMA_OTHER)                    },
  { RS(DELTA_NUMA_PAGES_MIGRATED),           RG(DELTA_NUMA_PAGES_MIGRATED)           },
  { RS(DELTA_NUMA_PTE_UPDATES),              RG(DELTA_NUMA_PTE_UPDATES)              },
  { RS(DELTA_PAGEOUTRUN),                    RG(DELTA_PAGEOUTRUN)                    },
  { RS(DELTA_PGACTIVATE),                    RG(DELTA_PGACTIVATE)                    },
  { RS(DELTA_PGALLOC_DMA),                   RG(DELTA_PGALLOC_DMA)                   },
  { RS(DELTA_PGALLOC_DMA32),                 RG(DELTA_PGALLOC_DMA32)                 },
  { RS(DELTA_PGALLOC_MOVABLE),               RG(DELTA_PGALLOC_MOVABLE)               },
  { RS(DELTA_PGALLOC_NORMAL),                RG(DELTA_PGALLOC_NORMAL)                },
  { RS(DELTA_PGDEACTIVATE),                  RG(DELTA_PGDEACTIVATE)                  },
  { RS(DELTA_PGFAULT),                       RG(DELTA_PGFAULT)                       },
  { RS(DELTA_PGFREE),                        RG(DELTA_PGFREE)                        },
  { RS(DELTA_PGINODESTEAL),                  RG(DELTA_PGINODESTEAL)                  },
  { RS(DELTA_PGMAJFAULT),                    RG(DELTA_PGMAJFAULT)                    },
  { RS(DELTA_PGMIGRATE_FAIL),                RG(DELTA_PGMIGRATE_FAIL)                },
  { RS(DELTA_PGMIGRATE_SUCCESS),             RG(DELTA_PGMIGRATE_SUCCESS)             },
  { RS(DELTA_PGPGIN),                        RG(DELTA_PGPGIN)                        },
  { RS(DELTA_PGPGOUT),                       RG(DELTA_PGPGOUT)                       },
  { RS(DELTA_PGREFILL_DMA),                  RG(DELTA_PGREFILL_DMA)                  },
  { RS(DELTA_PGREFILL_DMA32),                RG(DELTA_PGREFILL_DMA32)                },
  { RS(DELTA_PGREFILL_MOVABLE),              RG(DELTA_PGREFILL_MOVABLE)              },
  { RS(DELTA_PGREFILL_NORMAL),               RG(DELTA_PGREFILL_NORMAL)               },
  { RS(DELTA_PGROTATED),                     RG(DELTA_PGROTATED)                     },
  { RS(DELTA_PGSCAN_DIRECT_DMA),             RG(DELTA_PGSCAN_DIRECT_DMA)             },
  { RS(DELTA_PGSCAN_DIRECT_DMA32),           RG(DELTA_PGSCAN_DIRECT_DMA32)           },
  { RS(DELTA_PGSCAN_DIRECT_MOVABLE),         RG(DELTA_PGSCAN_DIRECT_MOVABLE)         },
  { RS(DELTA_PGSCAN_DIRECT_NORMAL),          RG(DELTA_PGSCAN_DIRECT_NORMAL)          },
  { RS(DELTA_PGSCAN_DIRECT_THROTTLE),        RG(DELTA_PGSCAN_DIRECT_THROTTLE)        },
  { RS(DELTA_PGSCAN_KSWAPD_DMA),             RG(DELTA_PGSCAN_KSWAPD_DMA)             },
  { RS(DELTA_PGSCAN_KSWAPD_DMA32),           RG(DELTA_PGSCAN_KSWAPD_DMA32)           },
  { RS(DELTA_PGSCAN_KSWAPD_MOVEABLE),        RG(DELTA_PGSCAN_KSWAPD_MOVEABLE)        },
  { RS(DELTA_PGSCAN_KSWAPD_NORMAL),          RG(DELTA_PGSCAN_KSWAPD_NORMAL)          },
  { RS(DELTA_PGSTEAL_DIRECT_DMA),            RG(DELTA_PGSTEAL_DIRECT_DMA)            },
  { RS(DELTA_PGSTEAL_DIRECT_DMA32),          RG(DELTA_PGSTEAL_DIRECT_DMA32)          },
  { RS(DELTA_PGSTEAL_DIRECT_MOVABLE),        RG(DELTA_PGSTEAL_DIRECT_MOVABLE)        },
  { RS(DELTA_PGSTEAL_DIRECT_NORMAL),         RG(DELTA_PGSTEAL_DIRECT_NORMAL)         },
  { RS(DELTA_PGSTEAL_KSWAPD_DMA),            RG(DELTA_PGSTEAL_KSWAPD_DMA)            },
  { RS(DELTA_PGSTEAL_KSWAPD_DMA32),          RG(DELTA_PGSTEAL_KSWAPD_DMA32)          },
  { RS(DELTA_PGSTEAL_KSWAPD_MOVABLE),        RG(DELTA_PGSTEAL_KSWAPD_MOVABLE)        },
  { RS(DELTA_PGSTEAL_KSWAPD_NORMAL),         RG(DELTA_PGSTEAL_KSWAPD_NORMAL)         },
  { RS(DELTA_PSWPIN),                        RG(DELTA_PSWPIN)                        },
  { RS(DELTA_PSWPOUT),                       RG(DELTA_PSWPOUT)                       },
  { RS(DELTA_SLABS_SCANNED),                 RG(DELTA_SLABS_SCANNED)                 },
  { RS(DELTA_THP_COLLAPSE_ALLOC),            RG(DELTA_THP_COLLAPSE_ALLOC)            },
  { RS(DELTA_THP_COLLAPSE_ALLOC_FAILED),     RG(DELTA_THP_COLLAPSE_ALLOC_FAILED)     },
  { RS(DELTA_THP_FAULT_ALLOC),               RG(DELTA_THP_FAULT_ALLOC)               },
  { RS(DELTA_THP_FAULT_FALLBACK),            RG(DELTA_THP_FAULT_FALLBACK)            },
  { RS(DELTA_THP_SPLIT),                     RG(DELTA_THP_SPLIT)                     },
  { RS(DELTA_THP_ZERO_PAGE_ALLOC),           RG(DELTA_THP_ZERO_PAGE_ALLOC)           },
  { RS(DELTA_THP_ZERO_PAGE_ALLOC_FAILED),    RG(DELTA_THP_ZERO_PAGE_ALLOC_FAILED)    },
  { RS(DELTA_UNEVICTABLE_PGS_CLEARED),       RG(DELTA_UNEVICTABLE_PGS_CLEARED)       },
  { RS(DELTA_UNEVICTABLE_PGS_CULLED),        RG(DELTA_UNEVICTABLE_PGS_CULLED)        },
  { RS(DELTA_UNEVICTABLE_PGS_MLOCKED),       RG(DELTA_UNEVICTABLE_PGS_MLOCKED)       },
  { RS(DELTA_UNEVICTABLE_PGS_MUNLOCKED),     RG(DELTA_UNEVICTABLE_PGS_MUNLOCKED)     },
  { RS(DELTA_UNEVICTABLE_PGS_RESCUED),       RG(DELTA_UNEVICTABLE_PGS_RESCUED)       },
  { RS(DELTA_UNEVICTABLE_PGS_SCANNED),       RG(DELTA_UNEVICTABLE_PGS_SCANNED)       },
  { RS(DELTA_UNEVICTABLE_PGS_STRANDED),      RG(DELTA_UNEVICTABLE_PGS_STRANDED)      },
  { RS(DELTA_WORKINGSET_ACTIVATE),           RG(DELTA_WORKINGSET_ACTIVATE)           },
  { RS(DELTA_WORKINGSET_NODERECLAIM),        RG(DELTA_WORKINGSET_NODERECLAIM)        },
  { RS(DELTA_WORKINGSET_REFAULT),            RG(DELTA_WORKINGSET_REFAULT)            },
  { RS(DELTA_ZONE_RECLAIM_FAILED),           RG(DELTA_ZONE_RECLAIM_FAILED)           },

 // dummy entry corresponding to PROCPS_VMSTAT_logical_end ...
  { NULL,                                    NULL                                    }
};

    /* please note,
     * this enum MUST be 1 greater than the highest value of any enum */
enum vmstat_item PROCPS_VMSTAT_logical_end = PROCPS_VMSTAT_DELTA_ZONE_RECLAIM_FAILED + 1;

#undef setNAME
#undef getNAME
#undef RS
#undef RG


// ___ Private Functions ||||||||||||||||||||||||||||||||||||||||||||||||||||||

static inline void assign_results (
        struct vmstat_stack *stack,
        struct vmstat_hist *hist)
{
    struct vmstat_result *this = stack->head;

    for (;;) {
        enum vmstat_item item = this->item;
        if (item >= PROCPS_VMSTAT_logical_end)
            break;
        Item_table[item].setsfunc(this, hist);
        ++this;
    }
    return;
} // end: assign_results


static inline void cleanup_stack (
        struct vmstat_result *this)
{
    for (;;) {
        if (this->item >= PROCPS_VMSTAT_logical_end)
            break;
        if (this->item > PROCPS_VMSTAT_noop)
            this->result.ul_int = 0;
        ++this;
    }
} // end: cleanup_stack


static inline void cleanup_stacks_all (
        struct procps_vmstat *info)
{
    int i;
    struct stacks_extent *ext = info->extents;

    while (ext) {
        for (i = 0; ext->stacks[i]; i++)
            cleanup_stack(ext->stacks[i]->head);
        ext = ext->next;
    };
    info->dirty_stacks = 0;
} // end: cleanup_stacks_all


static void extents_free_all (
        struct procps_vmstat *info)
{
    do {
        struct stacks_extent *p = info->extents;
        info->extents = info->extents->next;
        free(p);
    } while (info->extents);
} // end: extents_free_all


static inline struct vmstat_result *itemize_stack (
        struct vmstat_result *p,
        int depth,
        enum vmstat_item *items)
{
    struct vmstat_result *p_sav = p;
    int i;

    for (i = 0; i < depth; i++) {
        p->item = items[i];
        p->result.ul_int = 0;
        ++p;
    }
    return p_sav;
} // end: itemize_stack


static inline int items_check_failed (
        int numitems,
        enum vmstat_item *items)
{
    int i;

    /* if an enum is passed instead of an address of one or more enums, ol' gcc
     * will silently convert it to an address (possibly NULL).  only clang will
     * offer any sort of warning like the following:
     *
     * warning: incompatible integer to pointer conversion passing 'int' to parameter of type 'enum vmstat_item *'
     * my_stack = procps_vmstat_select(info, PROCPS_VMSTAT_noop, num);
     *                                       ^~~~~~~~~~~~~~~~
     */
    if (numitems < 1
    || (void *)items < (void *)(unsigned long)(2 * PROCPS_VMSTAT_logical_end))
        return -1;

    for (i = 0; i < numitems; i++) {
        // a vmstat_item is currently unsigned, but we'll protect our future
        if (items[i] < 0)
            return -1;
        if (items[i] >= PROCPS_VMSTAT_logical_end)
            return -1;
    }

    return 0;
} // end: items_check_failed


static int make_hash_failed (
        struct procps_vmstat *info)
{
 #define htVAL(f) e.key = STRINGIFY(f); e.data = &info->hist.new. f; \
  if (!hsearch_r(e, ENTER, &ep, &info->hashtab)) return -errno;
    ENTRY e, *ep;
    size_t n;

    n = sizeof(struct vmstat_data) / sizeof(unsigned long);
    // we'll follow the hsearch recommendation of an extra 25%
    hcreate_r(n + (n / 4), &info->hashtab);

    htVAL(allocstall)
    htVAL(balloon_deflate)
    htVAL(balloon_inflate)
    htVAL(balloon_migrate)
    htVAL(compact_fail)
    htVAL(compact_free_scanned)
    htVAL(compact_isolated)
    htVAL(compact_migrate_scanned)
    htVAL(compact_stall)
    htVAL(compact_success)
    htVAL(drop_pagecache)
    htVAL(drop_slab)
    htVAL(htlb_buddy_alloc_fail)
    htVAL(htlb_buddy_alloc_success)
    htVAL(kswapd_high_wmark_hit_quickly)
    htVAL(kswapd_inodesteal)
    htVAL(kswapd_low_wmark_hit_quickly)
    htVAL(nr_active_anon)
    htVAL(nr_active_file)
    htVAL(nr_alloc_batch)
    htVAL(nr_anon_pages)
    htVAL(nr_anon_transparent_hugepages)
    htVAL(nr_bounce)
    htVAL(nr_dirtied)
    htVAL(nr_dirty)
    htVAL(nr_dirty_background_threshold)
    htVAL(nr_dirty_threshold)
    htVAL(nr_file_pages)
    htVAL(nr_free_cma)
    htVAL(nr_free_pages)
    htVAL(nr_inactive_anon)
    htVAL(nr_inactive_file)
    htVAL(nr_isolated_anon)
    htVAL(nr_isolated_file)
    htVAL(nr_kernel_stack)
    htVAL(nr_mapped)
    htVAL(nr_mlock)
    htVAL(nr_pages_scanned)
    htVAL(nr_page_table_pages)
    htVAL(nr_shmem)
    htVAL(nr_slab_reclaimable)
    htVAL(nr_slab_unreclaimable)
    htVAL(nr_unevictable)
    htVAL(nr_unstable)
    htVAL(nr_vmscan_immediate_reclaim)
    htVAL(nr_vmscan_write)
    htVAL(nr_writeback)
    htVAL(nr_writeback_temp)
    htVAL(nr_written)
    htVAL(numa_foreign)
    htVAL(numa_hint_faults)
    htVAL(numa_hint_faults_local)
    htVAL(numa_hit)
    htVAL(numa_huge_pte_updates)
    htVAL(numa_interleave)
    htVAL(numa_local)
    htVAL(numa_miss)
    htVAL(numa_other)
    htVAL(numa_pages_migrated)
    htVAL(numa_pte_updates)
    htVAL(pageoutrun)
    htVAL(pgactivate)
    htVAL(pgalloc_dma)
    htVAL(pgalloc_dma32)
    htVAL(pgalloc_movable)
    htVAL(pgalloc_normal)
    htVAL(pgdeactivate)
    htVAL(pgfault)
    htVAL(pgfree)
    htVAL(pginodesteal)
    htVAL(pgmajfault)
    htVAL(pgmigrate_fail)
    htVAL(pgmigrate_success)
    htVAL(pgpgin)
    htVAL(pgpgout)
    htVAL(pgrefill_dma)
    htVAL(pgrefill_dma32)
    htVAL(pgrefill_movable)
    htVAL(pgrefill_normal)
    htVAL(pgrotated)
    htVAL(pgscan_direct_dma)
    htVAL(pgscan_direct_dma32)
    htVAL(pgscan_direct_movable)
    htVAL(pgscan_direct_normal)
    htVAL(pgscan_direct_throttle)
    htVAL(pgscan_kswapd_dma)
    htVAL(pgscan_kswapd_dma32)
    htVAL(pgscan_kswapd_movable)
    htVAL(pgscan_kswapd_normal)
    htVAL(pgsteal_direct_dma)
    htVAL(pgsteal_direct_dma32)
    htVAL(pgsteal_direct_movable)
    htVAL(pgsteal_direct_normal)
    htVAL(pgsteal_kswapd_dma)
    htVAL(pgsteal_kswapd_dma32)
    htVAL(pgsteal_kswapd_movable)
    htVAL(pgsteal_kswapd_normal)
    htVAL(pswpin)
    htVAL(pswpout)
    htVAL(slabs_scanned)
    htVAL(thp_collapse_alloc)
    htVAL(thp_collapse_alloc_failed)
    htVAL(thp_fault_alloc)
    htVAL(thp_fault_fallback)
    htVAL(thp_split)
    htVAL(thp_zero_page_alloc)
    htVAL(thp_zero_page_alloc_failed)
    htVAL(unevictable_pgs_cleared)
    htVAL(unevictable_pgs_culled)
    htVAL(unevictable_pgs_mlocked)
    htVAL(unevictable_pgs_munlocked)
    htVAL(unevictable_pgs_rescued)
    htVAL(unevictable_pgs_scanned)
    htVAL(unevictable_pgs_stranded)
    htVAL(workingset_activate)
    htVAL(workingset_nodereclaim)
    htVAL(workingset_refault)
    htVAL(zone_reclaim_failed)

    return 0;
 #undef htVAL
} // end: make_hash_failed


/*
 * read_vmstat_failed():
 *
 * Read the data out of /proc/vmstat putting the information
 * into the supplied info structure
 */
static int read_vmstat_failed (
        struct procps_vmstat *info)
{
    char buf[8192];
    char *head, *tail;
    int size;
    unsigned long *valptr;

    if (info == NULL)
        return -1;

    // remember history from last time around
    memcpy(&info->hist.old, &info->hist.new, sizeof(struct vmstat_data));
    // clear out the soon to be 'current' values
    memset(&info->hist.new, 0, sizeof(struct vmstat_data));

    if (-1 == info->vmstat_fd
    && (info->vmstat_fd = open(VMSTAT_FILE, O_RDONLY)) == -1)
        return -errno;

    if (lseek(info->vmstat_fd, 0L, SEEK_SET) == -1)
        return -errno;

    for (;;) {
        if ((size = read(info->vmstat_fd, buf, sizeof(buf)-1)) < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            return -errno;
        }
        break;
    }
    if (size == 0)
        return 0;
    buf[size] = '\0';

    head = buf;
    do {
        static ENTRY e;      // just to keep coverity off our backs (e.data)
        ENTRY *ep;

        tail = strchr(head, ' ');
        if (!tail)
            break;
        *tail = '\0';
        valptr = NULL;

        e.key = head;
        if (hsearch_r(e, FIND, &ep, &info->hashtab))
            valptr = ep->data;

        head = tail + 1;
        if (valptr)
            *valptr = strtoul(head, &tail, 10);

        tail = strchr(head, '\n');
        if (!tail)
            break;

        head = tail + 1;
    } while(tail);

    // let's not distort the deltas the first time thru ...
    if (!info->vmstat_was_read)
        memcpy(&info->hist.old, &info->hist.new, sizeof(struct vmstat_data));
    info->vmstat_was_read = 1;

    return 0;
} // end: read_vmstat_failed


/*
 * stacks_alloc():
 *
 * Allocate and initialize one or more stacks each of which is anchored in an
 * associated vmstat_stack structure.
 *
 * All such stacks will have their result structures properly primed with
 * 'items', while the result itself will be zeroed.
 *
 * Returns a stacks_extent struct anchoring the 'heads' of each new stack.
 */
static struct stacks_extent *stacks_alloc (
        struct procps_vmstat *info,
        int maxstacks)
{
    struct stacks_extent *p_blob;
    struct vmstat_stack **p_vect;
    struct vmstat_stack *p_head;
    size_t vect_size, head_size, list_size, blob_size;
    void *v_head, *v_list;
    int i;

    if (info == NULL || info->items == NULL)
        return NULL;
    if (maxstacks < 1)
        return NULL;

    vect_size  = sizeof(void *) * maxstacks;                   // size of the addr vectors |
    vect_size += sizeof(void *);                               // plus NULL addr delimiter |
    head_size  = sizeof(struct vmstat_stack);                  // size of that head struct |
    list_size  = sizeof(struct vmstat_result)*info->numitems;  // any single results stack |
    blob_size  = sizeof(struct stacks_extent);                 // the extent anchor itself |
    blob_size += vect_size;                                    // plus room for addr vects |
    blob_size += head_size * maxstacks;                        // plus room for head thing |
    blob_size += list_size * maxstacks;                        // plus room for our stacks |

    /* note: all of our memory is allocated in a single blob, facilitating a later free(). |
             as a minimum, it is important that the result structures themselves always be |
             contiguous for every stack since they are accessed through relative position. | */
    if (NULL == (p_blob = calloc(1, blob_size)))
        return NULL;

    p_blob->next = info->extents;                              // push this extent onto... |
    info->extents = p_blob;                                    // ...some existing extents |
    p_vect = (void *)p_blob + sizeof(struct stacks_extent);    // prime our vector pointer |
    p_blob->stacks = p_vect;                                   // set actual vectors start |
    v_head = (void *)p_vect + vect_size;                       // prime head pointer start |
    v_list = v_head + (head_size * maxstacks);                 // prime our stacks pointer |

    for (i = 0; i < maxstacks; i++) {
        p_head = (struct vmstat_stack *)v_head;
        p_head->head = itemize_stack((struct vmstat_result *)v_list, info->numitems, info->items);
        p_blob->stacks[i] = p_head;
        v_list += list_size;
        v_head += head_size;
    }
    p_blob->ext_numstacks = maxstacks;
    return p_blob;
} // end: stacks_alloc


// ___ Public Functions |||||||||||||||||||||||||||||||||||||||||||||||||||||||

// --- standard required functions --------------------------------------------

/*
 * procps_vmstat_new:
 *
 * Create a new container to hold the stat information
 *
 * The initial refcount is 1, and needs to be decremented
 * to release the resources of the structure.
 *
 * Returns: a pointer to a new vmstatinfo struct
 */
PROCPS_EXPORT int procps_vmstat_new (
        struct procps_vmstat **info)
{
    struct procps_vmstat *p;
    int rc;

    if (info == NULL || *info != NULL)
        return -EINVAL;
    if (!(p = calloc(1, sizeof(struct procps_vmstat))))
        return -ENOMEM;

    p->refcount = 1;
    p->vmstat_fd = -1;

    if ((rc = make_hash_failed(p))) {
        free(p);
        return rc;
    }

    *info = p;
    return 0;
} // end: procps_vmstat_new


PROCPS_EXPORT int procps_vmstat_ref (
        struct procps_vmstat *info)
{
    if (info == NULL)
        return -EINVAL;

    info->refcount++;
    return info->refcount;
} // end: procps_vmstat_ref


PROCPS_EXPORT int procps_vmstat_unref (
        struct procps_vmstat **info)
{
    if (info == NULL || *info == NULL)
        return -EINVAL;
    (*info)->refcount--;

    if ((*info)->refcount == 0) {
        if ((*info)->extents)
            extents_free_all((*info));
        if ((*info)->items)
            free((*info)->items);
        hdestroy_r(&(*info)->hashtab);
        free(*info);
        *info = NULL;
        return 0;
    }
    return (*info)->refcount;
} // end: procps_vmstat_unref


// --- variable interface functions -------------------------------------------

PROCPS_EXPORT signed long procps_vmstat_get (
        struct procps_vmstat *info,
        enum vmstat_item item)
{
    static time_t sav_secs;
    time_t cur_secs;
    int rc;

    if (info == NULL)
        return -EINVAL;
    if (item < 0 || item >= PROCPS_VMSTAT_logical_end)
        return -EINVAL;

    /* we will NOT read the vmstat file with every call - rather, we'll offer
       a granularity of 1 second between reads ... */
    cur_secs = time(NULL);
    if (1 <= cur_secs - sav_secs) {
        if ((rc = read_vmstat_failed(info)))
            return rc;
        sav_secs = cur_secs;
    }

    return Item_table[item].getsfunc(info);
} // end: procps_vmstat_get


/* procps_vmstat_select():
 *
 * Harvest all the requested /proc/vmstat information then return
 * it in a results stack.
 *
 * Returns: pointer to a vmstat_stack struct on success, NULL on error.
 */
PROCPS_EXPORT struct vmstat_stack *procps_vmstat_select (
        struct procps_vmstat *info,
        enum vmstat_item *items,
        int numitems)
{
    if (info == NULL || items == NULL)
        return NULL;
    if (items_check_failed(numitems, items))
        return NULL;

    /* is this the first time or have things changed since we were last called?
       if so, gotta' redo all of our stacks stuff ... */
    if (info->numitems != numitems + 1
    || memcmp(info->items, items, sizeof(enum vmstat_item) * numitems)) {
        // allow for our PROCPS_VMSTAT_logical_end
        if (!(info->items = realloc(info->items, sizeof(enum vmstat_item) * (numitems + 1))))
            return NULL;
        memcpy(info->items, items, sizeof(enum vmstat_item) * numitems);
        info->items[numitems] = PROCPS_VMSTAT_logical_end;
        info->numitems = numitems + 1;
        if (info->extents)
            extents_free_all(info);
    }
    if (!info->extents
    && !(stacks_alloc(info, 1)))
       return NULL;

    if (info->dirty_stacks)
        cleanup_stacks_all(info);

    if (read_vmstat_failed(info))
        return NULL;
    assign_results(info->extents->stacks[0], &info->hist);
    info->dirty_stacks = 1;

    return info->extents->stacks[0];
} // end: procps_vmstat_select
