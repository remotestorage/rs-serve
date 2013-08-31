
window.onload = function() {
  state = new OAuthState(document.location.search.substr(1));
  session = new Session('http://localhost:8888');
  ui = new UI(session, state);
};
