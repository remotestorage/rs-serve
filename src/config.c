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

static void print_help(const char *progname) {
  fprintf(stderr,
          "Usage: %s [options]\n"
          "\n"
          "Options:\n"
          "  -h        | --help            - Display this text and exit.\n"
          "  -v        | --version         - Print program version and exit.\n"
          "  -p <port> | --port=<port>     - Bind to given port (default: 80).\n"
          "  -n <name> | --hostname=<name> - Set hostname (defaults to local.dev)\n"
          "  -r <root> | --root=<root>     - Root directory to serve (defaults to cwd)\n"
          "\n"
          "This program is distributed in the hope that it will be useful,\n"
          "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
          "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
          "GNU Affero General Public License for more details.\n"
          "(c) 2013 Niklas E. Cathor\n\n"
          , progname);
}

static void print_version() {
  fprintf(stderr, "rs-serve %d.%d%s\n", RS_VERSION_MAJOR, RS_VERSION_MINOR, RS_VERSION_POSTFIX);
}

int rs_port = 80;
char *rs_hostname = "local.dev";
char *rs_storage_root;
int rs_storage_root_len;

static struct option long_options[] = {
  { "port", required_argument, 0, 'p' },
  { "hostname", required_argument, 0, 'n' },
  { "root", required_argument, 0, 'r' },
  // TODO:
  //{ "listen", required_argument, 0, 'l' },
  //{ "log-file", required_argument, 0, 'f' },
  //{ "background", no_argument, 0, 'b' },
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'v' },
  { 0, 0, 0, 0 }
};

void init_config(int argc, char **argv) {
  int opt;
  for(;;) {
    int opt_index = 0;
    opt = getopt_long(argc, argv, "p:n:r:hv", long_options, &opt_index);
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
    } else if(opt == 'r') {
      rs_storage_root = strdup(optarg);
      rs_storage_root_len = strlen(rs_storage_root);
    } else if(opt == 'h') {
      print_help(argv[0]);
      exit(EXIT_SUCCESS);
    } else if(opt == 'v') {
      print_version();
      exit(EXIT_SUCCESS);
    }
  }

  // init RS_STORAGE_ROOT
  if(rs_storage_root == NULL) {
    rs_storage_root = malloc(PATH_MAX + 1);
    if(getcwd(rs_storage_root, PATH_MAX + 1) == NULL) {
      perror("getcwd() failed");
      exit(EXIT_FAILURE);
    }
    rs_storage_root_len = strlen(rs_storage_root);
    rs_storage_root = realloc(rs_storage_root, rs_storage_root_len + 1);
  }
}

void cleanup_config() {
  free(rs_storage_root);
}
