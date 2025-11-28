#!/bin/bash

set -e
cd $(dirname "${BASH_SOURCE[0]}")
wd=$(pwd)

if [[ "$1" == "linux-arm64-musl" ]]; then
    platform="linux/arm64"
    libc="musl"
elif [[ "$1" == "linux-arm64" ]]; then
    platform="linux/arm64"
    libc="glibc"
elif [[ "$1" == "linux-amd64-musl" ]]; then
    platform="linux/amd64"
    libc="musl"
elif [[ "$1" == "linux-amd64" ]]; then
    platform="linux/amd64"
    libc="glibc"
elif [[ "$1" == "apple-arm64" ]]; then
    native=1
else
    echo "Invalid target '$1'"
    echo "Usage: $0 <arch>"
    echo ""
    echo "Valid architectures:"
    echo "   linux-arm64-musl -- musl"
    echo "   linux-amd64-musl -- musl"
    echo "   linux-arm64      -- glibc" 
    echo "   linux-amd64      -- glibc"
    echo "   apple-arm64      -- Apple Silicon"
    echo ""
    exit 1
fi

name=pogocache-$1
btag=build-$name

cleanup() {
    cd $wd
    rm -rf $name ../repo.tar.gz ./repo.tar.gz ./pogocache ./native-repo
}

finish() { 
    cleanup
    if [[ "$ok" == "1" ]]; then
        echo "SUCCESS"
    else
        echo "FAIL"
    fi
}
trap finish EXIT

echo "Building $name..."
cleanup
cd ../..
make distclean clean
make gitinfo
tar -czf repo.tar.gz deps/ src/
cd $wd
mv ../../repo.tar.gz .
reposha=$(tar -O -xf repo.tar.gz | sha1sum | awk '{print $1;}')

build=1
if [[ -f "../../packages/$name.tar.gz" ]]; then
    tar -xzf ../../packages/$name.tar.gz
    reposha2=$(cat $name/reposha)
    if [[ "$reposha2" == "$reposha" ]]; then
        build=0
    fi
fi
rm -rf $name
if [[ "$build" == "1" && "$native" == 1 ]]; then
    # Build on host machine
    rm -rf native-repo
    mkdir -p native-repo
    mv repo.tar.gz native-repo
    cd native-repo
    tar -xzf repo.tar.gz
    cd src && NOGITINFOGEN=1 make all
    cd ../..
    mv native-repo/pogocache .
    rm -rf native-repo
elif [[ "$build" == "1" ]]; then
    # Build from Dockerfile
    docker build --platform $platform -f Dockerfile.$libc . --tag=$btag
    docker rm -fv tempctr
    docker create --name tempctr $btag
    docker cp tempctr:/pogocache/pogocache ./pogocache 
    docker rm -fv tempctr
    docker rmi -f $btag
fi
if [[ "$build" == "1" ]]; then
    # Package
    mkdir $name
    # Copy final package files
    cp -f ./pogocache $name                                  # /pogocache
    echo $reposha > $name/reposha                            # /reposha
    cp -f ../../LICENSE $name                                # /LICENSE
    tar -czf $name.tar.gz $name/*
    # Move final package to "packages" directory
    mkdir -p ../../packages
    mv $name.tar.gz ../../packages
    rm -rf $name.tar.gz
fi

ok=1
