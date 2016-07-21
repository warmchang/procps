/*
 * diskstat - Disk statistics - part of procps
 *
 * Copyright (C) 2003 Fabian Frederick
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
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "procps-private.h"
#include <proc/diskstats.h>

/* The following define will cause the 'node_add' function to maintain our |
   nodes list in ascending alphabetical order which could be used to avoid |
   a sort on name. Without it, we default to a 'pull-up' stack at slightly |
   more effort than a simple 'push-down' list to duplicate prior behavior. | */
//#define ALPHABETIC_NODES

#define DISKSTATS_LINE_LEN  1024
#define DISKSTATS_NAME_LEN  34
#define DISKSTATS_FILE      "/proc/diskstats"
#define SYSBLOCK_DIR        "/sys/block"

#define STACKS_INCR         64
#define STR_COMPARE         strverscmp

struct dev_data {
    unsigned long reads;
    unsigned long reads_merged;
    unsigned long read_sectors;
    unsigned long read_time;
    unsigned long writes;
    unsigned long writes_merged;
    unsigned long write_sectors;
    unsigned long write_time;
    unsigned long io_inprogress;
    unsigned long io_time;
    unsigned long io_wtime;
};

struct dev_node {
    char name[DISKSTATS_NAME_LEN+1];
    int type;
    int major;
    int minor;
    time_t stamped;
    struct dev_data new;
    struct dev_data old;
    struct dev_node *next;
};

struct stacks_extent {
    int ext_numstacks;
    struct stacks_extent *next;
    struct diskstats_stack **stacks;
};

struct ext_support {
    int numitems;                      // includes 'logical_end' delimiter
    enum diskstats_item *items;        // includes 'logical_end' delimiter
    struct stacks_extent *extents;     // anchor for these extents
    int dirty_stacks;
};

struct fetch_support {
    struct diskstats_stack **anchor;   // fetch consolidated extents
    int n_alloc;                       // number of above pointers allocated
    int n_inuse;                       // number of above pointers occupied
    int n_alloc_save;                  // last known reap.stacks allocation
    struct diskstats_reap results;     // count + stacks for return to caller
};

struct diskstats_info {
    int refcount;
    FILE *diskstats_fp;
    time_t old_stamp;                  /* previous read seconds */
    time_t new_stamp;                  /* current read seconds */
    struct dev_node *nodes;            /* dev nodes anchor */
    struct ext_support select_ext;     /* supports concurrent select/reap */
    struct ext_support fetch_ext;      /* supports concurrent select/reap */
    struct fetch_support fetch;        /* support for procps_diskstats_reap */
    struct diskstats_result get_this;  /* used by procps_diskstats_get */
};


// ___ Results 'Set' Support ||||||||||||||||||||||||||||||||||||||||||||||||||

#define setNAME(e) set_results_ ## e
#define setDECL(e) static void setNAME(e) \
    (struct diskstats_result *R, struct dev_node *N)

// regular assignment
#define DEV_set(e,t,x) setDECL(e) { R->result. t = N-> x; }
#define REG_set(e,t,x) setDECL(e) { R->result. t = N->new . x; }
// delta assignment
#define HST_set(e,t,x) setDECL(e) { R->result. t = ( N->new . x - N->old. x ); }

setDECL(noop)  { (void)R; (void)N; }
setDECL(extra) { (void)R; (void)N; }

DEV_set(DISKSTATS_NAME,                 str,     name)
DEV_set(DISKSTATS_TYPE,                 s_int,   type)
DEV_set(DISKSTATS_MAJOR,                s_int,   major)
DEV_set(DISKSTATS_MINOR,                s_int,   minor)

REG_set(DISKSTATS_READS,                ul_int,  reads)
REG_set(DISKSTATS_READS_MERGED,         ul_int,  reads_merged)
REG_set(DISKSTATS_READ_SECTORS,         ul_int,  read_sectors)
REG_set(DISKSTATS_READ_TIME,            ul_int,  read_time)
REG_set(DISKSTATS_WRITES,               ul_int,  writes)
REG_set(DISKSTATS_WRITES_MERGED,        ul_int,  writes_merged)
REG_set(DISKSTATS_WRITE_SECTORS,        ul_int,  write_sectors)
REG_set(DISKSTATS_WRITE_TIME,           ul_int,  write_time)
REG_set(DISKSTATS_IO_TIME,              ul_int,  io_time)
REG_set(DISKSTATS_IO_WTIME,             ul_int,  io_wtime)

REG_set(DISKSTATS_IO_INPROGRESS,        s_int,   io_inprogress)

HST_set(DISKSTATS_DELTA_READS,          s_int,   reads)
HST_set(DISKSTATS_DELTA_READS_MERGED,   s_int,   reads_merged)
HST_set(DISKSTATS_DELTA_READ_SECTORS,   s_int,   read_sectors)
HST_set(DISKSTATS_DELTA_READ_TIME,      s_int,   read_time)
HST_set(DISKSTATS_DELTA_WRITES,         s_int,   writes)
HST_set(DISKSTATS_DELTA_WRITES_MERGED,  s_int,   writes_merged)
HST_set(DISKSTATS_DELTA_WRITE_SECTORS,  s_int,   write_sectors)
HST_set(DISKSTATS_DELTA_WRITE_TIME,     s_int,   write_time)
HST_set(DISKSTATS_DELTA_IO_TIME,        s_int,   io_time)
HST_set(DISKSTATS_DELTA_IO_WTIME,       s_int,   io_wtime)

#undef setDECL
#undef DEV_set
#undef REG_set
#undef HST_set


// ___ Sorting Support ||||||||||||||||||||||||||||||||||||||||||||||||||||||||

struct sort_parms {
    int offset;
    enum diskstats_sort_order order;
};

#define srtNAME(t) sort_results_ ## t
#define srtDECL(t) static int srtNAME(t) \
    (const struct diskstats_stack **A, const struct diskstats_stack **B, struct sort_parms *P)

srtDECL(s_int) {
    const struct diskstats_result *a = (*A)->head + P->offset; \
    const struct diskstats_result *b = (*B)->head + P->offset; \
    return P->order * (a->result.s_int - b->result.s_int);
}

srtDECL(ul_int) {
    const struct diskstats_result *a = (*A)->head + P->offset; \
    const struct diskstats_result *b = (*B)->head + P->offset; \
    if ( a->result.ul_int > b->result.ul_int ) return P->order > 0 ?  1 : -1; \
    if ( a->result.ul_int < b->result.ul_int ) return P->order > 0 ? -1 :  1; \
    return 0;
}

srtDECL(str) {
    const struct diskstats_result *a = (*A)->head + P->offset;
    const struct diskstats_result *b = (*B)->head + P->offset;
    return P->order * STR_COMPARE(a->result.str, b->result.str);
}

srtDECL(noop) { \
    (void)A; (void)B; (void)P; \
    return 0;
}

#undef srtDECL


// ___ Controlling Table ||||||||||||||||||||||||||||||||||||||||||||||||||||||

typedef void (*SET_t)(struct diskstats_result *, struct dev_node *);
#define RS(e) (SET_t)setNAME(e)

typedef int  (*QSR_t)(const void *, const void *, void *);
#define QS(t) (QSR_t)srtNAME(t)

        /*
         * Need it be said?
         * This table must be kept in the exact same order as
         * those *enum diskstats_item* guys ! */
static struct {
    SET_t setsfunc;              // the actual result setting routine
    QSR_t sortfunc;              // sort cmp func for a specific type
} Item_table[] = {
/*  setsfunc                            sortfunc
    ----------------------------------  ---------  */
  { RS(noop),                           QS(ul_int) },
  { RS(extra),                          QS(noop)   },

  { RS(DISKSTATS_NAME),                 QS(str)    },
  { RS(DISKSTATS_TYPE),                 QS(s_int)  },
  { RS(DISKSTATS_MAJOR),                QS(s_int)  },
  { RS(DISKSTATS_MINOR),                QS(s_int)  },

  { RS(DISKSTATS_READS),                QS(ul_int) },
  { RS(DISKSTATS_READS_MERGED),         QS(ul_int) },
  { RS(DISKSTATS_READ_SECTORS),         QS(ul_int) },
  { RS(DISKSTATS_READ_TIME),            QS(ul_int) },
  { RS(DISKSTATS_WRITES),               QS(ul_int) },
  { RS(DISKSTATS_WRITES_MERGED),        QS(ul_int) },
  { RS(DISKSTATS_WRITE_SECTORS),        QS(ul_int) },
  { RS(DISKSTATS_WRITE_TIME),           QS(ul_int) },
  { RS(DISKSTATS_IO_TIME),              QS(ul_int) },
  { RS(DISKSTATS_IO_WTIME),             QS(ul_int) },

  { RS(DISKSTATS_IO_INPROGRESS),        QS(s_int)  },

  { RS(DISKSTATS_DELTA_READS),          QS(s_int)  },
  { RS(DISKSTATS_DELTA_READS_MERGED),   QS(s_int)  },
  { RS(DISKSTATS_DELTA_READ_SECTORS),   QS(s_int)  },
  { RS(DISKSTATS_DELTA_READ_TIME),      QS(s_int)  },
  { RS(DISKSTATS_DELTA_WRITES),         QS(s_int)  },
  { RS(DISKSTATS_DELTA_WRITES_MERGED),  QS(s_int)  },
  { RS(DISKSTATS_DELTA_WRITE_SECTORS),  QS(s_int)  },
  { RS(DISKSTATS_DELTA_WRITE_TIME),     QS(s_int)  },
  { RS(DISKSTATS_DELTA_IO_TIME),        QS(s_int)  },
  { RS(DISKSTATS_DELTA_IO_WTIME),       QS(s_int)  },

 // dummy entry corresponding to DISKSTATS_logical_end ...
  { NULL,                               NULL       }
};

    /* please note,
     * this enum MUST be 1 greater than the highest value of any enum */
enum diskstats_item DISKSTATS_logical_end = DISKSTATS_DELTA_IO_WTIME + 1;

#undef setNAME
#undef srtNAME
#undef RS
#undef QS


// ___ Private Functions ||||||||||||||||||||||||||||||||||||||||||||||||||||||
// --- dev_node specific support ----------------------------------------------

static struct dev_node *node_add (
        struct diskstats_info *info,
        struct dev_node *this)
{
    struct dev_node *prev, *walk;

#ifdef ALPHABETIC_NODES
    if (!info->nodes
    || (STR_COMPARE(this->name, info->nodes->name) < 0)) {
        this->next = info->nodes;
        info->nodes = this;
        return this;
    }
    prev = info->nodes;
    walk = info->nodes->next;
    while (walk) {
        if (STR_COMPARE(this->name, walk->name) < 0)
            break;
        prev = walk;
        walk = walk->next;
    }
    prev->next = this;
    this->next = walk;
#else
    if (!info->nodes)
        info->nodes = this;
    else {
        walk = info->nodes;
        do {
            prev = walk;
            walk = walk->next;
        } while (walk);
        prev->next = this;
    }
#endif
    return this;
} // end: node_add

static void node_classify (
        struct dev_node *this)
{
    DIR *dirp;
    struct dirent *dent;

    /* all disks start off as partitions. this function
       checks /sys/block and changes a device found there
       into a disk. if /sys/block cannot have the directory
       read, all devices are then treated as disks. */
    this->type = DISKSTATS_TYPE_PARTITION;

    if (!(dirp = opendir(SYSBLOCK_DIR))) {
        this->type = DISKSTATS_TYPE_DISK;
        return;
    }
    while((dent = readdir(dirp))) {
        if (strcmp(this->name, dent->d_name) == 0) {
            this->type = DISKSTATS_TYPE_DISK;
            break;
        }
    }
    closedir(dirp);
} // end: node_classify


static struct dev_node *node_cut (
        struct diskstats_info *info,
        struct dev_node *this)
{
    struct dev_node *node = info->nodes;

    if (this) {
        if (this == node) {
            info->nodes = node->next;
            return this;
        }
        do {
            if (this == node->next) {
                node->next = node->next->next;
                return this;
            }
            node = node->next;
        } while (node);
    }
    return NULL;
} // end: node_cut


static struct dev_node *node_get (
        struct diskstats_info *info,
        const char *name)
{
    struct dev_node *node = info->nodes;

    while (node) {
        if (strcmp(name, node->name) == 0)
            break;
        node = node->next;
    }
    if (node) {
        /* if this disk or partition has somehow gotten stale, we'll lose
           it and then pretend it was never actually found ...
         [ we test against both stamps in case a 'read' was avoided ] */
        if (node->stamped != info->old_stamp
        && (node->stamped != info->new_stamp)) {
            free(node_cut(info, node));
            node = NULL;
        }
    }
    return node;
} // end: node_get


static int node_update (
        struct diskstats_info *info,
        struct dev_node *source)
{
    struct dev_node *target = node_get(info, source->name);

    if (!target) {
        if (!(target = malloc(sizeof(struct dev_node))))
            return -ENOMEM;
        memcpy(target, source, sizeof(struct dev_node));
        // let's not distort the deltas when a new node is created ...
        memcpy(&target->old, &target->new, sizeof(struct dev_data));
        node_classify(target);
        node_add(info, target);
        return 0;
    }
    // remember history from last time around ...
    memcpy(&source->old, &target->new, sizeof(struct dev_data));
    // preserve some stuff from the existing node struct ...
    source->type = target->type;
    source->next = target->next;
    // finally 'update' the existing node struct ...
    memcpy(target, source, sizeof(struct dev_node));
    return 0;
} // end: node_update


// ___ Private Functions ||||||||||||||||||||||||||||||||||||||||||||||||||||||
// --- generalized support ----------------------------------------------------

static inline void assign_results (
        struct diskstats_stack *stack,
        struct dev_node *node)
{
    struct diskstats_result *this = stack->head;

    for (;;) {
        enum diskstats_item item = this->item;
        if (item >= DISKSTATS_logical_end)
            break;
        Item_table[item].setsfunc(this, node);
        ++this;
    }
    return;
} // end: assign_results


static inline void cleanup_stack (
        struct diskstats_result *this)
{
    for (;;) {
        if (this->item >= DISKSTATS_logical_end)
            break;
        if (this->item > DISKSTATS_noop)
            this->result.ul_int = 0;
        ++this;
    }
} // end: cleanup_stack


static inline void cleanup_stacks_all (
        struct ext_support *this)
{
    struct stacks_extent *ext = this->extents;
    int i;

    while (ext) {
        for (i = 0; ext->stacks[i]; i++)
            cleanup_stack(ext->stacks[i]->head);
        ext = ext->next;
    };
    this->dirty_stacks = 0;
} // end: cleanup_stacks_all


static void extents_free_all (
        struct ext_support *this)
{
    while (this->extents) {
        struct stacks_extent *p = this->extents;
        this->extents = this->extents->next;
        free(p);
    };
} // end: extents_free_all


static inline struct diskstats_result *itemize_stack (
        struct diskstats_result *p,
        int depth,
        enum diskstats_item *items)
{
    struct diskstats_result *p_sav = p;
    int i;

    for (i = 0; i < depth; i++) {
        p->item = items[i];
        p->result.ul_int = 0;
        ++p;
    }
    return p_sav;
} // end: itemize_stack


static void itemize_stacks_all (
        struct ext_support *this)
{
    struct stacks_extent *ext = this->extents;

    while (ext) {
        int i;
        for (i = 0; ext->stacks[i]; i++)
            itemize_stack(ext->stacks[i]->head, this->numitems, this->items);
        ext = ext->next;
    };
    this->dirty_stacks = 0;
} // end: static void itemize_stacks_all


static inline int items_check_failed (
        struct ext_support *this,
        enum diskstats_item *items,
        int numitems)
{
    int i;

    /* if an enum is passed instead of an address of one or more enums, ol' gcc
     * will silently convert it to an address (possibly NULL).  only clang will
     * offer any sort of warning like the following:
     *
     * warning: incompatible integer to pointer conversion passing 'int' to parameter of type 'enum diskstats_item *'
     * my_stack = procps_diskstats_select(info, DISKSTATS_noop, num);
     *                                          ^~~~~~~~~~~~~~~~
     */
    if (numitems < 1
    || (void *)items < (void *)(unsigned long)(2 * DISKSTATS_logical_end))
        return -1;

    for (i = 0; i < numitems; i++) {
        // a diskstats_item is currently unsigned, but we'll protect our future
        if (items[i] < 0)
            return -1;
        if (items[i] >= DISKSTATS_logical_end)
            return -1;
    }

    return 0;
} // end: items_check_failed


/*
 * read_diskstats_failed:
 *
 * @info: info structure created at procps_diskstats_new
 *
 * Read the data out of /proc/diskstats putting the information
 * into the supplied info structure
 *
 * Returns: 0 on success, negative on error
 */
static int read_diskstats_failed (
        struct diskstats_info *info)
{
    static const char *fmtstr = "%d %d %" STRINGIFY(DISKSTATS_NAME_LEN) \
        "s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu";
    char buf[DISKSTATS_LINE_LEN];
    struct dev_node node;
    int rc;

    if (!info->diskstats_fp
    && (!(info->diskstats_fp = fopen(DISKSTATS_FILE, "r"))))
        return -errno;

    if (fseek(info->diskstats_fp, 0L, SEEK_SET) == -1)
        return -errno;

    info->old_stamp = info->new_stamp;
    info->new_stamp = time(NULL);

    while (fgets(buf, DISKSTATS_LINE_LEN, info->diskstats_fp)) {
        // clear out the soon to be 'current'values
        memset(&node, 0, sizeof(struct dev_node));

        rc = sscanf(buf, fmtstr
            , &node.major
            , &node.minor
            , &node.name[0]
            , &node.new.reads
            , &node.new.reads_merged
            , &node.new.read_sectors
            , &node.new.read_time
            , &node.new.writes
            , &node.new.writes_merged
            , &node.new.write_sectors
            , &node.new.write_time
            , &node.new.io_inprogress
            , &node.new.io_time
            , &node.new.io_wtime);

        if (rc != 14) {
            if (errno != 0)
                return -errno;
            return -EINVAL;
        }
        node.stamped = info->new_stamp;
        if ((rc = node_update(info, &node)))
            return rc;
    }

    return 0;
} //end: read_diskstats_failed


static struct stacks_extent *stacks_alloc (
        struct ext_support *this,
        int maxstacks)
{
    struct stacks_extent *p_blob;
    struct diskstats_stack **p_vect;
    struct diskstats_stack *p_head;
    size_t vect_size, head_size, list_size, blob_size;
    void *v_head, *v_list;
    int i;

    if (this == NULL || this->items == NULL)
        return NULL;
    if (maxstacks < 1)
        return NULL;

    vect_size  = sizeof(void *) * maxstacks;                      // size of the addr vectors |
    vect_size += sizeof(void *);                                  // plus NULL addr delimiter |
    head_size  = sizeof(struct diskstats_stack);                  // size of that head struct |
    list_size  = sizeof(struct diskstats_result) * this->numitems;// any single results stack |
    blob_size  = sizeof(struct stacks_extent);                    // the extent anchor itself |
    blob_size += vect_size;                                       // plus room for addr vects |
    blob_size += head_size * maxstacks;                           // plus room for head thing |
    blob_size += list_size * maxstacks;                           // plus room for our stacks |

    /* note: all of our memory is allocated in a single blob, facilitating some later free(). |
             as a minimum, it is important that all those result structs themselves always be |
             contiguous within every stack since they will be accessed via relative position. | */
    if (NULL == (p_blob = calloc(1, blob_size)))
        return NULL;

    p_blob->next = this->extents;                                 // push this extent onto... |
    this->extents = p_blob;                                       // ...some existing extents |
    p_vect = (void *)p_blob + sizeof(struct stacks_extent);       // prime our vector pointer |
    p_blob->stacks = p_vect;                                      // set actual vectors start |
    v_head = (void *)p_vect + vect_size;                          // prime head pointer start |
    v_list = v_head + (head_size * maxstacks);                    // prime our stacks pointer |

    for (i = 0; i < maxstacks; i++) {
        p_head = (struct diskstats_stack *)v_head;
        p_head->head = itemize_stack((struct diskstats_result *)v_list, this->numitems, this->items);
        p_blob->stacks[i] = p_head;
        v_list += list_size;
        v_head += head_size;
    }
    p_blob->ext_numstacks = maxstacks;
    return p_blob;
} // end: stacks_alloc


static int stacks_fetch (
        struct diskstats_info *info)
{
 #define n_alloc  info->fetch.n_alloc
 #define n_inuse  info->fetch.n_inuse
 #define n_saved  info->fetch.n_alloc_save
    struct stacks_extent *ext;
    struct dev_node *node;

    // initialize stuff -----------------------------------
    if (!info->fetch.anchor) {
        if (!(info->fetch.anchor = calloc(sizeof(void *), STACKS_INCR)))
            return -ENOMEM;
        n_alloc = STACKS_INCR;
    }
    if (!info->fetch_ext.extents) {
        if (!(ext = stacks_alloc(&info->fetch_ext, n_alloc)))
            return -ENOMEM;
        memset(info->fetch.anchor, 0, sizeof(void *) * n_alloc);
        memcpy(info->fetch.anchor, ext->stacks, sizeof(void *) * n_alloc);
        itemize_stacks_all(&info->fetch_ext);
    }
    cleanup_stacks_all(&info->fetch_ext);

    // iterate stuff --------------------------------------
    n_inuse = 0;
    node = info->nodes;
    while (node) {
        if (!(n_inuse < n_alloc)) {
            n_alloc += STACKS_INCR;
            if ((!(info->fetch.anchor = realloc(info->fetch.anchor, sizeof(void *) * n_alloc)))
            || (!(ext = stacks_alloc(&info->fetch_ext, STACKS_INCR))))
                return -1;
            memcpy(info->fetch.anchor + n_inuse, ext->stacks, sizeof(void *) * STACKS_INCR);
        }
        assign_results(info->fetch.anchor[n_inuse], node);
        ++n_inuse;
        node = node->next;
    }

    // finalize stuff -------------------------------------
    /* note: we go to this trouble of maintaining a duplicate of the consolidated |
             extent stacks addresses represented as our 'anchor' since these ptrs |
             are exposed to a user (um, not that we don't trust 'em or anything). |
             plus, we can NULL delimit these ptrs which we couldn't do otherwise. | */
    if (n_saved < n_inuse + 1) {
        n_saved = n_inuse + 1;
        if (!(info->fetch.results.stacks = realloc(info->fetch.results.stacks, sizeof(void *) * n_saved)))
            return -ENOMEM;
    }
    memcpy(info->fetch.results.stacks, info->fetch.anchor, sizeof(void *) * n_inuse);
    info->fetch.results.stacks[n_inuse] = NULL;
    info->fetch.results.total = n_inuse;

    return n_inuse;
 #undef n_alloc
 #undef n_inuse
 #undef n_saved
} // end: stacks_fetch


static int stacks_reconfig_maybe (
        struct ext_support *this,
        enum diskstats_item *items,
        int numitems)
{
    if (items_check_failed(this, items, numitems))
        return -EINVAL;
    /* is this the first time or have things changed since we were last called?
       if so, gotta' redo all of our stacks stuff ... */
    if (this->numitems != numitems + 1
    || memcmp(this->items, items, sizeof(enum diskstats_item) * numitems)) {
        // allow for our DISKSTATS_logical_end
        if (!(this->items = realloc(this->items, sizeof(enum diskstats_item) * (numitems + 1))))
            return -ENOMEM;
        memcpy(this->items, items, sizeof(enum diskstats_item) * numitems);
        this->items[numitems] = DISKSTATS_logical_end;
        this->numitems = numitems + 1;
        if (this->extents)
            extents_free_all(this);
        return 1;
    }
    return 0;
} // end: stacks_reconfig_maybe


// ___ Public Functions |||||||||||||||||||||||||||||||||||||||||||||||||||||||

// --- standard required functions --------------------------------------------

/*
 * procps_diskstats_new():
 *
 * @info: location of returned new structure
 *
 * Returns: < 0 on failure, 0 on success along with
 *          a pointer to a new context struct
 */
PROCPS_EXPORT int procps_diskstats_new (
        struct diskstats_info **info)
{
    struct diskstats_info *p;
    int rc;

    if (info == NULL)
        return -EINVAL;

    if (!(p = calloc(1, sizeof(struct diskstats_info))))
        return -ENOMEM;

    p->refcount = 1;

    /* do a priming read here for the following potential benefits: |
         1) ensure there will be no problems with subsequent access |
         2) make delta results potentially useful, even if 1st time | */
    if ((rc = read_diskstats_failed(p))) {
        procps_diskstats_unref(&p);
        return rc;
    }

    *info = p;
    return 0;
} // end: procps_diskstats_new


PROCPS_EXPORT int procps_diskstats_ref (
        struct diskstats_info *info)
{
    if (info == NULL)
        return -EINVAL;

    info->refcount++;
    return info->refcount;
} // end: procps_diskstats_ref


PROCPS_EXPORT int procps_diskstats_unref (
        struct diskstats_info **info)
{
    struct dev_node *node;

    if (info == NULL || *info == NULL)
        return -EINVAL;

    (*info)->refcount--;
    if ((*info)->refcount == 0) {
        if ((*info)->diskstats_fp) {
            fclose((*info)->diskstats_fp);
            (*info)->diskstats_fp = NULL;
        }
        node = (*info)->nodes;
        while (node) {
            struct dev_node *p = node;
            node = p->next;
            free(p);
        }
        if ((*info)->select_ext.extents)
            extents_free_all((&(*info)->select_ext));
        if ((*info)->select_ext.items)
            free((*info)->select_ext.items);

        if ((*info)->fetch.anchor)
            free((*info)->fetch.anchor);
        if ((*info)->fetch.results.stacks)
            free((*info)->fetch.results.stacks);

        if ((*info)->fetch_ext.extents)
            extents_free_all(&(*info)->fetch_ext);
        if ((*info)->fetch_ext.items)
            free((*info)->fetch_ext.items);

        free(*info);
        *info = NULL;

        return 0;
    }
    return (*info)->refcount;
} // end: procps_diskstats_unref


// --- variable interface functions -------------------------------------------

PROCPS_EXPORT struct diskstats_result *procps_diskstats_get (
        struct diskstats_info *info,
        const char *name,
        enum diskstats_item item)
{
    struct dev_node *node;
    time_t cur_secs;

    if (info == NULL)
        return NULL;
    if (item < 0 || item >= DISKSTATS_logical_end)
        return NULL;

    /* we will NOT read the diskstat file with every call - rather, we'll offer
       a granularity of 1 second between reads ... */
    cur_secs = time(NULL);
    if (1 <= cur_secs - info->new_stamp) {
        if (read_diskstats_failed(info))
            return NULL;
    }

    info->get_this.item = item;
//  with 'get', we must NOT honor the usual 'noop' guarantee
//  if (item > DISKSTATS_noop)
        info->get_this.result.ul_int = 0;

    if (!(node = node_get(info, name)))
        return NULL;
    Item_table[item].setsfunc(&info->get_this, node);

    return &info->get_this;
} // end: procps_diskstats_get


/* procps_diskstats_reap():
 *
 * Harvest all the requested disks information providing
 * the result stacks along with the total number of harvested.
 *
 * Returns: pointer to a diskstats_reap struct on success, NULL on error.
 */
PROCPS_EXPORT struct diskstats_reap *procps_diskstats_reap (
        struct diskstats_info *info,
        enum diskstats_item *items,
        int numitems)
{
    if (info == NULL || items == NULL)
        return NULL;

    if (0 > stacks_reconfig_maybe(&info->fetch_ext, items, numitems))
        return NULL;

    if (info->fetch_ext.dirty_stacks)
        cleanup_stacks_all(&info->fetch_ext);

    if (read_diskstats_failed(info))
        return NULL;
    stacks_fetch(info);
    info->fetch_ext.dirty_stacks = 1;

    return &info->fetch.results;
} // end: procps_diskstats_reap


/* procps_diskstats_select():
 *
 * Obtain all the requested disk/partition information then return
 * it in a single library provided results stack.
 *
 * Returns: pointer to a diskstats_stack struct on success, NULL on error.
 */
PROCPS_EXPORT struct diskstats_stack *procps_diskstats_select (
        struct diskstats_info *info,
        const char *name,
        enum diskstats_item *items,
        int numitems)
{
    struct dev_node *node;

    if (info == NULL || items == NULL)
        return NULL;

    if (0 > stacks_reconfig_maybe(&info->select_ext, items, numitems))
        return NULL;

    if (!info->select_ext.extents
    && !(stacks_alloc(&info->select_ext, 1)))
       return NULL;

    if (info->select_ext.dirty_stacks)
        cleanup_stacks_all(&info->select_ext);

    if (read_diskstats_failed(info))
        return NULL;
    if (!(node = node_get(info, name)))
        return NULL;

    assign_results(info->select_ext.extents->stacks[0], node);
    info->select_ext.dirty_stacks = 1;

    return info->select_ext.extents->stacks[0];
} // end: procps_diskstats_select


/*
 * procps_diskstats_sort():
 *
 * Sort stacks anchored in the passed stack pointers array
 * based on the designated sort enumerator and specified order.
 *
 * Returns those same addresses sorted.
 *
 * Note: all of the stacks must be homogeneous (of equal length and content).
 */
PROCPS_EXPORT struct diskstats_stack **procps_diskstats_sort (
        struct diskstats_info *info,
        struct diskstats_stack *stacks[],
        int numstacked,
        enum diskstats_item sortitem,
        enum diskstats_sort_order order)
{
    struct diskstats_result *p;
    struct sort_parms parms;
    int offset;

    if (info == NULL || stacks == NULL)
        return NULL;

    // a diskstats_item is currently unsigned, but we'll protect our future
    if (sortitem < 0 || sortitem >= DISKSTATS_logical_end)
        return NULL;
    if (order != DISKSTATS_SORT_ASCEND && order != DISKSTATS_SORT_DESCEND)
        return NULL;
    if (numstacked < 2)
        return stacks;

    offset = 0;
    p = stacks[0]->head;
    for (;;) {
        if (p->item == sortitem)
            break;
        ++offset;
        if (p->item == DISKSTATS_logical_end)
            return NULL;
        ++p;
    }
    parms.offset = offset;
    parms.order = order;

    qsort_r(stacks, numstacked, sizeof(void *), (QSR_t)Item_table[p->item].sortfunc, &parms);
    return stacks;
} // end: procps_diskstats_sort
