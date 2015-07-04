#ifndef _PROC_SLAB_H
#define _PROC_SLAB_H

__BEGIN_DECLS

enum procps_slabinfo_stat {
    PROCPS_SLABINFO_OBJS,
    PROCPS_SLABINFO_AOBJS,
    PROCPS_SLABINFO_PAGES,
    PROCPS_SLABINFO_SLABS,
    PROCPS_SLABINFO_ASLABS,
    PROCPS_SLABINFO_CACHES,
    PROCPS_SLABINFO_ACACHES,
    PROCPS_SLABINFO_SIZE_AVG,
    PROCPS_SLABINFO_SIZE_MIN,
    PROCPS_SLABINFO_SIZE_MAX,
    PROCPS_SLABINFO_SIZE_TOTAL,
    PROCPS_SLABINFO_SIZE_ACTIVE,
};

enum procps_slabinfo_nodeitem {
    PROCPS_SLABNODE_NAME,
    PROCPS_SLABNODE_SIZE,
    PROCPS_SLABNODE_OBJS,
    PROCPS_SLABNODE_AOBJS,
    PROCPS_SLABNODE_OBJ_SIZE,
    PROCPS_SLABNODE_OBJS_PER_SLAB,
    PROCPS_SLABNODE_PAGES_PER_SLAB,
    PROCPS_SLABNODE_SLABS,
    PROCPS_SLABNODE_ASLABS,
    PROCPS_SLABNODE_USE
};

struct procps_slabinfo;
struct procps_slabnode;

struct procps_slabinfo_result {
    enum procps_slabinfo_stat item;
    unsigned long result;
    struct procps_slabinfo_result *next;
};

struct procps_slabnode_result {
    enum procps_slabinfo_nodeitem item;
    unsigned long result;
    struct procps_slabnode_result *next;
};

int procps_slabinfo_new (struct procps_slabinfo **info);
int procps_slabinfo_read (struct procps_slabinfo *info);

int procps_slabinfo_ref (struct procps_slabinfo *info);
int procps_slabinfo_unref (struct procps_slabinfo **info);

unsigned long procps_slabinfo_stat_get (struct procps_slabinfo *info,
                              enum procps_slabinfo_stat item);

int procps_slabinfo_stat_getchain (struct procps_slabinfo *info,
                                   struct procps_slabinfo_result *result);

int procps_slabinfo_sort( struct procps_slabinfo *info,
                          const enum procps_slabinfo_nodeitem item);

int procps_slabinfo_node_count(const struct procps_slabinfo *info);

int procps_slabinfo_node_get (struct procps_slabinfo *info,
                              struct procps_slabnode **node);
int procps_slabinfo_node_getchain (struct procps_slabinfo *info,
                                   struct procps_slabnode_result *result,
                                   int nodeid);

char *procps_slabinfo_node_getname(struct procps_slabinfo *info,
                                   int nodeid);
__END_DECLS


#endif /* _PROC_SLAB_H */
