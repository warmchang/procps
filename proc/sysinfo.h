#ifndef PROC_SYSINFO_H
#define PROC_SYSINFO_H
#include <sys/types.h>
#include <dirent.h>

#include <features.h>
__BEGIN_DECLS

extern int have_privs;             /* boolean, true if setuid or similar */

long procps_cpu_count(void);
long procps_hertz_get(void);
int procps_loadavg(double *av1, double *av5, double *av15);
unsigned int procps_pid_length(void);

#define BUFFSIZE (64*1024)

typedef struct slab_cache{
	char name[48];
	unsigned active_objs;
	unsigned num_objs;
	unsigned objsize;
	unsigned objperslab;
}slab_cache;

extern unsigned int getslabinfo (struct slab_cache**);


__END_DECLS
#endif /* SYSINFO_H */
