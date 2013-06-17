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

unsigned int request_count;

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

void handle_signal(evutil_socket_t fd, short events, void *arg) {
  log_info("handle_signal()");
  struct signalfd_siginfo siginfo;
  if(read(fd, &siginfo, sizeof(siginfo)) < 0) {
    log_error("Failed to read signal: %s", strerror(errno));
    return;
  }
  switch(siginfo.ssi_signo) {
  case SIGINT:
  case SIGTERM:
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

static evhtp_res finish_request(evhtp_request_t *req, void *arg) {
  request_count--;
  log_info("[rc=%d] %s %s -> %d (fini: %d)", request_count, method_strmap[req->method], req->uri->path->full, req->status, req->finished);
  return 0;
}

static void verify_user(evhtp_request_t *req) {
  evhtp_path_t *path = req->uri->path;
  char *username = path->match_start;
  uid_t uid = user_get_uid(username);
  if(uid == -1) {
    req->status = EVHTP_RES_NOTFOUND;
  } else if(uid == -2) {
    req->status = EVHTP_RES_SERVERR;
  } else if(! UID_ALLOWED(uid)) {
    log_info("User not allowed: %s (uid: %ld)", username, uid);
    req->status = EVHTP_RES_NOTFOUND;
  } else {
    log_debug("User found: %s (uid: %ld)", username, uid);
  }
}

void add_cors_headers(evhtp_request_t *req) {
  ADD_RESP_HEADER(req, "Access-Control-Allow-Origin", RS_ALLOW_ORIGIN);
  ADD_RESP_HEADER(req, "Access-Control-Allow-Headers", RS_ALLOW_HEADERS);
  ADD_RESP_HEADER(req, "Access-Control-Allow-Methods", RS_ALLOW_METHODS);
}

static void handle_storage(evhtp_request_t *req, void *arg) {
  request_count++;
  log_info("[rc=%d] (start) %s %s", request_count, method_strmap[req->method], req->uri->path->full, req->status, req->finished);

  req->status = 0;

  do {

    // validate user
    verify_user(req);

    log_info("status after verify_user(): %d", req->status);

    if(req->status) break; // bail

    // authorize request
    if(req->method != htp_method_OPTIONS) {
      int auth_result = authorize_request(req);
      if(auth_result == 0) {
        log_debug("Request authorized.");
      } else if(auth_result == -1) {
        log_info("Request NOT authorized.");
        req->status = EVHTP_RES_UNAUTH;
      } else if(auth_result == -2) {
        log_error("An error occured while authorizing request.");    
        req->status = EVHTP_RES_SERVERR; 
      }
    }

    log_info("status after authorize_request(): %d", req->status);

    if(req->status) break; // bail

    // dispatch to storage handler
    if(req->status == 0) {
      switch(req->method) {
      case htp_method_OPTIONS:
        add_cors_headers(req);
        req->status = EVHTP_RES_NOCONTENT;
        break;
      case htp_method_GET:
        req->status = storage_handle_get(req);
        break;
      case htp_method_HEAD:
        req->status = storage_handle_head(req);
        break;
      case htp_method_PUT:
        req->status = storage_handle_put(req);
        break;
      case htp_method_DELETE:
        req->status = storage_handle_delete(req);
        break;
      default:
        req->status = EVHTP_RES_METHNALLOWED;
      }
    }

  } while(0);

  // send reply, if status was set
  if(req->status) {
    evhtp_send_reply(req, req->status);
  }
}

magic_t magic_cookie;

int main(int argc, char **argv) {

  init_config(argc, argv);

  open_authorizations("r");

  init_webfinger();

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

  evhtp_set_cb(server, "/.well-known/webfinger", handle_webfinger, NULL);

  // support legacy webfinger clients (we don't support XRD though):
  evhtp_set_cb(server, "/.well-known/host-meta", handle_webfinger, NULL);
  evhtp_set_cb(server, "/.well-known/host-meta.json", handle_webfinger, NULL);

  evhtp_callback_t *storage_cb = evhtp_set_regex_cb(server, "^/storage/([^/]+)/.*$", handle_storage, NULL);

  evhtp_set_hook(&storage_cb->hooks, evhtp_hook_on_request_fini, finish_request, NULL);

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
