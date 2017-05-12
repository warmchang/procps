CYGWINFLAGS =
if CYGWIN
  CYGWINFLAGS += -lintl
endif

AM_CPPFLAGS = \
	-include $(top_builddir)/config.h \
	-I$(top_srcdir) \
	-I$(top_srcdir)/include \
	-DLOCALEDIR=\"$(localedir)\" \
	@HARDEN_CFLAGS@

AM_LDFLAGS = @HARDEN_LDFLAGS@

PACKAGE_VERSION = @PACKAGE_VERSION@

ACLOCAL_AMFLAGS = -I m4
SUBDIRS = \
	include \
	man-po \
	po \
	testsuite

AM_CFLAGS = -Iproc
LDADD = ./proc/libprocps.la $(CYGWINFLAGS)

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
	ps/ps.1 \
	doc/libproc.3 \
	doc/procps_linux_version.3 \
	doc/procps_pids_new.3 \
	doc/procps_pids_reap.3 \
	doc/procps_pids_ref.3 \
	doc/procps_pids_reset.3 \
	doc/procps_pids_select.3 \
	doc/procps_pids_sort.3 \
	doc/procps_pids_unref.3 \
	doc/procps_uptime.3 \
	doc/procps_uptime_sprint.3 \
	doc/procps_uptime_sprint_short.3

EXTRA_DIST = \
	.version \
	autogen.sh \
	contrib \
	COPYING.LIB \
	misc/git-version-gen \
	Documentation/CodingStyle.md \
	Documentation/TODO \
	sysctl.conf \
	ps/HACKING \
	ps/regression \
	$(DIST_MAN_POTS)

procpsngdir = $(docdir)
dist_procpsng_DATA = \
	Documentation/bugs.md \
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
kill_SOURCES = kill.c lib/strutils.c lib/fileutils.c lib/signals.c
kill_LDADD =
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
slabtop_LDADD = $(LDADD) @NCURSES_LIBS@
watch_SOURCES = watch.c lib/strutils.c lib/fileutils.c
watch_LDADD = @WATCH_NCURSES_LIBS@ $(CYGWINFLAGS)
top_top_SOURCES = \
	top/top.h \
	top/top.c \
	top/top_nls.h \
	top/top_nls.c \
	lib/fileutils.c \
	lib/signals.c
if CYGWIN
  top_top_SOURCES += lib/strverscmp.c
endif

top_top_LDADD = $(LDADD) @NCURSES_LIBS@ $(DL_LIB)
endif

if BUILD_SKILL
bin_PROGRAMS += \
	skill \
	snice
skill_SOURCES = skill.c lib/strutils.c lib/fileutils.c lib/signals.c
snice_SOURCES = skill.c lib/strutils.c lib/fileutils.c lib/signals.c
dist_man_MANS += \
	skill.1 \
	snice.1
else
  EXTRA_DIST += \
	skill.1 \
	snice.1
endif

free_SOURCES = free.c lib/strutils.c lib/fileutils.c
pgrep_SOURCES = pgrep.c lib/fileutils.c lib/signals.c
pkill_SOURCES = pgrep.c lib/fileutils.c lib/signals.c
pmap_SOURCES = pmap.c lib/fileutils.c
pwdx_SOURCES = pwdx.c lib/fileutils.c
pwdx_LDADD= $(CYGWINFLAGS)
sysctl_SOURCES = sysctl.c lib/fileutils.c
sysctl_LDADD=
tload_SOURCES = tload.c lib/strutils.c lib/fileutils.c
uptime_SOURCES = uptime.c lib/fileutils.c
vmstat_SOURCES = vmstat.c lib/strutils.c lib/fileutils.c
w_SOURCES = w.c lib/fileutils.c

# proc/libprocps.la

# See http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
LIBprocps_CURRENT=5
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
	proc/diskstats.c \
	proc/diskstats.h \
	proc/escape.c \
	proc/escape.h \
	proc/procps-private.h \
	proc/meminfo.c \
	proc/meminfo.h \
	proc/namespace.c \
	proc/namespace.h \
	proc/numa.c \
	proc/numa.h \
	proc/pids.c \
	proc/pids.h \
	proc/procps.h \
	proc/pwcache.c \
	proc/pwcache.h \
	proc/readproc.c \
	proc/readproc.h \
	proc/slabinfo.c \
	proc/slabinfo.h \
	proc/stat.c \
	proc/stat.h \
	proc/sysinfo.c \
	proc/sysinfo.h \
	proc/version.c \
	proc/version.h \
	proc/vmstat.c \
	proc/vmstat.h \
	proc/wchan.c \
	proc/wchan.h \
	proc/uptime.c \
	proc/uptime.h \
	proc/xtra-procps-debug.h

proc_libprocps_la_includedir = $(includedir)/proc/
proc_libprocps_la_include_HEADERS = \
	proc/alloc.h \
	proc/devname.h \
	proc/diskstats.h \
	proc/escape.h \
	proc/numa.h \
	proc/pids.h \
	proc/procps.h \
	proc/pwcache.h \
	proc/readproc.h \
	proc/slabinfo.h \
	proc/sysinfo.h \
	proc/uptime.h \
	proc/version.h \
	proc/vmstat.h \
	proc/wchan.h \
	proc/xtra-procps-debug.h

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
	lib/fileutils.c \
	lib/signals.c


# Test programs required for dejagnu
noinst_PROGRAMS = \
	lib/test_strutils \
	lib/test_fileutils \
	lib/test_process

lib_test_strutils_SOURCES = lib/test_strutils.c lib/strutils.c
lib_test_strutils_LDADD = $(CYGWINFLAGS)
lib_test_fileutils_SOURCES = lib/test_fileutils.c lib/fileutils.c
lib_test_fileutils_LDADD = $(CYGWINFLAGS)
lib_test_process_SOURCES = lib/test_process.c
lib_test_process_LDADD = $(CYGWINFLAGS)

proc_test_sysinfo_SOURCES = proc/test_sysinfo.c
proc_test_sysinfo_LDADD = proc/libprocps.la
proc_test_namespace_SOURCES = proc/test_namespace.c
proc_test_namespace_LDADD = proc/libprocps.la
proc_test_pids_SOURCES = proc/test_pids.c
proc_test_pids_LDADD = proc/libprocps.la
proc_test_uptime_SOURCES = include/tests.h proc/test_uptime.c
proc_test_uptime_LDADD = proc/libprocps.la
proc_test_version_SOURCES = include/tests.h proc/test_version.c
proc_test_version_LDADD = proc/libprocps.la
lib_test_strtod_nol_SOURCES = lib/test_strtod_nol.c lib/strutils.c
lib_test_strtod_nol_LDADD = $(CYGWINFLAGS)

if EXAMPLE_FILES
sysconf_DATA = sysctl.conf
endif

BUILT_SOURCES = $(top_srcdir)/.version

# Test programs not used by dejagnu but run directly
TESTS = proc/test_sysinfo \
	proc/test_pids \
	proc/test_namespace \
	proc/test_uptime \
	proc/test_version \
	lib/test_strtod_nol
check_PROGRAMS = $(TESTS)

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
