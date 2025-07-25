#!/bin/bash


# ./run.sh [<test-name>]

set -e
cd $(dirname "${BASH_SOURCE[0]}")

# OK=0
finish() { 
    # rm -fr *.o
    # rm -fr *.out
    # rm -fr *.test
    # rm -fr *.profraw
    # rm -fr *.dSYM
    # rm -fr *.profdata
    # rm -fr *.wasm
    # rm -fr *.js
    pkill -9 pogocache || true
    # if [[ "$OK" != "1" ]]; then
        # echo "FAIL"
    # fi
}
trap finish EXIT

# Kill any running Pogocache
pkill -9 pogocache || true

# Build pogocache with sanitizers
buildok=0
if [[ -f ../pogocache ]]; then
    if [[ "$(../pogocache --version)" == *CCSANI* ]]; then
        buildok=1
    fi
fi
if [[ $buildok == "0" ]]; then
    make -C .. clean
fi
CCSANI=1 make -C ..

# Run Pogocache
../pogocache --shards=128 --cas=yes &
sleep 0.1


run=$@
if [[ "$run" == "" ]]; then
    run='.'
fi

go test -v -run $run



# OK=1
# echo "PASSED"
