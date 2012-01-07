#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.64])
AC_CONFIG_MACRO_DIR([m4])
AC_INIT([procps-ng],
	m4_esyscmd([tools/git-version-gen .tarball-version]),
	[procps@freelists.org],,[http://gitorious.org/procps])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([free.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_GNU_SOURCE
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LN_S

AC_SUBST([WITH_WATCH8BIT])
AC_ARG_ENABLE([watch8bit],
              AS_HELP_STRING([--enable-watch8bit], [enable watch to be 8bit clean (requires ncursesw)]),
              [enable_watch8bit=$enableval],
              [enable_watch8bit="no"])
if test "$enable_watch8bit" = "yes"; then
  AC_DEFINE([WITH_WATCH8BIT], [1], [Enable 8 bit clean watch])
fi

# Checks for header files.
AC_HEADER_MAJOR
AC_CHECK_HEADERS([\
	fcntl.h \
	langinfo.h \
	libintl.h \
	limits.h \
	locale.h \
	netinet/in.h \
	stdint.h \
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
AC_HEADER_STDBOOL
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
	getline \
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
	rpmatch \
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
	strverscmp \
	uname \
	utmpname \
	wcwidth
])

usrbin_execdir='${exec_prefix}/usr/bin'
AC_SUBST([usrbin_execdir])

AM_GNU_GETTEXT_VERSION([0.14.1])
AM_GNU_GETTEXT([external])
dnl if test -d "$srcdir/po"
dnl then
dnl	ALL_LINGUAS=`cd $srcdir/po > /dev/null && echo *.po | sed 's/\.po//g'`
dnl else
dnl	ALL_LINGUAS="af am ar as be bg bn_IN bn ca cs cy da de el en_GB es et eu_ES fa fi fr gl gu he hi hr hu hy id is it ja ka kn ko ku lo lt lv mk ml mr ms my nb nl nn no nso or pa pl pt_BR pt ro ru si sk sl sq sr@Latn sr sv ta te th tr uk ur vi zh_CN zh_TW zu"
dnl fi

AC_MSG_CHECKING(whether program_invocation_short_name is defined)
AC_TRY_COMPILE([#include <argp.h>],
		[program_invocation_short_name = "test";],
		AC_DEFINE(HAVE_PROGRAM_INVOCATION_SHORT_NAME, 1,
			[Define if program_invocation_short_name is defined])
		AC_MSG_RESULT(yes),
		AC_MSG_RESULT(no))

AC_ARG_WITH([ncurses],
  AS_HELP_STRING([--without-ncurses], [build only applications not needing ncurses]),
  [with_ncurses=no], [with_ncurses=yes]
)

if test "x$with_ncurses" = xno; then
  AM_CONDITIONAL(WITH_NCURSES, false)
else
  AC_CHECK_LIB(ncursesw, initscr, [have_ncurses=yes], [have_ncurses=no])
  AC_CHECK_HEADERS(curses.h ncurses.h term.h, [], [have_ncurses=no], AC_INCLUDES_DEFAULT)
  if test "x$have_ncurses" = xno; then
    AC_MSG_ERROR([ncurses support missing/incomplete (for partial build use --without-ncurses)])
  fi
  AM_CONDITIONAL(WITH_NCURSES, true)
  if test "$enable_watch8bit" = yes; then
    AC_CHECK_LIB([ncursesw], [addwstr], [WATCH_NCURSES_LIBS=-lncursesw],
                [AC_MSG_ERROR([Cannot find ncurses wide library ncursesw with --enable-watch8bit])])
  else
    WATCH_NCURSES_LIBS="-lncurses"
  fi
  NCURSES_LIBS="-lncurses"
fi
AC_SUBST([NCURSES_LIBS])
AC_SUBST([WATCH_NCURSES_LIBS])

AC_ARG_ENABLE([kill],
  AS_HELP_STRING([--disable-kill], [do not build kill]),
  [], enable_kill=yes
)
AM_CONDITIONAL(BUILD_KILL, test "x$enable_kill" = xyes)
AM_CONDITIONAL(LINUX, test "x$host_os" = xlinux-gnu)

AC_ARG_ENABLE([oomem],
  AS_HELP_STRING([--enable-oomem], [add out-of-memory fields to the library and top]),
  [], enable_oomem=no
)
if test "x$enable_oomem" = xyes; then
  AC_DEFINE(OOMEM_ENABLE, 1, [add out-of-memory fields to the library and top])
fi

AC_ARG_ENABLE([w-from],
  AS_HELP_STRING([--enable-w-from], [enable w from field by default]),
  [], enable_w_from=no
)
if test "x$enable_w_from" = xyes; then
  AC_DEFINE(W_SHOWFROM, 1, [enable w from field by default])
fi

AC_ARG_ENABLE([whining],
  AS_HELP_STRING([--disable-whining], [do not print unnessary warnings (slackware-ism)]),
  [], enable_whining=yes
)
if test "x$enable_whining" = xyes; then
  AC_DEFINE(BUILD_WITH_WHINE, 1, [should extra warnings be printed (slackware-ism)])
fi

if test x"$DEJAGNU" = x
then
  DEJAGNU="\$(top_srcdir)/testsuite/global-conf.exp"
fi
AC_SUBST(DEJAGNU)

AC_CONFIG_FILES([
	Makefile
	include/Makefile
	lib/Makefile
	po/Makefile.in
	proc/Makefile
	proc/libprocps.pc
	ps/Makefile
	testsuite/Makefile
	top/Makefile
])

AC_OUTPUT
