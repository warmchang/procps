#ifndef PROCPS_NG_NSUTILS
#define PROCPS_NG_NSUTILS

#include "proc/readproc.h"
int ns_read(pid_t pid, proc_t *ns_task);

#endif
