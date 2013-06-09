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

#ifndef RS_REQUEST_H
#define RS_REQUEST_H

struct rs_header {
  char *key;
  char *value;
  struct rs_header *next;
};

struct rs_request {
  // the connection's fd
  int fd;
  // process info, so we can access it from anywhere w/o the need to pass it
  // around separately.
  struct rs_process_info *proc;
  // the event that we set up to read the request
  struct event *read_event;
  // the event used to write to the socket, once we started
  // assembling the response.
  struct event *write_event;
  // event used to read or write a file on disk
  struct event *file_event;
  // chained list of headers
  struct rs_header *headers;

  char *path;
  int path_len;

  // buffer for the parser.
  char *buf;
  int buflen;

  struct evbuffer *response_buf;
  short response_ended;
  short status;
};

void free_header(struct rs_header *header);
void free_request(struct rs_request *header);

#endif /* !RS_REQUEST_H */
