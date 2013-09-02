(function(global) {
  /**
   * Class: eventhandling
   */
  var methods = {
    /**
     * Method: addEventListener
     *
     * Install an event handler for the given event name.
     */
    addEventListener: function(eventName, handler) {
      this._validateEvent(eventName);
      this._handlers[eventName].push(handler);
    },

    /**
     * Method: removeEventListener
     *
     * Remove a previously installed event handler
     */
    removeEventListener: function(eventName, handler) {
      this._validateEvent(eventName);
      var hl = this._handlers[eventName].length;
      for(var i=0;i<hl;i++) {
        if(this._handlers[eventName][i] === handler) {
          this._handlers[eventName].splice(i, 1);
          return;
        }
      }
    },

    _emit: function(eventName) {
      this._validateEvent(eventName);
      var args = Array.prototype.slice.call(arguments, 1);
      this._handlers[eventName].forEach(function(handler) {
        handler.apply(this, args);
      });
    },

    _validateEvent: function(eventName) {
      if(! (eventName in this._handlers)) {
        throw new Error("Unknown event: " + eventName);
      }
    },

    _delegateEvent: function(eventName, target) {
      target.on(eventName, function(event) {
        this._emit(eventName, event);
      }.bind(this));
    },

    _addEvent: function(eventName) {
      this._handlers[eventName] = [];
    }
  };

  // Method: eventhandling.on
  // Alias for <addEventListener>
  methods.on = methods.addEventListener;

  /**
   * Function: eventHandling
   *
   * Mixes event handling functionality into an object.
   *
   * The first parameter is always the object to be extended.
   * All remaining parameter are expected to be strings, interpreted as valid event
   * names.
   *
   * Example:
   *   (start code)
   *   var MyConstructor = function() {
   *     eventHandling(this, 'connected', 'disconnected');
   *
   *     this._emit('connected');
   *     this._emit('disconnected');
   *     // this would throw an exception:
   *     //this._emit('something-else');
   *   };
   *
   *   var myObject = new MyConstructor();
   *   myObject.on('connected', function() { console.log('connected'); });
   *   myObject.on('disconnected', function() { console.log('disconnected'); });
   *   // this would throw an exception as well:
   *   //myObject.on('something-else', function() {});
   *
   *   (end code)
   */
  global.eventHandling = function(object) {
    var eventNames = Array.prototype.slice.call(arguments, 1);
    for(var key in methods) {
      object[key] = methods[key];
    }
    object._handlers = {};
    eventNames.forEach(function(eventName) {
      object._addEvent(eventName);
    });
  };
})(this);

function parseJsonCb(nextCallback) {
  return function(err, xhr) {
    if(err) {
      nextCallback(err);
    } else {
      nextCallback(null, JSON.parse(xhr.responseText));
    }
  };
}

var Session = function(baseUrl) {
  eventHandling(this, 'authenticated', 'authentication-failed', 'not-authenticated');

  this.baseUrl = baseUrl;

  var savedData = localStorage['rs-serve-auth-session'];
  if(savedData) {
    savedData = JSON.parse(savedData);
    this.sessionToken = savedData.sessionToken;
    this.username = savedData.username
  }

  setTimeout(function() {
    console.log('session token ? ', this.sessionToken);
    if(this.sessionToken) {
      this._emit('authenticated');
    } else {
      console.log('not authenticated');
      this._emit('not-authenticated');
    }
  }.bind(this));
};

var SUCCESS_CODES = {
  200: true, 201: true, 204: true, 304: true
};

Session.prototype = {

  listAuthorizations: function(callback) {
    callback = callback.bind(this);
    this._sessionRequest('GET', '/authorizations', {}, parseJsonCb(callback));
  },

  createAuthorization: function(scopes, callback) {
    callback = callback.bind(this);
    this._sessionRequest('POST', '/authorizations', {
      body: JSON.stringify({ scopes: scopes }),
      headers: {
        'Content-Type': 'application/json; charset=UTF-8'
      }
    }, parseJsonCb(callback));
  },

  authenticate: function(username, password) {
    this.username = username;
    this._authenticate(username, password, function(error, result) {
      if(result && result.success) {
        this.sessionToken = result.session_token;
        localStorage['rs-serve-auth-session'] = JSON.stringify({
          username: username,
          sessionToken: this.sessionToken
        });
        this._emit('authenticated');
      } else {
        this._emit('authentication-failed', error);
      }
    });
  },

  logout: function() {
    this._sessionRequest('DELETE', '/authenticate', {}, function(error, xhr) {
      if(error) {
        console.error("Logout failed: ", error);
      } else {
        delete this.username;
        delete this.sessionToken;
        this._emit('not-authenticated');
      }
    });
  },

  _authenticate: function(username, password, callback) {
    callback = callback.bind(this);
    this._request('GET', '/authenticate', {}, function(error, xhr) {
      if(error) { callback(error); return; }
      this._request(
        'POST', '/authenticate?login_token=' +
          encodeURIComponent(JSON.parse(xhr.responseText).login_token),
        {
          body: JSON.stringify({
            username: username,
            password: password
          }),
          headers: {
            'Content-Type': 'application/json; charset=UTF-8'
          }
        }, parseJsonCb(callback)
      );
    });
  },

  _sessionRequest: function(method, path, options, callback) {
    this._request(method, path + '?session_token=' + encodeURIComponent(this.sessionToken), options, callback);
  },

  _request: function(method, path, options, callback) {
    callback = callback.bind(this);

    var xhr = new XMLHttpRequest();
    xhr.open(method, this.baseUrl + path, true);
    if(options.responseType) {
      xhr.responseType = options.responseType;
    }
    if(options.headers) {
      for(var key in options.headers) {
        xhr.setRequestHeader(key, options.headers[key]);
      }
    }
    xhr.onload = function() {
      if(SUCCESS_CODES[xhr.status]) {
        callback(null, xhr);
      } else {
        callback("Request failed with status " + xhr.status + " (" + xhr.responseText + ")");
      }
    };
    xhr.onerror = function(error) {
      callback(error);
    };

    xhr.send(options.body);
  }

};
