
#include "rs-serve.h"

#define RS_ADDRESS NULL
#define RS_PORT 8181
#define RS_ALLOWED_METHODS EVHTTP_REQ_OPTIONS | EVHTTP_REQ_HEAD | EVHTTP_REQ_GET |\
  EVHTTP_REQ_PUT | EVHTTP_REQ_DELETE


static void fatal_error_callback(int err) {
  fprintf(stderr, "A fatal error occured (code: %d)\nExiting.\n", err);
  exit(EXIT_FAILURE);
}

static void handle_request_callback(struct evhttp_request *request, void *ctx) {
  switch(evhttp_request_get_command(request)) {
  case EVHTTP_REQ_OPTIONS:
    storage_options(request);
    break;
  case EVHTTP_REQ_GET:
    storage_get(request);
    break;
  case EVHTTP_REQ_PUT:
    storage_put(request);
    break;
  case EVHTTP_REQ_DELETE:
    storage_delete(request);
    break;
  case EVHTTP_REQ_HEAD:
    storage_head(request);
    break;
  }

  log_request(request);
}

int main(int argc, char **argv) {
  event_set_fatal_callback(fatal_error_callback);
  event_enable_debug_mode();

  struct event_base *base = event_base_new();

  if(! base) {
    perror("Failed to create event base");
    return 1;
  }

  struct evhttp *server = evhttp_new(base);

  if(! server) {
    perror("Failed to create server");
    return 2;
  }

  evhttp_bind_socket(server, RS_ADDRESS, RS_PORT);
  evhttp_set_allowed_methods(server, RS_ALLOWED_METHODS);
  evhttp_set_gencb(server, handle_request_callback, NULL);

  log_starting(RS_ADDRESS, RS_PORT);

  return event_base_dispatch(base);
}
