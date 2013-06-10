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

#ifndef RS_SERVE_H
#define RS_SERVE_H

#define _GNU_SOURCE

// standard headers
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <getopt.h>
#include <libgen.h>
#include <pwd.h>
#include <grp.h>
#include <stdarg.h>
#include <search.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/prctl.h>

// libevent headers
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/http.h> // TODO: remove this line!
#include <event2/keyvalq_struct.h>
#include <event2/util.h>

// libevhtp headers
#include <evhtp.h> // TODO: figure out if we can just include htparse.h

// libevent doesn't define this for some reason.
// (TODO: remove these when we don't have event2/http.h anymore)
#define HTTP_UNAUTHORIZED 401
#define HTTP_FORBIDDEN 403

// libmagic headers
#include <magic.h>

// libattr headers

#include <attr/xattr.h>

// rs-serve headers

#include "version.h"
#include "config.h"

#include "common/process.h"
#include "common/log.h"
#include "common/request_response.h"
#include "common/user.h"

#include "handler/dispatch.h"
#include "handler/storage.h"
#include "handler/response.h"

extern magic_t magic_cookie;

/* CONFIG */

void init_config(int argc, char **argv);
void cleanup_config(void);

/* COMMON */

void log_dump_state_start(void);
void log_dump_state_end(void);
void add_cors_headers(struct evkeyvalq *headers);
char *generate_token(size_t bytes);

/* HANDLER */

void fatal_error_callback(int err);

/* WEBFINGER */

void webfinger_get_resource(struct evhttp_request *request, const char *address);
void webfinger_get_hostmeta(struct evhttp_request *request);

/* CSRF PROTECTION */

int csrf_protection_init(struct evhttp_request *request, char **csrf_token_result);
int csrf_protection_verify(struct evhttp_request *request, char *csrf_token);

#endif /* !RS_SERVE_H */
