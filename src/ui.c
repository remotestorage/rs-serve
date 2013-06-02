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

static void write_html_escaped_attr(struct evbuffer *buf, const char *value) {
  const char *valuep;
  for(valuep = value; *valuep != 0; valuep++) {
    if(*valuep == '"') {
      evbuffer_add(buf, "&quot;", 6);
    } else {
      evbuffer_add(buf, valuep, 1);
    }
  }
}

static void write_html_escaped(struct evbuffer *buf, const char *value) {
  const char *valuep;
  for(valuep = value; *valuep != 0; valuep++) {
    switch(*valuep) {
    case '&':
      evbuffer_add(buf, "&amp;", 5);
      break;
    case '>':
      evbuffer_add(buf, "&gt;", 4);
      break;
    case '<':
      evbuffer_add(buf, "&lt;", 4);
      break;
    default:
      evbuffer_add(buf, valuep, 1);
    }
  }
}

static void write_html_input(struct evbuffer *buf, const char *type, const char *name, const char *value) {
  evbuffer_add_printf(buf, "<input");
  if(type) {
    evbuffer_add_printf(buf, " type=\"%s\"", type);
  }
  if(name) {
    evbuffer_add_printf(buf, " name=\"%s\"", name);
  }
  if(value) {
    evbuffer_add_printf(buf, " value=\"");
    write_html_escaped_attr(buf, value);
    evbuffer_add_printf(buf, "\"");
  }
  evbuffer_add_printf(buf,">\n");
}

void ui_prompt_authorization(struct evhttp_request *request, struct rs_authorization *auth, const char *redirect_uri, const char *scope_string, const char *csrf_token) {
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
    evbuffer_add_printf(buf, "<tr><td>");
    write_html_escaped(buf, scope_name);
    evbuffer_add_printf(buf, "</td><td>%s</td></tr>\n",
                        scope->write ? "read-write" : "read-only");
  }
  evbuffer_add_printf(buf, "</tbody>\n</table>\n");

  evbuffer_add_printf(buf, "<form action=\"%s\" method=\"POST\">\n", RS_AUTH_PATH);

  write_html_input(buf, "hidden", "scope", scope_string);
  write_html_input(buf, "hidden", "redirect_uri", redirect_uri);
  write_html_input(buf, "hidden", "csrf_token", csrf_token);
  write_html_input(buf, "submit", NULL, "Confirm");

  evbuffer_add_printf(buf, "</form>\n");
  
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

