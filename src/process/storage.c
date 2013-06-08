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

int hook_on_msg_begin(htparser *parser) {
  log_debug("HOOK: on_msg_begin");
  return 0;
}
int hook_on_hdrs_begin(htparser *parser) {
  log_debug("HOOK: on_hdrs_begin");
  return 0;
}
int hook_on_hdrs_complete(htparser *parser) {
  log_debug("HOOK: on_hdrs_complete");
  return 0;
}
int hook_on_new_chunk(htparser *parser) {
  log_debug("HOOK: on_new_chunk");
  return 0;
}
int hook_on_chunk_complete(htparser *parser) {
  log_debug("HOOK: on_chunk_complete");
  return 0;
}
int hook_on_chunks_complete(htparser *parser) {
  log_debug("HOOK: on_chunks_complete");
  return 0;
}
int hook_on_msg_complete(htparser *parser) {
  log_debug("HOOK: on_msg_complete");
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
    status = storage_handle_put(request);
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
  // otherwise they return zero and we can continue reading the request.
  if(status != 0) {
    send_error_response(request, status);
    return -1;
  } else {
    return 0;
  }
}

int hook_method(htparser *parser, const char *buf, size_t count) {
  log_debug("DATA HOOK: method, data: %s", buf);
  return 0;
}
int hook_scheme(htparser *parser, const char *buf, size_t count) {
  log_debug("DATA HOOK: scheme, data: %s", buf);
  return 0;
}
int hook_host(htparser *parser, const char *buf, size_t count) {
  log_debug("DATA HOOK: host, data: %s", buf);
  return 0;
}
int hook_port(htparser *parser, const char *buf, size_t count) {
  log_debug("DATA HOOK: port, data: %s", buf);
  return 0;
}
int hook_path(htparser *parser, const char *buf, size_t count) {
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
int hook_args(htparser *parser, const char *buf, size_t count) {
  log_debug("DATA HOOK: args, data: %s", buf);
  return 0;
}
int hook_uri(htparser *parser, const char *buf, size_t count) {
  log_debug("DATA HOOK: uri, data: %s", buf);
  return 0;
}
int hook_hdr_key(htparser *parser, const char *buf, size_t count) {
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
int hook_hdr_val(htparser *parser, const char *buf, size_t count) {
  log_debug("DATA HOOK: hdr_val, data: %s", buf);
  struct rs_request *req = htparser_get_userdata(parser);
  struct rs_header *header = req->headers;
  header->value = malloc(count + 1);
  if(header->value == NULL) {
    log_error("malloc() failed to allocate new header value: %s", strerror(errno));
    return -1;
  }
  strncpy(header->value, buf, count);
  header->value[count] = 0;
  return 0;
}
int hook_hostname(htparser *parser, const char *buf, size_t count) {
  log_debug("DATA HOOK: hostname, data: %s", buf);
  return 0;
}
int hook_body(htparser *parser, const char *buf, size_t count) {
  log_debug("DATA HOOK: body, data: %s", buf);
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

static void process_request(struct rs_process_info *process_info, int fd, char *initbuf, int initbuf_len) {
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

static void receive_request_fd(evutil_socket_t fd, short events, void *_process_info) {
  struct rs_process_info *process_info = _process_info;
  struct msghdr msg = {0};
  struct cmsghdr *cmsg;
  int buf[CMSG_SPACE(sizeof(int))];

  int databuf_len = 9 + PATH_MAX;
  char *databuf = malloc(4096);
  if(databuf == NULL) {
    log_error("malloc() failed: %s", strerror(errno));
    return;
  }
  *databuf = 0;
  struct iovec iov;
  iov.iov_base = databuf;
  iov.iov_len = databuf_len;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = buf;
  msg.msg_controllen = sizeof(buf);
  cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(int));
  if(recvmsg(fd, &msg, 0) < 0) {
    log_error("recvmsg() failed: %s", strerror(errno));
    return;
  }
  int request_fd = CMSG_DATA(cmsg)[0];
  // make sure that socket is non blocking:
  fcntl(request_fd, F_SETFL, O_NONBLOCK);
  process_request(process_info, request_fd, databuf, databuf_len);
}

static void storage_exit(struct rs_process_info *process_info) {
  log_info("stopping process: storage (uid: %ld)", process_info->uid);
  event_base_loopexit(process_info->base, NULL);
}

static void read_signal(evutil_socket_t sfd, short events, void *_process_info) {
  struct signalfd_siginfo siginfo;
  if(read(sfd, &siginfo, sizeof(siginfo)) == -1) {
    log_error("failed to read() from signal fd: %s", strerror(errno));
    return;
  }

  if(siginfo.ssi_signo == SIGHUP) {
    log_info("Received SIGHUP, will exit now.");
    storage_exit((struct rs_process_info*) _process_info);
  } else {
    log_error("Unhandled signal: %s", strsignal(siginfo.ssi_signo));
  }
}

int storage_main(struct rs_process_info *process_info,
                 int initial_fd, char *initial_buf, int initial_buf_len) {
  log_info("starting process: storage (uid: %ld)", process_info->uid);

  /** CHROOT TO HOME DIRECTORY **/

  struct passwd *user_entry = getpwuid(process_info->uid);
  if(chroot(user_entry->pw_dir) != 0) {
    log_error("Failed to chroot to home directory \"%s\": %s",
              user_entry->pw_dir, strerror(errno));
    exit(EXIT_FAILURE);
  }

  log_debug("changed root to \"%s\"", user_entry->pw_dir);

  /** SET SOME OPTIONS **/

  // set process name
  char process_name[16];
  snprintf(process_name, 16, "rs-serve [user: %s]", user_entry->pw_name);
  if(prctl(PR_SET_NAME, process_name, 0, 0, 0) != 0) {
    log_error("Failed to set process name: %s", strerror(errno));
  }

  // tell kernel to send us SIGHUP when the parent process dies
  // for whatever reason.
  if(prctl(PR_SET_PDEATHSIG, SIGHUP, 0, 0, 0) != 0) {
    log_error("Failed to specify parent-death-signal: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  /** DROP PRIVILEGES **/

  if(setuid(process_info->uid) != 0) {
    log_error("failed to drop privileges: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  log_debug("dropped privileges");

  /** SETUP EVENT BASE **/

  process_info->base = event_base_new();

  /** SETUP SIGNALS **/

  sigset_t sigmask;
  sigemptyset(&sigmask);
  sigaddset(&sigmask, SIGHUP);
  if(sigprocmask(SIG_BLOCK, &sigmask, NULL) != 0) {
    log_error("sigprocmask() failed: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }
  int sfd = signalfd(-1, &sigmask, SFD_NONBLOCK);
  if(sfd == -1) {
    log_error("signalfd() failed: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  struct event *signal_event = event_new(process_info->base, sfd,
                                         EV_READ | EV_PERSIST,
                                         read_signal, process_info);
  event_add(signal_event, NULL);

  /** SETUP SOCKET THAT INFORMS ABOUT REQUESTS **/

  // socket pair sends request fds to child process
  struct event *socket_event = event_new(process_info->base,
                                         process_info->socket_out,
                                         EV_READ | EV_PERSIST,
                                         receive_request_fd,
                                         process_info);
  event_add(socket_event, NULL);

  if(initial_fd > 0) {
    // got initial fd passed. -> process it immediately!
    process_request(process_info, initial_fd, initial_buf, initial_buf_len);
  }

  /** RUN EVENT LOOP **/

  // run loop
  event_base_dispatch(process_info->base);
  // loop done (probably storage_exit has been called)
  event_base_free(process_info->base);
  exit(EXIT_SUCCESS);
}
