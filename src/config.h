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

#ifndef RS_CONFIG_H
#define RS_CONFIG_H

/**
 * File: config.h
 *
 * This file contains configuration options and other constants that don't
 * change after the server process has been started. Some of the values are
 * compile-time constants, others can be changed through command-line flags
 * or are detected during startup.
 *
 * All variables within this file declared at "extern" are defined and
 * initialized in "config.c", which also evaluates command line arguments.
 *
 * All code outside this file and config.c only accesses these values via
 * their uppercase (RS_*) variant, so if you want to turn any of the mutable
 * options into compile-time constants, you can do so via this file.
 *
 */

// address & port to bind to
#define RS_ADDRESS "0.0.0.0"
extern int rs_port;
#define RS_PORT rs_port

// (exception: rs_event_base is defined in main.c)
extern struct event_base *rs_event_base;
#define RS_EVENT_BASE rs_event_base

// only used for webfinger result at the moment
#define RS_SCHEME "http"
extern char *rs_hostname;
#define RS_HOSTNAME rs_hostname
#define RS_STORAGE_API "draft-dejong-remotestorage-01"
#define RS_AUTH_METHOD "http://tools.ietf.org/html/rfc6749#section-4.2"
extern char *rs_auth_uri;
#define RS_AUTH_URI rs_auth_uri
extern int rs_auth_uri_len;
#define RS_AUTH_URI_LEN rs_auth_uri_len
extern int rs_webfinger_enabled;
#define RS_WEBFINGER_ENABLED rs_webfinger_enabled

// magic database file to use (NULL indicates system default)
#define RS_MAGIC_DATABASE NULL

// CORS header values
#define RS_ALLOW_ORIGIN "*"
#define RS_ALLOW_HEADERS "Authorization, Content-Type, Content-Length, ETag"
#define RS_ALLOW_METHODS "HEAD, GET, PUT, DELETE"

// permissions for newly created files
#define RS_FILE_CREATE_MODE S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP

// log file
extern FILE *rs_log_file;
#define RS_LOG_FILE rs_log_file

// pid file
extern FILE *rs_pid_file;
#define RS_PID_FILE rs_pid_file
extern char *rs_pid_file_path;
#define RS_PID_FILE_PATH rs_pid_file_path

extern char *rs_home_serve_root;
#define RS_HOME_SERVE_ROOT rs_home_serve_root
extern int rs_home_serve_root_len;
#define RS_HOME_SERVE_ROOT_LEN rs_home_serve_root_len

extern struct rs_header rs_default_headers;
#define RS_DEFAULT_HEADERS rs_default_headers

#define RS_MIN_UID 1000

#define RS_AUTH_FILE_PATH "/var/lib/rs-serve/authorizations.dat"

void init_config(int argc, char **argv);
void cleanup_config(void);

#endif /* !RS_CONFIG_H */
