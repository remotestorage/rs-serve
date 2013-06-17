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

#define ADD_CORS_HEADERS(req)                                           \
  ADD_RESP_HEADER(req, "Access-Control-Allow-Origin", RS_ALLOW_ORIGIN); \
  ADD_RESP_HEADER(req, "Access-Control-Allow-Headers", RS_ALLOW_HEADERS); \
  ADD_RESP_HEADER(req, "Access-Control-Allow-Methods", "GET")

char *lrdd_template = NULL;
char *storage_uri_format = NULL;
size_t storage_uri_format_len = 0;

void init_webfinger() {
  lrdd_template = malloc(strlen(RS_SCHEME) + strlen(RS_HOSTNAME) + 40 + 1);
  if(lrdd_template == NULL) {
    log_error("malloc() failed: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }
  sprintf(lrdd_template, "%s://%s/.well-known/webfinger?resource={uri}",
          RS_SCHEME, RS_HOSTNAME);
  storage_uri_format_len = strlen(RS_SCHEME) + strlen(RS_HOSTNAME) + 12;
  storage_uri_format = malloc(storage_uri_format_len + 2 + 1);
  sprintf(storage_uri_format, "%s://%s/storage/%%s", RS_SCHEME, RS_HOSTNAME);
}

static size_t json_writer(char *buf, size_t count, void *arg) {
  return evbuffer_add((struct evbuffer*)arg, buf, count);
}

static int process_resource(const char *resource, char **storage_uri, char **auth_uri) {

  size_t resource_len = strlen(resource);
  char *resource_buf = malloc(resource_len + 1);
  if(resource_buf == NULL) {
    log_error("malloc() failed: %s", strerror(errno));
    return -2;
  }

  memset(resource_buf, 0, resource_len + 1);

  errno = 0;
  if(evhtp_unescape_string((unsigned char**)&resource_buf, (unsigned char*)resource, resource_len) != 0) {
    log_error("unescaping string failed (errno: %s)", strerror(errno));
    return -2;
  }

  // check scheme
  if(strncmp(resource_buf, "acct:", 5) == 0) {

    log_debug("scheme matches");

    // extract local-part
    char *local_part = resource_buf + 5;
    char *ptr;
    for(ptr = local_part; *ptr && *ptr != '@'; ptr++);
    if(*ptr == '@') {
      // '@' found, host starts (otherwise invalid)
      *ptr = 0;
      log_debug("local-part: %s", local_part);
      char *hostname = ptr + 1;
      log_debug("hostname: %s", hostname);
      // check hostname
      if(strcmp(hostname, RS_HOSTNAME) == 0) {
        uid_t uid = user_get_uid(local_part);
        log_debug("got uid: %d (RS_MIN_UID: %d, allowed: %d)", uid,
                  RS_MIN_UID, UID_ALLOWED(uid));
        // check if user is valid
        if(UID_ALLOWED(uid)) {
          *storage_uri = malloc(storage_uri_format_len + strlen(local_part) + 1);
          sprintf(*storage_uri, storage_uri_format, local_part);
          *auth_uri = malloc(RS_AUTH_URI_LEN + strlen(local_part) + 1);
          sprintf(*auth_uri, RS_AUTH_URI, local_part);

          free(resource_buf);
          return 0; // success!
        }
      }
    }

    free(resource_buf);
  }
  return -1;
}

void reject_webfinger(evhtp_request_t *req, void *arg) {
  evhtp_send_reply(req, EVHTP_RES_NOTIMPL);
}

void handle_webfinger(evhtp_request_t *req, void *arg) {
  struct json *json;
  switch(evhtp_request_get_method(req)) {
  case htp_method_GET:
    ADD_CORS_HEADERS(req);
    ADD_RESP_HEADER(req, "Content-Type", "application/json");
    
    json = new_json(json_writer, req->buffer_out);
    json_start_object(json);

    const char *resource;
    if(req->uri->query && (resource = evhtp_kv_find(req->uri->query, "resource"))) {
      char *storage_uri = NULL, *auth_uri = NULL;
      if(process_resource(resource, &storage_uri, &auth_uri) != 0) {
        req->status = EVHTP_RES_NOTFOUND;
      } else {
        json_write_key_val(json, "subject", resource);
        json_write_key(json, "links");
        json_start_array(json);
        json_start_object(json);
        json_write_key_val(json, "rel", "remotestorage");
        json_write_key_val(json, "type", RS_STORAGE_API);
        json_write_key_val(json, "href", storage_uri);
        free(storage_uri);
        json_write_key(json, "properties");
        json_start_object(json);
        json_write_key_val(json, RS_AUTH_METHOD, auth_uri);
        // (begin legacy support [<= remotestorage-00])
        json_write_key_val(json, "auth-method", RS_AUTH_METHOD);
        json_write_key_val(json, "auth-endpoint", auth_uri);
        // (end legacy support)
        free(auth_uri);
        json_end_object(json);  // properties
        json_end_object(json); // link rel=remotestorage]
        json_end_array(json); // links
        req->status = EVHTP_RES_OK;
      }
    } else {
      json_write_key_val(json, "subject", RS_HOSTNAME);
      json_write_key(json, "links");
      json_start_array(json);
      json_start_object(json);
      json_write_key_val(json, "rel", "lrdd");
      json_write_key_val(json, "template", lrdd_template);
      json_end_object(json);
      json_end_array(json);
      req->status = EVHTP_RES_OK;
    }

    json_end_object(json);
    free_json(json);

    break;
  case htp_method_OPTIONS:
    ADD_CORS_HEADERS(req);
    req->status = EVHTP_RES_NOCONTENT;
    break;
  default:
    req->status = EVHTP_RES_METHNALLOWED;
  }

  evhtp_send_reply(req, req->status);
}
