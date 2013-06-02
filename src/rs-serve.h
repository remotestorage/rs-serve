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

#define _XOPEN_SOURCE_EXTENDED

// standard headers
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <getopt.h>
#include <libgen.h>
#include <pwd.h>
#include <grp.h>

// libevent headers
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/http.h>
#include <event2/keyvalq_struct.h>

// libevent doesn't define this for some reason.
#define HTTP_UNAUTHORIZED 401

// libmagic headers
#include <magic.h>

// rs-serve headers

#include "version.h"
#include "config.h"
#include "auth_struct.h"

extern magic_t magic_cookie;

/* CONFIG */

void init_config(int argc, char **argv);
void cleanup_config(void);

/* COMMON */

void log_starting(void);
void log_request(struct evhttp_request *request);
void add_cors_headers(struct evkeyvalq *headers);

/* HANDLER */

void fatal_error_callback(int err);
void handle_request_callback(struct evhttp_request *request, void *ctx);

/* STORAGE */

void storage_options(struct evhttp_request *request);
void storage_get(struct evhttp_request *request, int sendbody);
void storage_put(struct evhttp_request *request);
void storage_delete(struct evhttp_request *request);

/* AUTH */

int authorize_request(struct evhttp_request *request);
void auth_get(struct evhttp_request *request);
void auth_put(struct evhttp_request *request);
void auth_delete(struct evhttp_request *request);

/* WEBFINGER */

void webfinger_get_resource(struct evhttp_request *request, const char *address);
void webfinger_get_hostmeta(struct evhttp_request *request);

/* UI */

void ui_prompt_authorization(struct evhttp_request *request, struct rs_authorization *authorization, const char *redirect_uri);
void ui_list_authorizations(struct evhttp_request *request);

#endif /* !RS_SERVE_H */
