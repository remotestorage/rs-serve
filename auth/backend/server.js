
var http = require('http');

var app = require('./app');

http.createServer(app).listen(8888);
