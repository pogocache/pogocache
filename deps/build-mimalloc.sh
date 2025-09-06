#!/bin/sh

vers=3.1.5

set -e
cd "$(dirname "$0")"

if [ ! -d "mimalloc" ]; then
    rm -rf mimalloc/
    git clone --depth 1 --branch v$vers \
        https://github.com/microsoft/mimalloc.git
fi

cd mimalloc

if [ ! -f "build.ready" ]; then
    mkdir -p out/release
    cd out/release
    cmake ../..
    make -j10
    cd ../..
    touch build.ready
fi
