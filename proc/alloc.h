#ifndef PROCPS_PROC_ALLOC_H
#define PROCPS_PROC_ALLOC_H

#include <features.h>

__BEGIN_DECLS

#define MALLOC __attribute__ ((__malloc__))

extern void *xcalloc(unsigned int size) MALLOC;
extern void *xmalloc(size_t size) MALLOC;
extern void *xrealloc(void *oldp, unsigned int size) MALLOC;
extern char *xstrdup(const char *str) MALLOC;

__END_DECLS

#endif
