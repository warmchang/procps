#ifndef PROC_SYSINFO_H
#define PROC_SYSINFO_H
#include <sys/types.h>
#include <dirent.h>

#include <features.h>
__BEGIN_DECLS

long procps_cpu_count(void);
long procps_hertz_get(void);
int procps_loadavg(double *av1, double *av5, double *av15);
unsigned int procps_pid_length(void);

__END_DECLS
#endif /* SYSINFO_H */
