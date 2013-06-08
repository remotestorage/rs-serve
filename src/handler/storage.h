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

#ifndef RS_HANDLER_STORAGE_H
#define RS_HANDLER_STORAGE_H

int storage_handle_head(struct rs_request *request);
int storage_handle_get(struct rs_request *request);
int storage_handle_put(struct rs_request *request);
int storage_handle_delete(struct rs_request *request);

#endif /* !RS_HANDLER_STORAGE_H */
