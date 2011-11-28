AM_CPPFLAGS = -include $(top_builddir)/config.h

ACLOCAL_AMFLAGS = -I m4
SUBDIRS = proc ps testsuite

AM_CFLAGS = -Iproc
AM_LDFLAGS = ./proc/libproc-ng.la

sbin_PROGRAMS = \
	sysctl

usrbin_exec_PROGRAMS = \
	free \
	pgrep \
	pkill \
	pmap \
	pwdx \
	skill \
	snice \
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
	skill.1 \
	snice.1 \
	sysctl.8 \
	sysctl.conf.5 \
	tload.1 \
	uptime.1 \
	vmstat.8 \
	w.1

if BUILD_KILL
bin_PROGRAMS = kill
dist_man_MANS += kill.1
endif

if WITH_NCURSES
usrbin_exec_PROGRAMS += \
	slabtop \
	top \
	watch
top_SOURCES = top.c top.h
dist_man_MANS += \
	slabtop.1 \
	top.1 \
	watch.1
slabtop_LDADD = @NCURSES_LIBS@
top_LDADD = @NCURSES_LIBS@
watch_LDADD = @NCURSES_LIBS@
endif

kill_SOURCES = skill.c
snice_SOURCES = skill.c
pkill_SOURCES = pgrep.c

sysconf_DATA = sysctl.conf

EXTRA_DIST = \
	autogen.sh \
	CodingStyle \
	contrib/ \
	COPYING.LIB \
	$(sysconf_DATA)

procpsngdir = $(docdir)
dist_procpsng_DATA = \
	BUGS \
	FAQ \
	README.top
