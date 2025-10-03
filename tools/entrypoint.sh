#!/bin/sh
set -e

if [ "${1#-}" != "$1" ] || [ "${1%.conf}" != "$1" ]; then
	set -- pogocache "$@"
fi

exec "$@" -h 0.0.0.0 $POGOCACHE_EXTRA_FLAGS
