
myexecbin_PROGRAMS = ps
myexecbindir = $(prefix)/bin

ps_SOURCES = common.h display.c escape.c global.c help.c output.c \
	         parser.c select.c sortformat.c stacktrace.c
ps_LDADD = ${top_srcdir}/proc/libproc.la

man_MANS = ps.1

EXTRA_DIST = $(man_MANS)
