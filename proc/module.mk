# This file gets included into the main Makefile, in the top directory.

NAME      :=  proc

LIBSRC += $(wildcard proc/*.c)

SONAME    :=  lib$(NAME).so.$(LIBVERSION)

ALL        += proc/lib$(NAME).a
INSTALL    += $(lib)/lib$(NAME).a
LIB_CFLAGS := $(CFLAGS)

ifeq ($(SHARED),1)
ALL        += proc/$(SONAME)
INSTALL    += $(lib)/$(SONAME)
LIB_CFLAGS += -fpic
endif

# clean away all output files, .depend, and symlinks
CLEAN += $(addprefix proc/,$(ALL) .depend)

------

# INSTALLATION OPTIONS
HDRDIR    := /usr/include/$(NAME)#	where to put .h files
LIBDIR    := /usr/lib#		where to put library files
HDROWN    := $(OWNERGROUP) #		owner of header files
LIBOWN    := $(OWNERGROUP) #		owner of library files
INSTALL   := install

SRC       :=  $(sort $(wildcard *.c) $(filter %.c,$(RCSFILES)))
HDR       :=  $(sort $(wildcard *.h) $(filter %.h,$(RCSFILES)))
OBJ       :=  $(SRC:.c=.o)


proc/lib$(NAME).a: $(OBJ)
	$(AR) rcs $@ $^

proc/$(SONAME): $(OBJ)
	gcc -shared -Wl,-soname,$(SONAME) -o $@ $^ -lc
	ln -sf $(SONAME) lib$(NAME).so


# AUTOMATIC DEPENDENCY GENERATION -- GCC AND GNUMAKE DEPENDENT
.depend:
	$(strip $(CC) $(LIB_CFLAGS) -MM -MG $(SRC) > .depend)
-include .depend


# INSTALLATION
install: all
	if ! [ -d $(HDRDIR) ] ; then mkdir $(HDRDIR) ; fi
	$(INSTALL) $(HDROWN) $(HDR) /usr/include/$(NAME)
	$(INSTALL) $(LIBOWN) lib$(NAME).a $(LIBDIR)
ifeq ($(SHARED),1)
	$(INSTALL) $(LIBOWN) $(SONAME) $(SHLIBDIR)
	cd $(SHLIBDIR) && ln -sf $(SONAME) lib$(NAME).so
	ldconfig
endif


# CUSTOM c -> o rule so that command-line has minimal whitespace
%.o : %.c
	$(strip $(CC) $(LIB_CFLAGS) -c $<)


version.o:	version.c version.h
ifdef MINORVERSION
	$(strip $(CC) $(LIB_CFLAGS) -DVERSION=\"$(VERSION)\" -DSUBVERSION=\"$(SUBVERSION)\" -DMINORVERSION=\"$(MINORVERSION)\" -c version.c)
else
	$(strip $(CC) $(LIB_CFLAGS) -DVERSION=\"$(VERSION)\" -DSUBVERSION=\"$(SUBVERSION)\" -c version.c)
endif
