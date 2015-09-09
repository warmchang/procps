#ifndef PROCPS_PROC_PROCPS_H
#define PROCPS_PROC_PROCPS_H

#include <features.h>

#define KLONG long
#define KLF "l"
#define STRTOUKL strtoul

// since gcc-2.5
#define NORETURN __attribute__((__noreturn__))
#define FUNCTION __attribute__((__const__))  // no access to global mem, even via ptr, and no side effect

#if __GNUC__ > 2 || __GNUC_MINOR__ >= 96
// won't alias anything, and aligned enough for anything
#define MALLOC __attribute__ ((__malloc__))
// no side effect, may read globals
#define PURE __attribute__ ((__pure__))
// tell gcc what to expect:   if(unlikely(err)) die(err);
#define likely(x)       __builtin_expect(!!(x),1)
#define unlikely(x)     __builtin_expect(!!(x),0)
#define expected(x,y)   __builtin_expect((x),(y))
#else
#define MALLOC
#define PURE
#define likely(x)       (x)
#define unlikely(x)     (x)
#define expected(x,y)   (x)
#endif

#ifdef SHARED
# if SHARED==1 && (__GNUC__ > 2 || __GNUC_MINOR__ >= 96)
#  define LABEL_OFFSET
# endif
#endif

#define STRINGIFY_ARG(a)	#a
#define STRINGIFY(a)		STRINGIFY_ARG(a)

// marks old junk, to warn non-procps-ng library users
#if ( __GNUC__ == 3 && __GNUC_MINOR__ > 0 ) || __GNUC__ > 3
#define OBSOLETE __attribute__((deprecated))
#else
#define OBSOLETE
#endif

// Like HIDDEN, but for an alias that gets created.
// In gcc-3.2 there is an alias+hidden conflict.
// Many will have patched this bug, but oh well.
#if ( __GNUC__ == 3 && __GNUC_MINOR__ > 2 ) || __GNUC__ > 3
#define HIDDEN_ALIAS(x) extern __typeof(x) x##_direct __attribute__((alias(#x),visibility("hidden")))
#else
#define HIDDEN_ALIAS(x) extern __typeof(x) x##_direct __attribute__((alias(#x)))
#endif


typedef void (*message_fn)(const char *__restrict, ...) __attribute__((format(printf,1,2)));

#endif
