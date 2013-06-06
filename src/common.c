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

static char *time_now() {
  static char timestamp[100]; // FIXME: not threadsafe!
  time_t t = time(NULL);
  struct tm *tmp = localtime(&t);
  strftime(timestamp, 99, "%F %T %z", tmp);
  return timestamp;
}

void log_starting() {
  const char *address = RS_ADDRESS;
  if(! address) {
    address = "0.0.0.0";
  }
  fprintf(RS_LOG_FILE, "[%s] Serving from \"%s\" on %s:%d\n", time_now(), RS_STORAGE_ROOT, address, RS_PORT);
  fflush(RS_LOG_FILE);
}

void log_request(struct evhttp_request *request) {
  char *method;
  switch(evhttp_request_get_command(request)) {
  case EVHTTP_REQ_GET: method = "GET"; break;
  case EVHTTP_REQ_POST: method = "POST"; break;
  case EVHTTP_REQ_HEAD: method = "HEAD"; break;
  case EVHTTP_REQ_PUT: method = "PUT"; break;
  case EVHTTP_REQ_OPTIONS: method = "OPTIONS"; break;
  case EVHTTP_REQ_DELETE: method = "DELETE"; break;
  default: method = "(unknown)"; break;
  }
  
  fprintf(RS_LOG_FILE, "[%s] %s %s -> %d\n", time_now(), method,
         evhttp_request_get_uri(request),
         evhttp_request_get_response_code(request));
  fflush(RS_LOG_FILE);
}

void log_warn(char *format, ...) {
  va_list ap;
  va_start(ap, format);
  char *timestamp = time_now();
  char new_format[strlen(timestamp) +
                  strlen(format) +
                  6 + // some space for PID
                  + 14];
  sprintf(new_format, "[%d] [%s] [WARN] %s\n", getpid(), timestamp, format);
  vfprintf(RS_LOG_FILE, new_format, ap);
  va_end(ap);
}

void log_info(char *format, ...) {
  va_list ap;
  va_start(ap, format);
  char *timestamp = time_now();
  char new_format[strlen(timestamp) +
                  strlen(format) +
                  6 + // some space for PID
                  + 14];
  sprintf(new_format, "[%d] [%s] [INFO] %s\n", getpid(), timestamp, format);
  vfprintf(RS_LOG_FILE, new_format, ap);
  va_end(ap);
}

void log_error(char *format, ...) {
  va_list ap;
  va_start(ap, format);
  char *timestamp = time_now();
  char new_format[strlen(timestamp) +
                  strlen(format) +
                  6 + // some space for PID
                  + 15];
  sprintf(new_format, "[%d] [%s] [ERROR] %s\n", getpid(), timestamp, format);
  vfprintf(RS_LOG_FILE, new_format, ap);
  va_end(ap);
}

void do_log_debug(const char *file, int line, char *format, ...) {
  va_list ap;
  va_start(ap, format);
  char *timestamp = time_now();
  char new_format[strlen(timestamp) +
                  strlen(format) +
                  strlen(file) +
                  5 + // (reasonable length for 'line')
                  6 + // space for PID
                  + 23];
  sprintf(new_format, "[%d] [%s] [DEBUG] %s:%d: %s\n", getpid(), timestamp, file, line, format);
  vfprintf(RS_LOG_FILE, new_format, ap);
  va_end(ap);
}

void log_dump_state_start(void) {
  fprintf(RS_LOG_FILE, "[%s] -- START STATE DUMP --\n", time_now());
}

void log_dump_state_end(void) {
  fprintf(RS_LOG_FILE, "[%s] -- END STATE DUMP --\n", time_now());
}

void add_cors_headers(struct evkeyvalq *headers) {
  evhttp_add_header(headers, "Access-Control-Allow-Origin", RS_ALLOW_ORIGIN);
  evhttp_add_header(headers, "Access-Control-Allow-Headers", RS_ALLOW_HEADERS);
  evhttp_add_header(headers, "Access-Control-Allow-Methods", RS_ALLOW_METHODS);
}

static char hex_chars[] = "0123456789ABCDEF";

char *generate_token(size_t bytes) {
  char *raw_buf = malloc(bytes);
  if(raw_buf == NULL) {
    perror("malloc() failed for raw token buffer");
    return NULL;
  }
  evutil_secure_rng_get_bytes(raw_buf, bytes);
  char *encoded_buf = malloc(2 * bytes + 1);
  if(encoded_buf == NULL) {
    perror("malloc() failed for encoded token buffer");
    return NULL;
  }
  int i;
  for(i=0;i<bytes;i++) {
    encoded_buf[i*2] = hex_chars[(raw_buf[i] & 0xF0) >> 4];
    encoded_buf[i*2+1] = hex_chars[raw_buf[i] & 0x0F];
  }
  encoded_buf[2 * bytes] = 0;
  free(raw_buf);
  return encoded_buf;
}

