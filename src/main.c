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

magic_t magic_cookie;

int main(int argc, char **argv) {
  event_set_fatal_callback(fatal_error_callback);
  event_enable_debug_mode();

  magic_cookie = magic_open(MAGIC_MIME_TYPE);

  if(magic_cookie == NULL) {
    perror("magic_open() failed");
    exit(EXIT_FAILURE);
  }

  if(magic_load(magic_cookie, RS_MAGIC_DATABASE) != 0) {
    fprintf(stderr, "magic_load() failed: %s\n", magic_error(magic_cookie));
    exit(EXIT_FAILURE);
  }

  struct event_base *base = event_base_new();

  if(! base) {
    perror("Failed to create event base");
    exit(EXIT_FAILURE);
  }

  struct evhttp *server = evhttp_new(base);

  if(! server) {
    perror("Failed to create server");
    exit(EXIT_FAILURE);
  }

  evhttp_bind_socket(server, RS_ADDRESS, RS_PORT);
  evhttp_set_allowed_methods(server, EVHTTP_REQ_OPTIONS | EVHTTP_REQ_HEAD | EVHTTP_REQ_GET | EVHTTP_REQ_PUT | EVHTTP_REQ_DELETE);
  evhttp_set_gencb(server, handle_request_callback, NULL);

  log_starting(RS_ADDRESS, RS_PORT);

  return event_base_dispatch(base);
}
