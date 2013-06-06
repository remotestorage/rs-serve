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

void storage_exit(struct rs_process_info *process_info) {
  log_info("stopping process: storage (uid: %ld)", process_info->uid);
  event_base_loopexit(process_info->base, NULL);
}

#define REQ_BUF_SIZE 4096

struct rs_request {
  struct rs_process_info *proc;
  struct event *read_event;
  char *buf;
  int buflen;
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
  return 0;
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
  log_debug("DATA HOOK: hdr_key, data: %s", buf);
  return 0;
}
int hook_hdr_val(htparser *parser, const char *buf, size_t count) {
  log_debug("DATA HOOK: hdr_val, data: %s", buf);
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
  hook_on_msg_begin,
  hook_method,
  hook_scheme,
  hook_host,
  hook_port,
  hook_path,
  hook_args,
  hook_uri,
  hook_on_hdrs_begin,
  hook_hdr_key,
  hook_hdr_val,
  hook_hostname,
  hook_on_hdrs_complete,
  hook_on_new_chunk,
  hook_on_chunk_complete,
  hook_on_chunks_complete,
  hook_body,
  hook_on_msg_complete
};

void free_request(struct rs_request *req) {
  if(req->read_event) {
    event_del(req->read_event);
    event_free(req->read_event);
  }
  if(req->buf) {
    free(req->buf);
  }
  free(req);
}

void feed_parser(evutil_socket_t fd, short events, void *_parser) {
  htparser *parser = _parser;
  struct rs_request *req = htparser_get_userdata(parser);
  size_t nbytes = read(fd, req->buf, req->buflen);
  if(nbytes == 0) { // remote closed connection.
    free_request(req);
    return;
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
  req->buflen = REQ_BUF_SIZE;
  htparser_set_userdata(parser, req);
  htparser_run(parser, &parser_hooks, initbuf, initbuf_len);
  req->read_event = event_new(process_info->base, fd,
                              EV_READ | EV_PERSIST,
                              feed_parser, parser);
  event_add(req->read_event, NULL);
  free(initbuf);
}

void receive_request_fd(evutil_socket_t fd, short events, void *_process_info) {
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
  log_info("storage server received request fd: %d", request_fd);
  process_request(process_info, request_fd, databuf, databuf_len);
}

void read_signal(evutil_socket_t sfd, short events, void *_process_info) {
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

int storage_main(struct rs_process_info *process_info) {
  log_info("starting process: storage (uid: %ld)", process_info->uid);

  process_info->base = event_base_new();

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

  // socket pair sends request fds to child process
  struct event *socket_event = event_new(process_info->base,
                                         process_info->socket_out,
                                         EV_READ | EV_PERSIST,
                                         receive_request_fd,
                                         process_info);
  event_add(socket_event, NULL);

  // run loop
  event_base_dispatch(process_info->base);
  // loop done (probably storage_exit has been called)
  event_base_free(process_info->base);
  exit(EXIT_SUCCESS);
}
