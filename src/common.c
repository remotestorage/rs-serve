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

