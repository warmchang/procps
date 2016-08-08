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
#include <stdio.h>
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

struct vmstat_info {
    int refcount;
    int vmstat_fd;
    int vmstat_was_read;
    int dirty_stacks;
    struct vmstat_hist hist;
    int numitems;
    enum vmstat_item *items;
    struct stacks_extent *extents;
    struct hsearch_data hashtab;
    struct vmstat_result get_this;
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


// ___ Controlling Table ||||||||||||||||||||||||||||||||||||||||||||||||||||||

typedef void (*SET_t)(struct vmstat_result *, struct vmstat_hist *);
#define RS(e) (SET_t)setNAME(e)

#define TS(t) STRINGIFY(t)
#define TS_noop ""

        /*
         * Need it be said?
         * This table must be kept in the exact same order as
         * those 'enum vmstat_item' guys ! */
static struct {
    SET_t setsfunc;              // the actual result setting routine
    char *type2str;              // the result type as a string value
} Item_table[] = {
/*  setsfunc                                  type2str
    ----------------------------------------  ---------- */
  { RS(noop),                                 TS_noop    },
  { RS(extra),                                TS_noop    },

  { RS(ALLOCSTALL),                           TS(ul_int) },
  { RS(BALLOON_DEFLATE),                      TS(ul_int) },
  { RS(BALLOON_INFLATE),                      TS(ul_int) },
  { RS(BALLOON_MIGRATE),                      TS(ul_int) },
  { RS(COMPACT_FAIL),                         TS(ul_int) },
  { RS(COMPACT_FREE_SCANNED),                 TS(ul_int) },
  { RS(COMPACT_ISOLATED),                     TS(ul_int) },
  { RS(COMPACT_MIGRATE_SCANNED),              TS(ul_int) },
  { RS(COMPACT_STALL),                        TS(ul_int) },
  { RS(COMPACT_SUCCESS),                      TS(ul_int) },
  { RS(DROP_PAGECACHE),                       TS(ul_int) },
  { RS(DROP_SLAB),                            TS(ul_int) },
  { RS(HTLB_BUDDY_ALLOC_FAIL),                TS(ul_int) },
  { RS(HTLB_BUDDY_ALLOC_SUCCESS),             TS(ul_int) },
  { RS(KSWAPD_HIGH_WMARK_HIT_QUICKLY),        TS(ul_int) },
  { RS(KSWAPD_INODESTEAL),                    TS(ul_int) },
  { RS(KSWAPD_LOW_WMARK_HIT_QUICKLY),         TS(ul_int) },
  { RS(NR_ACTIVE_ANON),                       TS(ul_int) },
  { RS(NR_ACTIVE_FILE),                       TS(ul_int) },
  { RS(NR_ALLOC_BATCH),                       TS(ul_int) },
  { RS(NR_ANON_PAGES),                        TS(ul_int) },
  { RS(NR_ANON_TRANSPARENT_HUGEPAGES),        TS(ul_int) },
  { RS(NR_BOUNCE),                            TS(ul_int) },
  { RS(NR_DIRTIED),                           TS(ul_int) },
  { RS(NR_DIRTY),                             TS(ul_int) },
  { RS(NR_DIRTY_BACKGROUND_THRESHOLD),        TS(ul_int) },
  { RS(NR_DIRTY_THRESHOLD),                   TS(ul_int) },
  { RS(NR_FILE_PAGES),                        TS(ul_int) },
  { RS(NR_FREE_CMA),                          TS(ul_int) },
  { RS(NR_FREE_PAGES),                        TS(ul_int) },
  { RS(NR_INACTIVE_ANON),                     TS(ul_int) },
  { RS(NR_INACTIVE_FILE),                     TS(ul_int) },
  { RS(NR_ISOLATED_ANON),                     TS(ul_int) },
  { RS(NR_ISOLATED_FILE),                     TS(ul_int) },
  { RS(NR_KERNEL_STACK),                      TS(ul_int) },
  { RS(NR_MAPPED),                            TS(ul_int) },
  { RS(NR_MLOCK),                             TS(ul_int) },
  { RS(NR_PAGES_SCANNED),                     TS(ul_int) },
  { RS(NR_PAGE_TABLE_PAGES),                  TS(ul_int) },
  { RS(NR_SHMEM),                             TS(ul_int) },
  { RS(NR_SLAB_RECLAIMABLE),                  TS(ul_int) },
  { RS(NR_SLAB_UNRECLAIMABLE),                TS(ul_int) },
  { RS(NR_UNEVICTABLE),                       TS(ul_int) },
  { RS(NR_UNSTABLE),                          TS(ul_int) },
  { RS(NR_VMSCAN_IMMEDIATE_RECLAIM),          TS(ul_int) },
  { RS(NR_VMSCAN_WRITE),                      TS(ul_int) },
  { RS(NR_WRITEBACK),                         TS(ul_int) },
  { RS(NR_WRITEBACK_TEMP),                    TS(ul_int) },
  { RS(NR_WRITTEN),                           TS(ul_int) },
  { RS(NUMA_FOREIGN),                         TS(ul_int) },
  { RS(NUMA_HINT_FAULTS),                     TS(ul_int) },
  { RS(NUMA_HINT_FAULTS_LOCAL),               TS(ul_int) },
  { RS(NUMA_HIT),                             TS(ul_int) },
  { RS(NUMA_HUGE_PTE_UPDATES),                TS(ul_int) },
  { RS(NUMA_INTERLEAVE),                      TS(ul_int) },
  { RS(NUMA_LOCAL),                           TS(ul_int) },
  { RS(NUMA_MISS),                            TS(ul_int) },
  { RS(NUMA_OTHER),                           TS(ul_int) },
  { RS(NUMA_PAGES_MIGRATED),                  TS(ul_int) },
  { RS(NUMA_PTE_UPDATES),                     TS(ul_int) },
  { RS(PAGEOUTRUN),                           TS(ul_int) },
  { RS(PGACTIVATE),                           TS(ul_int) },
  { RS(PGALLOC_DMA),                          TS(ul_int) },
  { RS(PGALLOC_DMA32),                        TS(ul_int) },
  { RS(PGALLOC_MOVABLE),                      TS(ul_int) },
  { RS(PGALLOC_NORMAL),                       TS(ul_int) },
  { RS(PGDEACTIVATE),                         TS(ul_int) },
  { RS(PGFAULT),                              TS(ul_int) },
  { RS(PGFREE),                               TS(ul_int) },
  { RS(PGINODESTEAL),                         TS(ul_int) },
  { RS(PGMAJFAULT),                           TS(ul_int) },
  { RS(PGMIGRATE_FAIL),                       TS(ul_int) },
  { RS(PGMIGRATE_SUCCESS),                    TS(ul_int) },
  { RS(PGPGIN),                               TS(ul_int) },
  { RS(PGPGOUT),                              TS(ul_int) },
  { RS(PGREFILL_DMA),                         TS(ul_int) },
  { RS(PGREFILL_DMA32),                       TS(ul_int) },
  { RS(PGREFILL_MOVABLE),                     TS(ul_int) },
  { RS(PGREFILL_NORMAL),                      TS(ul_int) },
  { RS(PGROTATED),                            TS(ul_int) },
  { RS(PGSCAN_DIRECT_DMA),                    TS(ul_int) },
  { RS(PGSCAN_DIRECT_DMA32),                  TS(ul_int) },
  { RS(PGSCAN_DIRECT_MOVABLE),                TS(ul_int) },
  { RS(PGSCAN_DIRECT_NORMAL),                 TS(ul_int) },
  { RS(PGSCAN_DIRECT_THROTTLE),               TS(ul_int) },
  { RS(PGSCAN_KSWAPD_DMA),                    TS(ul_int) },
  { RS(PGSCAN_KSWAPD_DMA32),                  TS(ul_int) },
  { RS(PGSCAN_KSWAPD_MOVEABLE),               TS(ul_int) },
  { RS(PGSCAN_KSWAPD_NORMAL),                 TS(ul_int) },
  { RS(PGSTEAL_DIRECT_DMA),                   TS(ul_int) },
  { RS(PGSTEAL_DIRECT_DMA32),                 TS(ul_int) },
  { RS(PGSTEAL_DIRECT_MOVABLE),               TS(ul_int) },
  { RS(PGSTEAL_DIRECT_NORMAL),                TS(ul_int) },
  { RS(PGSTEAL_KSWAPD_DMA),                   TS(ul_int) },
  { RS(PGSTEAL_KSWAPD_DMA32),                 TS(ul_int) },
  { RS(PGSTEAL_KSWAPD_MOVABLE),               TS(ul_int) },
  { RS(PGSTEAL_KSWAPD_NORMAL),                TS(ul_int) },
  { RS(PSWPIN),                               TS(ul_int) },
  { RS(PSWPOUT),                              TS(ul_int) },
  { RS(SLABS_SCANNED),                        TS(ul_int) },
  { RS(THP_COLLAPSE_ALLOC),                   TS(ul_int) },
  { RS(THP_COLLAPSE_ALLOC_FAILED),            TS(ul_int) },
  { RS(THP_FAULT_ALLOC),                      TS(ul_int) },
  { RS(THP_FAULT_FALLBACK),                   TS(ul_int) },
  { RS(THP_SPLIT),                            TS(ul_int) },
  { RS(THP_ZERO_PAGE_ALLOC),                  TS(ul_int) },
  { RS(THP_ZERO_PAGE_ALLOC_FAILED),           TS(ul_int) },
  { RS(UNEVICTABLE_PGS_CLEARED),              TS(ul_int) },
  { RS(UNEVICTABLE_PGS_CULLED),               TS(ul_int) },
  { RS(UNEVICTABLE_PGS_MLOCKED),              TS(ul_int) },
  { RS(UNEVICTABLE_PGS_MUNLOCKED),            TS(ul_int) },
  { RS(UNEVICTABLE_PGS_RESCUED),              TS(ul_int) },
  { RS(UNEVICTABLE_PGS_SCANNED),              TS(ul_int) },
  { RS(UNEVICTABLE_PGS_STRANDED),             TS(ul_int) },
  { RS(WORKINGSET_ACTIVATE),                  TS(ul_int) },
  { RS(WORKINGSET_NODERECLAIM),               TS(ul_int) },
  { RS(WORKINGSET_REFAULT),                   TS(ul_int) },
  { RS(ZONE_RECLAIM_FAILED),                  TS(ul_int) },

  { RS(DELTA_ALLOCSTALL),                     TS(sl_int) },
  { RS(DELTA_BALLOON_DEFLATE),                TS(sl_int) },
  { RS(DELTA_BALLOON_INFLATE),                TS(sl_int) },
  { RS(DELTA_BALLOON_MIGRATE),                TS(sl_int) },
  { RS(DELTA_COMPACT_FAIL),                   TS(sl_int) },
  { RS(DELTA_COMPACT_FREE_SCANNED),           TS(sl_int) },
  { RS(DELTA_COMPACT_ISOLATED),               TS(sl_int) },
  { RS(DELTA_COMPACT_MIGRATE_SCANNED),        TS(sl_int) },
  { RS(DELTA_COMPACT_STALL),                  TS(sl_int) },
  { RS(DELTA_COMPACT_SUCCESS),                TS(sl_int) },
  { RS(DELTA_DROP_PAGECACHE),                 TS(sl_int) },
  { RS(DELTA_DROP_SLAB),                      TS(sl_int) },
  { RS(DELTA_HTLB_BUDDY_ALLOC_FAIL),          TS(sl_int) },
  { RS(DELTA_HTLB_BUDDY_ALLOC_SUCCESS),       TS(sl_int) },
  { RS(DELTA_KSWAPD_HIGH_WMARK_HIT_QUICKLY),  TS(sl_int) },
  { RS(DELTA_KSWAPD_INODESTEAL),              TS(sl_int) },
  { RS(DELTA_KSWAPD_LOW_WMARK_HIT_QUICKLY),   TS(sl_int) },
  { RS(DELTA_NR_ACTIVE_ANON),                 TS(sl_int) },
  { RS(DELTA_NR_ACTIVE_FILE),                 TS(sl_int) },
  { RS(DELTA_NR_ALLOC_BATCH),                 TS(sl_int) },
  { RS(DELTA_NR_ANON_PAGES),                  TS(sl_int) },
  { RS(DELTA_NR_ANON_TRANSPARENT_HUGEPAGES),  TS(sl_int) },
  { RS(DELTA_NR_BOUNCE),                      TS(sl_int) },
  { RS(DELTA_NR_DIRTIED),                     TS(sl_int) },
  { RS(DELTA_NR_DIRTY),                       TS(sl_int) },
  { RS(DELTA_NR_DIRTY_BACKGROUND_THRESHOLD),  TS(sl_int) },
  { RS(DELTA_NR_DIRTY_THRESHOLD),             TS(sl_int) },
  { RS(DELTA_NR_FILE_PAGES),                  TS(sl_int) },
  { RS(DELTA_NR_FREE_CMA),                    TS(sl_int) },
  { RS(DELTA_NR_FREE_PAGES),                  TS(sl_int) },
  { RS(DELTA_NR_INACTIVE_ANON),               TS(sl_int) },
  { RS(DELTA_NR_INACTIVE_FILE),               TS(sl_int) },
  { RS(DELTA_NR_ISOLATED_ANON),               TS(sl_int) },
  { RS(DELTA_NR_ISOLATED_FILE),               TS(sl_int) },
  { RS(DELTA_NR_KERNEL_STACK),                TS(sl_int) },
  { RS(DELTA_NR_MAPPED),                      TS(sl_int) },
  { RS(DELTA_NR_MLOCK),                       TS(sl_int) },
  { RS(DELTA_NR_PAGES_SCANNED),               TS(sl_int) },
  { RS(DELTA_NR_PAGE_TABLE_PAGES),            TS(sl_int) },
  { RS(DELTA_NR_SHMEM),                       TS(sl_int) },
  { RS(DELTA_NR_SLAB_RECLAIMABLE),            TS(sl_int) },
  { RS(DELTA_NR_SLAB_UNRECLAIMABLE),          TS(sl_int) },
  { RS(DELTA_NR_UNEVICTABLE),                 TS(sl_int) },
  { RS(DELTA_NR_UNSTABLE),                    TS(sl_int) },
  { RS(DELTA_NR_VMSCAN_IMMEDIATE_RECLAIM),    TS(sl_int) },
  { RS(DELTA_NR_VMSCAN_WRITE),                TS(sl_int) },
  { RS(DELTA_NR_WRITEBACK),                   TS(sl_int) },
  { RS(DELTA_NR_WRITEBACK_TEMP),              TS(sl_int) },
  { RS(DELTA_NR_WRITTEN),                     TS(sl_int) },
  { RS(DELTA_NUMA_FOREIGN),                   TS(sl_int) },
  { RS(DELTA_NUMA_HINT_FAULTS),               TS(sl_int) },
  { RS(DELTA_NUMA_HINT_FAULTS_LOCAL),         TS(sl_int) },
  { RS(DELTA_NUMA_HIT),                       TS(sl_int) },
  { RS(DELTA_NUMA_HUGE_PTE_UPDATES),          TS(sl_int) },
  { RS(DELTA_NUMA_INTERLEAVE),                TS(sl_int) },
  { RS(DELTA_NUMA_LOCAL),                     TS(sl_int) },
  { RS(DELTA_NUMA_MISS),                      TS(sl_int) },
  { RS(DELTA_NUMA_OTHER),                     TS(sl_int) },
  { RS(DELTA_NUMA_PAGES_MIGRATED),            TS(sl_int) },
  { RS(DELTA_NUMA_PTE_UPDATES),               TS(sl_int) },
  { RS(DELTA_PAGEOUTRUN),                     TS(sl_int) },
  { RS(DELTA_PGACTIVATE),                     TS(sl_int) },
  { RS(DELTA_PGALLOC_DMA),                    TS(sl_int) },
  { RS(DELTA_PGALLOC_DMA32),                  TS(sl_int) },
  { RS(DELTA_PGALLOC_MOVABLE),                TS(sl_int) },
  { RS(DELTA_PGALLOC_NORMAL),                 TS(sl_int) },
  { RS(DELTA_PGDEACTIVATE),                   TS(sl_int) },
  { RS(DELTA_PGFAULT),                        TS(sl_int) },
  { RS(DELTA_PGFREE),                         TS(sl_int) },
  { RS(DELTA_PGINODESTEAL),                   TS(sl_int) },
  { RS(DELTA_PGMAJFAULT),                     TS(sl_int) },
  { RS(DELTA_PGMIGRATE_FAIL),                 TS(sl_int) },
  { RS(DELTA_PGMIGRATE_SUCCESS),              TS(sl_int) },
  { RS(DELTA_PGPGIN),                         TS(sl_int) },
  { RS(DELTA_PGPGOUT),                        TS(sl_int) },
  { RS(DELTA_PGREFILL_DMA),                   TS(sl_int) },
  { RS(DELTA_PGREFILL_DMA32),                 TS(sl_int) },
  { RS(DELTA_PGREFILL_MOVABLE),               TS(sl_int) },
  { RS(DELTA_PGREFILL_NORMAL),                TS(sl_int) },
  { RS(DELTA_PGROTATED),                      TS(sl_int) },
  { RS(DELTA_PGSCAN_DIRECT_DMA),              TS(sl_int) },
  { RS(DELTA_PGSCAN_DIRECT_DMA32),            TS(sl_int) },
  { RS(DELTA_PGSCAN_DIRECT_MOVABLE),          TS(sl_int) },
  { RS(DELTA_PGSCAN_DIRECT_NORMAL),           TS(sl_int) },
  { RS(DELTA_PGSCAN_DIRECT_THROTTLE),         TS(sl_int) },
  { RS(DELTA_PGSCAN_KSWAPD_DMA),              TS(sl_int) },
  { RS(DELTA_PGSCAN_KSWAPD_DMA32),            TS(sl_int) },
  { RS(DELTA_PGSCAN_KSWAPD_MOVEABLE),         TS(sl_int) },
  { RS(DELTA_PGSCAN_KSWAPD_NORMAL),           TS(sl_int) },
  { RS(DELTA_PGSTEAL_DIRECT_DMA),             TS(sl_int) },
  { RS(DELTA_PGSTEAL_DIRECT_DMA32),           TS(sl_int) },
  { RS(DELTA_PGSTEAL_DIRECT_MOVABLE),         TS(sl_int) },
  { RS(DELTA_PGSTEAL_DIRECT_NORMAL),          TS(sl_int) },
  { RS(DELTA_PGSTEAL_KSWAPD_DMA),             TS(sl_int) },
  { RS(DELTA_PGSTEAL_KSWAPD_DMA32),           TS(sl_int) },
  { RS(DELTA_PGSTEAL_KSWAPD_MOVABLE),         TS(sl_int) },
  { RS(DELTA_PGSTEAL_KSWAPD_NORMAL),          TS(sl_int) },
  { RS(DELTA_PSWPIN),                         TS(sl_int) },
  { RS(DELTA_PSWPOUT),                        TS(sl_int) },
  { RS(DELTA_SLABS_SCANNED),                  TS(sl_int) },
  { RS(DELTA_THP_COLLAPSE_ALLOC),             TS(sl_int) },
  { RS(DELTA_THP_COLLAPSE_ALLOC_FAILED),      TS(sl_int) },
  { RS(DELTA_THP_FAULT_ALLOC),                TS(sl_int) },
  { RS(DELTA_THP_FAULT_FALLBACK),             TS(sl_int) },
  { RS(DELTA_THP_SPLIT),                      TS(sl_int) },
  { RS(DELTA_THP_ZERO_PAGE_ALLOC),            TS(sl_int) },
  { RS(DELTA_THP_ZERO_PAGE_ALLOC_FAILED),     TS(sl_int) },
  { RS(DELTA_UNEVICTABLE_PGS_CLEARED),        TS(sl_int) },
  { RS(DELTA_UNEVICTABLE_PGS_CULLED),         TS(sl_int) },
  { RS(DELTA_UNEVICTABLE_PGS_MLOCKED),        TS(sl_int) },
  { RS(DELTA_UNEVICTABLE_PGS_MUNLOCKED),      TS(sl_int) },
  { RS(DELTA_UNEVICTABLE_PGS_RESCUED),        TS(sl_int) },
  { RS(DELTA_UNEVICTABLE_PGS_SCANNED),        TS(sl_int) },
  { RS(DELTA_UNEVICTABLE_PGS_STRANDED),       TS(sl_int) },
  { RS(DELTA_WORKINGSET_ACTIVATE),            TS(sl_int) },
  { RS(DELTA_WORKINGSET_NODERECLAIM),         TS(sl_int) },
  { RS(DELTA_WORKINGSET_REFAULT),             TS(sl_int) },
  { RS(DELTA_ZONE_RECLAIM_FAILED),            TS(sl_int) },

 // dummy entry corresponding to VMSTAT_logical_end ...
  { NULL,                                     NULL        }
};

    /* please note,
     * this enum MUST be 1 greater than the highest value of any enum */
enum vmstat_item VMSTAT_logical_end = VMSTAT_DELTA_ZONE_RECLAIM_FAILED + 1;

#undef setNAME
#undef RS


// ___ Private Functions ||||||||||||||||||||||||||||||||||||||||||||||||||||||

static inline void assign_results (
        struct vmstat_stack *stack,
        struct vmstat_hist *hist)
{
    struct vmstat_result *this = stack->head;

    for (;;) {
        enum vmstat_item item = this->item;
        if (item >= VMSTAT_logical_end)
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
        if (this->item >= VMSTAT_logical_end)
            break;
        if (this->item > VMSTAT_noop)
            this->result.ul_int = 0;
        ++this;
    }
} // end: cleanup_stack


static inline void cleanup_stacks_all (
        struct vmstat_info *info)
{
    struct stacks_extent *ext = info->extents;
    int i;

    while (ext) {
        for (i = 0; ext->stacks[i]; i++)
            cleanup_stack(ext->stacks[i]->head);
        ext = ext->next;
    };
    info->dirty_stacks = 0;
} // end: cleanup_stacks_all


static void extents_free_all (
        struct vmstat_info *info)
{
    while (info->extents) {
        struct stacks_extent *p = info->extents;
        info->extents = info->extents->next;
        free(p);
    };
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
     * my_stack = procps_vmstat_select(info, VMSTAT_noop, num);
     *                                       ^~~~~~~~~~~~~~~~
     */
    if (numitems < 1
    || (void *)items < (void *)(unsigned long)(2 * VMSTAT_logical_end))
        return -1;

    for (i = 0; i < numitems; i++) {
        // a vmstat_item is currently unsigned, but we'll protect our future
        if (items[i] < 0)
            return -1;
        if (items[i] >= VMSTAT_logical_end)
            return -1;
    }

    return 0;
} // end: items_check_failed


static int make_hash_failed (
        struct vmstat_info *info)
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
        struct vmstat_info *info)
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
        return -1;
    buf[size] = '\0';

    head = buf;

    for (;;) {
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
    }

    // let's not distort the deltas the first time thru ...
    if (!info->vmstat_was_read) {
        memcpy(&info->hist.old, &info->hist.new, sizeof(struct vmstat_data));
        info->vmstat_was_read = 1;
    }
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
        struct vmstat_info *info,
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
 * Returns: < 0 on failure, 0 on success along with
 *          a pointer to a new context struct
 */
PROCPS_EXPORT int procps_vmstat_new (
        struct vmstat_info **info)
{
    struct vmstat_info *p;
    int rc;

    if (info == NULL || *info != NULL)
        return -EINVAL;
    if (!(p = calloc(1, sizeof(struct vmstat_info))))
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
        struct vmstat_info *info)
{
    if (info == NULL)
        return -EINVAL;

    info->refcount++;
    return info->refcount;
} // end: procps_vmstat_ref


PROCPS_EXPORT int procps_vmstat_unref (
        struct vmstat_info **info)
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

PROCPS_EXPORT struct vmstat_result *procps_vmstat_get (
        struct vmstat_info *info,
        enum vmstat_item item)
{
    static time_t sav_secs;
    time_t cur_secs;

    if (info == NULL)
        return NULL;
    if (item < 0 || item >= VMSTAT_logical_end)
        return NULL;

    /* we will NOT read the vmstat file with every call - rather, we'll offer
       a granularity of 1 second between reads ... */
    cur_secs = time(NULL);
    if (1 <= cur_secs - sav_secs) {
        if (read_vmstat_failed(info))
            return NULL;
        sav_secs = cur_secs;
    }

    info->get_this.item = item;
//  with 'get', we must NOT honor the usual 'noop' guarantee
//  if (item > VMSTAT_noop)
        info->get_this.result.ul_int = 0;
    Item_table[item].setsfunc(&info->get_this, &info->hist);

    return &info->get_this;
} // end: procps_vmstat_get


/* procps_vmstat_select():
 *
 * Harvest all the requested /proc/vmstat information then return
 * it in a results stack.
 *
 * Returns: pointer to a vmstat_stack struct on success, NULL on error.
 */
PROCPS_EXPORT struct vmstat_stack *procps_vmstat_select (
        struct vmstat_info *info,
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
        // allow for our VMSTAT_logical_end
        if (!(info->items = realloc(info->items, sizeof(enum vmstat_item) * (numitems + 1))))
            return NULL;
        memcpy(info->items, items, sizeof(enum vmstat_item) * numitems);
        info->items[numitems] = VMSTAT_logical_end;
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


// --- special debugging function(s) ------------------------------------------
/*
 *  The following isn't part of the normal programming interface.  Rather,
 *  it exists to validate result types referenced in application programs.
 *
 *  It's used only when:
 *      1) the 'XTRA_PROCPS_DEBUG' has been defined, or
 *      2) the '#include <proc/xtra-procps-debug.h>' used
 */

PROCPS_EXPORT struct vmstat_result *xtra_vmstat_get (
        struct vmstat_info *info,
        enum vmstat_item actual_enum,
        const char *typestr,
        const char *file,
        int lineno)
{
    struct vmstat_result *r = procps_vmstat_get(info, actual_enum);

    if (actual_enum < 0 || actual_enum >= VMSTAT_logical_end) {
        fprintf(stderr, "%s line %d: invalid item = %d, type = %s\n"
            , file, lineno, actual_enum, typestr);
    }
    if (r) {
        char *str = Item_table[r->item].type2str;
        if (str[0]
        && (strcmp(typestr, str)))
            fprintf(stderr, "%s line %d: was %s, expected %s\n", file, lineno, typestr, str);
    }
    return r;
} // end: xtra_vmstat_get_


PROCPS_EXPORT struct vmstat_result *xtra_vmstat_val (
        int relative_enum,
        const char *typestr,
        const struct vmstat_stack *stack,
        struct vmstat_info *info,
        const char *file,
        int lineno)
{
    char *str;
    int i;

    for (i = 0; stack->head[i].item < VMSTAT_logical_end; i++)
        ;
    if (relative_enum < 0 || relative_enum >= i) {
        fprintf(stderr, "%s line %d: invalid relative_enum = %d, type = %s\n"
            , file, lineno, relative_enum, typestr);
        return NULL;
    }
    str = Item_table[stack->head[relative_enum].item].type2str;
    if (str[0]
    && (strcmp(typestr, str))) {
        fprintf(stderr, "%s line %d: was %s, expected %s\n", file, lineno, typestr, str);
    }
    return &stack->head[relative_enum];
} // end: xtra_vmstat_val
