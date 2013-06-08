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

// this is part of libevhtp
const char *status_code_to_str(evhtp_res code);

static void setup_response(struct rs_request *request);
static void response_send_status(struct rs_request *request, short status);
static void response_send_header(struct rs_request *request, const char *key, const char *value);
static void response_end_header(struct rs_request *request);
static void response_write(struct rs_request *request, const char *format, ...);
static void response_end(struct rs_request *request);

void send_response_head(struct rs_request *request, short status, struct rs_header *header) {
  setup_response(request);
  response_send_status(request, status);
  for(; header != NULL; header = header->next) {
    response_send_header(request, header->key, header->value);
  }
  response_end_header(request);
}

void send_response_body(struct rs_request *request, struct evbuffer *buf) {
  evbuffer_remove_buffer(buf, request->response_buf, evbuffer_get_length(buf));
  response_end(request);
}

void send_error_response(struct rs_request *request, short status) {
  setup_response(request);

  response_send_status(request, status);
  response_send_header(request, "Content-Type", "text/plain");
  response_send_header(request, "Connection", "close");
  response_end_header(request);
  response_write(request, status_code_to_str(status));
  response_end(request);
}

static void continue_response(evutil_socket_t fd, short events, void *arg) {
  struct rs_request *request = arg;
  int buflen = evbuffer_get_length(request->response_buf);
  if(buflen > 0) {
    char buf[4096];
    int count = evbuffer_remove(request->response_buf, buf, 4096);
    if(write(request->fd, buf, count) < 0) {
      log_error("write() failed: %s", strerror(errno));
    } else {
      log_debug("wrote %d bytes", buflen);
    }
  } else if(request->response_ended) {
    free_request(request);
  }
}

static void setup_response(struct rs_request *request) {
  request->response_buf = evbuffer_new();
  request->write_event = event_new(request->proc->base, request->fd,
                                   EV_WRITE | EV_PERSIST,
                                   continue_response, request);
  event_add(request->write_event, NULL);
}

static void response_send_status(struct rs_request *request, short status) {
  response_write(request, "HTTP/1.1 %d %s\n", status, status_code_to_str(status));
}

static void response_send_header(struct rs_request *request, const char *key, const char *value) {
  response_write(request, "%s: %s\n", key, value);
}

static void response_end_header(struct rs_request *request) {
  response_write(request, "\n");
}

static void response_write(struct rs_request *request, const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  evbuffer_add_vprintf(request->response_buf, format, ap);
  va_end(ap);
}

static void response_end(struct rs_request *request) {
  request->response_ended = 1;
}
