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
#include <sys/un.h>

// libevent headers
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/util.h>

// libevhtp headers
#include <evhtp.h> // TODO: figure out if we can just include htparse.h

// libmagic headers
#include <magic.h>

// libattr headers

#include <attr/xattr.h>

// rs-serve headers

#include "version.h"
#include "config.h"

#include "common/log.h"
#include "common/user.h"
#include "common/auth.h"
#include "common/json.h"

#include "handler/auth.h"
#include "handler/storage.h"
#include "handler/webfinger.h"

#define UID_ALLOWED(uid) ( (uid) >= RS_MIN_UID )

extern magic_t magic_cookie;

/* CONFIG */

void init_config(int argc, char **argv);
void cleanup_config(void);

/* COMMON */

void log_dump_state_start(void);
void log_dump_state_end(void);


// don't use this for key/val that should be copied.
#define ADD_RESP_HEADER(req, key, val)                                  \
  evhtp_headers_add_header(req->headers_out, evhtp_header_new(key, val, 0, 0))

#define ADD_RESP_HEADER_CP(req, key, val)                                  \
  evhtp_headers_add_header(req->headers_out, evhtp_header_new(key, val, 0, 1))

#endif /* !RS_SERVE_H */
