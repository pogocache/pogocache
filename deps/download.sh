#!/usr/bin/env bash

set -e
cd $(dirname "${BASH_SOURCE[0]}")

repo="$1"
name="$2"
vers="$3"
tag="$4"

if [[ ! -f $name-$vers.tar.gz ]]; then
    rm -rf dl.$name-$vers.tar.gz
    wget -O dl.$name-$vers.tar.gz $repo/archive/refs/tags/$tag.tar.gz
    mv dl.$name-$vers.tar.gz $name-$vers.tar.gz
fi
