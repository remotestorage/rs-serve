
PUT /storage/foo "bar"
GET /storage/foo
assert_response "GET /foo after PUT" "bar"
PUT /storage/a/b/c "abc"
GET /storage/a/b/c
assert_response "GET /a/b/c after PUT" "abc"

rm -f foo
rm -rf a/
