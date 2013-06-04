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
          "  -n <name> | --hostname=<name> - Set hostname (defaults to local.dev).\n"
          "  -r <root> | --root=<root>     - Root directory to serve (defaults to cwd).\n"
          "              --chroot          - chroot() to root directory before serving any\n"
          "                                  files.\n"
          "  -f <file> | --log-file=<file> - Log to given file (defaults to stdout)\n"
          "  -d        | --detach          - After starting the server, detach server\n"
          "                                  process and exit. If you don't use this in\n"
          "                                  combination with the --log-file option, all\n"
          "                                  future output will be lost.\n"
          "  -u        | --uid             - After binding to the specified port (but\n"
          "                                  before accepting any connections) set the\n"
          "                                  user ID of to the given value. This only\n"
          "                                  works if the process is run as root.\n"
          "              --user            - Same as --uid option, but specify the user\n"
          "                                  by name.\n"
          "  -g        | --gid             - Same as --uid, but setting the group ID.\n"
          "              --group           - Same as --user, but setting the group.\n"
          "  --pid-file                    - Write PID to given file.\n"
          "  --stop                        - Stop a running rs-serve process. The process\n"
          "                                  is identified by the PID file specified via\n"
          "                                  the --pid-file option. NOTE: the --stop option\n"
          "                                  MUST precede the --pid-file option on the\n"
          "                                  command line for this this to work.\n"
          "  --homes=<dir>                 - Serve from user's home directories instead of\n"
          "                                  from the root path.   The options --root and\n"
          "                                  --homes are mutually exclusive.\n"
          "                                  The given <dir> argument denotes the directory\n"
          "                                  within each user's home directory to use as\n"
          "                                  their storage-root.\n"
          "  --homes-group=<group>         - Only serve home directories of members of this\n"
          "                                  system group.\n"
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
char *rs_storage_root = NULL;
int rs_storage_root_len = 0;
int rs_chroot = 0;
int rs_detach = 0;
// equivalents of rs_storage_root(_len), but rs_chroot aware
char *rs_real_storage_root = NULL;
int rs_real_storage_root_len = 0;
FILE *rs_log_file = NULL;
uid_t rs_set_uid = 0;
gid_t rs_set_gid = 0;
FILE *rs_pid_file = NULL;
char *rs_pid_file_path = NULL;
// home directory serving
int rs_serve_homes = 0;
char *rs_serve_homes_dir = NULL;
gid_t rs_serve_homes_gid;

int rs_stop_other = 0;

static struct option long_options[] = {
  { "port", required_argument, 0, 'p' },
  { "hostname", required_argument, 0, 'n' },
  { "root", required_argument, 0, 'r' },
  { "chroot", no_argument, 0, 0 },
  { "uid", required_argument, 0, 'u' },
  { "gid", required_argument, 0, 'g' },
  { "user", required_argument, 0, 0 },
  { "group", required_argument, 0, 0 },
  { "pid-file", required_argument, 0, 0 },
  { "stop", no_argument, 0, 0 },
  { "homes", required_argument, 0, 0 },
  { "homes-group", required_argument, 0, 0 },
  // TODO:
  //{ "listen", required_argument, 0, 'l' },
  { "log-file", required_argument, 0, 'f' },
  { "detach", no_argument, 0, 'd' },
  { "help", no_argument, 0, 'h' },
  { "version", no_argument, 0, 'v' },
  { 0, 0, 0, 0 }
};

void close_pid_file() {
  fclose(RS_PID_FILE);
  unlink(RS_PID_FILE_PATH);
}

void init_config(int argc, char **argv) {
  int opt;
  for(;;) {
    int opt_index = 0;
    opt = getopt_long(argc, argv, "p:n:r:f:du:g:hv", long_options, &opt_index);
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
    } else if(opt == 'f') {
      rs_log_file = fopen(optarg, "a");
      if(rs_log_file == NULL) {
        perror("Failed to open log file");
        exit(EXIT_FAILURE);
      }
    } else if(opt == 'd') {
      rs_detach = 1;
    } else if(opt == 'u') {
      rs_set_uid = atoi(optarg);
    } else if(opt == 'g') {
      rs_set_gid = atoi(optarg);
    } else if(opt == 'h') {
      print_help(argv[0]);
      exit(127);
    } else if(opt == 'v') {
      print_version();
      exit(127);
    } else if(opt == 0) {
      // long option with no short equivalent
      if(strcmp(long_options[opt_index].name, "chroot") == 0) {
        rs_chroot = 1;
      } else if(strcmp(long_options[opt_index].name, "user") == 0) { // --user
        errno = 0;
        struct passwd *user_entry = getpwnam(optarg);
        if(user_entry == NULL) {
          if(errno != 0) {
            perror("getpwnam() failed");
          } else {
            fprintf(stderr, "Failed to find UID for user \"%s\".\n", optarg);
          }
          exit(EXIT_FAILURE);
        }
        rs_set_uid = user_entry->pw_uid;
      } else if(strcmp(long_options[opt_index].name, "group") == 0) { // --group
        errno = 0;
        struct group *group_entry = getgrnam(optarg);
        if(group_entry == NULL) {
          if(errno != 0) {
            perror("getgrnam() failed");
          } else {
            fprintf(stderr, "Failed to find GID for group \"%s\".\n", optarg);
          }
          exit(EXIT_FAILURE);
        }
        rs_set_gid = group_entry->gr_gid;
      } else if(strcmp(long_options[opt_index].name, "pid-file") == 0) { // --pid-file
        rs_pid_file_path = optarg;

        // stop was requested, kill other process by pid-file
        if(rs_stop_other) {
          rs_pid_file = fopen(rs_pid_file_path, "r");
          if(rs_pid_file == NULL) {
            perror("Failed to open pid file for reading");
            exit(EXIT_FAILURE);
          }
          pid_t pid;
          fscanf(rs_pid_file, "%d", &pid);
          if(kill(pid, SIGTERM) == 0) {
            printf("Sent SIGTERM to process %d\n", pid);
            exit(EXIT_SUCCESS);
          } else {
            fprintf(stderr, "Sending SIGTERM to process %d failed: %s\n", pid, strerror(errno));
            exit(EXIT_FAILURE);
          }
        }

        // open pid file and store our pid there
        rs_pid_file = fopen(rs_pid_file_path, "wx");
        if(rs_pid_file == NULL) {
          perror("Failed to open pid file");
          exit(EXIT_FAILURE);
        }
        atexit(close_pid_file);
      } else if(strcmp(long_options[opt_index].name, "stop") == 0) { // --stop
        rs_stop_other = 1;
      } else if(strcmp(long_options[opt_index].name, "homes") == 0) { // --homes
        rs_serve_homes = 1;
        rs_serve_homes_dir = optarg;
        fprintf(stderr, "WARNING: --homes is not implemented yet.\n");
      } else if(strcmp(long_options[opt_index].name, "homes-group") == 0) { // --homes-group
        errno = 0;
        struct group *group_entry = getgrnam(optarg);
        if(group_entry == NULL) {
          if(errno != 0) {
            perror("getgrnam() failed");
          } else {
            fprintf(stderr, "Failed to find GID for group \"%s\".\n", optarg);
          }
          exit(EXIT_FAILURE);
        }
        rs_serve_homes_gid = group_entry->gr_gid;
        fprintf(stderr, "WARNING: --homes-group is not implemented yet.\n");
      }
    }
  }

  if(rs_storage_root && rs_serve_homes) {
    fprintf(stderr, "ERROR: You cannot specify both --root and --homes options.\n");
    exit(EXIT_FAILURE);
  }

  if(rs_stop_other) {
    fprintf(stderr, "ERROR: can't stop existing process without --pid-file option.\n");
    exit(EXIT_FAILURE);
  }

  if(! rs_serve_homes) {
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

    if(RS_CHROOT) {
      rs_real_storage_root = "";
      rs_real_storage_root_len = 0;
    } else {
      rs_real_storage_root = rs_storage_root;
      rs_real_storage_root_len = rs_storage_root_len;
    }
  }

  if(rs_log_file == NULL) {
    rs_log_file = stdout;
  }

}

void cleanup_config() {
  free(rs_storage_root);
}
