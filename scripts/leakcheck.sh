#!/bin/bash

get() {
  curl -H "Authorization: Bearer static-token-for-now" $1 >/dev/null 2>&1
}

valgrind --leak-check=full --show-reachable=yes --log-file=leakcheck.log ./rs-serve -p 8181 &

sleep 2

# webfinger requests
get http://localhost:8181/.well-known/webfinger
get http://localhost:8181/.well-known/webfinger?resource=acct%3Ame@local.dev
# authorization request
get http://localhost:8181/auth?redirect_uri=http%3A%2F%2Flocalhost%3A3000%2Fsome%2Fapp
# retrieve some directories
get http://localhost:8181/storage/ 
get http://localhost:8181/storage/src/
get http://localhost:8181/storage/scripts/
# retrieve some files
get http://localhost:8181/storage/Makefile
get http://localhost:8181/storage/rs-serve
get http://localhost:8181/storage/LICENSE
get http://localhost:8181/storage/src/main.c
get http://localhost:8181/storage/src/config.h
get http://localhost:8181/storage/src/rs-serve.h
get http://localhost:8181/storage/src/storage.c

# done.
kill %1

sleep 2

less leakcheck.log
