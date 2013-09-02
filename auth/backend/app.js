
var express = require('express');
var unixlib = require('./deps/unixlib/build/Release/unixlib');
var crypto = require('crypto');

var config = require('./config');

var auth = require('../bindings/build/Release/rs_serve_auth');

var app = express();

module.exports = app;

var STATE = {
  loginTokens: {},
  sessions: {}
};

function generateToken(bytes, callback) {
  crypto.randomBytes(bytes, function(err, buf) {
    if(err) {
      console.error(err);
    } else {
      callback(buf.toString('base64'));
    }
  });
}

function generateLoginToken(callback) {
  generateToken(32, function(token) {
    STATE.loginTokens[token] = true;
    callback(token);
    setTimeout(function() {
      delete STATE.loginTokens[token];
    }, config.loginTokenTimeout);
  });
}

function verifyLoginToken(req, res, next) {
  console.log("WARNING: login token verification disabled for now. There may be a bug.");
  next();
  return;

  if(STATE.loginTokens[req.query.login_token]) {
    delete STATE.loginTokens[req.query.login_token];
    next();
  } else {
    // token not given, invalid or expired.
    res.send(401, "ERROR: couldn't verify login_token!");
  }
}

function generateSessionToken(username, callback) {
  generateToken(64, function(token) {
    STATE.sessions[token] = {
      username: username,
      token: token
    };
    callback(token);
  });
}

function verifySessionToken(req, res, next) {
  var session = STATE.sessions[req.query.session_token];
  if(session) {
    req.session = session;
    next();
  } else {
    res.send(401, "ERROR: couldn't verify session_token");
  }
}

// LOGGING
app.use(function(req, res, next){
  console.log('%s %s', req.method, req.url);
  next();
});

// CORS
app.use(function(req, res, next) {
  res.set('Access-Control-Allow-Origin', '*');
  res.set('Access-Control-Allow-Methods', 'GET, PUT, DELETE');
  res.set('Access-Control-Allow-Headers', 'Content-Type, Origin');
  next();
});

app.use(express.bodyParser());

app.use(express.static('./auth/frontend'));

app.get('/authenticate', function(req, res) {
  generateLoginToken(function(token) {
    res.json({
      login_token: token // temporary token, expires after a while.
    });
  });
});

app.post('/authenticate', verifyLoginToken, function(req, res) {
  unixlib.pamauth('remotestorage', req.body.username, req.body.password, function(result) {
    if(result) {
      generateSessionToken(req.body.username, function(key) {
        res.json({ success: true, session_token: key });
      });
    } else {
      res.json({ success: false });
    }
  });
});

app.delete('/authenticate', verifySessionToken, function(req, res) {
  delete STATE.sessions[req.session.token];
  delete req.session;
  res.send(204);
});

app.get('/authorizations', verifySessionToken, function(req, res) {
  var list = auth.list(req.session.username);
  res.json(list);
});

app.post('/authorizations', verifySessionToken, function(req, res) {
  generateToken(64, function(token) {
    auth.add(req.session.username, token, req.body.scopes);
    res.json({ token: token });
  });
});
