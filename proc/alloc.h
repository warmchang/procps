#ifndef PROCPS_PROC_ALLOC_H
#define PROCPS_PROC_ALLOC_H

#include "procps.h"

EXTERN_C_BEGIN

typedef void (*message_fn)(const char *__restrict, ...) __attribute__((format(printf,1,2)));

 /* change xalloc_err_handler to override the default fprintf(stderr... */
extern message_fn xalloc_err_handler;

extern void *xcalloc(size_t size) MALLOC;
extern void *xmalloc(size_t size) MALLOC;
extern void *xrealloc(void *oldp, size_t size) MALLOC;
extern char *xstrdup(const char *str) MALLOC;

EXTERN_C_END

#endif
