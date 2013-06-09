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

void free_header(struct rs_header *header) {
  if(header->next) {
    free_header(header->next);
  }
  if(header->key) {
    free(header->key);
  }
  if(header->value) {
    free(header->value);
  }
  free(header);
}

void free_request(struct rs_request *req) {
  if(req->read_event) {
    event_del(req->read_event);
    event_free(req->read_event);
  }
  if(req->write_event) {
    event_del(req->write_event);
    event_free(req->write_event);
  }
  if(req->file_event) {
    event_del(req->file_event);
    event_free(req->file_event);
  }
  if(req->path) {
    free(req->path);
  }
  if(req->headers) {
    free(req->headers);
  }
  if(req->buf) {
    free(req->buf);
  }
  if(req->response_buf) {
    evbuffer_free(req->response_buf);
  }
  if(req->fd > 0) {
    log_debug("closing socket %d", req->fd);
    if(close(req->fd) != 0) {
      log_error("close() failed: %s", strerror(errno));
    }
  }
  free(req);
}
