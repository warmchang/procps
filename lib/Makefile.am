AM_CPPFLAGS = \
	-include $(top_builddir)/config.h \
	-I$(top_srcdir) \
	-I$(top_srcdir)/include \
	-DLOCALEDIR=\"$(localedir)\"

noinst_PROGRAMS = test_strutils test_fileutils test_nsutils

test_strutils_SOURCES = test_strutils.c strutils.c
test_fileutils_SOURCES = test_fileutils.c fileutils.c
test_nsutils_SOURCES = test_nsutils.c nsutils.c
