#ifndef PROCPS_PROC_OUTPUT_H
#define PROCPS_PROC_OUTPUT_H

#include <stdio.h>
#include <sys/types.h>
#include "procps.h"

EXTERN_C_BEGIN

extern unsigned print_str    (FILE *restrict file, const char *restrict s, unsigned max);
extern unsigned print_strlist(FILE *restrict file, const char *restrict const *restrict strs, unsigned max);

EXTERN_C_END

#endif
