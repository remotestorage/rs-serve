
var OAuthState = function(fragment) {
  fragment.split('&').forEach(function(s) {
    var kv = s.split('=');
    var key = decodeURIComponent(kv[0]);
    if(key in OAuthState.ALLOWED_KEYS) {
      this[key] = decodeURIComponent(kv[1]);
    }
  }.bind(this));

  if(this.scope) {
    this.scope = this._parseScope(this.scope);
  }
};

OAuthState.prototype = {

  createRedirectUri: function(token) {
    return this.redirect_uri + '#access_token=' + encodeURIComponent(token);
  },

  _parseScope: function(scope) {
    return scope.split(' ').reduce(function(m, s) {
      var kv = s.split(':');
      m[kv[0]] = kv[1];
      return m;
    }, {});
  }
};

OAuthState.ALLOWED_KEYS = {
  client_id: true,
  redirect_uri: true,
  scope: true,
  state: true
};
