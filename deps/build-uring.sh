#!/bin/sh

vers=2.12

set -e
cd "$(dirname "$0")"

if [ ! -d "liburing" ]; then
    rm -rf liburing/
    git clone --depth 1 --branch liburing-$vers \
        https://github.com/axboe/liburing.git
fi

cd liburing

if [ ! -f "build.ready" ]; then
    make -j10
    touch build.ready
fi
