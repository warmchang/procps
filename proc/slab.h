#ifndef _PROC_SLAB_H
#define _PROC_SLAB_H

#define SLAB_INFO_NAME_LEN	64

struct slab_info {
	char name[SLAB_INFO_NAME_LEN];	/* name of this cache */
	struct slab_info *next;
	int nr_objs;			/* number of objects in this cache */
	int nr_active_objs;		/* number of active objects */
	int obj_size;			/* size of each object */
	int objs_per_slab;		/* number of objects per slab */
	int pages_per_slab;		/* number of pages per slab */
	int nr_slabs;			/* number of slabs in this cache */
	int nr_active_slabs;		/* number of active slabs */
	int use;			/* percent full: total / active */
	int cache_size;			/* size of entire cache */
};

struct slab_stat {
	int nr_objs;		/* number of objects, among all caches */
	int nr_active_objs;	/* number of active objects, among all caches */
	int total_size;		/* size of all objects */
	int active_size;	/* size of all active objects */
	int nr_pages;		/* number of pages consumed by all objects */
	int nr_slabs;		/* number of slabs, among all caches */
	int nr_active_slabs;	/* number of active slabs, among all caches */
	int nr_caches;		/* number of caches */
	int nr_active_caches;	/* number of active caches */
	int avg_obj_size;	/* average object size */
	int min_obj_size;	/* size of smallest object */
	int max_obj_size;	/* size of largest object */
};

extern void put_slabinfo(struct slab_info *);
extern void free_slabinfo(struct slab_info *);
extern int get_slabinfo(struct slab_info **, struct slab_stat *);

#endif /* _PROC_SLAB_H */
