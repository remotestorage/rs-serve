
#include "rs-serve.h"

void fatal_error_callback(int err) {
  fprintf(stderr, "A fatal error occured (code: %d)\nExiting.\n", err);
  exit(EXIT_FAILURE);
}

static void handle_storage_request(struct evhttp_request *request) {
  switch(evhttp_request_get_command(request)) {
  case EVHTTP_REQ_OPTIONS:
    storage_options(request);
    break;
  case EVHTTP_REQ_GET:
    storage_get(request, 1);
    break;
  case EVHTTP_REQ_PUT:
    storage_put(request);
    break;
  case EVHTTP_REQ_DELETE:
    storage_delete(request);
    break;
  case EVHTTP_REQ_HEAD:
    storage_get(request, 0);
    break;
  }
}

static void handle_auth_request(struct evhttp_request *request) {
  switch(evhttp_request_get_command(request)) {
  case EVHTTP_REQ_GET: // request authorization or list current authorizations
    auth_get(request);
    break;
  case EVHTTP_REQ_PUT: // confirm authorization
    auth_put(request);
    break;
  case EVHTTP_REQ_DELETE: // delete existing authorization
    auth_delete(request);
    break;
  default:
    evhttp_send_error(request, HTTP_BADMETHOD, NULL);
  }
}

static void handle_webfinger_request(struct evhttp_request *request) {
  evhttp_send_reply(request, HTTP_OK, NULL, NULL);
}

static void handle_bad_request(struct evhttp_request *request) {
  evhttp_send_error(request, HTTP_BADREQUEST, NULL);
}

void handle_request_callback(struct evhttp_request *request, void *ctx) {

  const char *uri = evhttp_request_get_uri(request);

  if(strncmp(uri, RS_STORAGE_PATH, RS_STORAGE_PATH_LEN) == 0) {
    handle_storage_request(request);
  } else if(strncmp(uri, RS_AUTH_PATH, RS_AUTH_PATH_LEN) == 0) {
    handle_auth_request(request);
  } else if(strncmp(uri, RS_WEBFINGER_PATH, RS_WEBFINGER_PATH_LEN) == 0) {
    handle_webfinger_request(request);
  } else {
    handle_bad_request(request);
  }

  log_request(request);
}
