
#include "rs-serve.h"

static int match_scope(struct rs_scope *scope, evhtp_request_t *req) {
  const char *file_path = req->uri->path->match_end;
  log_debug("checking scope, name: %s, write: %d", scope->name, scope->write);
  // check path
  if( (strcmp(scope->name, "") == 0) || // root scope
      (strncmp(file_path + 1, scope->name, scope->len) == 0) ) { // other scope
    log_debug("path authorized");
    // check mode
    if(scope->write ||
       req->method == htp_method_GET ||
       req->method == htp_method_HEAD) {
       log_debug("mode authorized");
      return 0;
    }
  }
  return -1;
}

int authorize_request(evhtp_request_t *req) {
  char *username = req->uri->path->match_start;
  const char *auth_header = evhtp_header_find(req->headers_in, "Authorization");
  log_debug("Got auth header: %s", auth_header);
  const char *token;
  if(auth_header) {
    if(strncmp(auth_header, "Bearer ", 7) == 0) {
      token = auth_header + 7;
      log_debug("Got token: %s", token);
      struct rs_authorization *auth = lookup_authorization(username, token);
      if(auth != NULL) {
        log_debug("Got authorization (%p, scopes: %p)", auth, auth->scopes);
        struct rs_scope *scope;
        for(scope = auth->scopes; scope != NULL; scope = scope->next) {
          log_debug("Compare scope %s", scope->name);
          if(match_scope(scope, req) == 0) {
            return 0;
          }
        }
      }
    }
  }
  // TODO: handle /public/ requests.
  return -1;
}
