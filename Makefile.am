AM_CPPFLAGS = \
	-include $(top_builddir)/config.h \
	-I$(top_srcdir)/include \
	-DLOCALEDIR=\"$(localedir)\"

ACLOCAL_AMFLAGS = -I m4
SUBDIRS = \
	include \
	lib \
	po \
	proc \
	ps \
	testsuite \
	top

AM_CFLAGS = -Iproc
AM_LDFLAGS = ./proc/libprocps.la

sbin_PROGRAMS = \
	sysctl

usrbin_exec_PROGRAMS = \
	free \
	pgrep \
	pkill \
	pmap \
	pwdx \
	tload \
	uptime \
	vmstat \
	w

dist_man_MANS = \
	free.1 \
	pgrep.1 \
	pkill.1 \
	pmap.1 \
	pwdx.1 \
	sysctl.8 \
	sysctl.conf.5 \
	tload.1 \
	uptime.1 \
	vmstat.8 \
	w.1

EXTRA_DIST = \
	autogen.sh \
	contrib \
	COPYING.LIB \
	misc/git-version-gen \
	Documentation/CodingStyle \
	Documentation/TODO \
	sysctl.conf

if BUILD_PIDOF
usrbin_exec_PROGRAMS += pidof
dist_man_MANS += pidof.1
pidof_SOURCES = pidof.c $(top_srcdir)/lib/fileutils.c
else
  EXTRA_DIST += pidof.1
endif

if BUILD_KILL
bin_PROGRAMS = kill
dist_man_MANS += kill.1
kill_SOURCES = skill.c $(top_srcdir)/lib/strutils.c $(top_srcdir)/lib/fileutils.c $(top_srcdir)/lib/nsutils.c
else
  EXTRA_DIST += kill.1
endif

if WITH_NCURSES
usrbin_exec_PROGRAMS += \
	slabtop \
	watch
dist_man_MANS += \
	slabtop.1 \
	watch.1
slabtop_SOURCES = slabtop.c $(top_srcdir)/lib/strutils.c $(top_srcdir)/lib/fileutils.c
slabtop_LDADD = @NCURSES_LIBS@
watch_SOURCES = watch.c $(top_srcdir)/lib/strutils.c $(top_srcdir)/lib/fileutils.c
watch_LDADD = @WATCH_NCURSES_LIBS@
endif

if BUILD_SKILL
usrbin_exec_PROGRAMS += \
	skill \
	snice
skill_SOURCES = skill.c $(top_srcdir)/lib/strutils.c $(top_srcdir)/lib/fileutils.c $(top_srcdir)/lib/nsutils.c
snice_SOURCES = skill.c $(top_srcdir)/lib/strutils.c $(top_srcdir)/lib/fileutils.c $(top_srcdir)/lib/nsutils.c
dist_man_MANS += \
	skill.1 \
	snice.1
else
  EXTRA_DIST += \
	skill.1 \
	snice.1
endif

free_SOURCES = free.c $(top_srcdir)/lib/strutils.c $(top_srcdir)/lib/fileutils.c
pgrep_SOURCES = pgrep.c $(top_srcdir)/lib/fileutils.c $(top_srcdir)/lib/nsutils.c
pkill_SOURCES = pgrep.c $(top_srcdir)/lib/fileutils.c $(top_srcdir)/lib/nsutils.c
pmap_SOURCES = pmap.c $(top_srcdir)/lib/fileutils.c
pwdx_SOURCES = pwdx.c $(top_srcdir)/lib/fileutils.c
sysctl_SOURCES = sysctl.c $(top_srcdir)/lib/fileutils.c
tload_SOURCES = tload.c $(top_srcdir)/lib/strutils.c $(top_srcdir)/lib/fileutils.c
uptime_SOURCES = uptime.c $(top_srcdir)/lib/fileutils.c
vmstat_SOURCES = vmstat.c $(top_srcdir)/lib/strutils.c $(top_srcdir)/lib/fileutils.c
w_SOURCES = w.c $(top_srcdir)/lib/fileutils.c

if EXAMPLE_FILES
sysconf_DATA = sysctl.conf
endif

procpsngdir = $(docdir)
dist_procpsng_DATA = \
	Documentation/BUGS \
	Documentation/FAQ

BUILT_SOURCES = $(top_srcdir)/.version
$(top_srcdir)/.version:
	touch $(top_srcdir)/.version
	echo $(VERSION) > $@-t && mv $@-t $@
dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version

