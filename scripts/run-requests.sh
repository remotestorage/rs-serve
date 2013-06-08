#!/bin/bash

## run various requests against running test server.
## this script is pretty useless on it's own, but used by leakcheck.sh/limitcheck.sh

get() {
  echo -n "GET $1"
  curl -H "Authorization: Bearer static-token-for-now" $1 >/dev/null 2>&1
  echo " -> $?"
}

put() {
  echo -n "PUT $1"
  curl -X PUT -H "Authorization: Bearer static-token-for-now" $1 --data "$2" >/dev/null 2>&1
  echo " -> $?"
}

delete() {
  echo -n "DELETE $1"
  curl -X DELETE -H "Authorization: Bearer static-token-for-now" $1 --data "$2" >/dev/null 2>&1
  echo " -> $?"
}

# retrieve some directories
get http://localhost:8181/nil/src/rs-serve/ 
get http://localhost:8181/nil/src/rs-serve/src/
get http://localhost:8181/nil/src/rs-serve/scripts/
# retrieve some files
get http://localhost:8181/nil/src/rs-serve/Makefile
get http://localhost:8181/nil/src/rs-serve/rs-serve
get http://localhost:8181/nil/src/rs-serve/LICENSE
get http://localhost:8181/nil/src/rs-serve/src/main.c
get http://localhost:8181/nil/src/rs-serve/src/config.h
get http://localhost:8181/nil/src/rs-serve/src/rs-serve.h
get http://localhost:8181/nil/src/rs-serve/src/storage.c
# create some stuff
put http://localhost:8181/nil/src/rs-serve/foo/bar/a "File A"
put http://localhost:8181/nil/src/rs-serve/foo/bar/b "File B"
put http://localhost:8181/nil/src/rs-serve/foo/bar/c "File C"
put http://localhost:8181/nil/src/rs-serve/foo/bar/baz/d "File D"
put http://localhost:8181/nil/src/rs-serve/foo/bar/baz/e "File E"
put http://localhost:8181/nil/src/rs-serve/foo/bar/baz/f "File F"
# retrieve just created files & folders
get http://localhost:8181/nil/src/rs-serve/foo/
get http://localhost:8181/nil/src/rs-serve/foo/bar/
get http://localhost:8181/nil/src/rs-serve/foo/bar/a
get http://localhost:8181/nil/src/rs-serve/foo/bar/b
get http://localhost:8181/nil/src/rs-serve/foo/bar/c
get http://localhost:8181/nil/src/rs-serve/foo/bar/baz/
get http://localhost:8181/nil/src/rs-serve/foo/bar/baz/d
get http://localhost:8181/nil/src/rs-serve/foo/bar/baz/e
get http://localhost:8181/nil/src/rs-serve/foo/bar/baz/f
# delete some files
delete http://localhost:8181/nil/src/rs-serve/foo/bar/a
delete http://localhost:8181/nil/src/rs-serve/foo/bar/b
delete http://localhost:8181/nil/src/rs-serve/foo/bar/c
# attempt to get deleted files & dir
get http://localhost:8181/nil/src/rs-serve/foo/bar/
get http://localhost:8181/nil/src/rs-serve/foo/bar/a
get http://localhost:8181/nil/src/rs-serve/foo/bar/b
get http://localhost:8181/nil/src/rs-serve/foo/bar/c
# delete the rest
delete http://localhost:8181/nil/src/rs-serve/foo/bar/baz/d
delete http://localhost:8181/nil/src/rs-serve/foo/bar/baz/e
delete http://localhost:8181/nil/src/rs-serve/foo/bar/baz/f

