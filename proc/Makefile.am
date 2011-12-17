PACKAGE_VERSION = @PACKAGE_VERSION@
AM_CPPFLAGS = -include $(top_builddir)/config.h

LIBPROCFS_CURRENT=0
LIBPROCFS_REVISION=0
LIBPROCFS_AGE=0

lib_LTLIBRARIES = libprocfs.la

libprocfs_ladir = $(ladir)
libprocfs_la_LIBADD = $(LIB_KPARTS)
libprocfs_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(all_libraries) \
	-version-info $(LIBPROCFS_CURRENT):$(LIBPROCFS_REVISION):$(LIBPROCFS_AGE) \
	-no-undefined \
	-Wl,--version-script=$(top_srcdir)/proc/libprocfs.sym

EXTRA_DIST = library.map

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libprocfs.pc

libprocfs_la_SOURCES = \
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

libprocfs_la_includedir = $(includedir)/proc/
libprocfs_la_include_HEADERS =  \
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
