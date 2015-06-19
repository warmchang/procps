#ifndef PROCPS_PROC_WCHAN_H
#define PROCPS_PROC_WCHAN_H

#include <proc/procps.h>

__BEGIN_DECLS

extern const char * lookup_wchan (int pid);

__END_DECLS

#endif
