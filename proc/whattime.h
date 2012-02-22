#ifndef PROC_WHATTIME_H
#define PROC_WHATTIME_H

#include "procps.h"

EXTERN_C_BEGIN

extern void print_uptime(int human_readable);
extern char *sprint_uptime(int human_readable);

EXTERN_C_END

#endif
