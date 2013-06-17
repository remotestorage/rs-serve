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

static void add_cors_headers(evhtp_request_t *req) {
  ADD_RESP_HEADER(req, "Access-Control-Allow-Origin", RS_ALLOW_ORIGIN);
  ADD_RESP_HEADER(req, "Access-Control-Allow-Headers", RS_ALLOW_HEADERS);
  ADD_RESP_HEADER(req, "Access-Control-Allow-Methods", RS_ALLOW_METHODS);
}

static void verify_user(evhtp_request_t *req) {
  char *username = REQUEST_GET_USER(req);
  uid_t uid = user_get_uid(username);
  if(uid == -1) {
    req->status = EVHTP_RES_NOTFOUND;
  } else if(uid == -2) {
    req->status = EVHTP_RES_SERVERR;
  } else if(! UID_ALLOWED(uid)) {
    log_info("User not allowed: %s (uid: %ld)", username, uid);
    req->status = EVHTP_RES_NOTFOUND;
  } else {
    log_debug("User found: %s (uid: %ld)", username, uid);
  }
}

void dispatch_storage(evhtp_request_t *req, void *arg) {
  req->status = 0;

  do {

    add_cors_headers(req);

    // validate user
    verify_user(req);

    log_info("status after verify_user(): %d", req->status);

    if(req->status) break; // bail

    // authorize request
    if(req->method != htp_method_OPTIONS) {
      int auth_result = authorize_request(req);
      if(auth_result == 0) {
        log_debug("Request authorized.");
      } else if(auth_result == -1) {
        log_info("Request NOT authorized.");
        req->status = EVHTP_RES_UNAUTH;
      } else if(auth_result == -2) {
        log_error("An error occured while authorizing request.");    
        req->status = EVHTP_RES_SERVERR; 
      }
    }

    log_info("status after authorize_request(): %d", req->status);

    if(req->status) break; // bail

    // dispatch to storage handler
    if(req->status == 0) {
      switch(req->method) {
      case htp_method_OPTIONS:
        req->status = EVHTP_RES_NOCONTENT;
        break;
      case htp_method_GET:
        req->status = storage_handle_get(req);
        break;
      case htp_method_HEAD:
        req->status = storage_handle_head(req);
        break;
      case htp_method_PUT:
        req->status = storage_handle_put(req);
        break;
      case htp_method_DELETE:
        req->status = storage_handle_delete(req);
        break;
      default:
        req->status = EVHTP_RES_METHNALLOWED;
      }
    }

  } while(0);

  // send reply, if status was set
  if(req->status) {
    evhtp_send_reply(req, req->status);
  }
}
