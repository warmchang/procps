#ifndef PROCPS_PROC_PROCPS_H
#define PROCPS_PROC_PROCPS_H

#ifdef  __cplusplus
#define EXTERN_C_BEGIN extern "C" {
#define EXTERN_C_END }
#else
#define EXTERN_C_BEGIN
#define EXTERN_C_END
#endif

// Some ports make the mistake of running a 32-bit userspace
// on a 64-bit kernel. Shame on them. It's not at all OK to
// make everything "long long", since that causes unneeded
// slowness on 32-bit hardware.
//
// SPARC: 32-bit kernel is an ex-penguin, so use "long long".
//
// MIPS: Used for embedded systems and obsolete hardware.
// Oh, there's a 64-bit version? SGI is headed toward IA-64,
// so don't worry about 64-bit MIPS.
//
// PowerPC: Big ugly problem! Macs are popular. :-/
//
// Unknown: PA-RISC, zSeries, and x86-64
//
#if defined(k64test) || defined(__sparc__)  // || defined(__mips__) || defined(__powerpc__)
#define KLONG long long    // not typedef; want "unsigned KLONG" to work
#define KLF "L"
#define STRTOUKL strtoull
#else
#define KLONG long
#define KLF "l"
#define STRTOUKL strtoul
#endif

// since gcc-2.5
#define NORETURN __attribute__((__noreturn__))
#define FUNCTION __attribute__((__const__))  // no access to global mem, even via ptr, and no side effect

#if !defined(restrict) && __STDC_VERSION__ < 199901
#if __GNUC__ > 2 || __GNUC_MINOR__ >= 92
#define restrict __restrict__
#else
#warning No restrict keyword?
#define restrict
#endif
#endif

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

#if SHARED==1 && (__GNUC__ > 2 || __GNUC_MINOR__ >= 96)
#define LABEL_OFFSET
#endif

#define STRINGIFY_ARG(a)	#a
#define STRINGIFY(a)		STRINGIFY_ARG(a)

// marks old junk, to warn non-procps library users
#if ( __GNUC__ == 3 && __GNUC_MINOR__ > 0 ) || __GNUC__ > 3
#define OBSOLETE __attribute__((deprecated))
#else
#define OBSOLETE
#endif

#if ( __GNUC__ == 3 && __GNUC_MINOR__ > 1 ) || __GNUC__ > 3
// Tells gcc that function is library-internal;
// so no need to do dynamic linking at run-time.
// This might work with slightly older compilers too.
#define HIDDEN __attribute__((visibility("hidden")))
// Tell g++ that a function won't throw exceptions.
#define NOTHROW __attribute__((__nothrow__))
#else
#define HIDDEN
#define NOTHROW
#endif

// Like HIDDEN, but for an alias that gets created.
// In gcc-3.2 there is an alias+hidden conflict.
// Many will have patched this bug, but oh well.
#if ( __GNUC__ == 3 && __GNUC_MINOR__ > 2 ) || __GNUC__ > 3
#define HIDDEN_ALIAS(x) extern __typeof(x) x##_direct __attribute__((alias(#x),visibility("hidden")))
#else
#define HIDDEN_ALIAS(x) extern __typeof(x) x##_direct __attribute__((alias(#x)))
#endif

#endif
