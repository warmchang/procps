# procps Makefile
# Albert Cahalan, 2002-2003
#
# Recursive make is considered harmful:
# http://google.com/search?q=%22recursive+make+considered+harmful%22
#
# For now this Makefile uses explicit dependencies. The project
# hasn't grown big enough to need something complicated, and the
# dependency tracking files are an ugly annoyance.
#
# This file includes */module.mk files which add on to variables:
# FOO += bar/baz
#
#
# Set (or uncomment) SKIP if you wish to avoid something.
# For example, you may prefer the /bin/kill from util-linux or bsdutils.


VERSION      := 3
SUBVERSION   := 2
MINORVERSION := 0
TARVERSION   := 3.2.0
LIBVERSION   := 3.2.0

############ vars

# so you can disable them or choose alternates
ldconfig := ldconfig
ln_f     := ln -f
ln_sf    := ln -sf
install  := install -D --owner 0 --group 0

# Lame x86-64 /lib64 and /usr/lib64 abomination:
lib64    := lib$(shell [ -d /lib64 ] && echo 64)

usr/bin                  := $(DESTDIR)/usr/bin/
bin                      := $(DESTDIR)/bin/
sbin                     := $(DESTDIR)/sbin/
usr/proc/bin             := $(DESTDIR)/usr/bin/
man1                     := $(DESTDIR)/usr/share/man/man1/
man5                     := $(DESTDIR)/usr/share/man/man5/
man8                     := $(DESTDIR)/usr/share/man/man8/
etc/X11/applnk/Utilities := $(DESTDIR)/etc/X11/applnk/Utilities/
usr/X11R6/bin            := $(DESTDIR)/usr/X11R6/bin/
lib                      := $(DESTDIR)/$(lib64)/
usr/lib                  := $(DESTDIR)/usr/$(lib64)/
usr/include              := $(DESTDIR)/usr/include/

#SKIP     := $(bin)kill $(man1)kill.1

BINFILES := $(usr/bin)uptime $(usr/bin)tload $(usr/bin)free $(usr/bin)w \
            $(usr/bin)top $(usr/bin)vmstat $(usr/bin)watch $(usr/bin)skill \
            $(usr/bin)snice $(bin)kill $(sbin)sysctl $(usr/bin)pmap \
            $(usr/proc/bin)pgrep $(usr/proc/bin)pkill $(usr/bin)slabtop

MANFILES := $(man1)uptime.1 $(man1)tload.1 $(man1)free.1 $(man1)w.1 \
            $(man1)top.1 $(man1)watch.1 $(man1)skill.1 $(man1)kill.1 \
            $(man1)snice.1 $(man1)pgrep.1 $(man1)pkill.1 $(man1)pmap.1 \
            $(man5)sysctl.conf.5 $(man8)vmstat.8 $(man8)sysctl.8 \
            $(man1)slabtop.1

TARFILES := AUTHORS BUGS NEWS README TODO COPYING COPYING.LIB \
            Makefile procps.lsm procps.spec v t README.top CodingStyle \
            minimal.c $(notdir $(MANFILES)) dummy.c \
            uptime.c tload.c free.c w.c top.c vmstat.c watch.c skill.c \
            sysctl.c pgrep.c top.h pmap.c slabtop.c

# Stuff (tests, temporary hacks, etc.) left out of the standard tarball
# plus the top-level Makefile to make it work stand-alone.
_TARFILES := Makefile

CURSES := -I/usr/include/ncurses -lncurses

# Preprocessor flags.
PKG_CPPFLAGS := -D_GNU_SOURCE -I proc
CPPFLAGS :=
ALL_CPPFLAGS := $(PKG_CPPFLAGS) $(CPPFLAGS)

# Left out -Wconversion due to noise in glibc headers.
# Left out a number of things that older compilers lack:
# -Wpadded -Wunreachable-code -Wdisabled-optimization
#
# Since none of the PKG_CFLAGS things are truly required
# to compile procps, they might best be moved to CFLAGS.
# On the other hand, they aren't normal -O -g things either.
#
PKG_CFLAGS := -fno-common -ffast-math \
  -W -Wall -Wshadow -Wcast-align -Wredundant-decls \
  -Wbad-function-cast -Wcast-qual -Wwrite-strings -Waggregate-return \
  -Wstrict-prototypes -Wmissing-prototypes
CFLAGS := -O2 -g3
ALL_CFLAGS := $(PKG_CFLAGS) $(CFLAGS)

PKG_LDFLAGS := -Wl,-warn-common
LDFLAGS :=
ALL_LDFLAGS := $(PKG_LDFLAGS) $(LDFLAGS)

############ Add some extra flags if gcc allows

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),tar)  
ifneq ($(MAKECMDGOALS),extratar)

# Unlike the kernel one, this check_gcc goes all the way to
# producing an executable. There might be a -m64 that works
# until you go looking for a 64-bit curses library.
check_gcc = $(shell if $(CC) $(ALL_CFLAGS) dummy.c $(ALL_LDFLAGS) $(1) -o /dev/null $(CURSES) > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi ;)

ALL_CFLAGS += $(call check_gcc,-Wdeclaration-after-statement,)
ALL_CFLAGS += $(call check_gcc,-Wpadded,)

# Be 64-bit if at all possible.
ALL_CFLAGS += $(call check_gcc,-m64,)

endif
endif
endif

############ misc.

# free.c pmap.c sysctl.c uptime.c vmstat.c watch.c pgrep.c skill.c tload.c top.c w.c
# utmp.c oldtop.c tmp-junk.c minimal.c

.SUFFIXES:
.SUFFIXES: .a .o .c .s .h

.PHONY: all clean do_all install tar extratar

ALL := $(notdir $(BINFILES))

CLEAN := $(notdir $(BINFILES))

DIRS :=

INSTALL := $(BINFILES) $(MANFILES)

# want this rule first, use := on ALL, and ALL not filled in yet
all: do_all

-include */module.mk

do_all:    $(ALL)

junk := DEADJOE *~ *.o core gmon.out

# Remove $(junk) from all $(DIRS)
CLEAN += $(junk) $(foreach dir,$(DIRS),$(addprefix $(dir), $(junk)))

##########
# not maintained because it isn't really needed:
#
#SRC :=
#OBJ := $(patsubst %.c,%.o, $(filter %.c,$(SRC)))
#
#ifneq ($(MAKECMDGOALS),clean)
#-include $(OBJ:.o=.d)
#endif
#
#%.d: %.c
#	depend.sh $(ALL_CPPFLAGS) $(ALL_CFLAGS) $< > $@
############

# don't want to type "make procps-$(TARVERSION).tar.gz"
tar: $(TARFILES)
	mkdir procps-$(TARVERSION)
	(tar cf - $(TARFILES)) | (cd procps-$(TARVERSION) && tar xf -)
	tar cf procps-$(TARVERSION).tar procps-$(TARVERSION)
	gzip -9 procps-$(TARVERSION).tar

extratar: $(_TARFILES)
	mkdir procps-$(TARVERSION)
	(tar cf - $(_TARFILES)) | (cd procps-$(TARVERSION) && tar xf -)
	tar cf extra-$(TARVERSION).tar procps-$(TARVERSION)
	gzip -9 extra-$(TARVERSION).tar

clean:
	rm -f $(CLEAN)

###### install

$(BINFILES) : all
	$(install) --mode a=rx --strip $(notdir $@) $@

$(MANFILES) : all
	$(install) --mode a=r $(notdir $@) $@

install: $(filter-out $(SKIP) $(addprefix $(DESTDIR),$(SKIP)),$(INSTALL))
	cd $(usr/bin) && $(ln_f) skill snice
	cd $(usr/proc/bin) && $(ln_f) pgrep pkill

############ prog.c --> prog.o

top.o : top.h

%.o : %.c
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -c -o $@ $<

w.o:    w.c
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) $(W_SHOWFROM) -c $<

############ prog.o --> prog

pmap w uptime tload free sysctl vmstat utmp pgrep skill: % : %.o $(LIBPROC)
	$(CC) $(ALL_CFLAGS) $^ $(ALL_LDFLAGS) -o $@

slabtop top: % : %.o $(LIBPROC)
	$(CC) $(ALL_CFLAGS) $^ $(ALL_LDFLAGS) -o $@ $(CURSES)

watch: % : %.o
	$(CC) $(ALL_CFLAGS) $^ $(ALL_LDFLAGS) -o $@ $(CURSES)

############ progX --> progY

snice kill: skill
	ln -f skill $@

pkill: pgrep
	ln -f pgrep pkill
