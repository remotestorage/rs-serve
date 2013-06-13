/*
 * rs-serve - (c) 2013 Niklas E. Cathor
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "rs-serve.h"

#define REQ_BUF_SIZE 4096

static const char * method_strmap[] = {
    "GET",
    "HEAD",
    "POST",
    "PUT",
    "DELETE",
    "MKCOL",
    "COPY",
    "MOVE",
    "OPTIONS",
    "PROPFIND",
    "PROPATCH",
    "LOCK",
    "UNLOCK",
    "TRACE",
    "CONNECT",
    "PATCH",
};

// BEGIN HOOKS

static int hook_on_msg_begin(htparser *parser) {
  log_debug("HOOK: on_msg_begin");
  return 0;
}
static int hook_on_hdrs_begin(htparser *parser) {
  log_debug("HOOK: on_hdrs_begin");
  return 0;
}
static int hook_on_hdrs_complete(htparser *parser) {
  log_debug("HOOK: on_hdrs_complete");
  log_debug("Got headers:");
  struct rs_request *request = htparser_get_userdata(parser);
  struct rs_header *header;
  for(header = request->headers; header != NULL; header = header->next) {
    log_debug("  \"%s\" -> \"%s\"", header->key, header->value);
  }
  enum htp_method method = htparser_get_method(parser);
  int status;
  switch(method) {
  case htp_method_HEAD:
    status = storage_handle_head(request);
    break;
  case htp_method_GET:
    status = storage_handle_get(request);
    break;
  case htp_method_PUT:
    status = storage_begin_put(request);
    break;
  case htp_method_DELETE:
    status = storage_handle_delete(request);
    break;
  default:
    log_error("Unexpected HTTP verb: %s", method_strmap[method]);
    status = 405;
  }
  log_debug("Storage handler returned status: %d", status);
  // storage_handle_* methods only return a status if they fail.
  // otherwise they return zero and we can continue.
  if(status != 0) {
    send_error_response(request, status);
    return -1;
  } else {
    return 0;
  }
}
static int hook_on_new_chunk(htparser *parser) {
  log_debug("HOOK: on_new_chunk");
  return 0;
}
static int hook_on_chunk_complete(htparser *parser) {
  log_debug("HOOK: on_chunk_complete");
  return 0;
}
static int hook_on_chunks_complete(htparser *parser) {
  log_debug("HOOK: on_chunks_complete");
  return 0;
}
static int hook_on_msg_complete(htparser *parser) {
  log_debug("HOOK: on_msg_complete");
  struct rs_request *request = htparser_get_userdata(parser);
  enum htp_method method = htparser_get_method(parser);
  if(method == htp_method_PUT) {
    log_debug("ending PUT");
    int status = storage_end_put(request);
    log_debug("got storage_end_put status: %d", status);
    if(status != 0) {
      send_error_response(request, status);
      free_request(request);
      return -1;
    }
  }
  free_request(request);
  return 0;
}

static int hook_method(htparser *parser, const char *buf, size_t count) {
  log_debug("DATA HOOK: method, data: %s", buf);
  return 0;
}
static int hook_scheme(htparser *parser, const char *buf, size_t count) {
  log_debug("DATA HOOK: scheme, data: %s", buf);
  return 0;
}
static int hook_host(htparser *parser, const char *buf, size_t count) {
  log_debug("DATA HOOK: host, data: %s", buf);
  return 0;
}
static int hook_port(htparser *parser, const char *buf, size_t count) {
  log_debug("DATA HOOK: port, data: %s", buf);
  return 0;
}
static int hook_path(htparser *parser, const char *buf, size_t count) {
  log_debug("DATA HOOK: path, data: %s", buf);
  struct rs_request *req = htparser_get_userdata(parser);
  req->path = malloc(count + 1);
  if(req->path == NULL) {
    log_error("malloc() failed to allocate buffer for request path");
    return -1;
  }
  strncpy(req->path, buf, count);
  req->path[count] = 0;
  req->path_len = count;
  return 0;
}
static int hook_args(htparser *parser, const char *buf, size_t count) {
  log_debug("DATA HOOK: args, data: %s", buf);
  return 0;
}
static int hook_uri(htparser *parser, const char *buf, size_t count) {
  log_debug("DATA HOOK: uri, data: %s", buf);
  return 0;
}
static int hook_hdr_key(htparser *parser, const char *buf, size_t count) {
  log_debug("DATA HOOK: hdr_key, data: %s (%d bytes)", buf, count);
  struct rs_request *req = htparser_get_userdata(parser);
  struct rs_header *header = malloc(sizeof(struct rs_header));
  if(header == NULL) {
    log_error("malloc() failed to allocate new header: %s", strerror(errno));
    return -1;
  }
  header->key = malloc(count + 1);
  if(header->key == NULL) {
    log_error("malloc() failed to allocate new header key: %s", strerror(errno));
    free(header);
    return -1;
  }
  strncpy(header->key, buf, count);
  header->key[count] = 0;
  header->next = req->headers;
  req->headers = header;
  return 0;
}
static int hook_hdr_val(htparser *parser, const char *buf, size_t count) {
  log_debug("DATA HOOK: hdr_val, data: %s", buf);
  struct rs_request *req = htparser_get_userdata(parser);
  struct rs_header *header = req->headers;
  if(header == NULL) {
    log_error("received header value (\"%s\") before seeing any header key", buf);
    return -1;
  }
  header->value = malloc(count + 1);
  if(header->value == NULL) {
    log_error("malloc() failed to allocate new header value: %s", strerror(errno));
    return -1;
  }
  strncpy(header->value, buf, count);
  header->value[count] = 0;
  return 0;
}
static int hook_hostname(htparser *parser, const char *buf, size_t count) {
  log_debug("DATA HOOK: hostname, data: %s", buf);
  return 0;
}
static int hook_body(htparser *parser, const char *buf, size_t count) {
  //log_debug("DATA HOOK: body, data: (%d bytes)", count);
  struct rs_request *req = htparser_get_userdata(parser);
  if(req->file_fd > 0) {
    if(write(req->file_fd, buf, count) < 0) {
      log_error("write() to req->file_fd failed: %s", strerror(errno));
      return -1;
    }
  } else {
    log_error("Can't write body, no file_fd found for request!");
    return -1;
  }
  return 0;
}

// END HOOKS

struct htparse_hooks parser_hooks = {
  .on_msg_begin = hook_on_msg_begin,
  .method = hook_method,
  .scheme = hook_scheme,
  .host = hook_host,
  .port = hook_port,
  .path = hook_path,
  .args = hook_args,
  .uri = hook_uri,
  .on_hdrs_begin = hook_on_hdrs_begin,
  .hdr_key = hook_hdr_key,
  .hdr_val = hook_hdr_val,
  .hostname = hook_hostname,
  .on_hdrs_complete = hook_on_hdrs_complete,
  .on_new_chunk = hook_on_new_chunk,
  .on_chunk_complete = hook_on_chunk_complete,
  .on_chunks_complete = hook_on_chunks_complete,
  .body = hook_body,
  .on_msg_complete = hook_on_msg_complete
};

static void feed_parser(evutil_socket_t fd, short events, void *_parser) {
  htparser *parser = _parser;
  struct rs_request *req = htparser_get_userdata(parser);
  size_t nbytes = read(fd, req->buf, req->buflen);
  if(nbytes == 0) { // remote closed connection.
    log_debug("connection closed");
    free_request(req);
    return;
  } else if(nbytes < 0) {
    if(errno == EAGAIN || errno == EWOULDBLOCK) {
      return;
    }
  }
  log_debug("Feeding parser with %d bytes", nbytes);
  htparser_run(parser, &parser_hooks, req->buf, nbytes);
}

void process_request(struct rs_process_info *process_info, int fd, char *initbuf, int initbuf_len) {
  htparser *parser = htparser_new();
  htparser_init(parser, htp_type_request);
  struct rs_request *req = malloc(sizeof(struct rs_request));
  if(req == NULL) {
    log_error("malloc() failed: %s", strerror(errno));
    free_request(req);
    close(fd);
    return;
  }
  memset(req, 0, sizeof(struct rs_request));
  req->proc = process_info;
  req->buf = malloc(REQ_BUF_SIZE);
  if(req->buf == NULL) {
    log_error("malloc() failed: %s", strerror(errno));
    close(fd);
    free_request(req);
    return;
  }
  req->fd = fd;
  req->buflen = REQ_BUF_SIZE;
  htparser_set_userdata(parser, req);
  htparser_run(parser, &parser_hooks, initbuf, initbuf_len);
  req->read_event = event_new(process_info->base, fd,
                              EV_READ | EV_PERSIST,
                              feed_parser, parser);
  event_add(req->read_event, NULL);
  free(initbuf);
}
