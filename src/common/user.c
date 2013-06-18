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

#define USER_NOT_FOUND (struct passwd*)-1

uid_t user_get_uid(const char *username) {
  char *bufptr;
  struct passwd *user_entry = user_get_entry(username, &bufptr);
  if(user_entry == USER_NOT_FOUND) {
    return -1; // not found
  } else if(user_entry == NULL) {
    return -2; // error occured
  } else {
    uid_t uid = user_entry->pw_uid;
    free(user_entry);
    free(bufptr);
    return uid;
  }
}

struct passwd *user_get_entry(const char *username, char **bufptr) {
  if(username == NULL) {
    return USER_NOT_FOUND;
  }
  int buflen = sysconf(_SC_GETPW_R_SIZE_MAX);
  char *buf = malloc(buflen);
  if(buf == NULL) {
    log_error("malloc() failed: %s", strerror(errno));
    return NULL;
  }
  struct passwd *user_entry, *result_ptr;
  user_entry = malloc(sizeof(struct passwd));
  if(user_entry == NULL) {
    log_error("malloc() failed: %s", strerror(errno));
    free(buf);
    return NULL;
  }
  int getpwnam_result = getpwnam_r(username, user_entry, buf, buflen, &result_ptr);
  if(getpwnam_result != 0) {
    log_error("getpwnam_r() failed: %s", strerror(getpwnam_result));
    free(user_entry);
    free(buf);
    return NULL;
  }
  if(result_ptr == NULL) {
    log_error("User not found: %s", username);
    free(user_entry);
    free(buf);
    return USER_NOT_FOUND;
  }
  *bufptr = buf;
  return user_entry;
}
