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

static uid_t user_uid(const char *username) {
  int buflen = sysconf(_SC_GETPW_R_SIZE_MAX);
  char *buf = malloc(buflen);
  if(buf == NULL) {
    perror("malloc() failed");
    return -2;
  }
  struct passwd *user_entry, *result_ptr;
  user_entry = malloc(sizeof(struct passwd));
  if(user_entry == NULL) {
    perror("malloc() failed");
    free(buf);
    return -2;
  }
  int getpwnam_result = getpwnam_r(username, user_entry, buf, buflen, &result_ptr);
  if(getpwnam_result != 0) {
    log_error("getpwnam_r() failed: %s", strerror(getpwnam_result));
    free(user_entry);
    free(buf);
    return -2;
  }
  if(result_ptr == NULL) {
    log_error("User not found: %s", username);
    free(user_entry);
    free(buf);
    return -1;
  }
  uid_t uid = user_entry->pw_uid;
  free(user_entry);
  free(buf);
  return uid;
}

struct rs_process_info *find_storage_process(uid_t uid) {
  struct rs_process_info *storage_process = process_find_uid(uid);
  if(storage_process == NULL) {
    storage_process = malloc(sizeof(struct rs_process_info));
    if(storage_process == NULL) {
      perror("malloc() failed");
      return NULL;
    }
    storage_process->uid = uid;
    if(start_process(storage_process, storage_main) != 0) {
      free(storage_process);
      return NULL;
    }
  }
  return storage_process;
}

static evhtp_res dispatch_handler_parse_path(evhtp_request_t *request, evhtp_path_t *path, void *arg) {
  char *username = path->match_start;
  char *file_path = path->match_end;

  log_debug("got request for user: \"%s\", path: \"%s\"", username, file_path);

  uid_t uid = user_uid(username);

  if(uid == -1) {
    return EVHTP_RES_NOTFOUND;
  } else if(uid == -2) {
    return EVHTP_RES_SERVERR;
  }

  // TODO: add check for UID > MIN_UID
  //   (we don't want to fork storage workers for system users)

  evhtp_request_pause(request);

  log_debug("Connection socket: %d", request->conn->sock);

  struct rs_process_info *storage_process = find_storage_process(uid);

  htp_method method = evhtp_request_get_method(request);
  char *method_name;
  switch(method) {
  case htp_method_GET: method_name = "GET"; break;
  case htp_method_PUT: method_name = "PUT"; break;
  case htp_method_DELETE: method_name = "DELETE"; break;
  case htp_method_OPTIONS: method_name = "OPTIONS"; break;
  default: log_error("Unexpected method: %d", method); return EVHTP_RES_BADREQ;
  }
  int method_and_path_len = strlen(file_path) + strlen(method_name) + 9 + 1;
  char *method_and_path = malloc(method_and_path_len);

  if(method_and_path == NULL) {
    log_error("malloc() failed: %s", strerror(errno));
    return EVHTP_RES_SERVERR;
  }

  sprintf(method_and_path, "%s %s HTTP/1.1", method_name, file_path);

  send_fd_to_process(storage_process, request->conn->sock,
                     method_and_path, method_and_path_len);

  free(method_and_path);

  event_del(request->conn->resume_ev);

  return EVHTP_RES_PAUSE;
}

void dispatch_handler(evhtp_request_t *request, void *arg) {
  log_error("dispatch_handler called, but it's not supposed to be!");
}

void setup_dispatch_handler(evhtp_t *server, const char *re) {
  evhtp_callback_t *cb = evhtp_set_regex_cb(server, re, dispatch_handler, NULL);
  evhtp_set_hook(&cb->hooks, evhtp_hook_on_path,
                 dispatch_handler_parse_path, NULL);
}
