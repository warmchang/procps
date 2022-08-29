
CYGWINFLAGS =
if CYGWIN
CYGWINFLAGS += -lintl
usrbin_exec_PROGRAMS =
endif

AM_CPPFLAGS = \
	-include $(top_builddir)/config.h \
	-I$(top_srcdir) \
	-I$(top_srcdir)/include \
	-Wno-unused-result \
	-DLOCALEDIR=\"$(localedir)\" \
	@HARDEN_CFLAGS@

AM_LDFLAGS = @HARDEN_LDFLAGS@

PACKAGE_VERSION = @PACKAGE_VERSION@

ACLOCAL_AMFLAGS = -I m4
SUBDIRS = \
	include \
	po-man \
	po \
	testsuite

AM_CFLAGS = -Iproc
LDADD = ./proc/libproc-2.la $(CYGWINFLAGS)

if CYGWIN
AM_LDFLAGS += ./proc/libproc-2.la $(CYGWINFLAGS)
endif

transform =

if !CYGWIN
transform += s/pscommand/ps/; $(program_transform_name)
sbin_PROGRAMS = \
	src/sysctl
else
transform += s/pscommand/procps/; $(program_transform_name)
endif

if !CYGWIN
bin_PROGRAMS = \
	src/ps/pscommand \
	src/free \
	src/pgrep \
	src/pkill \
	src/pmap \
	src/pwdx \
	src/tload \
	src/uptime \
	src/vmstat
if BUILD_PIDWAIT
bin_PROGRAMS += src/pidwait
endif
else
usrbin_exec_PROGRAMS += \
	src/ps/pscommand \
	src/free \
	src/pgrep \
	src/pkill \
	src/pmap \
	src/uptime \
	src/vmstat
endif

lib_LTLIBRARIES = \
	proc/libproc-2.la

dist_man_MANS = \
	man/free.1 \
	man/pgrep.1 \
	man/pkill.1 \
	man/pmap.1 \
	man/uptime.1 \
	man/vmstat.8 \
	man/procps.3 \
	man/procps_pids.3 \
	man/procps_misc.3

if !CYGWIN
dist_man_MANS += \
	man/pwdx.1 \
	man/tload.1 \
	man/sysctl.8 \
	man/sysctl.conf.5 \
	man/ps.1

if BUILD_PIDWAIT
dist_man_MANS += man/pidwait.1
endif
endif

EXTRA_DIST = \
	.version \
	autogen.sh \
	COPYING.LIB \
	misc/git-version-gen \
	doc/CodingStyle.md \
	doc/TODO \
	src/ps/HACKING \
	src/ps/regression \
	$(DIST_MAN_POTS)

if !CYGWIN
EXTRA_DIST += \
	sysctl.conf
endif

procpsngdir = $(docdir)
dist_procpsng_DATA = \
	doc/bugs.md \
	doc/FAQ \
	doc/libproc.supp

if BUILD_PIDOF
if !CYGWIN
bin_PROGRAMS += src/pidof
else
usrbin_exec_PROGRAMS += src/pidof
endif
dist_man_MANS += man/pidof.1
src_pidof_SOURCES = src/pidof.c lib/fileutils.c
else
  EXTRA_DIST += man/pidof.1
endif

if BUILD_KILL
if CYGWIN
transform += ;s/^kill/prockill/; $(program_transform_name)
usrbin_exec_PROGRAMS += src/kill
else
bin_PROGRAMS += src/kill
endif
dist_man_MANS += man/kill.1
src_kill_SOURCES = src/kill.c lib/strutils.c lib/fileutils.c lib/signals.c
src_kill_LDADD =
else
  EXTRA_DIST += man/kill.1
endif

if BUILD_W
if CYGWIN
usrbin_exec_PROGRAMS += src/w
else
bin_PROGRAMS += src/w
endif

dist_man_MANS += man/w.1
src_w_SOURCES = src/w.c lib/fileutils.c
else
  EXTRA_DIST += man/w.1
endif

if WITH_NCURSES
if !CYGWIN
bin_PROGRAMS += \
	src/watch \
	src/top/top
else
usrbin_exec_PROGRAMS += \
	src/watch \
	src/top/top
endif

dist_man_MANS += \
	man/watch.1 \
	man/top.1
if !CYGWIN
bin_PROGRAMS += \
	src/slabtop
dist_man_MANS += \
	man/slabtop.1
src_slabtop_SOURCES = src/slabtop.c lib/strutils.c lib/fileutils.c
src_slabtop_LDADD = $(LDADD) @NCURSES_LIBS@
endif
src_watch_SOURCES = src/watch.c lib/strutils.c lib/fileutils.c
src_watch_LDADD = @NCURSES_LIBS@ $(CYGWINFLAGS)
src_top_top_SOURCES = \
	src/top/top.h \
	src/top/top.c \
	src/top/top_nls.h \
	src/top/top_nls.c \
	lib/fileutils.c \
	lib/signals.c
if CYGWIN
src_top_top_SOURCES += lib/strverscmp.c
endif

src_top_top_LDADD = $(LDADD) @NCURSES_LIBS@ $(DL_LIB)
endif

if BUILD_SKILL
if !CYGWIN
bin_PROGRAMS += \
	src/skill \
	src/snice
else
usrbin_exec_PROGRAMS += \
	src/skill \
	src/snice
endif
src_skill_SOURCES = src/skill.c lib/strutils.c lib/fileutils.c lib/signals.c
src_snice_SOURCES = src/skill.c lib/strutils.c lib/fileutils.c lib/signals.c
dist_man_MANS += \
	man/skill.1 \
	man/snice.1
else
  EXTRA_DIST += \
	man/skill.1 \
	man/snice.1
endif

src_free_SOURCES = src/free.c lib/strutils.c lib/fileutils.c
src_pgrep_SOURCES = src/pgrep.c lib/fileutils.c lib/signals.c
src_pkill_SOURCES = src/pgrep.c lib/fileutils.c lib/signals.c
src_pmap_SOURCES = src/pmap.c lib/fileutils.c
if BUILD_PIDWAIT
src_pidwait_SOURCES = src/pgrep.c lib/fileutils.c lib/signals.c
endif
if !CYGWIN
src_pwdx_SOURCES = src/pwdx.c lib/fileutils.c
src_pwdx_LDADD= $(CYGWINFLAGS)
src_sysctl_SOURCES = \
	src/sysctl.c \
	lib/fileutils.c \
	lib/procio.c
src_sysctl_LDADD=
endif
src_tload_SOURCES = src/tload.c lib/strutils.c lib/fileutils.c
src_uptime_SOURCES = src/uptime.c lib/fileutils.c
src_vmstat_SOURCES = src/vmstat.c lib/strutils.c lib/fileutils.c


# See http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
LIBproc_2_CURRENT=0
LIBproc_2_REVISION=0
LIBproc_2_AGE=0

proc_libproc_2_la_LIBADD = $(LIB_KPARTS)

if WITH_SYSTEMD
proc_libproc_2_la_LIBADD += @SYSTEMD_LIBS@
endif

if WITH_ELOGIND
proc_libproc_2_la_LIBADD += @ELOGIND_LIBS@
endif

proc_libproc_2_la_LDFLAGS = \
	-version-info $(LIBproc_2_CURRENT):$(LIBproc_2_REVISION):$(LIBproc_2_AGE) \
	-no-undefined \
	-Wl,--version-script=$(top_srcdir)/proc/libproc-2.sym

proc_libproc_2_la_SOURCES = \
	proc/devname.c \
	proc/devname.h \
	proc/diskstats.c \
	proc/diskstats.h \
	proc/escape.c \
	proc/escape.h \
	proc/procps-private.h \
	proc/meminfo.c \
	proc/meminfo.h \
	proc/misc.h \
	proc/namespace.c \
	proc/numa.c \
	proc/numa.h \
	proc/pids.c \
	proc/pids.h \
	proc/pwcache.c \
	proc/pwcache.h \
	proc/readproc.c \
	proc/readproc.h \
	proc/slabinfo.c \
	proc/slabinfo.h \
	proc/stat.c \
	proc/stat.h \
	proc/sysinfo.c \
	proc/version.c \
	proc/vmstat.c \
	proc/vmstat.h \
	proc/wchan.c \
	proc/wchan.h \
	proc/uptime.c \
	proc/xtra-procps-debug.h

proc_libproc_2_la_includedir = $(includedir)/procps/
proc_libproc_2_la_include_HEADERS = \
	proc/diskstats.h \
	proc/meminfo.h \
	proc/misc.h \
	proc/pids.h \
	proc/slabinfo.h \
	proc/stat.h \
	proc/vmstat.h \
	proc/xtra-procps-debug.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = \
	proc/libproc-2.pc

EXTRA_DIST += proc/libproc-2.sym

# ps/pscommand

src_ps_pscommand_SOURCES =  \
	src/ps/common.h \
	src/ps/display.c \
	src/ps/global.c \
	src/ps/help.c \
	src/ps/output.c \
	src/ps/parser.c \
	src/ps/select.c \
	src/ps/sortformat.c \
	src/ps/stacktrace.c \
	lib/fileutils.c \
	lib/signals.c


# Test programs required for dejagnu or standalone testing
check_PROGRAMS = \
	lib/test_strutils \
	lib/test_fileutils \
	lib/test_process \
	lib/test_strtod_nol \
	lib/test_shm

lib_test_strutils_SOURCES = lib/test_strutils.c lib/strutils.c
lib_test_strutils_LDADD = $(CYGWINFLAGS)
lib_test_fileutils_SOURCES = lib/test_fileutils.c lib/fileutils.c
lib_test_fileutils_LDADD = $(CYGWINFLAGS)
lib_test_process_SOURCES = lib/test_process.c
lib_test_process_LDADD = $(CYGWINFLAGS)
lib_test_strtod_nol_SOURCES = lib/test_strtod_nol.c lib/strutils.c
lib_test_strtod_nol_LDADD = $(CYGWINFLAGS)
lib_test_shm_SOURCES = lib/test_shm.c lib/strutils.c
lib_test_shm_LDADD = $(CYGWINFLAGS)

check_PROGRAMS += \
	proc/test_Itemtables \
	proc/test_pids \
	proc/test_uptime \
	proc/test_sysinfo \
	proc/test_version \
	proc/test_namespace

proc_test_Itemtables_SOURCES = proc/test_Itemtables.c
proc_test_Itemtables_LDADD = proc/libproc-2.la
proc_test_pids_SOURCES = proc/test_pids.c
proc_test_pids_LDADD = proc/libproc-2.la
proc_test_uptime_SOURCES = proc/test_uptime.c
proc_test_uptime_LDADD = proc/libproc-2.la
proc_test_sysinfo_SOURCES = proc/test_sysinfo.c
proc_test_sysinfo_LDADD = proc/libproc-2.la
proc_test_version_SOURCES = proc/test_version.c
proc_test_version_LDADD = proc/libproc-2.la
proc_test_namespace_SOURCES = proc/test_namespace.c
proc_test_namespace_LDADD = proc/libproc-2.la

if CYGWIN
	src_skill_LDADD = $(CYGWINFLAGS)
	src_kill_LDADD = $(CYGWINFLAGS)
	src_free_LDADD = $(CYGWINFLAGS)
	src_pgrep_LDADD = $(CYGWINFLAGS)
	src_pkill_LDADD = $(CYGWINFLAGS)
	src_pidof_LDADD = $(CYGWINFLAGS)
	src_pmap_LDADD = $(CYGWINFLAGS)
	src_snice_LDADD = $(CYGWINFLAGS)
	src_tload_LDADD = $(CYGWINFLAGS)
	src_uptime_LDADD = $(CYGWINFLAGS)
	src_w_LDADD = $(CYGWINFLAGS)
	src_vmstat_LDADD = $(CYGWINFLAGS)
endif

if EXAMPLE_FILES
if !CYGWIN
sysconf_DATA = sysctl.conf
endif
endif

BUILT_SOURCES = $(top_srcdir)/.version

check-lib: clean
	$(MAKE) CFLAGS=-DITEMTABLE_DEBUG proc/test_Itemtables
	$(top_builddir)/proc/test_Itemtables
	$(MAKE) clean &>/dev/null

# Test programs not used by dejagnu but run directly
TESTS = \
	proc/test_pids \
	proc/test_uptime \
	proc/test_sysinfo \
	proc/test_version \
	proc/test_namespace \
	lib/test_fileutils \
	lib/test_strtod_nol

# Automake should do this, but it doesn't
check: $(check_PROGRAMS) $(PROGRAMS)

$(top_srcdir)/.version:
	touch $(top_srcdir)/.version
	echo $(VERSION) > $@-t && mv $@-t $@
dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version

get-trans:
	echo "Getting the latest translations from translationproject.org..."
	rsync -Lrtvz  translationproject.org::tp/latest/procps-ng/  po
	rsync -Lrtvz  translationproject.org::tp/latest/procps-ng-man/  po-man
