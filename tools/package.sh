#!/bin/bash

set -e
cd $(dirname "${BASH_SOURCE[0]}")

build/run.sh linux-aarch64
build/run.sh linux-arm64
build/run.sh linux-x86_64
build/run.sh linux-amd64
if [[ "$(uname)" == "Darwin" && "$(uname -m)" == "arm64" ]]; then
    build/run.sh apple-arm64
fi
