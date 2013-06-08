#!/bin/bash

sudo valgrind --trace-children=yes --leak-check=full --show-reachable=yes --log-file=leakcheck.log ./rs-serve -p 8181 &

sleep 2

. scripts/run-requests.sh

# echo -n "Waiting for session(s) to expire (needs RS_SESSION_MAX_AGE=10 to work)..."
# sleep 12
# echo "!"

# done.
kill %1

sleep 2

[ "$LEAKCHECK_SILENT" != "1" ] && less leakcheck.log

exit 0
