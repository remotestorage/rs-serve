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

void fatal_error_callback(int err) {
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
  default:
    evhttp_send_error(request, HTTP_BADMETHOD, NULL);
  }
}

static void handle_auth_request(struct evhttp_request *request) {
  switch(evhttp_request_get_command(request)) {
  case EVHTTP_REQ_GET: // request authorization or list current authorizations
    auth_get(request);
    break;
  case EVHTTP_REQ_POST: // confirm authorization
    auth_post(request);
    break;
  case EVHTTP_REQ_DELETE: // delete existing authorization
    auth_delete(request);
    break;
  default:
    evhttp_send_error(request, HTTP_BADMETHOD, NULL);
  }
}

static void handle_webfinger_request(struct evhttp_request *request) {
  const char *uri = evhttp_request_get_uri(request);
  const char *resource_query = strstr(uri, "resource=");
  if(resource_query) {
    webfinger_get_resource(request, resource_query + 10);
  } else {
    webfinger_get_hostmeta(request);
  }
}

static void handle_bad_request(struct evhttp_request *request) {
  evhttp_send_error(request, HTTP_BADREQUEST, NULL);
}

void handle_request_callback(struct evhttp_request *request, void *ctx) {

  const char *uri = evhttp_request_get_uri(request);

  if(strncmp(uri, RS_STORAGE_PATH, RS_STORAGE_PATH_LEN) == 0) {
    handle_storage_request(request);
  } else if(strncmp(uri, RS_AUTH_PATH, RS_AUTH_PATH_LEN) == 0) {
    handle_auth_request(request);
  } else if(strncmp(uri, RS_WEBFINGER_PATH, RS_WEBFINGER_PATH_LEN) == 0) {
    handle_webfinger_request(request);
  } else {
    handle_bad_request(request);
  }

  log_request(request);

  if(evhttp_request_is_owned(request)) {
    evhttp_request_free(request);
  }
}
