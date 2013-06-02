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

  // TODO: check token & scope

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

  struct evhttp_uri *uri = evhttp_uri_parse(evhttp_request_get_uri(request));

  const char *query = evhttp_uri_get_query(uri);

  if(query) {
    // process auth request

    struct evkeyvalq params;

    // parse query into params
    if(evhttp_parse_query_str(query, &params) != 0) {
      perror("evhttp_parse_query_str() failed to parse query string");
      evhttp_uri_free(uri);
      evhttp_send_error(request, HTTP_INTERNAL, NULL);
      return;
    }

    // extract the params we care about
    const char *encoded_redirect_uri = evhttp_find_header(&params, "redirect_uri");
    const char *encoded_scope = evhttp_find_header(&params, "scope");

    // validate required params are given
    if(encoded_redirect_uri == NULL || encoded_scope == NULL) {
      evhttp_send_error(request, HTTP_BADREQUEST, NULL);
      evhttp_uri_free(uri);
      evhttp_clear_headers(&params);
      return;
    }

    // decode redirect uri
    char *redirect_uri = evhttp_uridecode(encoded_redirect_uri, 0, NULL);
    if(redirect_uri == NULL) {
      perror("evhttp_uridecode() failed for redirect_uri");
      evhttp_send_error(request, HTTP_INTERNAL, NULL);
      evhttp_uri_free(uri);
      evhttp_clear_headers(&params);
      return;
    }
    // decode scope
    char *scope = evhttp_uridecode(encoded_scope, 0, NULL);
    if(scope == NULL) {
      perror("evhttp_uridecode() failed for scope");
      evhttp_send_error(request, HTTP_INTERNAL, NULL);
      evhttp_uri_free(uri);
      evhttp_clear_headers(&params);
      free(redirect_uri);
      return;
    }

    // create authorization structure
    struct rs_authorization *authorization = malloc(sizeof(struct rs_authorization));
    if(authorization == NULL) {
      perror("malloc() failed to allocate new authorization");
      evhttp_send_error(request, HTTP_INTERNAL, NULL);
      free(scope);
      free(redirect_uri);
      evhttp_uri_free(uri);
      evhttp_clear_headers(&params);
      return;
    }
    memset(authorization, 0, sizeof(struct rs_authorization));

    // parse scope parameter
    char *scope_saveptr = NULL;
    char *scope_part;
    struct rs_auth_scope *last_scope = NULL;
    for(scope_part = strtok_r(scope, " ", &scope_saveptr);
        scope_part != NULL;
        scope_part = strtok_r(NULL, " ", &scope_saveptr)) {
      authorization->scope = make_auth_scope(scope_part, last_scope);
      if(authorization->scope == NULL) {
        evhttp_send_error(request, HTTP_INTERNAL, NULL);
        free(scope);
        free(redirect_uri);
        free_auth(authorization);
        evhttp_uri_free(uri);
        evhttp_clear_headers(&params);
        return;
      }
      last_scope = authorization->scope;
    }

    ui_prompt_authorization(request, authorization, redirect_uri);

    evhttp_clear_headers(&params);
    free(scope);
    free(redirect_uri);
    free_auth(authorization);
  } else {
    // display list of tokens
    ui_list_authorizations(request);
  }

  evhttp_uri_free(uri);
}

void auth_put(struct evhttp_request *request) {

  // TODO: generate token
  // TODO: store token
  // TODO: redirect somewhere

}

void auth_delete(struct evhttp_request *request) {
}

