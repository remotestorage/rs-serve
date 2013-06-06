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

#define ASSERT_NOT(cond, desc) {                \
    if(cond) {                                  \
      perror(#desc " failed");                  \
      exit(EXIT_FAILURE);                       \
    }                                           \
  }
#define ASSERT_NOT_NULL(var, desc) ASSERT_NOT((var) == NULL, desc)
#define ASSERT_ZERO(var, desc) ASSERT_NOT((var) != 0, desc)
#define ASSERT_NOT_EQ(a, b, desc) ASSERT_NOT((a) == (b), desc);

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
        // FIXME: this causes a double-free error.
        //free(child_info);
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

int main(int argc, char **argv) {

  init_config(argc, argv);

  log_info("starting process: main");

  rs_event_base = event_base_new();
  ASSERT_NOT_NULL(rs_event_base, "event_base_new()");

  event_set_log_callback(log_event_base_message);

  evhtp_t *server = evhtp_new(rs_event_base, NULL);
  ASSERT_NOT_NULL(server, "evhtp_new()");

  log_debug("libevent method: %s", event_base_get_method(rs_event_base));

  setup_dispatch_handler(server, RS_STORAGE_PATH_RE);

  ASSERT_NOT_NULL(server, "initializing evhttp server");
  ASSERT_ZERO(evhtp_bind_socket(server, RS_ADDRESS, RS_PORT, 1024),
              "binding socket");

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

  return event_base_dispatch(rs_event_base);
}
