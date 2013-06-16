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

uid_t user_get_uid(const char *username) {
  if(username == NULL) {
    return -1;
  }
  int buflen = sysconf(_SC_GETPW_R_SIZE_MAX);
  char *buf = malloc(buflen);
  if(buf == NULL) {
    log_error("malloc() failed: %s", strerror(errno));
    return -2;
  }
  struct passwd *user_entry, *result_ptr;
  user_entry = malloc(sizeof(struct passwd));
  if(user_entry == NULL) {
    log_error("malloc() failed: %s", strerror(errno));
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

