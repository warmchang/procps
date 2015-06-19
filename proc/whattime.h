#ifndef PROC_WHATTIME_H
#define PROC_WHATTIME_H

#include <proc/procps.h>

__BEGIN_DECLS

extern void print_uptime(int human_readable);
extern char *sprint_uptime(int human_readable);

__END_DECLS

#endif
