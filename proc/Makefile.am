AM_CPPFLAGS = -include $(top_builddir)/config.h

lib_LTLIBRARIES = libproc.la

libproc_ladir = $(ladir)
libproc_la_LIBADD = $(LIB_KPARTS)
libproc_la_LDFLAGS = $(all_libraries) -release 3.2.8 -no-undefined

libproc_la_SOURCES = \
	alloc.c \
	alloc.h \
	devname.c \
	devname.h \
	escape.c \
	escape.h \
	ksym.c \
	procps.h \
	pwcache.c \
	pwcache.h \
	readproc.c \
	readproc.h \
	sig.c \
	sig.h \
	slab.c \
	slab.h \
	sysinfo.c \
	sysinfo.h \
	version.c \
	version.h \
	wchan.h \
	whattime.c \
	whattime.h

libproc_la_includedir = $(includedir)/proc/
libproc_la_include_HEADERS =  \
	alloc.h \
	devname.h \
	escape.h \
	procps.h \
	pwcache.h \
	readproc.h \
	sig.h \
	slab.h \
	sysinfo.h \
	version.h \
	wchan.h \
	whattime.h
