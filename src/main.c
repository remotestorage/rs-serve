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

magic_t magic_cookie;

struct event_base *base;
struct evhttp *server;

void cleanup_handler(int signum) {
  evhttp_free(server);
  event_base_free(base);
  magic_close(magic_cookie);
  cleanup_config();
  fprintf(stderr, "Exiting.\n");
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {

  // parse command line arguments, set configuration vars (rs_*)
  init_config(argc, argv);

  // initialize libmagic
  magic_cookie = magic_open(MAGIC_MIME_TYPE);

  if(magic_cookie == NULL) {
    perror("magic_open() failed");
    exit(EXIT_FAILURE);
  }

  if(magic_load(magic_cookie, RS_MAGIC_DATABASE) != 0) {
    fprintf(stderr, "magic_load() failed: %s\n", magic_error(magic_cookie));
    exit(EXIT_FAILURE);
  }

  // change root if requested
  if(RS_CHROOT) {
    if(chroot(RS_STORAGE_ROOT) != 0) {
      perror("chroot() failed");
      exit(EXIT_FAILURE);
    }
  }

  // initialize libevent
  event_set_fatal_callback(fatal_error_callback);

  base = event_base_new();

  if(! base) {
    perror("Failed to create event base");
    exit(EXIT_FAILURE);
  }

  // hook up event handlers (TERM and INT both terminate)
  struct sigaction termaction;
  memset(&termaction, 0, sizeof(struct sigaction));
  termaction.sa_handler = cleanup_handler;

  sigaction(SIGTERM, &termaction, NULL);
  sigaction(SIGINT, &termaction, NULL);

  // setup server
  server = evhttp_new(base);

  if(! server) {
    perror("Failed to create server");
    exit(EXIT_FAILURE);
  }

  log_starting();

  if(evhttp_bind_socket(server, RS_ADDRESS, RS_PORT) != 0) {
    perror("Failed to bind to socket");
    exit(EXIT_FAILURE);
  }
  evhttp_set_allowed_methods(server, EVHTTP_REQ_OPTIONS | EVHTTP_REQ_HEAD | EVHTTP_REQ_GET | EVHTTP_REQ_PUT | EVHTTP_REQ_DELETE);
  evhttp_set_gencb(server, handle_request_callback, NULL);

  if(RS_SET_GID) {
    if(setgid(RS_SET_GID) != 0) {
      perror("setgid() failed");
      exit(EXIT_FAILURE);
    }
  }

  if(RS_SET_UID) {
    if(setuid(RS_SET_UID) != 0) {
      perror("setuid() failed");
      exit(EXIT_FAILURE);
    }
  }

  if(getuid() == 0) {
    fprintf(stderr, "warning: running as root is discouraged. Use the --uid or --user option.\n");
  }

  if(RS_DETACH) {
    int child_pid = fork();
    if(child_pid == 0) {
      freopen("/dev/null", "w", stdout);
      freopen("/dev/null", "w", stderr);
      if(RS_LOG_FILE != stdout) {
        // if we have a log file, redirect errors to that file as well.
        stderr = RS_LOG_FILE;
      }
      return event_base_dispatch(base);
    } else if(child_pid == -1) {
      perror("fork() failed");
      exit(EXIT_FAILURE);
    } else {
      if(RS_LOG_FILE == stdout) {
        fprintf(stderr, "warning: --log-file option not given, future output will be lost.\n");
      }
      fclose(stdout);
      fclose(stderr);
      _exit(0);
    }
  } else {
    return event_base_dispatch(base);
  }
}
