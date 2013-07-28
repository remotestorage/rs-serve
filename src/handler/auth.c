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

#define IS_READ(r) (r->method == htp_method_GET || r->method == htp_method_HEAD)

static int match_scope(struct rs_scope *scope, evhtp_request_t *req) {
  const char *file_path = REQUEST_GET_PATH(req);
  log_debug("checking scope, name: %s, write: %d", scope->name, scope->write);
  int scope_len = strlen(scope->name);
  // check path
  if( (strcmp(scope->name, "") == 0) || // root scope
      ((strncmp(file_path + 1, scope->name, scope_len) == 0) && // other scope
       file_path[1 + scope_len] == '/') ) {
    log_debug("path authorized");
    // check mode
    if(scope->write || IS_READ(req)) {
      log_debug("mode authorized");
      return 0;
    }
  }
  return -1;
}

int authorize_request(evhtp_request_t *req) {
  char *username = REQUEST_GET_USER(req);
  const char *auth_header = evhtp_header_find(req->headers_in, "Authorization");
  log_debug("Got auth header: %s", auth_header);
  const char *token;
  if(auth_header) {
    if(strncmp(auth_header, "Bearer ", 7) == 0) {
      token = auth_header + 7;
      log_debug("Got token: %s", token);
      struct rs_authorization *auth = lookup_authorization(username, token);
      if(auth != NULL) {
        log_debug("Got authorization (%p, scopes: %d)", auth, auth->scopes.count);
        struct rs_scope *scope;
        int i;
        for(i=0;i<auth->scopes.count;i++) {
          scope = auth->scopes.ptr[i];
          log_debug("Compare scope %s", scope->name);
          if(match_scope(scope, req) == 0) {
            return 0;
          }
        }
      }
    }
  }
  // special case: public reads on files (not directories) are allowed.
  // nothing else though.
  if(strncmp(REQUEST_GET_PATH(req), "/public/", 8) == 0 && IS_READ(req) &&
     req->uri->path->file != NULL) {
    return 0;
  }
  return -1;
}
