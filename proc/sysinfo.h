#ifndef PROC_SYSINFO_H
#define PROC_SYSINFO_H
#include <sys/types.h>
#include <dirent.h>
#include <proc/procps.h>

__BEGIN_DECLS

extern int have_privs;             /* boolean, true if setuid or similar */

long procps_cpu_count(void);
long procps_hertz_get(void);
int procps_loadavg(double *av1, double *av5, double *av15);
long procps_pagesize_get(void);

#define BUFFSIZE (64*1024)
typedef unsigned long long jiff;

typedef struct disk_stat{
	unsigned long long reads_sectors;
	unsigned long long written_sectors;
	char               disk_name [16];
	unsigned           inprogress_IO;
	unsigned           merged_reads;
	unsigned           merged_writes;
	unsigned           milli_reading;
	unsigned           milli_spent_IO;
	unsigned           milli_writing;
	unsigned           partitions;
	unsigned           reads;
	unsigned           weighted_milli_spent_IO;
	unsigned           writes;
}disk_stat;

typedef struct partition_stat{
	char partition_name [16];
	unsigned long long reads_sectors;
	unsigned           parent_disk;  // index into a struct disk_stat array
	unsigned           reads;
	unsigned           writes;
	unsigned long long requested_writes;
}partition_stat;

extern unsigned int getpartitions_num(struct disk_stat *disks, int ndisks);
extern unsigned int getdiskstat (struct disk_stat**,struct partition_stat**);

typedef struct slab_cache{
	char name[48];
	unsigned active_objs;
	unsigned num_objs;
	unsigned objsize;
	unsigned objperslab;
}slab_cache;

extern unsigned int getslabinfo (struct slab_cache**);

extern unsigned get_pid_digits(void) FUNCTION;

__END_DECLS
#endif /* SYSINFO_H */
