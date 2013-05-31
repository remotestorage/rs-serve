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

int authorize_request(struct evhttp_request *request) {
  struct evkeyvalq *headers = evhttp_request_get_input_headers(request);
  const char *auth_header = evhttp_find_header(headers, "Authorization");

  if(auth_header && strncmp(auth_header, "Bearer ", 7) != 0) {
    evhttp_send_error(request, HTTP_BADREQUEST, NULL);
    return 0;
  } else if(auth_header && strcmp(auth_header + 7, RS_TOKEN) == 0) {
    return 1;
  } else {
    evhttp_send_error(request, HTTP_UNAUTHORIZED, NULL);
    return 0;
  }
}

void auth_get(struct evhttp_request *request) {}

void auth_put(struct evhttp_request *request) {}

void auth_delete(struct evhttp_request *request) {}

