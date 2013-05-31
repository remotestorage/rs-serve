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

static void fatal_error_callback(int err) {
  fprintf(stderr, "A fatal error occured (code: %d)\nExiting.\n", err);
  exit(EXIT_FAILURE);
}

static void handle_storage_request(struct evhttp_request *request) {
  switch(evhttp_request_get_command(request)) {
  case EVHTTP_REQ_OPTIONS:
    storage_options(request);
    break;
  case EVHTTP_REQ_GET:
    storage_get(request, 1);
    break;
  case EVHTTP_REQ_PUT:
    storage_put(request);
    break;
  case EVHTTP_REQ_DELETE:
    storage_delete(request);
    break;
  case EVHTTP_REQ_HEAD:
    storage_get(request, 0);
    break;
  }
}

static void handle_auth_request(struct evhttp_request *request) {
  switch(evhttp_request_get_command(request)) {
  case EVHTTP_REQ_GET: // request authorization or list current authorizations
    auth_get(request);
    break;
  case EVHTTP_REQ_PUT: // confirm authorization
    auth_put(request);
    break;
  case EVHTTP_REQ_DELETE: // delete existing authorization
    auth_delete(request);
    break;
  default:
    evhttp_send_error(request, HTTP_BADMETHOD, NULL);
  }
}

static void handle_bad_request(struct evhttp_request *request) {
  evhttp_send_error(request, HTTP_BADREQUEST, NULL);
}

static void handle_request_callback(struct evhttp_request *request, void *ctx) {

  const char *uri = evhttp_request_get_uri(request);

  if(strncmp(uri, RS_STORAGE_PATH, RS_STORAGE_PATH_LEN) == 0) {
    handle_storage_request(request);
  } else if(strncmp(uri, RS_AUTH_PATH, RS_AUTH_PATH_LEN) == 0) {
    handle_auth_request(request);
  } else {
    handle_bad_request(request);
  }

  log_request(request);
}

int main(int argc, char **argv) {
  event_set_fatal_callback(fatal_error_callback);
  event_enable_debug_mode();

  struct event_base *base = event_base_new();

  if(! base) {
    perror("Failed to create event base");
    return 1;
  }

  struct evhttp *server = evhttp_new(base);

  if(! server) {
    perror("Failed to create server");
    return 2;
  }

  evhttp_bind_socket(server, RS_ADDRESS, RS_PORT);
  evhttp_set_allowed_methods(server, EVHTTP_REQ_OPTIONS | EVHTTP_REQ_HEAD | EVHTTP_REQ_GET | EVHTTP_REQ_PUT | EVHTTP_REQ_DELETE);
  evhttp_set_gencb(server, handle_request_callback, NULL);

  log_starting(RS_ADDRESS, RS_PORT);

  return event_base_dispatch(base);
}
