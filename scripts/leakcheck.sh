#!/bin/bash

get() {
  curl -H "Authorization: Bearer static-token-for-now" $1 >/dev/null 2>&1
}

put() {
  curl -X PUT -H "Authorization: Bearer static-token-for-now" $1 --data "$2" >/dev/null 2>&1
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
# create some stuff
put http://localhost:8181/storage/foo/bar/a "File A"
put http://localhost:8181/storage/foo/bar/b "File B"
put http://localhost:8181/storage/foo/bar/c "File C"
put http://localhost:8181/storage/foo/bar/baz/d "File D"
put http://localhost:8181/storage/foo/bar/baz/e "File E"
put http://localhost:8181/storage/foo/bar/baz/f "File F"
# retrieve just created files & folders
get http://localhost:8181/storage/foo/
get http://localhost:8181/storage/foo/bar/
get http://localhost:8181/storage/foo/bar/a
get http://localhost:8181/storage/foo/bar/b
get http://localhost:8181/storage/foo/bar/z
get http://localhost:8181/storage/foo/bar/baz/
get http://localhost:8181/storage/foo/bar/baz/d
get http://localhost:8181/storage/foo/bar/baz/e
get http://localhost:8181/storage/foo/bar/baz/f

# done.
kill %1

sleep 2

# cleanup

rm -r foo

less leakcheck.log
