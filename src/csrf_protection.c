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

int csrf_protection_init(struct evhttp_request *request, char **csrf_token_result) {
  char *session_id = generate_token(RS_SESSION_ID_SIZE);
  if(session_id == NULL) {
    return 1;
  }
  char *csrf_token = generate_token(RS_CSRF_TOKEN_SIZE);
  if(csrf_token == NULL) {
    free(session_id);
    return 1;
  }
  struct session_data *session_data = make_session_data(csrf_token);
  if(session_data == NULL || push_session(session_id, session_data) != 0) {
    free(session_id);
    free(csrf_token);
    return 1;
  }

  struct evkeyvalq *input_headers = evhttp_request_get_input_headers(request);

  char *cookie_value = malloc(TOKEN_BYTESIZE(RS_SESSION_ID_SIZE) + RS_SESSION_ID_NAME_LEN + 2);

  sprintf(cookie_value, "%s=%s", RS_SESSION_ID_NAME, session_id);

  evhttp_add_header(input_headers, "Set-Cookie", cookie_value);

  free(cookie_value);

  return 0;
}

int csrf_protection_verify(struct evhttp_request *request, char *csrf_token) {
  if(csrf_token == NULL) {
    return 1;
  }
  
}
