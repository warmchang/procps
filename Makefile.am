CYGWINFLAGS =
if CYGWIN
CYGWINFLAGS += -lintl
usrbin_exec_PROGRAMS =
endif

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
LDADD = ./proc/libprocps.la $(CYGWINFLAGS)

if CYGWIN
AM_LDFLAGS = ./proc/libprocps.la $(CYGWINFLAGS)
endif

transform =

if !CYGWIN
transform += s/pscommand/ps/; $(program_transform_name)
sbin_PROGRAMS = \
	sysctl
else
transform += s/pscommand/procps/; $(program_transform_name)
endif

if !CYGWIN
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
else
usrbin_exec_PROGRAMS += \
	ps/pscommand \
	free \
	pgrep \
	pkill \
	pmap \
	uptime \
	vmstat \
	w
endif

lib_LTLIBRARIES = \
	proc/libprocps.la

dist_man_MANS = \
	free.1 \
	pgrep.1 \
	pkill.1 \
	pmap.1 \
	uptime.1 \
	vmstat.8 \
	w.1 \
	ps/procps.1

if !CYGWIN
dist_man_MANS += \
	pwdx.1 \
	tload.1 \
	sysctl.8 \
	sysctl.conf.5 \
	ps/ps.1
endif

EXTRA_DIST = \
	.version \
	autogen.sh \
	contrib \
	COPYING.LIB \
	misc/git-version-gen \
	Documentation/CodingStyle.md \
	Documentation/TODO \
	ps/HACKING \
	ps/regression \
	$(DIST_MAN_POTS)

procpsngdir = $(docdir)
dist_procpsng_DATA = \
	Documentation/bugs.md \
	Documentation/FAQ

if !CYGWIN
EXTRA_DIST += \
	sysctl.conf
endif

if BUILD_PIDOF
if !CYGWIN
bin_PROGRAMS += pidof
else
usrbin_exec_PROGRAMS += pidof
endif
dist_man_MANS += pidof.1
pidof_SOURCES = pidof.c lib/fileutils.c
else
  EXTRA_DIST += pidof.1
endif

if BUILD_KILL
if CYGWIN
transform += ;s/^kill/prockill/; $(program_transform_name)
usrbin_exec_PROGRAMS += kill
else
bin_PROGRAMS += kill
endif

dist_man_MANS += kill.1
kill_SOURCES = skill.c lib/strutils.c lib/fileutils.c lib/nsutils.c
else
  EXTRA_DIST += kill.1
endif

if WITH_NCURSES
if !CYGWIN
bin_PROGRAMS += \
	slabtop \
	watch \
	top/top
else
usrbin_exec_PROGRAMS += \
	watch \
	top/top
endif

dist_man_MANS += \
	watch.1 \
	top/top.1
watch_SOURCES = watch.c lib/strutils.c lib/fileutils.c
watch_LDADD = @WATCH_NCURSES_LIBS@ $(CYGWINFLAGS)
watch_CFLAGS = @WATCH_NCURSES_CFLAGS@
top_top_SOURCES = \
	top/top.h \
	top/top.c \
	top/top_nls.h \
	top/top_nls.c \
	lib/fileutils.c

if CYGWIN
top_top_SOURCES += lib/strverscmp.c
else
dist_man_MANS += \
	slabtop.1
slabtop_SOURCES = slabtop.c lib/strutils.c lib/fileutils.c
slabtop_LDADD = $(LDADD) @NCURSES_LIBS@
endif

top_top_LDADD = $(LDADD) @NCURSES_LIBS@ $(DL_LIB)
endif

if BUILD_SKILL
if !CYGWIN
bin_PROGRAMS += \
	skill \
	snice
else
usrbin_exec_PROGRAMS += \
	skill \
	snice
endif
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
if !CYGWIN
pwdx_SOURCES = pwdx.c lib/fileutils.c
pwdx_LDADD= $(CYGWINFLAGS)
sysctl_SOURCES = \
	sysctl.c \
	lib/fileutils.c \
	procio.c
endif
tload_SOURCES = tload.c lib/strutils.c lib/fileutils.c
uptime_SOURCES = uptime.c lib/fileutils.c
vmstat_SOURCES = vmstat.c lib/strutils.c lib/fileutils.c
w_SOURCES = w.c lib/fileutils.c

# proc/libprocps.la

# See http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
LIBprocps_CURRENT=8
LIBprocps_REVISION=2
LIBprocps_AGE=0

proc_libprocps_la_LIBADD = $(LIB_KPARTS)

if WITH_SYSTEMD
proc_libprocps_la_LIBADD += @SYSTEMD_LIBS@
endif

if WITH_ELOGIND
proc_libprocps_la_LIBADD += @ELOGIND_LIBS@
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
	proc/numa.c \
	proc/numa.h \
	proc/procps-private.h \
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
	proc/wchan.c \
	proc/wchan.h \
	proc/whattime.c \
	proc/whattime.h

proc_libprocps_la_includedir = $(includedir)/proc/
proc_libprocps_la_include_HEADERS = \
	proc/alloc.h \
	proc/devname.h \
	proc/escape.h \
	proc/numa.h \
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

# Test programs not used by dejagnu but run directly
TESTS = \
	lib/test_strtod_nol
check_PROGRAMS = $(TESTS)

# Test programs required for dejagnu
noinst_PROGRAMS = \
	lib/test_strutils \
	lib/test_fileutils \
	lib/test_nsutils \
	lib/test_process

lib_test_strutils_SOURCES = lib/test_strutils.c lib/strutils.c
lib_test_strutils_LDADD = $(CYGWINFLAGS)
lib_test_fileutils_SOURCES = lib/test_fileutils.c lib/fileutils.c
lib_test_fileutils_LDADD = $(CYGWINFLAGS)
lib_test_nsutils_SOURCES = lib/test_nsutils.c lib/nsutils.c
lib_test_nsutils_LDADD = $(CYGWINFLAGS)
lib_test_process_SOURCES = lib/test_process.c
lib_test_process_LDADD = $(CYGWINFLAGS)

lib_test_strtod_nol_SOURCES = lib/test_strtod_nol.c lib/strutils.c
lib_test_strtod_nol_LDADD = $(CYGWINFLAGS)

if CYGWIN
	skill_LDADD = $(CYGWINFLAGS)
	kill_LDADD = $(CYGWINFLAGS)
	free_LDADD = $(CYGWINFLAGS)
	pgrep_LDADD = $(CYGWINFLAGS)
	pkill_LDADD = $(CYGWINFLAGS)
	pidof_LDADD = $(CYGWINFLAGS)
	pmap_LDADD = $(CYGWINFLAGS)
	snice_LDADD = $(CYGWINFLAGS)
	tload_LDADD = $(CYGWINFLAGS)
	uptime_LDADD = $(CYGWINFLAGS)
	w_LDADD = $(CYGWINFLAGS)
	vmstat_LDADD = $(CYGWINFLAGS)
endif

if EXAMPLE_FILES
if !CYGWIN
sysconf_DATA = sysctl.conf
endif
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
