#!/bin/sh
# Run this to generate all the initial makefiles, etc.
# This was lifted from the Gimp, and adapted slightly by
# Raph Levien.
# Since then, it has been rewritten quite a lot by misc. people.

# Call this file with AUTOCONF_SUFFIX and AUTOMAKE_SUFFIX set
# if you want us to call a specific version of autoconf or automake. 
# E.g. if you want us to call automake-1.6 instead of automake (which
# seems to be quite advisable if your automake is not already version 
# 1.6) then call this file with AUTOMAKE_SUFFIX set to "-1.6".

DIE=0
srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.
test "$srcdir" = "." && srcdir=`pwd`

PROJECT=gphoto2

(autoconf${AUTOCONF_SUFFIX} --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "You must have autoconf installed to compile $PROJECT."
    echo "Download the appropriate package for your distribution,"
    echo "or get the source tarball at ftp://ftp.gnu.org/gnu/autoconf/"
    DIE=1
}

(libtool --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "You must have libtool installed to compile $PROJECT."
    echo "Download the appropriate package for your distribution,"
    echo "or get the source tarball at ftp://ftp.gnu.org/gnu/libtool/"
    DIE=1
}

(automake${AUTOMAKE_SUFFIX} --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "You must have automake installed to compile $PROJECT."
    echo "Download the appropriate package for your distribution,"
    echo "or get the source tarball at ftp://ftp.gnu.org/gnu/automake/"
    DIE=1
}

(gettextize --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "You must have gettext installed to compile $PROJECT."
    echo "Download the appropriate package for your distribution,"
    echo "or get the source tarball at ftp://ftp.gnu.org/gnu/gettext/"
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
(
    echo processing "$dir"
    cd "$dir"
    # FIXME: perhaps we should delete stuff cleanly instead of
    #        overwriting with --force
    echo "Running gettextize --force --copy $gettext_opt"
    gettextize --force --copy $gettext_opt
    if test -f po/Makevars.template
    then
	cp po/Makevars.template po/Makevars
    fi
    echo "Running libtoolize"
    libtoolize --copy --force
    echo "Running aclocal${AUTOMAKE_SUFFIX} $ACLOCAL_FLAGS"
    aclocal${AUTOMAKE_SUFFIX} $ACLOCAL_FLAGS
    echo "Running autoheader${AUTOCONF_SUFFIX}"
    # FIXME: where does config.h.in come from? and libgphoto2/config.h.in?
    autoheader${AUTOCONF_SUFFIX}
    echo "Running automake${AUTOMAKE_SUFFIX} --add-missing --gnu $am_opt"
    automake${AUTOMAKE_SUFFIX} --add-missing --gnu $am_opt
    echo "Running autoconf${AUTOCONF_SUFFIX}"
    autoconf${AUTOCONF_SUFFIX}
)
done

echo 
echo "$PROJECT is now ready for configuration."
