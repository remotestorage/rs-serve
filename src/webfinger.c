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

#if RS_INDENT_WEBFINGER
# define WEBFINGER_TEMPLATE "{\n  \"links\":[{\n    \"rel\":\"remotestorage\",\n    \"href\":\"%s://%s:%d%s\",\n    \"type\":\"%s\",\n    \"properties\":{\n      \"auth-method\":\"%s\",\n      \"auth-endpoint\":\"%s://%s:%d%s\"\n    }\n  }]\n}\n"
#else
# define WEBFINGER_TEMPLATE "{\"links\":[{\"rel\":\"remotestorage\",\"href\":\"%s://%s:%d%s\",\"type\":\"%s\",\"properties\":{\"auth-method\":\"%s\",\"auth-endpoint\":\"%s://%s:%d%s\"}}]}"
#endif

static int is_valid_user(const char *name) {
  int buflen = sysconf(_SC_GETPW_R_SIZE_MAX);
  if(buflen == -1) {
    fprintf(stderr, "ERROR: Failed to get sysconf value for bufsize suitable for getpwnam_r. There is nothing I can do about it.\n");
    return -2;
  }
  char *buf = malloc(buflen);
  if(buf == NULL) {
    perror("Failed to allocate memory for buffer passed to getpwnam_r");
    return -2;
  }
  struct passwd *user_entry = malloc(sizeof(struct passwd));
  if(user_entry == NULL) {
    perror("Failed to allocate memory for user_entry");
    return -2;
  }
  struct passwd *user_entry_result_ptr = NULL;
  memset(user_entry, 0, sizeof(user_entry));
  int getpwnam_result = getpwnam_r(name, user_entry, buf, buflen,
                                   &user_entry_result_ptr);
  if(getpwnam_result != 0) {
    fprintf(stderr, "getpwnam_r() failed: %s\n", strerror(getpwnam_result));
    return -2;
  }
  int result = -1;
  if(user_entry_result_ptr != NULL) {
    log_debug("Got user entry for user %s", name);
    if(user_entry->pw_uid >= RS_SERVE_HOMES_MIN_UID) {
      // UID allowed
      result = 0;
    } else {
      // UID not allowed
      log_debug("User %s not allowed", name);
      result = -1;
    }
  } else {
    log_debug("Failed to get entry for user %s", name);
  }
  free(buf);
  return result;
}

void webfinger_get_resource(struct evhttp_request *request, const char *address) {
  struct evkeyvalq *headers = evhttp_request_get_output_headers(request);
  add_cors_headers(headers);

  char *storage_root_path = RS_STORAGE_PATH;

  if(RS_SERVE_HOMES) {
    char *address_copy = strdup(address);
    char *address_saveptr = NULL;
    char *name = strtok_r(address_copy, "@", &address_saveptr);
    char *host = strtok_r(NULL, "", &address_saveptr);

    if(strcmp(RS_HOSTNAME, host) != 0) {
      log_debug("Invalid host: got %s, expected %s", host, RS_HOSTNAME);
      free(address_copy);
      evhttp_send_error(request, HTTP_NOTFOUND, NULL);
      return;
    }

    switch(is_valid_user(name)) {
    case 0:
      // all right!
      break;
    case -1:
      // no such user (or not allowed based on UID)
      free(address_copy);
      evhttp_send_error(request, HTTP_NOTFOUND, NULL);
      return;
    case -2:
      // memory allocation or something else went wrong. Not the client's fault.
      free(address_copy);
      evhttp_send_error(request, HTTP_INTERNAL, NULL);
      return;
    }

    storage_root_path = malloc(RS_STORAGE_PATH_LEN + strlen(name) + 2);
    if(storage_root_path == NULL) {
      perror("failed to allocate memory for storage_root_path");
      free(address_copy);
      evhttp_send_error(request, HTTP_INTERNAL, NULL);
      return;
    }

    sprintf(storage_root_path, "%s/%s", RS_STORAGE_PATH, name);
    free(address_copy);
  }
  evhttp_add_header(headers, "Content-Type", "application/json");
  struct evbuffer *buf = evbuffer_new();
  evbuffer_add_printf(buf, WEBFINGER_TEMPLATE,
                      // href
                      RS_SCHEME, RS_HOSTNAME, RS_PORT, storage_root_path,
                      // type,        properties.auth-method
                      RS_STORAGE_API, RS_AUTH_METHOD,
                      // properties.auth-endpoint
                      RS_SCHEME, RS_HOSTNAME, RS_PORT, RS_AUTH_PATH);
  evhttp_send_reply(request, HTTP_OK, NULL, buf);
  evbuffer_free(buf);

  if(RS_SERVE_HOMES) {
    free(storage_root_path);
  }
}

void webfinger_get_hostmeta(struct evhttp_request *request) {
  struct evkeyvalq *headers = evhttp_request_get_output_headers(request);
  add_cors_headers(headers);
  evhttp_add_header(headers, "Content-Type", "application/json");
  struct evbuffer *buf = evbuffer_new();
  evbuffer_add_printf(buf, "{\"links\":[{\"rel\":\"lrdd\",\"template\":\"%s://%s:%d%s?resource={uri}\"}]}", RS_SCHEME, RS_HOSTNAME, RS_PORT, RS_WEBFINGER_PATH);
  evhttp_send_reply(request, HTTP_OK, NULL, buf);
  evbuffer_free(buf);
}

#undef WEBFINGER_TEMPLATE

