#!/bin/sh

vers=5.3.0

set -e
cd "$(dirname "$0")"

if [ ! -d "jemalloc" ]; then
    rm -rf jemalloc/
    git clone --depth 1 --branch $vers \
        https://github.com/jemalloc/jemalloc.git
fi

cd jemalloc

if [ ! -f "build.ready" ]; then
    ./autogen.sh
    make -j10
    touch build.ready
fi
