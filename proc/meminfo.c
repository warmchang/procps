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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <proc/procps-private.h>
#include <proc/meminfo.h>


#define MEMINFO_FILE "/proc/meminfo"

struct meminfo_data {
/** == man 5 proc shows these as: to be documented, so the hell with 'em */
    unsigned long Active;
    unsigned long Active_anon;         // as: (anon)
    unsigned long Active_file;         // as: (file)
    unsigned long AnonHugePages;
    unsigned long AnonPages;
    unsigned long Bounce;
    unsigned long Buffers;
    unsigned long Cached;
/** unsigned long CmaFree; ----------- */
/** unsigned long CmaTotal; ---------- */
    unsigned long CommitLimit;
    unsigned long Committed_AS;
/** unsigned long DirectMap1G; ------- */
/** unsigned long DirectMap2M; ------- */
/** unsigned long DirectMap4k; ------- */
    unsigned long Dirty;
    unsigned long HardwareCorrupted;
    unsigned long HighFree;
    unsigned long HighTotal;
    unsigned long HugePages_Free;
    unsigned long HugePages_Rsvd;
    unsigned long HugePages_Surp;
    unsigned long HugePages_Total;
    unsigned long Hugepagesize;
    unsigned long Inactive;
    unsigned long Inactive_anon;       // as: (anon)
    unsigned long Inactive_file;       // as: (file)
    unsigned long KernelStack;
    unsigned long LowFree;
    unsigned long LowTotal;
    unsigned long Mapped;
    unsigned long MemAvailable;
    unsigned long MemFree;
    unsigned long MemTotal;
    unsigned long Mlocked;
    unsigned long NFS_Unstable;
    unsigned long PageTables;
    unsigned long SReclaimable;
    unsigned long SUnreclaim;
    unsigned long Shmem;
    unsigned long Slab;
    unsigned long SwapCached;
    unsigned long SwapFree;
    unsigned long SwapTotal;
    unsigned long Unevictable;
    unsigned long VmallocChunk;
    unsigned long VmallocTotal;
    unsigned long VmallocUsed;
    unsigned long Writeback;
    unsigned long WritebackTmp;

    unsigned long derived_mem_hi_used;
    unsigned long derived_mem_lo_used;
    unsigned long derived_mem_used;
    unsigned long derived_swap_used;
};

struct hist_mem {
    struct meminfo_data new;
    struct meminfo_data old;
};

struct stacks_extent {
    int ext_numstacks;
    struct stacks_extent *next;
    struct meminfo_stack **stacks;
};

struct procps_meminfo {
    int refcount;
    int meminfo_fd;
    int meminfo_was_read;
    int dirty_stacks;
    struct hist_mem mem_hist;
    int numitems;
    enum meminfo_item *items;
    struct stacks_extent *extents;
};


// ___ Results 'Set' Support ||||||||||||||||||||||||||||||||||||||||||||||||||

#define setNAME(e) set_results_ ## e
#define setDECL(e) static void setNAME(e) \
    (struct meminfo_result *R, struct hist_mem *H)

// regular assignment
#define MEM_set(e,t,x) setDECL(e) { R->result. t = H->new . x; }
// delta assignment
#define HST_set(e,t,x) setDECL(e) { R->result. t = ( H->new . x - H->old. x ); \
    if (R->result. t < 0) R->result. t = 0; }

setDECL(noop)                { (void)R; (void)H; }
setDECL(extra)               { (void)R; (void)H; }

MEM_set(MEM_ACTIVE,             ul_int,  Active)
MEM_set(MEM_ACTIVE_ANON,        ul_int,  Active_anon)
MEM_set(MEM_ACTIVE_FILE,        ul_int,  Active_file)
MEM_set(MEM_ANON,               ul_int,  AnonPages)
MEM_set(MEM_AVAILABLE,          ul_int,  MemAvailable)
MEM_set(MEM_BOUNCE,             ul_int,  Bounce)
MEM_set(MEM_BUFFERS,            ul_int,  Buffers)
MEM_set(MEM_CACHED,             ul_int,  Cached)
MEM_set(MEM_COMMIT_LIMIT,       ul_int,  CommitLimit)
MEM_set(MEM_COMMITTED_AS,       ul_int,  Committed_AS)
MEM_set(MEM_HARD_CORRUPTED,     ul_int,  HardwareCorrupted)
MEM_set(MEM_DIRTY,              ul_int,  Dirty)
MEM_set(MEM_FREE,               ul_int,  MemFree)
MEM_set(MEM_HUGE_ANON,          ul_int,  AnonHugePages)
MEM_set(MEM_HUGE_FREE,          ul_int,  HugePages_Free)
MEM_set(MEM_HUGE_RSVD,          ul_int,  HugePages_Rsvd)
MEM_set(MEM_HUGE_SIZE,          ul_int,  Hugepagesize)
MEM_set(MEM_HUGE_SURPLUS,       ul_int,  HugePages_Surp)
MEM_set(MEM_HUGE_TOTAL,         ul_int,  HugePages_Total)
MEM_set(MEM_INACTIVE,           ul_int,  Inactive)
MEM_set(MEM_INACTIVE_ANON,      ul_int,  Inactive_anon)
MEM_set(MEM_INACTIVE_FILE,      ul_int,  Inactive_file)
MEM_set(MEM_KERNEL_STACK,       ul_int,  KernelStack)
MEM_set(MEM_LOCKED,             ul_int,  Mlocked)
MEM_set(MEM_MAPPED,             ul_int,  Mapped)
MEM_set(MEM_NFS_UNSTABLE,       ul_int,  NFS_Unstable)
MEM_set(MEM_PAGE_TABLES,        ul_int,  PageTables)
MEM_set(MEM_SHARED,             ul_int,  Shmem)
MEM_set(MEM_SLAB,               ul_int,  Slab)
MEM_set(MEM_SLAB_RECLAIM,       ul_int,  SReclaimable)
MEM_set(MEM_SLAB_UNRECLAIM,     ul_int,  SUnreclaim)
MEM_set(MEM_TOTAL,              ul_int,  MemTotal)
MEM_set(MEM_UNEVICTABLE,        ul_int,  Unevictable)
MEM_set(MEM_USED,               ul_int,  derived_mem_used)
MEM_set(MEM_VM_ALLOC_CHUNK,     ul_int,  VmallocChunk)
MEM_set(MEM_VM_ALLOC_TOTAL,    ull_int,  VmallocTotal)
MEM_set(MEM_VM_ALLOC_USED,      ul_int,  VmallocUsed)
MEM_set(MEM_WRITEBACK,          ul_int,  Writeback)
MEM_set(MEM_WRITEBACK_TMP,      ul_int,  WritebackTmp)

HST_set(DELTA_ACTIVE,            s_int,  Active)
HST_set(DELTA_ACTIVE_ANON,       s_int,  Active_anon)
HST_set(DELTA_ACTIVE_FILE,       s_int,  Active_file)
HST_set(DELTA_ANON,              s_int,  AnonPages)
HST_set(DELTA_AVAILABLE,         s_int,  MemAvailable)
HST_set(DELTA_BOUNCE,            s_int,  Bounce)
HST_set(DELTA_BUFFERS,           s_int,  Buffers)
HST_set(DELTA_CACHED,            s_int,  Cached)
HST_set(DELTA_COMMIT_LIMIT,      s_int,  CommitLimit)
HST_set(DELTA_COMMITTED_AS,      s_int,  Committed_AS)
HST_set(DELTA_HARD_CORRUPTED,    s_int,  HardwareCorrupted)
HST_set(DELTA_DIRTY,             s_int,  Dirty)
HST_set(DELTA_FREE,              s_int,  MemFree)
HST_set(DELTA_HUGE_ANON,         s_int,  AnonHugePages)
HST_set(DELTA_HUGE_FREE,         s_int,  HugePages_Free)
HST_set(DELTA_HUGE_RSVD,         s_int,  HugePages_Rsvd)
HST_set(DELTA_HUGE_SIZE,         s_int,  Hugepagesize)
HST_set(DELTA_HUGE_SURPLUS,      s_int,  HugePages_Surp)
HST_set(DELTA_HUGE_TOTAL,        s_int,  HugePages_Total)
HST_set(DELTA_INACTIVE,          s_int,  Inactive)
HST_set(DELTA_INACTIVE_ANON,     s_int,  Inactive_anon)
HST_set(DELTA_INACTIVE_FILE,     s_int,  Inactive_file)
HST_set(DELTA_KERNEL_STACK,      s_int,  KernelStack)
HST_set(DELTA_LOCKED,            s_int,  Mlocked)
HST_set(DELTA_MAPPED,            s_int,  Mapped)
HST_set(DELTA_NFS_UNSTABLE,      s_int,  NFS_Unstable)
HST_set(DELTA_PAGE_TABLES,       s_int,  PageTables)
HST_set(DELTA_SHARED,            s_int,  Shmem)
HST_set(DELTA_SLAB,              s_int,  Slab)
HST_set(DELTA_SLAB_RECLAIM,      s_int,  SReclaimable)
HST_set(DELTA_SLAB_UNRECLAIM,    s_int,  SUnreclaim)
HST_set(DELTA_TOTAL,             s_int,  MemTotal)
HST_set(DELTA_UNEVICTABLE,       s_int,  Unevictable)
HST_set(DELTA_USED,              s_int,  derived_mem_used)
HST_set(DELTA_VM_ALLOC_CHUNK,    s_int,  VmallocChunk)
HST_set(DELTA_VM_ALLOC_TOTAL,   sl_int,  VmallocTotal)
HST_set(DELTA_VM_ALLOC_USED,     s_int,  VmallocUsed)
HST_set(DELTA_WRITEBACK,         s_int,  Writeback)
HST_set(DELTA_WRITEBACK_TMP,     s_int,  WritebackTmp)

MEM_set(MEMHI_FREE,             ul_int,  HighFree)
MEM_set(MEMHI_TOTAL,            ul_int,  HighTotal)
MEM_set(MEMHI_USED,             ul_int,  derived_mem_hi_used)

MEM_set(MEMLO_FREE,             ul_int,  LowFree)
MEM_set(MEMLO_TOTAL,            ul_int,  LowTotal)
MEM_set(MEMLO_USED,             ul_int,  derived_mem_lo_used)

MEM_set(SWAP_CACHED,            ul_int,  SwapCached)
MEM_set(SWAP_FREE,              ul_int,  SwapFree)
MEM_set(SWAP_TOTAL,             ul_int,  SwapTotal)
MEM_set(SWAP_USED,              ul_int,  derived_swap_used)


// ___ Results 'Get' Support ||||||||||||||||||||||||||||||||||||||||||||||||||

#define getNAME(e) get_results_ ## e
#define getDECL(e) static signed long getNAME(e) \
    (struct procps_meminfo *I)

// regular get
#define MEM_get(e,x) getDECL(e) { return I->mem_hist.new. x; }
// delta get
#define HST_get(e,x) getDECL(e) { int n = I->mem_hist.new. x - I->mem_hist.old. x; \
    return ( n < 0 ? 0 : n ); }

getDECL(noop)                { (void)I; return 0; }
getDECL(extra)               { (void)I; return 0; }

MEM_get(MEM_ACTIVE,            Active)
MEM_get(MEM_ACTIVE_ANON,       Active_anon)
MEM_get(MEM_ACTIVE_FILE,       Active_file)
MEM_get(MEM_ANON,              AnonPages)
MEM_get(MEM_AVAILABLE,         MemAvailable)
MEM_get(MEM_BOUNCE,            Bounce)
MEM_get(MEM_BUFFERS,           Buffers)
MEM_get(MEM_CACHED,            Cached)
MEM_get(MEM_COMMIT_LIMIT,      CommitLimit)
MEM_get(MEM_COMMITTED_AS,      Committed_AS)
MEM_get(MEM_HARD_CORRUPTED,    HardwareCorrupted)
MEM_get(MEM_DIRTY,             Dirty)
MEM_get(MEM_FREE,              MemFree)
MEM_get(MEM_HUGE_ANON,         AnonHugePages)
MEM_get(MEM_HUGE_FREE,         HugePages_Free)
MEM_get(MEM_HUGE_RSVD,         HugePages_Rsvd)
MEM_get(MEM_HUGE_SIZE,         Hugepagesize)
MEM_get(MEM_HUGE_SURPLUS,      HugePages_Surp)
MEM_get(MEM_HUGE_TOTAL,        HugePages_Total)
MEM_get(MEM_INACTIVE,          Inactive)
MEM_get(MEM_INACTIVE_ANON,     Inactive_anon)
MEM_get(MEM_INACTIVE_FILE,     Inactive_file)
MEM_get(MEM_KERNEL_STACK,      KernelStack)
MEM_get(MEM_LOCKED,            Mlocked)
MEM_get(MEM_MAPPED,            Mapped)
MEM_get(MEM_NFS_UNSTABLE,      NFS_Unstable)
MEM_get(MEM_PAGE_TABLES,       PageTables)
MEM_get(MEM_SHARED,            Shmem)
MEM_get(MEM_SLAB,              Slab)
MEM_get(MEM_SLAB_RECLAIM,      SReclaimable)
MEM_get(MEM_SLAB_UNRECLAIM,    SUnreclaim)
MEM_get(MEM_TOTAL,             MemTotal)
MEM_get(MEM_UNEVICTABLE,       Unevictable)
MEM_get(MEM_USED,              derived_mem_used)
MEM_get(MEM_VM_ALLOC_CHUNK,    VmallocChunk)
MEM_get(MEM_VM_ALLOC_TOTAL,    VmallocTotal)
MEM_get(MEM_VM_ALLOC_USED,     VmallocUsed)
MEM_get(MEM_WRITEBACK,         Writeback)
MEM_get(MEM_WRITEBACK_TMP,     WritebackTmp)

HST_get(DELTA_ACTIVE,          Active)
HST_get(DELTA_ACTIVE_ANON,     Active_anon)
HST_get(DELTA_ACTIVE_FILE,     Active_file)
HST_get(DELTA_ANON,            AnonPages)
HST_get(DELTA_AVAILABLE,       MemAvailable)
HST_get(DELTA_BOUNCE,          Bounce)
HST_get(DELTA_BUFFERS,         Buffers)
HST_get(DELTA_CACHED,          Cached)
HST_get(DELTA_COMMIT_LIMIT,    CommitLimit)
HST_get(DELTA_COMMITTED_AS,    Committed_AS)
HST_get(DELTA_HARD_CORRUPTED,  HardwareCorrupted)
HST_get(DELTA_DIRTY,           Dirty)
HST_get(DELTA_FREE,            MemFree)
HST_get(DELTA_HUGE_ANON,       AnonHugePages)
HST_get(DELTA_HUGE_FREE,       HugePages_Free)
HST_get(DELTA_HUGE_RSVD,       HugePages_Rsvd)
HST_get(DELTA_HUGE_SIZE,       Hugepagesize)
HST_get(DELTA_HUGE_SURPLUS,    HugePages_Surp)
HST_get(DELTA_HUGE_TOTAL,      HugePages_Total)
HST_get(DELTA_INACTIVE,        Inactive)
HST_get(DELTA_INACTIVE_ANON,   Inactive_anon)
HST_get(DELTA_INACTIVE_FILE,   Inactive_file)
HST_get(DELTA_KERNEL_STACK,    KernelStack)
HST_get(DELTA_LOCKED,          Mlocked)
HST_get(DELTA_MAPPED,          Mapped)
HST_get(DELTA_NFS_UNSTABLE,    NFS_Unstable)
HST_get(DELTA_PAGE_TABLES,     PageTables)
HST_get(DELTA_SHARED,          Shmem)
HST_get(DELTA_SLAB,            Slab)
HST_get(DELTA_SLAB_RECLAIM,    SReclaimable)
HST_get(DELTA_SLAB_UNRECLAIM,  SUnreclaim)
HST_get(DELTA_TOTAL,           MemTotal)
HST_get(DELTA_UNEVICTABLE,     Unevictable)
HST_get(DELTA_USED,            derived_mem_used)
HST_get(DELTA_VM_ALLOC_CHUNK,  VmallocChunk)
HST_get(DELTA_VM_ALLOC_TOTAL,  VmallocTotal)
HST_get(DELTA_VM_ALLOC_USED,   VmallocUsed)
HST_get(DELTA_WRITEBACK,       Writeback)
HST_get(DELTA_WRITEBACK_TMP,   WritebackTmp)

MEM_get(MEMHI_FREE,            HighFree)
MEM_get(MEMHI_TOTAL,           HighTotal)
MEM_get(MEMHI_USED,            derived_mem_hi_used)

MEM_get(MEMLO_FREE,            LowFree)
MEM_get(MEMLO_TOTAL,           LowTotal)
MEM_get(MEMLO_USED,            derived_mem_lo_used)

MEM_get(SWAP_CACHED,           SwapCached)
MEM_get(SWAP_FREE,             SwapFree)
MEM_get(SWAP_TOTAL,            SwapTotal)
MEM_get(SWAP_USED,             derived_swap_used)


// ___ Controlling Table ||||||||||||||||||||||||||||||||||||||||||||||||||||||

typedef void (*SET_t)(struct meminfo_result *, struct hist_mem *);
#define RS(e) (SET_t)setNAME(e)

typedef long (*GET_t)(struct procps_meminfo *);
#define RG(e) (GET_t)getNAME(e)


        /*
         * Need it be said?
         * This table must be kept in the exact same order as
         * those 'enum meminfo_item' guys ! */
static struct {
    SET_t setsfunc;              // the actual result setting routine
    GET_t getsfunc;              // a routine to return single result
} Item_table[] = {
/*  setsfunc                     getsfunc
    ---------------------------  ---------------------------  */
  { RS(noop),                    RG(noop)                     },
  { RS(extra),                   RG(extra)                    },

  { RS(MEM_ACTIVE),              RG(MEM_ACTIVE)               },
  { RS(MEM_ACTIVE_ANON),         RG(MEM_ACTIVE_ANON)          },
  { RS(MEM_ACTIVE_FILE),         RG(MEM_ACTIVE_FILE)          },
  { RS(MEM_ANON),                RG(MEM_ANON)                 },
  { RS(MEM_AVAILABLE),           RG(MEM_AVAILABLE)            },
  { RS(MEM_BOUNCE),              RG(MEM_BOUNCE)               },
  { RS(MEM_BUFFERS),             RG(MEM_BUFFERS)              },
  { RS(MEM_CACHED),              RG(MEM_CACHED)               },
  { RS(MEM_COMMIT_LIMIT),        RG(MEM_COMMIT_LIMIT)         },
  { RS(MEM_COMMITTED_AS),        RG(MEM_COMMITTED_AS)         },
  { RS(MEM_HARD_CORRUPTED),      RG(MEM_HARD_CORRUPTED)       },
  { RS(MEM_DIRTY),               RG(MEM_DIRTY)                },
  { RS(MEM_FREE),                RG(MEM_FREE)                 },
  { RS(MEM_HUGE_ANON),           RG(MEM_HUGE_ANON)            },
  { RS(MEM_HUGE_FREE),           RG(MEM_HUGE_FREE)            },
  { RS(MEM_HUGE_RSVD),           RG(MEM_HUGE_RSVD)            },
  { RS(MEM_HUGE_SIZE),           RG(MEM_HUGE_SIZE)            },
  { RS(MEM_HUGE_SURPLUS),        RG(MEM_HUGE_SURPLUS)         },
  { RS(MEM_HUGE_TOTAL),          RG(MEM_HUGE_TOTAL)           },
  { RS(MEM_INACTIVE),            RG(MEM_INACTIVE)             },
  { RS(MEM_INACTIVE_ANON),       RG(MEM_INACTIVE_ANON)        },
  { RS(MEM_INACTIVE_FILE),       RG(MEM_INACTIVE_FILE)        },
  { RS(MEM_KERNEL_STACK),        RG(MEM_KERNEL_STACK)         },
  { RS(MEM_LOCKED),              RG(MEM_LOCKED)               },
  { RS(MEM_MAPPED),              RG(MEM_MAPPED)               },
  { RS(MEM_NFS_UNSTABLE),        RG(MEM_NFS_UNSTABLE)         },
  { RS(MEM_PAGE_TABLES),         RG(MEM_PAGE_TABLES)          },
  { RS(MEM_SHARED),              RG(MEM_SHARED)               },
  { RS(MEM_SLAB),                RG(MEM_SLAB)                 },
  { RS(MEM_SLAB_RECLAIM),        RG(MEM_SLAB_RECLAIM)         },
  { RS(MEM_SLAB_UNRECLAIM),      RG(MEM_SLAB_UNRECLAIM)       },
  { RS(MEM_TOTAL),               RG(MEM_TOTAL)                },
  { RS(MEM_UNEVICTABLE),         RG(MEM_UNEVICTABLE)          },
  { RS(MEM_USED),                RG(MEM_USED)                 },
  { RS(MEM_VM_ALLOC_CHUNK),      RG(MEM_VM_ALLOC_CHUNK)       },
  { RS(MEM_VM_ALLOC_TOTAL),      RG(MEM_VM_ALLOC_TOTAL)       },
  { RS(MEM_VM_ALLOC_USED),       RG(MEM_VM_ALLOC_USED)        },
  { RS(MEM_WRITEBACK),           RG(MEM_WRITEBACK)            },
  { RS(MEM_WRITEBACK_TMP),       RG(MEM_WRITEBACK_TMP)        },

  { RS(DELTA_ACTIVE),            RG(DELTA_ACTIVE)             },
  { RS(DELTA_ACTIVE_ANON),       RG(DELTA_ACTIVE_ANON)        },
  { RS(DELTA_ACTIVE_FILE),       RG(DELTA_ACTIVE_FILE)        },
  { RS(DELTA_ANON),              RG(DELTA_ANON)               },
  { RS(DELTA_AVAILABLE),         RG(DELTA_AVAILABLE)          },
  { RS(DELTA_BOUNCE),            RG(DELTA_BOUNCE)             },
  { RS(DELTA_BUFFERS),           RG(DELTA_BUFFERS)            },
  { RS(DELTA_CACHED),            RG(DELTA_CACHED)             },
  { RS(DELTA_COMMIT_LIMIT),      RG(DELTA_COMMIT_LIMIT)       },
  { RS(DELTA_COMMITTED_AS),      RG(DELTA_COMMITTED_AS)       },
  { RS(DELTA_HARD_CORRUPTED),    RG(DELTA_HARD_CORRUPTED)     },
  { RS(DELTA_DIRTY),             RG(DELTA_DIRTY)              },
  { RS(DELTA_FREE),              RG(DELTA_FREE)               },
  { RS(DELTA_HUGE_ANON),         RG(DELTA_HUGE_ANON)          },
  { RS(DELTA_HUGE_FREE),         RG(DELTA_HUGE_FREE)          },
  { RS(DELTA_HUGE_RSVD),         RG(DELTA_HUGE_RSVD)          },
  { RS(DELTA_HUGE_SIZE),         RG(DELTA_HUGE_SIZE)          },
  { RS(DELTA_HUGE_SURPLUS),      RG(DELTA_HUGE_SURPLUS)       },
  { RS(DELTA_HUGE_TOTAL),        RG(DELTA_HUGE_TOTAL)         },
  { RS(DELTA_INACTIVE),          RG(DELTA_INACTIVE)           },
  { RS(DELTA_INACTIVE_ANON),     RG(DELTA_INACTIVE_ANON)      },
  { RS(DELTA_INACTIVE_FILE),     RG(DELTA_INACTIVE_FILE)      },
  { RS(DELTA_KERNEL_STACK),      RG(DELTA_KERNEL_STACK)       },
  { RS(DELTA_LOCKED),            RG(DELTA_LOCKED)             },
  { RS(DELTA_MAPPED),            RG(DELTA_MAPPED)             },
  { RS(DELTA_NFS_UNSTABLE),      RG(DELTA_NFS_UNSTABLE)       },
  { RS(DELTA_PAGE_TABLES),       RG(DELTA_PAGE_TABLES)        },
  { RS(DELTA_SHARED),            RG(DELTA_SHARED)             },
  { RS(DELTA_SLAB),              RG(DELTA_SLAB)               },
  { RS(DELTA_SLAB_RECLAIM),      RG(DELTA_SLAB_RECLAIM)       },
  { RS(DELTA_SLAB_UNRECLAIM),    RG(DELTA_SLAB_UNRECLAIM)     },
  { RS(DELTA_TOTAL),             RG(DELTA_TOTAL)              },
  { RS(DELTA_UNEVICTABLE),       RG(DELTA_UNEVICTABLE)        },
  { RS(DELTA_USED),              RG(DELTA_USED)               },
  { RS(DELTA_VM_ALLOC_CHUNK),    RG(DELTA_VM_ALLOC_CHUNK)     },
  { RS(DELTA_VM_ALLOC_TOTAL),    RG(DELTA_VM_ALLOC_TOTAL)     },
  { RS(DELTA_VM_ALLOC_USED),     RG(DELTA_VM_ALLOC_USED)      },
  { RS(DELTA_WRITEBACK),         RG(DELTA_WRITEBACK)          },
  { RS(DELTA_WRITEBACK_TMP),     RG(DELTA_WRITEBACK_TMP)      },

  { RS(MEMHI_FREE),              RG(MEMHI_FREE)               },
  { RS(MEMHI_TOTAL),             RG(MEMHI_TOTAL)              },
  { RS(MEMHI_USED),              RG(MEMHI_USED)               },
  { RS(MEMLO_FREE),              RG(MEMLO_FREE)               },
  { RS(MEMLO_TOTAL),             RG(MEMLO_TOTAL)              },
  { RS(MEMLO_USED),              RG(MEMLO_USED)               },

  { RS(SWAP_CACHED),             RG(SWAP_CACHED)              },
  { RS(SWAP_FREE),               RG(SWAP_FREE)                },
  { RS(SWAP_TOTAL),              RG(SWAP_TOTAL)               },
  { RS(SWAP_USED),               RG(SWAP_USED)                },

  { NULL,                        NULL                         }
};

    /* please note,
     * this enum MUST be 1 greater than the highest value of any enum */
enum meminfo_item PROCPS_MEMINFO_logical_end = PROCPS_MEMINFO_SWAP_USED + 1;

#undef setNAME
#undef setDECL
#undef MEM_set
#undef HST_set
#undef getNAME
#undef getDECL
#undef MEM_get
#undef HST_get

// ___ Private Functions ||||||||||||||||||||||||||||||||||||||||||||||||||||||

static inline void assign_results (
        struct meminfo_stack *stack,
        struct hist_mem *mem_hist)
{
    struct meminfo_result *this = stack->head;

    for (;;) {
        enum meminfo_item item = this->item;
        if (item >= PROCPS_MEMINFO_logical_end)
            break;
        Item_table[item].setsfunc(this, mem_hist);
        ++this;
    }
    return;
} // end: assign_results


static inline void cleanup_stack (
        struct meminfo_result *this)
{
    for (;;) {
        if (this->item >= PROCPS_MEMINFO_logical_end)
            break;
        if (this->item > PROCPS_MEMINFO_noop)
            this->result.ull_int = 0;
        ++this;
    }
} // end: cleanup_stack


static inline void cleanup_stacks_all (
        struct procps_meminfo *info)
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
        struct procps_meminfo *info)
{
    do {
        struct stacks_extent *p = info->extents;
        info->extents = info->extents->next;
        free(p);
    } while (info->extents);
} // end: extents_free_all


static inline struct meminfo_result *itemize_stack (
        struct meminfo_result *p,
        int depth,
        enum meminfo_item *items)
{
    struct meminfo_result *p_sav = p;
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
        enum meminfo_item *items)
{
    int i;

    /* if an enum is passed instead of an address of one or more enums, ol' gcc
     * will silently convert it to an address (possibly NULL).  only clang will
     * offer any sort of warning like the following:
     *
     * warning: incompatible integer to pointer conversion passing 'int' to parameter of type 'enum meminfo_item *'
     * my_stack = procps_meminfo_select(info, PROCPS_MEMINFO_noop, num);
     *                                     ^~~~~~~~~~~~~~~~
     */
    if (numitems < 1
    || (void *)items < (void *)(unsigned long)(2 * PROCPS_MEMINFO_logical_end))
        return -1;

    for (i = 0; i < numitems; i++) {
        // a meminfo_item is currently unsigned, but we'll protect our future
        if (items[i] < 0)
            return -1;
        if (items[i] >= PROCPS_MEMINFO_logical_end)
            return -1;
    }

    return 0;
} // end: items_check_failed


/*
 * read_meminfo_failed():
 *
 * Read the data out of /proc/meminfo putting the information
 * into the supplied info structure
 */
PROCPS_EXPORT int read_meminfo_failed (
        struct procps_meminfo *info)
{
 /* a 'memory history reference' macro for readability,
    so we can focus the field names ... */
 #define mHr(f) info->mem_hist.new. f
    char buf[8192];
    char *head, *tail;
    int size;
    unsigned long *valptr;
    signed long mem_used;

    if (info == NULL)
        return -1;

    // remember history from last time around
    memcpy(&info->mem_hist.old, &info->mem_hist.new, sizeof(struct meminfo_data));
    // clear out the soon to be 'current' values
    memset(&info->mem_hist.new, 0, sizeof(struct meminfo_data));

    if (-1 == info->meminfo_fd
    && (info->meminfo_fd = open(MEMINFO_FILE, O_RDONLY)) == -1)
        return -errno;

    if (lseek(info->meminfo_fd, 0L, SEEK_SET) == -1)
        return -errno;

    for (;;) {
        if ((size = read(info->meminfo_fd, buf, sizeof(buf)-1)) < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            return -errno;
        }
        break;
    }
    if (size == 0)
        return 0;
    buf[size] = '\0';

    /* Scan the file */
    head = buf;
    do {
        tail = strchr(head, ' ');
        if (!tail)
            break;
        *tail = '\0';
        valptr = NULL;
        switch (*head) {
            case 'A':
                if (0 == strcmp(head, "Active:"))
                    valptr = &(mHr(Active));
                else
                if (0 == strcmp(head, "Active(anon):"))
                    valptr = &(mHr(Active_anon));
                else
                if (0 == strcmp(head, "Active(file):"))
                    valptr = &(mHr(Active_file));
                else
                if (0 == strcmp(head, "AnonHugePages:"))
                    valptr = &(mHr(AnonHugePages));
                else
                if (0 == strcmp(head, "AnonPages:"))
                    valptr = &(mHr(AnonPages));
                break;
            case 'B':
                if (0 == strcmp(head, "Bounce:"))
                    valptr = &(mHr(Bounce));
                else
                if (0 == strcmp(head, "Buffers:"))
                    valptr = &(mHr(Buffers));
                break;
            case 'C':
                if (0 == strcmp(head, "Cached:"))
                    valptr = &(mHr(Cached));
                else
                if (0 == strcmp(head, "CommitLimit:"))
                    valptr = &(mHr(CommitLimit));
                else
                if (0 == strcmp(head, "Committed_AS:"))
                    valptr = &(mHr(Committed_AS));
                break;
            case 'D':
                if (0 == strcmp(head, "Dirty:"))
                    valptr = &(mHr(Dirty));
                break;
            case 'H':
                if (0 == strcmp(head, "HardwareCorrupted:"))
                    valptr = &(mHr(HardwareCorrupted));
                else
                if (0 == strcmp(head, "HighFree:"))
                    valptr = &(mHr(HighFree));
                else
                if (0 == strcmp(head, "HighTotal:"))
                    valptr = &(mHr(HighTotal));
                else
                if (0 == strcmp(head, "HugePages_Free:"))
                    valptr = &(mHr(HugePages_Free));
                else
                if (0 == strcmp(head, "HugePages_Rsvd:"))
                    valptr = &(mHr(HugePages_Rsvd));
                else
                if (0 == strcmp(head, "HugePages_Surp:"))
                    valptr = &(mHr(HugePages_Surp));
                else
                if (0 == strcmp(head, "HugePages_Total:"))
                    valptr = &(mHr(HugePages_Total));
                else
                if (0 == strcmp(head, "Hugepagesize:"))
                    valptr = &(mHr(Hugepagesize));
                break;
            case 'I':
                if (0 == strcmp(head, "Inactive:"))
                    valptr = &(mHr(Inactive));
                else
                if (0 == strcmp(head, "Inactive(anon):"))
                    valptr = &(mHr(Inactive_anon));
                else
                if (0 == strcmp(head, "Inactive(file):"))
                    valptr = &(mHr(Inactive_file));
                break;
            case 'K':
                if (0 == strcmp(head, "KernelStack:"))
                    valptr = &(mHr(KernelStack));
                break;
            case 'L':
                if (0 == strcmp(head, "LowFree:"))
                    valptr = &(mHr(LowFree));
                else
                if (0 == strcmp(head, "LowTotal:"))
                    valptr = &(mHr(LowTotal));
                break;
            case 'M':
                if (0 == strcmp(head, "Mapped:"))
                    valptr = &(mHr(Mapped));
                else
                if (0 == strcmp(head, "MemAvailable:"))
                    valptr = &(mHr(MemAvailable));
                else
                if (0 == strcmp(head, "MemFree:"))
                    valptr = &(mHr(MemFree));
                else
                if (0 == strcmp(head, "MemTotal:"))
                    valptr = &(mHr(MemTotal));
                else
                if (0 == strcmp(head, "Mlocked:"))
                    valptr = &(mHr(Mlocked));
                break;
            case 'N':
                if (0 == strcmp(head, "NFS_Unstable:"))
                    valptr = &(mHr(NFS_Unstable));
                break;
            case 'P':
                if (0 == strcmp(head, "PageTables:"))
                    valptr = &(mHr(PageTables));
                break;
            case 'S':
                if (0 == strcmp(head, "SReclaimable:"))
                    valptr = &(mHr(SReclaimable));
                else
                if (0 == strcmp(head, "SUnreclaim:"))
                    valptr = &(mHr(SUnreclaim));
                else
                if (0 == strcmp(head, "Shmem:"))
                    valptr = &(mHr(Shmem));
                else
                if (0 == strcmp(head, "Slab:"))
                    valptr = &(mHr(Slab));
                else
                if (0 == strcmp(head, "SwapCached:"))
                    valptr = &(mHr(SwapCached));
                else
                if (0 == strcmp(head, "SwapFree:"))
                    valptr = &(mHr(SwapFree));
                else
                if (0 == strcmp(head, "SwapTotal:"))
                    valptr = &(mHr(SwapTotal));
                break;
            case 'U':
                if (0 == strcmp(head, "Unevictable:"))
                    valptr = &(mHr(Unevictable));
                break;
            case 'V':
                if (0 == strcmp(head, "VmallocChunk:"))
                    valptr = &(mHr(VmallocChunk));
                else
                if (0 == strcmp(head, "VmallocTotal:"))
                    valptr = &(mHr(VmallocTotal));
                else
                if (0 == strcmp(head, "VmallocUsed:"))
                    valptr = &(mHr(VmallocUsed));
                break;
            case 'W':
                if (0 == strcmp(head, "Writeback:"))
                    valptr = &(mHr(Writeback));
                else
                if (0 == strcmp(head, "WritebackTmp:"))
                    valptr = &(mHr(WritebackTmp));
                break;
            default:
                break;
        }
        head = tail+1;
        if (valptr) {
            *valptr = strtoul(head, &tail, 10);
        }
        tail = strchr(head, '\n');
        if (!tail)
            break;
        head = tail + 1;
    } while(tail);

    if (0 == mHr(MemAvailable))
        mHr(MemAvailable) = mHr(MemFree);
    /* if 'available' is greater than 'total' or our calculation of mem_used
       overflows, that's symptomatic of running within a lxc container where
       such values will be dramatically distorted over those of the host. */
    if (mHr(MemAvailable) > mHr(MemTotal))
        mHr(MemAvailable) = mHr(MemFree);

    mem_used = mHr(MemTotal) - mHr(MemFree) - mHr(Cached) - mHr(Buffers);
    if (mem_used < 0)
        mem_used = mHr(MemTotal) - mHr(MemFree);
    mHr(derived_mem_used) = (unsigned long)mem_used;

    if (mHr(HighFree) < mHr(HighTotal))
         mHr(derived_mem_hi_used) = mHr(HighTotal) - mHr(HighFree);

    mHr(Cached) += mHr(SReclaimable);

    if (0 == mHr(LowTotal)) {
        mHr(LowTotal) = mHr(MemTotal);
        mHr(LowFree)  = mHr(MemFree);
    }
    if (mHr(LowFree) < mHr(LowTotal))
        mHr(derived_mem_lo_used) = mHr(LowTotal) - mHr(LowFree);

    if (mHr(SwapFree) < mHr(SwapTotal))
        mHr(derived_swap_used) = mHr(SwapTotal) - mHr(SwapFree);

    // let's not distort the deltas the first time thru ...
    if (!info->meminfo_was_read)
        memcpy(&info->mem_hist.old, &info->mem_hist.new, sizeof(struct meminfo_data));
    info->meminfo_was_read = 1;

    return 0;
 #undef mHr
} // end: read_meminfo_failed


/*
 * stacks_alloc():
 *
 * Allocate and initialize one or more stacks each of which is anchored in an
 * associated meminfo_stack structure.
 *
 * All such stacks will have their result structures properly primed with
 * 'items', while the result itself will be zeroed.
 *
 * Returns a stacks_extent struct anchoring the 'heads' of each new stack.
 */
static struct stacks_extent *stacks_alloc (
        struct procps_meminfo *info,
        int maxstacks)
{
    struct stacks_extent *p_blob;
    struct meminfo_stack **p_vect;
    struct meminfo_stack *p_head;
    size_t vect_size, head_size, list_size, blob_size;
    void *v_head, *v_list;
    int i;

    if (info == NULL || info->items == NULL)
        return NULL;
    if (maxstacks < 1)
        return NULL;

    vect_size  = sizeof(void *) * maxstacks;                   // size of the addr vectors |
    vect_size += sizeof(void *);                               // plus NULL addr delimiter |
    head_size  = sizeof(struct meminfo_stack);                 // size of that head struct |
    list_size  = sizeof(struct meminfo_result)*info->numitems; // any single results stack |
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
        p_head = (struct meminfo_stack *)v_head;
        p_head->head = itemize_stack((struct meminfo_result *)v_list, info->numitems, info->items);
        p_blob->stacks[i] = p_head;
        v_list += list_size;
        v_head += head_size;
    }
    p_blob->ext_numstacks = maxstacks;
    return p_blob;
} // end: stacks_alloc


// ___ Public Functions |||||||||||||||||||||||||||||||||||||||||||||||||||||||

/*
 * procps_meminfo_new:
 *
 * Create a new container to hold the stat information
 *
 * The initial refcount is 1, and needs to be decremented
 * to release the resources of the structure.
 *
 * Returns: a pointer to a new meminfo struct
 */
PROCPS_EXPORT int procps_meminfo_new (
        struct procps_meminfo **info)
{
    struct procps_meminfo *p;

    if (info == NULL || *info != NULL)
        return -EINVAL;
    if (!(p = calloc(1, sizeof(struct procps_meminfo))))
        return -ENOMEM;

    p->refcount = 1;
    p->meminfo_fd = -1;

    *info = p;
    return 0;
} // end: procps_meminfo_new


PROCPS_EXPORT int procps_meminfo_ref (
        struct procps_meminfo *info)
{
    if (info == NULL)
        return -EINVAL;

    info->refcount++;
    return info->refcount;
} // end: procps_meminfo_ref


PROCPS_EXPORT int procps_meminfo_unref (
        struct procps_meminfo **info)
{
    if (info == NULL || *info == NULL)
        return -EINVAL;
    (*info)->refcount--;

    if ((*info)->refcount == 0) {
        if ((*info)->extents)
            extents_free_all((*info));
        if ((*info)->items)
            free((*info)->items);
        free(*info);
        *info = NULL;
        return 0;
    }
    return (*info)->refcount;
} // end: procps_meminfo_unref


PROCPS_EXPORT signed long procps_meminfo_get (
        struct procps_meminfo *info,
        enum meminfo_item item)
{
    static time_t sav_secs;
    time_t cur_secs;
    int rc;

    /* we will NOT read the meminfo file with every call - rather, we'll offer
       a granularity of 1 second between reads ... */
    cur_secs = time(NULL);
    if (1 <= cur_secs - sav_secs) {
        if ((rc = read_meminfo_failed(info)))
            return rc;
        sav_secs = cur_secs;
    }

    if (item < PROCPS_MEMINFO_logical_end)
        return Item_table[item].getsfunc(info);
    return -EINVAL;
} // end: procps_meminfo_get


/* procps_meminfo_select():
 *
 * Harvest all the requested MEM and/or SWAP information then return
 * it in a results stack.
 *
 * Returns: pointer to a meminfo_stack struct on success, NULL on error.
 */
PROCPS_EXPORT struct meminfo_stack *procps_meminfo_select (
        struct procps_meminfo *info,
        enum meminfo_item *items,
        int numitems)
{
    if (info == NULL || items == NULL)
        return NULL;
    if (items_check_failed(numitems, items))
        return NULL;

    /* is this the first time or have things changed since we were last called?
       if so, gotta' redo all of our stacks stuff ... */
    if (info->numitems != numitems + 1
    || memcmp(info->items, items, sizeof(enum meminfo_item) * numitems)) {
        // allow for our PROCPS_MEMINFO_logical_end
        if (!(info->items = realloc(info->items, sizeof(enum meminfo_item) * (numitems + 1))))
            return -ENOMEM;
        memcpy(info->items, items, sizeof(enum meminfo_item) * numitems);
        info->items[numitems] = PROCPS_MEMINFO_logical_end;
        info->numitems = numitems + 1;
        if (info->extents)
            extents_free_all(info);
    }
    if (!info->extents
    && !(info->extents = stacks_alloc(info, 1)))
       return NULL;

    if (info->dirty_stacks)
        cleanup_stacks_all(info);

    if (read_meminfo_failed(info))
        return NULL;
    assign_results(info->extents->stacks[0], &info->mem_hist);
    info->dirty_stacks = 1;

    return info->extents->stacks[0];
} // end: procps_meminfo_select
