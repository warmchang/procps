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
// SPARC: The 32-bit kernel was looking like an ex-penguin,
// but it lives! ("I'm not dead yet.") So, 64-bit users will
// just have to compile for 64-bit. Aw, the suffering.
//
// MIPS: Used 32-bit for embedded systems and obsolete hardware.
// The 64-bit systems use an n32 format executable, defining
// _ABIN32 to indicate this. Since n32 doesn't currently run on
// any 32-bit system, nobody get hurt if it's bloated. Not that
// this is sane of course, but it won't hurt the 32-bit users.
// __mips_eabi means eabi, which comes in both sizes, but isn't used.
//
// PowerPC: Big ugly problem! 32-bit Macs are still popular. :-/
//
// x86-64: So far, nobody has been dumb enough to go 32-bit.
//
// Unknown: PA-RISC and zSeries
//
#if defined(k64test) || (defined(_ABIN32) && _MIPS_SIM == _ABIN32)
#define KLONG long long    // not typedef; want "unsigned KLONG" to work
#define KLF "ll"
#define STRTOUKL strtoull
#else
#define KLONG long
#define KLF "l"
#define STRTOUKL strtoul
#endif

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
#if (( __GNUC__ == 3 && __GNUC_MINOR__ > 2 ) || __GNUC__ > 3) && !defined(__CYGWIN__)
#define HIDDEN_ALIAS(x) extern __typeof(x) x##_direct __attribute__((alias(#x),visibility("hidden")))
#else
#define HIDDEN_ALIAS(x) extern __typeof(x) x##_direct __attribute__((alias(#x)))
#endif

#endif
