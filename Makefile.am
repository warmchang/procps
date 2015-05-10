AM_CPPFLAGS = \
	-include $(top_builddir)/config.h \
	-I$(top_srcdir) \
	-I$(top_srcdir)/include \
	-DLOCALEDIR=\"$(localedir)\"

PACKAGE_VERSION = @PACKAGE_VERSION@

ACLOCAL_AMFLAGS = -I m4
SUBDIRS = \
	include \
	man-po \
	po \
	testsuite

AM_CFLAGS = -Iproc
AM_LDFLAGS = ./proc/libprocps.la

transform = s/pscommand/ps/; $(program_transform_name)

bin_PROGRAMS = \
	ps/pscommand \
	free \
	pgrep \
	pkill \
	pmap \
	pwdx \
	tload \
	uptime \
	vmstat \
	w

sbin_PROGRAMS = \
	sysctl

lib_LTLIBRARIES = \
	proc/libprocps.la

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
	w.1 \
	ps/ps.1

EXTRA_DIST = \
	autogen.sh \
	contrib \
	COPYING.LIB \
	misc/git-version-gen \
	Documentation/CodingStyle \
	Documentation/TODO \
	sysctl.conf \
	ps/HACKING \
	ps/regression \
	$(DIST_MAN_POTS)

procpsngdir = $(docdir)
dist_procpsng_DATA = \
	Documentation/BUGS \
	Documentation/FAQ

if BUILD_PIDOF
bin_PROGRAMS += pidof
dist_man_MANS += pidof.1
pidof_SOURCES = pidof.c lib/fileutils.c
else
  EXTRA_DIST += pidof.1
endif

if BUILD_KILL
bin_PROGRAMS += kill
dist_man_MANS += kill.1
kill_SOURCES = skill.c lib/strutils.c lib/fileutils.c lib/nsutils.c
else
  EXTRA_DIST += kill.1
endif

if WITH_NCURSES
bin_PROGRAMS += \
	slabtop \
	watch \
	top/top
dist_man_MANS += \
	slabtop.1 \
	watch.1 \
	top/top.1
slabtop_SOURCES = slabtop.c lib/strutils.c lib/fileutils.c
slabtop_LDADD = @NCURSES_LIBS@
watch_SOURCES = watch.c lib/strutils.c lib/fileutils.c
watch_LDADD = @WATCH_NCURSES_LIBS@
top_top_SOURCES = \
	top/top.h \
	top/top.c \
	top/top_nls.h \
	top/top_nls.c \
	lib/fileutils.c
top_top_LDADD = @NCURSES_LIBS@ $(DL_LIB)
dist_procpsng_DATA += \
	top/README.top
endif

if BUILD_SKILL
bin_PROGRAMS += \
	skill \
	snice
skill_SOURCES = skill.c lib/strutils.c lib/fileutils.c lib/nsutils.c
snice_SOURCES = skill.c lib/strutils.c lib/fileutils.c lib/nsutils.c
dist_man_MANS += \
	skill.1 \
	snice.1
else
  EXTRA_DIST += \
	skill.1 \
	snice.1
endif

free_SOURCES = free.c lib/strutils.c lib/fileutils.c
pgrep_SOURCES = pgrep.c lib/fileutils.c lib/nsutils.c
pkill_SOURCES = pgrep.c lib/fileutils.c lib/nsutils.c
pmap_SOURCES = pmap.c lib/fileutils.c
pwdx_SOURCES = pwdx.c lib/fileutils.c
sysctl_SOURCES = sysctl.c lib/fileutils.c
tload_SOURCES = tload.c lib/strutils.c lib/fileutils.c
uptime_SOURCES = uptime.c lib/fileutils.c
vmstat_SOURCES = vmstat.c lib/strutils.c lib/fileutils.c
w_SOURCES = w.c lib/fileutils.c

# proc/libprocps.la

# See http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
LIBprocps_CURRENT=4
LIBprocps_REVISION=0
LIBprocps_AGE=0

proc_libprocps_la_LIBADD = $(LIB_KPARTS)

if WITH_SYSTEMD
proc_libprocps_la_LIBADD += @SYSTEMD_LIBS@
endif

proc_libprocps_la_LDFLAGS = \
	-version-info $(LIBprocps_CURRENT):$(LIBprocps_REVISION):$(LIBprocps_AGE) \
	-no-undefined \
	-Wl,--version-script=$(top_srcdir)/proc/libprocps.sym

proc_libprocps_la_SOURCES = \
	proc/alloc.c \
	proc/alloc.h \
	proc/devname.c \
	proc/devname.h \
	proc/escape.c \
	proc/escape.h \
	proc/ksym.c \
	proc/procps.h \
	proc/pwcache.c \
	proc/pwcache.h \
	proc/readproc.c \
	proc/readproc.h \
	proc/sig.c \
	proc/sig.h \
	proc/slab.c \
	proc/slab.h \
	proc/sysinfo.c \
	proc/sysinfo.h \
	proc/version.c \
	proc/version.h \
	proc/wchan.h \
	proc/whattime.c \
	proc/whattime.h

proc_libprocps_la_includedir = $(includedir)/proc/
proc_libprocps_la_include_HEADERS = \
	proc/alloc.h \
	proc/devname.h \
	proc/escape.h \
	proc/procps.h \
	proc/pwcache.h \
	proc/readproc.h \
	proc/sig.h \
	proc/slab.h \
	proc/sysinfo.h \
	proc/version.h \
	proc/wchan.h \
	proc/whattime.h

dist_man_MANS += \
	proc/openproc.3 \
	proc/readproc.3 \
	proc/readproctab.3

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = \
	proc/libprocps.pc

EXTRA_DIST += proc/libprocps.sym

# ps/pscommand

ps_pscommand_SOURCES =  \
	ps/common.h \
	ps/display.c \
	ps/global.c \
	ps/help.c \
	ps/output.c \
	ps/parser.c \
	ps/select.c \
	ps/sortformat.c \
	ps/stacktrace.c \
	lib/fileutils.c

# lib/test_* binaries
noinst_PROGRAMS = lib/test_strutils lib/test_fileutils lib/test_nsutils

lib_test_strutils_SOURCES = lib/test_strutils.c lib/strutils.c
lib_test_fileutils_SOURCES = lib/test_fileutils.c lib/fileutils.c
lib_test_nsutils_SOURCES = lib/test_nsutils.c lib/nsutils.c

if EXAMPLE_FILES
sysconf_DATA = sysctl.conf
endif

BUILT_SOURCES = $(top_srcdir)/.version

$(top_srcdir)/.version:
	touch $(top_srcdir)/.version
	echo $(VERSION) > $@-t && mv $@-t $@
dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version

get-trans:
	echo "Getting the latest translations from translationproject.org..."
	rsync -Lrtvz  translationproject.org::tp/latest/procps-ng/  po
	rsync -Lrtvz  translationproject.org::tp/latest/procps-ng-man/  man-po
	rsync -Lrtvz  translationproject.org::tp/latest/procps-ng-man-ps/  man-po/ps
	rsync -Lrtvz  translationproject.org::tp/latest/procps-ng-man-top/  man-po/top
