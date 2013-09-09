
var http = require('http');

var app = require('./app');

var port = 8888;

http.createServer(app).listen(port);
console.log("Auth server running on http://0.0.0.0:" + port + "/");