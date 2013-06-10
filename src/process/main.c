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

magic_t magic_cookie;

int main(int argc, char **argv) {

  init_config(argc, argv);

  /** OPEN MAGIC DATABASE **/

  magic_cookie = magic_open(MAGIC_MIME);
  if(magic_load(magic_cookie, RS_MAGIC_DATABASE) != 0) {
    log_error("Failed to load magic database: %s", magic_error(magic_cookie));
    exit(EXIT_FAILURE);
  }

  /** CHECK IF WE ARE ROOT **/

  if(getuid() != 0) {
    log_error("Sorry, you need to run this as user root for (so the child processes are able to chroot(2)).");
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

  struct evconnlistener *listener = evconnlistener_new_bind(rs_event_base,
                                                            accept_connection,
                                                            NULL,
                                                            LEV_OPT_CLOSE_ON_FREE |
                                                            LEV_OPT_REUSEABLE, -1,
                                                            (struct sockaddr*)&sin,
                                                            sizeof(sin));
  ASSERT_NOT_NULL(listener, "evconnlistener_new_bind()");

  // TODO: add error cb to listener

  // unfortunately required to initialize htparse internas:
  evhtp_free(evhtp_new(rs_event_base, NULL));

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
