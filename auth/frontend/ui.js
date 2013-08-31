
var UI = function(session, oauthstate) {
  ['_showLoginForm', '_displayLoginError', '_doLogin',
   '_loggedIn', '_logout', '_confirmAuth'].forEach(function(method) {
    this[method] = this[method].bind(this);
  }.bind(this));

  this.session = session; // a Session instance
  this.oauthstate = oauthstate; // a OAuthState instance
  this.session.on('not-authenticated', this._showLoginForm);
  this.session.on('authenticated', this._loggedIn);
  this.session.on('authentication-failed', this._displayLoginError);

  this.containers = {
    header: document.getElementById('header'),
    content: document.getElementById('content')
  };
};

UI.prototype = {
  _showLoginForm: function(params) {
    this.containers.header.innerHTML = '';
    this._render('content', 'login-form', params);
    this.loginForm = document.getElementById('login-form');
    this.loginForm.addEventListener('submit', this._doLogin);
    this.loginForm.username.focus();
  },

  _loggedIn: function() {
    this._closeLoginForm();
    this._render('header', 'header');
    document.getElementById('logout-link').addEventListener('click', this._logout);
    if(this.oauthstate.scope) {
      this._showAuthRequest();
    } else {
      this._listAuthorizations();
    }
  },

  _logout: function() {
    this.session.logout();
  },

  _closeLoginForm: function() {
    this.containers.content.innerHTML = '';
    delete this.loginForm;
  },

  _displayLoginError: function() {
    var username = this.loginForm.username.value;
    this._closeLoginForm();
    this._showLoginForm({
      username: username,
      error: "Login failed! Please try again."
    });
  },

  _doLogin: function(event) {
    event.preventDefault();
    this.loginForm.button.disabled = true;
    this.session.authenticate(this.loginForm.username.value,
                              this.loginForm.password.value);
  },

  _showAuthRequest: function() {
    var params = {
      origin: this.oauthstate.origin,
      scopes: []
    };
    for(var name in this.oauthstate.scope) {
      params.scopes.push({
        name: name,
        mode: this.oauthstate.scope[name]
      });
    }
    this._render('content', 'auth-request', params);
    document.getElementById('confirm-auth').addEventListener('click', this._confirmAuth);
  },

  _confirmAuth: function() {
    this.session.createAuthorization(this.oauthstate.scope, function(err, result) {
      if(err) {
        console.error("Failed to create authorization: ", err);
      } else {
        document.location = this.oauthstate.createRedirectUri(result.token);
      }
    }.bind(this));
  },

  _listAuthorizations: function() {
    this.session.listAuthorizations(function(err, list) {
      if(err) throw err;
      this._render('content', 'authorizations', {
        authorizations: list.map(function(auth) {
          return JSON.stringify(auth);
        })
      });
    }.bind(this));
  },

  _render: function(container, templateName, params) {
    var template = document.querySelector('script[data-template-name="' + templateName + '"]').innerHTML;
    this.containers[container].innerHTML = Mustache.render(template, params);
  }
};
