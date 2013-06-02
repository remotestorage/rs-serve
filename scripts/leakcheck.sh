#!/bin/bash

valgrind --leak-check=full --show-reachable=yes --log-file=leakcheck.log ./rs-serve -p 8181 &

sleep 2

. scripts/run-requests.sh

# done.
kill %1

sleep 2

[ "$LEAKCHECK_SILENT" != "1" ] && less leakcheck.log

exit 0
