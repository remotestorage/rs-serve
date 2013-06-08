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

static struct rs_process_info *find_storage_process(uid_t uid) {
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

int dispatch_request(struct evbuffer *buffer, int fd) {
  size_t buf_len = evbuffer_get_length(buffer);
  char *buf = malloc(buf_len + 1);
  if(buf == NULL) {
    log_error("malloc() failed: %s", strerror(errno));
    return -1;
  }
  evbuffer_remove(buffer, buf, buf_len);
  buf[buf_len] = 0; // terminate string.

  // parse everything

  char *saveptr = NULL;
  char *verb = strtok_r(buf, " ", &saveptr);
  char *username = strtok_r(NULL, "/ ", &saveptr);
  char *file_path = strtok_r(NULL, " ", &saveptr);
  char *http_version = strtok_r(NULL, "\n", &saveptr);
  char *rest = strtok_r(NULL, "", &saveptr);

  /* log_debug("Parsed something. Let's see:\n  verb: %s\n  username: %s\n  path: %s\n  http version: %s\n  rest: %s", */
  /*           verb, username, file_path, http_version, rest); */

  uid_t uid = user_get_uid(username);

  if(uid == -1) {
    // TODO: send 404 response here
    log_error("User not found: %s", username);
    free(buf);
    return -1;
  } else if(uid == -2) {
    // TODO: send 500 response here
    log_error("Failed to get uid for user: %s", username);
    free(buf);
    return -1;
  }

  // TODO: add check for UID > MIN_UID
  //   (we don't want to fork storage workers for system users)

  // rewrite first line to only include file path

  int new_len = sprintf(buf, "%s /%s %s\n%s", verb, file_path, http_version, rest);
  log_debug("reallocating to %d bytes", new_len + 1);
  buf = realloc(buf, new_len + 1);
  if(buf == NULL) {
    log_error("BUG: realloc() failed to shrink buffer from %d to %d bytes: %s",
              buf_len + 1, new_len + 1, strerror(errno));
  }
  buf_len = new_len;

  struct rs_process_info *storage_process = find_storage_process(uid);
  send_fd_to_process(storage_process, fd, buf, buf_len + 1);

  free(buf);

  return 0;
}
