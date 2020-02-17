#!/bin/sh

buildid="${1:?"need buildid"}"

date
ls -l configure

shift

properdir="$(pwd)"

mkdir "_build-${buildid}"

set -e
set -x

cd "_build-${buildid}"

if ../configure --prefix="$(cd ".." && pwd)/_root-${buildid}" "$@"
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

cd "$properdir"

find "_root-${buildid}" -type f | sort

# (cd "_build-${buildid}" && make uninstall clean)
