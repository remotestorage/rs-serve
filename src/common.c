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

char *time_now() {
  static char timestamp[100]; // FIXME: not threadsafe!
  time_t t = time(NULL);
  struct tm *tmp = localtime(&t);
  strftime(timestamp, 99, "%F %T %z", tmp);
  return timestamp;
}

void log_starting(const char *address, int port) {
  if(! address) {
    address = "0.0.0.0";
  }
  printf("[%s] Server listening on %s:%d\n", time_now(), address, port);
}

void log_request(struct evhttp_request *request) {
  char *method;
  switch(evhttp_request_get_command(request)) {
  case EVHTTP_REQ_GET: method = "GET"; break;
  case EVHTTP_REQ_POST: method = "POST"; break;
  case EVHTTP_REQ_HEAD: method = "HEAD"; break;
  case EVHTTP_REQ_PUT: method = "PUT"; break;
  case EVHTTP_REQ_OPTIONS: method = "OPTIONS"; break;
  case EVHTTP_REQ_DELETE: method = "DELETE"; break;
  default: method = "(unknown)"; break;
  }
  
  printf("[%s] %s %s -> %d\n", time_now(), method,
         evhttp_request_get_uri(request),
         evhttp_request_get_response_code(request));
} 

void add_cors_headers(struct evkeyvalq *headers) {
  evhttp_add_header(headers, "Access-Control-Allow-Origin", RS_ALLOW_ORIGIN);
  evhttp_add_header(headers, "Access-Control-Allow-Headers", RS_ALLOW_HEADERS);
  evhttp_add_header(headers, "Access-Control-Allow-Methods", RS_ALLOW_METHODS);
}
