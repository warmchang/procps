
SUBDIRS = proc ps

myexecbin_PROGRAMS = kill
myexecbindir = $(prefix)/bin

bin_PROGRAMS = oldps uptime tload free w top vmstat watch pgrep

# Derived (symlinked binaries)
bin_SCRIPTS = skill snice pkill

sbin_PROGRAMS = sysctl

skill: kill
	ln -f kill skill

snice: kill
	ln -f kill snice

pkill: pgrep
	ln -f pgrep pkill

LDADD = proc/libproc.la

kill_SOURCES = skill.c
kill_INCLUDES = -DSYSV
#kill_LDADD =

oldps_SOURCES = oldps.c
uptime_SOURCES = uptime.c
tload_SOURCES = tload.c
free_SOURCES = free.c
w_SOURCES = w.c
vmstat_SOURCES = vmstat.c
pgrep_SOURCES = pgrep.c

top_SOURCES = top.c
top_LDADD = proc/libproc.la @NCURSES_LIB@

watch_SOURCES = watch.c
watch_LDADD = @NCURSES_LIB@

man_MANS = free.1 kill.1 oldps.1 pgrep.1 pkill.1 skill.1 snice.1 sysctl.8 \
	       sysctl.conf.5 tload.1 top.1 uptime.1 vmstat.8 w.1 watch.1

EXTRA_DIST = man_MANS
