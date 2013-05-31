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

// standard headers
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

// libevent headers
#include <event2/event.h>
#include <event2/http.h>

// rs-serve headers

#include "config.h"

/* COMMON */

void log_starting(const char *address, int port);
void log_request(struct evhttp_request *request);

/* STORAGE */

void storage_options(struct evhttp_request *request);
void storage_get(struct evhttp_request *request);
void storage_put(struct evhttp_request *request);
void storage_delete(struct evhttp_request *request);
void storage_head(struct evhttp_request *request);

/* AUTH */

void auth_get(struct evhttp_request *request);
void auth_put(struct evhttp_request *request);
void auth_delete(struct evhttp_request *request);

#endif
