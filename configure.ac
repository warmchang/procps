#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.64])
AC_CONFIG_MACRO_DIR([m4])
AC_INIT([procps-ng],
	[3.3.0],
	[procps@freelists.org],,[http://gitorious.org/procps])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([top.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_GNU_SOURCE
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LN_S

# Checks for libraries.
AC_CHECK_LIB([ncurses], [setupterm])

# Checks for header files.
AC_HEADER_MAJOR
AC_CHECK_HEADERS([\
	fcntl.h \
	langinfo.h \
	limits.h \
	locale.h \
	netinet/in.h \
	stdlib.h \
	string.h \
	sys/file.h \
	sys/ioctl.h \
	sys/param.h \
	sys/time.h \
	termios.h \
	unistd.h \
	utmp.h \
	values.h \
	wchar.h \
	wctype.h
])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_CHECK_MEMBERS([struct stat.st_rdev])
AC_C_INLINE
AC_C_RESTRICT
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UID_T

dnl libtool
LT_INIT

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MBRTOWC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([\
	alarm \
	getpagesize \
	gettimeofday \
	iswprint \
	memchr \
	memmove \
	memset \
	munmap \
	nl_langinfo \
	putenv \
	regcomp \
	select \
	setlocale \
	strcasecmp \
	strchr \
	strcspn \
	strdup \
	strerror \
	strncasecmp \
	strpbrk \
	strrchr \
	strspn \
	strstr \
	strtol \
	strtoul \
	uname \
	utmpname \
	wcwidth
])

usrbin_execdir='${exec_prefix}/usr/bin'
AC_SUBST([usrbin_execdir])

dnl UTIL_CHECK_LIB(LIBRARY, FUNCTION, [VARSUFFIX = $1]))
dnl The VARSUFFIX is optional and overrides the default behaviour. For example:
dnl     UTIL_CHECK_LIB(yyy, func, xxx) generates have_xxx and HAVE_LIBXXX
dnl     UTIL_CHECK_LIB(yyy, func)      generates have_yyy and HAVE_LIBYYY
dnl ---------------------------------
AC_DEFUN([UTIL_CHECK_LIB], [
  m4_define([suffix], m4_default([$3],$1))
  [have_]suffix=yes
  m4_ifdef([$3],
    [AC_CHECK_LIB([$1], [$2], [AC_DEFINE(AS_TR_CPP([HAVE_LIB]suffix), 1)], [[have_]suffix=no])],
    [AC_CHECK_LIB([$1], [$2], [], [[have_]suffix=no])])
  AM_CONDITIONAL(AS_TR_CPP([HAVE_]suffix), [test [$have_]suffix = yes])
])

AC_ARG_WITH([ncurses],
  AS_HELP_STRING([--with-ncurses], [build with non-wide ncurses, default is wide version
                                    (--without-ncurses disables all ncurses(w) support)]),
  [], with_ncurses=auto
)
AM_CONDITIONAL(HAVE_NCURSES, false)

if test "x$with_ncurses" != xno; then
  have_ncurses=no
  AC_CHECK_HEADERS([ncurses.h ncurses/ncurses.h], [
    if test "x$with_ncurses" = xauto; then
      UTIL_CHECK_LIB(ncursesw, initscr, ncurses)
      if test "x$have_ncurses" = xyes; then
        AC_CHECK_HEADERS([ncursesw/ncurses.h])
        NCURSES_LIBS="-lncursesw"
      fi
    fi
    if test "x$have_ncurses" = xno; then
      UTIL_CHECK_LIB(ncurses, initscr)
      if test "x$have_ncurses" = xyes; then
        NCURSES_LIBS="-lncurses"
      fi
    fi
  ])
  if test "x$have_ncurses" = xno; then
    AC_MSG_ERROR([ncurses or ncursesw selected, but library not found (--without-ncurses to disable)])
  fi
fi
AC_SUBST([NCURSES_LIBS])

AC_CONFIG_FILES([
Makefile
proc/Makefile
ps/Makefile
])
AC_OUTPUT
