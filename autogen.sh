#!/bin/sh
#
# Helps generate autoconf/automake stuff, when code is checked
# out from SCM.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

THEDIR=`pwd`
cd $srcdir
DIE=0

test -f top.c || {
	echo
	echo "You must run this script in the top-level procps-ng directory"
	echo
	DIE=1
}

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoconf installed to generate procps-ng build system."
	echo
	DIE=1
}
(autoheader --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoheader installed to generate procps-ng build system."
	echo "The autoheader command is part of the GNU autoconf package."
	echo
	DIE=1
}
(libtool --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have libtool-2 installed to generate procps-ng build system."
	echo
	DIE=1
}
(automake --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have automake installed to generate procps-ng build system."
	echo
	DIE=1
}

ltver=$(libtoolize --version | awk '/^libtoolize/ { print $4 }')
test ${ltver##2.} = "$ltver" && {
	echo "You must have libtool version >= 2.x.x, but you have $ltver."
	DIE=1
}

if test "$DIE" -eq 1; then
	exit 1
fi

echo
echo "Generate build-system by:"
echo "   aclocal:    $(aclocal --version | head -1)"
echo "   autoconf:   $(autoconf --version | head -1)"
echo "   autoheader: $(autoheader --version | head -1)"
echo "   automake:   $(automake --version | head -1)"
echo "   libtoolize: $(libtoolize --version | head -1)"

rm -rf autom4te.cache

set -e
libtoolize --force $LT_OPTS
aclocal -I m4 $AL_OPTS
autoconf $AC_OPTS
autoheader $AH_OPTS

automake --add-missing $AM_OPTS

cd $THEDIR

echo
echo "Now type '$srcdir/configure' and 'make' to compile."
echo
