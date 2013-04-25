AM_CPPFLAGS = \
	-include $(top_builddir)/config.h \
	-I$(top_srcdir)/include \
	-DLOCALEDIR=\"$(localedir)\"

AM_LDFLAGS = ../proc/libprocps.la

if WITH_SYSTEMD
AM_LDFLAGS += @SYSTEMD_LIBS@
endif

dist_man_MANS = ps.1

# Use `ginstall' in the definition of PROGRAMS and in dependencies to avoid
# confusion with the `install' target.  The install rule transforms `ginstall'
# to install before applying any user-specified name transformations.
transform = s/pscommand/ps/; $(program_transform_name)
bin_PROGRAMS = pscommand
pscommand_SOURCES =  \
	common.h \
	display.c \
	global.c \
	help.c \
	output.c \
	parser.c \
	select.c \
	sortformat.c \
	stacktrace.c \
	$(top_srcdir)/lib/fileutils.c

EXTRA_DIST = \
	HACKING \
	regression
