#!/bin/bash

## DOESN'T WORK!!!

## this script attempts to limit memory of rs-serve and see how it behaves.
## it doesn't work for me though. Setting $LIMIT < 5000 causes the shell process
## to run out of memory, but any higher number doesn't cause rs-serve to run out
## of memory.
## In either case, this probably only works anyway if /proc/sys/vm/overcommit_memory
## is adjusted, so requires root privileges.

LIMIT=$1
echo "Running with ulimit -v $LIMIT"
SERVER_PID=`bash <<EOF
  ulimit -v $LIMIT
  ./rs-serve --port 8181 >limitcheck-server.log 2>&1 &
  jobs -p
EOF`

if [ "$SERVER_PID" == "" ] ; then
    echo "Server not started!"
    exit 1
fi

echo "Server started as $SERVER_PID"
sleep 1

. scripts/run-requests.sh

kill $SERVER_PID
