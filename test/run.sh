#!/bin/bash

mkdir -p tmp/

./rs-serve --hostname test.host --port 8123 > tmp/test-server.log 2>&1 &
sleep 1

GET() {
  curl http://localhost:8123$1 -H "Authorization: Bearer static-token-for-now" > tmp/test-response 2>/dev/null
}

PUT() {
  curl http://localhost:8123$1 -X PUT -d "$2" -H "Authorization: Bearer static-token-for-now" >/dev/null 2>&1
}

assert_response() {
  echo -n $2 > tmp/expected-response
  diff tmp/{test-response,expected-response} > /dev/null
  if [ "$?" == "0" ] ; then
      echo "  $1 - OK"
  else
      echo "  $1 - FAIL"
  fi
}

for test in $(ls test/tests/*.sh) ; do
  name=$(echo $test | cut -d / -f 3 | sed 's/\.sh$//')
  echo "Test: $name"
  . $test
done

kill %1
