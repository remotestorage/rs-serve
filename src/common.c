
#include "rs-serve.h"

char *time_now() {
  static char timestamp[100]; // FIXME: not threadsafe!
  time_t t = time(NULL);
  struct tm *tmp = localtime(&t);
  strftime(timestamp, 99, "%F %T %z", tmp);
  return timestamp;
}

void log_starting(const char *address, int port) {
  if(! address) {
    address = "0.0.0.0";
  }
  printf("[%s] Server listening on %s:%d\n", time_now(), address, port);
}

void log_request(struct evhttp_request *request) {
  char *method;
  switch(evhttp_request_get_command(request)) {
  case EVHTTP_REQ_GET: method = "GET"; break;
  case EVHTTP_REQ_POST: method = "POST"; break;
  case EVHTTP_REQ_HEAD: method = "HEAD"; break;
  case EVHTTP_REQ_PUT: method = "PUT"; break;
  case EVHTTP_REQ_OPTIONS: method = "OPTIONS"; break;
  case EVHTTP_REQ_DELETE: method = "DELETE"; break;
  default: method = "(unknown)"; break;
  }
  
  printf("[%s] %s %s -> %d\n", time_now(), method,
         evhttp_request_get_uri(request),
         evhttp_request_get_response_code(request));
} 
