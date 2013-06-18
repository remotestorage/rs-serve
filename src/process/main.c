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
  struct signalfd_siginfo siginfo;
  if(read(fd, &siginfo, sizeof(siginfo)) < 0) {
    log_error("Failed to read signal: %s", strerror(errno));
    return;
  }
  switch(siginfo.ssi_signo) {
  case SIGINT:
    log_info("SIGINT caught, exiting.");
    exit(EXIT_SUCCESS);
    break;
  case SIGTERM:
    log_info("SIGTERM caught, exiting.");
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

static void handle_storage(evhtp_request_t *req, void *arg) {
  request_count++;
  log_info("[rc=%d] (start) %s %s", request_count, method_strmap[req->method], req->uri->path->full, req->status, req->finished);
  dispatch_storage(req, arg);
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

  if(RS_USE_SSL) {
    evhtp_ssl_cfg_t ssl_config;
    memset(&ssl_config, 0, sizeof(evhtp_ssl_cfg_t));
    ssl_config.pemfile = RS_SSL_CERT_PATH;
    ssl_config.privfile = RS_SSL_KEY_PATH;
    ssl_config.capath = RS_SSL_CA_PATH;

    if(evhtp_ssl_init(server, &ssl_config) != 0) {
      log_error("evhtp_ssl_init() failed");
      exit(EXIT_FAILURE);
    }
  }

  /* WEBFINGER */

  evhtp_callback_cb webfinger_cb = (RS_WEBFINGER_ENABLED ?
                                    handle_webfinger : reject_webfinger);
  evhtp_set_cb(server, "/.well-known/webfinger", webfinger_cb, NULL);
  // support legacy webfinger clients (we don't support XRD though):
  evhtp_set_cb(server, "/.well-known/host-meta", webfinger_cb, NULL);
  evhtp_set_cb(server, "/.well-known/host-meta.json", webfinger_cb, NULL);

  /* REMOTESTORAGE */

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

  /** RUN EVENT LOOP **/

  if(RS_DETACH) {
    int pid = fork();
    if(pid == 0) {
      event_reinit(rs_event_base);

      if(RS_LOG_FILE == stdout) {
        log_warn("No --log-file option given. Future output will be lost.");
        freopen("/dev/null", "r", stdout);
        freopen("/dev/null", "r", stderr);
      }

      return event_base_dispatch(rs_event_base);
    } else {
      printf("rs-serve detached with pid %d\n", pid);
      if(RS_PID_FILE) {
        fprintf(RS_PID_FILE, "%d", pid);
        fflush(RS_PID_FILE);
      }
      _exit(EXIT_SUCCESS);
    }
  } else {
    if(RS_PID_FILE) {
      fprintf(RS_PID_FILE, "%d", getpid());
      fflush(RS_PID_FILE);
    }
    return event_base_dispatch(rs_event_base);
  }
}
