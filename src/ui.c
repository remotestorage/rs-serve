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

static void write_html_head(struct evbuffer *buf, const char *title) {
  evbuffer_add_printf(buf, "<!DOCTYPE html>\n<html>\n<head>\n<title>%s</title>\n</head>\n<body>\n", title);
}

static void write_html_tail(struct evbuffer *buf) {
  evbuffer_add_printf(buf, "</body>\n</html>\n");
}

void ui_prompt_authorization(struct evhttp_request *request, struct rs_authorization *auth, const char *redirect_uri) {
  struct evbuffer *buf = evbuffer_new();
  if(buf == NULL) {
    evhttp_send_error(request, HTTP_INTERNAL, NULL);
    return;
  }
  write_html_head(buf, "Authorize request");

  evbuffer_add_printf(buf, "<h1>Authorize request:</h1>\n");
  evbuffer_add_printf(buf, "<table>\n<thead>\n");
  evbuffer_add_printf(buf, "<tr><th>Scope</th><th>Mode</th></tr>\n");
  evbuffer_add_printf(buf, "</thead>\n<tbody>\n");
  struct rs_auth_scope *scope;
  for(scope = auth->scope;
      scope != NULL;
      scope = scope->next) {
    char *scope_name = scope->name;
    if(*scope_name == 0) {
      scope_name = "root";
    }
    evbuffer_add_printf(buf, "<tr><td>%s</td><td>%s</td></tr>\n",
                        scope_name, scope->write ? "read-write" : "read-only");
  }
  evbuffer_add_printf(buf, "</tbody>\n</table>\n");
  
  write_html_tail(buf);
  evhttp_send_reply(request, HTTP_OK, NULL, buf);
  evbuffer_free(buf);
}

void ui_list_authorizations(struct evhttp_request *request) {
  struct evbuffer *buf = evbuffer_new();
  if(buf == NULL) {
    evhttp_send_error(request, HTTP_INTERNAL, NULL);
    return;
  }
  write_html_head(buf, "Authorizations");
  evbuffer_add_printf(buf, "<h1>TODO: list authorizations</h1>\n");
  write_html_tail(buf);
  evhttp_send_reply(request, HTTP_OK, NULL, buf);
  evbuffer_free(buf);
}

