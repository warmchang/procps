# Google "recursive make considered harmful" for how this works.

SRC += $(wildcard proc/*.c) $(wildcard proc/*.c)

------

# PROJECT SPECIFIC MACROS
NAME       =  proc

# INSTALLATION OPTIONS
TOPDIR     = /usr
HDRDIR     = $(TOPDIR)/include/$(NAME)#	where to put .h files
LIBDIR     = $(TOPDIR)/lib#		where to put library files
SHLIBDIR   = /lib#			where to put shared library files
HDROWN     = $(OWNERGROUP) #		owner of header files
LIBOWN     = $(OWNERGROUP) #		owner of library files
INSTALL    = install

SRC        =  $(sort $(wildcard *.c) $(filter %.c,$(RCSFILES)))
HDR        =  $(sort $(wildcard *.h) $(filter %.h,$(RCSFILES)))
OBJ        =  $(SRC:.c=.o)


SONAME     =  lib$(NAME).so.$(LIBVERSION)

ifeq ($(SHARED),1)
CFLAGS += -fpic
all: lib$(NAME).a $(SONAME)
else
all: lib$(NAME).a
endif

lib$(NAME).a: $(OBJ)
	$(AR) rcs $@ $^

$(SONAME): $(OBJ)
	gcc -shared -Wl,-soname,$(SONAME) -o $@ $^ -lc
	ln -sf $(SONAME) lib$(NAME).so


# AUTOMATIC DEPENDENCY GENERATION -- GCC AND GNUMAKE DEPENDENT
.depend:
	$(strip $(CC) $(CFLAGS) -MM -MG $(SRC) > .depend)
-include .depend


# INSTALLATION
install: all
	if ! [ -d $(HDRDIR) ] ; then mkdir $(HDRDIR) ; fi
	$(INSTALL) $(HDROWN) $(HDR) $(TOPDIR)/include/$(NAME)
	$(INSTALL) $(LIBOWN) lib$(NAME).a $(LIBDIR)
ifeq ($(SHARED),1)
	$(INSTALL) $(LIBOWN) $(SONAME) $(SHLIBDIR)
	cd $(SHLIBDIR) && ln -sf $(SONAME) lib$(NAME).so
	ldconfig
endif


# CUSTOM c -> o rule so that command-line has minimal whitespace
%.o : %.c
	$(strip $(CC) $(CFLAGS) -c $<)


version.o:	version.c version.h
ifdef MINORVERSION
	$(strip $(CC) $(CFLAGS) -DVERSION=\"$(VERSION)\" -DSUBVERSION=\"$(SUBVERSION)\" -DMINORVERSION=\"$(MINORVERSION)\" -c version.c)
else
	$(strip $(CC) $(CFLAGS) -DVERSION=\"$(VERSION)\" -DSUBVERSION=\"$(SUBVERSION)\" -c version.c)
endif
