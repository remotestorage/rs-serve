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
  log_debug("PARSE-QUERY(%s)", query);
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
    // (presence and validity of CSRF token is checked in csrf_protection_verify)
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
  struct evbuffer *input_buf = evhttp_request_get_input_buffer(request);
  char *body = malloc(RS_MAX_AUTH_BODY_SIZE + 1);
  if(body == NULL) {
    perror("failed to allocate memory for POST body");
    evhttp_send_error(request, HTTP_INTERNAL, NULL);
    return;
  }
  int body_bytes = evbuffer_remove(input_buf, body, RS_MAX_AUTH_BODY_SIZE);
  if(body_bytes == -1) {
    fprintf(stderr, "Failed to read <= %d bytes from input buffer\n", RS_MAX_AUTH_BODY_SIZE);
    evhttp_send_error(request, HTTP_INTERNAL, NULL);
    free(body);
    return;
  }
  body[body_bytes] = 0;

  char *redirect_uri = NULL, *scope_string = NULL, *csrf_token = NULL;

  if(extract_auth_params(body, &redirect_uri, &scope_string, &csrf_token) != 0) {
    evhttp_send_error(request, HTTP_BADREQUEST, NULL);
    free(body);
    return;
  }
  free(body);

  log_debug("PARAMS: redirect_uri=\"%s\", scope=\"%s\", csrf_token=\"%s\"",
            redirect_uri, scope_string, csrf_token);

  if(csrf_protection_verify(request, csrf_token) != 0) {
    evhttp_send_error(request, HTTP_BADREQUEST, "invalid CSRF token");
    free(redirect_uri);
    free(scope_string);
    free(csrf_token);
    return;
  }

  free(csrf_token);

  char *bearer_token = generate_token(RS_BEARER_TOKEN_SIZE);

  if(bearer_token == NULL) {
    free(redirect_uri);
    free(scope_string);
    evhttp_send_error(request, HTTP_INTERNAL, NULL);
    return;
  }

  if(store_authorization(bearer_token, scope_string) != 0) {
    free(bearer_token);
    free(redirect_uri);
    free(scope_string);
    evhttp_send_error(request, HTTP_INTERNAL, NULL);
    return;
  }
  free(scope_string);

  // construct redirect uri
  int fragment_size = TOKEN_BYTESIZE(RS_BEARER_TOKEN_SIZE) + 13;
  char *fragment = malloc(fragment_size + 1);
  if(fragment == NULL) {
    perror("failed to allocate memory for fragment");
    free(redirect_uri);
    evhttp_send_error(request, HTTP_INTERNAL, NULL);
    return;
  }
  struct evhttp_uri *uri = evhttp_uri_parse(redirect_uri);
  sprintf(fragment, "access_token=%s", bearer_token);
  evhttp_uri_set_fragment(uri, fragment);
  free(fragment);
  free(redirect_uri);

  int uri_buf_len = strlen(redirect_uri) + fragment_size + 2;
  char *uri_buf = malloc(uri_buf_len);
  if(uri_buf == NULL) {
    perror("failed to allocate memory for redirect URI buffer");
    evhttp_uri_free(uri);
    evhttp_send_error(request, HTTP_INTERNAL, NULL);
    return;
  }

  if(evhttp_uri_join(uri, uri_buf, uri_buf_len) == NULL) {
    perror("failed to join redirect URI");
    evhttp_uri_free(uri);
    evhttp_send_error(request, HTTP_INTERNAL, NULL);
    return;
  }
  evhttp_uri_free(uri);

  // set Location header
  struct evkeyvalq *output_headers = evhttp_request_get_output_headers(request);
  evhttp_add_header(output_headers, "Location", uri_buf);
  free(uri_buf);

  // redirect
  evhttp_send_reply(request, HTTP_MOVETEMP, NULL, NULL);
}

void auth_delete(struct evhttp_request *request) {
}
