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

#define ASSERT(cond, desc) {                    \
    if(!(cond)) {                               \
      perror(#desc " failed");                  \
      exit(EXIT_FAILURE);                       \
    }                                           \
  }
#define ASSERT_NOT_NULL(var, desc) ASSERT((var) != NULL, desc)
#define ASSERT_ZERO(var, desc)     ASSERT((var) == 0, desc)
#define ASSERT_NOT_EQ(a, b, desc)  ASSERT((a) != (b), desc)

void handle_signal(evutil_socket_t fd, short events, void *arg) {
  log_info("handle_signal()");
  struct signalfd_siginfo siginfo;
  if(read(fd, &siginfo, sizeof(siginfo)) < 0) {
    log_error("Failed to read signal: %s", strerror(errno));
    return;
  }
  int child_status = 0;
  switch(siginfo.ssi_signo) {
  case SIGCHLD:
    if(waitpid(siginfo.ssi_pid, &child_status, WNOHANG) == siginfo.ssi_pid) {
      log_info("Child %ld exited with status %d.", siginfo.ssi_pid, child_status);
      struct rs_process_info *child_info = process_remove(siginfo.ssi_pid);
      if(child_info == NULL) {
        log_error("while tidying up child %ld: couldn't find process info",
                  siginfo.ssi_pid);
      } else {
        free(child_info);
      }
    } else {
      log_error("Received SIGCHLD, but waitpid() failed: %s", strerror(errno));
    }
    break;
  case SIGINT:
  case SIGTERM:
    process_kill_all();
    exit(EXIT_SUCCESS);
    break;
  default:
    log_error("Unhandled signal caught: %s", strsignal(siginfo.ssi_signo));
  }
}

struct event_base *rs_event_base = NULL;

void log_event_base_message(int severity, const char *msg) {
  char *format = "(from libevent) %s";
  switch(severity) {
  case EVENT_LOG_DEBUG:
    log_debug(format, msg);
    break;
  case EVENT_LOG_MSG:
    log_info(format, msg);
    break;
  case EVENT_LOG_ERR:
    log_error(format, msg);
    break;
  case EVENT_LOG_WARN:
    log_warn(format, msg);
    break;
  }
}

struct rs_temp_request {
  struct evbuffer *buffer;
  struct event *event;
  int fd;
};

void free_temp_request(struct rs_temp_request *temp_req) {
  if(temp_req->buffer) {
    evbuffer_free(temp_req->buffer);
  }
  if(temp_req->event) {
    event_del(temp_req->event);
    event_free(temp_req->event);
    if(close(temp_req->fd) != 0) {
      log_error("close() failed: %s", strerror(errno));
    }
  }
  free(temp_req);
}

void read_first_line(evutil_socket_t fd, short events, void *arg) {
  struct rs_temp_request *temp_req = arg;
  char byte;
  int eol_reached = 0;
  int count;
  for(;;) {
    count = recv(fd, &byte, 1, 0);
    if(count == -1) {
      // recv() returned an error
      if(errno == EAGAIN || errno == EWOULDBLOCK) {
        // we've read all we could, but nothing fatal happened.
        if(eol_reached) {
          if(dispatch_request(temp_req->buffer, fd) != 0) {
            log_error("Failed to dispatch request. Closing socket.");
          }
          free_temp_request(temp_req);
          return;
        }
        return;
      } else {
        // some fatal error occured
        log_error("recv() failed: %s", strerror(errno));
        free_temp_request(temp_req);
        return;
      }
    } else if(count == 0) {
      log_error("Connection closed");
      free_temp_request(temp_req);
      return;
    } else {
      // recv() succeeded
      if(byte == '\n') {
        // end of line reached, ready to proceed once no more bytes to read.
        eol_reached = 1;
      }
      evbuffer_add(temp_req->buffer, &byte, 1);
    }
  }
}

static void accept_connection(struct evconnlistener *listener, evutil_socket_t fd,
                              struct sockaddr *address, int socklen, void *ctx) {
  log_debug("Accepted connection");
  // setup event to receive first line
  struct rs_temp_request *temp_req = malloc(sizeof(struct rs_temp_request));
  if(temp_req == NULL) {
    close(fd);
    log_error("Dropping connection, malloc() failed: %s\n", strerror(errno));
    return;
  }
  temp_req->fd = fd;
  temp_req->buffer = evbuffer_new();
  if(temp_req->buffer == NULL) {
    free(temp_req);
    close(fd);
    log_error("Dropping connection, evbuffer_new() failed: %s\n", strerror(errno));
    return;
  }
  temp_req->event = event_new(RS_EVENT_BASE, fd, EV_READ | EV_PERSIST,
                              read_first_line, temp_req);
  if(temp_req->event == NULL) {
    evbuffer_free(temp_req->buffer);
    free(temp_req);
    close(fd);
    log_error("Dropping connection, event_new() failed: %s\n", strerror(errno));
  }
  event_add(temp_req->event, NULL);
}

static evhtp_res receive_path(evhtp_request_t *req, evhtp_path_t *path, void *arg) {
  log_info("full path: %s (user: %s, path: %s, file: %s)",
           path->full, path->match_start, path->match_end, path->file);
  char *username = path->match_start;
  uid_t uid = user_get_uid(username);
  if(uid == -1) {
    evhtp_send_reply(req, EVHTP_RES_NOTFOUND);
  } else if(uid == -2) {
    evhtp_send_reply(req, EVHTP_RES_SERVERR);
  } else if(uid < RS_MIN_UID) {
    log_info("User not allowed: %s (uid: %ld)", username, uid);
    evhtp_send_reply(req, EVHTP_RES_NOTFOUND);
  } else {
    log_debug("User found: %s (uid: %ld)", username, uid);
    return EVHTP_RES_OK;
  }
  return EVHTP_RES_PAUSE;
}

void add_cors_headers(evhtp_request_t *req) {
  ADD_RESP_HEADER(req, "Access-Control-Allow-Origin", RS_ALLOW_ORIGIN);
  ADD_RESP_HEADER(req, "Access-Control-Allow-Headers", RS_ALLOW_HEADERS);
  ADD_RESP_HEADER(req, "Access-Control-Allow-Methods", RS_ALLOW_METHODS);
}

static evhtp_res handle_options(evhtp_request_t *req) {
  evhtp_send_reply(req, EVHTP_RES_NOCONTENT);
  return EVHTP_RES_PAUSE;
}

static evhtp_res receive_headers(evhtp_request_t *req, evhtp_headers_t *hdr, void *arg) {

  add_cors_headers(req);

  if(req->method == htp_method_OPTIONS) {
    return handle_options(req);
  }

  int auth_result = authorize_request(req);
  if(auth_result == 0) {
    log_debug("Request authorized.");
    return EVHTP_RES_OK;
  } else if(auth_result == -1) {
    log_info("Request NOT authorized.");
    evhtp_send_reply(req, EVHTP_RES_UNAUTH);
  } else if(auth_result == -2) {
    log_error("An error occured while authorizing request.");
    evhtp_send_reply(req, EVHTP_RES_SERVERR);
  }
  return EVHTP_RES_PAUSE;
}

static void handle_storage(evhtp_request_t *req, void *arg) {
  evhtp_res status = 0;
  switch(req->method) {
  case htp_method_GET:
    status = storage_handle_get(req);
    break;
  case htp_method_HEAD:
    status = storage_handle_head(req);
    break;
  //case htp_method_PUT:
  //  status = storage_handle_put(req);
  //  break;
  //case htp_method_DELETE:
  //  status = storage_handle_delete(req);
  //  break;
  default:
    status = EVHTP_RES_METHNALLOWED;
  }
  if(status != 0) {
    evhtp_send_reply(req, status);
  }
}

magic_t magic_cookie;

int main(int argc, char **argv) {

  init_config(argc, argv);

  open_authorizations("r");

  /** OPEN MAGIC DATABASE **/

  magic_cookie = magic_open(MAGIC_MIME);
  if(magic_load(magic_cookie, RS_MAGIC_DATABASE) != 0) {
    log_error("Failed to load magic database: %s", magic_error(magic_cookie));
    exit(EXIT_FAILURE);
  }

  log_info("starting process: main");

  if(prctl(PR_SET_NAME, "rs-serve [main]", 0, 0, 0) != 0) {
    log_error("Failed to set process name: %s", strerror(errno));
  }

  /** SETUP EVENT BASE **/

  rs_event_base = event_base_new();
  ASSERT_NOT_NULL(rs_event_base, "event_base_new()");
  log_debug("libevent method: %s", event_base_get_method(rs_event_base));
  event_set_log_callback(log_event_base_message);

  // TODO: add error cb to base


  /** SETUP LISTENER **/

  struct sockaddr_in sin;
  memset(&sin, 0, sizeof(struct sockaddr_in));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(0);
  sin.sin_port = htons(RS_PORT);

  evhtp_t *server = evhtp_new(rs_event_base, NULL);

  evhtp_callback_t *storage_cb = evhtp_set_regex_cb(server, "^/storage/([^/]+)/.*$", handle_storage, NULL);

  evhtp_set_hook(&storage_cb->hooks, evhtp_hook_on_headers, receive_headers, NULL);
  evhtp_set_hook(&storage_cb->hooks, evhtp_hook_on_path, receive_path, NULL);

  if(evhtp_bind_sockaddr(server, (struct sockaddr*)&sin, sizeof(sin), 1024) != 0) {
    log_error("evhtp_bind_sockaddr() failed: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }

  /** SETUP SIGNALS **/

  sigset_t sigmask;
  sigemptyset(&sigmask);
  sigaddset(&sigmask, SIGINT);
  sigaddset(&sigmask, SIGTERM);
  sigaddset(&sigmask, SIGCHLD);
  ASSERT_ZERO(sigprocmask(SIG_BLOCK, &sigmask, NULL), "sigprocmask()");
  int sfd = signalfd(-1, &sigmask, SFD_NONBLOCK);
  ASSERT_NOT_EQ(sfd, -1, "signalfd()");

  struct event *signal_event = event_new(rs_event_base, sfd, EV_READ | EV_PERSIST,
                                         handle_signal, NULL);
  event_add(signal_event, NULL);

  /** WRITE PID FILE **/
  if(RS_PID_FILE) {
    fprintf(RS_PID_FILE, "%d", getpid());
    fflush(RS_PID_FILE);
  }

  /** RUN EVENT LOOP **/

  return event_base_dispatch(rs_event_base);
}
