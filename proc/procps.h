#ifndef PROCPS_PROC_PROCPS_H
#define PROCPS_PROC_PROCPS_H

/* The shadow of the original with only common prototypes now. */
#include <stdio.h>
#include <sys/types.h>

/* The HZ constant from <asm/param.h> is replaced by the Hertz variable
 * available from "proc/sysinfo.h".
 */

/* get page info */
#include <asm/page.h>

#if !defined(restrict) && __STDC_VERSION__ < 199901
#if __GNUC__ > 2 || __GNUC_MINOR__ >= 91    // maybe 92 or 95 ?
#define restrict __restrict__
#else
#warning No restrict keyword?
#define restrict
#endif
#endif

#if __GNUC__ > 2 || __GNUC_MINOR__ >= 96
// won't alias anything, and aligned enough for anything
#define MALLOC __attribute__ ((__malloc__))
// tell gcc what to expect:   if(unlikely(err)) die(err);
#define likely(x)       __builtin_expect(!!(x),1)
#define unlikely(x)     __builtin_expect(!!(x),0)
#else
#define MALLOC
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif


extern void *xrealloc(void *oldp, unsigned int size) MALLOC;
extern void *xmalloc(unsigned int size) MALLOC;
extern void *xcalloc(void *pointer, int size) MALLOC;
       
extern int   mult_lvl_cmp(void* a, void* b);
       
extern char *user_from_uid(uid_t uid);
extern char *group_from_gid(gid_t gid);

extern const char * wchan(unsigned long address);
extern int   open_psdb(const char *restrict override);
extern int   open_psdb_message(const char *restrict override, void (*message)(const char *, ...));

extern unsigned print_str    (FILE *restrict file, const char *restrict s, unsigned max);
extern unsigned print_strlist(FILE *restrict file, const char *restrict const *restrict strs, unsigned max);

#endif
