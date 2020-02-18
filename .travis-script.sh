#!/bin/sh
#
# Usage:
#   sh .travis-script.sh <BUILD_ID> [--args-to-configure...]
#

buildid="${1:?"need buildid"}"

date
ls -l configure

shift

set -e
set -x

abs_topsrcdir="$(pwd)"
rel_prefixdir="_root-${buildid}"
abs_prefixdir="${abs_topsrcdir}/${rel_prefixdir}"
rel_builddir="_build-${buildid}"

mkdir "${rel_builddir}"
cd    "${rel_builddir}"


if ../configure SLEEP=no --prefix="$abs_prefixdir" "$@"
then
    echo "Configure successful."
else
    s="$?"
    echo "### BEGIN LOG: config.log ###"
    cat config.log
    echo "### END LOG: config.log ###"
    exit "$?"
fi

make -j2

if make check
then
    echo "\"make check\" successful."
else
    s="$?"
    for log in libgphoto2_port/test/test-suite.log test/test-suite.log
    do
	echo "### BEGIN LOG: $log ###"
	cat "$log"
	echo "### END LOG: $log ###"
    done
    exit "$?"
fi

if test -f tests/print-libgphoto2-version
then
    tests/print-libgphoto2-version
fi

make install

examples/sample-afl

cd "$abs_topsrcdir"

find "${rel_prefixdir}" -type f | sort

# (cd "${rel_builddir}" && make uninstall clean)
