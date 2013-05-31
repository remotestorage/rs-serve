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

/*
{
      href: <storage_root>,
      rel: "remotestorage",
      type: <storage_api>,
      properties: {
        'auth-method': "http://tools.ietf.org/html/rfc6749#section-4.2",
        'auth-endpoint': <auth_endpoint>
      }
    }    
*/

// address is ignored for now...
void webfinger_get_resource(struct evhttp_request *request, const char *address) {
  struct evkeyvalq *headers = evhttp_request_get_output_headers(request);
  add_cors_headers(headers);
  evhttp_add_header(headers, "Content-Type", "application/json");
  struct evbuffer *buf = evbuffer_new();
  evbuffer_add_printf(buf, "{\"links\":[{");
  evbuffer_add_printf(buf, "\"rel\":\"remotestorage\",");
  evbuffer_add_printf(buf, "\"href\":\"%s://%s:%d%s\",", RS_SCHEME, RS_HOSTNAME, RS_PORT, RS_STORAGE_PATH);
  evbuffer_add_printf(buf, "\"type\":\"%s\",", RS_STORAGE_API);
  evbuffer_add_printf(buf, "\"properties\":{");
  evbuffer_add_printf(buf, "\"auth-method\":\"%s\",", RS_AUTH_METHOD);
  evbuffer_add_printf(buf, "\"auth-endpoint\":\"%s://%s:%d%s\"", RS_SCHEME, RS_HOSTNAME, RS_PORT, RS_AUTH_PATH);
  evbuffer_add_printf(buf, "}}]}");
  evhttp_send_reply(request, HTTP_OK, NULL, buf);
  evbuffer_free(buf);
}

void webfinger_get_hostmeta(struct evhttp_request *request) {
  struct evkeyvalq *headers = evhttp_request_get_output_headers(request);
  add_cors_headers(headers);
  evhttp_add_header(headers, "Content-Type", "application/json");
  struct evbuffer *buf = evbuffer_new();
  evbuffer_add_printf(buf, "{\"links\":[{\"rel\":\"lrdd\",\"template\":\"%s://%s:%d%s?resource={uri}\"}]}", RS_SCHEME, RS_HOSTNAME, RS_PORT, RS_WEBFINGER_PATH);
  evhttp_send_reply(request, HTTP_OK, NULL, buf);
  evbuffer_free(buf);
}
