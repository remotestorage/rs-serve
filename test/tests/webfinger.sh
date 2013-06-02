
GET /.well-known/webfinger
assert_response "host request" '{"links":[{"rel":"lrdd","template":"http://test.host:8123/.well-known/webfinger?resource={uri}"}]}'

GET /.well-known/webfinger?resource=acct:me@test.host
assert_response "resource request" '{"links":[{"rel":"remotestorage","href":"http://test.host:8123/storage","type":"draft-dejong-remotestorage-00","properties":{"auth-method":"http://tools.ietf.org/html/rfc6749#section-4.2","auth-endpoint":"http://test.host:8123/auth"}}]}'