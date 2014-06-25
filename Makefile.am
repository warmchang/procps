#AUTOMAKE_OPTIONS = subdir-objects

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
	create-man-pot.sh \
	translate-man.sh \
	contrib \
	COPYING.LIB \
	misc/git-version-gen \
	Documentation/CodingStyle \
	Documentation/TODO \
	sysctl.conf \
	$(DIST_MAN_POTS)

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
# $(srcdir)/create-man-pot.sh
	$(srcdir)/translate-man.sh

MAN_PS_POT = man-po/template-man-ps.pot
MAN_PS_POT_FILES = $(srcdir)/ps/ps.1
MAN_TOP_POT = man-po/template-man-top.pot
MAN_TOP_POT_FILES = $(srcdir)/top/top.1
MAN_POT = man-po/template-man.pot
MAN_POT_FILES = $(srcdir)/free.1 $(srcdir)/kill.1 $(srcdir)/pgrep.1 \
		  $(srcdir)/pidof.1 $(srcdir)/pkill.1 $(srcdir)/pmap.1 \
		  $(srcdir)/pwdx.1 $(srcdir)/skill.1 $(srcdir)/slabtop.1 \
		  $(srcdir)/snice.1 $(srcdir)/sysctl.8 $(srcdir)/uptime.1 \
		  $(srcdir)/sysctl.conf.5 $(srcdir)/tload.1 \
		  $(srcdir)/vmstat.8 $(srcdir)/w.1 $(srcdir)/watch.1
DIST_MAN_POTS = $(MAN_PS_POT) $(MAN_TOP_POT) $(MAN_POT)
CLEANFILES = $(DIST_MAN_POTS)

$(MAN_PS_POT): $(MAN_PS_POT_FILES)
	po4a-updatepo -f man -m $< -p $@

$(MAN_TOP_POT): $(MAN_TOP_POT_FILES)
	po4a-updatepo -f man -m $< -p $@

$(MAN_POT): $(MAN_POT_FILES)
	po4a-updatepo -f man -m $< -p $@

get-trans:
	echo "Getting the latest translations from translationproject.org..."
	rsync -Lrtvz  translationproject.org::tp/latest/procps-ng/  po
	rsync -Lrtvz  translationproject.org::tp/latest/procps-ng-man/  man-po
	rsync -Lrtvz  translationproject.org::tp/latest/procps-ng-man-ps/  man-po/ps
	rsync -Lrtvz  translationproject.org::tp/latest/procps-ng-man-top/  man-po/top
