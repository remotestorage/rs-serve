
#include "rs-serve.h"


void storage_options(struct evhttp_request *request) {
  evhttp_send_reply(request, HTTP_OK, NULL, NULL);
}

void storage_get(struct evhttp_request *request) {
  evhttp_send_reply(request, HTTP_OK, NULL, NULL);
}

void storage_put(struct evhttp_request *request) {
  evhttp_send_reply(request, HTTP_OK, NULL, NULL);
}

void storage_delete(struct evhttp_request *request) {
  evhttp_send_reply(request, HTTP_OK, NULL, NULL);
}

void storage_head(struct evhttp_request *request) {
  evhttp_send_reply(request, HTTP_OK, NULL, NULL);
}
