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
#define RS_ADDRESS NULL
extern int rs_port;
#define RS_PORT rs_port

// only used for webfinger result at the moment
#define RS_SCHEME "http"
extern char *rs_hostname;
#define RS_HOSTNAME rs_hostname
#define RS_STORAGE_API "draft-dejong-remotestorage-00"
#define RS_AUTH_METHOD "http://tools.ietf.org/html/rfc6749#section-4.2"

// static auth token
#define RS_TOKEN "static-token-for-now"

// request path to the storage-root
#define RS_STORAGE_PATH "/storage"
#define RS_STORAGE_PATH_LEN 8
// request path to the authorization handler
#define RS_AUTH_PATH "/auth"
#define RS_AUTH_PATH_LEN 5
// request path to webfinger handler
#define RS_WEBFINGER_PATH "/.well-known/webfinger"
#define RS_WEBFINGER_PATH_LEN 22

// path to the storage-root on the filesystem
extern char *rs_storage_root;
#define RS_STORAGE_ROOT rs_storage_root
extern int rs_storage_root_len;
#define RS_STORAGE_ROOT_LEN rs_storage_root_len

// magic database file to use (NULL indicates system default)
#define RS_MAGIC_DATABASE NULL

// CORS header values
#define RS_ALLOW_ORIGIN "*"
#define RS_ALLOW_HEADERS "Authorization, Content-Type, Content-Length, ETag"
#define RS_ALLOW_METHODS "GET, PUT, DELETE"

// permissions for newly created files
#define RS_FILE_CREATE_MODE S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP

// whether to change root before serving any files
extern int rs_chroot;
#define RS_CHROOT rs_chroot
extern char *rs_real_storage_root;
extern int rs_real_storage_root_len;
#define RS_REAL_STORAGE_ROOT rs_real_storage_root
#define RS_REAL_STORAGE_ROOT_LEN rs_real_storage_root_len

// log file
extern FILE *rs_log_file;
#define RS_LOG_FILE rs_log_file

// detach option
extern int rs_detach;
#define RS_DETACH rs_detach

// user ID / group ID to set after binding socket
extern uid_t rs_set_uid;
#define RS_SET_UID rs_set_uid
extern gid_t rs_set_gid;
#define RS_SET_GID rs_set_gid

/*
 * A NOTE ABOUT TOKEN SIZES:
 *
 * The token sizes are given in bytes here. These are the true numbers of random
 * bytes that will be generated for each token. The actual tokens transferred on the
 * wire will be encoded though (either hex- or base64-encoded, not sure yet), so will
 * be larger than those number of bytes.
 *
 * Before changing these numbers, keep in mind that while the CSRF token will only
 * be transferred twice (during authorization GET and POST), the bearer token is
 * transferred upon *every* request for data in storage.
 *
 */

// number of bytes to use for CSRF token
#define RS_CSRF_TOKEN_SIZE 32
// number of bytes to use for bearer token
#define RS_BEARER_TOKEN_SIZE 32

// number of bytes for session ID
// (stored in cookie, used to associate request with correct CSRF token)
#define RS_SESSION_ID_SIZE 16
// cookie name for storing the session ID
#define RS_SESSION_ID_NAME "rs_serve_session_id"
#define RS_SESSION_ID_NAME_LEN 19

// using hex-encoded tokens for now, so 2 bytes per byte.
#define TOKEN_BYTESIZE(length) (length * 2)

// maximum number of bytes to POST to /auth
// (must fit the token, the requested scope, the redirect_uri, plus the parameter
//  keys for that and the URI encoding overhead)
#define RS_MAX_AUTH_BODY_SIZE 4096

#endif /* !RS_CONFIG_H */
