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

static int extract_auth_params(const char *query, char **redirect_uri, char **scope_string, char **csrf_token) {
  struct evkeyvalq params;
  // parse query into params
  if(evhttp_parse_query_str(query, &params) != 0) {
    perror("evhttp_parse_query_str() failed to parse query string");
    return 1;
  }
  // extract the params we care about
  const char *encoded_redirect_uri = evhttp_find_header(&params, "redirect_uri");
  const char *encoded_scope = evhttp_find_header(&params, "scope");
  const char *encoded_csrf_token = NULL;
  if(csrf_token) { // (optional)
    encoded_csrf_token = evhttp_find_header(&params, "csrf_token");
  }
  // validate required params are given
  if(encoded_redirect_uri == NULL || encoded_scope == NULL) {
    evhttp_clear_headers(&params);
    return 1;
  }
  // decode redirect uri
  *redirect_uri = evhttp_uridecode(encoded_redirect_uri, 0, NULL);
  if(*redirect_uri == NULL) {
    perror("evhttp_uridecode() failed for redirect_uri");
    evhttp_clear_headers(&params);
    return 1;
  }
  // decode scope
  *scope_string = evhttp_uridecode(encoded_scope, 0, NULL);
  if(*scope_string == NULL) {
    perror("evhttp_uridecode() failed for scope");
    evhttp_clear_headers(&params);
    free(*redirect_uri);
    return 1;
  }
  // decode csrf token (optional)
  if(csrf_token && encoded_csrf_token) {
    *csrf_token = evhttp_uridecode(encoded_csrf_token, 0, NULL);
    // (presence of CSRF token is checked in csrf_protection_verify)
  }
  evhttp_clear_headers(&params);
  return 0;
}

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

  // TODO: add CSRF protection

  struct evhttp_uri *uri = evhttp_uri_parse(evhttp_request_get_uri(request));

  const char *query = evhttp_uri_get_query(uri);

  if(query) {
    // process auth request

    char *redirect_uri = NULL, *scope_string = NULL;
    if(extract_auth_params(query, &redirect_uri, &scope_string, NULL) != 0) {
      evhttp_uri_free(uri);
      evhttp_send_error(request, HTTP_BADREQUEST, NULL);
      return;
    }

    struct rs_authorization *authorization = make_authorization(scope_string);

    if(authorization == NULL) {
      evhttp_uri_free(uri);
      evhttp_send_error(request, HTTP_INTERNAL, NULL);
      return;
    }


    char *csrf_token = NULL;
    if(csrf_protection_init(request, &csrf_token) != 0) {
      evhttp_uri_free(uri);
      evhttp_send_error(request, HTTP_INTERNAL, NULL);
      return;
    }

    ui_prompt_authorization(request, authorization, redirect_uri, scope_string, csrf_token);

    free(scope_string);
    free(redirect_uri);
    free_auth(authorization);
  } else {
    // display list of tokens
    ui_list_authorizations(request);
  }

  evhttp_uri_free(uri);
}

void auth_post(struct evhttp_request *request) {

  /* char *redirect_uri, *scope_string, *csrf_token; */

  /* if(csrf_protection_verify(request, csrf_token) != 0) { */
  /*   evhttp_send_error(request, HTTP_BADREQUEST, "invalide CSRF token"); */
  /*   return; */
  /* } */

  // TODO: parse params
  // TODO: generate token
  // TODO: store token
  // TODO: redirect somewhere

}

void auth_delete(struct evhttp_request *request) {
}
