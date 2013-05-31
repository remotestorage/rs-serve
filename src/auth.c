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

int authorize_request(struct evhttp_request *request) {
  struct evkeyvalq *headers = evhttp_request_get_input_headers(request);
  const char *auth_header = evhttp_find_header(headers, "Authorization");

  if(auth_header && strncmp(auth_header, "Bearer ", 7) != 0) {
    // invalid Authorization header (ie doesn't start with "Bearer ")
    evhttp_send_error(request, HTTP_BADREQUEST, NULL);
    return 0;
  } else if(auth_header && strcmp(auth_header + 7, RS_TOKEN) == 0) {
    // Authorization header present and token correct
    return 1;
  } else {
    // no Authorization header or invalid token
    evhttp_send_error(request, HTTP_UNAUTHORIZED, NULL);
    return 0;
  }
}

void auth_get(struct evhttp_request *request) {
  const char *uri = evhttp_request_get_uri(request);
  const char *redirect_uri_param = strstr(uri, "redirect_uri=");
  if(redirect_uri_param) {
    // FIXME: cleanup this code, it's unreadable.
    const char *redirect_uri_start = redirect_uri_param + 13;
    const char *next_param = strstr(redirect_uri_start, "&");
    size_t redirect_uri_len = next_param ? next_param - redirect_uri_start : strlen(redirect_uri_start);
    char encoded_redirect_uri[redirect_uri_len + 1];
    strncpy(encoded_redirect_uri, redirect_uri_start, redirect_uri_len);
    size_t result_len = 0;
    encoded_redirect_uri[redirect_uri_len] = 0; // why doesn't this happen automatically?
    char *clean_redirect_uri = evhttp_uridecode(encoded_redirect_uri, 0, &result_len);
    clean_redirect_uri = realloc(clean_redirect_uri, result_len + strlen(RS_TOKEN) + 14 + 1);
    if(clean_redirect_uri == NULL) {
      perror("realloc() failed");
      evhttp_send_error(request, HTTP_INTERNAL, NULL);
      return;
    }
    strcat(clean_redirect_uri, "#access_token=");
    strcat(clean_redirect_uri, RS_TOKEN);
    struct evkeyvalq *headers = evhttp_request_get_output_headers(request);
    evhttp_add_header(headers, "Location", clean_redirect_uri);
    evhttp_send_reply(request, HTTP_MOVETEMP, NULL, NULL);
    free(clean_redirect_uri);
  } else {
    evhttp_send_error(request, HTTP_BADREQUEST, NULL);
  }
}

void auth_put(struct evhttp_request *request) {
}

void auth_delete(struct evhttp_request *request) {
}

