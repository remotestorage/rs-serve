
#ifndef RS_SERVE_H
#define RS_SERVE_H

// standard headers
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

// libevent headers
#include <event2/event.h>
#include <event2/http.h>

/* COMMON */

void log_starting(const char *address, int port);
void log_request(struct evhttp_request *request);

/* STORAGE */

void storage_options(struct evhttp_request *request);
void storage_get(struct evhttp_request *request);
void storage_put(struct evhttp_request *request);
void storage_delete(struct evhttp_request *request);
void storage_head(struct evhttp_request *request);

#endif
