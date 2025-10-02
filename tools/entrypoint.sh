#!/bin/sh
set -e

if [ "${1#-}" != "$1" ] || [ "${1%.conf}" != "$1" ]; then
	set -- pogocache "$@"
fi

exec "$@" $POGOCACHE_EXTRA_FLAGS
