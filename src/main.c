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
  fprintf(stderr, "Exiting.\n");
  exit(EXIT_SUCCESS);
}

void print_help(const char *progname) {
  fprintf(stderr,
          "Usage: %s [options]\n"
          "\n"
          "Options:\n"
          "  -h | --help                   - Display this text and exit.\n"
          "  -v | --version                - Print program version and exit.\n"
          "  -p <port> | --port=<port>     - Bind to given port (default: 80).\n"
          "  -n <name> | --hostname=<name> - Set hostname (defaults to local.dev)\n"
          "\n"
          "This program is distributed in the hope that it will be useful,\n"
          "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
          "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
          "GNU Affero General Public License for more details.\n\n"
          , progname);
}

void print_version() {
  fprintf(stderr, "rs-serve %d.%d%s\n", RS_VERSION_MAJOR, RS_VERSION_MINOR, RS_VERSION_POSTFIX);
}

int rs_port = 80;
char *rs_hostname = "local.dev";

static struct option long_options[] = {
  { "port", required_argument, 0, 'p' },
  { "hostname", required_argument, 0, 'n' },
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'v' },
  { 0, 0, 0, 0 }
};

int main(int argc, char **argv) {

  int opt;
  for(;;) {
    int opt_index = 0;
    opt = getopt_long(argc, argv, "p:n:hv", long_options, &opt_index);
    if(opt == '?') {
      // invalid option
      exit(EXIT_FAILURE);
    } else if(opt == -1) {
      // no more options
      break;
    } else if(opt == 'p') {
      rs_port = atoi(optarg);
    } else if(opt == 'n') {
      rs_hostname = optarg;
    } else if(opt == 'h') {
      print_help(argv[0]);
      return 0;
    } else if(opt == 'v') {
      print_version();
      return 0;
    }
  }

  event_set_fatal_callback(fatal_error_callback);

  magic_cookie = magic_open(MAGIC_MIME_TYPE);

  if(magic_cookie == NULL) {
    perror("magic_open() failed");
    exit(EXIT_FAILURE);
  }

  if(magic_load(magic_cookie, RS_MAGIC_DATABASE) != 0) {
    fprintf(stderr, "magic_load() failed: %s\n", magic_error(magic_cookie));
    exit(EXIT_FAILURE);
  }

  base = event_base_new();

  if(! base) {
    perror("Failed to create event base");
    exit(EXIT_FAILURE);
  }

  struct sigaction termaction;
  memset(&termaction, 0, sizeof(struct sigaction));
  termaction.sa_handler = cleanup_handler;

  sigaction(SIGTERM, &termaction, NULL);
  sigaction(SIGINT, &termaction, NULL);

  server = evhttp_new(base);

  if(! server) {
    perror("Failed to create server");
    exit(EXIT_FAILURE);
  }

  evhttp_bind_socket(server, RS_ADDRESS, RS_PORT);
  evhttp_set_allowed_methods(server, EVHTTP_REQ_OPTIONS | EVHTTP_REQ_HEAD | EVHTTP_REQ_GET | EVHTTP_REQ_PUT | EVHTTP_REQ_DELETE);
  evhttp_set_gencb(server, handle_request_callback, NULL);

  log_starting(RS_ADDRESS, RS_PORT);

  return event_base_dispatch(base);
}
