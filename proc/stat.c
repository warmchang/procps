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

#ifndef NUMA_DISABLE
#include <dlfcn.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <proc/sysinfo.h>

#include <proc/procps-private.h>
#include <proc/stat.h>


#define STAT_FILE "/proc/stat"

#define STACKS_INCR   64               // amount reap stack allocations grow
#define NEWOLD_INCR   32               // amount jiffs hist allocations grow

/* ------------------------------------------------------------------------- +
   a strictly development #define, existing specifically for the top program |
   ( and it has no affect if ./configure --disable-numa has been specified ) | */
//#define PRETEND_NUMA     // pretend there are 3 'discontiguous' numa nodes |
// ------------------------------------------------------------------------- +

/* ------------------------------------------------------------------------- +
   because 'reap' would be forced to duplicate the global SYS stuff in every |
   TIC type results stack, the following #define can be used to enforce that |
   only STAT_noop and STAT_extra plus all the STAT_TIC items will be allowed | */
//#define ENFORCE_LOGICAL  // ensure only logical items are accepted by reap |
// ------------------------------------------------------------------------- +


struct stat_jifs {
    unsigned long long user, nice, system, idle, iowait, irq, sirq, stolen, guest, gnice;
};

struct stat_data {
    unsigned long intr;
    unsigned long ctxt;
    unsigned long btime;
    unsigned long procs_created;
    unsigned long procs_blocked;
    unsigned long procs_running;
};

struct hist_sys {
    struct stat_data new;
    struct stat_data old;
};

struct hist_tic {
    int id;
    int numa_node;
    struct stat_jifs new;
    struct stat_jifs old;
};

struct stacks_extent {
    int ext_numstacks;
    struct stacks_extent *next;
    struct stat_stack **stacks;
};

struct item_support {
    int num;                           // includes 'logical_end' delimiter
    enum stat_item *enums;             // includes 'logical_end' delimiter
};

struct ext_support {
    struct item_support *items;        // how these stacks are configured
    struct stacks_extent *extents;     // anchor for these extents
    int dirty_stacks;
};

struct tic_support {
    int n_alloc;                       // number of below structs allocated
    int n_inuse;                       // number of below structs occupied
    struct hist_tic *tics;             // actual new/old jiffies
};

struct reap_support {
    int total;                         // independently obtained # of cpus/nodes
    struct ext_support fetch;          // extents plus items details
    struct tic_support hist;           // cpu and node jiffies management
    int n_alloc;                       // last known anchor pointers allocation
    struct stat_stack **anchor;        // reapable stacks (consolidated extents)
    int n_alloc_save;                  // last known results.stacks allocation
    struct stat_reap result;           // summary + stacks returned to caller
};

struct stat_info {
    int refcount;
    int stat_fd;
    int stat_was_read;                 // is stat file history valid?
    struct hist_sys sys_hist;          // SYS type management
    struct hist_tic cpu_hist;          // TIC type management for cpu summary
    struct reap_support cpus;          // TIC type management for real cpus
    struct reap_support nodes;         // TIC type management for numa nodes
    struct ext_support cpu_summary;    // supports /proc/stat line #1 results
    struct ext_support select;         // support for 'procps_stat_select()'
    struct stat_reaped results;        // for return to caller after a reap
#ifndef NUMA_DISABLE
    void *libnuma_handle;              // if dlopen() for libnuma succeessful
    int (*our_max_node)(void);         // a libnuma function call via dlsym()
    int (*our_node_of_cpu)(int);       // a libnuma function call via dlsym()
#endif
    struct stat_result get_this;       // for return to caller after a get
    struct item_support reap_items;    // items used for reap (shared among 3)
    struct item_support select_items;  // items unique to select
};


// ___ Results 'Set' Support ||||||||||||||||||||||||||||||||||||||||||||||||||

#define setNAME(e) set_stat_ ## e
#define setDECL(e) static void setNAME(e) \
    (struct stat_result *R, struct hist_sys *S, struct hist_tic *T)

// regular assignment
#define TIC_set(e,t,x) setDECL(e) { \
    (void)S; R->result. t = T->new . x; }
#define SYS_set(e,t,x) setDECL(e) { \
    (void)T; R->result. t = S->new . x; }
// delta assignment
#define TICsetH(e,t,x) setDECL(e) { \
    (void)S; R->result. t = ( T->new . x - T->old. x ); \
    if (R->result. t < 0) R->result. t = 0; }
#define SYSsetH(e,t,x) setDECL(e) { \
    (void)T; R->result. t = ( S->new . x - S->old. x ); \
    if (R->result. t < 0) R->result. t = 0; }

setDECL(noop)                   { (void)R; (void)S; (void)T; }
setDECL(extra)                  { (void)R; (void)S; (void)T; }

setDECL(TIC_ID)                 { (void)S; R->result.s_int = T->id;  }
setDECL(TIC_NUMA_NODE)          { (void)S; R->result.s_int = T->numa_node; }

TIC_set(TIC_USER,                 ull_int,  user)
TIC_set(TIC_NICE,                 ull_int,  nice)
TIC_set(TIC_SYSTEM,               ull_int,  system)
TIC_set(TIC_IDLE,                 ull_int,  idle)
TIC_set(TIC_IOWAIT,               ull_int,  iowait)
TIC_set(TIC_IRQ,                  ull_int,  irq)
TIC_set(TIC_SOFTIRQ,              ull_int,  sirq)
TIC_set(TIC_STOLEN,               ull_int,  stolen)
TIC_set(TIC_GUEST,                ull_int,  guest)
TIC_set(TIC_GUEST_NICE,           ull_int,  gnice)

TICsetH(TIC_DELTA_USER,           sl_int,   user)
TICsetH(TIC_DELTA_NICE,           sl_int,   nice)
TICsetH(TIC_DELTA_SYSTEM,         sl_int,   system)
TICsetH(TIC_DELTA_IDLE,           sl_int,   idle)
TICsetH(TIC_DELTA_IOWAIT,         sl_int,   iowait)
TICsetH(TIC_DELTA_IRQ,            sl_int,   irq)
TICsetH(TIC_DELTA_SOFTIRQ,        sl_int,   sirq)
TICsetH(TIC_DELTA_STOLEN,         sl_int,   stolen)
TICsetH(TIC_DELTA_GUEST,          sl_int,   guest)
TICsetH(TIC_DELTA_GUEST_NICE,     sl_int,   gnice)

SYS_set(SYS_CTX_SWITCHES,         ul_int,   ctxt)
SYS_set(SYS_INTERRUPTS,           ul_int,   intr)
SYS_set(SYS_PROC_BLOCKED,         ul_int,   procs_blocked)
SYS_set(SYS_PROC_CREATED,         ul_int,   procs_created)
SYS_set(SYS_PROC_RUNNING,         ul_int,   procs_running)
SYS_set(SYS_TIME_OF_BOOT,         ul_int,   btime)

SYSsetH(SYS_DELTA_CTX_SWITCHES,   s_int,    ctxt)
SYSsetH(SYS_DELTA_INTERRUPTS,     s_int,    intr)
setDECL(SYS_DELTA_PROC_BLOCKED) { (void)T; R->result.s_int = S->new.procs_blocked - S->old.procs_blocked; }
SYSsetH(SYS_DELTA_PROC_CREATED,   s_int,    procs_created)
setDECL(SYS_DELTA_PROC_RUNNING) { (void)T; R->result.s_int = S->new.procs_running - S->old.procs_running; }

#undef setDECL
#undef TIC_set
#undef SYS_set
#undef TICsetH
#undef SYSsetH


// ___ Sorting Support ||||||||||||||||||||||||||||||||||||||||||||||||||||||||

struct sort_parms {
    int offset;
    enum stat_sort_order order;
};

#define srtNAME(t) sort_stat_ ## t
#define srtDECL(t) static int srtNAME(t) \
    (const struct stat_stack **A, const struct stat_stack **B, struct sort_parms *P)

srtDECL(s_int) {
    const struct stat_result *a = (*A)->head + P->offset; \
    const struct stat_result *b = (*B)->head + P->offset; \
    return P->order * (a->result.s_int - b->result.s_int);
}

srtDECL(sl_int) {
    const struct stat_result *a = (*A)->head + P->offset; \
    const struct stat_result *b = (*B)->head + P->offset; \
    return P->order * (a->result.sl_int - b->result.sl_int);
}

srtDECL(ul_int) {
    const struct stat_result *a = (*A)->head + P->offset; \
    const struct stat_result *b = (*B)->head + P->offset; \
    if ( a->result.ul_int > b->result.ul_int ) return P->order > 0 ?  1 : -1; \
    if ( a->result.ul_int < b->result.ul_int ) return P->order > 0 ? -1 :  1; \
    return 0;
}

srtDECL(ull_int) {
    const struct stat_result *a = (*A)->head + P->offset; \
    const struct stat_result *b = (*B)->head + P->offset; \
    if ( a->result.ull_int > b->result.ull_int ) return P->order > 0 ?  1 : -1; \
    if ( a->result.ull_int < b->result.ull_int ) return P->order > 0 ? -1 :  1; \
    return 0;
}

srtDECL(noop) { \
    (void)A; (void)B; (void)P; \
    return 0;
}

#undef srtDECL


// ___ Controlling Table ||||||||||||||||||||||||||||||||||||||||||||||||||||||

typedef void (*SET_t)(struct stat_result *, struct hist_sys *, struct hist_tic *);
#define RS(e) (SET_t)setNAME(e)

typedef int  (*QSR_t)(const void *, const void *, void *);
#define QS(t) (QSR_t)srtNAME(t)

#define TS(t) STRINGIFY(t)
#define TS_noop ""

        /*
         * Need it be said?
         * This table must be kept in the exact same order as
         * those 'enum stat_item' guys ! */
static struct {
    SET_t setsfunc;              // the actual result setting routine
    QSR_t sortfunc;              // sort cmp func for a specific type
    char *type2str;              // the result type as a string value
} Item_table[] = {
/*  setsfunc                     sortfunc      type2str
    ---------------------------  ------------  ----------- */
  { RS(noop),                    QS(noop),     TS_noop     },
  { RS(extra),                   QS(ull_int),  TS_noop     },

  { RS(TIC_ID),                  QS(s_int),    TS(s_int)   },
  { RS(TIC_NUMA_NODE),           QS(s_int),    TS(s_int)   },
  { RS(TIC_USER),                QS(ull_int),  TS(ull_int) },
  { RS(TIC_NICE),                QS(ull_int),  TS(ull_int) },
  { RS(TIC_SYSTEM),              QS(ull_int),  TS(ull_int) },
  { RS(TIC_IDLE),                QS(ull_int),  TS(ull_int) },
  { RS(TIC_IOWAIT),              QS(ull_int),  TS(ull_int) },
  { RS(TIC_IRQ),                 QS(ull_int),  TS(ull_int) },
  { RS(TIC_SOFTIRQ),             QS(ull_int),  TS(ull_int) },
  { RS(TIC_STOLEN),              QS(ull_int),  TS(ull_int) },
  { RS(TIC_GUEST),               QS(ull_int),  TS(ull_int) },
  { RS(TIC_GUEST_NICE),          QS(ull_int),  TS(ull_int) },

  { RS(TIC_DELTA_USER),          QS(sl_int),   TS(sl_int)  },
  { RS(TIC_DELTA_NICE),          QS(sl_int),   TS(sl_int)  },
  { RS(TIC_DELTA_SYSTEM),        QS(sl_int),   TS(sl_int)  },
  { RS(TIC_DELTA_IDLE),          QS(sl_int),   TS(sl_int)  },
  { RS(TIC_DELTA_IOWAIT),        QS(sl_int),   TS(sl_int)  },
  { RS(TIC_DELTA_IRQ),           QS(sl_int),   TS(sl_int)  },
  { RS(TIC_DELTA_SOFTIRQ),       QS(sl_int),   TS(sl_int)  },
  { RS(TIC_DELTA_STOLEN),        QS(sl_int),   TS(sl_int)  },
  { RS(TIC_DELTA_GUEST),         QS(sl_int),   TS(sl_int)  },
  { RS(TIC_DELTA_GUEST_NICE),    QS(sl_int),   TS(sl_int)  },

  { RS(SYS_CTX_SWITCHES),        QS(ul_int),   TS(ul_int)  },
  { RS(SYS_INTERRUPTS),          QS(ul_int),   TS(ul_int)  },
  { RS(SYS_PROC_BLOCKED),        QS(ul_int),   TS(ul_int)  },
  { RS(SYS_PROC_CREATED),        QS(ul_int),   TS(ul_int)  },
  { RS(SYS_PROC_RUNNING),        QS(ul_int),   TS(ul_int)  },
  { RS(SYS_TIME_OF_BOOT),        QS(ul_int),   TS(ul_int)  },

  { RS(SYS_DELTA_CTX_SWITCHES),  QS(s_int),    TS(s_int)   },
  { RS(SYS_DELTA_INTERRUPTS),    QS(s_int),    TS(s_int)   },
  { RS(SYS_DELTA_PROC_BLOCKED),  QS(s_int),    TS(s_int)   },
  { RS(SYS_DELTA_PROC_CREATED),  QS(s_int),    TS(s_int)   },
  { RS(SYS_DELTA_PROC_RUNNING),  QS(s_int),    TS(s_int)   },

 // dummy entry corresponding to STAT_logical_end ...
  { NULL,                        NULL,         NULL        }
};

    /* please note,
     * 1st enum MUST be kept in sync with highest TIC type
     * 2nd enum MUST be 1 greater than the highest value of any enum */
#ifdef ENFORCE_LOGICAL
enum stat_item STAT_TIC_highest = STAT_TIC_DELTA_GUEST_NICE;
#endif
enum stat_item STAT_logical_end = STAT_SYS_DELTA_PROC_RUNNING + 1;

#undef setNAME
#undef srtNAME
#undef RS
#undef QS


// ___ Private Functions ||||||||||||||||||||||||||||||||||||||||||||||||||||||

#ifndef NUMA_DISABLE
 #ifdef PRETEND_NUMA
static int fake_max_node (void) { return 3; }
static int fake_node_of_cpu (int n) { return (1 == (n % 4)) ? 0 : (n % 4); }
 #endif
#endif


static inline void stat_assign_results (
        struct stat_stack *stack,
        struct hist_sys *sys_hist,
        struct hist_tic *tic_hist)
{
    struct stat_result *this = stack->head;

    for (;;) {
        enum stat_item item = this->item;
        if (item >= STAT_logical_end)
            break;
        Item_table[item].setsfunc(this, sys_hist, tic_hist);
        ++this;
    }
    return;
} // end: stat_assign_results


static inline void stat_cleanup_stack (
        struct stat_result *this)
{
    for (;;) {
        if (this->item >= STAT_logical_end)
            break;
        if (this->item > STAT_noop)
            this->result.ull_int = 0;
        ++this;
    }
} // end: stat_cleanup_stack


static inline void stat_cleanup_stacks_all (
        struct ext_support *this)
{
    struct stacks_extent *ext = this->extents;
    int i;

    while (ext) {
        for (i = 0; ext->stacks[i]; i++)
            stat_cleanup_stack(ext->stacks[i]->head);
        ext = ext->next;
    };
    this->dirty_stacks = 0;
} // end: stat_cleanup_stacks_all


static void stat_extents_free_all (
        struct ext_support *this)
{
    while (this->extents) {
        struct stacks_extent *p = this->extents;
        this->extents = this->extents->next;
        free(p);
    };
} // end: stat_extents_free_all


static inline struct stat_result *stat_itemize_stack (
        struct stat_result *p,
        int depth,
        enum stat_item *items)
{
    struct stat_result *p_sav = p;
    int i;

    for (i = 0; i < depth; i++) {
        p->item = items[i];
        p->result.ull_int = 0;
        ++p;
    }
    return p_sav;
} // end: stat_itemize_stack


static inline int stat_items_check_failed (
        int numitems,
        enum stat_item *items)
{
    int i;

    /* if an enum is passed instead of an address of one or more enums, ol' gcc
     * will silently convert it to an address (possibly NULL).  only clang will
     * offer any sort of warning like the following:
     *
     * warning: incompatible integer to pointer conversion passing 'int' to parameter of type 'enum stat_item *'
     * my_stack = procps_stat_select(info, STAT_noop, num);
     *                                     ^~~~~~~~~~~~~~~~
     */
    if (numitems < 1
    || (void *)items < (void *)(unsigned long)(2 * STAT_logical_end))
        return -1;

    for (i = 0; i < numitems; i++) {
        // a stat_item is currently unsigned, but we'll protect our future
        if (items[i] < 0)
            return -1;
        if (items[i] >= STAT_logical_end) {
            return -1;
        }
    }
    return 0;
} // end: stat_items_check_failed


static int stat_make_numa_hist (
        struct stat_info *info)
{
#ifndef NUMA_DISABLE
    struct hist_tic *cpu_ptr, *nod_ptr;
    int i, node;

    if (info->libnuma_handle == NULL
    || (!info->nodes.total)) {
        return 0;
    }

    /* are numa nodes dynamic like online cpus can be?
       ( and be careful, this libnuma call returns the highest node id in use, )
       ( NOT an actual number of nodes - some of those 'slots' might be unused ) */
    info->nodes.total = info->our_max_node() + 1;

    if (!info->nodes.hist.n_alloc
    || !(info->nodes.total < info->nodes.hist.n_alloc)) {
        info->nodes.hist.n_alloc = info->nodes.total + NEWOLD_INCR;
        info->nodes.hist.tics = realloc(info->nodes.hist.tics, info->nodes.hist.n_alloc * sizeof(struct hist_tic));
        if (!(info->nodes.hist.tics))
            return -ENOMEM;
    }

    // forget all of the prior node statistics & anticipate unassigned slots
    memset(info->nodes.hist.tics, 0, info->nodes.hist.n_alloc * sizeof(struct hist_tic));
    nod_ptr = info->nodes.hist.tics;
    for (i = 0; i < info->nodes.total; i++) {
        nod_ptr->id = nod_ptr->numa_node = STAT_NODE_INVALID;
        ++nod_ptr;
    }

    // spin thru each cpu and value the jiffs for it's numa node
    for (i = 0; i < info->cpus.hist.n_inuse; i++) {
        cpu_ptr = info->cpus.hist.tics + i;
        if (-1 < (node = info->our_node_of_cpu(cpu_ptr->id))) {
            nod_ptr = info->nodes.hist.tics + node;
            nod_ptr->new.user   += cpu_ptr->new.user;   nod_ptr->old.user   += cpu_ptr->old.user;
            nod_ptr->new.nice   += cpu_ptr->new.nice;   nod_ptr->old.nice   += cpu_ptr->old.nice;
            nod_ptr->new.system += cpu_ptr->new.system; nod_ptr->old.system += cpu_ptr->old.system;
            nod_ptr->new.idle   += cpu_ptr->new.idle;   nod_ptr->old.idle   += cpu_ptr->old.idle;
            nod_ptr->new.iowait += cpu_ptr->new.iowait; nod_ptr->old.iowait += cpu_ptr->old.iowait;
            nod_ptr->new.irq    += cpu_ptr->new.irq;    nod_ptr->old.irq    += cpu_ptr->old.irq;
            nod_ptr->new.sirq   += cpu_ptr->new.sirq;   nod_ptr->old.sirq   += cpu_ptr->old.sirq;
            nod_ptr->new.stolen += cpu_ptr->new.stolen; nod_ptr->old.stolen += cpu_ptr->old.stolen;
            /*
             * note: the above call to 'our_node_of_cpu' will produce a modest
             *       memory leak summarized as:
             *          ==1234== LEAK SUMMARY:
             *          ==1234==    definitely lost: 512 bytes in 1 blocks
             *          ==1234==    indirectly lost: 48 bytes in 2 blocks
             *          ==1234==    ...
             * [ thanks very much libnuma, for all the pain you've caused us ]
             */
            cpu_ptr->numa_node = node;
            nod_ptr->id = node;
        }
    }
    info->nodes.hist.n_inuse = info->nodes.total;
    return info->nodes.hist.n_inuse;
#else
    return 0;
#endif
} // end: stat_make_numa_hist


static int stat_read_failed (
        struct stat_info *info)
{
    struct hist_tic *sum_ptr, *cpu_ptr;
    char buf[8192], *bp, *b;
    int i, rc, size;
    unsigned long long llnum = 0;

    if (info == NULL)
        return -EINVAL;

    if (!info->cpus.hist.n_alloc) {
        info->cpus.hist.tics = calloc(NEWOLD_INCR, sizeof(struct hist_tic));
        if (!(info->cpus.hist.tics))
            return -ENOMEM;
        info->cpus.hist.n_alloc = NEWOLD_INCR;
        info->cpus.hist.n_inuse = 0;
    }

    if (-1 == info->stat_fd && (info->stat_fd = open(STAT_FILE, O_RDONLY)) == -1)
        return -errno;
    if (lseek(info->stat_fd, 0L, SEEK_SET) == -1)
        return -errno;

    for (;;) {
        if ((size = read(info->stat_fd, buf, sizeof(buf)-1)) < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            return -errno;
        }
        break;
    }
    if (size == 0) {
        return -EIO;
    }
    buf[size] = '\0';
    bp = buf;

    sum_ptr = &info->cpu_hist;
    // remember summary from last time around
    memcpy(&sum_ptr->old, &sum_ptr->new, sizeof(struct stat_jifs));

    sum_ptr->id = STAT_SUMMARY_ID;              // mark as summary
    sum_ptr->numa_node = STAT_NODE_INVALID;     // mark as invalid

    // now value the cpu summary tics from line #1
    if (8 > sscanf(bp, "cpu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu"
        , &sum_ptr->new.user,  &sum_ptr->new.nice,   &sum_ptr->new.system
        , &sum_ptr->new.idle,  &sum_ptr->new.iowait, &sum_ptr->new.irq
        , &sum_ptr->new.sirq,  &sum_ptr->new.stolen
        , &sum_ptr->new.guest, &sum_ptr->new.gnice))
            return -1;
    // let's not distort the deltas the first time thru ...
    if (!info->stat_was_read)
        memcpy(&sum_ptr->old, &sum_ptr->new, sizeof(struct stat_jifs));

    i = 0;
reap_em_again:
    cpu_ptr = info->cpus.hist.tics + i;   // adapt to relocated if reap_em_again

    do {
        bp = 1 + strchr(bp, '\n');
        // remember this cpu from last time around
        memcpy(&cpu_ptr->old, &cpu_ptr->new, sizeof(struct stat_jifs));
        // next can be overridden under 'stat_make_numa_hist'
        cpu_ptr->numa_node = STAT_NODE_INVALID;

        if (8 > (rc = sscanf(bp, "cpu%d %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu"
            , &cpu_ptr->id
            , &cpu_ptr->new.user,  &cpu_ptr->new.nice,   &cpu_ptr->new.system
            , &cpu_ptr->new.idle,  &cpu_ptr->new.iowait, &cpu_ptr->new.irq
            , &cpu_ptr->new.sirq,  &cpu_ptr->new.stolen
            , &cpu_ptr->new.guest, &cpu_ptr->new.gnice))) {
                int id_sav = cpu_ptr->id;
                memmove(cpu_ptr, sum_ptr, sizeof(struct hist_tic));
                cpu_ptr->id = id_sav;
                break;                   // we must tolerate cpus taken offline
        }
        // let's not distort the deltas the first time thru ...
        if (!info->stat_was_read)
            memcpy(&cpu_ptr->old, &cpu_ptr->new, sizeof(struct stat_jifs));
        ++i;
        ++cpu_ptr;
    } while (i < info->cpus.hist.n_alloc);

    if (i == info->cpus.hist.n_alloc && rc >= 8) {
        info->cpus.hist.n_alloc += NEWOLD_INCR;
        info->cpus.hist.tics = realloc(info->cpus.hist.tics, info->cpus.hist.n_alloc * sizeof(struct hist_tic));
        if (!(info->cpus.hist.tics))
            return -ENOMEM;
        goto reap_em_again;
    }

    info->cpus.total = info->cpus.hist.n_inuse = i;

    // remember sys_hist stuff from last time around
    memcpy(&info->sys_hist.old, &info->sys_hist.new, sizeof(struct stat_data));

    llnum = 0;
    if ((b = strstr(bp, "intr ")))
        sscanf(b,  "intr %llu", &llnum);
    info->sys_hist.new.intr = llnum;

    llnum = 0;
    if ((b = strstr(bp, "ctxt ")))
        sscanf(b,  "ctxt %llu", &llnum);
    info->sys_hist.new.ctxt = llnum;

    llnum = 0;
    if ((b = strstr(bp, "btime ")))
        sscanf(b,  "btime %llu", &llnum);
    info->sys_hist.new.btime = llnum;

    llnum = 0;
    if ((b = strstr(bp, "processes ")))
        sscanf(b,  "processes %llu", &llnum);
    info->sys_hist.new.procs_created = llnum;

    llnum = 0;
    if ((b = strstr(bp, "procs_blocked ")))
        sscanf(b,  "procs_blocked %llu", &llnum);
    info->sys_hist.new.procs_blocked = llnum;

    llnum = 0;
    if ((b = strstr(bp, "procs_running ")))
        sscanf(b,  "procs_running %llu", &llnum);
    info->sys_hist.new.procs_running = llnum;

    // let's not distort the deltas the first time thru ...
    if (!info->stat_was_read) {
        memcpy(&info->sys_hist.old, &info->sys_hist.new, sizeof(struct stat_data));
        info->stat_was_read = 1;
    }
    return 0;
} // end: stat_read_failed


/*
 * stat_stacks_alloc():
 *
 * Allocate and initialize one or more stacks each of which is anchored in an
 * associated context structure.
 *
 * All such stacks will have their result structures properly primed with
 * 'items', while the result itself will be zeroed.
 *
 * Returns a stack_extent struct anchoring the 'heads' of each new stack.
 */
static struct stacks_extent *stat_stacks_alloc (
        struct ext_support *this,
        int maxstacks)
{
    struct stacks_extent *p_blob;
    struct stat_stack **p_vect;
    struct stat_stack *p_head;
    size_t vect_size, head_size, list_size, blob_size;
    void *v_head, *v_list;
    int i;

    if (this == NULL || this->items == NULL)
        return NULL;
    if (maxstacks < 1)
        return NULL;

    vect_size  = sizeof(void *) * maxstacks;                     // size of the addr vectors |
    vect_size += sizeof(void *);                                 // plus NULL addr delimiter |
    head_size  = sizeof(struct stat_stack);                      // size of that head struct |
    list_size  = sizeof(struct stat_result) * this->items->num;  // any single results stack |
    blob_size  = sizeof(struct stacks_extent);                   // the extent anchor itself |
    blob_size += vect_size;                                      // plus room for addr vects |
    blob_size += head_size * maxstacks;                          // plus room for head thing |
    blob_size += list_size * maxstacks;                          // plus room for our stacks |

    /* note: all of our memory is allocated in one single blob, facilitating a later free(). |
             as a minimum, it is important that those result structures themselves always be |
             contiguous within each stack since they are accessed through relative position. | */
    if (NULL == (p_blob = calloc(1, blob_size)))
        return NULL;

    p_blob->next = this->extents;                                // push this extent onto... |
    this->extents = p_blob;                                      // ...some existing extents |
    p_vect = (void *)p_blob + sizeof(struct stacks_extent);      // prime our vector pointer |
    p_blob->stacks = p_vect;                                     // set actual vectors start |
    v_head = (void *)p_vect + vect_size;                         // prime head pointer start |
    v_list = v_head + (head_size * maxstacks);                   // prime our stacks pointer |

    for (i = 0; i < maxstacks; i++) {
        p_head = (struct stat_stack *)v_head;
        p_head->head = stat_itemize_stack((struct stat_result *)v_list, this->items->num, this->items->enums);
        p_blob->stacks[i] = p_head;
        v_list += list_size;
        v_head += head_size;
    }
    p_blob->ext_numstacks = maxstacks;
    return p_blob;
} // end: stat_stacks_alloc


static int stat_stacks_fetch (
        struct stat_info *info,
        struct reap_support *this)
{
 #define n_alloc  this->n_alloc
 #define n_inuse  this->hist.n_inuse
 #define n_saved  this->n_alloc_save
    struct stacks_extent *ext;
    int i;

    if (this == NULL)
        return -EINVAL;

    // initialize stuff -----------------------------------
    if (!this->anchor) {
        if (!(this->anchor = calloc(sizeof(void *), STACKS_INCR)))
            return -ENOMEM;
        n_alloc = STACKS_INCR;
    }
    if (!this->fetch.extents) {
        if (!(ext = stat_stacks_alloc(&this->fetch, n_alloc)))
            return -ENOMEM;
        memcpy(this->anchor, ext->stacks, sizeof(void *) * n_alloc);
    }
    if (this->fetch.dirty_stacks)
        stat_cleanup_stacks_all(&this->fetch);

    // iterate stuff --------------------------------------
    for (i = 0; i < n_inuse; i++) {
        if (!(i < n_alloc)) {
            n_alloc += STACKS_INCR;
            if ((!(this->anchor = realloc(this->anchor, sizeof(void *) * n_alloc)))
            || (!(ext = stat_stacks_alloc(&this->fetch, STACKS_INCR)))) {
                return -ENOMEM;
            }
            memcpy(this->anchor + i, ext->stacks, sizeof(void *) * STACKS_INCR);
        }
        stat_assign_results(this->anchor[i], &info->sys_hist, &this->hist.tics[i]);
    }

    // finalize stuff -------------------------------------
    /* note: we go to this trouble of maintaining a duplicate of the consolidated |
             extent stacks addresses represented as our 'anchor' since these ptrs |
             are exposed to a user (um, not that we don't trust 'em or anything). |
             plus, we can NULL delimit these ptrs which we couldn't do otherwise. | */
    if (n_saved < i + 1) {
        n_saved = i + 1;
        if (!(this->result.stacks = realloc(this->result.stacks, sizeof(void *) * n_saved)))
            return -ENOMEM;
    }
    memcpy(this->result.stacks, this->anchor, sizeof(void *) * i);
    this->result.stacks[i] = NULL;
    this->result.total = i;
    this->fetch.dirty_stacks = 1;

    // callers beware, this might be zero (maybe no libnuma.so) ...
    return this->result.total;
 #undef n_alloc
 #undef n_inuse
 #undef n_saved
} // end: stat_stacks_fetch


static int stat_stacks_reconfig_maybe (
        struct ext_support *this,
        enum stat_item *items,
        int numitems)
{
    if (stat_items_check_failed(numitems, items))
        return -EINVAL;

    /* is this the first time or have things changed since we were last called?
       if so, gotta' redo all of our stacks stuff ... */
    if (this->items->num != numitems + 1
    || memcmp(this->items->enums, items, sizeof(enum stat_item) * numitems)) {
        // allow for our STAT_logical_end
        if (!(this->items->enums = realloc(this->items->enums, sizeof(enum stat_item) * (numitems + 1))))
            return -ENOMEM;
        memcpy(this->items->enums, items, sizeof(enum stat_item) * numitems);
        this->items->enums[numitems] = STAT_logical_end;
        this->items->num = numitems + 1;
        stat_extents_free_all(this);
        return 1;
    }
    return 0;
} // end: stat_stacks_reconfig_maybe


static struct stat_stack *stat_update_single_stack (
        struct stat_info *info,
        struct ext_support *this)
{
    if (!this->extents
    && !(stat_stacks_alloc(this, 1)))
       return NULL;

    if (this->dirty_stacks)
        stat_cleanup_stacks_all(this);

    stat_assign_results(this->extents->stacks[0], &info->sys_hist, &info->cpu_hist);
    this->dirty_stacks = 1;

    return this->extents->stacks[0];
} // end: stat_update_single_stack


#if defined(PRETEND_NUMA) && defined(NUMA_DISABLE)
# warning 'PRETEND_NUMA' ignored, 'NUMA_DISABLE' is active
#endif


// ___ Public Functions |||||||||||||||||||||||||||||||||||||||||||||||||||||||

// --- standard required functions --------------------------------------------

/*
 * procps_stat_new:
 *
 * Create a new container to hold the stat information
 *
 * The initial refcount is 1, and needs to be decremented
 * to release the resources of the structure.
 *
 * Returns: < 0 on failure, 0 on success along with
 *          a pointer to a new context struct
 */
PROCPS_EXPORT int procps_stat_new (
        struct stat_info **info)
{
    struct stat_info *p;

    if (info == NULL || *info != NULL)
        return -EINVAL;
    if (!(p = calloc(1, sizeof(struct stat_info))))
        return -ENOMEM;

    p->refcount = 1;
    p->stat_fd = -1;

    p->results.cpus = &p->cpus.result;
    p->results.nodes = &p->nodes.result;
    p->cpus.total = procps_cpu_count();

    // these 3 are for reap, sharing a single set of items
    p->cpu_summary.items = p->cpus.fetch.items = p->nodes.fetch.items = &p->reap_items;

    // the select guy has its own set of items
    p->select.items = &p->select_items;

#ifndef NUMA_DISABLE
 #ifndef PRETEND_NUMA
    // we'll try for the most recent version, then a version we know works...
    if ((p->libnuma_handle = dlopen("libnuma.so", RTLD_LAZY))
    || (p->libnuma_handle = dlopen("libnuma.so.1", RTLD_LAZY))) {
        p->our_max_node = dlsym(p->libnuma_handle, "numa_max_node");
        p->our_node_of_cpu = dlsym(p->libnuma_handle, "numa_node_of_cpu");
        if (p->our_max_node && p->our_node_of_cpu)
            p->nodes.total = p->our_max_node() + 1;
        else {
            dlclose(p->libnuma_handle);
            p->libnuma_handle = NULL;
        }
    }
 #else
    p->libnuma_handle = (void *)-1;
    p->our_max_node = fake_max_node;
    p->our_node_of_cpu = fake_node_of_cpu;
    p->nodes.total = fake_max_node() + 1;
 #endif
#endif

    *info = p;
    return 0;
} // end :procps_stat_new


PROCPS_EXPORT int procps_stat_ref (
        struct stat_info *info)
{
    if (info == NULL)
        return -EINVAL;

    info->refcount++;
    return info->refcount;
} // end: procps_stat_ref


PROCPS_EXPORT int procps_stat_unref (
        struct stat_info **info)
{
    if (info == NULL || *info == NULL)
        return -EINVAL;
    (*info)->refcount--;

    if ((*info)->refcount == 0) {
        if ((*info)->cpus.anchor)
            free((*info)->cpus.anchor);
        if ((*info)->cpus.result.stacks)
            free((*info)->cpus.result.stacks);
        if ((*info)->cpus.hist.tics)
            free((*info)->cpus.hist.tics);
        if ((*info)->cpus.fetch.extents)
            stat_extents_free_all(&(*info)->cpus.fetch);

        if ((*info)->nodes.anchor)
            free((*info)->nodes.anchor);
        if ((*info)->nodes.result.stacks)
            free((*info)->nodes.result.stacks);
        if ((*info)->nodes.hist.tics)
            free((*info)->nodes.hist.tics);
        if ((*info)->nodes.fetch.extents)
            stat_extents_free_all(&(*info)->nodes.fetch);

        if ((*info)->cpu_summary.extents)
            stat_extents_free_all(&(*info)->cpu_summary);

        if ((*info)->select.extents)
            stat_extents_free_all(&(*info)->select);

        if ((*info)->reap_items.enums)
            free((*info)->reap_items.enums);
        if ((*info)->select_items.enums)
            free((*info)->select_items.enums);

#ifndef NUMA_DISABLE
 #ifndef PRETEND_NUMA
        if ((*info)->libnuma_handle)
            dlclose((*info)->libnuma_handle);
 #endif
#endif
        free(*info);
        *info = NULL;
        return 0;
    }
    return (*info)->refcount;
} // end: procps_stat_unref


// --- variable interface functions -------------------------------------------

PROCPS_EXPORT struct stat_result *procps_stat_get (
        struct stat_info *info,
        enum stat_item item)
{
    static time_t sav_secs;
    time_t cur_secs;

    if (info == NULL)
        return NULL;
    if (item < 0 || item >= STAT_logical_end)
        return NULL;

    /* we will NOT read the source file with every call - rather, we'll offer
       a granularity of 1 second between reads ... */
    cur_secs = time(NULL);
    if (1 <= cur_secs - sav_secs) {
        if (stat_read_failed(info))
            return NULL;
        sav_secs = cur_secs;
    }

    info->get_this.item = item;
//  with 'get', we must NOT honor the usual 'noop' guarantee
//  if (item > STAT_noop)
        info->get_this.result.ull_int = 0;
    Item_table[item].setsfunc(&info->get_this, &info->sys_hist, &info->cpu_hist);

    return &info->get_this;
} // end: procps_stat_get


/* procps_stat_reap():
 *
 * Harvest all the requested NUMA NODE and/or CPU information providing the
 * result stacks along with totals and the cpu summary.
 *
 * Returns: pointer to a stat_reaped struct on success, NULL on error.
 */
PROCPS_EXPORT struct stat_reaped *procps_stat_reap (
        struct stat_info *info,
        enum stat_reap_type what,
        enum stat_item *items,
        int numitems)
{
    int rc;

    if (info == NULL || items == NULL)
        return NULL;
    if (what != STAT_REAP_CPUS_ONLY && what != STAT_REAP_CPUS_AND_NODES)
        return NULL;

#ifdef ENFORCE_LOGICAL
{   int i;
    // those STAT_SYS_type enum's make sense only to 'select' ...
    for (i = 0; i < numitems; i++) {
        if (items[i] > STAT_TIC_highest)
            return NULL;
    }
}
#endif

    if (0 > (rc = stat_stacks_reconfig_maybe(&info->cpu_summary, items, numitems)))
        return NULL;
    if (rc) {
        stat_extents_free_all(&info->cpus.fetch);
        stat_extents_free_all(&info->nodes.fetch);
    }

    if (stat_read_failed(info))
        return NULL;
    info->results.summary = stat_update_single_stack(info, &info->cpu_summary);

    /* unlike the other 'reap' functions, <stat> provides for two separate |
       stacks pointer arrays exposed to callers. Thus, to keep our promise |
       of NULL delimit we must ensure a minimal array for the optional one | */
    if (!info->nodes.result.stacks
    && (!(info->nodes.result.stacks = malloc(sizeof(void *)))))
        return NULL;
    info->nodes.result.total = 0;
    info->nodes.result.stacks[0] = NULL;

    switch (what) {
        case STAT_REAP_CPUS_ONLY:
            if (!stat_stacks_fetch(info, &info->cpus))
                return NULL;
            break;
        case STAT_REAP_CPUS_AND_NODES:
#ifndef NUMA_DISABLE
            /* note: if we are doing numa at all, we must call stat_make_numa_hist |
               before we build (fetch) the cpu stacks since the read_stat guy |
               will have marked (temporarily) all the cpu node ids as invalid | */
            if (0 > stat_make_numa_hist(info))
                return NULL;
            // tolerate an unexpected absence of libnuma.so ...
            stat_stacks_fetch(info, &info->nodes);
#endif
            if (!stat_stacks_fetch(info, &info->cpus))
                return NULL;
            break;
        default:
            return NULL;
    };

    return &info->results;
} // end: procps_stat_reap


/* procps_stat_select():
 *
 * Harvest all the requested TIC and/or SYS information then return
 * it in a results stack.
 *
 * Returns: pointer to a stat_stack struct on success, NULL on error.
 */
PROCPS_EXPORT struct stat_stack *procps_stat_select (
        struct stat_info *info,
        enum stat_item *items,
        int numitems)
{
    if (info == NULL || items == NULL)
        return NULL;

    if (0 > stat_stacks_reconfig_maybe(&info->select, items, numitems))
        return NULL;

    if (stat_read_failed(info))
        return NULL;

    return stat_update_single_stack(info, &info->select);
} // end: procps_stat_select


/*
 * procps_stat_sort():
 *
 * Sort stacks anchored in the passed stack pointers array
 * based on the designated sort enumerator and specified order.
 *
 * Returns those same addresses sorted.
 *
 * Note: all of the stacks must be homogeneous (of equal length and content).
 */
PROCPS_EXPORT struct stat_stack **procps_stat_sort (
        struct stat_info *info,
        struct stat_stack *stacks[],
        int numstacked,
        enum stat_item sortitem,
        enum stat_sort_order order)
{
    struct stat_result *p;
    struct sort_parms parms;
    int offset;

    if (info == NULL || stacks == NULL)
        return NULL;

    // a stat_item is currently unsigned, but we'll protect our future
    if (sortitem < 0 || sortitem >= STAT_logical_end)
        return NULL;
    if (order != STAT_SORT_ASCEND && order != STAT_SORT_DESCEND)
        return NULL;
    if (numstacked < 2)
        return stacks;

    offset = 0;
    p = stacks[0]->head;
    for (;;) {
        if (p->item == sortitem)
            break;
        ++offset;
        if (p->item >= STAT_logical_end)
            return NULL;
        ++p;
    }
    parms.offset = offset;
    parms.order = order;

    qsort_r(stacks, numstacked, sizeof(void *), (QSR_t)Item_table[p->item].sortfunc, &parms);
    return stacks;
} // end: procps_stat_sort


// --- special debugging function(s) ------------------------------------------
/*
 *  The following isn't part of the normal programming interface.  Rather,
 *  it exists to validate result types referenced in application programs.
 *
 *  It's used only when:
 *      1) the 'XTRA_PROCPS_DEBUG' has been defined, or
 *      2) the '#include <proc/xtra-procps-debug.h>' used
 */

PROCPS_EXPORT struct stat_result *xtra_stat_get (
        struct stat_info *info,
        enum stat_item actual_enum,
        const char *typestr,
        const char *file,
        int lineno)
{
    struct stat_result *r = procps_stat_get(info, actual_enum);

    if (actual_enum < 0 || actual_enum >= STAT_logical_end) {
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
} // end: xtra_stat_get_


PROCPS_EXPORT struct stat_result *xtra_stat_val (
        int relative_enum,
        const char *typestr,
        const struct stat_stack *stack,
        struct stat_info *info,
        const char *file,
        int lineno)
{
    char *str;
    int i;

    for (i = 0; stack->head[i].item < STAT_logical_end; i++)
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
} // end: xtra_stat_val
