#!/bin/bash

# Apple Silicon (must be run on Mac OS)

set -e
cd $(dirname "${BASH_SOURCE[0]}")
wd=$(pwd)

platform="darwin/arm64"
name=pogocache-apple-arm64

echo "Building $name..."

cd ../..
make clean distclean
make
cd $wd

mv ../../pogocache .
