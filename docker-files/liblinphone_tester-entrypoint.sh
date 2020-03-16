#!/bin/sh
set -e
/wait
# first arg is `-f` or `--some-option`
if [ "${1#-}" != "$1" ]; then
	set -- ../bin/liblinphone_tester "$@"
fi

exec "$@"
