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

#define EXTRACT_PATH(request) (evhttp_request_get_uri(request) + RS_STORAGE_PATH_LEN)

void storage_options(struct evhttp_request *request) {
  evhttp_send_reply(request, HTTP_OK, NULL, NULL);
}

void storage_get(struct evhttp_request *request) {
  const char *path = EXTRACT_PATH(request);
  size_t path_len = strlen(path);

  printf("GET STORAGE: %s\n", path);

  if(path[path_len - 1] == '/') {
    // directory request
  } else {
    // file request
  }

  evhttp_send_reply(request, HTTP_OK, NULL, NULL);
}

void storage_put(struct evhttp_request *request) {
  evhttp_send_reply(request, HTTP_OK, NULL, NULL);
}

void storage_delete(struct evhttp_request *request) {
  evhttp_send_reply(request, HTTP_OK, NULL, NULL);
}

void storage_head(struct evhttp_request *request) {
  evhttp_send_reply(request, HTTP_OK, NULL, NULL);
}
