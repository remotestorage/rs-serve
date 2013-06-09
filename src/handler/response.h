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

#ifndef RS_HANDLER_RESPONSE_H
#define RS_HANDLER_RESPONSE_H

void send_error_response(struct rs_request *request, short status);
void send_response_head(struct rs_request *request, short status, struct rs_header *header);
void send_response_body(struct rs_request *request, struct evbuffer *buf);
void send_response_body_fd(struct rs_request *request, int fd);
void send_response_empty(struct rs_request *request);

#endif
