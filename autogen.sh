#!/bin/sh
# Run this to generate all the initial makefiles, etc.
# This was lifted from the Gimp, and adapted slightly by
# Raph Levien .

DIE=0
srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.
test "$srcdir" = "." && srcdir=`pwd`

PROJECT=gphoto2

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "You must have autoconf installed to compile $PROJECT."
    echo "Download the appropriate package for your distribution,"
    echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
    DIE=1
}

(libtool --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "You must have libtool installed to compile $PROJECT."
    echo "Get ftp://ftp.gnu.org/pub/gnu/libtool-1.2.tar.gz"
    echo "(or a newer version if it is available)"
    DIE=1
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "You must have automake installed to compile $PROJECT."
    echo "Get ftp://ftp.gnu.org/pub/gnu/automake-1.3.tar.gz"
    echo "(or a newer version if it is available)"
    DIE=1
}

(gettextize --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "You must have gettext installed to compile $PROJECT."
    DIE=1
}

if test "$DIE" -eq 1; then
    exit 1
fi

test -f libgphoto2/gphoto2.h || {
        echo "You must run this script in the top-level gphoto2 directory"
        exit 1
}

case "$CC" in
*xlc | *xlc\ * | *lcc | *lcc\ *) am_opt=--include-deps;;
esac

ACLOCAL_FLAGS="-I ${srcdir}/libgphoto2_port/m4 ${ACLOCAL_FLAGS}"

gettext_version=`gettextize --version 2>&1 | sed -n 's/^.*GNU gettext.* \([0-9]*\.[0-9.]*\).*$/\1/p'`
case $gettext_version in
0.11.*)
	gettext_opt="$gettext_opt --intl";;
esac

for dir in libgphoto2_port .
do 
   (echo processing "$dir";					\
    cd "$dir";							\
    echo "Running gettextize --force --copy $gettext_opt";	\
    gettextize --force --copy $gettext_opt;			\
    test -f po/Makevars.template &&				\
    cp po/Makevars.template po/Makevars;			\
    echo "Running aclocal $ACLOCAL_FLAGS";			\
    aclocal $ACLOCAL_FLAGS;					\
    echo "Running autoheader";					\
    autoheader;							\
    echo "Running automake --add-missing --gnu $am_opt";	\
    automake --add-missing --gnu $am_opt;			\
    echo "Running autoconf";					\
    autoconf;							\
   )
done

echo 
echo "$PROJECT is now ready for configuration."
