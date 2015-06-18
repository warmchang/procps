#ifndef PROCPS_PROC_WCHAN_H
#define PROCPS_PROC_WCHAN_H

#include "procps.h"

EXTERN_C_BEGIN

extern const char * lookup_wchan (int pid);

EXTERN_C_END

#endif
